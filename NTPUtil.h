#ifndef NTP_UTIL_H
#define NTP_UTIL_H

#include <Arduino.h>

class NTPUtil
{
public:
    bool initNTP();
    void getDateTime(String& dateTime, uint32_t timeoutMs = 5000);
};

#endif