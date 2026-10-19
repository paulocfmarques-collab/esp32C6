#include "SharedSpi.h"
#include "SDUtil.h"
#include <SPI.h>
#include <time.h>
#include <esp_timer.h>

bool SDUtil::appendDiagnostic(const char* record)
{
    SharedSpi::Guard spiGuard;
    if (!mounted_ || !record) return false;
    const char* current = "/diagnostic.log";
    const char* previous = "/diagnostic.previous.log";
    FsFile file = sdCard.open(current, O_READ);
    const bool rotate = file && file.fileSize() >= 262144UL;
    file.close();
    if (rotate) {
        if (sdCard.exists(previous) && !sdCard.remove(previous)) return false;
        if (!sdCard.rename(current, previous)) return false;
    }
    file = sdCard.open(current, O_WRITE | O_CREAT | O_AT_END);
    if (!file) return false;
    const size_t length = strlen(record);
    const bool complete = file.write(record, length) == length;
    const bool synced = file.sync();
    file.close();
    return complete && synced;
}

SdMetricsSnapshot SDUtil::metricsSnapshot()
{
    portENTER_CRITICAL(&metricsMux);
    SdMetricsSnapshot copy = metrics;
    portEXIT_CRITICAL(&metricsMux);
    copy.appends = appends.load();
    copy.appendErrors = appendErrors.load();
    return copy;
}

void SDUtil::processMetrics()
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&metricsMux);
    const bool start = !metricsBusy && (!metricsAttempted || now - metricsAttemptAt >= 30000UL);
    if (start)
    {
        metricsBusy = true;
        metricsAttempted = true;
        metricsAttemptAt = now;
    }
    portEXIT_CRITICAL(&metricsMux);
    if (start && xTaskCreate(metricsTask, "sdmetrics", 4096, this, 1, nullptr) != pdPASS)
    {
        Serial.println("[METRICS] Sem memoria para atualizar estatisticas SD.");
        portENTER_CRITICAL(&metricsMux);
        metrics.valid = false;
        metricsBusy = false;
        portEXIT_CRITICAL(&metricsMux);
    }
}

void SDUtil::metricsTask(void* context)
{
    static_cast<SDUtil*>(context)->refreshMetrics();
    vTaskDelete(nullptr);
}

void SDUtil::refreshMetrics()
{
    SdMetricsSnapshot next;
    uint32_t generation;
    {
        SharedSpi::Guard spiGuard;
        generation = mountGeneration;
        next.mounted = mounted_;
        if (mounted_ && sdCard.card() && sdCard.vol())
        {
            next.capacity = uint64_t(sdCard.card()->sectorCount()) * 512ULL;
            next.total = uint64_t(sdCard.vol()->clusterCount()) *
                         sdCard.vol()->sectorsPerCluster() * 512ULL;
            for (size_t i = 0; i < 3; ++i)
            {
                const char* path = METRIC_LOG_PATHS[i];
                if (!sdCard.exists(path)) continue;
                FsFile file = sdCard.open(path, O_READ);
                if (file && !file.isDir())
                {
                    next.logBytes[i] = file.fileSize();
                    next.logValid[i] = true;
                }
                else Serial.printf("[METRICS] Falha ao consultar tamanho de %s.\n", path);
                file.close();
            }
        }
    }
    bool usedOk = false;
    if (next.mounted) next.used = getUsedBytesCooperative(&usedOk);
    next.valid = next.mounted && usedOk && next.capacity && next.total && next.used <= next.total;
    if (next.valid) next.updatedUs = esp_timer_get_time();
    else if (next.mounted) Serial.println("[METRICS] Estatisticas de espaco SD indisponiveis.");
    {
        SharedSpi::Guard spiGuard;
        if (generation != mountGeneration) next.valid = false;
        next.mounted = mounted_;
        portENTER_CRITICAL(&metricsMux);
        metrics = next;
        metricsBusy = false;
        portEXIT_CRITICAL(&metricsMux);
    }
}

bool SDUtil::salvarCrash(const String& relatorio, String& caminho, String& erro)
{
    SharedSpi::Guard spiGuard;
    caminho = "";
    erro = "";
    if (!mounted_)
    {
        erro = "SD indisponivel";
        return false;
    }
    struct tm hora;
    char data[32];
    if (getLocalTime(&hora, 0))
        strftime(data, sizeof(data), "%Y%m%d_%H%M%S", &hora);
    else
        snprintf(data, sizeof(data), "SEM_NTP_%lu", (unsigned long)millis());
    for (unsigned i = 0; i < 1000; ++i)
    {
        caminho = "/crash_" + String(data) + (i ? "_" + String(i) : String()) + ".txt";
        if (!sdCard.exists(caminho.c_str()))
            break;
        caminho = "";
    }
    if (!caminho.length())
    {
        erro = "Nomes de arquivo esgotados";
        return false;
    }
    FsFile arquivo = sdCard.open(caminho.c_str(), O_WRITE | O_CREAT | O_EXCL);
    if (!arquivo)
    {
        erro = "Falha ao criar " + caminho;
        return false;
    }
    String cabecalho = "Capturado em: " + String(data) + " | uptime=" + String(millis()) + "ms\n";
    size_t esperado = cabecalho.length() + relatorio.length();
    size_t gravado = arquivo.print(cabecalho);
    gravado += arquivo.print(relatorio);
    bool sincronizado = arquivo.sync();
    arquivo.close();
    if (!sincronizado || gravado != esperado)
    {
        erro = "Gravacao incompleta: " + caminho;
        return false;
    }
    arquivo = sdCard.open(caminho.c_str(), O_READ);
    bool ok = arquivo && arquivo.fileSize() == esperado;
    size_t posicao = 0;
    uint8_t bloco[128];
    while (ok && posicao < esperado)
    {
        size_t tamanho = min(sizeof(bloco), esperado - posicao);
        int lidos = arquivo.read(bloco, tamanho);
        if (lidos <= 0)
        {
            ok = false;
            break;
        }
        for (int i = 0; i < lidos; ++i, ++posicao)
        {
            char original = posicao < cabecalho.length() ? cabecalho[posicao]
                                                         : relatorio[posicao - cabecalho.length()];
            if (bloco[i] != uint8_t(original))
            {
                ok = false;
                break;
            }
        }
    }
    arquivo.close();
    if (!ok)
    {
        erro = "Falha ao conferir " + caminho;
        return false;
    }
    return true;
}

bool SDUtil::begin()
{
    SharedSpi::Guard spiGuard;
    ++mountGeneration;
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
    for (uint8_t i = 0; i < 10; ++i)
        SPI.transfer(0xFF);
    SPI.endTransaction();

    // 4. Inicializa o cartão usando a velocidade de 2MHz (ideal para cartões de 1GB antigos em
    // barramento compartilhado) Usamos o modo SHARED_SPI para que a biblioteca saiba que divide
    // espaço com o display Impede a SdFat de iniciar outro SPI ou aplicar pinos padrao da placa.
    SdSpiConfig configSD(SD_CS, SHARED_SPI | USER_SPI_BEGIN, SD_SCK_MHZ(2), &SPI);

    bool resultado = sdCard.begin(configSD);
    mounted_ = resultado;
    portENTER_CRITICAL(&metricsMux);
    metrics.mounted = resultado;
    metrics.valid = false;
    portEXIT_CRITICAL(&metricsMux);
    if (!resultado)
    {
        Serial.println("[SD] Falha ao inicializar: SCLK=7 MISO=5 MOSI=6 CS=4");
        sdCard.initErrorPrint(&Serial);
        if (sdCard.sdErrorCode() == SD_CARD_ERROR_CMD0)
        {
            Serial.println(
                "[SD] Cartao nao respondeu ao CMD0; falha anterior a leitura do formato.");
            Serial.println("[SD] Desligue a placa, recoloque o cartao e religue. Se persistir, "
                           "teste outro cartao.");
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
    if (!sdCard.card())
        return "Desconhecido";

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
    if (!sdCard.card())
        return 0;
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
    if (!file)
        return false;

    size_t bytesWritten = file.print(text);
    file.close();

    return bytesWritten == text.length();
}

bool SDUtil::appendText(String path, String text)
{
    SharedSpi::Guard spiGuard;
    if (!sdCard.card())
    {
        ++appendErrors;
        Serial.println("[SD] Falha ao acrescentar texto: cartao indisponivel.");
        return false;
    }

    // 1. Bloqueio Físico: Desativa eletricamente o chip da Tela (GPIO 14 em HIGH)
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);

    // Pequeno atraso em microssegundos para estabilização elétrica do barramento de pinos 6 e 7
    delayMicroseconds(100);

    // 2. Abre o arquivo garantindo exclusividade de blocos no cartão SD
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_AT_END);
    if (!file)
    {
        digitalWrite(SD_CS, HIGH); // Libera o CS do cartão em caso de erro
        ++appendErrors;
        Serial.println("[SD] Falha ao abrir arquivo para acrescentar texto.");
        return false;
    }

    size_t bytesWritten = file.print(text);

    // 3. Força a gravação física imediata nos setores do MicroSD antes de liberar o barramento
    bool synced = file.sync();
    file.close();

    // 4. Devolve o estado de repouso para o pino CS do cartão SD (Pino 4)
    digitalWrite(SD_CS, HIGH);

    const bool ok = synced && bytesWritten == text.length();
    if (ok) ++appends;
    else
    {
        ++appendErrors;
        Serial.println("[SD] Falha ao gravar/sincronizar texto acrescentado.");
    }
    return ok;
}

String SDUtil::readText(String path)
{
    SharedSpi::Guard spiGuard;
    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file)
        return "";

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
    if (!root || !root.isDir())
        return "Erro ao abrir diretorio";

    FsFile file;
    while (file.openNext(&root, O_READ))
    {
        char name[128];
        file.getName(name, sizeof(name));

        result += String(name);
        if (!file.isDir())
        {
            result += " (" + String((uint32_t)file.fileSize()) + " bytes)\n";
        }
        else
        {
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
    if (!sdCard.card())
        return false;

    // 1. Bloqueio de concorrência: Desativa o chip CS da Tela
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);

    // 2. Abre o arquivo limpando qualquer conteudo anterior de forma exclusiva
    FsFile file = sdCard.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    if (!file)
    {
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
    if (!sdCard.card() || !sdCard.exists(path.c_str()))
        return "";

    // Bloqueia a tela
    digitalWrite(14, HIGH);

    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file)
        return "";

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

uint64_t SDUtil::getUsedBytes(bool* valid)
{
    SharedSpi::Guard spiGuard;
    if (valid) *valid = false;
    if (!sdCard.card() || !sdCard.vol())
        return 0;

    uint32_t clusterCount = sdCard.vol()->clusterCount();
    int64_t freeClusters = sdCard.vol()->freeClusterCount();
    if (freeClusters < 0 || uint64_t(freeClusters) > clusterCount)
        return 0;

    uint32_t sectorsPerCluster = sdCard.vol()->sectorsPerCluster();

    uint64_t usedClusters = (uint64_t)clusterCount - freeClusters;

    if (valid) *valid = true;
    return usedClusters * sectorsPerCluster * 512ULL;
}

uint64_t SDUtil::getUsedBytesCooperative(bool* valid)
{
    if (valid) *valid = false;
    uint32_t clusters, firstFat, sectorsPerCluster, generation;
    uint8_t fatType;
    {
        SharedSpi::Guard spiGuard;
        if (!mounted_ || !sdCard.card() || !sdCard.vol())
            return 0;
        generation = mountGeneration;
        fatType = sdCard.vol()->fatType();
        // A contagem por setores abaixo e exclusiva de FAT16/FAT32.
        if (fatType != 16 && fatType != 32)
            return getUsedBytes(valid);
        clusters = sdCard.vol()->clusterCount();
        firstFat = sdCard.vol()->fatStartSector();
        sectorsPerCluster = sdCard.vol()->sectorsPerCluster();
    }
    const uint16_t entryBytes = fatType == 16 ? 2 : 4;
    const uint16_t entriesPerSector = 512 / entryBytes;
    uint8_t sector[512];
    uint32_t freeClusters = 0;
    // Pula as entradas reservadas 0 e 1; cada leitura libera o SPI para o LCD.
    for (uint32_t cluster = 2; cluster < clusters + 2;)
    {
        const uint32_t sectorIndex = cluster / entriesPerSector;
        {
            SharedSpi::Guard spiGuard;
            if (generation != mountGeneration || !mounted_ || !sdCard.card())
            {
                Serial.println("[SD] Consulta de espaco cancelada: montagem alterada.");
                return 0;
            }
            if (!sdCard.card()->readSector(firstFat + sectorIndex, sector))
            {
                Serial.println("[SD] Falha ao consultar espaco usado.");
                return 0;
            }
        }
        while (cluster < clusters + 2 && cluster / entriesPerSector == sectorIndex)
        {
            const uint16_t offset = (cluster % entriesPerSector) * entryBytes;
            uint32_t value = uint32_t(sector[offset]) | (uint32_t(sector[offset + 1]) << 8);
            if (entryBytes == 4)
            {
                value |=
                    (uint32_t(sector[offset + 2]) << 16) | (uint32_t(sector[offset + 3]) << 24);
                value &= 0x0FFFFFFF;
            }
            if (value == 0)
                ++freeClusters;
            ++cluster;
        }
        vTaskDelay(1);
    }
    if (valid) *valid = true;
    return uint64_t(clusters - freeClusters) * sectorsPerCluster * 512ULL;
}

int SDUtil::readLogTail(const char* path, String& content, bool& truncated)
{
    SharedSpi::Guard spiGuard;
    content = "";
    truncated = false;
    if (!mounted_)
    {
        content = "Cartao SD indisponivel.";
        return 503;
    }
    if (!sdCard.exists(path))
    {
        content = "Este log ainda nao foi criado.";
        return 404;
    }
    FsFile file = sdCard.open(path, O_READ);
    if (!file || file.isDir())
    {
        content = "Nao foi possivel abrir o log.";
        return 500;
    }
    const size_t limit = 8192;
    uint64_t size = file.fileSize();
    truncated = size > limit;
    if (truncated && !file.seekSet(size - limit))
    {
        file.close();
        content = "Falha ao posicionar a leitura do log.";
        return 500;
    }
    char buffer[256];
    if (!content.reserve(limit))
    {
        file.close();
        content = "Memoria insuficiente para consultar o log.";
        return 500;
    }
    size_t remaining = size < limit ? size_t(size) : limit;
    while (remaining)
    {
        size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        int n = file.read(buffer, count);
        if (n <= 0)
        {
            file.close();
            content = "Falha ao ler o log do cartao.";
            return 500;
        }
        if (!content.concat(buffer, size_t(n)))
        {
            file.close();
            content = "Memoria insuficiente para consultar o log.";
            return 500;
        }
        remaining -= size_t(n);
    }
    file.close();
    // A janela pode iniciar no meio de uma linha; mostra somente linhas completas.
    if (truncated)
    {
        int newline = content.indexOf('\n');
        if (newline >= 0)
            content.remove(0, newline + 1);
    }
    return 200;
}

String SDUtil::readTail(String path, size_t maxBytes)
{
    SharedSpi::Guard spiGuard;
    FsFile file = sdCard.open(path.c_str(), O_READ);
    if (!file)
        return "";

    uint64_t size = file.fileSize();
    if (size > maxBytes)
        file.seekSet(size - maxBytes);

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

bool SDUtil::renameFile(const String& from, const String& to)
{
    SharedSpi::Guard spiGuard;
    return sdCard.rename(from.c_str(), to.c_str());
}
