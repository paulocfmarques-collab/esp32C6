#ifndef DISPLAY_UTIL_H
#define DISPLAY_UTIL_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <vector>

class DisplayUtil {
private:
    Arduino_ST7789* gfx;
    std::vector<String> lines;
    uint16_t textColor;
    uint16_t backgroundColor;
    uint8_t textSize;
    
    String ultimaHora;
    String ultimaData;

    int historicoRSSI[50]; // Tamanho fixo do array explícito
    int ponteiroHistorico;

    bool sdGravandoAnimacao;
    uint32_t fimAnimacaoSD;

public:
    DisplayUtil();
    void begin();
    void clear();
    void setTextColor(uint16_t color);
    void setBackgroundColor(uint16_t color);
    void setTextSize(uint8_t size);
    void setRotation(uint8_t rotation);
    
    void showClock(String dateTime);
    void desenharMatrixScreensaver(); 
    void dispararAnimacaoGravacaoSD(); 
    
    void print(String text);
    void println(String text);
    void addLine(String line);
    void redraw();
    
    Arduino_ST7789* getDisplay();
};

#endif
