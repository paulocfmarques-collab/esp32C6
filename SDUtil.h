#ifndef SDUTIL_H
#define SDUTIL_H

#include <Arduino.h>
#include <SPI.h>
#include "SdFat.h" // Substitui a biblioteca SD padrão
#include <atomic>

struct SdMetricsSnapshot
{
    bool mounted = false, valid = false;
    uint64_t capacity = 0, total = 0, used = 0;
    bool logValid[3]{};
    uint64_t logBytes[3]{};
    int64_t updatedUs = 0;
    uint32_t appends = 0, appendErrors = 0;
    uint32_t logRecords = 0, logErrors = 0;
};

class SDUtil
{
public:
    bool begin();
    bool appendDiagnostic(const char* record);
    bool salvarCrash(const String& relatorio, String& caminho, String& erro);
    bool test();
    String getCardType();
    uint64_t getCardSizeBytes();
    uint64_t getUsedBytes(bool* valid = nullptr);
    uint64_t getUsedBytesCooperative(bool* valid = nullptr);
    void processMetrics();
    SdMetricsSnapshot metricsSnapshot();
    inline static constexpr const char* METRIC_LOG_PATHS[] =
        {"/log.txt", "/network_history.log", "/network_history.previous.log"};

    bool exists(String path);
    bool writeText(String path, String text);
    bool appendText(String path, String text);
    String readText(String path);
    bool removeFile(String path);
    bool createDir(String path);
    String listFiles(String path = "/");
    bool writeJson(String path, String jsonContent);
    String readJson(String path);
    uint64_t getFileSize(String path);
    String readTail(String path, size_t maxBytes);
    // Retorna um status HTTP e limita a leitura para o portal de logs.
    int readLogTail(const char* path, String& content, bool& truncated);
    bool removeDir(String path);
    bool renameFile(const String& from,const String& to);

private:
    SdFat sdCard; // Nova instância estável gerenciada pela SdFat
    bool mounted_ = false;
    uint32_t mountGeneration = 0;
    SdMetricsSnapshot metrics;
    portMUX_TYPE metricsMux = portMUX_INITIALIZER_UNLOCKED;
    bool metricsBusy = false, metricsAttempted = false;
    uint32_t metricsAttemptAt = 0;
    std::atomic<uint32_t> appends{0}, appendErrors{0};
    std::atomic<uint32_t> logRecords{0}, logErrors{0};
    bool recordLogWrite(bool success) {
        if (success) ++logRecords; else ++logErrors;
        return success;
    }
    static void metricsTask(void* context);
    void refreshMetrics();

    // Pinos oficiais da placa Waveshare ESP32-C6-LCD-1.47
    const int SD_CS = 4;
    const int SD_MISO = 5;
    const int SD_MOSI = 6;
    const int SD_SCLK = 7;
};

#endif
