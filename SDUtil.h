#ifndef SDUTIL_H
#define SDUTIL_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class SDUtil
{
public:
    bool begin();
    bool test();
    String getCardType();
    uint64_t getCardSizeBytes();

    bool exists(String path);

    bool writeText(String path, String text);

    bool appendText(String path, String text);

    String readText(String path);

    bool removeFile(String path);

    bool createDir(String path);

    String listFiles(String path = "/");

private:
    SPIClass sdSPI = SPIClass(FSPI);

    const int SD_CS = 4;
    const int SD_MISO = 5;
    const int SD_MOSI = 6;
    const int SD_SCLK = 7;
};

#endif