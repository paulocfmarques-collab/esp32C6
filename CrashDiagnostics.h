#pragma once
#include <Arduino.h>

class CrashDiagnostics
{
  public:
    static String ler(const String& motivoReset, bool& podeLimpar);
    static String limpar();
};
