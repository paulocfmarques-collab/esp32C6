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
    bool statusPageInitialized;

public:
    DisplayUtil();
    void begin();
    void clear();
    void setTextColor(uint16_t color);
    void setBackgroundColor(uint16_t color);
    void setTextSize(uint8_t size);
    void setRotation(uint8_t rotation);
    
    void showClock(String dateTime);
    void showStatusPage(bool sdReady, bool ntpSynchronized, const String& ntpDateTime,
                        int32_t timezoneOffset, bool daylightSaving, uint8_t part = 0);
    void showNetworkPage(uint8_t part = 0);
    void showSdLoading(uint8_t frame);
    void showSystemPage();
    void showSavedWifiPage(const String ssids[5], int connectedSlot, uint8_t nextSlot);
    void showHoldMessage(const String& line1, const String& line2, uint16_t color);
    void showSdPage(bool sdReady, const String& cardType, uint64_t totalBytes,
                    uint64_t usedBytes);
    void desenharMatrixScreensaver(); 
    void dispararAnimacaoGravacaoSD(); 
    
    void print(String text);
    void println(String text);
    void addLine(String line);
    void redraw();
    
    Arduino_ST7789* getDisplay();
};

#endif
