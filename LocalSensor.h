#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <atomic>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Modulo DHT22/AM2302: VCC=3V3, DATA=GPIO3, GND comum.
class LocalSensor
{
  public:
    static constexpr uint8_t PIN = 3;
    static constexpr uint8_t HISTORY_SIZE = 48;

    struct Snapshot
    {
        float temperature = 0, humidity = 0;
        float history[HISTORY_SIZE]{};
        bool historyValid[HISTORY_SIZE]{};
        uint8_t head = 0, count = 0;
        bool running = false, valid = false, lastFailed = false;
        int64_t updatedUs = 0;
        uint32_t errors = 0;

        bool fresh() const
        {
            return running && valid && esp_timer_get_time() - updatedUs < 10000000LL;
        }
    };

    void begin();
    Snapshot snapshot() const;
    void pauseForOta(bool paused) { paused_.store(paused); }

  private:
    portMUX_TYPE captureMux_ = portMUX_INITIALIZER_UNLOCKED;
    std::atomic<bool> paused_{false};
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    Snapshot state_;
    TaskHandle_t task_ = nullptr;
    static void worker(void* context);
    bool readFrame(float& temperature, float& humidity);
};

extern LocalSensor localSensor;
