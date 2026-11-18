#include "PanelPortal.h"
#include "PortalUi.h"
#include "ClimaManager.h"
#include "DigitalMic.h"
#include "DisplayUtil.h"
#include "LocalSensor.h"
#include "NetworkMonitor.h"
#include "NTPUtil.h"
#include "SDUtil.h"
#include <esp_timer.h>
#include <time.h>
#include <math.h>

extern DisplayUtil display;
extern NTPUtil ntp;
extern SDUtil sd;
extern SoundMonitor soundMonitor;

namespace {
constexpr const char* END_CARD = "</dl></article>";

void startCard(String& html, const char* title) {
    html += F("<article class='card screen'><h2>");
    html += title;
    html += F("</h2><dl>");
}

void row(String& html, const char* label, const String& value) {
    html += F("<div class='row'><dt>");
    html += label;
    html += F("</dt><dd>");
    for (size_t i = 0; i < value.length(); ++i) {
        switch (value[i]) {
            case '&': html += F("&amp;"); break;
            case '<': html += F("&lt;"); break;
            case '>': html += F("&gt;"); break;
            default: html += value[i]; break;
        }
    }
    html += F("</dd></div>");
}

void startChart(String& html, const char* label, bool bars, int minimum, int maximum) {
    html += F("</dl><p class='chart-label'>");
    html += label;
    html += F("</p><canvas role='img' aria-label='");
    html += label;
    html += F("' width='280' height='76' data-bars='");
    html += bars ? '1' : '0';
    html += F("' data-min='");
    html += String(minimum);
    html += F("' data-max='");
    html += String(maximum);
    html += F("' data-values='");
}

void endChart(String& html) {
    html += F("'></canvas><dl>");
}

String measurement(bool valid, float value, const char* unit) {
    return valid && isfinite(value) ? String(value, 1) + unit : String("—");
}
}

String panelPortalPage() {
    String html = portalStart("Telas", false, false, false, false, true);
    html += F(R"PANELS(
<style>
.nav{flex-wrap:wrap}.screen-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px;align-items:start}
.screen{padding:16px}.screen h2{margin-bottom:8px}.screen dl{margin:0}.screen dt{color:var(--muted)}
.screen dd{margin:0;text-align:right;overflow-wrap:anywhere;min-width:0}.screen .row{gap:10px;padding:7px 0}
.screen canvas{display:block;width:100%;height:auto;background:var(--bg);border-radius:6px}
.chart-label{margin:12px 0 4px;color:var(--muted);font-size:12px}
@media(max-width:980px){.screen-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media(max-width:620px){.screen-grid{grid-template-columns:1fr}}
</style>
<div class='top'><h1>Telas do ESP32</h1><p id='panel-status' class='muted' role='status'>Carregando leituras…</p></div>
<section class='screen-grid' id='screens' aria-label='Telas do dispositivo'></section>
<noscript><p>Ative o JavaScript para acompanhar as leituras.</p></noscript>
<script>
const screens = document.getElementById('screens');
const statusLine = document.getElementById('panel-status');
function drawChart(canvas) {
  const values = canvas.dataset.values.split(',').map(v => v === '' ? NaN : Number(v));
  const ctx = canvas.getContext('2d');
  const min = Number(canvas.dataset.min), max = Number(canvas.dataset.max);
  const w = canvas.width, h = canvas.height;
  const y = v => h - 4 - Math.min(1, Math.max(0, (v - min) / (max - min))) * (h - 8);
  ctx.strokeStyle = '#26354d';
  ctx.beginPath();
  ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();
  ctx.fillStyle = '#55d7ad'; ctx.strokeStyle = '#55d7ad'; ctx.lineWidth = 2;
  if (canvas.dataset.bars === '1') {
    const width = w / values.length;
    values.forEach((v, i) => {
      if (Number.isFinite(v)) ctx.fillRect(i * width + 2, y(v), Math.max(1, width - 4), h - 4 - y(v));
    });
    return;
  }
  ctx.beginPath();
  let started = false;
  values.forEach((v, i) => {
    if (!Number.isFinite(v)) { started = false; return; }
    const x = 2 + i * (w - 4) / Math.max(1, values.length - 1);
    if (started) ctx.lineTo(x, y(v));
    else { ctx.moveTo(x, y(v)); ctx.fillRect(x, y(v), 2, 2); }
    started = true;
  });
  ctx.stroke();
}
async function refreshPanels() {
  if (!document.hidden) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 4500);
    try {
      const response = await fetch('/api/telas', {cache:'no-store', signal:controller.signal});
      if (!response.ok) throw new Error('HTTP ' + response.status);
      screens.innerHTML = await response.text();
      screens.querySelectorAll('canvas').forEach(drawChart);
      statusLine.textContent = 'Atualizado às ' + new Date().toLocaleTimeString('pt-BR');
    } catch {
      statusLine.textContent = 'Atualização indisponível. Tentando novamente…';
    } finally { clearTimeout(timeout); }
  }
  setTimeout(refreshPanels, 2000);
}
refreshPanels();
</script>
)PANELS");
    html += portalEnd();
    return html;
}

String panelPortalCards(const NetworkMonitor& network, const String* profiles, uint8_t nextProfile) {
    String html;
    if (!html.reserve(8200)) return html;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const auto local = localSensor.snapshot();
    const bool localOk = local.fresh() && !local.lastFailed;
    const auto weather = ClimaManager::snapshot();
    const auto storage = sd.metricsSnapshot();
    const char* storageStatus = !sd.enabled() ? "Desativado" : storage.mounted ? "Montado" : "Indisponível";
    const String rssi = connected ? String(WiFi.RSSI()) + " dBm" : String("—");
    const String ip = (connected ? WiFi.localIP() : WiFi.softAPIP()).toString();
    const time_t wallTime = time(nullptr);
    const bool timeReady = wallTime > 1577836800;
    char clock[32] = "Aguardando NTP";
    struct tm localTime{};
    if (timeReady && localtime_r(&wallTime, &localTime))
        strftime(clock, sizeof(clock), "%d/%m/%Y %H:%M:%S", &localTime);

    startCard(html, "Relógio");
    row(html, "Hora local", clock);
    row(html, "Temperatura local", measurement(localOk, local.temperature, " °C"));
    row(html, "Umidade local", measurement(localOk, local.humidity, " %"));
    html += END_CARD;

    startCard(html, "NTP · Sincronização");
    row(html, "Estado", timeReady ? "Horário disponível" : "Aguardando");
    row(html, "Fuso", String("UTC") + (ntp.getFusoHora() >= 0 ? "+" : "") + String(ntp.getFusoHora()));
    row(html, "Horário de verão", ntp.isDstAtivo() ? "Ativo" : "Desativado");
    html += END_CARD;

    startCard(html, "NTP · Serviços");
    row(html, "Wi-Fi", connected ? "Conectado" : "Desconectado");
    row(html, "IP", ip);
    row(html, "Sinal", rssi);
    row(html, "Tempo ligado", String(static_cast<unsigned long>(esp_timer_get_time() / 1000000)) + " s");
    row(html, "SD", storageStatus);
    html += END_CARD;

    startCard(html, "Rede · Conexão");
    row(html, "SSID", connected ? WiFi.SSID() : String("—"));
    row(html, "IP", ip);
    row(html, "Gateway", connected ? WiFi.gatewayIP().toString() : String("—"));
    row(html, "Máscara", connected ? WiFi.subnetMask().toString() : String("—"));
    html += END_CARD;

    startCard(html, "Rede · Detalhes");
    row(html, "DNS 1", connected ? WiFi.dnsIP(0).toString() : String("—"));
    row(html, "DNS 2", connected ? WiFi.dnsIP(1).toString() : String("—"));
    row(html, "MAC", WiFi.macAddress());
    row(html, "Canal", connected ? String(WiFi.channel()) : String("—"));
    row(html, "Sinal", rssi);
    html += END_CARD;

    startCard(html, "Sistema");
    row(html, "CPU", String(ESP.getCpuFreqMHz()) + " MHz");
    row(html, "RAM livre", String(ESP.getFreeHeap() / 1024) + " KiB");
    row(html, "Menor RAM livre", String(ESP.getMinFreeHeap() / 1024) + " KiB");
    row(html, "Flash usada", String(ESP.getSketchSize() / 1024) + " KiB");
    html += END_CARD;

    startCard(html, "Redes salvas");
    for (uint8_t i = 0; i < 5; ++i) {
        const String label = String("Perfil ") + String(i + 1);
        row(html, label.c_str(), profiles[i].isEmpty() ? String("Vazio") : profiles[i]);
    }
    row(html, "Próximo perfil", String(nextProfile + 1));
    html += END_CARD;

    startCard(html, "Cartão SD");
    row(html, "Estado", storageStatus);
    if (storage.valid) {
        row(html, "Volume", String(static_cast<unsigned long>(storage.total / 1048576)) + " MiB");
        row(html, "Espaço usado", String(static_cast<unsigned long>(storage.used / 1048576)) + " MiB");
        row(html, "Espaço livre", String(static_cast<unsigned long>((storage.total - storage.used) / 1048576)) + " MiB");
    }
    html += END_CARD;

    startCard(html, "Monitor de rede");
    row(html, "Ping", !network.hasResult ? String("Aguardando") :
        network.replied ? String(network.rtt) + " ms" : String("Sem resposta"));
    row(html, "Quedas", String(network.drops));
    row(html, "Pings sem resposta", String(network.failures) + " / " + String(network.attempts));
    if (network.initialized) {
        uint32_t ceiling = 100;
        for (int value : network.history)
            if (value > 0 && uint32_t(value) > ceiling) ceiling = uint32_t(value);
        startChart(html, "Latência dos últimos pings (ms)", false, 0, ceiling);
        for (uint8_t i = 0; i < 24; ++i) {
            if (i) html += ',';
            const int value = network.history[(network.head + i) % 24];
            if (value >= 0) html += String(value);
        }
        endChart(html);
    }
    html += END_CARD;

    startCard(html, "Clima · Porto Alegre");
    row(html, "Temperatura", measurement(weather.sincronizado, weather.temperatura, " °C"));
    row(html, "Umidade", measurement(weather.sincronizado, weather.umidade, " %"));
    row(html, "Mínima / máxima", weather.previsaoValida ?
        String(weather.minima, 1) + " / " + String(weather.maxima, 1) + " °C" : String("—"));
    row(html, "Chance de chuva", weather.previsaoValida ? String(weather.chuva) + " %" : String("—"));
    html += END_CARD;

    startCard(html, "Som · RoboCore");
    const auto sound = soundMonitor.snapshot();
    const uint32_t now = millis();
    const bool soundFresh = sound.running && sound.updatedAt && uint32_t(now - sound.updatedAt) < 1000;
    row(html, "Ganho visual", String(sound.gain) + "×");
    row(html, "Nível relativo", soundFresh ?
        String(min(100UL, sound.latest * uint32_t(sound.gain) * 100UL / 4095UL)) + " %" : String("—"));
    if (soundFresh) {
        startChart(html, "Espectro · 80 Hz a 10 kHz", true, 0, 100);
        for (size_t i = 0; i < SoundMonitor::BAND_COUNT; ++i) {
            if (i) html += ',';
            html += String(100 * sqrtf(fminf(1, sound.bands[i] * sound.gain / 2047.5f)), 1);
        }
        endChart(html);
        startChart(html, "Histórico · últimos 9,6 s", false, 0, 100);
        uint16_t samples[SoundMonitor::HISTORY_SIZE]{};
        bool populated[SoundMonitor::HISTORY_SIZE]{};
        for (size_t i = 0; i < sound.count; ++i) {
            const uint32_t age = now - sound.times[i];
            if (age >= SoundMonitor::HISTORY_MS) continue;
            const size_t bin = SoundMonitor::HISTORY_SIZE - 1 - age / SoundMonitor::WINDOW_MS;
            samples[bin] = sound.values[i];
            populated[bin] = true;
        }
        for (size_t i = 0; i < SoundMonitor::HISTORY_SIZE; ++i) {
            if (i) html += ',';
            if (populated[i]) html += String(min(100UL, samples[i] * uint32_t(sound.gain) * 100UL / 4095UL));
        }
        endChart(html);
    }
    html += END_CARD;

    startCard(html, "Ambiente · DHT22");
    row(html, "Temperatura", measurement(localOk, local.temperature, " °C"));
    row(html, "Umidade", measurement(localOk, local.humidity, " %"));
    row(html, "Erros de leitura", String(local.errors));
    if (local.count) {
        startChart(html, "Histórico de temperatura (°C)", false, -10, 50);
        for (size_t i = 0; i < local.count; ++i) {
            if (i) html += ',';
            const size_t index = (local.head + LocalSensor::HISTORY_SIZE - local.count + i) % LocalSensor::HISTORY_SIZE;
            if (local.historyValid[index]) html += String(local.history[index], 1);
        }
        endChart(html);
    }
    html += END_CARD;

    startCard(html, "Luminosidade · LDR");
    const auto light = soundMonitor.lightSnapshot();
    const int dark = light.calibrated() ? light.dark : 0;
    const int bright = light.calibrated() ? light.bright : SoundMonitor::LightSnapshot::ADC_MAX;
    const int span = bright - dark;
    const int percent = (constrain(int(light.raw) - dark, 0, span) * 100 + span / 2) / span;
    row(html, "Luminosidade relativa", light.fresh() ? String(percent) + " %" : String("—"));
    row(html, "ADC", light.fresh() ? String(light.raw) : String("—"));
    row(html, "Escala", light.calibrated() ? "Calibrada" : "Bruta");
    html += END_CARD;

    startCard(html, "Microfone · INMP441");
    const auto mic = digitalMic.snapshot();
    const bool micFresh = mic.fresh();
    row(html, "Nível RMS", measurement(micFresh, mic.rmsDbfs, " dBFS"));
    row(html, "Pico", measurement(micFresh, mic.peakDbfs, " dBFS"));
    row(html, "Limite", String(mic.thresholdDbfs) + " dBFS");
    row(html, "Eventos", String(mic.events));
    row(html, "Alerta", !micFresh ? "Sem leitura recente" : mic.eventActive ? "Ativo" : "Inativo");
    if (micFresh) {
        startChart(html, "Espectro · 63 Hz a 8 kHz", true, -90, 0);
        for (size_t i = 0; i < DigitalMic::BAND_COUNT; ++i) {
            if (i) html += ',';
            html += String(mic.bands[i], 1);
        }
        endChart(html);
    }
    {
        DigitalMic::HistoryPoint points[DigitalMic::HISTORY_SIZE];
        const size_t count = digitalMic.history(points);
        if (count) {
            int16_t bins[DigitalMic::HISTORY_SIZE];
            for (auto& bin : bins) bin = INT16_MIN;
            const uint32_t plotNow = millis();
            for (size_t i = 0; i < count; ++i) {
                const uint32_t age = plotNow - points[i].timestamp;
                const size_t slot = age / DigitalMic::HISTORY_STEP_MS;
                if (slot < DigitalMic::HISTORY_SIZE)
                    bins[DigitalMic::HISTORY_SIZE - 1 - slot] = int16_t(points[i].rmsDbfs * 10);
            }
            startChart(html, "Histórico · últimos 30 s (dBFS)", false, -90, 0);
            for (size_t i = 0; i < DigitalMic::HISTORY_SIZE; ++i) {
                if (i) html += ',';
                if (bins[i] != INT16_MIN) html += String(bins[i] / 10.0f, 1);
            }
            endChart(html);
        }
    }
    html += END_CARD;
    return html;
}
