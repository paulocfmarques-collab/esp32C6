#include "ESP32Gateway.h"
#include "DisplayUtil.h"
#include "RGBLed.h"

extern DisplayUtil display;
extern RGBLed rgbLed;

ESP32Gateway::ESP32Gateway()
  : server(80)
{
}

void ESP32Gateway::begin()
{
    // Tenta conectar ao WiFi
    if (!connectWifi())
    {
        initPortal();
    }

    udp.begin(udpPort);
} 

void ESP32Gateway::handleClient()
{
  if (WiFi.getMode() == WIFI_AP)
  {
    server.handleClient();
  }
}

void ESP32Gateway::clearConfig()
{
  cleanConfig();
}

bool ESP32Gateway::receiveCommand(String& comando)
{
  comando = "";

  int packetSize = udp.parsePacket();
  if (packetSize <= 0)
  {
    return false;
  }

  char packetBuffer[256];
  int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
  if (len <= 0)
  {
    return false;
  }

  packetBuffer[len] = '\0';
  comando = String(packetBuffer);
  comando.trim();

  return comando.length() > 0;
}

bool ESP32Gateway::connectWifi() 
{
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("senha", "");
  prefs.end();

  if (ssid == "") return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  display.println("Conectando a:");
  display.println(ssid);
  Serial.println("Conectando a:");
  Serial.println(ssid);
  rgbLed.blue();

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    tentativas++;
  }

  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado)
  {
    rgbLed.green();
    display.println(WiFi.localIP().toString());
    Serial.println(WiFi.localIP().toString());
  }

  return conectado;
}

void ESP32Gateway::saveWifi() {
  display.println("Salvando rede...");
  String novoSSID = server.arg("ssid");
  String novaSenha = server.arg("senha");

  prefs.begin("wifi", false);
  prefs.putString("ssid", novoSSID);
  prefs.putString("senha", novaSenha);
  prefs.end();

  server.send(200, "text/html", "<h2>Configuracao salva! Reiniciando...</h2>");
  delay(2000);
  ESP.restart();
}

void ESP32Gateway::initPortal() 
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_C6_CONFIG");
  rgbLed.yellow();
  
    display.println("Portal Ativo!");
    display.println("Wifi: ESP32_C6_CONFIG");
    display.println("IP: 192.168.4.1");

    server.on("/", HTTP_GET, [this]() {
      server.send(200, "text/html", htmlPage);
  });
    server.on("/salvar", HTTP_POST, [this]() {
      saveWifi();
    });
  server.begin();
}

void ESP32Gateway::cleanConfig() 
{
  display.println("Limpando Memoria...");

  prefs.begin("wifi", false);
  prefs.clear(); 
  prefs.end();
  
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.print("WiFi zerado. Reiniciando...\n");
  udp.endPacket();

  for(int i=0; i<10; i++) 
  {
    rgbLed.breathing(255, 0, 0, 100);
  }
  ESP.restart(); 
}

void ESP32Gateway::sendMessage(String message) 
{
  if (!udp.remoteIP())
  {
    return;
  }

  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.print(message);
  udp.endPacket();
}
