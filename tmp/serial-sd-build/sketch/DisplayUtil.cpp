#line 1 "C:\\PC\\ESP32\\ESP32C6\\DisplayUtil.cpp"
#include "SharedSpi.h"
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
    statusPageInitialized = false;
    for (int i = 0; i < 50; i++) historicoRSSI[i] = -100;
}

void DisplayUtil::begin() {
    SharedSpi::Guard spiGuard;
    pinMode(TFT_BL, OUTPUT);
    backlightReady_=ledcAttach(TFT_BL,5000,8);
    if(!backlightReady_)Serial.println("Backlight PWM unavailable; using on/off");
    setBacklight(brightness_);wake();

    // DC, CS, SCLK, MOSI, MISO: mesma pinagem do SD no SPI compartilhado.
    Arduino_DataBus* bus = new Arduino_HWSPI(15, 14, 7, 6, 5, &SPI, true);
    gfx = new Arduino_ST7789(bus, 21, 0, true, 172, 320, 34, 0, 34, 0);
    gfx->begin();
    gfx->setRotation(1); 
    clear();
}

void DisplayUtil::clear() {
    SharedSpi::Guard spiGuard;
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    statusPageInitialized = false;
    gfx->fillScreen(backgroundColor);
}

void DisplayUtil::setTextColor(uint16_t color) {
    SharedSpi::Guard spiGuard; textColor = color; }
void DisplayUtil::setBackgroundColor(uint16_t color) {
    SharedSpi::Guard spiGuard; backgroundColor = color; }
void DisplayUtil::setTextSize(uint8_t size) {
    SharedSpi::Guard spiGuard; textSize = size; }

void DisplayUtil::setRotation(uint8_t rotation) {
    SharedSpi::Guard spiGuard;
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    gfx->setRotation(rotation);
    clear();
}

void DisplayUtil::dispararAnimacaoGravacaoSD() {
    SharedSpi::Guard spiGuard;
    sdGravandoAnimacao = true;
    fimAnimacaoSD = millis() + 400; 
}

void DisplayUtil::showClock(String dateTime) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1); 

    if (dateTime == "Erro ao obter data e hora" || dateTime.length() < 19) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(5, 5, gfx->width() - 10, gfx->height() - 10, 0xF800); 
        gfx->setTextColor(0xF800);
        gfx->setTextSize(2);
        
        String msgErro = "AGUARDANDO NTP...";
        int errX = (gfx->width() - (msgErro.length() * 12)) / 2;
        gfx->setCursor(errX, gfx->height() / 2 - 8);
        gfx->print(msgErro);
        gfx->setTextSize(1);gfx->setCursor(14,112);
        gfx->print(connecting_.length() ? "Buscando: "+connecting_ : "Aguardando rede / horario");
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
            gfx->fillRect(barX, barY, 3, barHeight, barrasAtivas>=3 ? COLOR_GREEN : barrasAtivas==2 ? 0xFFE0 : 0xF800); 
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
    gfx->print(ClimaManager::snapshot().sincronizado ? ClimaManager::obterTextoCondicao() : String("Clima..."));
    
    gfx->setCursor(climaX, gfx->height() - 26);
    if (ClimaManager::snapshot().sincronizado) {
        gfx->printf("%.1f C", ClimaManager::snapshot().temperatura);
    } else {
        gfx->print("-- C");
    }

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

    if(WiFi.status()!=WL_CONNECTED){
        gfx->fillRect(16,114,288,14,backgroundColor);gfx->setTextSize(1);gfx->setTextColor(0xFFE0);gfx->setCursor(18,117);
        gfx->print(connecting_.length() ? "Buscando: "+connecting_ : "Sem rede: AP ESP32_C6_CONFIG");
    }else gfx->fillRect(16,114,288,14,backgroundColor);

    // --- BARRINHA DE PROGRESSO DOS SEGUNDOS ---
    int secNum = segundos.toInt();
    int larguraMaximaBarra = gfx->width() - 48; 
    int barWidth = map(secNum, 0, 59, 0, larguraMaximaBarra);
    
    gfx->fillRect(24, 108, larguraMaximaBarra, 4, 0x10A2); 
    gfx->fillRect(24, 108, barWidth, 4, COLOR_CYAN);      
}

void DisplayUtil::showStatusPage(bool sdReady, bool ntpSynchronized, const String& ntpDateTime,
                                 int32_t timezoneOffset, bool daylightSaving, uint8_t part) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print(part == 0 ? "NTP 1/2" : "NTP 2/2");
        statusPageInitialized = true;
    }

    String lines[5];
    uint16_t colors[5];
    bool wifiConectado = WiFi.status() == WL_CONNECTED;

    if (part == 0) {
        lines[0] = ntpSynchronized ? "SINCRONIZADO" : "AGUARDANDO";
        colors[0] = ntpSynchronized ? COLOR_GREEN : 0xF800;
        lines[1] = ntpSynchronized ? ntpDateTime.substring(0, 10) : String("--");
        lines[2] = ntpSynchronized ? ntpDateTime.substring(11, 19) : String("--");
        lines[3] = String("Fuso: UTC") + (timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset);
        lines[4] = String("DST: ") + (daylightSaving ? "ON" : "OFF");
        colors[1] = colors[2] = colors[3] = colors[4] = COLOR_WHITE;
    } else {
        unsigned long up = millis() / 1000UL;
        char upBuf[32];
        snprintf(upBuf, sizeof(upBuf), "UP: %lud %02lu:%02lu", up / 86400UL, (up / 3600UL) % 24UL, (up / 60UL) % 60UL);
        lines[0] = wifiConectado ? "Wi-Fi: conectado" : "Wi-Fi: desconectado";
        colors[0] = wifiConectado ? COLOR_GREEN : 0xF800;
        lines[1] = String("IP: ") + (wifiConectado ? WiFi.localIP().toString() : String("--"));
        lines[2] = String("RSSI: ") + (wifiConectado ? String(WiFi.RSSI()) + " dBm" : String("--"));
        lines[3] = upBuf;
        lines[4] = sdReady ? "SD: PRONTO" : "SD: INDISPONIVEL";
        colors[1] = colors[2] = colors[3] = COLOR_WHITE;
        colors[4] = sdReady ? COLOR_GREEN : 0xF800;
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++) {
        int y = 44 + (i * 24);
        gfx->fillRect(16, y, gfx->width() - 32, 20, backgroundColor);
        gfx->setTextColor(colors[i]);
        gfx->setCursor(20, y + 2);
        String l = lines[i];
        if (l.length() > 24) l = l.substring(0, 24);
        gfx->print(l);
    }
}

void DisplayUtil::showNetworkPage(uint8_t part) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print(part == 0 ? "REDE WI-FI 1/2" : "REDE WI-FI 2/2");
        statusPageInitialized = true;
    }

    bool connected = WiFi.status() == WL_CONNECTED;
    String lines[5];
    if (part == 0) {
        lines[0] = String("Estado: ") + (connected ? "ON" : "OFF");
        lines[1] = String("SSID: ") + (connected ? WiFi.SSID() : "--");
        lines[2] = String("IP: ") + (connected ? WiFi.localIP().toString() : "--");
        lines[3] = String("GW: ") + (connected ? WiFi.gatewayIP().toString() : "--");
        lines[4] = String("Mask: ") + (connected ? WiFi.subnetMask().toString() : "--");
    } else {
        lines[0] = String("DNS1: ") + (connected ? WiFi.dnsIP(0).toString() : "--");
        lines[1] = String("DNS2: ") + (connected ? WiFi.dnsIP(1).toString() : "--");
        lines[2] = String("MAC: ") + WiFi.macAddress();
        lines[3] = String("Canal: ") + (connected ? String(WiFi.channel()) : "--");
        lines[4] = String("RSSI: ") + (connected ? String(WiFi.RSSI()) + " dBm" : "--");
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++) {
        int y = 44 + (i * 22);
        gfx->fillRect(16, y, gfx->width() - 32, 18, backgroundColor);
        gfx->setTextColor(part == 0 && i == 0 ? (connected ? COLOR_GREEN : 0xF800) : COLOR_WHITE);
        gfx->setCursor(20, y + 1);
        if (lines[i].length() > 24) {
            lines[i] = lines[i].substring(0, 24);
        }
        gfx->print(lines[i]);
    }
}

void DisplayUtil::showSystemPage() {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print("SISTEMA");
        statusPageInitialized = true;
    }

    uint32_t heapTotal = ESP.getHeapSize();
    uint32_t heapFree = ESP.getFreeHeap();
    uint8_t heapPercent = heapTotal > 0 ? (heapFree * 100ULL) / heapTotal : 0;
    float temp = temperatureRead();
    String lines[5] = {
        String("CPU: ") + String(ESP.getCpuFreqMHz()) + " MHz",
        String("Temp: ") + String(temp, 1) + " C",
        String("RAM: ") + String(heapFree / 1024) + " KB (" + String(heapPercent) + "%)",
        String("RAM min: ") + String(ESP.getMinFreeHeap() / 1024) + " KB",
        String("Flash: ") + String(ESP.getSketchSize() * 100ULL / ESP.getFlashChipSize()) + "% usada"
    };

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++) {
        int y = 44 + (i * 22);
        gfx->fillRect(16, y, gfx->width() - 32, 18, backgroundColor);
        gfx->setTextColor(i == 1 && temp > 70.0f ? 0xF800 : COLOR_WHITE);
        gfx->setCursor(20, y + 1);
        gfx->print(lines[i]);
    }
}
void DisplayUtil::showSavedWifiPage(const String ssids[5], int connectedSlot, uint8_t nextSlot) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print("REDES SALVAS");
        statusPageInitialized = true;
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++) {
        int y = 44 + (i * 21);
        gfx->fillRect(16, y, gfx->width() - 32, 19, backgroundColor);
        bool vazio = ssids[i].length() == 0;
        bool conectada = static_cast<int>(i) == connectedSlot;
        gfx->setTextColor(conectada ? COLOR_GREEN : vazio ? COLOR_GRAY : COLOR_WHITE);
        gfx->setCursor(20, y + 2);
        String linha = String(i + 1) + (conectada ? "*" : (i == nextSlot ? ">" : " ")) +
                       (vazio ? String("(vazio)") : ssids[i]);
        if (linha.length() > 24) linha = linha.substring(0, 24);
        gfx->print(linha);
    }
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_GRAY);
    gfx->fillRect(16, 152, gfx->width() - 32, 10, backgroundColor);
    gfx->setCursor(20, 153);
    gfx->print("* conectada   > proxima a substituir");
}

void DisplayUtil::showHoldMessage(const String& line1, const String& line2, uint16_t color) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    gfx->fillScreen(backgroundColor);
    gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, color);
    gfx->setTextColor(color);
    gfx->setTextSize(3);
    gfx->setCursor((gfx->width() - line1.length() * 18) / 2, 60);
    gfx->print(line1);
    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor((gfx->width() - line2.length() * 12) / 2, 110);
    gfx->print(line2);
    statusPageInitialized = false;
}

void DisplayUtil::showSdLoading(uint8_t frame) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print("CARTAO SD");
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor((gfx->width() - 14 * 12) / 2, 140);
        gfx->print("Lendo cartao...");
        statusPageInitialized = true;
    }

    frame %= 16;
    const int cx = gfx->width() / 2;
    const int cy = 80;
    const uint8_t pontos = 16;
    gfx->fillRect(20, 42, gfx->width() - 40, 92, backgroundColor);

    // Cartao iluminado com trilhas e uma linha de leitura que o atravessa.
    gfx->fillRoundRect(cx - 21, cy - 29, 42, 58, 5, 0x10A2);
    gfx->drawRoundRect(cx - 21, cy - 29, 42, 58, 5, COLOR_CYAN);
    for (uint8_t i = 0; i < 4; ++i) {
        gfx->fillRect(cx - 14 + i * 8, cy - 23, 5, 11, 0xFEA0);
        gfx->drawFastVLine(cx - 12 + i * 8, cy - 9, 26, 0x2945);
    }
    const int scanY = cy - 8 + (frame * 2);
    gfx->drawFastHLine(cx - 17, scanY, 34, COLOR_CYAN);
    gfx->drawFastHLine(cx - 17, scanY + 1, 34, COLOR_WHITE);

    // Pacotes orbitando o cartao, com duas caudas em sentidos opostos.
    for (uint8_t i = 0; i < pontos; i++) {
        float ang = (i * 2.0f * PI) / pontos;
        int x = cx + static_cast<int>(cosf(ang) * 78);
        int y = cy + static_cast<int>(sinf(ang) * 29);
        uint8_t dist = (frame + pontos - i) % pontos;
        uint8_t opposite = (i + frame) % pontos;
        uint16_t cor = dist == 0 ? COLOR_WHITE : dist < 3 ? COLOR_CYAN :
                       opposite < 2 ? COLOR_MAGENTA : dist < 6 ? 0x0410 : 0x2945;
        gfx->fillCircle(x, y, dist == 0 ? 4 : 2, cor);
    }
    // Indicador de atividade; nao representa uma porcentagem de progresso.
    gfx->drawRoundRect(cx - 96, 118, 192, 9, 3, 0x2945);
    gfx->fillRoundRect(cx - 92 + frame * 9, 120, 45, 5, 2, COLOR_CYAN);
}
void DisplayUtil::showSdPage(bool sdReady, const String& cardType,
                             uint64_t totalBytes, uint64_t usedBytes) {
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized) {
        gfx->fillScreen(backgroundColor);
        gfx->drawRect(6, 6, gfx->width() - 12, gfx->height() - 12, COLOR_GRAY);
        gfx->fillRect(12, 12, gfx->width() - 24, 26, 0x10A2);
        gfx->setTextColor(COLOR_CYAN);
        gfx->setTextSize(2);
        gfx->setCursor(20, 18);
        gfx->print("CARTAO SD");
        statusPageInitialized = true;
    }

    const uint64_t bytesPerMiB = 1024ULL * 1024ULL;
    uint64_t usedMiB = usedBytes / bytesPerMiB;
    uint64_t totalMiB = totalBytes / bytesPerMiB;
    uint64_t freeMiB = totalBytes > usedBytes ? (totalBytes - usedBytes) / bytesPerMiB : 0;
    uint8_t usagePercent = totalBytes > 0
        ? static_cast<uint8_t>((usedBytes * 100ULL) / totalBytes)
        : 0;

    String lines[5] = {
        String("Estado: ") + (sdReady ? "DISPONIVEL" : "INDISPONIVEL"),
        String("Tipo: ") + (sdReady ? cardType : "--"),
        String("Total: ") + String(static_cast<unsigned long>(totalMiB)) + " MiB",
        String("Usado: ") + String(static_cast<unsigned long>(usedMiB)) + " MiB (" +
            String(usagePercent) + "%)",
        String("Livre: ") + String(static_cast<unsigned long>(freeMiB)) + " MiB"
    };

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++) {
        int y = 44 + (i * 24);
        gfx->fillRect(16, y, gfx->width() - 32, 20, backgroundColor);
        gfx->setTextColor(i == 0 && sdReady ? COLOR_GREEN :
                          i == 0 ? 0xF800 : COLOR_WHITE);
        gfx->setCursor(20, y + 2);
        String l = lines[i];
        if (l.length() > 24) l = l.substring(0, 24);
        gfx->print(l);
    }
}

void DisplayUtil::desenharMatrixScreensaver() {
    SharedSpi::Guard spiGuard;
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

void DisplayUtil::print(String text) {
    SharedSpi::Guard spiGuard; println(text); }

void DisplayUtil::println(String text) {
    SharedSpi::Guard spiGuard;
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
    SharedSpi::Guard spiGuard;
    lines.push_back(line);
    int lineHeight = 8 * 2; 
    int maxLines = (gfx->height() - 45) / lineHeight; 
    while ((int)lines.size() > maxLines) lines.erase(lines.begin());
}

void DisplayUtil::redraw() {
    SharedSpi::Guard spiGuard;
    statusPageInitialized=false; ultimaData=""; ultimaHora="";
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

Arduino_ST7789* DisplayUtil::getDisplay() {
    SharedSpi::Guard spiGuard; return gfx; }

void DisplayUtil::setBacklight(uint8_t percent){
 brightness_=min(uint8_t(100),percent);powerSleeping_=false;lastActivity_=millis();
 if(backlightReady_)ledcWrite(TFT_BL,uint32_t(brightness_)*255/100);else digitalWrite(TFT_BL,brightness_?HIGH:LOW);
}
void DisplayUtil::wake(){lastActivity_=millis();powerSleeping_=false;
 if(backlightReady_)ledcWrite(TFT_BL,uint32_t(brightness_)*255/100);else digitalWrite(TFT_BL,brightness_?HIGH:LOW);
}
void DisplayUtil::updatePower(){
 uint32_t idle=millis()-lastActivity_;
 uint8_t level=idle>=900000?0:idle>=120000?min(uint8_t(20),brightness_):brightness_;
 if(backlightReady_)ledcWrite(TFT_BL,uint32_t(level)*255/100);else digitalWrite(TFT_BL,level?HIGH:LOW);
 powerSleeping_=idle>=900000;
}
void DisplayUtil::showMonitorPage(){
 SharedSpi::Guard guard;if(!monitor_)return;const auto& m=*monitor_;if(!statusPageInitialized){gfx->fillScreen(0);statusPageInitialized=true;}gfx->fillRect(8,32,304,140,0);gfx->setTextSize(2);gfx->setTextColor(0x07FF);
 gfx->setCursor(8,6);gfx->print("MONITOR DE REDE");gfx->setTextColor(0xFFFF);
 gfx->setCursor(8,34);gfx->print(!m.connected?String("WiFi desconectado"):!m.hasResult?String("Ping aguardando"):m.replied?"Gateway: "+String(m.rtt)+" ms":String("Gateway: timeout"));
 gfx->setCursor(8,58);gfx->printf("Quedas:%lu Offline:%lus",(unsigned long)m.drops,(unsigned long)m.offlineSeconds());
 gfx->setCursor(8,82);gfx->printf("Sem resposta: %lu/%lu",(unsigned long)m.failures,(unsigned long)m.attempts);
 int peak=10;for(int value:m.history)if(value>peak)peak=value;
 for(int i=0;i<24;i++){int value=m.history[(m.head+i)%24],x=8+i*12;if(value==-1)gfx->drawFastVLine(x,116,34,0xF800);else if(value>=0){int h=max(1,value*34/peak);gfx->drawFastVLine(x,150-h,h,0x07E0);}}
 gfx->setTextSize(1);gfx->setCursor(8,158);gfx->print("Verde: RTT / vermelho: sem resposta");
}
void DisplayUtil::showForecastPage(){
 SharedSpi::Guard guard;auto c=ClimaManager::snapshot();if(!statusPageInitialized){gfx->fillScreen(0);statusPageInitialized=true;}gfx->fillRect(8,32,304,140,0);gfx->setTextSize(2);gfx->setTextColor(0x07FF);gfx->setCursor(8,6);gfx->print("PREVISAO DO TEMPO");
 if(!c.previsaoValida){gfx->setCursor(8,45);gfx->print("Aguardando dados...");return;}
 gfx->setTextColor(0xFFFF);gfx->setCursor(8,32);gfx->print(c.data);
 gfx->setCursor(8,58);gfx->printf("Min: %.1f C  Max: %.1f C",c.minima,c.maxima);
 gfx->setTextColor(0x07E0);gfx->setCursor(8,86);gfx->printf("Chance de chuva: %d%%",c.chuva);
 gfx->setTextColor(0xFFFF);gfx->setCursor(8,112);gfx->printf("Dados de ha %lu min",(unsigned long)((millis()-c.previsaoAtualizada)/60000));
 gfx->setTextSize(1);gfx->setCursor(8,146);gfx->print(c.ultimaFalhou?"Falha na atualizacao: dados anteriores":"Probabilidade maxima de precipitacao do dia");
}
