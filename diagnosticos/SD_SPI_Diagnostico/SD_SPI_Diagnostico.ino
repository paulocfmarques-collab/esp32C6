#include <Arduino.h>
#include <SPI.h>

// ESP32-C6-LCD-1.47 sem Touch. Confirme o modelo antes de mudar os pinos.
constexpr int SD_SCLK = 7;
constexpr int SD_MISO = 5;
constexpr int SD_MOSI = 6;
constexpr int SD_CS = 4;
constexpr int LCD_CS = 14;

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== SD SPI isolado: sem display, SdFat ou escrita ===");
  Serial.printf("SCLK=%d MISO=%d MOSI=%d CS=%d\n", SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(500);
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  for (int i = 0; i < 16; ++i) SPI.transfer(0xFF);

  bool respondeu = false;
  for (int tentativa = 1; tentativa <= 10; ++tentativa)
  {
    digitalWrite(SD_CS, LOW);
    SPI.transfer(0xFF);
    // CMD0, argumento zero, CRC valido. Apenas reinicia o protocolo do cartao.
    const uint8_t cmd0[] = {0x40, 0, 0, 0, 0, 0x95};
    for (uint8_t b : cmd0) SPI.transfer(b);
    uint8_t resposta = 0xFF;
    for (int i = 0; i < 32; ++i)
    {
      resposta = SPI.transfer(0xFF);
      if ((resposta & 0x80) == 0) break;
    }
    digitalWrite(SD_CS, HIGH);
    SPI.transfer(0xFF);
    Serial.printf("CMD0 tentativa %d: 0x%02X\n", tentativa, resposta);
    if (resposta == 0x01)
    {
      respondeu = true;
      break;
    }
    delay(100);
  }
  SPI.endTransaction();
  Serial.println(respondeu
    ? "OK: cartao respondeu diretamente ao SPI. Investigar integracao/SdFat."
    : "FALHA: sem resposta idle ao CMD0. Conferir modelo/pinos, cartao, slot e alimentacao.");
  Serial.println("Teste concluido. Nenhum arquivo foi lido, gravado ou formatado.");
}

void loop() {}
