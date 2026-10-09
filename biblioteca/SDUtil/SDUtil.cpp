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

bool SDUtil::exists(String path)
{
    return SD.exists(path);
}

bool SDUtil::writeText(String path, String text)
{
    File file = SD.open(path, FILE_WRITE);

    if (!file)
        return false;

    file.print(text);

    file.close();

    return true;
}

bool SDUtil::appendText(String path, String text)
{
    File file = SD.open(path, FILE_APPEND);

    if (!file)
        return false;

    file.print(text);

    file.close();

    return true;
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

        file = root.openNextFile();
    }

    return result;
}