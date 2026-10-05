#include "RGBLed.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "SDUtil.h"
#include "CommandProcessor.h"
#include "OTAManager.h"
#include "ClimaManager.h"
#include <WiFi.h>

DisplayUtil display;
RGBLed rgbLed;
ESP32Gateway gateway;
NTPUtil ntp;
SDUtil sd;
CommandProcessor commandProcessor(display, gateway, ntp, rgbLed, sd);

bool otaInicializadoCompleto = false;
uint32_t tempoUltimoComando = 0; // Monitor de ociosidade

void setup() 
{
  Serial.begin(115200);
  delay(100); 

  display.begin();
  display.setRotation(1); 
  display.clear();
  display.println("Sistema Inicializando...");
  delay(200); 

  commandProcessor.begin();
  delay(100);

  rgbLed.begin();
  gateway.begin();

  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    ntp.carregarConfiguracoes();
    ntp.initNTP();
    
    // Sincroniza o clima logo após obter o horário correto da rede
    ClimaManager::atualizar();
    
    OTAManager::begin("ESP32-C6-Gateway");
    otaInicializadoCompleto = true;
  }
  tempoUltimoComando = millis();
}


void loop() 
{
  static bool showingClock = false;
  static uint32_t responseUntil = 0;
  static uint32_t lastClockUpdate = 0;
  String comando;

  gateway.handleClient();
  commandProcessor.update();  

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
    showingClock = false;
    commandProcessor.executeCommand(comando);
    responseUntil = millis() + 10000;
    tempoUltimoComando = millis(); // Reseta Protetor de Tela
  }

  commandProcessor.update();

  // Controle de estados da tela (Console Log -> Relógio/Dashboard -> Protetor de Tela)
  if (!showingClock && static_cast<int32_t>(millis() - responseUntil) >= 0)
  {
    showingClock = true;
    display.setRotation(1);
    display.clear(); 
    lastClockUpdate = 0;
  }

  if (showingClock) 
  {
    // Se estiver ocioso há mais de 60 segundos, roda a cascata Sci-Fi hacker
    if (millis() - tempoUltimoComando > 900000) 
    {
      display.desenharMatrixScreensaver();
      lastClockUpdate = millis(); // Evita desenhar o relógio por cima
    } 
    // Caso contrário, renderiza o painel completo atualizado de 1 em 1 segundo
    else if (millis() - lastClockUpdate >= 1000) 
    {
      String dateTime;
      ntp.getDateTime(dateTime, 10);
      display.showClock(dateTime);
      lastClockUpdate = millis();
    }
  }
}
