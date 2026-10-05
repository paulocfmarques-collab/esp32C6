#include "DisplayUtil.h"
#include "ClimaManager.h"
#include <WiFi.h>

#define TFT_BL 22

#define COLOR_CYAN    0x07FF
#define COLOR_DARK    0x0000
#define COLOR_GRAY    0x5AEB
#define COLOR_WHITE   0xFFFF
#define COLOR_GREEN   0x07E0
#define COLOR_MAGENTA 0xF81F

DisplayUtil::DisplayUtil() {
    textColor = COLOR_WHITE;
    backgroundColor = COLOR_DARK;
    textSize = 2;
    ultimaHora = "";
    ultimaData = "";
    ponteiroHistorico = 0;
    sdGravandoAnimacao = false;
    fimAnimacaoSD = 0;
    for (int i = 0; i < 50; i++) historicoRSSI[i] = -100;
}

void DisplayUtil::begin() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    Arduino_DataBus* bus = new Arduino_HWSPI(15, 14, 7, 6, GFX_NOT_DEFINED);
    gfx = new Arduino_ST7789(bus, 21, 0, true, 172, 320, 34, 0, 34, 0);
    gfx->begin();
    gfx->setRotation(1); 
    clear();
}

void DisplayUtil::clear() {
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    gfx->fillScreen(backgroundColor);
}

void DisplayUtil::setTextColor(uint16_t color) { textColor = color; }
void DisplayUtil::setBackgroundColor(uint16_t color) { backgroundColor = color; }
void DisplayUtil::setTextSize(uint8_t size) { textSize = size; }

void DisplayUtil::setRotation(uint8_t rotation) {
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    gfx->setRotation(rotation);
    clear();
}

void DisplayUtil::dispararAnimacaoGravacaoSD() {
    sdGravandoAnimacao = true;
    fimAnimacaoSD = millis() + 400; 
}

void DisplayUtil::showClock(String dateTime) {
    gfx->setRotation(1); 

    if (dateTime == "Erro ao obter data e hora" || dateTime.length() < 19) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(5, 5, gfx->width() - 10, gfx->height() - 10, 0xF800); 
        gfx->setTextColor(0xF800);
        gfx->setTextSize(2);
        
        String msgErro = "AGUARDANDO CONEXAO NTP...";
        int errX = (gfx->width() - (msgErro.length() * 12)) / 2;
        gfx->setCursor(errX, gfx->height() / 2 - 8);
        gfx->print(msgErro);
        return;
    }

    int separator = dateTime.indexOf(' ');
    String dataStr = separator >= 0 ? dateTime.substring(0, separator) : dateTime;
    String horaCompleta = separator >= 0 ? dateTime.substring(separator + 1) : "";
    
    String stringHora = horaCompleta.substring(0, 2);
    String stringMinuto = horaCompleta.substring(3, 5);
    String segundos = horaCompleta.substring(6, 8);   

    int rssiAtual = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100;

    if (segundos != ultimaHora.substring(6, 8)) {
        historicoRSSI[ponteiroHistorico] = rssiAtual;
        ponteiroHistorico = (ponteiroHistorico + 1) % 50;
    }

    if (ultimaData != dataStr) {
        ultimaData = dataStr;
        gfx->fillScreen(backgroundColor);
        
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->drawRect(8, 8, gfx->width() - 16, gfx->height() - 16, 0x18E3); 

        gfx->fillRect(12, 12, gfx->width() - 24, 22, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(1);
        gfx->setCursor(20, 19);
        gfx->print("GATEWAY ESP32-C6 // TECH PANEL");

        gfx->setTextColor(COLOR_GREEN);
        gfx->setTextSize(2);
        int dataX = ((gfx->width() - (dataStr.length() * 12)) / 2) - 25;
        gfx->setCursor(dataX, gfx->height() - 36);
        gfx->print(dataStr);
    }

    // --- INDICADOR DE SINAL DE CELULAR VERDE ---
    int barrasAtivas = 0;
    if (rssiAtual > -60) barrasAtivas = 4;      
    else if (rssiAtual > -70) barrasAtivas = 3; 
    else if (rssiAtual > -80) barrasAtivas = 2; 
    else if (rssiAtual > -90) barrasAtivas = 1; 
    
    int baseX = gfx->width() - 40; 
    int baseY = 28;                
    gfx->fillRect(baseX - 2, baseY - 12, 25, 14, 0x10A2);

    for (int b = 0; b < 4; b++) {
        int barHeight = (b + 1) * 3; 
        int barX = baseX + (b * 5);  
        int barY = baseY - barHeight;
        if (b < barrasAtivas) {
            gfx->fillRect(barX, barY, 3, barHeight, COLOR_GREEN); 
        } else {
            gfx->drawRect(barX, barY, 3, barHeight, 0x4A49);       
        }
    }

    // --- ATUALIZAÇÃO DA HORA COM DOIS PONTOS PISCANTE ---
    int intSegundos = segundos.toInt();
    bool mostrarPontos = (intSegundos % 2 == 0); 

    static bool ultimoEstadoPontos = false;
    String horaMinutoJuncao = stringHora + (mostrarPontos ? ":" : " ") + stringMinuto;

    if (ultimaHora.substring(0, 5) != horaCompleta.substring(0, 5) || mostrarPontos != ultimoEstadoPontos) {
        ultimoEstadoPontos = mostrarPontos;
        gfx->fillRect(15, 45, 180, 50, backgroundColor); 
        gfx->setTextColor(COLOR_WHITE);
        gfx->setTextSize(6); 
        gfx->setCursor(18, 46);
        gfx->print(horaMinutoJuncao);
    }

    // --- ATUALIZAÇÃO DOS SEGUNDOS PEQUENOS ---
    int segX = 198; 
    gfx->fillRect(segX, 46, 26, 16, backgroundColor); 
    gfx->setTextColor(COLOR_CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(segX, 46);
    gfx->print(segundos);
    
    ultimaHora = horaCompleta;

    // --- ANIMAÇÃO DO ÍCONE DO CARTÃO SD ---
    int sdX = 20, sdY = gfx->height() - 36;
    if (sdGravandoAnimacao && millis() < fimAnimacaoSD) {
        gfx->fillRect(sdX, sdY, 14, 18, COLOR_MAGENTA);
        gfx->fillTriangle(sdX + 10, sdY, sdX + 14, sdY, sdX + 14, sdY + 4, backgroundColor);
    } else {
        sdGravandoAnimacao = false;
        gfx->fillRect(sdX, sdY, 14, 18, 0x31A6);
        gfx->fillTriangle(sdX + 10, sdY, sdX + 14, sdY, sdX + 14, sdY + 4, backgroundColor);
        gfx->drawRect(sdX, sdY, 14, 18, COLOR_GRAY);
    }

    // --- CLIMA E PREVISÃO EXTREMA DIREITA ---
    int climaX = gfx->width() - 105; 
    gfx->fillRect(climaX, gfx->height() - 40, 90, 22, backgroundColor); 
    
    gfx->setTextColor(COLOR_CYAN);
    gfx->setTextSize(1);
    gfx->setCursor(climaX, gfx->height() - 38);
    gfx->print(ClimaManager::obterTextoCondicao());
    
    gfx->setCursor(climaX, gfx->height() - 26);
    gfx->printf("%.1f C", ClimaManager::temperatura);

    // --- DIMINUIÇÃO DA BARRA DE OSCILAÇÃO (LARGURA MENOR CORRIGIDA) ---
    int graphX = gfx->width() - 92; 
    int graphY = 52;
    int graphWidth = 66;            
    int graphHeight = 44;

    gfx->drawRect(graphX, graphY, graphWidth, graphHeight, 0x18E3);
    gfx->fillRect(graphX + 1, graphY + 1, graphWidth - 2, graphHeight - 2, backgroundColor);
    gfx->drawFastHLine(graphX, graphY + (graphHeight / 2), graphWidth, 0x0841);

    for (int i = 0; i < 32; i++) {
        int idx1 = (ponteiroHistorico + i) % 50;
        int idx2 = (ponteiroHistorico + i + 1) % 50;

        int h1 = map(constrain(historicoRSSI[idx1], -95, -45), -95, -45, 2, graphHeight - 4);
        int h2 = map(constrain(historicoRSSI[idx2], -95, -45), -95, -45, 2, graphHeight - 4);

        int x1 = graphX + (i * 2);
        int x2 = graphX + ((i + 1) * 2);
        int y1 = graphY + graphHeight - h1 - 2;
        int y2 = graphY + graphHeight - h2 - 2;

        if (historicoRSSI[idx1] > -100 && historicoRSSI[idx2] > -100) {
            gfx->drawLine(x1, y1, x2, y2, COLOR_GREEN);
        }
    }

    // --- BARRINHA DE PROGRESSO DOS SEGUNDOS ---
    int secNum = segundos.toInt();
    int larguraMaximaBarra = gfx->width() - 48; 
    int barWidth = map(secNum, 0, 59, 0, larguraMaximaBarra);
    
    gfx->fillRect(24, 108, larguraMaximaBarra, 4, 0x10A2); 
    gfx->fillRect(24, 108, barWidth, 4, COLOR_CYAN);      
}

void DisplayUtil::desenharMatrixScreensaver() {
    int colunas = gfx->width() / 12;
    for (int i = 0; i < 3; i++) { 
        int x = random(0, colunas) * 12;
        int y = random(0, gfx->height() / 16) * 16;
        
        gfx->fillRect(x, y, 10, 14, backgroundColor); 
        
        uint16_t tomVerde = (random(0, 10) > 3) ? COLOR_GREEN : 0x0400;
        gfx->setTextColor(tomVerde);
        gfx->setTextSize(1);
        gfx->setCursor(x, y);
        gfx->print((char)random(33, 126)); 
    }
    delay(10); 
}

void DisplayUtil::print(String text) { println(text); }

void DisplayUtil::println(String text) {
    text.replace("\r", ""); text.replace("\n", "");
    int charWidth = 6 * 2; 
    int maxChars = (gfx->width() - 24) / charWidth;
    while (text.length() > maxChars) {
        addLine(text.substring(0, maxChars));
        text = text.substring(maxChars);
    }
    if (text.length() > 0) addLine(text);
    redraw();
}

void DisplayUtil::addLine(String line) {
    lines.push_back(line);
    int lineHeight = 8 * 2; 
    int maxLines = (gfx->height() - 45) / lineHeight; 
    while ((int)lines.size() > maxLines) lines.erase(lines.begin());
}

void DisplayUtil::redraw() {
    gfx->fillScreen(backgroundColor);
    gfx->fillRect(0, 0, gfx->width(), 26, 0x2104); 
    gfx->drawFastHLine(0, 26, gfx->width(), COLOR_CYAN); 
    gfx->setTextColor(COLOR_WHITE); gfx->setTextSize(1); gfx->setCursor(12, 9);
    gfx->print("> CONSOLE LOG INTERFACES_");
    int y = 35; int lineHeight = 8 * 2; gfx->setTextSize(2);
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].startsWith(">")) { gfx->setTextColor(COLOR_GREEN); gfx->setCursor(8, y); }
        else { gfx->setTextColor(0xCE79); gfx->setCursor(12, y); }
        gfx->println(lines[i]); y += lineHeight;
    }
}

Arduino_ST7789* DisplayUtil::getDisplay() { return gfx; }
