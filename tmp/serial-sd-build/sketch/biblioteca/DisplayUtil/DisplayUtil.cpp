#line 1 "C:\\PC\\ESP32\\ESP32C6\\biblioteca\\DisplayUtil\\DisplayUtil.cpp"
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