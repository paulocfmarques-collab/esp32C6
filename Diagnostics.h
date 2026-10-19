#pragma once
#include <Arduino.h>

class SDUtil;
namespace Diagnostics 
{
void begin(SDUtil& sd);
void process();
void event(const char* name, const char* detail = "");
void wifiDisconnected(uint32_t reason);
uint32_t metricsStart(uint32_t peerIp);
void metricsEnd(uint32_t id, size_t bytes, uint32_t durationMs, int code, bool connected);
}
