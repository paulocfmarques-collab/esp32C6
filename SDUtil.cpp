#include "SDUtil.h"

#include "SDUtil.h"
#include <SPI.h>
#include <SD.h> // Usando a SD nativa temporariamente para teste de compatibilidade

#include "SDUtil.h"
#include <SPI.h>

bool SDUtil::begin()
{
    // 1. Configura e força o pino CS da tela (14) a ficar em HIGH (Desativada)
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH); 
    
    // 2. Configura e força o pino CS do cartão SD (4) a ficar em HIGH (Desativado)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(50);

    // 3. Reinicializa a configuração SPI nativa do ESP32 para remapear os registradores compartilhados
    // Ordem correta dos pinos do ESP32-C6 nesta placa: SCLK=7, MISO=5, MOSI=6
    SPI.end(); // Libera qualquer travamento prévio do barramento
    SPI.begin(7, 5, 6, SD_CS);
    delay(50);

    // 4. Inicializa o cartão usando a velocidade de 2MHz (ideal para cartões de 1GB antigos em barramento compartilhado)
    // Usamos o modo SHARED_SPI para que a biblioteca saiba que divide espaço com o display
    SdSpiConfig configSD(SD_CS, SHARED_SPI, SD_SCK_MHZ(2));
    
    bool resultado = sdCard.begin(configSD);
    
    // 5. Após tentar iniciar o SD, devolve o controle do pino CS do cartão para HIGH
    digitalWrite(SD_CS, HIGH);
    
    return resultado;
}


bool SDUtil::test()
{
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
    if (!sdCard.card()) return 0;
    // Calcula o tamanho total multiplicando o número de blocos pelo tamanho do bloco (512 bytes)
    return (uint64_t)sdCard.card()->sectorCount() * 512ULL;
}

bool SDUtil::exists(String path)
{
    return sdCard.exists(path.c_str());
}

bool SDUtil::writeText(String path, String text)
{
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    if (!file) return false;

    size_t bytesWritten = file.print(text);
    file.close();

    return bytesWritten == text.length();
}

bool SDUtil::appendText(String path, String text)
{
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
    return sdCard.remove(path.c_str());
}

bool SDUtil::createDir(String path)
{
    return sdCard.mkdir(path.c_str());
}

String SDUtil::listFiles(String path)
{
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
