#include "RGBLed.h"

void RGBLed::begin()
{
    pixel.begin();
    pixel.setBrightness(currentBrightness);
    pixel.clear();
    pixel.show();
}

void RGBLed::setBrightness(uint8_t brightness)
{
    currentBrightness = brightness;
    pixel.setBrightness(brightness);
    pixel.show();
}

void RGBLed::off()
{
    pixel.clear();
    pixel.show();
}

void RGBLed::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    pixel.setPixelColor(0, pixel.Color(g, r, b));
    pixel.show();
}

void RGBLed::red()
{
    setColor(255, 0, 0);
}

void RGBLed::green()
{
    setColor(0, 255, 0);
}

void RGBLed::blue()
{
    setColor(0, 0, 255);
}

void RGBLed::yellow()
{
    setColor(255, 255, 0);
}

void RGBLed::cyan()
{
    setColor(0, 255, 255);
}

void RGBLed::magenta()
{
    setColor(255, 0, 255);
}

void RGBLed::white()
{
    setColor(255, 255, 255);
}

void RGBLed::breathing(uint8_t r, uint8_t g, uint8_t b, uint16_t stepDelay)
{
    pixel.setPixelColor(0, pixel.Color(g, r, b));

    for (int brightness = 0; brightness <= 255; brightness++)
    {
        pixel.setBrightness(brightness);
        pixel.show();
        delay(stepDelay);
    }

    for (int brightness = 255; brightness >= 0; brightness--)
    {
        pixel.setBrightness(brightness);
        pixel.show();
        delay(stepDelay);
    }
}

void RGBLed::breathingTask(uint8_t r, uint8_t g, uint8_t b)
{
    static int brightness = 0;
    static int direction = 1;
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate < 10)
        return;

    lastUpdate = millis();

    pixel.setPixelColor(0, pixel.Color(g, r, b));
    pixel.setBrightness(brightness);
    pixel.show();

    brightness += direction;

    if (brightness >= 255)
    {
        brightness = 255;
        direction = -1;
    }

    if (brightness <= 0)
    {
        brightness = 0;
        direction = 1;
    }
}

void RGBLed::blink(uint8_t color, uint16_t interval, uint16_t timeBlink)
{
    unsigned long startTime = millis();
    while (millis() - startTime < timeBlink)
    {
        setColorByEnum(color);
        delay(interval);
        off();
        delay(interval);
    }
}

void RGBLed::setColorByEnum(uint8_t color)
{
    switch(color)
    {
        case RED:
            red();
            break;
        case GREEN:
            green();
            break;
        case BLUE:
            blue();
            break;
        case YELLOW:
            yellow();
            break;
        case CYAN:
            cyan();
            break;
        case MAGENTA:
            magenta();
            break;
        case WHITE:
            white();
            break;
    }
}