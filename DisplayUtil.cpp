#include "SharedSpi.h"
#include "DisplayUtil.h"
#include "ClimaManager.h"
#include <WiFi.h>
#include "RobotScreensaver.h"
#include "Diagnostics.h"
#include "LocalSensor.h"
#include "DigitalMic.h"
#include "DeviceConfig.h"
#include <new>

#define TFT_BL 22

#define COLOR_CYAN 0x07FF
#define COLOR_DARK 0x0000
#define COLOR_GRAY 0x5AEB
#define COLOR_WHITE 0xFFFF
#define COLOR_GREEN 0x07E0
#define COLOR_MAGENTA 0xF81F

DisplayUtil::DisplayUtil()
{
    textColor = COLOR_WHITE;
    backgroundColor = COLOR_DARK;
    textSize = 2;
    ultimaHora = "";
    ultimaData = "";
    ponteiroHistorico = 0;
    sdGravandoAnimacao = false;
    fimAnimacaoSD = 0;
    statusPageInitialized = false;
    for (int i = 0; i < 50; i++)
        historicoRSSI[i] = -100;
}

void DisplayUtil::begin()
{
    SharedSpi::Guard spiGuard;
    pinMode(TFT_BL, OUTPUT);
    backlightReady_ = ledcAttach(TFT_BL, 5000, 8);
    if (!backlightReady_)
        Serial.println("Backlight PWM unavailable; using on/off");
    setBacklight(brightness_);
    wake();

    // DC, CS, SCLK, MOSI, MISO: mesma pinagem do SD no SPI compartilhado.
    Arduino_DataBus* bus = new Arduino_HWSPI(15, 14, 7, 6, 5, &SPI, true);
    gfx = new Arduino_ST7789(bus, 21, 0, true, 172, 320, 34, 0, 34, 0);
    gfx->begin();
    gfx->setRotation(1);
    clear();
}

void DisplayUtil::clear()
{
    SharedSpi::Guard spiGuard;
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    statusPageInitialized = false;
    robotStartedAt_ = millis();
    robotPreviousX_ = robotPreviousY_ = -1;
    for (uint8_t i = 0; i < SoundMonitor::BAND_COUNT; ++i)
    {
        soundLevels_[i] = soundPeaks_[i] = 0;
        soundPeakUntil_[i] = 0;
    }
    const bool wasRobotActive = robotCanvasAttempted_;
    // O buffer so ocupa RAM enquanto a protecao estiver em uso.
    delete robotCanvas_;
    robotCanvas_ = nullptr;
    robotCanvasAttempted_ = false;
    if (wasRobotActive)
        Diagnostics::event("ROBOT_END", "buffer_released=1");
    gfx->fillScreen(backgroundColor);
}

void DisplayUtil::setTextColor(uint16_t color)
{
    SharedSpi::Guard spiGuard;
    textColor = color;
}

void DisplayUtil::setBackgroundColor(uint16_t color)
{
    SharedSpi::Guard spiGuard;
    backgroundColor = color;
}

void DisplayUtil::setTextSize(uint8_t size)
{
    SharedSpi::Guard spiGuard;
    textSize = size;
}

void DisplayUtil::setRotation(uint8_t rotation)
{
    SharedSpi::Guard spiGuard;
    lines.clear();
    ultimaHora = "";
    ultimaData = "";
    gfx->setRotation(rotation);
    clear();
}

void DisplayUtil::dispararAnimacaoGravacaoSD()
{
    SharedSpi::Guard spiGuard;
    sdGravandoAnimacao = true;
    fimAnimacaoSD = millis() + 400;
}

void DisplayUtil::drawPageHeader(const char* title, const char* section)
{
    if (statusPageInitialized)
        return;
    gfx->fillScreen(backgroundColor);
    gfx->fillRoundRect(6, 5, 308, 30, 6, 0x10A2);
    gfx->fillRoundRect(6, 5, 4, 30, 2, COLOR_CYAN);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(17, 13);
    gfx->print(title);
    gfx->drawFastHLine(12, 168, 296, 0x18E3);
    // O subtitulo curto identifica a origem sem ocupar a area dos dados.
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_CYAN);
    gfx->setCursor(246, 17);
    gfx->print(section);
    statusPageInitialized = true;
}

void DisplayUtil::showClock(String dateTime)
{
    SharedSpi::Guard guard;
    gfx->setRotation(1);
    if (!statusPageInitialized)
    {
        drawPageHeader("PAINEL LOCAL", "INICIO");
        gfx->drawRoundRect(10, 98, 148, 44, 5, 0x18E3);
        gfx->drawRoundRect(166, 98, 144, 44, 5, 0x18E3);
    }
    const bool timeOk = dateTime.length() >= 19 && dateTime[2] == '/' && dateTime[10] == ' ';
    drawPanelLine(16, 42, 4, 12, timeOk ? dateTime.substring(11, 19) : "--:--:--", COLOR_WHITE);
    drawPanelLine(16, 80, 1, 48,
                  timeOk ? dateTime.substring(0, 10) + "  |  " +
                               (WiFi.status() == WL_CONNECTED ? "WI-FI CONECTADO" : "SEM WI-FI")
                         : "Horario aguardando NTP | sensor independente",
                  COLOR_GRAY);
    const auto local = localSensor.snapshot();
    const bool ready = local.fresh() && !local.lastFailed;
    drawPanelLine(16, 103, 1, 22, "TEMPERATURA LOCAL", COLOR_CYAN);
    drawPanelLine(174, 103, 1, 21, "UMIDADE LOCAL", COLOR_CYAN);
    char t[20], h[20];
    if (ready)
    {
        snprintf(t, sizeof(t), "%.1f C", local.temperature);
        snprintf(h, sizeof(h), "%.1f %%", local.humidity);
    }
    else
    {
        snprintf(t, sizeof(t), "-- C");
        snprintf(h, sizeof(h), "-- %%");
    }
    drawPanelLine(16, 120, 2, 11, t, COLOR_WHITE);
    drawPanelLine(174, 120, 2, 11, h, COLOR_WHITE);
    drawPanelLine(16, 150, 1, 48,
                  ready              ? "DHT22 ativo | BOOT: paginas | local: tela:11"
                  : !local.running   ? "DHT22: tarefa indisponivel | veja diagnostico"
                  : local.lastFailed ? "DHT22 sem resposta | confira GPIO3 e 3V3"
                  : local.valid      ? "DHT22: leitura antiga | aguardando atualizacao"
                                     : "DHT22: aguardando primeira leitura...",
                  ready ? COLOR_GREEN : 0xFFE0);
}

void DisplayUtil::showStatusPage(bool sdReady, bool ntpSynchronized, const String& ntpDateTime,
                                 int32_t timezoneOffset, bool daylightSaving, uint8_t part)
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    drawPageHeader(part == 0 ? "NTP 1/2" : "NTP 2/2", "HORA");

    String lines[5];
    uint16_t colors[5];
    bool wifiConectado = WiFi.status() == WL_CONNECTED;

    if (part == 0)
    {
        lines[0] = ntpSynchronized ? "SINCRONIZADO" : "AGUARDANDO";
        colors[0] = ntpSynchronized ? COLOR_GREEN : 0xF800;
        lines[1] = ntpSynchronized ? ntpDateTime.substring(0, 10) : String("--");
        lines[2] = ntpSynchronized ? ntpDateTime.substring(11, 19) : String("--");
        lines[3] = String("Fuso: UTC") + (timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset);
        lines[4] = String("DST: ") + (daylightSaving ? "ON" : "OFF");
        colors[1] = colors[2] = colors[3] = colors[4] = COLOR_WHITE;
    }
    else
    {
        unsigned long up = millis() / 1000UL;
        char upBuf[32];
        snprintf(upBuf, sizeof(upBuf), "UP: %lud %02lu:%02lu", up / 86400UL, (up / 3600UL) % 24UL,
                 (up / 60UL) % 60UL);
        lines[0] = wifiConectado ? "Wi-Fi: conectado" : "Wi-Fi: desconectado";
        colors[0] = wifiConectado ? COLOR_GREEN : 0xF800;
        lines[1] = String("IP: ") + (wifiConectado ? WiFi.localIP().toString() : String("--"));
        lines[2] = String("RSSI: ") + (wifiConectado ? String(WiFi.RSSI()) + " dBm" : String("--"));
        lines[3] = upBuf;
        lines[4] = !DeviceConfig::SDCARD_ENABLED ? "SD: DESATIVADO" : sdReady ? "SD: PRONTO" : "SD: INDISPONIVEL";
        colors[1] = colors[2] = colors[3] = COLOR_WHITE;
        colors[4] = !DeviceConfig::SDCARD_ENABLED ? COLOR_GRAY : sdReady ? COLOR_GREEN : 0xF800;
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 44 + (i * 24);
        drawPanelLine(20, y + 2, 2, 24, lines[i], colors[i]);
    }
}

void DisplayUtil::showNetworkPage(uint8_t part)
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    drawPageHeader(part == 0 ? "REDE 1/2" : "REDE 2/2", "WI-FI");

    bool connected = WiFi.status() == WL_CONNECTED;
    String lines[5];
    if (part == 0)
    {
        lines[0] = String("Estado: ") + (connected ? "ON" : "OFF");
        lines[1] = String("SSID: ") + (connected ? WiFi.SSID() : "--");
        lines[2] = String("IP: ") + (connected ? WiFi.localIP().toString() : "--");
        lines[3] = String("GW: ") + (connected ? WiFi.gatewayIP().toString() : "--");
        lines[4] = String("Mask: ") + (connected ? WiFi.subnetMask().toString() : "--");
    }
    else
    {
        lines[0] = String("DNS1: ") + (connected ? WiFi.dnsIP(0).toString() : "--");
        lines[1] = String("DNS2: ") + (connected ? WiFi.dnsIP(1).toString() : "--");
        lines[2] = String("MAC: ") + WiFi.macAddress();
        lines[3] = String("Canal: ") + (connected ? String(WiFi.channel()) : "--");
        lines[4] = String("RSSI: ") + (connected ? String(WiFi.RSSI()) + " dBm" : "--");
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 44 + (i * 22);
        drawPanelLine(20, y + 1, 2, 24, lines[i],
                      part == 0 && i == 0 ? (connected ? COLOR_GREEN : 0xF800) : COLOR_WHITE);
    }
}

void DisplayUtil::showSystemPage()
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    drawPageHeader("SISTEMA", "ESP32-C6");

    uint32_t heapTotal = ESP.getHeapSize();
    uint32_t heapFree = ESP.getFreeHeap();
    uint8_t heapPercent = heapTotal > 0 ? (heapFree * 100ULL) / heapTotal : 0;
    float temp = temperatureRead();
    String lines[5] = {
        String("CPU: ") + String(ESP.getCpuFreqMHz()) + " MHz",
        String("Temp: ") + String(temp, 1) + " C",
        String("RAM: ") + String(heapFree / 1024) + " KB (" + String(heapPercent) + "%)",
        String("RAM min: ") + String(ESP.getMinFreeHeap() / 1024) + " KB",
        String("Flash: ") + String(ESP.getSketchSize() * 100ULL / ESP.getFlashChipSize()) +
            "% usada"};

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 44 + (i * 22);
        drawPanelLine(20, y + 1, 2, 24, lines[i], i == 1 && temp > 70.0f ? 0xF800 : COLOR_WHITE);
    }
}

void DisplayUtil::showSavedWifiPage(const String ssids[5], int connectedSlot, uint8_t nextSlot)
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    drawPageHeader("REDES SALVAS", "PERFIS");

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 44 + (i * 21);
        bool vazio = ssids[i].length() == 0;
        bool conectada = static_cast<int>(i) == connectedSlot;
        String linha = String(i + 1) + (conectada ? "*" : (i == nextSlot ? ">" : " ")) +
                       (vazio ? String("(vazio)") : ssids[i]);
        drawPanelLine(20, y + 2, 2, 24, linha,
                      conectada ? COLOR_GREEN
                      : vazio   ? COLOR_GRAY
                                : COLOR_WHITE);
    }
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_GRAY);
    gfx->fillRect(16, 152, gfx->width() - 32, 10, backgroundColor);
    gfx->setCursor(20, 153);
    gfx->print("* conectada   > proxima a substituir");
}

void DisplayUtil::showHoldMessage(const String& line1, const String& line2, uint16_t color)
{
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

void DisplayUtil::showSdLoading(uint8_t frame)
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    if (!statusPageInitialized)
    {
        drawPageHeader("CARTAO SD", "LEITURA");
        gfx->setTextSize(2);
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
    for (uint8_t i = 0; i < 4; ++i)
    {
        gfx->fillRect(cx - 14 + i * 8, cy - 23, 5, 11, 0xFEA0);
        gfx->drawFastVLine(cx - 12 + i * 8, cy - 9, 26, 0x2945);
    }
    const int scanY = cy - 8 + (frame * 2);
    gfx->drawFastHLine(cx - 17, scanY, 34, COLOR_CYAN);
    gfx->drawFastHLine(cx - 17, scanY + 1, 34, COLOR_WHITE);

    // Pacotes orbitando o cartao, com duas caudas em sentidos opostos.
    for (uint8_t i = 0; i < pontos; i++)
    {
        float ang = (i * 2.0f * PI) / pontos;
        int x = cx + static_cast<int>(cosf(ang) * 78);
        int y = cy + static_cast<int>(sinf(ang) * 29);
        uint8_t dist = (frame + pontos - i) % pontos;
        uint8_t opposite = (i + frame) % pontos;
        uint16_t cor = dist == 0      ? COLOR_WHITE
                       : dist < 3     ? COLOR_CYAN
                       : opposite < 2 ? COLOR_MAGENTA
                       : dist < 6     ? 0x0410
                                      : 0x2945;
        gfx->fillCircle(x, y, dist == 0 ? 4 : 2, cor);
    }
    // Indicador de atividade; nao representa uma porcentagem de progresso.
    gfx->drawRoundRect(cx - 96, 118, 192, 9, 3, 0x2945);
    gfx->fillRoundRect(cx - 92 + frame * 9, 120, 45, 5, 2, COLOR_CYAN);
}

void DisplayUtil::showSdPage(bool sdReady, const String& cardType, uint64_t totalBytes,
                             uint64_t usedBytes, bool usedValid)
{
    SharedSpi::Guard spiGuard;
    gfx->setRotation(1);
    drawPageHeader("CARTAO SD", "ARQUIVOS");
    if (!DeviceConfig::SDCARD_ENABLED) {
        drawPanelLine(20, 48, 2, 24, "SD DESATIVADO", COLOR_CYAN);
        drawPanelLine(20, 80, 1, 46, "Sensores, tela, serial e Prometheus ativos.", COLOR_WHITE);
        drawPanelLine(20, 104, 1, 46, "Logs em cartao e consultas SD suspensos.", COLOR_GRAY);
        return;
    }

    const uint64_t bytesPerMiB = 1024ULL * 1024ULL;
    uint64_t usedMiB = usedBytes / bytesPerMiB;
    uint64_t totalMiB = totalBytes / bytesPerMiB;
    uint64_t freeMiB = totalBytes > usedBytes ? (totalBytes - usedBytes) / bytesPerMiB : 0;
    uint8_t usagePercent =
        totalBytes > 0 ? static_cast<uint8_t>((usedBytes * 100ULL) / totalBytes) : 0;

    String lines[5] = {String("Estado: ") + (sdReady ? "DISPONIVEL" : "INDISPONIVEL"),
                       String("Tipo: ") + (sdReady ? cardType : "--"),
                       String("Total: ") + String(static_cast<unsigned long>(totalMiB)) + " MiB",
                       String("Usado: ") + String(static_cast<unsigned long>(usedMiB)) + " MiB (" +
                           String(usagePercent) + "%)",
                       String("Livre: ") + String(static_cast<unsigned long>(freeMiB)) + " MiB"};
    if (!usedValid || !sdReady)
    {
        lines[3] = "Usado: nao calculado";
        lines[4] = "Livre: nao calculado";
    }
    if (!totalBytes) {
        lines[2] = "Total: indisponivel";
        lines[3] = "Usado: nao calculado";
        lines[4] = "Livre: nao calculado";
    }

    gfx->setTextSize(2);
    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 44 + (i * 24);
        drawPanelLine(20, y + 2, 2, 24, lines[i],
                      i == 0 && sdReady ? COLOR_GREEN
                      : i == 0          ? 0xF800
                                        : COLOR_WHITE);
    }
}

void DisplayUtil::desenharRobotScreensaver()
{
    SharedSpi::Guard spiGuard;
    const uint32_t elapsed = millis() - robotStartedAt_;
    // Trajetoria independente da taxa de quadros, com reflexao nas bordas.
    auto bounce = [elapsed](int span, uint32_t speed) -> int
    {
        if (span <= 0)
            return 0;
        const uint32_t phase = (uint64_t(elapsed) * speed / 1000 + span / 2) % (2 * span);
        return phase <= uint32_t(span) ? int(phase) : 2 * span - int(phase);
    };
    const int x = bounce(gfx->width() - 176, 25);
    const int y = bounce(gfx->height() - 136, 13);
    if (robotPreviousX_ >= 0)
    {
        // Apaga apenas as faixas deixadas para tras, sem piscar a tela inteira.
        auto erase = [&](int left, int top, int width, int height)
        {
            if (width > 0 && height > 0)
                gfx->fillRect(left, top, width, height, backgroundColor);
        };
        const int oldX = robotPreviousX_, oldY = robotPreviousY_;
        erase(oldX, oldY, 176, min(y, oldY + 136) - oldY);
        const int bottom = max(y + 136, oldY);
        erase(oldX, bottom, 176, oldY + 136 - bottom);
        const int top = max(y, oldY), height = min(y + 136, oldY + 136) - top;
        erase(oldX, top, min(x, oldX + 176) - oldX, height);
        const int right = max(x + 176, oldX);
        erase(right, top, oldX + 176 - right, height);
    }
    if (!robotCanvasAttempted_)
    {
        robotCanvasAttempted_ = true;
        Diagnostics::event("ROBOT_START", "buffer_requested_bytes=47872");
        robotCanvas_ = new (std::nothrow) Arduino_Canvas(176, 136, gfx, x, y);
        // A tela e o SPI ja estao inicializados; nao reinicializar o hardware.
        if (robotCanvas_ && !robotCanvas_->begin(GFX_SKIP_OUTPUT_BEGIN))
        {
            delete robotCanvas_;
            robotCanvas_ = nullptr;
        }
        if (!robotCanvas_)
            Serial.println("[Tela] Robo sem buffer: usando desenho direto.");
        Diagnostics::event(robotCanvas_ ? "ROBOT_BUFFER_OK" : "ROBOT_BUFFER_FAILED");
    }
    if (robotCanvas_)
    {
        robotCanvas_->fillScreen(backgroundColor);
        RobotScreensaver::draw(*robotCanvas_, elapsed);
        gfx->draw16bitRGBBitmap(x, y, robotCanvas_->getFramebuffer(), 176, 136);
    }
    else
    {
        gfx->fillRect(x, y, 176, 136, backgroundColor);
        RobotScreensaver::draw(*gfx, elapsed, x, y);
    }
    robotPreviousX_ = x;
    robotPreviousY_ = y;
}

void DisplayUtil::print(String text)
{
    SharedSpi::Guard spiGuard;
    println(text);
}

void DisplayUtil::println(String text)
{
    SharedSpi::Guard spiGuard;
    text.replace("\r", "");
    text.replace("\n", "");
    int charWidth = 6 * 2;
    int maxChars = (gfx->width() - 24) / charWidth;
    while (text.length() > maxChars)
    {
        addLine(text.substring(0, maxChars));
        text = text.substring(maxChars);
    }
    if (text.length() > 0)
        addLine(text);
    redraw();
}

void DisplayUtil::addLine(String line)
{
    SharedSpi::Guard spiGuard;
    lines.push_back(line);
    int lineHeight = 8 * 2;
    int maxLines = (gfx->height() - 45) / lineHeight;
    while ((int)lines.size() > maxLines)
        lines.erase(lines.begin());
}

void DisplayUtil::redraw()
{
    SharedSpi::Guard spiGuard;
    statusPageInitialized = false;
    ultimaData = "";
    ultimaHora = "";
    gfx->fillScreen(backgroundColor);
    gfx->fillRect(0, 0, gfx->width(), 26, 0x2104);
    gfx->drawFastHLine(0, 26, gfx->width(), COLOR_CYAN);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(12, 9);
    gfx->print("> CONSOLE LOG INTERFACES_");
    int y = 35;
    int lineHeight = 8 * 2;
    gfx->setTextSize(2);
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].startsWith(">"))
        {
            gfx->setTextColor(COLOR_GREEN);
            gfx->setCursor(8, y);
        }
        else
        {
            gfx->setTextColor(0xCE79);
            gfx->setCursor(12, y);
        }
        gfx->println(lines[i]);
        y += lineHeight;
    }
}

Arduino_ST7789* DisplayUtil::getDisplay()
{
    SharedSpi::Guard spiGuard;
    return gfx;
}

void DisplayUtil::setBacklight(uint8_t percent)
{
    brightness_ = min(uint8_t(100), percent);
    powerSleeping_ = false;
    lastActivity_ = millis();
    applyBacklight(brightness_);
}

void DisplayUtil::wake()
{
    lastActivity_ = millis();
    powerSleeping_ = false;
    applyBacklight(brightness_);
}

void DisplayUtil::updatePower()
{
    uint32_t idle = millis() - lastActivity_;
    uint8_t level = idle >= 120000 ? min(uint8_t(20), brightness_) : brightness_;
    applyBacklight(level);
    powerSleeping_ = false;
}

void DisplayUtil::applyBacklight(uint8_t level)
{
    // Reaplicar o mesmo duty continuamente perturba desnecessariamente o PWM.
    if (appliedBrightness_ == level)
        return;
    appliedBrightness_ = level;
    bool ok = true;
    if (backlightReady_)
        ok = ledcWrite(TFT_BL, uint32_t(level) * 255 / 100);
    else
        digitalWrite(TFT_BL, level ? HIGH : LOW);
    char detail[80];
    snprintf(detail, sizeof(detail), "level=%u pwm=%u write_ok=%u", unsigned(level),
             unsigned(backlightReady_), unsigned(ok));
    Diagnostics::event("BACKLIGHT", detail);
}

void DisplayUtil::drawPanelLine(int x, int y, uint8_t size, uint8_t columns, const String& text,
                                uint16_t color)
{
    // Texto opaco substitui os caracteres antigos sem apagar a linha inteira.
    String line = text.substring(0, columns);
    while (line.length() < columns)
        line += ' ';
    gfx->setTextSize(size);
    gfx->setTextColor(color, backgroundColor);
    gfx->setCursor(x, y);
    gfx->print(line);
}

void DisplayUtil::showSoundPage(const SoundMonitor& sound)
{
    SharedSpi::Guard guard;
    const auto state = sound.snapshot();
    const uint32_t now = millis();
    uint16_t bars[SoundMonitor::HISTORY_SIZE]{};
    uint16_t peak = 0;
    for (uint8_t i = 0; i < state.count; ++i)
    {
        const uint32_t age = uint32_t(now - state.times[i]);
        if (age >= SoundMonitor::HISTORY_MS)
            continue;
        const uint8_t column = SoundMonitor::HISTORY_SIZE - 1 - age / SoundMonitor::WINDOW_MS;
        if (state.values[i] > bars[column])
            bars[column] = state.values[i];
        if (state.values[i] > peak)
            peak = state.values[i];
    }
    if (!statusPageInitialized)
    {
        drawPageHeader(state.equalizer ? "EQUALIZADOR" : "VOLUME NO TEMPO", "AUDIO");
        gfx->drawRoundRect(10, 68, 300, 76, 4, COLOR_GRAY);
        statusPageInitialized = true;
    }
    if (!state.running)
    {
        drawPanelLine(8, 38, 2, 25, state.pin < 0 ? "Sensor desativado" : "Falha ao iniciar sensor",
                      0xFFE0);
        drawPanelLine(8, 57, 1, 50, "Verifique SoundMonitor.h e o diagnostico.", COLOR_GRAY);
    }
    else
    {
        char line[64];
        if (state.updatedAt && uint32_t(now - state.updatedAt) < 1000)
        {
            if (state.equalizer)
                snprintf(line, sizeof(line), "Ganho:%2ux  Nivel:%3u%%", unsigned(state.gain),
                         unsigned(min(100UL, state.latest * uint32_t(state.gain) * 100UL / 4095)));
            else
                snprintf(line, sizeof(line), "G:%2ux N:%3u%% P:%3u%%", unsigned(state.gain),
                         unsigned(min(100UL, state.latest * uint32_t(state.gain) * 100UL / 4095)),
                         unsigned(min(100UL, peak * uint32_t(state.gain) * 100UL / 4095)));
            drawPanelLine(8, 38, 2, 25, line, COLOR_WHITE);
        }
        else
            drawPanelLine(8, 38, 2, 25, "Aguardando amostras...", 0xFFE0);
        snprintf(line, sizeof(line), "GPIO%d | %s | %s", state.pin,
                 state.equalizer ? "12 faixas / 80 Hz a 10 kHz" : "historico / 9,6 s",
                 state.clipped ? "ADC SATURADO" : "sem dB");
        drawPanelLine(8, 57, 1, 50, line, COLOR_GRAY);
    }
    if (state.equalizer)
    {
        const bool fresh =
            state.running && state.updatedAt && uint32_t(now - state.updatedAt) < 1000;
        for (uint8_t band = 0; band < SoundMonitor::BAND_COUNT; ++band)
        {
            float level = fresh ? state.bands[band] * state.gain / 2047.5f : 0;
            if (level > 1)
                level = 1;
            // Compressao visual para destacar sinais fracos; nao representa dB.
            const uint8_t target = uint8_t(sqrtf(level) * 60);
            uint8_t& height = soundLevels_[band];
            if (target >= height)
                height = target;
            else
                height = height > target + 3 ? height - 3 : target;
            if (height >= soundPeaks_[band])
            {
                soundPeaks_[band] = height;
                soundPeakUntil_[band] = now + 700;
            }
            else if (int32_t(now - soundPeakUntil_[band]) >= 0)
            {
                soundPeaks_[band] = soundPeaks_[band] > 2 ? soundPeaks_[band] - 2 : 0;
            }
            const int x = 18 + band * 24;
            gfx->fillRect(x, 74, 20, 5, backgroundColor);
            for (uint8_t segment = 0; segment < 12; ++segment)
            {
                uint16_t color = segment >= 9 ? COLOR_MAGENTA : segment >= 6 ? 0xFFE0 : COLOR_GREEN;
                if (height < segment * 5 + 1)
                    color = backgroundColor;
                gfx->fillRect(x, 135 - segment * 5, 20, 4, color);
                gfx->drawFastHLine(x, 134 - segment * 5, 20, backgroundColor);
            }
            if (soundPeaks_[band])
                gfx->drawFastHLine(x, 137 - soundPeaks_[band], 20, COLOR_WHITE);
        }
        drawPanelLine(10, 151, 1, 50, "GRAVES            MEDIOS              AGUDOS", COLOR_GRAY);
        return;
    }
    // Historico preservado como modo alternativo, com o mesmo ganho visual.
    for (uint8_t i = 0; i < SoundMonitor::HISTORY_SIZE; ++i)
    {
        const int x = 16 + i * 3;
        const uint16_t value = bars[i];
        const int height = int(min(4095UL, value * uint32_t(state.gain)) * 64UL / 4095);
        const uint16_t color = value > 3071 ? 0xF800 : value > 1638 ? 0xFFE0 : COLOR_CYAN;
        if (height)
            gfx->fillRect(x, 138 - height, 2, height, color);
        if (height < 64)
            gfx->fillRect(x, 74, 2, 64 - height, backgroundColor);
    }
    drawPanelLine(10, 151, 1, 50, "-9,6 s                -4,8 s             agora", COLOR_GRAY);
}

void DisplayUtil::showMonitorPage()
{
    SharedSpi::Guard guard;
    if (!monitor_)
        return;
    const auto& m = *monitor_;
    drawPageHeader("MONITOR DE REDE", "PING");
    drawPanelLine(8, 38, 2, 25,
                  !m.connected   ? String("WiFi desconectado")
                  : !m.hasResult ? String("Ping aguardando")
                  : m.replied    ? "Gateway: " + String(m.rtt) + " ms"
                                 : String("Gateway: timeout"),
                  COLOR_WHITE);
    char line[80];
    snprintf(line, sizeof(line), "Quedas:%lu Offline:%lus", (unsigned long)m.drops,
             (unsigned long)m.offlineSeconds());
    drawPanelLine(8, 58, 2, 25, line, COLOR_WHITE);
    snprintf(line, sizeof(line), "Sem resposta: %lu/%lu", (unsigned long)m.failures,
             (unsigned long)m.attempts);
    drawPanelLine(8, 82, 2, 25, line, COLOR_WHITE);
    int peak = 10;
    for (int value : m.history)
        if (value > peak)
            peak = value;
    for (int i = 0; i < 24; i++)
    {
        int value = m.history[(m.head + i) % 24], x = 8 + i * 12;
        if (value == -1)
            gfx->drawFastVLine(x, 116, 34, 0xF800);
        else if (value >= 0)
        {
            int h = max(1, value * 34 / peak);
            gfx->drawFastVLine(x, 150 - h, h, 0x07E0);
            gfx->drawFastVLine(x, 116, 34 - h, backgroundColor);
        }
        else
            gfx->drawFastVLine(x, 116, 34, backgroundColor);
    }
    drawPanelLine(8, 158, 1, 50, "Verde: RTT / vermelho: sem resposta", COLOR_GRAY);
}

void DisplayUtil::showForecastPage()
{
    SharedSpi::Guard guard;
    drawPageHeader("PORTO ALEGRE", "INTERNET");
    const auto c = ClimaManager::snapshot();
    char line[64];
    drawPanelLine(16, 43, 1, 47, "OPEN-METEO | CLIMA EXTERNO", COLOR_CYAN);
    if (c.sincronizado)
        snprintf(line, sizeof(line), "%.1fC  %.0f%%", c.temperatura, c.umidade);
    else
        snprintf(line, sizeof(line), "-- C       -- %%");
    drawPanelLine(16, 62, 3, 16, line, COLOR_WHITE);
    drawPanelLine(16, 92, 1, 47,
                  c.sincronizado ? ClimaManager::obterTextoCondicao() : "Aguardando internet...",
                  COLOR_GRAY);
    if (c.previsaoValida)
        snprintf(line, sizeof(line), "Min %.1f C  Max %.1f C  Chuva %d%%", c.minima, c.maxima,
                 c.chuva);
    else
        snprintf(line, sizeof(line), "Previsao diaria ainda indisponivel");
    drawPanelLine(16, 115, 1, 47, line, COLOR_WHITE);
    if (c.sincronizado)
        snprintf(line, sizeof(line), "Atualizado ha %lld min%s",
                 (esp_timer_get_time() - c.ultimaAtualizacaoUs) / 60000000LL,
                 c.ultimaFalhou ? " | falha ao renovar" : "");
    else
        snprintf(line, sizeof(line), "Dados da internet, nao do sensor local");
    drawPanelLine(16, 147, 1, 47, line, c.ultimaFalhou ? 0xFFE0 : COLOR_GRAY);
}

void DisplayUtil::showDigitalMicPage(const DigitalMic& mic)
{
    SharedSpi::Guard guard;
    drawPageHeader("INMP441", "I2S");
    const auto state = mic.snapshot();
    const bool fresh = state.fresh();
    const bool signal = fresh && state.hasSignal;
    const char* status = !state.running ? "Falha ao iniciar I2S" :
        !fresh ? "Aguardando amostras..." : "Canal sem variacao";
    drawPanelLine(16, 48, 2, 24, signal ? String(state.rmsDbfs, 1) + " dBFS RMS" : String(status),
                  signal ? COLOR_WHITE : 0xFFE0);

    constexpr int x = 16, width = 288;
    if (state.mode != DigitalMic::Mode::Meter) {
        constexpr int top = 77, bottom = 137, height = bottom - top;
        gfx->fillRect(x, top, width, height + 1, 0x18E3);
        const auto plotY = [bottom, height](float dbfs) {
            return bottom - int(constrain((dbfs + 90.0f) / 90.0f, 0.0f, 1.0f) * height);
        };
        if (state.mode == DigitalMic::Mode::History) {
            DigitalMic::HistoryPoint points[DigitalMic::HISTORY_SIZE];
            const size_t count = mic.history(points);
            const uint32_t now = millis();
            constexpr uint32_t span = DigitalMic::HISTORY_SIZE * DigitalMic::HISTORY_STEP_MS;
            gfx->drawFastHLine(x, plotY(state.thresholdDbfs), width, 0xFFE0);
            bool previous = false;
            int lastX = 0, lastY = 0;
            uint32_t lastTime = 0;
            for (size_t i = 0; i < count; ++i) {
                const uint32_t age = now - points[i].timestamp;
                if (age > span) continue;
                const int px = x + int((span - age) * (width - 1) / span);
                const int py = plotY(points[i].rmsDbfs);
                if (previous && uint32_t(points[i].timestamp - lastTime) <= 400)
                    gfx->drawLine(lastX, lastY, px, py, COLOR_CYAN);
                else gfx->drawPixel(px, py, COLOR_CYAN);
                previous = true;
                lastX = px;
                lastY = py;
                lastTime = points[i].timestamp;
            }
            drawPanelLine(x, 148, 1, 48, "-30 s                    0 s | RMS dBFS", COLOR_GRAY);
        } else {
            constexpr int column = width / DigitalMic::BAND_COUNT;
            if (signal) {
                for (size_t i = 0; i < DigitalMic::BAND_COUNT; ++i) {
                    const int y = plotY(state.bands[i]);
                    if (y < bottom) gfx->fillRect(x + int(i) * column + 2, y, column - 4, bottom - y, COLOR_CYAN);
                }
            }
            drawPanelLine(x, 148, 1, 48, "63 Hz      Graves / Medios / Agudos      8 kHz", COLOR_GRAY);
        }
        drawPanelLine(x, 158, 1, 48, state.eventActive && fresh ? "EVENTO SONORO" : "", 0xF81F);
        return;
    }
    const int filled = signal ? int(constrain((state.rmsDbfs + 90.0f) / 90.0f, 0.0f, 1.0f) * width) : 0;
    if (filled > 0) gfx->fillRect(x, 86, filled, 12, COLOR_CYAN);
    if (filled < width) gfx->fillRect(x + filled, 86, width - filled, 12, 0x18E3);
    drawPanelLine(x, 108, 1, 48, signal ? String("Pico: ") + String(state.peakDbfs, 1) + " dBFS" : String("Confira VDD, GND e L/R em GND"), COLOR_GRAY);
    char wiring[49];
    snprintf(wiring, sizeof(wiring), "16 kHz | Canal esquerdo | SCK%d WS%d SD%d",
             int(DigitalMic::BCLK), int(DigitalMic::WS), int(DigitalMic::DATA));
    drawPanelLine(x, 132, 1, 48, wiring, COLOR_CYAN);
    const String event = String(fresh && state.eventActive ? "EVENTO | " : "") +
        "Limite " + String(state.thresholdDbfs) + " dBFS | Total " + String(state.events);
    drawPanelLine(x, 148, 1, 48, event, fresh && state.eventActive ? 0xF81F : COLOR_GRAY);
}

void DisplayUtil::showLightPage(const SoundMonitor& sound)
{
    SharedSpi::Guard guard;
    drawPageHeader("LUMINOSIDADE", "LDR");
    const auto light = sound.lightSnapshot();
    const bool fresh = light.fresh();
    constexpr int x = 16, width = 288;
    const bool calibrated = light.calibrated();
    const int dark = calibrated ? light.dark : 0;
    const int bright = calibrated ? light.bright : SoundMonitor::LightSnapshot::ADC_MAX;
    const int span = bright - dark;
    const int level = fresh ? constrain(int(light.raw) - dark, 0, span) : 0;
    const int filled = level * width / span;
    const int percent = (level * 100 + span / 2) / span;
    drawPanelLine(x, 48, 4, 12, fresh ? String(percent) + " %" : String("-- %"), COLOR_WHITE);
    if (filled > 0) gfx->fillRect(x, 94, filled, 10, COLOR_CYAN);
    if (filled < width) gfx->fillRect(x + filled, 94, width - filled, 10, 0x18E3);
    drawPanelLine(x, 115, 1, 48, fresh
        ? String("GPIO1 | ADC ") + String(light.raw) + " / 4095"
        : String("GPIO1 | Sem amostras recentes do ADC"), fresh ? COLOR_CYAN : 0xFFE0);
    const String scale = calibrated ? String("Calibrado | 0%: ") + String(dark) + " | 100%: " + String(bright) :
        String("Escala ADC | ldr_escuro / ldr_claro");
    drawPanelLine(x, 146, 1, 48, scale, COLOR_GRAY);
}

void DisplayUtil::showLocalSensorPage()
{
    SharedSpi::Guard guard;
    if (!statusPageInitialized)
    {
        drawPageHeader("AMBIENTE LOCAL", "DHT22");
        gfx->drawFastVLine(160, 43, 51, 0x18E3);
    }
    const auto s = localSensor.snapshot();
    const bool ready = s.fresh() && !s.lastFailed;
    drawPanelLine(16, 42, 1, 22, "TEMPERATURA", COLOR_CYAN);
    drawPanelLine(178, 42, 1, 20, "UMIDADE", COLOR_GREEN);
    char t[20], h[20], info[64];
    if (ready)
    {
        snprintf(t, sizeof(t), "%.1f", s.temperature);
        snprintf(h, sizeof(h), "%.1f", s.humidity);
    }
    else
    {
        snprintf(t, sizeof(t), "--");
        snprintf(h, sizeof(h), "--");
    }
    drawPanelLine(16, 58, 3, 7, t, COLOR_WHITE);
    drawPanelLine(178, 58, 3, 7, h, COLOR_WHITE);
    drawPanelLine(16, 85, 1, 22, "GRAUS CELSIUS", COLOR_GRAY);
    drawPanelLine(178, 85, 1, 20, "% RELATIVA", COLOR_GRAY);
    // Apenas a pequena area do grafico e limpa; nao ha framebuffer adicional.
    gfx->fillRect(16, 117, 288, 28, backgroundColor);
    gfx->drawFastHLine(16, 144, 288, 0x18E3);
    float low = 100, high = -100;
    for (uint8_t i = 0; i < s.count; ++i)
    {
        const uint8_t n =
            (s.head + LocalSensor::HISTORY_SIZE - s.count + i) % LocalSensor::HISTORY_SIZE;
        if (s.historyValid[n])
        {
            low = min(low, s.history[n]);
            high = max(high, s.history[n]);
        }
    }
    if (high >= low)
    {
        low -= 0.5f;
        high += 0.5f; // Escala automatica; sem limites de conforto inventados.
        snprintf(info, sizeof(info), "TENDENCIA %.1f..%.1f C | ~2 MIN", low, high);
        drawPanelLine(16, 103, 1, 47, info, COLOR_CYAN);
        bool previous = false;
        int px = 0, py = 0;
        for (uint8_t i = 0; i < s.count; ++i)
        {
            const uint8_t n =
                (s.head + LocalSensor::HISTORY_SIZE - s.count + i) % LocalSensor::HISTORY_SIZE;
            if (!s.historyValid[n])
            {
                previous = false;
                continue;
            }
            const int x = 16 + (LocalSensor::HISTORY_SIZE - s.count + i) * 6;
            const int y = 142 - int((s.history[n] - low) * 23 / (high - low));
            if (previous)
                gfx->drawLine(px, py, x, y, COLOR_CYAN);
            gfx->fillCircle(x, y, 1, COLOR_CYAN);
            previous = true;
            px = x;
            py = y;
        }
    }
    else
        drawPanelLine(16, 103, 1, 47, "TEMPERATURA | AGUARDANDO HISTORICO", COLOR_CYAN);
    if (ready)
        snprintf(info, sizeof(info), "GPIO3 | ha %llds | erros: %lu | BOOT: sair",
                 (esp_timer_get_time() - s.updatedUs) / 1000000LL, (unsigned long)s.errors);
    else
        snprintf(info, sizeof(info), "%s",
                 !s.running     ? "Falha ao iniciar tarefa do sensor"
                 : s.lastFailed ? "Sem resposta: confira DATA, 3V3 e GND"
                 : s.valid      ? "Leitura antiga: aguardando atualizacao"
                                : "Estabilizando: aguardando primeira leitura");
    drawPanelLine(16, 153, 1, 48, info, ready ? COLOR_GREEN : 0xFFE0);
}
