#line 1 "C:\\PC\\ESP32\\ESP32C6\\biblioteca\\RGBLed\\RGBLed.h"
#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class RGBLed
{
public:
    void begin();

    void off();

    void red();
    void green();
    void blue();

    void yellow();
    void cyan();
    void magenta();
    void white();

    void setColor(uint8_t r, uint8_t g, uint8_t b);

    void setBrightness(uint8_t brightness);

    void breathing(uint8_t r, uint8_t g, uint8_t b, uint16_t stepDelay = 5);

    void breathingTask(uint8_t r, uint8_t g, uint8_t b);

private:
    Adafruit_NeoPixel pixel =
        Adafruit_NeoPixel(1, 8, NEO_GRB + NEO_KHZ800);

    uint8_t currentBrightness = 50;
};

#endif