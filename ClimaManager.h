#ifndef CLIMA_MANAGER_H
#define CLIMA_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h> // Biblioteca nativa e leve para o ESP32-C6

class ClimaManager {
public:
    inline static float temperatura = 0.0;
    inline static int codigoCondicao = 0;
    inline static uint32_t ultimaAtualizacao = 0;

    static void atualizar() {
        if (millis() - ultimaAtualizacao < 900000 && ultimaAtualizacao != 0) return;
        if (WiFi.status() != WL_CONNECTED) return;

        ultimaAtualizacao = millis();
        
        NetworkClientSecure client;
        client.setInsecure(); // Desativa a checagem de chaves RSA pesadas para salvar memória RAM

        HTTPClient http;
        
        // Ativa o redirecionamento automático de HTTP (301) para HTTPS (443) de forma transparente
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        // URL da API configurada com os parâmetros corretos para Porto Alegre - RS
        String url = "https://open-meteo.com";
        
        http.begin(client, url);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            // Busca linear direta no texto recebido da API para evitar usar ArduinoJson
            int currentBlockPos = payload.indexOf("\"current\":");
            if (currentBlockPos >= 0) {
                String currentPayload = payload.substring(currentBlockPos);

                // Captura a temperatura real dentro do bloco numérico do "current"
                int tempPos = currentPayload.indexOf("\"temperature_2m\":");
                if (tempPos >= 0) {
                    int startPos = tempPos + 17;
                    int endPos = currentPayload.indexOf(",", startPos);
                    if (endPos == -1 || endPos > currentPayload.indexOf("}", startPos)) {
                        endPos = currentPayload.indexOf("}", startPos);
                    }
                    
                    if (endPos > startPos) {
                        temperatura = currentPayload.substring(startPos, endPos).toFloat();
                    }
                }

                // Captura o código de condição meteorológica correspondente do "current"
                int codePos = currentPayload.indexOf("\"weather_code\":");
                if (codePos >= 0) {
                    int startPos = codePos + 15;
                    int endPos = currentPayload.indexOf(",", startPos);
                    if (endPos == -1 || endPos > currentPayload.indexOf("}", startPos)) {
                        endPos = currentPayload.indexOf("}", startPos);
                    }

                    if (endPos > startPos) {
                        codigoCondicao = currentPayload.substring(startPos, endPos).toInt();
                    }
                }
                Serial.printf("[CLIMA] Sincronizado -> Temp: %.1f C | Cod WMO: %d\n", temperatura, codigoCondicao);
            }
        } 
        else {
            Serial.printf("[CLIMA ERRO] Falha no transporte. Codigo HTTP: %d\n", httpCode);
        }
        http.end();
    }

    static String obterTextoCondicao() {
        if (codigoCondicao == 0) return "Ceu Limpo";
        if (codigoCondicao >= 1 && codigoCondicao <= 3) return "Parc. Nublado";
        if (codigoCondicao >= 45 && codigoCondicao <= 48) return "Nevoeiro";
        if (codigoCondicao >= 51 && codigoCondicao <= 65) return "Chuva/Garoa";
        if (codigoCondicao >= 71 && codigoCondicao <= 77) return "Neve";
        if (codigoCondicao >= 80 && codigoCondicao <= 82) return "Pancadas Chuva";
        if (codigoCondicao >= 95 && codigoCondicao <= 99) return "Tempestade";
        return "Nublado";
    }
};

#endif
