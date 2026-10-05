#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ArduinoOTA.h>
#include <WiFi.h>
#include "DisplayUtil.h"
#include "RGBLed.h"

extern DisplayUtil display;
extern RGBLed rgbLed;

class OTAManager {
private:
    static int ultimoPercentual;

public:
    static void begin(const char* hostname = "ESP32-C6-Gateway") {
        ArduinoOTA.setHostname(hostname);

        ArduinoOTA.onStart([]() {
            ultimoPercentual = -1;
            
            // Força o rádio Wi-Fi a trabalhar na velocidade máxima
            WiFi.setSleep(false); 
            Serial.println("[OTA] Alta velocidade e modo horizontal ativados.");
            
            // MUDA A TELA PARA A HORIZONTAL (Modo Paisagem)
            // Usamos a rotação 1 (ou 3 caso precise inverter o lado dos cabos)
            Arduino_ST7789* gfx = display.getDisplay();
            gfx->setRotation(1); 
            gfx->fillScreen(0x0000); // Fundo preto

            // Desenha moldura Tech adaptada para o tamanho horizontal
            gfx->drawRect(8, 8, gfx->width() - 16, gfx->height() - 16, 0x4A49); // Cinza Escuro

            // Título Superior Centralizado
            gfx->setTextColor(0x07FF); // Ciano
            gfx->setTextSize(2);
            String titulo = "SYSTEM FIRMWARE UPDATE";
            int tituloX = (gfx->width() - (titulo.length() * 12)) / 2;
            gfx->setCursor(tituloX, 22);
            gfx->print(titulo);

            // Subtítulo
            gfx->setTextColor(0x7BEF); // Cinza Claro
            gfx->setTextSize(1);
            String subtitulo = "Gravando novos dados via rede sem fio...";
            int subX = (gfx->width() - (subtitulo.length() * 6)) / 2;
            gfx->setCursor(subX, 42);
            gfx->print(subtitulo);

            // Desenha o contorno da Barra de Progresso Horizontal (Centralizada)
            int barY = gfx->height() / 2 - 5;
            int barWidth = gfx->width() - 80; // Deixa 40 pixels de margem nas laterais
            gfx->drawRoundRect(40, barY, barWidth, 18, 5, 0x07FF); // Contorno Ciano
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            int percentage = (progress / (total / 100));
            
            // Atualiza apenas de 2% em 2% para manter o desempenho rápido
            if (percentage >= ultimoPercentual + 2 || percentage == 100) {
                ultimoPercentual = percentage;
                
                Arduino_ST7789* gfx = display.getDisplay();
                int barY = gfx->height() / 2 - 5;
                int maxBarWidth = gfx->width() - 84;
                int currentBarWidth = (maxBarWidth * percentage) / 100;

                // Desenha o preenchimento interno da barra
                uint16_t corBarra = (percentage < 50) ? 0x001F : 0x07FF; // Azul para Ciano
                gfx->fillRoundRect(42, barY + 2, currentBarWidth, 14, 3, corBarra);

                // Mostra a porcentagem centralizada logo abaixo da barra
                int textoY = barY + 28;
                gfx->fillRect(100, textoY, gfx->width() - 200, 20, 0x0000); // Limpa número antigo
                gfx->setTextColor(0xFFFF); // Branco
                gfx->setTextSize(2);
                
                String textoProgresso = String(percentage) + "%";
                int posX = (gfx->width() - (textoProgresso.length() * 12)) / 2;
                gfx->setCursor(posX, textoY);
                gfx->print(textoProgresso);

                // Pisca o LED RGB (Magenta / Off)
                if (percentage % 4 == 0) {
                    rgbLed.magenta();
                } else if (percentage % 2 == 0) {
                    rgbLed.off();
                }
            }
        });

        ArduinoOTA.onEnd([]() {
            WiFi.setSleep(true); 
            Serial.println("\n[OTA] Concluido!");
            
            Arduino_ST7789* gfx = display.getDisplay();
            gfx->fillScreen(0x0000);
            
            // Tela de Sucesso Centralizada na Horizontal
            gfx->setTextColor(0x07E0); // Verde
            gfx->setTextSize(3);
            String msgSucesso = "UPDATE OK!";
            int sucX = (gfx->width() - (msgSucesso.length() * 18)) / 2;
            gfx->setCursor(sucX, gfx->height() / 2 - 20);
            gfx->print(msgSucesso);
            
            gfx->setTextColor(0xFFFF);
            gfx->setTextSize(2);
            String msgReiniciar = "Reiniciando sistema...";
            int reX = (gfx->width() - (msgReiniciar.length() * 12)) / 2;
            gfx->setCursor(reX, gfx->height() / 2 + 15);
            gfx->print(msgReiniciar);
            
            rgbLed.green();
            delay(1200);
        });

        ArduinoOTA.onError([](ota_error_t error) {
            WiFi.setSleep(true); 
            Arduino_ST7789* gfx = display.getDisplay();
            gfx->fillScreen(0x0000);
            
            gfx->setTextColor(0xF800); // Vermelho
            gfx->setTextSize(2);
            String msgErro = "FALHA NO UPLOAD";
            int errX = (gfx->width() - (msgErro.length() * 12)) / 2;
            gfx->setCursor(errX, gfx->height() / 2 - 10);
            gfx->print(msgErro);
            
            rgbLed.red();
            delay(2000);
        });

        ArduinoOTA.begin();
    }

    static void handle() {
        ArduinoOTA.handle();
    }
};

int OTAManager::ultimoPercentual = -1;

#endif
