#include "DigitalMic.h"
#include "AudioFft.h"
#include "Diagnostics.h"
#include <driver/gpio.h>
#include <math.h>

bool DigitalMic::begin() {
    if (channel_) return true;

    i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channelConfig.dma_desc_num = 4;
    channelConfig.dma_frame_num = FRAMES;
    esp_err_t error = i2s_new_channel(&channelConfig, nullptr, &channel_);
    if (error == ESP_OK) {
        i2s_std_config_t config{};
        config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
        config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
        config.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        config.gpio_cfg.mclk = GPIO_NUM_NC;
        config.gpio_cfg.bclk = BCLK;
        config.gpio_cfg.ws = WS;
        config.gpio_cfg.dout = GPIO_NUM_NC;
        config.gpio_cfg.din = DATA;
        error = i2s_channel_init_std_mode(channel_, &config);
        if (error == ESP_OK) {
            // Define o nivel de repouso quando o microfone deixa DOUT em alta impedancia.
            gpio_pulldown_en(DATA);
            error = i2s_channel_enable(channel_);
        }
    }

    if (error != ESP_OK) {
        if (channel_) i2s_del_channel(channel_);
        channel_ = nullptr;
        Diagnostics::event("INMP441_START_FAILED", esp_err_to_name(error));
        Serial.printf("[INMP441] Falha ao iniciar I2S: %s\n", esp_err_to_name(error));
        return false;
    }

    for (uint16_t i = 0; i < FFT_SIZE; ++i) {
        window_[i] = 0.5f - 0.5f * cosf(2.0f * PI * i / (FFT_SIZE - 1));
        windowPower_ += window_[i] * window_[i];
    }
    state_.running = true;
    if (xTaskCreate(capture, "inmp441", 3072, this, 1, nullptr) != pdPASS) {
        state_.running = false;
        i2s_channel_disable(channel_);
        i2s_del_channel(channel_);
        channel_ = nullptr;
        Diagnostics::event("INMP441_START_FAILED", "task_allocation_failed=1");
        Serial.println("[INMP441] Sem memoria para a tarefa de captura.");
        return false;
    }
    Serial.printf("[INMP441] I2S iniciado: SCK=%d WS=%d SD=%d, canal esquerdo, 16 kHz.\n",
                  int(BCLK), int(WS), int(DATA));
    return true;
}

DigitalMic::Snapshot DigitalMic::snapshot() const {
    portENTER_CRITICAL(&mux_);
    const auto copy = state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

size_t DigitalMic::history(HistoryPoint* dest) const {
    portENTER_CRITICAL(&mux_);
    const size_t count = historyCount_;
    for (size_t i = 0; i < count; ++i)
        dest[i] = history_[(historyHead_ + HISTORY_SIZE - count + i) % HISTORY_SIZE];
    portEXIT_CRITICAL(&mux_);
    return count;
}

void DigitalMic::setMode(Mode mode) {
    portENTER_CRITICAL(&mux_);
    state_.mode = mode;
    portEXIT_CRITICAL(&mux_);
}

void DigitalMic::setThreshold(int dbfs) {
    portENTER_CRITICAL(&mux_);
    state_.thresholdDbfs = dbfs;
    state_.eventActive = false;
    above_ = below_ = 0;
    portEXIT_CRITICAL(&mux_);
}

void DigitalMic::publish(float rms, float peak, bool hasSignal) {
    float mean = 0;
    for (float sample : fftReal_) mean += sample;
    mean /= FFT_SIZE;
    for (uint16_t i = 0; i < FFT_SIZE; ++i) {
        fftReal_[i] = (fftReal_[i] - mean) * window_[i];
        fftImag_[i] = 0;
    }
    audioFft(fftReal_, fftImag_, FFT_SIZE);
    constexpr uint16_t edges[] = {62, 125, 200, 315, 500, 800, 1250, 2000, 3150, 4000, 5000, 6300, 8000};
    float bands[BAND_COUNT]{};
    for (uint16_t bin = 2; bin < FFT_SIZE / 2; ++bin) {
        const uint32_t hz = uint32_t(bin) * SAMPLE_RATE / FFT_SIZE;
        for (size_t band = 0; band < BAND_COUNT; ++band) {
            if (hz < edges[band + 1]) {
                bands[band] += fftReal_[bin] * fftReal_[bin] + fftImag_[bin] * fftImag_[bin];
                break;
            }
        }
    }
    for (float& value : bands)
        value = 10 * log10f(fmaxf(2 * value / (FFT_SIZE * windowPower_), 1e-12f));

    const uint32_t now = millis();
    bool triggered = false;
    int threshold;
    portENTER_CRITICAL(&mux_);
    threshold = state_.thresholdDbfs;
    state_.rmsDbfs = rms;
    state_.peakDbfs = peak;
    state_.updatedAt = now;
    state_.hasSignal = hasSignal;
    state_.valid = true;
    for (size_t i = 0; i < BAND_COUNT; ++i) state_.bands[i] = bands[i];
    if (!state_.eventActive) {
        if (hasSignal && rms >= threshold) {
            if (++above_ == 3) {
                state_.eventActive = true;
                ++state_.events;
                triggered = true;
                above_ = 0;
            }
        } else above_ = 0;
    } else if (!hasSignal || rms < threshold - 5) {
        if (++below_ == 5) {
            state_.eventActive = false;
            below_ = 0;
        }
    } else below_ = 0;
    if (!historyCount_ || uint32_t(now - history_[(historyHead_ + HISTORY_SIZE - 1) % HISTORY_SIZE].timestamp)
            >= HISTORY_STEP_MS - 10) {
        history_[historyHead_] = {now, rms};
        historyHead_ = (historyHead_ + 1) % HISTORY_SIZE;
        if (historyCount_ < HISTORY_SIZE) ++historyCount_;
    }
    portEXIT_CRITICAL(&mux_);
    if (triggered) {
        char detail[64];
        snprintf(detail, sizeof(detail), "rms_dbfs=%.1f threshold_dbfs=%d", rms, threshold);
        Diagnostics::event("INMP441_SOUND_EVENT", detail);
    }
}

void DigitalMic::capture(void* context) {
    auto& mic = *static_cast<DigitalMic*>(context);
    const uint32_t startedAt = millis();
    uint32_t count = 0;
    float sum = 0, squares = 0, low = 1, high = -1;
    for (;;) {
        size_t bytes = 0;
        const esp_err_t error = i2s_channel_read(mic.channel_, mic.samples_, sizeof(mic.samples_), &bytes, 100);
        if (error != ESP_OK) {
            portENTER_CRITICAL(&mic.mux_);
            mic.state_.valid = false;
            mic.state_.eventActive = false;
            mic.above_ = mic.below_ = 0;
            portEXIT_CRITICAL(&mic.mux_);
            count = 0;
            sum = squares = 0;
            low = 1; high = -1;
            vTaskDelay(1);
            continue;
        }
        if (uint32_t(millis() - startedAt) < 350) continue;

        const size_t frames = bytes / (2 * sizeof(int32_t));
        for (size_t i = 0; i < frames; ++i) {
            // Os 24 bits assinados ocupam o topo do slot de 32 bits. L/R em GND usa o primeiro slot.
            const float value = float(mic.samples_[2 * i] >> 8) / 8388608.0f;
            if (count < FFT_SIZE) mic.fftReal_[count] = value;
            sum += value;
            squares += value * value;
            low = fminf(low, value);
            high = fmaxf(high, value);
            if (++count < SAMPLE_RATE / 10) continue;

            const float mean = sum / count;
            const float rms = sqrtf(fmaxf(0, squares / count - mean * mean));
            const float peak = fmaxf(fabsf(low), fabsf(high));
            const float rmsDbfs = 20 * log10f(fmaxf(rms, 0.000001f));
            const float peakDbfs = 20 * log10f(fmaxf(peak, 0.000001f));
            mic.publish(rmsDbfs, peakDbfs, high > low);
            count = 0;
            sum = squares = 0;
            low = 1; high = -1;
        }
    }
}
