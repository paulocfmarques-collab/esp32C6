#include "SoundMonitor.h"
#include "Diagnostics.h"
#include <math.h>
#include <soc/soc_caps.h>

bool SoundMonitor::begin(int pin) {
    if (task_) return true;
    if (pin < 0 || pin > 3) return false;
    state_.pin = pin;
    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t error = adc_continuous_io_to_channel(pin, &unit, &channel);
    if (error != ESP_OK || unit != ADC_UNIT_1) return false;
    channel_ = uint8_t(channel);
    adc_continuous_handle_cfg_t handle{};
    handle.max_store_buf_size = sizeof(dma_) * 2;
    handle.conv_frame_size = sizeof(dma_);
    handle.flags.flush_pool = true;
    error = adc_continuous_new_handle(&handle, &adc_);
    if (error != ESP_OK) {
        Diagnostics::event("SOUND_START_FAILED", esp_err_to_name(error));
        return false;
    }
    adc_digi_pattern_config_t pattern{};
    pattern.atten = ADC_ATTEN_DB_12;
    pattern.channel = channel_;
    pattern.unit = ADC_UNIT_1;
    pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    adc_continuous_config_t config{};
    config.pattern_num = 1;
    config.adc_pattern = &pattern;
    config.sample_freq_hz = SAMPLE_RATE;
    config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    error = adc_continuous_config(adc_, &config);
    if (error == ESP_OK) error = adc_continuous_start(adc_);
    if (error != ESP_OK) {
        adc_continuous_deinit(adc_); adc_ = nullptr;
        Diagnostics::event("SOUND_START_FAILED", esp_err_to_name(error));
        return false;
    }
    windowPower_ = 0;
    for (uint16_t i = 0; i < FFT_SIZE; ++i) {
        window_[i] = 0.5f - 0.5f * cosf(2.0f * PI * i / (FFT_SIZE - 1));
        windowPower_ += window_[i] * window_[i];
    }
    historyAt_ = millis();
    state_.running = true;
    if (xTaskCreate(sampleTask, "sound_fft", 4096, this, 1, &task_) != pdPASS) {
        adc_continuous_stop(adc_); adc_continuous_deinit(adc_); adc_ = nullptr;
        task_ = nullptr; state_.running = false;
        Diagnostics::event("SOUND_START_FAILED", "task_allocation_failed=1");
        return false;
    }
    Diagnostics::event("SOUND_START", "adc_dma=1 fs=20000 fft=512 bands=12 gain=32");
    return true;
}

SoundMonitor::Snapshot SoundMonitor::snapshot() const {
    portENTER_CRITICAL(&mux_);
    Snapshot copy = state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

bool SoundMonitor::setGain(uint8_t gain) {
    if (gain < 1 || gain > 64) return false;
    portENTER_CRITICAL(&mux_); state_.gain = gain; portEXIT_CRITICAL(&mux_);
    return true;
}

void SoundMonitor::setEqualizer(bool enabled) {
    portENTER_CRITICAL(&mux_); state_.equalizer = enabled; portEXIT_CRITICAL(&mux_);
}

void SoundMonitor::analyzeFrame() {
    float mean = 0;
    uint16_t minimum = 4095, maximum = 0;
    for (uint16_t i = 0; i < FFT_SIZE; ++i) {
        mean += real_[i];
        if (real_[i] < minimum) minimum = uint16_t(real_[i]);
        if (real_[i] > maximum) maximum = uint16_t(real_[i]);
    }
    mean /= FFT_SIZE;
    for (uint16_t i = 0; i < FFT_SIZE; ++i) {
        real_[i] = (real_[i] - mean) * window_[i];
        imag_[i] = 0;
    }
    // FFT radix-2, em memoria fixa: remocao de DC e janela Hann antes da transformada.
    for (uint16_t i = 1, j = 0; i < FFT_SIZE; ++i) {
        uint16_t bit = FFT_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float swap = real_[i]; real_[i] = real_[j]; real_[j] = swap;
            swap = imag_[i]; imag_[i] = imag_[j]; imag_[j] = swap;
        }
    }
    for (uint16_t length = 2; length <= FFT_SIZE; length <<= 1) {
        const float angle = -2.0f * PI / length;
        const float stepR = cosf(angle), stepI = sinf(angle);
        for (uint16_t start = 0; start < FFT_SIZE; start += length) {
            float wr = 1, wi = 0;
            for (uint16_t j = 0; j < length / 2; ++j) {
                const uint16_t a = start + j, b = a + length / 2;
                const float vr = real_[b] * wr - imag_[b] * wi;
                const float vi = real_[b] * wi + imag_[b] * wr;
                const float ar = real_[a], ai = imag_[a];
                real_[a] = ar + vr; imag_[a] = ai + vi;
                real_[b] = ar - vr; imag_[b] = ai - vi;
                const float nextR = wr * stepR - wi * stepI;
                wi = wr * stepI + wi * stepR; wr = nextR;
            }
        }
    }
    static constexpr uint16_t edges[BAND_COUNT + 1] =
        {80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 6300, 8000, 10000};
    float bands[BAND_COUNT]{};
    for (uint16_t k = 1; k < FFT_SIZE / 2; ++k) {
        const float frequency = float(k) * SAMPLE_RATE / FFT_SIZE;
        for (uint8_t band = 0; band < BAND_COUNT; ++band) {
            if (frequency >= edges[band] && frequency < edges[band + 1]) {
                bands[band] += real_[k] * real_[k] + imag_[k] * imag_[k];
                break;
            }
        }
    }
    for (uint8_t band = 0; band < BAND_COUNT; ++band)
        bands[band] = sqrtf(2.0f * bands[band] / (FFT_SIZE * windowPower_));
    if (minimum < historyMin_) historyMin_ = minimum;
    if (maximum > historyMax_) historyMax_ = maximum;
    const uint32_t now = millis();
    portENTER_CRITICAL(&mux_);
    for (uint8_t band = 0; band < BAND_COUNT; ++band) state_.bands[band] = bands[band];
    state_.clipped = minimum <= 2 || maximum >= 4093;
    state_.updatedAt = now;
    if (uint32_t(now - historyAt_) >= WINDOW_MS) {
        state_.latest = historyMax_ - historyMin_;
        state_.values[state_.head] = state_.latest;
        state_.times[state_.head] = now;
        state_.head = (state_.head + 1) % HISTORY_SIZE;
        if (state_.count < HISTORY_SIZE) ++state_.count;
        historyAt_ = now; historyMin_ = 4095; historyMax_ = 0;
    }
    portEXIT_CRITICAL(&mux_);
}

void SoundMonitor::sampleTask(void* context) {
    auto* self = static_cast<SoundMonitor*>(context);
    uint16_t filled = 0;
    uint32_t lastError = 0, lastRead = millis();
    for (;;) {
        uint32_t bytes = 0;
        const esp_err_t error = adc_continuous_read(self->adc_, self->dma_, sizeof(self->dma_), &bytes, 100);
        const uint32_t now = millis();
        if (error != ESP_OK) {
            filled = 0;
            self->historyMin_ = 4095; self->historyMax_ = 0; self->historyAt_ = now;
            if (error != ESP_ERR_TIMEOUT && uint32_t(now - lastError) >= 10000) {
                Diagnostics::event("SOUND_ADC_ERROR", esp_err_to_name(error)); lastError = now;
            }
            vTaskDelay(1);
            continue;
        }
        if (uint32_t(now - lastRead) > 200) {
            filled = 0; self->historyMin_ = 4095; self->historyMax_ = 0; self->historyAt_ = now;
        }
        lastRead = now;
        for (uint32_t offset = 0; offset + SOC_ADC_DIGI_RESULT_BYTES <= bytes; offset += SOC_ADC_DIGI_RESULT_BYTES) {
            const auto* result = reinterpret_cast<const adc_digi_output_data_t*>(self->dma_ + offset);
            if (result->type2.channel != self->channel_) continue;
            self->real_[filled++] = result->type2.data;
            if (filled == FFT_SIZE) { self->analyzeFrame(); filled = 0; }
        }
        vTaskDelay(1);
    }
}
