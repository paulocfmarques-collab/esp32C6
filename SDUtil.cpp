#include "SDUtil.h"

bool SDUtil::begin()
{
    sdSPI.begin(
        SD_SCLK,
        SD_MISO,
        SD_MOSI,
        SD_CS);

    return SD.begin(SD_CS, sdSPI);
}

bool SDUtil::test()
{
    String path = "/sd_test_" + String(millis()) + ".tmp";
    while (exists(path))
    {
        path = "/sd_test_" + String(millis()) + "_" + String(random(0x7fffffff)) + ".tmp";
    }

    const String expected = "ESP32-C6 SD test";
    if (!writeText(path, expected))
    {
        removeFile(path);
        return false;
    }

    bool readSucceeded = readText(path) == expected;
    bool removeSucceeded = removeFile(path);
    return readSucceeded && removeSucceeded;
}

String SDUtil::getCardType()
{
    switch (SD.cardType())
    {
        case CARD_MMC:
            return "MMC";
        case CARD_SD:
            return "SDSC";
        case CARD_SDHC:
            return "SDHC/SDXC";
        default:
            return "Desconhecido";
    }
}

uint64_t SDUtil::getCardSizeBytes()
{
    return SD.cardSize();
}

bool SDUtil::exists(String path)
{
    return SD.exists(path);
}

bool SDUtil::writeText(String path, String text)
{
    File file = SD.open(path, FILE_WRITE);

    if (!file)
        return false;

    size_t bytesWritten = file.print(text);

    file.close();

    return bytesWritten == text.length();
}

bool SDUtil::appendText(String path, String text)
{
    File file = SD.open(path, FILE_APPEND);

    if (!file)
        return false;

    size_t bytesWritten = file.print(text);

    file.close();

    return bytesWritten == text.length();
}

String SDUtil::readText(String path)
{
    File file = SD.open(path);

    if (!file)
        return "";

    String content;

    while (file.available())
    {
        content += (char)file.read();
    }

    file.close();

    return content;
}

bool SDUtil::removeFile(String path)
{
    return SD.remove(path);
}

bool SDUtil::createDir(String path)
{
    return SD.mkdir(path);
}

String SDUtil::listFiles(String path)
{
    String result;

    File root = SD.open(path);

    if (!root)
        return "Erro ao abrir diretorio";

    File file = root.openNextFile();

    while (file)
    {
        result += file.name();
        result += " (";
        result += String(file.size());
        result += " bytes)\n";

        file.close();
        file = root.openNextFile();
    }

    root.close();
    return result;
}