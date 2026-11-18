#pragma once
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DigitalMic {
public:
    static constexpr gpio_num_t BCLK = GPIO_NUM_18;
    static constexpr gpio_num_t WS = GPIO_NUM_2;
    static constexpr gpio_num_t DATA = GPIO_NUM_19;
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint16_t FFT_SIZE = 512;
    static constexpr size_t BAND_COUNT = 12;
    static constexpr size_t HISTORY_SIZE = 150;
    static constexpr uint32_t HISTORY_STEP_MS = 200;
    enum class Mode { Meter, History, Spectrum };

    struct HistoryPoint {
        uint32_t timestamp = 0;
        float rmsDbfs = -120;
    };

    struct Snapshot {
        float rmsDbfs = -120;
        float peakDbfs = -120;
        uint32_t updatedAt = 0;
        bool valid = false;
        bool running = false;
        bool hasSignal = false;
        float bands[BAND_COUNT]{};
        uint32_t events = 0;
        int thresholdDbfs = -35;
        bool eventActive = false;
        Mode mode = Mode::Meter;
        bool fresh() const { return valid && uint32_t(millis() - updatedAt) < 1000; }
    };

    bool begin();
    Snapshot snapshot() const;
    size_t history(HistoryPoint* dest) const;
    void setMode(Mode mode);
    void setThreshold(int dbfs);

private:
    static void capture(void* context);
    static constexpr size_t FRAMES = 160;
    int32_t samples_[FRAMES * 2]{};
    float fftReal_[FFT_SIZE]{}, fftImag_[FFT_SIZE]{}, window_[FFT_SIZE]{};
    float windowPower_ = 0;
    i2s_chan_handle_t channel_ = nullptr;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    Snapshot state_;
    HistoryPoint history_[HISTORY_SIZE]{};
    size_t historyHead_ = 0, historyCount_ = 0;
    uint8_t above_ = 0, below_ = 0;
    void publish(float rms, float peak, bool hasSignal);
};

extern DigitalMic digitalMic;
