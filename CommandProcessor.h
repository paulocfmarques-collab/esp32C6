#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>

class DisplayUtil;
class ESP32Gateway;
class NTPUtil;
class RGBLed;
class SDUtil;

class CommandProcessor
{
public:
    CommandProcessor(DisplayUtil& display, ESP32Gateway& gateway, NTPUtil& ntp,
                     RGBLed& led, SDUtil& sd);

    bool begin();
    void executeCommand(String command);
    void update();

private:
    void answerAll(String message, bool log = true);
    void saveLog(String message);
    void startBlink(uint16_t pulses, uint32_t interval, bool continuous);

    DisplayUtil& display_;
    ESP32Gateway& gateway_;
    NTPUtil& ntp_;
    RGBLed& led_;
    SDUtil& sd_;

    bool sdReady_ = false;
    bool blinkActive_ = false;
    bool blinkOn_ = false;
    bool blinkContinuous_ = false;
    uint16_t pulsesRemaining_ = 0;
    uint32_t blinkInterval_ = 250;
    uint32_t lastBlinkChange_ = 0;
};

#endif