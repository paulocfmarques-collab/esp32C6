#include "CommandProcessor.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "RGBLed.h"
#include "SDUtil.h"
#include "ClimaManager.h"

#include <WiFi.h>
#include <esp_system.h>

#include <Preferences.h>

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

  // 1. Grava no cartão isoladamente com exclusividade física
  if (!sd_.appendText("/log.txt", entry))
  {
    Serial.println("[LOG ERRO] Falha ao gravar /log.txt no cartao SD.");
  }
  else 
  {
    // 2. Só dispara o visual gráfico DEPOIS que o arquivo fechou com sucesso!
    display_.dispararAnimacaoGravacaoSD();
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
  String message = "";
  Serial.print(commandLog);
  display_.println("> " + command);
  saveLog(commandLog);
  
  if (command == "help")
  {
    message = "===== COMANDOS DISPONIVEIS =====\n"
              "[ SISTEMA ]\n"
              "  help        - Mostra este menu de ajuda\n"
              "  info        - Diagnostico completo do dispositivo\n"
              "  status      - Resumo simplificado de conexoes\n"
              "  uptime      - Tempo de atividade da CPU\n"
              "  reason      - Exibe o motivo do ultimo reset\n"
              "  version     - Exibe a versao do firmware\n"
              "  build       - Exibe a data/hora de compilacao\n"
              "  alive       - Teste rápido de ping de rede\n"
              "  reboot      - Reinicia o dispositivo remotamente\n"
              "[ REDE / WI-FI ]\n"
              "  net_info    - Detalhes de IP, Gateway, DNS e RSSI\n"
              "  mac         - Endereco fisico MAC do chip\n"
              "  reset_wifi  - Apaga credenciais e reinicia em modo AP\n"
              "  net_scan    - Escaneia redes proximas e gera netscan.json\n"
              "  time        - Exibe a hora atual do NTP\n"
              "  date        - Exibe a data atual do NTP\n"
              "  set_fuso:X  - Altera fuso horario (Ex: set_fuso:-3)\n"
              "  dst_on/off  - Ativa/Desativa horario de verao\n"
              "  clima_sync  - Forca sincronizacao com API de clima\n"
              "[ CARTAO SD ]\n"
              "  list        - Lista arquivos e diretorios no SD\n"
              "  read:PATH   - Le conteudo de um arquivo (Ex: read:log.txt)\n"
              "  del:PATH    - Remove um arquivo do cartao\n"
              "  log_clear   - Limpa o arquivo de logs do sistema\n"
              "  sd_type     - Exibe o padrao fisico do cartao\n"
              "  sd_size     - Exibe a capacidade total do cartao\n"
              "  sd_test     - Executa teste de leitura/escrita\n"
              "[ CONTROLE LED ]\n"
              "  led_on/off  - Liga/Desliga o LED em modo estatico\n"
              "  led_cyan    - Altera cor do LED para ciano\n"
              "  led_breath  - Ativa efeito de pulsacao em ciano\n"
              "  led_pisca:P:I - Pisca P vezes no intervalo I (ms)\n"
              "  led_blink:I   - Pisca continuamente no intervalo I (ms)\n"
              "  led_color:R:G:B - Define cor RGB customizada (0-255)\n"
              "================================\n";
    answerAll(message);
  }
  else if (command == "reboot")
  {
    answerAll("Reiniciando o sistema remotamente via UDP...\n");
    delay(500);
    ESP.restart();
  }
  else if (command == "clima_sync")
  {
    answerAll("Forcando atualizacao manual do clima...\n");
    // Zera o timer interno para burlar a trava de 15 minutos
    ClimaManager::ultimaAtualizacao = 0; 
    ClimaManager::atualizar();
    
    message = "Clima Atualizado ->\nTemp: " + String(ClimaManager::temperatura, 1) + 
              " C\nCondicao: " + ClimaManager::obterTextoCondicao() + "\n";
    answerAll(message);
  }
  else if (command == "log_clear")
  {
    if (!sdReady_) {
      answerAll("Erro: SD indisponivel\n");
    } else {
      if (sd_.exists("/log.txt")) {
        sd_.removeFile("/log.txt");
        answerAll("Arquivo /log.txt deletado com sucesso.\n");
      } else {
        answerAll("Arquivo /log.txt nao existia no cartao.\n");
      }
    }
  }
  else if (command == "net_scan")
  {
    if (scanningWifi_) {
      answerAll("Erro: Varredura ja esta em andamento.\n");
    } else if (!sdReady_) {
      answerAll("Erro: SD inativo. Nao eh possivel salvar o JSON.\n");
    } else {
      answerAll("Iniciando varredura Wi-Fi em background...\n");
      scanningWifi_ = true;
      // O parametro 'true' ativa o escaneamento assincrono (nao travante)
      WiFi.scanNetworks(true); 
    }
  }
  else if (command == "info")
  {
    String dataHoraCompleta; ntp_.getDateTime(dataHoraCompleta, 100);
    String dataStr = "Sem Sinc.", horaStr = "--:--:--";
    if (dataHoraCompleta.length() >= 19) {
        dataStr = dataHoraCompleta.substring(0, 10);
        horaStr = dataHoraCompleta.substring(11, 19);
    }
    uint32_t heapLivre = ESP.getFreeHeap() / 1024;
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

    message = "===== DEVICE INFO =====\n"
              "Hostname: ESP32C6\n"
              "Firmware: " + obterVersaoAutomatica() + "\n"
              "Build: " + String(__DATE__) + " " + String(__TIME__) + "\n" +
              "SSID: " + WiFi.SSID() + "\n" +
              "IP: " + WiFi.localIP().toString() + "\n" +
              "MAC: " + WiFi.macAddress() + "\n" +
              "RSSI: " + String(WiFi.RSSI()) + " dBm\n" +
              "Heap Livre: " + String(heapLivre) + " KB\n" +
              "Flash Livre: " + String(flashLivre, 1) + " MB\n" +
              "SD Card: " + (sdReady_ ? "Ativo" : "Inativo") + "\n" + 
              "Data: " + dataStr + "\n" +
              "Hora: " + horaStr + "\n" +
              "Uptime: " + String(millis()) + " ms\n" +
              "Reset: " + obterMotivoReset() + "\n" + 
              "=======================\n";
    answerAll(message);              
  }
  else if (command == "reason")
  {
      message = "===== ULTIMO RESET =====\n"
                "Motivo: " + obterMotivoReset() + "\n"
                "Uptime Atual: " + String(millis() / 1000) + " s\n"
                "========================\n";
      answerAll(message);
  }
  else if (command == "reset_wifi")
  {
    answerAll("Limpando configuracao Wi-Fi...\n");
    gateway_.clearConfig();
  }
  else if (command == "version")
  {
      message = "Versao Firmware: " + obterVersaoAutomatica() + "\n";
      answerAll(message);
  }
  else if (command == "build")
  {
      message = "Build:\nData: " + String(__DATE__) + "\nHora: " + String(__TIME__) + "\n";
      answerAll(message);
  }
  else if (command == "status") {
        String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
        uint32_t heapKB = ESP.getFreeHeap() / 1024;
        message = "ONLINE (MESTRE)\nWiFi: " + statusWifi + 
                  "\nSD: " + (sdReady_ ? "OK" : "FALHA") + 
                  "\nNTP: " + (ntp_.isSincronizado() ? "OK" : "FALHA") + 
                  "\nHeap: " + String(heapKB) + " KB\n";
        answerAll(message);
  }
  else if (command == "led_on")
  {
    blinkActive_ = false;
    breathActive_ = false; // Desativa o modo de respiração se outro comando de LED for recebido
    led_.white();
    answerAll("LED ligado\n");
  }
  else if (command == "led_off")
  {
    blinkActive_ = false;
    breathActive_ = false; // Desativa o modo de respiração se outro comando de LED for recebido
    led_.off();
    answerAll("LED desligado\n");
  }
  else if (command.startsWith("led_pisca"))
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
  else if (command.startsWith("led_blink"))
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
  else if (command == "dst_on")
  {
    Preferences prefs;
    prefs.begin("ntp_cfg", true);
    int currentFuso = prefs.getInt("fuso", -3);
    prefs.end();

    ntp_.atualizarConfiguracao(currentFuso, true);
    answerAll("Horário de Verão ativado (DST ON)\n");
  }
  else if (command == "dst_off")
  {
    Preferences prefs;
    prefs.begin("ntp_cfg", true);
    int currentFuso = prefs.getInt("fuso", -3);
    prefs.end();

    ntp_.atualizarConfiguracao(currentFuso, false);
    answerAll("Horário de Verão desativado (DST OFF)\n");
  }
  else if (command.startsWith("set_fuso:"))
  {
    int separator = command.indexOf(':');
    if (separator >= 0)
    {
      int novoFuso = command.substring(separator + 1).toInt();
      if (novoFuso < -12 || novoFuso > 14)
      {
        answerAll("Erro: Fuso inválido. Escolha de -12 a +14.\n");
        return;
      }
      
      Preferences prefs;
      prefs.begin("ntp_cfg", true);
      bool currentDst = prefs.getBool("dst", false);
      prefs.end();

      ntp_.atualizarConfiguracao(novoFuso, currentDst);
      answerAll("Novo fuso configurado: " + String(novoFuso) + "\n");
    }
  }
  else if (command == "temp") {
    message = "CPU Temp: " + String(temperatureRead()) + " C";
    answerAll(message);
  }
  else if (command == "cpu")
  {
    message = "CPU:\nModelo: " + String(ESP.getChipModel()) +
              "\nRevisao: " + String(ESP.getChipRevision()) +
              "\nNucleos: " + String(ESP.getChipCores()) +
              "\nCPU: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    answerAll(message);
  }
  else if (command == "ram")
  {
    message = "RAM:\nHeap Total: " + String(ESP.getHeapSize() / 1024.0) + " KB\n" +
              "Heap Livre: " + String(ESP.getFreeHeap() / 1024.0) + " KB\n" +
              "Menor Bloco Livre: " + String(ESP.getMinFreeHeap() / 1024.0) + " KB\n" +
              "Maior Bloco Livre: " + String(ESP.getMaxAllocHeap() / 1024.0) + " KB\n" +
              "RAM Utilizada: " + String(100.0 * (ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) + " %\n";
    answerAll(message);
  }
  else if (command == "flash")
  {
    message = "FLASH:\nTamanho: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB\n" +
              "Velocidade: " + String(ESP.getFlashChipSpeed() / 1000 / 1000) + " MHz\n" + 
              "Flash Mode: " + String(ESP.getFlashChipMode()) + "\n" +
              "Sketch Size: " + String(ESP.getSketchSize() / 1024 / 1024) + " MB\n" +
              "Free Sketch Space: " + String(ESP.getFreeSketchSpace() / 1024 / 1024) + " MB\n" +
              "Flash livre: " + String(100.0 * (ESP.getFlashChipSize() - ESP.getFreeSketchSpace()) / ESP.getFlashChipSize()) + " %\n";
    answerAll(message);
  }
  else if (command == "init")
  {
    answerAll("Motivo reset: " + obterMotivoReset() + "\n");
  }
  else if (command == "uptime")
  {
    answerAll("Uptime: " + String(millis()) + " ms\n");
  }
  else if (command == "mac")
  {
    answerAll("MAC: " + WiFi.macAddress() + "\n");
  }
  else if (command == "net_info")
  {
    message = "NET:\nSSID: " + WiFi.SSID() + "\n" +
              "IP: " + WiFi.localIP().toString() + "\n" +
              "Gateway: " + WiFi.gatewayIP().toString() + "\n" +
              "Subnet: " + WiFi.subnetMask().toString() + "\n" +
              "DNS1: " + WiFi.dnsIP(0).toString() + "\n" +
              "DNS2: " + WiFi.dnsIP(1).toString() + "\n" +
              "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    answerAll(message);
  }
  else if (command == "time") {
      answerAll("Hora:\n" + ntp_.getSomenteHora() + "\n");
  }
  else if (command == "date") {
      String dataHoraCompleta; ntp_.getDateTime(dataHoraCompleta, 100);
      String data = (dataHoraCompleta.length() >= 10) ? dataHoraCompleta.substring(0, 10) : "Erro NTP";
      answerAll("Data:\n" + data + "\n");
  }
  else if (command == "list")
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
  else if (command == "sd_type")
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
  else if (command == "sd_size")
  {
    if (!sdReady_)
    {
      answerAll("Erro: SD inacessivel\n");
    }
    else
    {
      uint64_t sizeBytes = sd_.getCardSizeBytes();
      uint64_t sizeMiB = sizeBytes / (1024ULL * 1024ULL);
      message = "";
      char buffer[96];
      snprintf(buffer, sizeof(buffer), "Tamanho do cartao SD: %llu bytes (%llu MiB)\n",
               static_cast<unsigned long long>(sizeBytes),
           static_cast<unsigned long long>(sizeMiB));
      message = buffer; 
      answerAll(message);
    }
  }
  else if (command == "sd_test")
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
  else if (command.startsWith("read:"))
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
  else if (command.startsWith("del:"))
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
  else if (command == "psram") {
    bool psramPresente = ESP.getPsramSize() > 0;
    message = "PSRAM:\nPSRAM Presente: " + String(psramPresente ? "SIM" : "NAO") + 
              "\nTamanho PSRAM: " + String(ESP.getPsramSize() / 1024 / 1024) + " MB" +
              "\nPSRAM Livre: " + String(ESP.getFreePsram() / 1024 / 1024) + " MB" +
              "\nMaior Bloco Livre PSRAM: " + String(ESP.getMaxAllocPsram() / 1024 / 1024) + " MB" +
              "\nPSRAM Utilizada: " + String(100.0 * (ESP.getPsramSize() - ESP.getFreePsram()) / ESP.getPsramSize()) + " %\n";
    answerAll(message);
  }
  else if (command == "led_cyan")
  {
    blinkActive_ = false;
    breathActive_ = false; // Desativa o modo de respiração se outro comando de LED for recebido
    led_.cyan();
    answerAll("LED customizado: Ciano ativo\n");
  }
  else if (command == "led_breath")
  {
    blinkActive_ = false;
    breathActive_ = true; // Ativa o modo de respiração do LED
    answerAll("Modo do LED alterado para Pulsar (Breathing)\n");
    led_.breathing(0, 255, 255, 3); // Pulsação em Ciano
  }
  else if (command.startsWith("led_color:"))
  {
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);
    int thirdColon = command.indexOf(':', secondColon + 1);

    if (firstColon >= 0 && secondColon > firstColon && thirdColon > secondColon)
    {
      int r = command.substring(firstColon + 1, secondColon).toInt();
      int g = command.substring(secondColon + 1, thirdColon).toInt();
      int b = command.substring(thirdColon + 1).toInt();

      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
        blinkActive_ = false;
        breathActive_ = false; // Desativa o modo de respiração se outro comando de LED for recebido
        led_.setColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        answerAll("LED definido para RGB(" + String(r) + "," + String(g) + "," + String(b) + ")\n");
      } else {
        answerAll("Valores RGB invalidos (Use de 0 a 255)\n");
      }
    } else {
      answerAll("Formato incorreto. Use led_color:R:G:B\n");
    }
  }
  else if (command == "alive") {
    answerAll("ip: " + WiFi.localIP().toString() + " - yes\n");
  }
  else
  {
    answerAll("Comando invalido\n");
  }
}

String CommandProcessor::obterMotivoReset() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_UNKNOWN:   return "DESCONHECIDO";
        case ESP_RST_POWERON:   return "POWER-ON (Tomada/VCC)";
        case ESP_RST_EXT:       return "PINO RESET (Botao EN)";
        case ESP_RST_SW:        return "SOFTWARE / OUTROS";
        case ESP_RST_PANIC:     return "CRASH / PANIC (Exception)";
        case ESP_RST_INT_WDT:   return "WATCHDOG INTERNO (Core Travado)";
        case ESP_RST_TASK_WDT:  return "TASK WATCHDOG (Loop Travado)";
        case ESP_RST_WDT:       return "OUTROS WATCHDOGS";
        case ESP_RST_DEEPSLEEP: return "ACORDOU DO DEEP SLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (Queda de Tensao)";
        case ESP_RST_SDIO:      return "RESET VIA SDIO";
        default:                return "CODIGO NAO MAPEADO";
    }
}

void CommandProcessor::update()
{
  if (breathActive_) {
    led_.breathingTask(0, 255, 255); // Roda a tarefa não bloqueante em Ciano
  }
  
  // 1. Processamento da varredura Wi-Fi assíncrona
  if (scanningWifi_)
  {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) // Varredura concluída com sucesso
    {
      String json = "{\n  \"total_redes\": " + String(n) + ",\n  \"redes\": [\n";
      for (int i = 0; i < n; ++i)
      {
        json += "    {\n";
        json += "      \"ssid\": \"" + WiFi.SSID(i) + "\",\n";
        json += "      \"rssi\": " + String(WiFi.RSSI(i)) + ",\n";
        json += "      \"canal\": " + String(WiFi.channel(i)) + "\n";
        json += "    }";
        if (i < n - 1) json += ",";
        json += "\n";
      }
      json += "  ]\n}";
      
      // Salva usando o método protegido contra concorrência do Display
      if (sd_.writeJson("/netscan.json", json)) {
        answerAll("[NET_SCAN] Concluido. Dados salvos em /netscan.json\n");
      } else {
        answerAll("[NET_SCAN] Concluido, mas falhou ao gravar arquivo JSON.\n");
      }
      
      WiFi.scanDelete(); // Limpa a memória alocada pelo scan
      scanningWifi_ = false;
    }
    else if (n == WIFI_SCAN_FAILED)
    {
      answerAll("[NET_SCAN] Falha ao executar varredura de redes.\n");
      scanningWifi_ = false;
    }
  }

  // 2. Manutencao do Blink do LED (Mantenha o seu codigo original abaixo)
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
