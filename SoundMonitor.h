#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_adc/adc_continuous.h>

// RoboCore analogico: VCC em 3V3, GND comum, sinal em GPIO0.
// Use -1 para desativar a leitura quando o sensor nao estiver conectado.
namespace SoundSensorConfig { constexpr int ANALOG_PIN = 0; }

class SoundMonitor {
public:
    static constexpr uint8_t HISTORY_SIZE = 96;
    static constexpr uint32_t WINDOW_MS = 100;
    static constexpr uint32_t HISTORY_MS = HISTORY_SIZE * WINDOW_MS;
    static constexpr uint16_t FFT_SIZE = 512;
    static constexpr uint8_t BAND_COUNT = 12;
    static constexpr uint32_t SAMPLE_RATE = 20000;
    struct Snapshot {
        uint16_t values[HISTORY_SIZE]{};
        uint32_t times[HISTORY_SIZE]{};
        uint8_t head = 0, count = 0;
        uint16_t latest = 0;
        uint32_t updatedAt = 0;
        int pin = -1;
        bool running = false;
        float bands[BAND_COUNT]{}; // RMS em contagens ADC; ganho apenas na visualizacao.
        uint8_t gain = 32;
        bool equalizer = true, clipped = false;
    };
    bool begin(int pin);
    Snapshot snapshot() const;
    bool setGain(uint8_t gain);
    void setEqualizer(bool enabled);
private:
    Snapshot state_;
    TaskHandle_t task_ = nullptr;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    adc_continuous_handle_t adc_ = nullptr;
    uint8_t channel_ = 0;
    alignas(4) uint8_t dma_[FFT_SIZE * 4]{};
    float real_[FFT_SIZE]{}, imag_[FFT_SIZE]{}, window_[FFT_SIZE]{};
    float windowPower_ = 0;
    uint16_t historyMin_ = 4095, historyMax_ = 0;
    uint32_t historyAt_ = 0;
    void analyzeFrame();
    static void sampleTask(void* context);
};
