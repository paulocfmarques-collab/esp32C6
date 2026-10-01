#include "CommandProcessor.h"

#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "RGBLed.h"
#include "SDUtil.h"

#include <WiFi.h>
#include <esp_system.h>

CommandProcessor::CommandProcessor(DisplayUtil& display, ESP32Gateway& gateway,
                                   NTPUtil& ntp, RGBLed& led, SDUtil& sd)
    : display_(display), gateway_(gateway), ntp_(ntp), led_(led), sd_(sd)
{
}

bool CommandProcessor::begin()
{
  sdReady_ = sd_.begin();
  String status = sdReady_ ? "Cartao SD pronto" : "Cartao SD indisponivel";
  Serial.println(status);
  display_.println(status);
  return sdReady_;
}

void CommandProcessor::answerAll(String message, bool log)
{
  Serial.print(message);

  String displayMessage = message;
  displayMessage.replace("\r", "");
  displayMessage.replace("\n", "");
  if (displayMessage.length() > 0)
  {
    display_.println(displayMessage);
  }

  gateway_.sendMessage(message);

  if (log)
  {
    saveLog(message);
  }
}

void CommandProcessor::saveLog(String message)
{
  if (!sdReady_)
  {
    Serial.println("[LOG ERRO] Cartao SD nao esta ativo.");
    return;
  }

  String timestamp;
  ntp_.getDateTime(timestamp);
  if (timestamp == "Erro ao obter data e hora")
  {
    timestamp = "Sem Hora Sinc.";
  }

  String entry = "[" + timestamp + "] " + message;
  if (!entry.endsWith("\n"))
  {
    entry += "\n";
  }

  if (!sd_.appendText("/log.txt", entry))
  {
    Serial.println("[LOG ERRO] Falha ao gravar /log.txt no cartao SD.");
  }
}

void CommandProcessor::startBlink(uint16_t pulses, uint32_t interval, bool continuous)
{
  blinkActive_ = true;
  blinkOn_ = true;
  blinkContinuous_ = continuous;
  pulsesRemaining_ = pulses;
  blinkInterval_ = interval;
  lastBlinkChange_ = millis();
  led_.white();
}

void CommandProcessor::executeCommand(String command)
{
  command.trim();
  if (command.length() == 0)
  {
    return;
  }

  String commandLog = "> " + command + "\n";
  Serial.print(commandLog);
  display_.println("> " + command);
  saveLog(commandLog);

  if (command == "RESET_WIFI")
  {
    answerAll("Limpando configuracao Wi-Fi...\n");
    gateway_.clearConfig();
  }
  else if (command == "LED_ON")
  {
    blinkActive_ = false;
    led_.white();
    answerAll("LED ligado\n");
  }
  else if (command == "LED_OFF")
  {
    blinkActive_ = false;
    led_.off();
    answerAll("LED desligado\n");
  }
  else if (command.startsWith("LED_PISCA"))
  {
    uint16_t pulses = 10;
    uint32_t interval = 250;
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);

    if (firstColon >= 0 && secondColon > firstColon)
    {
      int requestedPulses = command.substring(firstColon + 1, secondColon).toInt();
      long requestedInterval = command.substring(secondColon + 1).toInt();
      if (requestedPulses < 1 || requestedPulses > 100 ||
          requestedInterval < 1 || requestedInterval > 5000)
      {
        answerAll("Parametros LED_PISCA invalidos\n");
        return;
      }
      pulses = static_cast<uint16_t>(requestedPulses);
      interval = static_cast<uint32_t>(requestedInterval);
    }

    startBlink(pulses, interval, false);
    answerAll("LED piscando " + String(pulses) + " vezes\n");
  }
  else if (command.startsWith("LED_BLINK"))
  {
    uint32_t interval = 250;
    int separator = command.indexOf(':');
    if (separator >= 0)
    {
      long requestedInterval = command.substring(separator + 1).toInt();
      if (requestedInterval < 50 || requestedInterval > 60000)
      {
        answerAll("Intervalo LED_BLINK invalido (50 a 60000 ms)\n");
        return;
      }
      interval = static_cast<uint32_t>(requestedInterval);
    }

    startBlink(0, interval, true);
    answerAll("Blink iniciado (" + String(interval) + " ms)\n");
  }
  else if (command == "CPU")
  {
    String message = "Modelo: " + String(ESP.getChipModel()) +
                     "\nRevisao: " + String(ESP.getChipRevision()) +
                     "\nNucleos: " + String(ESP.getChipCores()) +
                     "\nCPU: " + String(ESP.getCpuFreqMHz()) + " MHz" +
                     "\nRAM livre: " + String(ESP.getFreeHeap()) + " bytes\n";
    answerAll(message);
  }
  else if (command == "RAM")
  {
    String message = "Heap livre: " + String(ESP.getFreeHeap()) +
                     "\nMenor heap livre: " + String(ESP.getMinFreeHeap()) +
                     "\nMaior bloco livre: " + String(ESP.getMaxAllocHeap()) + "\n";
    answerAll(message);
  }
  else if (command == "FLASH")
  {
    String message = "Flash total: " + String(ESP.getFlashChipSize()) +
                     "\nVelocidade Flash: " + String(ESP.getFlashChipSpeed()) +
                     "\nTamanho Sketch: " + String(ESP.getSketchSize()) +
                     "\nEspaco livre: " + String(ESP.getFreeSketchSpace()) + "\n";
    answerAll(message);
  }
  else if (command == "INIT")
  {
    answerAll("Motivo reset: " + String(static_cast<int>(esp_reset_reason())) + "\n");
  }
  else if (command == "UPTIME")
  {
    answerAll("Uptime: " + String(millis()) + " ms\n");
  }
  else if (command == "MAC")
  {
    answerAll("MAC: " + WiFi.macAddress() + "\n");
  }
  else if (command == "NET_INFO")
  {
    IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
    String message = "IP: " + ip.toString() +
                     "\nGateway: " + WiFi.gatewayIP().toString() +
                     "\nMascara de rede: " + WiFi.subnetMask().toString() +
                     "\nRSSI: " + String(WiFi.RSSI()) + " dBm\nNome da Rede: " +
                     WiFi.SSID() + "\n";
    answerAll(message);
  }
  else if (command == "TIME")
  {
    String dateTime;
    ntp_.getDateTime(dateTime);
    answerAll("Data e hora: " + dateTime + "\n");
  }
  else if (command == "LIST")
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
    }
    else
    {
      answerAll("--- Arquivos SD ---\n" + sd_.listFiles("/") + "-------------------\n");
    }
  }
  else if (command == "SD_TYPE")
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
    }
    else
    {
      answerAll("Tipo do cartao SD: " + sd_.getCardType() + "\n");
    }
  }
  else if (command == "SD_SIZE")
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
    }
    else
    {
      uint64_t sizeBytes = sd_.getCardSizeBytes();
      uint64_t sizeMiB = sizeBytes / (1024ULL * 1024ULL);
      char message[96];
      snprintf(message, sizeof(message), "Tamanho do cartao SD: %llu bytes (%llu MiB)\n",
               static_cast<unsigned long long>(sizeBytes),
           static_cast<unsigned long long>(sizeMiB));
      answerAll(message);
    }
  }
  else if (command == "SD_TEST")
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
    }
    else
    {
      answerAll(sd_.test() ? "Teste do SD: OK\n" : "Teste do SD: FALHOU\n");
    }
  }
  else if (command.startsWith("READ:"))
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
      return;
    }

    String path = command.substring(5);
    path.trim();
    if (!path.startsWith("/"))
    {
      path = "/" + path;
    }
    if (!sd_.exists(path))
    {
      answerAll("Arquivo nao existe: " + path + "\n");
      return;
    }

    String contents = sd_.readText(path);
    answerAll("--- Lendo: " + path + " ---\n", false);
    for (size_t offset = 0; offset < contents.length(); offset += 512)
    {
      answerAll(contents.substring(offset, offset + 512), false);
    }
    answerAll("\n--- Fim do arquivo ---\n");
  }
  else if (command.startsWith("DEL:"))
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
      return;
    }

    String path = command.substring(4);
    path.trim();
    if (!path.startsWith("/"))
    {
      path = "/" + path;
    }
    answerAll(sd_.removeFile(path) ? "Arquivo deletado: " + path + "\n"
                                      : "Erro ao deletar: " + path + "\n");
  }
  else
  {
    answerAll("Comando invalido\n");
  }
}

void CommandProcessor::update()
{
  if (!blinkActive_ || millis() - lastBlinkChange_ < blinkInterval_)
  {
    return;
  }

  lastBlinkChange_ = millis();
  blinkOn_ = !blinkOn_;
  if (blinkOn_)
  {
    led_.white();
  }
  else
  {
    led_.off();
    if (!blinkContinuous_ && pulsesRemaining_ > 0 && --pulsesRemaining_ == 0)
    {
      blinkActive_ = false;
    }
  }
}