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
    String savedSsid(uint8_t slot) const;
    uint8_t nextSlot() const { return nextWifiSlot_; }
    

private:
    struct WifiCredential
    {
        String ssid;
        String password;
    };

    bool connectWifi();
    void loadWifiCredentials();
    String buildConfigPage() const;
    void initPortal();
    void setupInfoRoutes();
    void setupWifiConfigRoutes();
    void saveWifi();
    void cleanConfig();

    WiFiUDP udp;
    const int udpPort = 4210;
    WebServer server;
    Preferences prefs;
    WifiCredential savedNetworks_[5];
    uint8_t nextWifiSlot_ = 0;
};

#endif // ESP32_GATEWAY_H