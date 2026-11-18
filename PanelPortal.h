#pragma once
#include <Arduino.h>

class NetworkMonitor;
String panelPortalPage();
String panelPortalCards(const NetworkMonitor& network, const String* profiles, uint8_t nextProfile);
