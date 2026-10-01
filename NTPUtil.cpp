#include "NTPUtil.h"

#include <time.h>

bool NTPUtil::initNTP()
{
  Serial.println("--- Inicializando NTP para ESP32-C6 ---");

  setenv("TZ", "BRT3", 1);
  tzset();
  configTime(0, 0, "a.st1.ntp.br", "pool.ntp.org", "200.160.7.186");
  Serial.println("[NTP] Serviço iniciado. Aguardando sincronização...");

  for (int tentativa = 0; tentativa < 20; tentativa++)
  {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 1000))
    {
      Serial.println("\n[NTP] Sincronização concluída.");
      Serial.printf("Hora atualizada: %02d:%02d:%02d\n",
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      Serial.println("------------------------------------------------");
      return true;
    }

    Serial.println("[NTP] Aguardando sincronização...");
  }

  Serial.println("[NTP] Erro: não foi possível sincronizar o horário.");
  Serial.println("------------------------------------------------");
  return false;
}

void NTPUtil::getDateTime(String& dateTime, uint32_t timeoutMs)
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, timeoutMs))
  {
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    dateTime = String(buffer);
  }
  else
  {
    dateTime = "Erro ao obter data e hora";
  }
}