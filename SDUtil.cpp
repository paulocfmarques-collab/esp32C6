#include "SharedSpi.h"
#include "SDUtil.h"
#include <SPI.h>

bool SDUtil::begin()
{
    SharedSpi::Guard spiGuard;
    // 1. Configura e força o pino CS da tela (14) a ficar em HIGH (Desativada)
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH); 
    
    // 2. Configura e força o pino CS do cartão SD (4) a ficar em HIGH (Desativado)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(250); // Aguarda a alimentacao do cartao estabilizar no boot.

    // O SD inicia antes do display e define os pinos do SPI compartilhado.
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    // Clocks de partida com ambos os CS altos e MOSI em HIGH.
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (uint8_t i = 0; i < 10; ++i) SPI.transfer(0xFF);
    SPI.endTransaction();

    // 4. Inicializa o cartão usando a velocidade de 2MHz (ideal para cartões de 1GB antigos em barramento compartilhado)
    // Usamos o modo SHARED_SPI para que a biblioteca saiba que divide espaço com o display
    // Impede a SdFat de iniciar outro SPI ou aplicar pinos padrao da placa.
    SdSpiConfig configSD(SD_CS, SHARED_SPI | USER_SPI_BEGIN, SD_SCK_MHZ(2), &SPI);
    
    bool resultado = sdCard.begin(configSD);
    if (!resultado)
    {
        Serial.println("[SD] Falha ao inicializar: SCLK=7 MISO=5 MOSI=6 CS=4");
        sdCard.initErrorPrint(&Serial);
        if (sdCard.sdErrorCode() == SD_CARD_ERROR_CMD0)
        {
            Serial.println("[SD] Cartao nao respondeu ao CMD0; falha anterior a leitura do formato.");
            Serial.println("[SD] Desligue a placa, recoloque o cartao e religue. Se persistir, teste outro cartao.");
        }
    }
    
    // 5. Após tentar iniciar o SD, devolve o controle do pino CS do cartão para HIGH
    digitalWrite(SD_CS, HIGH);
    
    return resultado;
}


bool SDUtil::test()
{
    SharedSpi::Guard spiGuard;
    String path = "/sd_test_" + String(millis()) + ".tmp";
    while (exists(path))
    {
        path = "/sd_test_" + String(millis()) + "_" + String(random(0x7fffffff)) + ".tmp";
    }

    const String expected = "ESP32-C6 SD test";
    if (!writeText(path, expected))
    {
        removeFile(path);
        return false;
    }

    bool readSucceeded = readText(path) == expected;
    bool removeSucceeded = removeFile(path);
    return readSucceeded && removeSucceeded;
}

String SDUtil::getCardType()
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card()) return "Desconhecido";
    
    switch (sdCard.card()->type())
    {
        case SD_CARD_TYPE_SD1:
            return "SDSC (SD1)";
        case SD_CARD_TYPE_SD2:
            return "SDSC (SD2)";
        case SD_CARD_TYPE_SDHC:
            return "SDHC/SDXC";
        default:
            return "Desconhecido";
    }
}

uint64_t SDUtil::getCardSizeBytes()
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card()) return 0;
    // Calcula o tamanho total multiplicando o número de blocos pelo tamanho do bloco (512 bytes)
    return (uint64_t)sdCard.card()->sectorCount() * 512ULL;
}

bool SDUtil::exists(String path)
{
    SharedSpi::Guard spiGuard;
    return sdCard.exists(path.c_str());
}

bool SDUtil::writeText(String path, String text)
{
    SharedSpi::Guard spiGuard;
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    if (!file) return false;

    size_t bytesWritten = file.print(text);
    file.close();

    return bytesWritten == text.length();
}

bool SDUtil::appendText(String path, String text)
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card()) return false;

    // 1. Bloqueio Físico: Desativa eletricamente o chip da Tela (GPIO 14 em HIGH)
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
    
    // Pequeno atraso em microssegundos para estabilização elétrica do barramento de pinos 6 e 7
    delayMicroseconds(100); 

    // 2. Abre o arquivo garantindo exclusividade de blocos no cartão SD
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_AT_END);
    if (!file) {
        digitalWrite(SD_CS, HIGH); // Libera o CS do cartão em caso de erro
        return false;
    }

    size_t bytesWritten = file.print(text);
    
    // 3. Força a gravação física imediata nos setores do MicroSD antes de liberar o barramento
    file.sync(); 
    file.close();

    // 4. Devolve o estado de repouso para o pino CS do cartão SD (Pino 4)
    digitalWrite(SD_CS, HIGH);

    return bytesWritten == text.length();
}

String SDUtil::readText(String path)
{
    SharedSpi::Guard spiGuard;
    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file) return "";

    String content;
    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();

    return content;
}

bool SDUtil::removeFile(String path)
{
    SharedSpi::Guard spiGuard;
    return sdCard.remove(path.c_str());
}

bool SDUtil::createDir(String path)
{
    SharedSpi::Guard spiGuard;
    return sdCard.mkdir(path.c_str());
}

String SDUtil::listFiles(String path)
{
    SharedSpi::Guard spiGuard;
    String result;
    FsFile root = sdCard.open(path.c_str());
    if (!root || !root.isDir()) return "Erro ao abrir diretorio";

    FsFile file;
    while (file.openNext(&root, O_READ))
    {
        char name[128];
        file.getName(name, sizeof(name));
        
        result += String(name);
        if (!file.isDir()) {
            result += " (" + String((uint32_t)file.fileSize()) + " bytes)\n";
        } else {
            result += " <DIR>\n";
        }
        file.close();
    }
    root.close();
    return result;
}

bool SDUtil::writeJson(String path, String jsonContent)
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card()) return false;

    // 1. Bloqueio de concorrência: Desativa o chip CS da Tela
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
    
    // 2. Abre o arquivo limpando qualquer conteudo anterior de forma exclusiva
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    if (!file) {
        Serial.println("[SD ERRO] Falha ao abrir arquivo para escrita JSON.");
        return false;
    }

    // 3. Força a escrita completa da string em uma única operação de bloco
    size_t bytesEscritos = file.print(jsonContent);
    
    // 4. Sincroniza fisicamente os setores do cartao antes de fechar
    file.sync(); 
    file.close();

    // 5. Devolve o estado de repouso para o pino CS do cartão SD
    digitalWrite(SD_CS, HIGH);

    return bytesEscritos == jsonContent.length();
}

String SDUtil::readJson(String path)
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card() || !sdCard.exists(path.c_str())) return "";

    // Bloqueia a tela
    digitalWrite(14, HIGH);

    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file) return "";

    String content;
    // Pré-aloca espaço na memória RAM para evitar fragmentação de Heap no ESP32-C6
    content.reserve(file.fileSize()); 

    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();
    
    digitalWrite(SD_CS, HIGH);
    return content;
}

uint64_t SDUtil::getUsedBytes()
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card())
        return 0;

    uint32_t clusterCount = sdCard.vol()->clusterCount();
    int64_t freeClusters = sdCard.vol()->freeClusterCount();
    if(freeClusters<0 || uint64_t(freeClusters)>clusterCount)return 0;

    uint32_t sectorsPerCluster =
        sdCard.vol()->sectorsPerCluster();

    uint64_t usedClusters =
        (uint64_t)clusterCount - freeClusters;

    return usedClusters *
           sectorsPerCluster *
           512ULL;
}

uint64_t SDUtil::getUsedBytesCooperative()
{
    uint32_t clusters, firstFat, sectorsPerCluster;
    uint8_t fatType;
    {
        SharedSpi::Guard spiGuard;
        if (!sdCard.card() || !sdCard.vol()) return 0;
        fatType = sdCard.vol()->fatType();
        // A contagem por setores abaixo e exclusiva de FAT16/FAT32.
        if (fatType != 16 && fatType != 32) return getUsedBytes();
        clusters = sdCard.vol()->clusterCount();
        firstFat = sdCard.vol()->fatStartSector();
        sectorsPerCluster = sdCard.vol()->sectorsPerCluster();
    }
    const uint16_t entryBytes = fatType == 16 ? 2 : 4;
    const uint16_t entriesPerSector = 512 / entryBytes;
    uint8_t sector[512];
    uint32_t freeClusters = 0;
    // Pula as entradas reservadas 0 e 1; cada leitura libera o SPI para o LCD.
    for (uint32_t cluster = 2; cluster < clusters + 2; )
    {
        const uint32_t sectorIndex = cluster / entriesPerSector;
        {
            SharedSpi::Guard spiGuard;
            if (!sdCard.card()->readSector(firstFat + sectorIndex, sector)) {
                Serial.println("[SD] Falha ao consultar espaco usado.");
                return 0;
            }
        }
        while (cluster < clusters + 2 && cluster / entriesPerSector == sectorIndex) {
            const uint16_t offset = (cluster % entriesPerSector) * entryBytes;
            uint32_t value = uint32_t(sector[offset]) | (uint32_t(sector[offset + 1]) << 8);
            if (entryBytes == 4) {
                value |= (uint32_t(sector[offset + 2]) << 16) | (uint32_t(sector[offset + 3]) << 24);
                value &= 0x0FFFFFFF;
            }
            if (value == 0) ++freeClusters;
            ++cluster;
        }
        vTaskDelay(1);
    }
    return uint64_t(clusters - freeClusters) * sectorsPerCluster * 512ULL;
}

String SDUtil::readTail(String path, size_t maxBytes)
{
    SharedSpi::Guard spiGuard;
    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file) return "";

    uint64_t size = file.fileSize();
    if (size > maxBytes) file.seekSet(size - maxBytes);

    String content;
    content.reserve(maxBytes);
    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();
    return content;
}

bool SDUtil::removeDir(String path)
{
    SharedSpi::Guard spiGuard;
    return sdCard.rmdir(path.c_str());
}

uint64_t SDUtil::getFileSize(String path)
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card())
        return 0;

    FsFile file = sdCard.open(path.c_str(), O_READ);

    if (!file)
        return 0;

    uint64_t tamanho = file.fileSize();

    file.close();

    return tamanho;
}
bool SDUtil::renameFile(const String& from,const String& to){SharedSpi::Guard spiGuard;return sdCard.rename(from.c_str(),to.c_str());}
