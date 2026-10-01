#include "DisplayUtil.h"

#define TFT_BL 22

DisplayUtil::DisplayUtil()
{
    textColor = 0xFFFF;
    backgroundColor = 0x0000;
    textSize = 2;
}

void DisplayUtil::begin()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    Arduino_DataBus* bus = new Arduino_HWSPI(
        15,
        14,
        7,
        6,
        GFX_NOT_DEFINED);

    gfx = new Arduino_ST7789(
        bus,
        21,
        0,
        true,
        172,
        320,
        34,
        0,
        34,
        0);

    gfx->begin();
    gfx->setRotation(0);

    clear();
}

void DisplayUtil::clear()
{
    lines.clear();
    gfx->fillScreen(backgroundColor);
}

void DisplayUtil::setTextColor(uint16_t color)
{
    textColor = color;
}

void DisplayUtil::setBackgroundColor(uint16_t color)
{
    backgroundColor = color;
}

void DisplayUtil::setTextSize(uint8_t size)
{
    textSize = size;
}

void DisplayUtil::setRotation(uint8_t rotation)
{
    lines.clear();
    gfx->setRotation(rotation);
    clear();
}

void DisplayUtil::showClock(String dateTime)
{
    if (dateTime == "Erro ao obter data e hora")
    {
        String message = "Sem hora NTP";
        gfx->fillScreen(backgroundColor);
        gfx->setTextColor(textColor);
        gfx->setTextSize(2);
        int messageX = (gfx->width() - static_cast<int>(message.length()) * 12) / 2;
        gfx->setCursor(messageX, gfx->height() / 2 - 8);
        gfx->print(message);
        return;
    }

    int separator = dateTime.indexOf(' ');
    String date = separator >= 0 ? dateTime.substring(0, separator) : dateTime;
    String time = separator >= 0 ? dateTime.substring(separator + 1) : "";
    if (time.length() > 8)
    {
        time = time.substring(0, 8);
    }

    gfx->fillScreen(backgroundColor);
    gfx->setTextColor(textColor);
    gfx->setTextSize(3);

    int dateX = (gfx->width() - static_cast<int>(date.length()) * 18) / 2;
    int timeX = (gfx->width() - static_cast<int>(time.length()) * 18) / 2;
    gfx->setCursor(dateX, gfx->height() / 2 - 28);
    gfx->print(date);
    if (time.length() > 0)
    {
        gfx->setCursor(timeX, gfx->height() / 2 + 4);
        gfx->print(time);
    }
}

void DisplayUtil::print(String text)
{
    println(text);
}

void DisplayUtil::println(String text)
{
    int charWidth = 6 * textSize;
    int maxChars = gfx->width() / charWidth;

    while (text.length() > maxChars)
    {
        addLine(text.substring(0, maxChars));
        text = text.substring(maxChars);
    }

    addLine(text);

    redraw();
}

void DisplayUtil::addLine(String line)
{
    lines.push_back(line);

    int lineHeight = 8 * textSize;
    int maxLines = gfx->height() / lineHeight;

    while ((int)lines.size() > maxLines)
    {
        lines.erase(lines.begin());
    }
}

void DisplayUtil::redraw()
{
    gfx->fillScreen(backgroundColor);

    gfx->setTextColor(textColor);
    gfx->setTextSize(textSize);

    int y = 0;
    int lineHeight = 8 * textSize;

    for (size_t i = 0; i < lines.size(); i++)
    {
        gfx->setCursor(0, y);
        gfx->println(lines[i]);
        y += lineHeight;
    }
}

Arduino_ST7789* DisplayUtil::getDisplay()
{
    return gfx;
}