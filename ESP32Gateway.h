#ifndef ESP32_GATEWAY_H
#define ESP32_GATEWAY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

class ESP32Gateway
{
public:
    ESP32Gateway();
    void begin();
    void handleClient();
    bool receiveCommand(String& comando);
    void sendMessage(String message);
    void clearConfig();
    

private:
    bool connectWifi();
    void initPortal();
    void saveWifi();
    void cleanConfig();

    WiFiUDP udp;
    const int udpPort = 4210;
    WebServer server;
    Preferences prefs;

    // HTML da página de configuração
    const char* htmlPage PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuração WiFi</title>
    <style>
    body { font-family: Arial, sans-serif; margin: 40px; background-color: #f4f4f9; text-align: center; }
    .container { background: white; max-width: 300px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    input { width: 100%; padding: 8px; margin: 10px 0; box-sizing: border-box; }
    input[type="submit"] { background: #007bff; color: white; border: none; cursor: pointer; }
    </style>
    </head>
    <body>
    <div class="container">
    <h2>Configuração WiFi - ESP32-C6</h2>
    <form action="/salvar" method="POST">
        <label>SSID:</label>
        <input type="text" name="ssid" placeholder="Nome da rede" required>
        <label>Senha:</label>
        <input type="password" name="senha" placeholder="Senha da rede">
        <input type="submit" value="Salvar">
    </form>
    </div>
    </body>
    </html>
    )rawliteral";
};

#endif // ESP32_GATEWAY_H