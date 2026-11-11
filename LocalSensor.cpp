#include "LocalSensor.h"
#include "Diagnostics.h"
#include <math.h>
#include <esp_rom_sys.h>

LocalSensor localSensor;

void LocalSensor::begin() {
    if (task_) return;
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << PIN;
    config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) {
        Diagnostics::event("DHT_GPIO_FAILED");
        return;
    }
    gpio_set_level(gpio_num_t(PIN), 1);
    portENTER_CRITICAL(&mux_);
    state_.running = true;
    portEXIT_CRITICAL(&mux_);
    if (xTaskCreate(worker, "dht22", 3072, this, 1, &task_) != pdPASS) {
        task_ = nullptr;
        portENTER_CRITICAL(&mux_);
        state_.running = false;
        portEXIT_CRITICAL(&mux_);
        Diagnostics::event("DHT_TASK_FAILED");
        Serial.println("[DHT22] Sem memoria para iniciar leitura.");
    }
}

LocalSensor::Snapshot LocalSensor::snapshot() const {
    portENTER_CRITICAL(&mux_);
    Snapshot copy = state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

bool LocalSensor::readFrame(float& temperature, float& humidity) {
    if (paused_.load()) return false;
    const gpio_num_t pin = gpio_num_t(PIN);
    gpio_set_level(pin, 0);
    // Inicio de pelo menos 1 ms com interrupcoes habilitadas.
    vTaskDelay(pdMS_TO_TICKS(2) + 1);
    uint16_t highUs[40]{};
    bool ok = true;
    portENTER_CRITICAL(&captureMux_);
    if (paused_.load()) {
        gpio_set_level(pin, 1);
        portEXIT_CRITICAL(&captureMux_);
        return false;
    }
    const int64_t started = esp_timer_get_time();
    auto pulse = [&](int level, uint16_t* duration = nullptr) {
        const int64_t at = esp_timer_get_time();
        while (gpio_get_level(pin) == level) {
            const int64_t now = esp_timer_get_time();
            if (now - at >= 120 || now - started >= 6000) return false;
        }
        if (duration) *duration = uint16_t(esp_timer_get_time() - at);
        return true;
    };
    gpio_set_level(pin, 1); // Solta a linha open-drain; sensor assume o barramento.
    esp_rom_delay_us(55);
    ok = pulse(0) && pulse(1);
    for (uint8_t i = 0; i < 40 && ok; ++i)
        ok = pulse(0) && pulse(1, &highUs[i]);
    portEXIT_CRITICAL(&captureMux_);
    if (!ok) return false;
    // Decodificacao, checksum e floats apenas com interrupcoes habilitadas.
    uint8_t data[5]{};
    for (uint8_t i = 0; i < 40; ++i)
        data[i / 8] = uint8_t((data[i / 8] << 1) | (highUs[i] > 45 ? 1 : 0));
    if (uint8_t(data[0] + data[1] + data[2] + data[3]) != data[4]) return false;
    humidity = float((uint16_t(data[0]) << 8) | data[1]) * 0.1f;
    temperature = float((uint16_t(data[2] & 0x7F) << 8) | data[3]) * 0.1f;
    if (data[2] & 0x80) temperature = -temperature;
    return true;
}

void LocalSensor::worker(void* context) {
    auto* self = static_cast<LocalSensor*>(context);
    Diagnostics::event("DHT_START", "pin=3 interval_ms=2500");
    bool wasOk = false;
    bool attempted = false;
    for (;;) {
        // Aguarda estabilizacao inicial e respeita o intervalo minimo do DHT22.
        vTaskDelay(pdMS_TO_TICKS(2500));
        if (self->paused_.load()) continue;
        float humidity = 0, temperature = 0;
        const bool frameOk = self->readFrame(temperature, humidity);
        if (self->paused_.load()) continue;
        const bool ok = frameOk && isfinite(temperature) && isfinite(humidity) &&
            temperature >= -40 && temperature <= 80 && humidity >= 0 && humidity <= 100;
        const int64_t now = esp_timer_get_time();
        portENTER_CRITICAL(&self->mux_);
        auto& s = self->state_;
        s.lastFailed = !ok;
        if (ok) {
            s.temperature = temperature;
            s.humidity = humidity;
            s.valid = true;
            s.updatedUs = now;
        } else ++s.errors;
        s.history[s.head] = ok ? temperature : 0;
        s.historyValid[s.head] = ok;
        s.head = (s.head + 1) % HISTORY_SIZE;
        if (s.count < HISTORY_SIZE) ++s.count;
        portEXIT_CRITICAL(&self->mux_);
        if (!attempted || ok != wasOk) {
            Diagnostics::event(ok ? "DHT_READY" : "DHT_READ_FAILED", "pin=3");
            Serial.println(ok ? "[DHT22] Sensor local pronto no GPIO3." :
                                "[DHT22] Falha de leitura no GPIO3; confira DATA e alimentacao.");
        }
        attempted = true;
        wasOk = ok;
    }
}
