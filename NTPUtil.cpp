#include <Preferences.h>
#include "NTPUtil.h"
#include <time.h>

void NTPUtil::carregarConfiguracoes() {
    Preferences prefs;
    prefs.begin("ntp_cfg", true);
    fusoHora = prefs.getInt("fuso", -3); // Padrão travado em UTC-3
    dstAtivo = prefs.getBool("dst", false); 
    prefs.end();
}

void NTPUtil::aplicarConfiguracaoNTP() {
    int32_t deslocamentoSegundos = fusoHora * 3600;
    int32_t deslocamentoDST = dstAtivo ? 3600 : 0;

    configTime(deslocamentoSegundos, deslocamentoDST, "a.st1.ntp.br", "pool.ntp.org", "time.nist.gov");
    Serial.printf("[NTP] Configuracao aplicada -> Fuso: %d | DST: %s\n", fusoHora, dstAtivo ? "ON" : "OFF");
}

bool NTPUtil::initNTP() {
    Serial.println("--- Inicializando NTP para ESP32-C6 ---");
    carregarConfiguracoes();
    aplicarConfiguracaoNTP();

    for (int tentativa = 0; tentativa < 15; tentativa++) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 1000)) {
            Serial.println("[NTP] Sincronizacao concluida.");
            return true;
        }
        delay(500);
    }
    return false;
}

void NTPUtil::atualizarConfiguracao(int32_t novoFuso, bool novoDst) {
    fusoHora = novoFuso;
    dstAtivo = novoDst;

    Preferences prefs;
    prefs.begin("ntp_cfg", false);
    prefs.putInt("fuso", fusoHora);
    prefs.putBool("dst", dstAtivo);
    prefs.end();

    aplicarConfiguracaoNTP();
}

void NTPUtil::getDateTime(String& dateTime, uint32_t timeoutMs) {
    struct tm timeinfo;
    uint32_t timeoutSeguro = (timeoutMs < 50) ? 50 : timeoutMs;

    if (getLocalTime(&timeinfo, timeoutSeguro)) {
        char buffer[30]; // Tamanho seguro alocado explicitamente
        strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
        dateTime = String(buffer);
    } else {
        dateTime = "Erro ao obter data e hora";
    }
}

String NTPUtil::getDateTime(uint32_t timeoutMs) {
    String dateTime;
    getDateTime(dateTime, timeoutMs);
    return dateTime;
}

bool NTPUtil::isSincronizado() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo, 50); // Timeout rápido e seguro para diagnóstico
}

String NTPUtil::getSomenteHora() {
    String dateTime;
    getDateTime(dateTime, 50);
    int separator = dateTime.indexOf(' ');
    if (separator >= 0) {
        return dateTime.substring(separator + 1);
    }
    return "--:--:--";
}
