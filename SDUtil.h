#ifndef SDUTIL_H
#define SDUTIL_H

#include <Arduino.h>
#include <SPI.h>
#include "SdFat.h" // Substitui a biblioteca SD padrão

class SDUtil
{
public:
    bool begin();
    bool test();
    String getCardType();
    uint64_t getCardSizeBytes();
    uint64_t getUsedBytes();

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
    bool removeDir(String path);

private:
    SdFat sdCard; // Nova instância estável gerenciada pela SdFat

    // Pinos oficiais da placa Waveshare ESP32-C6-LCD-1.47
    const int SD_CS = 4;
    const int SD_MISO = 5;
    const int SD_MOSI = 6;
    const int SD_SCLK = 7;
};

#endif
