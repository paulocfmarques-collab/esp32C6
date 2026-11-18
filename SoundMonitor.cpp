#include "SoundMonitor.h"
#include "AudioFft.h"
#include "Diagnostics.h"
#include <Preferences.h>
#include <math.h>
#include <soc/soc_caps.h>

bool SoundMonitor::begin(int pin) {
    if (task_) return true;
    Preferences calibration;
    if (calibration.begin("ldr", true)) {
        const uint32_t points = calibration.getUInt("points", 0xFFFFFFFF);
        calibration.end();
        const uint16_t dark = uint16_t(points);
        const uint16_t bright = uint16_t(points >> 16);
        if (dark <= LightSnapshot::UNSET && bright <= LightSnapshot::UNSET &&
            (dark == LightSnapshot::UNSET || bright == LightSnapshot::UNSET || dark < bright)) {
            light_.dark = dark;
            light_.bright = bright;
        }
    }
    constexpr int lightPin = LightSensorConfig::ANALOG_PIN;
    if (pin > 3 || pin == lightPin) return false;
    state_.pin = pin;
    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t error = adc_continuous_io_to_channel(lightPin, &unit, &channel);
    if (error != ESP_OK || unit != ADC_UNIT_1) return false;
    lightChannel_ = uint8_t(channel);
    if (pin >= 0) {
        error = adc_continuous_io_to_channel(pin, &unit, &channel);
        if (error != ESP_OK || unit != ADC_UNIT_1) return false;
        channel_ = uint8_t(channel);
    }
    adc_continuous_handle_cfg_t handle{};
    handle.max_store_buf_size = sizeof(dma_) * (pin >= 0 ? 4 : 2);
    handle.conv_frame_size = sizeof(dma_);
    handle.flags.flush_pool = true;
    error = adc_continuous_new_handle(&handle, &adc_);
    if (error != ESP_OK) {
        Diagnostics::event("SOUND_START_FAILED", esp_err_to_name(error));
        return false;
    }
    adc_digi_pattern_config_t pattern[2]{};
    pattern[0].atten = ADC_ATTEN_DB_12;
    pattern[0].channel = lightChannel_;
    pattern[0].unit = ADC_UNIT_1;
    pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    pattern[1] = pattern[0];
    pattern[1].channel = channel_;
    adc_continuous_config_t config{};
    config.pattern_num = pin >= 0 ? 2 : 1;
    config.adc_pattern = pattern;
    config.sample_freq_hz = SAMPLE_RATE * config.pattern_num;
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
    state_.running = pin >= 0;
    if (xTaskCreate(sampleTask, "sound_fft", 4096, this, 1, &task_) != pdPASS) {
        adc_continuous_stop(adc_); adc_continuous_deinit(adc_); adc_ = nullptr;
        task_ = nullptr; state_.running = false;
        Diagnostics::event("SOUND_START_FAILED", "task_allocation_failed=1");
        return false;
    }
    Diagnostics::event("ADC_START", pin >= 0 ? "light_gpio=1 audio_hz=20000 total_hz=40000" : "light_gpio=1 total_hz=20000");
    if (pin >= 0) Diagnostics::event("SOUND_START", "adc_dma=1 fs=20000 fft=512 bands=12 gain=32");
    return true;
}

SoundMonitor::LightSnapshot SoundMonitor::lightSnapshot() const {
    portENTER_CRITICAL(&mux_);
    const auto copy = light_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

bool SoundMonitor::saveLightCalibration(uint16_t dark, uint16_t bright) {
    Preferences calibration;
    if (!calibration.begin("ldr", false)) return false;
    const uint32_t points = (uint32_t(bright) << 16) | dark;
    const bool saved = calibration.putUInt("points", points) == sizeof(points);
    calibration.end();
    if (saved) {
        portENTER_CRITICAL(&mux_);
        light_.dark = dark;
        light_.bright = bright;
        portEXIT_CRITICAL(&mux_);
    }
    return saved;
}

String SoundMonitor::calibrateLight(bool darkPoint) {
    const auto light = lightSnapshot();
    if (!light.fresh()) return "LDR sem leitura recente. Aguarde e tente novamente.\n";

    const uint16_t dark = darkPoint ? light.raw : light.dark;
    const uint16_t bright = darkPoint ? light.bright : light.raw;
    if (dark == LightSnapshot::ADC_MAX || bright == 0 ||
        (dark != LightSnapshot::UNSET && bright != LightSnapshot::UNSET && dark >= bright))
        return "Calibracao invalida: ADC do escuro deve ser menor que o da luz.\n";
    if (!saveLightCalibration(dark, bright)) return "Falha ao salvar calibracao do LDR.\n";

    String report = String(darkPoint ? "Escuro (0%)" : "Luz maxima (100%)") +
        " salvo em ADC " + String(light.raw) + ".\n";
    if (dark == LightSnapshot::UNSET) report += "Falta registrar: ldr_escuro\n";
    if (bright == LightSnapshot::UNSET) report += "Falta registrar: ldr_claro\n";
    return report;
}

String SoundMonitor::resetLightCalibration() {
    if (!saveLightCalibration(LightSnapshot::UNSET, LightSnapshot::UNSET))
        return "Falha ao limpar calibracao do LDR.\n";
    return "Calibracao limpa. Escala padrao ADC 0 a 4095.\n";
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
    audioFft(real_, imag_, FFT_SIZE);
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
    uint32_t lightSum = 0, lightCount = 0, lightAt = lastRead;
    for (;;) {
        uint32_t bytes = 0;
        const esp_err_t error = adc_continuous_read(self->adc_, self->dma_, sizeof(self->dma_), &bytes, 100);
        const uint32_t now = millis();
        if (error != ESP_OK) {
            lightSum = lightCount = 0;
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
            if (result->type2.channel == self->lightChannel_) {
                lightSum += result->type2.data;
                ++lightCount;
                continue;
            }
            if (!self->state_.running || result->type2.channel != self->channel_) continue;
            self->real_[filled++] = result->type2.data;
            if (filled == FFT_SIZE) { self->analyzeFrame(); filled = 0; }
        }
        if (lightCount && uint32_t(now - lightAt) >= 250) {
            portENTER_CRITICAL(&self->mux_);
            self->light_.raw = uint16_t(lightSum / lightCount);
            self->light_.updatedAt = now;
            self->light_.valid = true;
            portEXIT_CRITICAL(&self->mux_);
            lightSum = lightCount = 0;
            lightAt = now;
        }
        vTaskDelay(1);
    }
}
