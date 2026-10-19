#include "Diagnostics.h"
#include "SDUtil.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <time.h>

namespace {
struct Record { char text[448]; };
struct ScrapeMarker { uint32_t magic, pending, id; uint64_t upMs; };
// Nao reinicializar este marcador durante um reset por panic/watchdog.
RTC_NOINIT_ATTR ScrapeMarker marker;
constexpr uint32_t markerMagic = 0xC6D1A601;
QueueHandle_t queue = nullptr;
SDUtil* storage = nullptr;
std::atomic<uint32_t> dropped{0}, writeFailures{0};
uint32_t lastHeartbeat = 0, lastDisconnects = 0;
std::atomic<uint32_t> disconnectCount{0}, disconnectReason{0};
int lastWifi = -1;
bool started = false;

void makeRecord(Record& record, const char* name, const char* detail) 
{
    char stamp[24] = "SEM_NTP";
    const time_t now = time(nullptr);
    if (now > 1577836800) {
        struct tm local;
        localtime_r(&now, &local);
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    }
    const int wifi = WiFi.status();
    snprintf(record.text, sizeof(record.text),
        "%s up_ms=%llu event=%.24s heap=%u min_heap=%u largest=%u wifi=%d rssi=%d %.*s\n",
        stamp, (unsigned long long)(esp_timer_get_time() / 1000), name,
        (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), wifi,
        wifi == WL_CONNECTED ? WiFi.RSSI() : 0, 190, detail);
}

void writer(void*) {
    Record record;
    for (;;) {
        if (xQueueReceive(queue, &record, portMAX_DELAY) != pdTRUE) continue;
        if (!storage->appendDiagnostic(record.text)) {
            const uint32_t failures = writeFailures.fetch_add(1);
            if (failures == 0) Serial.println("[DIAG] Falha ao gravar diagnostico no SD.");
            // Evita insistir continuamente em um cartao com falha.
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
}

namespace Diagnostics {
void begin(SDUtil& sd) {
    if (started) return;
    started = true;
    storage = &sd;
    Record boot;
    char detail[192];
    const esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_POWERON || marker.magic != markerMagic || marker.pending > 1)
        marker = {markerMagic, 0, 0, 0};
    const char* reset = reason == ESP_RST_PANIC ? "PANIC" :
        reason == ESP_RST_TASK_WDT ? "TASK_WDT" : reason == ESP_RST_INT_WDT ? "INT_WDT" :
        reason == ESP_RST_WDT ? "WDT" : reason == ESP_RST_BROWNOUT ? "BROWNOUT" :
        reason == ESP_RST_POWERON ? "POWERON" : reason == ESP_RST_SW ? "SOFTWARE" : "OUTRO";
    snprintf(detail, sizeof(detail), "reset=%s reset_code=%d build=%s_%s", reset, int(reason), __DATE__, __TIME__);
    makeRecord(boot, "BOOT", detail);
    // Registra o boot antes de iniciar tela, Wi-Fi e demais tarefas.
    if (!sd.appendDiagnostic(boot.text)) Serial.println("[DIAG] SD indisponivel para registrar o boot.");
    if (marker.magic == markerMagic && marker.pending == 1) {
        snprintf(detail, sizeof(detail), "request=%lu started_up_ms=%llu reinicio_com_consulta_pendente=1",
            (unsigned long)marker.id, (unsigned long long)marker.upMs);
        makeRecord(boot, "METRICS_INTERRUPTED", detail);
        if (sd.appendDiagnostic(boot.text)) marker.pending = 0;
    }
    queue = xQueueCreate(8, sizeof(Record));
    if (!queue) {
        makeRecord(boot, "LOGGER_UNAVAILABLE", "reason=queue_allocation_failed");
        sd.appendDiagnostic(boot.text);
        Serial.println("[DIAG] Sem memoria para fila de diagnostico."); return;
    }
    if (xTaskCreate(writer, "sd_diagnostics", 4096, nullptr, 1, nullptr) != pdPASS) {
        vQueueDelete(queue); queue = nullptr;
        makeRecord(boot, "LOGGER_UNAVAILABLE", "reason=task_allocation_failed");
        sd.appendDiagnostic(boot.text);
        Serial.println("[DIAG] Sem memoria para tarefa de diagnostico.");
    }
}

void event(const char* name, const char* detail) {
    if (!queue) return;
    Record record;
    makeRecord(record, name, detail);
    if (xQueueSend(queue, &record, 0) != pdTRUE) ++dropped;
}

void wifiDisconnected(uint32_t reason) {
    // Callback de Wi-Fi apenas publica dados; nao acessa o SD.
    disconnectReason.store(reason);
    ++disconnectCount;
}

void process() {
    if (!started) return;
    const uint32_t now = millis();
    const uint32_t disconnects = disconnectCount.load();
    if (disconnects != lastDisconnects) {
        char detail[100];
        snprintf(detail, sizeof(detail), "count=%lu delta=%lu last_reason=%lu",
            (unsigned long)disconnects, (unsigned long)(disconnects - lastDisconnects),
            (unsigned long)disconnectReason.load());
        event("WIFI_DISCONNECTED", detail);
        lastDisconnects = disconnects;
    }
    const int wifi = WiFi.status();
    if (wifi != lastWifi) {
        char detail[80];
        snprintf(detail, sizeof(detail), "previous=%d current=%d", lastWifi, wifi);
        event("WIFI_STATE", detail);
        lastWifi = wifi;
    }
    if (now - lastHeartbeat >= 60000UL) {
        lastHeartbeat = now;
        char detail[120];
        snprintf(detail, sizeof(detail), "queue_dropped=%lu sd_write_failures=%lu logger=%s",
            (unsigned long)dropped.load(), (unsigned long)writeFailures.load(), queue ? "active" : "unavailable");
        event("HEARTBEAT", detail);
    }
}

uint32_t metricsStart(uint32_t peerIp) {
    ++marker.id;
    marker.magic = markerMagic;
    marker.upMs = esp_timer_get_time() / 1000;
    marker.pending = 1;
    char detail[80];
    const IPAddress peer(peerIp);
    snprintf(detail, sizeof(detail), "request=%lu peer=%u.%u.%u.%u", (unsigned long)marker.id,
        unsigned(peer[0]), unsigned(peer[1]), unsigned(peer[2]), unsigned(peer[3]));
    event("METRICS_START", detail);
    return marker.id;
}

void metricsEnd(uint32_t id, size_t bytes, uint32_t durationMs, int code, bool connected) {
    char detail[160];
    snprintf(detail, sizeof(detail), "request=%lu bytes_generated=%u duration_ms=%lu http=%d client_connected=%d send_returned=1",
        (unsigned long)id, (unsigned)bytes, (unsigned long)durationMs, code, connected ? 1 : 0);
    event("METRICS_END", detail);
    marker.pending = 0;
}
}
