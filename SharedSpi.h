#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// Initialize once in setup before creating any worker task. Nested calls are safe.
namespace SharedSpi {
 inline SemaphoreHandle_t mutex=nullptr;
 inline void begin(){if(!mutex)mutex=xSemaphoreCreateRecursiveMutex();if(!mutex){Serial.println("SPI mutex allocation failed");abort();}}
 class Guard {
 public:
  Guard(){begin();xSemaphoreTakeRecursive(mutex,portMAX_DELAY);}
  ~Guard(){digitalWrite(4,HIGH);digitalWrite(14,HIGH);xSemaphoreGiveRecursive(mutex);}
  Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
 };
}
