#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_adc/adc_continuous.h>

// Sensor analogico RoboCore ativo no GPIO0. INMP441 usa DigitalMic.
// Adaptador legado: VCC em 3V3, GND comum, sinal em GPIO0 (GPIO3 reservado ao DHT22).
// Use -1 para desativar a leitura quando o sensor nao estiver conectado.
namespace SoundSensorConfig { constexpr int ANALOG_PIN = 0; }
namespace LightSensorConfig { constexpr int ANALOG_PIN = 1; }

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
    struct LightSnapshot {
        static constexpr uint16_t ADC_MAX = 4095;
        static constexpr uint16_t UNSET = ADC_MAX + 1;
        uint16_t raw = 0;
        uint32_t updatedAt = 0;
        bool valid = false;
        uint16_t dark = UNSET;
        uint16_t bright = UNSET;

        bool fresh() const { return valid && uint32_t(millis() - updatedAt) < 1500; }
        bool calibrated() const { return bright <= ADC_MAX && dark < bright; }
    };
    LightSnapshot lightSnapshot() const;
    String calibrateLight(bool darkPoint);
    String resetLightCalibration();
    Snapshot snapshot() const;
    bool setGain(uint8_t gain);
    void setEqualizer(bool enabled);
private:
    Snapshot state_;
    TaskHandle_t task_ = nullptr;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    adc_continuous_handle_t adc_ = nullptr;
    uint8_t channel_ = 0;
    uint8_t lightChannel_ = 0;
    LightSnapshot light_;
    bool saveLightCalibration(uint16_t dark, uint16_t bright);
    alignas(4) uint8_t dma_[FFT_SIZE * 4]{};
    float real_[FFT_SIZE]{}, imag_[FFT_SIZE]{}, window_[FFT_SIZE]{};
    float windowPower_ = 0;
    uint16_t historyMin_ = 4095, historyMax_ = 0;
    uint32_t historyAt_ = 0;
    void analyzeFrame();
    static void sampleTask(void* context);
};
