#include "SharedSpi.h"
#include "SDUtil.h"
#include <SPI.h>
#include <time.h>
#include <esp_timer.h>
#include "FatLib/FatFormatter.h"

bool SDUtil::appendDiagnostic(const char* record)
{
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    if (!mounted_ || !record) return recordLogWrite(false);
    const char* current = "/diagnostic.log";
    const char* previous = "/diagnostic.previous.log";
    FsFile file = sdCard.open(current, O_READ);
    const bool rotate = file && file.fileSize() >= 262144UL;
    file.close();
    if (rotate) {
        if (sdCard.exists(previous) && !sdCard.remove(previous)) return recordLogWrite(false);
        if (!sdCard.rename(current, previous)) return recordLogWrite(false);
    }
    file = sdCard.open(current, O_WRITE | O_CREAT | O_AT_END);
    if (!file) return recordLogWrite(false);
    const size_t length = strlen(record);
    const bool complete = file.write(record, length) == length;
    const bool synced = file.sync();
    file.close();
    return recordLogWrite(complete && synced);
}

SdMetricsSnapshot SDUtil::metricsSnapshot()
{
    if (!enabled()) { return {}; }
    portENTER_CRITICAL(&metricsMux);
    SdMetricsSnapshot copy = metrics;
    portEXIT_CRITICAL(&metricsMux);
    copy.appends = appends.load();
    copy.appendErrors = appendErrors.load();
    copy.logRecords = logRecords.load();
    copy.logErrors = logErrors.load();
    return copy;
}

void SDUtil::processMetrics()
{
    if (!enabled()) { return; }
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
    if (!enabled()) { return; }
    SdMetricsSnapshot next;
    uint32_t generation;
    {
        SharedSpi::Guard spiGuard;
        generation = mountGeneration;
        next.mounted = mounted_;
        next.spiHz = spiHz_;
        if (mounted_ && sdCard.card() && sdCard.vol())
        {
            next.capacity = capacityBytes_;
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
    if (!enabled()) { caminho = ""; erro = "SD desativado neste dispositivo"; return false; }
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

namespace {
uint16_t sdLe16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
uint32_t sdLe32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
        (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool sdLooksLikeFatBoot(const uint8_t* p, uint32_t capacity) {
    const uint32_t total = sdLe16(p + 19) ? sdLe16(p + 19) : sdLe32(p + 32);
    return p[510] == 0x55 && p[511] == 0xAA &&
        (p[0] == 0xEB || p[0] == 0xE9) && sdLe16(p + 11) == 512 &&
        p[13] && p[13] <= 128 && !(p[13] & (p[13] - 1)) &&
        sdLe16(p + 14) && (p[16] == 1 || p[16] == 2) &&
        total && total <= capacity && (sdLe16(p + 22) || sdLe32(p + 36));
}
template<typename Card>
bool sdReadStartupSector(Card* card, uint32_t lba, uint8_t* sector, const char* label) {
    uint32_t expected = 0;
    for (uint8_t i = 0; i < 5; ++i) {
        if (!card->readSector(lba, sector)) {
            Serial.printf("[SD] %s LBA=%lu repeticao=%u: falha de leitura\n",
                label, (unsigned long)lba, unsigned(i + 1));
            return false;
        }
        uint32_t hash = 2166136261UL;
        for (uint16_t n = 0; n < 512; ++n) hash = (hash ^ sector[n]) * 16777619UL;
        Serial.printf("[SD] %s LBA=%lu repeticao=%u hash=%08lX\n",
            label, (unsigned long)lba, unsigned(i + 1), (unsigned long)hash);
        if (i && hash != expected) {
            Serial.println("[SD] Conteudo mudou entre leituras sem escrita; montagem cancelada.");
            return false;
        }
        expected = hash;
        delay(20); // Mesmo espacamento do diagnostico isolado que passou.
    }
    return true;
}
} // namespace

bool SDUtil::begin()
{
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    ++mountGeneration;
    // Replica a espera inicial do diagnostico que passou. Nao repete no uso normal.
    const uint32_t bootMs = millis();
    if (bootMs < 3000UL) delay(3000UL - bootMs);
    // 1. Configura e força o pino CS da tela (14) a ficar em HIGH (Desativada)
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);

    // 2. Configura e força o pino CS do cartão SD (4) a ficar em HIGH (Desativado)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(250); // Aguarda a alimentacao do cartao estabilizar no boot.

    // O SD inicia antes do display e define os pinos do SPI compartilhado.
    sdCard.end();
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    Serial.println("[SD] Perfil isolado: 5 leituras por setor, intervalo 20 ms.");

    bool resultado = false;
    uint32_t selectedHz = 0;
    uint64_t selectedCapacity = 0;
    // Tentativas limitadas, sem modificar arquivos ou formatar o cartao.
    // Separa reset, registradores, leitura fisica e montagem para localizar a falha.
    const uint32_t rates[] = {400000UL, 100000UL, 2000000UL};
    uint8_t sector[512];
    for (uint8_t attempt = 0; attempt < 3 && !resultado; ++attempt)
    {
        digitalWrite(14, HIGH);
        digitalWrite(SD_CS, HIGH);
        delay(250);
        SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
        for (uint8_t i = 0; i < 16; ++i) SPI.transfer(0xFF);
        SPI.endTransaction();
        Serial.printf("[SD] Tentativa %u/3: dados ate %lu Hz\n", unsigned(attempt + 1), (unsigned long)rates[attempt]);
        SdSpiConfig config(SD_CS, SHARED_SPI | USER_SPI_BEGIN, rates[attempt], &SPI);
        uint32_t capacity = 0;
        initStage_ = "CARD_BEGIN";
        bool ok = sdCard.cardBegin(config);
        if (ok) {
            Serial.println("[SD] CARD_BEGIN OK; lendo CSD...");
            initStage_ = "READ_CSD";
            csd_t csd;
            ok = sdCard.card()->readCSD(&csd);
            if (ok) capacity = csd.capacity();
        }
        if (ok) {
            Serial.println("[SD] CSD OK; lendo setor 0...");
            initStage_ = "READ_SECTOR_0";
            ok = sdReadStartupSector(sdCard.card(), 0, sector, "SETOR_0");
        }
        if (ok) {
            Serial.printf("[SD] Setor 0 OK; capacidade=%lu setores assinatura=%02X%02X\n",
                (unsigned long)capacity, unsigned(sector[511]), unsigned(sector[510]));
            bool explicitStart = sdLooksLikeFatBoot(sector, capacity);
            uint32_t start = 0;
            if (explicitStart) Serial.println("[SD] FAT direto no setor 0 (sem tabela MBR).");
            else if (sector[510] == 0x55 && sector[511] == 0xAA) {
                // Valida a tabela antes de seguir enderecos de particao.
                for (uint8_t part = 0; part < 4; ++part) {
                    const uint8_t* entry = sector + 446 + part * 16;
                    const uint32_t first = sdLe32(entry + 8), count = sdLe32(entry + 12);
                    Serial.printf("[SD] MBR p%u tipo=0x%02X inicio=%lu setores=%lu\n",
                        unsigned(part + 1), unsigned(entry[4]), (unsigned long)first, (unsigned long)count);
                    const bool supported = entry[4] == 0x01 || entry[4] == 0x04 || entry[4] == 0x06 ||
                        entry[4] == 0x0B || entry[4] == 0x0C || entry[4] == 0x0E || entry[4] == 0x07;
                    if (!explicitStart && supported && (entry[0] == 0 || entry[0] == 0x80) &&
                        first && first < capacity && count && count <= capacity - first) {
                        explicitStart = true;
                        start = first;
                    }
                }
                if (explicitStart) {
                    initStage_ = "READ_VOLUME_BOOT";
                    Serial.printf("[SD] Lendo boot do volume no setor %lu...\n", (unsigned long)start);
                    ok = sdReadStartupSector(sdCard.card(), start, sector, "BOOT_VOLUME");
                }
            }
            if (ok) {
                if (explicitStart) {
                    Serial.printf("[SD] BPB: bytes/setor=%u setores/cluster=%u FATs=%u reservados=%u\n",
                        unsigned(sdLe16(sector + 11)), unsigned(sector[13]), unsigned(sector[16]), unsigned(sdLe16(sector + 14)));
                    initStage_ = "VOLUME_BEGIN_EXPLICIT";
                    ok = sdCard.vol()->begin(sdCard.card(), true, 0, start);
                } else {
                    Serial.println("[SD] Layout nao identificado; usando montagem automatica.");
                    initStage_ = "VOLUME_BEGIN";
                    ok = sdCard.volumeBegin();
                }
            }
        }
        initError_ = sdCard.sdErrorCode();
        initErrorData_ = sdCard.sdErrorData();
        digitalWrite(SD_CS, HIGH);
        if (ok) {
            resultado = true;
            selectedHz = rates[attempt];
            selectedCapacity = uint64_t(capacity) * 512ULL;
            initStage_ = "READY";
            Serial.printf("[SD] Volume montado; SPI ate %lu Hz.\n", (unsigned long)selectedHz);
        } else Serial.printf("[SD] Falha etapa=%s SdError=0x%02X,0x%02X\n",
            initStage_, unsigned(initError_), unsigned(initErrorData_));
    }
    mounted_ = resultado;
    spiHz_ = selectedHz;
    capacityBytes_ = selectedCapacity;
    portENTER_CRITICAL(&metricsMux);
    metrics.mounted = resultado;
    metrics.valid = false;
    metrics.spiHz = selectedHz;
    portEXIT_CRITICAL(&metricsMux);
    if (!resultado)
    {
        Serial.println("[SD] Falha ao inicializar: SCLK=7 MISO=5 MOSI=6 CS=4");
        sdCard.initErrorPrint(&Serial);
        if (initError_ == SD_CARD_ERROR_READ_TIMEOUT)
            Serial.println("[SD] READ_TIMEOUT: aguardando inicio dos dados. Nao confirma erro de formato.");
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

String SDUtil::initStatus()
{
    if (!enabled()) { return "SD desativado neste dispositivo.\n"; }
    SharedSpi::Guard guard;
    if (mounted_) return "SD: montado (escrita nao testada)\n";
    char text[150];
    snprintf(text, sizeof(text), "SD indisponivel: ultimo begin() falhou; SdError=0x%02X,0x%02X\nSCLK=7 MISO=5 MOSI=6 CS=4\n",
        unsigned(initError_), unsigned(initErrorData_));
    String result(text);
    result += "Etapa: " + String(initStage_) + "\n";
    if (initError_ == SD_CARD_ERROR_READ_TIMEOUT)
        result += "READ_TIMEOUT: cartao nao iniciou envio dos dados no prazo.\n";
    return result;
}

namespace {
// O formatador imprime progresso a intervalos regulares. Cede CPU nesses pontos
// sem soltar o SPI: tarefas de SD/display continuam excluidas durante a operacao.
class SdFormatProgress : public Print {
public:
    size_t write(uint8_t ch) override {
        const size_t written = Serial.write(ch);
        vTaskDelay(1);
        return written;
    }
};
}

bool SDUtil::formatCard(String& report)
{
    if (!enabled()) { report = "SD desativado: formatacao bloqueada.\n"; return false; }
    SharedSpi::Guard guard;
    ++mountGeneration;
    mounted_ = false;
    spiHz_ = 0;
    capacityBytes_ = 0;
    portENTER_CRITICAL(&metricsMux);
    metrics.mounted = false;
    metrics.valid = false;
    metrics.spiHz = 0;
    portEXIT_CRITICAL(&metricsMux);
    sdCard.end();
    pinMode(14, OUTPUT); digitalWrite(14, HIGH);
    pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    // Nao exige volume montado: a estrutura FAT atual pode estar ilegivel.
    bool ready = false;
    uint32_t sectors = 0;
    const uint32_t rates[] = {400000UL, 100000UL};
    for (uint8_t i = 0; i < 2 && !ready; ++i) {
        digitalWrite(SD_CS, HIGH);
        delay(100);
        Serial.printf("[SD FORMAT] Inicializacao fisica a %lu Hz...\n", (unsigned long)rates[i]);
        initStage_ = "FORMAT_CARD_BEGIN";
        ready = sdCard.cardBegin(SdSpiConfig(SD_CS, SHARED_SPI | USER_SPI_BEGIN, rates[i], &SPI));
        if (ready) {
            initStage_ = "FORMAT_READ_CSD";
            sectors = sdCard.card()->sectorCount();
            ready = sectors != 0;
        }
        initError_ = sdCard.sdErrorCode();
        initErrorData_ = sdCard.sdErrorData();
        if (!ready) Serial.printf("[SD FORMAT] Falha etapa=%s SdError=0x%02X,0x%02X\n",
            initStage_, unsigned(initError_), unsigned(initErrorData_));
    }
    if (!ready) {
        report = "Formatador nao iniciado: falha de comunicacao fisica com o cartao.\n" + initStatus();
        return false;
    }
    // FatFormatter cria FAT16 para este cartao de ~1 GB; FAT32 nos maiores.
    if (sectors <= 12288UL || uint64_t(sectors) * 512 > 32ULL * 1024 * 1024 * 1024) {
        report = "Formatacao abortada: capacidade fora do intervalo suportado (mais de 6 MiB ate 32 GiB).\n";
        return false;
    }
    Serial.printf("[SD FORMAT] APAGANDO estrutura de arquivos do SD CS=4: %lu setores (%llu MiB).\n",
        (unsigned long)sectors, uint64_t(sectors) * 512 / (1024 * 1024));
    Serial.println("[SD FORMAT] Aguarde. Nao desligue a alimentacao.");
    uint8_t buffer[512];
    FatFormatter formatter;
    SdFormatProgress progress;
    initStage_ = "FORMAT_WRITE";
    // Uma unica tentativa de escrita. Nao repete uma formatacao interrompida.
    const bool formatted = formatter.format(sdCard.card(), buffer, &progress);
    const bool synced = formatted && sdCard.card()->syncDevice();
    initError_ = sdCard.sdErrorCode();
    initErrorData_ = sdCard.sdErrorData();
    if (!synced) {
        report = "Formatacao falhou; a estrutura do cartao pode ter sido parcialmente alterada.\n" + initStatus();
        return false;
    }
    Serial.println("[SD FORMAT] Escrita concluida; conferindo montagem...");
    if (!begin()) {
        report = "Formatacao concluida, mas a remontagem falhou.\n" + initStatus();
        return false;
    }
    report = "SD formatado e remontado com sucesso. Arquivos anteriores apagados.\nTipo: " + getCardType() + "\n";
    return true;
}

bool SDUtil::test()
{
    if (!enabled()) { return false; }
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
    if (!enabled()) { return "Desativado"; }
    SharedSpi::Guard spiGuard;
    if (!mounted_ || !sdCard.card())
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
    if (!enabled()) { return 0; }
    SharedSpi::Guard spiGuard;
    // Capacidade ja validada na montagem; evita nova leitura CSD intermitente.
    return mounted_ ? capacityBytes_ : 0;
}

uint64_t SDUtil::getVolumeSizeBytes()
{
    if (!enabled()) { return 0; }
    SharedSpi::Guard guard;
    if (!mounted_ || !sdCard.vol() || !sdCard.vol()->fatType()) return 0;
    return uint64_t(sdCard.vol()->clusterCount()) * sdCard.vol()->sectorsPerCluster() * 512ULL;
}

bool SDUtil::exists(String path)
{
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    return sdCard.exists(path.c_str());
}

bool SDUtil::writeText(String path, String text)
{
    if (!enabled()) { return false; }
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
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    const bool isLog = path == "/log.txt" || path == "/network_history.log" ||
        path == "/network_history.previous.log" || path == "/diagnostic.log" ||
        path == "/diagnostic.previous.log";
    if (!mounted_ || !sdCard.card())
    {
        ++appendErrors;
        Serial.println("[SD] Falha ao acrescentar texto: cartao indisponivel.");
        if (isLog) recordLogWrite(false);
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
        if (isLog) recordLogWrite(false);
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
    if (isLog) recordLogWrite(ok);
    return ok;
}

String SDUtil::readText(String path)
{
    if (!enabled()) { return "SD desativado neste dispositivo.\n"; }
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
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    return sdCard.remove(path.c_str());
}

bool SDUtil::createDir(String path)
{
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    return sdCard.mkdir(path.c_str());
}

String SDUtil::listFiles(String path)
{
    if (!enabled()) { return "SD desativado neste dispositivo.\n"; }
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
    if (!enabled()) { return false; }
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
    if (!enabled()) { return ""; }
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
    if (!enabled()) { if (valid) *valid = false; return 0; }
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
    if (!enabled()) { if (valid) *valid = false; return 0; }
    if (valid) *valid = false;
    const uint32_t startedAt = millis();
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
            return 0; // Nao inicia uma varredura bloqueante para outros formatos.
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
        if (millis() - startedAt >= 15000UL)
        {
            Serial.println("[SD] Consulta de espaco excedeu 15 segundos.");
            return 0;
        }
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
        // Libera o SPI a cada setor, mas dorme apenas a cada oito setores.
        // Um tick por setor tornava a contagem de FATs grandes muito lenta.
        if ((sectorIndex & 7U) == 0) vTaskDelay(1);
    }
    if (valid) *valid = true;
    return uint64_t(clusters - freeClusters) * sectorsPerCluster * 512ULL;
}

int SDUtil::readLogTail(const char* path, String& content, bool& truncated)
{
    if (!enabled()) { content = "SD desativado neste dispositivo.\n"; truncated = false; return 503; }
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
    if (!enabled()) { return "SD desativado neste dispositivo.\n"; }
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
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    return sdCard.rmdir(path.c_str());
}

uint64_t SDUtil::getFileSize(String path)
{
    if (!enabled()) { return 0; }
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
    if (!enabled()) { return false; }
    SharedSpi::Guard spiGuard;
    return sdCard.rename(from.c_str(), to.c_str());
}
