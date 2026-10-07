#include "RGBLed.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "SDUtil.h"
#include "CommandProcessor.h"
#include "OTAManager.h"
#include "ClimaManager.h"
#include <WiFi.h>

constexpr uint8_t USER_BUTTON_PIN = 9; // Botao BOOT onboard da Waveshare ESP32-C6-LCD-1.47

DisplayUtil display;
RGBLed rgbLed;
ESP32Gateway gateway;
NTPUtil ntp;
SDUtil sd;
CommandProcessor commandProcessor(display, gateway, ntp, rgbLed, sd);

bool otaInicializadoCompleto = false;
bool sdDisponivel = false;
enum { PAGE_CLOCK = 0, PAGE_NTP_1, PAGE_NTP_2, PAGE_NET_1, PAGE_NET_2, PAGE_SYS, PAGE_WIFI, PAGE_SD, PAGE_COUNT };
uint32_t tempoUltimoComando = 0; // Monitor de ociosidade

void setup() 
{
  Serial.begin(115200);
  delay(100); 
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

  display.begin();
  display.setRotation(1); 
  display.clear();
  display.println("Sistema Inicializando...");
  delay(200); 

  sdDisponivel = commandProcessor.begin();
  delay(100);

  rgbLed.begin();
  gateway.begin();
  ntp.carregarConfiguracoes();

  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    ntp.initNTP();
    
    // Sincroniza o clima logo após obter o horário correto da rede
    ClimaManager::atualizar();
    
    OTAManager::begin("ESP32-C6-Gateway");
    otaInicializadoCompleto = true;
  }
  tempoUltimoComando = millis();
}


struct SdInfoCache
{
  char type[24] = "--";
  uint64_t total = 0;
  uint64_t used = 0;
};
SdInfoCache sdInfo;
volatile bool sdReading = false;
bool sdLoadingShown = false;

void sdInfoTask(void*)
{
  String type = sd.getCardType();
  strncpy(sdInfo.type, type.c_str(), sizeof(sdInfo.type) - 1);
  sdInfo.type[sizeof(sdInfo.type) - 1] = '\0';
  sdInfo.total = sd.getCardSizeBytes();
  sdInfo.used = sd.getUsedBytes();
  sdReading = false;
  vTaskDelete(NULL);
}

void startSdRead()
{
  if (sdReading || !sdDisponivel)
  {
    return;
  }
  sdReading = true;
  if (xTaskCreate(sdInfoTask, "sdinfo", 4096, NULL, 1, NULL) != pdPASS)
  {
    sdReading = false;
  }
}

void loop() 
{
  static bool showingDashboard = false;
  static uint32_t responseUntil = 0;
  static uint32_t lastClockUpdate = 0;
  static uint8_t currentPage = 0;
  static uint32_t statusPageShownAt = 0;
  static int lastButtonReading = HIGH;
  static int buttonState = HIGH;
  static uint32_t lastDebounceTime = 0;
  static uint32_t buttonPressedAt = 0;
  static uint8_t holdStage = 0;
  static int8_t lastLedState = -1;
  String comando;

  gateway.handleClient();
  commandProcessor.update();  

  int buttonReading = digitalRead(USER_BUTTON_PIN);
  if (buttonReading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime >= 50 && buttonReading != buttonState)
  {
    buttonState = buttonReading;
    if (buttonState == LOW)
    {
      buttonPressedAt = millis();
      holdStage = 0;
    }
    else if (holdStage == 0)
    {
      currentPage = (currentPage + 1) % PAGE_COUNT;
      display.setRotation(1);
      if (currentPage == PAGE_SD)
      {
        startSdRead();
      }
      showingDashboard = true;
      lastClockUpdate = 0;
      statusPageShownAt = millis();
      tempoUltimoComando = millis();
    }
    else if (holdStage == 1)
    {
      display.showHoldMessage("REINICIANDO", "", 0x07FF);
      delay(500);
      ESP.restart();
    }
    else
    {
      display.showHoldMessage("WI-FI APAGADO", "Reiniciando...", 0xF800);
      delay(500);
      gateway.clearConfig();
    }
  }
  lastButtonReading = buttonReading;

  if (buttonState == LOW)
  {
    uint32_t held = millis() - buttonPressedAt;
    if (held >= 10000 && holdStage < 2)
    {
      holdStage = 2;
      rgbLed.red();
      display.showHoldMessage("APAGAR WI-FI", "Solte p/ confirmar", 0xF800);
    }
    else if (held >= 3000 && holdStage < 1)
    {
      holdStage = 1;
      rgbLed.blue();
      display.showHoldMessage("REINICIAR", "Solte (ou segure 10s)", 0x07FF);
    }
    tempoUltimoComando = millis();
    statusPageShownAt = millis();
  }
  else if (commandProcessor.ledBusy())
  {
    lastLedState = -1;
  }
  else
  {
    int8_t ledState = WiFi.getMode() == WIFI_AP ? 1 : (WiFi.status() == WL_CONNECTED ? 0 : 2);
    if (ledState != lastLedState)
    {
      lastLedState = ledState;
      if (ledState == 0) rgbLed.green();
      else if (ledState == 1) rgbLed.yellow();
      else rgbLed.red();
    }
  }

  if (otaInicializadoCompleto && WiFi.status() == WL_CONNECTED) 
  {
    OTAManager::handle();
    ClimaManager::atualizar(); // Mantém o clima sincronizado a cada 15 min
  }
  else if (!otaInicializadoCompleto && WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    OTAManager::begin("ESP32-C6-Gateway");
    otaInicializadoCompleto = true;
  }

  // Se receber comando UDP, zera o temporizador do Screensaver e acorda o display
  if (gateway.receiveCommand(comando))
  {
    display.setRotation(0);
    showingDashboard = false;
    commandProcessor.executeCommand(comando);
    responseUntil = millis() + 10000;
    tempoUltimoComando = millis(); // Reseta Protetor de Tela
  }

  commandProcessor.update();

  // Controle de estados da tela (Console Log -> Relógio/Dashboard -> Protetor de Tela)
  if (!showingDashboard && static_cast<int32_t>(millis() - responseUntil) >= 0)
  {
    showingDashboard = true;
    display.setRotation(1);
    display.clear(); 
    lastClockUpdate = 0;
  }

  if (showingDashboard)
  {
    if (currentPage != 0 && millis() - statusPageShownAt >= 30000)
    {
      currentPage = 0;
      display.clear();
      lastClockUpdate = 0;
    }

    // Se estiver ocioso há mais de 60 segundos, roda a cascata Sci-Fi hacker
    if (millis() - tempoUltimoComando > 900000) 
    {
      display.desenharMatrixScreensaver();
      lastClockUpdate = millis(); // Evita desenhar o relógio por cima
    } 
    // Caso contrário, renderiza o painel completo atualizado de 1 em 1 segundo
    else if (millis() - lastClockUpdate >= ((currentPage == PAGE_SD && sdReading) ? 100UL : 1000UL)) 
    {
      if (currentPage == 0)
      {
        String dateTime;
        ntp.getDateTime(dateTime, 10);
        display.showClock(dateTime);
      }
      else if (currentPage == PAGE_NTP_1 || currentPage == PAGE_NTP_2)
      {
        bool ntpSynchronized = ntp.isSincronizado();
        String ntpDateTime = ntp.getDateTime(50);
        display.showStatusPage(sdDisponivel, ntpSynchronized, ntpDateTime,
                               ntp.getFusoHora(), ntp.isDstAtivo(),
                               currentPage == PAGE_NTP_1 ? 0 : 1);
      }
      else if (currentPage == PAGE_NET_1 || currentPage == PAGE_NET_2)
      {
        display.showNetworkPage(currentPage == PAGE_NET_1 ? 0 : 1);
      }
      else if (currentPage == PAGE_SYS)
      {
        display.showSystemPage();
      }
      else if (currentPage == PAGE_WIFI)
      {
        String ssids[5];
        int conectada = -1;
        for (uint8_t i = 0; i < 5; i++)
        {
          ssids[i] = gateway.savedSsid(i);
          if (WiFi.status() == WL_CONNECTED && ssids[i].length() > 0 && ssids[i] == WiFi.SSID())
          {
            conectada = i;
          }
        }
        display.showSavedWifiPage(ssids, conectada, gateway.nextSlot());
      }
      else if (sdReading)
      {
        static uint8_t sdFrame = 0;
        sdLoadingShown = true;
        display.showSdLoading(sdFrame++ % 8);
      }
      else
      {
        
        if (sdLoadingShown)
        {
          display.clear();
          sdLoadingShown = false;
        }
        display.showSdPage(sdDisponivel, sdDisponivel ? String(sdInfo.type) : "--",
                           sdInfo.total, sdInfo.used);
      }      lastClockUpdate = millis();
    }
  }
}
