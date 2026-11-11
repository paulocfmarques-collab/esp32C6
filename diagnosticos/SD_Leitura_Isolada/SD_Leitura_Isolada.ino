#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

// Waveshare ESP32-C6-LCD-1.47 SEM TOUCH. Teste somente de leitura.
constexpr int SCLK_PIN = 7, MISO_PIN = 5, MOSI_PIN = 6, SD_CS = 4;
constexpr int LCD_CS = 14, LCD_BL = 22;
SdFs card;
uint8_t sector[512];

uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
         uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

uint32_t fingerprint() {
  uint32_t value = 2166136261UL;
  for (uint8_t b : sector) value = (value ^ b) * 16777619UL;
  return value;
}

void failure(const char* step) {
  Serial.printf("[FALHA] etapa=%s SdError=0x%02X,0x%02X\n", step,
    unsigned(card.sdErrorCode()), unsigned(card.sdErrorData()));
}

uint8_t commandCrc(const uint8_t* bytes) {
  uint8_t crc = 0;
  for (uint8_t n = 0; n < 5; ++n) {
    uint8_t value = bytes[n];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc <<= 1;
      if ((value & 0x80) != (crc & 0x80)) crc ^= 0x09;
      value <<= 1;
    }
  }
  return uint8_t((crc << 1) | 1);
}

// CMD17 direto, somente leitura. Compara argumentos sem alterar tipo na SdFat.
bool rawRead(uint32_t argument, uint32_t hz, const char* mode) {
  digitalWrite(LCD_CS, HIGH);
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, LOW);
  uint32_t at = millis();
  uint8_t ready;
  do { ready = SPI.transfer(0xFF); } while (ready != 0xFF && millis() - at < 300);
  uint8_t response = 0xFF, token = 0xFF;
  bool ok = false;
  uint16_t received = 0, calculated = 0;
  if (ready == 0xFF) {
    uint8_t command[6] = {0x51, uint8_t(argument >> 24), uint8_t(argument >> 16),
                         uint8_t(argument >> 8), uint8_t(argument), 0};
    command[5] = commandCrc(command);
    for (uint8_t b : command) SPI.transfer(b);
    for (uint8_t n = 0; n < 32; ++n) {
      response = SPI.transfer(0xFF);
      if (!(response & 0x80)) break;
    }
    if (response == 0) {
      at = millis();
      do { token = SPI.transfer(0xFF); } while (token == 0xFF && millis() - at < 300);
      if (token == 0xFE) {
        for (uint16_t n = 0; n < 512; ++n) {
          sector[n] = SPI.transfer(0xFF);
          calculated ^= uint16_t(sector[n]) << 8;
          for (uint8_t bit = 0; bit < 8; ++bit)
            calculated = (calculated & 0x8000) ? uint16_t((calculated << 1) ^ 0x1021) : uint16_t(calculated << 1);
        }
        received = uint16_t(SPI.transfer(0xFF)) << 8;
        received |= SPI.transfer(0xFF);
        ok = received == calculated;
      }
    }
  }
  digitalWrite(SD_CS, HIGH);
  SPI.transfer(0xFF);
  SPI.endTransaction();
  Serial.printf("[RAW] %s CMD17 arg=0x%08lX ready=%02X R1=%02X token=%02X CRC=%04X/%04X %s\n",
    mode, (unsigned long)argument, unsigned(ready), unsigned(response), unsigned(token),
    unsigned(received), unsigned(calculated), ok ? "PASS" : "FAIL");
  if (token == 0xFE) {
    const bool fatBoot = sector[510] == 0x55 && sector[511] == 0xAA && sector[11] == 0 && sector[12] == 2;
    Serial.printf("[RAW] hash=%08lX assinatura=%02X%02X BPB_512=%s\n", (unsigned long)fingerprint(),
      unsigned(sector[511]), unsigned(sector[510]), fatBoot ? "SIM" : "NAO");
  }
  delay(20);
  return ok;
}

bool repeatedRead(uint32_t lba, const char* label) {
  uint32_t first = 0;
  for (uint8_t i = 0; i < 5; ++i) {
    if (!card.card()->readSector(lba, sector)) {
      Serial.printf("LBA=%lu repeticao=%u\n", (unsigned long)lba, unsigned(i + 1));
      failure(label);
      return false;
    }
    const uint32_t value = fingerprint();
    Serial.printf("[LEITURA] %s LBA=%lu repeticao=%u hash=%08lX\n",
      label, (unsigned long)lba, unsigned(i + 1), (unsigned long)value);
    if (i && value != first) {
      Serial.println("[FALHA] Conteudo mudou entre leituras do mesmo setor.");
      return false;
    }
    first = value;
    delay(20);
  }
  return true;
}

bool run(uint32_t hz) {
  Serial.printf("\n--- TESTE: dados ate %lu Hz ---\n", (unsigned long)hz);
  card.end(); // Nao ha arquivos abertos para escrita neste sketch.
  // card.end() tambem chama SPI.end(), mesmo com USER_SPI_BEGIN.
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delay(250);
  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  for (uint8_t i = 0; i < 16; ++i) SPI.transfer(0xFF);
  SPI.endTransaction();
  if (!card.cardBegin(SdSpiConfig(SD_CS, SHARED_SPI | USER_SPI_BEGIN, hz, &SPI))) {
    failure("CARD_BEGIN"); return false;
  }
  Serial.println("[OK] CARD_BEGIN");
  csd_t csd;
  if (!card.card()->readCSD(&csd)) { failure("READ_CSD"); return false; }
  const uint32_t capacity = csd.capacity();
  const uint8_t* csdBytes = reinterpret_cast<const uint8_t*>(&csd);
  Serial.printf("[REG] CSD_STRUCTURE=%u CSD=", unsigned(csdBytes[0] >> 6));
  for (uint8_t n = 0; n < 16; ++n) Serial.printf("%02X", unsigned(csdBytes[n]));
  uint32_t ocr = 0;
  if (card.card()->readOCR(&ocr)) Serial.printf(" OCR=%08lX CCS=%u\n", (unsigned long)ocr, unsigned((ocr >> 30) & 1));
  else { Serial.println(" OCR=FALHA"); failure("READ_OCR"); }
  Serial.printf("[OK] CSD tipo=%u capacidade=%lu setores (%llu MiB)\n",
    unsigned(card.card()->type()), (unsigned long)capacity,
    uint64_t(capacity) * 512 / 1048576);
  if (!capacity) { Serial.println("[FALHA] Capacidade invalida."); return false; }
  if (!repeatedRead(0, "SETOR_0")) return false;

  if (sector[510] != 0x55 || sector[511] != 0xAA) {
    Serial.println("[FALHA] Setor 0 sem assinatura 55 AA."); return false;
  }
  // Escolhe apenas um inicio de volume dentro da capacidade fisica.
  const bool directFat = (sector[0] == 0xEB || sector[0] == 0xE9) &&
    sector[11] == 0 && sector[12] == 2 && (sector[16] == 1 || sector[16] == 2);
  uint32_t start = 0;
  bool found = directFat;
  if (directFat) Serial.println("[LAYOUT] FAT direto no setor 0.");
  else {
    for (uint8_t i = 0; i < 4; ++i) {
      const uint8_t* p = sector + 446 + i * 16;
      const uint32_t first = le32(p + 8), count = le32(p + 12);
      Serial.printf("[MBR] p%u tipo=0x%02X inicio=%lu setores=%lu\n",
        unsigned(i + 1), unsigned(p[4]), (unsigned long)first, (unsigned long)count);
      const bool supported = p[4] == 0x01 || p[4] == 0x04 || p[4] == 0x06 ||
        p[4] == 0x0B || p[4] == 0x0C || p[4] == 0x0E || p[4] == 0x07;
      if (!found && supported && (p[0] == 0 || p[0] == 0x80) &&
          first && first < capacity && count && count <= capacity - first) {
        start = first; found = true;
      }
    }
  }
  if (!found) { Serial.println("[FALHA] Nenhum volume candidato valido."); return false; }
  if (!repeatedRead(start, "BOOT_VOLUME")) {
    Serial.println("[RAW] Investigando enderecamento; apenas CMD17, sem escrita.");
    rawRead(start, hz, "ARG_SETORES");
    if (start && uint64_t(start) * 512 <= 0xFFFFFFFFULL)
      rawRead(uint32_t(uint64_t(start) * 512), hz, "ARG_BYTES");
    return false; // Leitura RAW nao converte falha da SdFat em PASS de montagem.
  }
  Serial.printf("[BPB] bytes/setor=%u setores/cluster=%u FATs=%u\n",
    unsigned(sector[11] | uint16_t(sector[12]) << 8), unsigned(sector[13]), unsigned(sector[16]));
  if (!card.vol()->begin(card.card(), true, 0, start)) {
    failure("MONTAGEM"); return false;
  }
  Serial.printf("[OK] MONTAGEM FAT/exFAT=%u\n", unsigned(card.vol()->fatType()));
  FsFile root = card.open("/", O_RDONLY);
  if (!root) { failure("ABRIR_RAIZ"); return false; }
  Serial.println("[ARQUIVOS] Ate 10 entradas, apenas nomes e tamanhos:");
  for (uint8_t i = 0; i < 10; ++i) {
    FsFile file;
    if (!file.openNext(&root, O_RDONLY)) break;
    char name[64]{};
    file.getName(name, sizeof(name));
    Serial.printf("  %s%s %llu bytes\n", name, file.isDir() ? "/" : "",
      uint64_t(file.fileSize()));
    file.close();
    delay(1);
  }
  const bool directoryError = root.getError();
  root.close();
  if (directoryError) { failure("LISTAR_RAIZ"); return false; }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== SD LEITURA ISOLADA v3 ===");
  Serial.printf("Build %s %s | SdFat %s\n", __DATE__, __TIME__, SD_FAT_VERSION_STR);
  Serial.println("Sem Wi-Fi, OTA, display, microfone ou DHT22 ativos.");
  Serial.println("SCLK=7 MISO=5 MOSI=6 CS=4 LCD_CS=14");
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, LOW);
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
  const uint32_t rates[] = {400000UL, 2000000UL, 100000UL};
  uint8_t passed = 0;
  for (uint32_t rate : rates) {
    const bool ok = run(rate);
    if (ok) ++passed;
    Serial.printf("[RESULTADO] %lu Hz: %s\n", (unsigned long)rate, ok ? "PASS" : "FAIL");
    digitalWrite(SD_CS, HIGH);
    digitalWrite(LCD_CS, HIGH);
    delay(500);
  }
  Serial.printf("\n=== FIM: %u/3 velocidades passaram ===\n", unsigned(passed));
  Serial.println("Nenhum arquivo foi gravado, apagado ou formatado.");
  Serial.println("Envie o log inteiro. Para repetir, aperte RESET.");
}

void loop() { delay(100); }
