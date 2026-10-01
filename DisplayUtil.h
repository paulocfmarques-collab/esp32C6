#ifndef DISPLAY_UTIL_H
#define DISPLAY_UTIL_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <vector>

#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define ORANGE  0xFD20

class DisplayUtil
{
public:
    DisplayUtil();

    void begin();

    void clear();

    void print(String text);

    void println(String text);

    void setTextColor(uint16_t color);

    void setBackgroundColor(uint16_t color);

    void setTextSize(uint8_t size);

    void setRotation(uint8_t rotation);

    void showClock(String dateTime);

    Arduino_ST7789 *getDisplay();

private:
    Arduino_ST7789 *gfx;
    std::vector<String> lines;

    uint16_t textColor;
    uint16_t backgroundColor;

    uint8_t textSize;

    void redraw();
    void addLine(String line);
};

#endif