#include "RGBLed.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "SDUtil.h"
#include "CommandProcessor.h"
#include <WiFi.h>

DisplayUtil display;
RGBLed rgbLed;
ESP32Gateway gateway;
NTPUtil ntp;
SDUtil sd;
CommandProcessor commandProcessor(display, gateway, ntp, rgbLed, sd);

void setup() 
{
  String text;
  Serial.begin(115200);
  delay(500); 

  Serial.println("----------------------------------------------------------------------------------------");
  display.begin();
  Serial.println("-----------");
  display.println("-----------");
  display.println("Display ok!");
  Serial.println("Display ok!");
  rgbLed.begin();
  gateway.begin();
  commandProcessor.begin();

  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    ntp.initNTP();
  }

  display.println("Gateway ok!");
  Serial.println("Gateway ok!");
  Serial.println("----------------------------------------------------------------------------------------");

}

void loop() 
{
  static bool showingClock = false;
  static uint32_t responseUntil = 0;
  static uint32_t lastClockUpdate = 0;
  String comando;

  gateway.handleClient();

  if (gateway.receiveCommand(comando))
  {
    display.setRotation(0);
    showingClock = false;
    commandProcessor.executeCommand(comando);
    responseUntil = millis() + 10000;
  }

  commandProcessor.update();

  if (!showingClock && static_cast<int32_t>(millis() - responseUntil) >= 0)
  {
    display.setRotation(1);
    showingClock = true;
    lastClockUpdate = 0;
  }

  if (showingClock && millis() - lastClockUpdate >= 1000)
  {
    String dateTime;
    ntp.getDateTime(dateTime, 10);
    display.showClock(dateTime);
    lastClockUpdate = millis();
  }
}