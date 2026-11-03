# Prometheus - ESP32-C6 Gateway

Revisão documental: **02/11/2026** · [Início](README.md) · [Pinout](PINOUT.md) · [Comandos](COMANDOS.md) · [Operação](LEIA-ME.md)

## Configuracao web e portas

1. Abra `http://IP_DO_ESP32/prometheus` pelo link **Prometheus** no portal.
   Painel, redes, logs e formulario ficam sempre na **porta 80**.
2. Informe o endereco do servidor Prometheus: IPv4 ou DNS sem protocolo,
   caminho ou porta, como `192.168.1.10` ou `prometheus.local`.
3. Escolha a **porta de metricas do ESP32**, entre 1 e 65535; padrao **9120**.
4. Escolha o intervalo sugerido de coleta: 5..3600 segundos; padrao **15**.
5. Salve nas preferencias. A troca de porta ocorre cerca de um segundo
   depois, sem reboot nem redirecionamento da interface web.
6. Baixe `/prometheus.yml`, adicione o job no servidor e recarregue sua
   configuracao. Salvar no ESP nao configura o servidor automaticamente.

O modelo e **pull**: o Prometheus consulta
`http://IP_DO_ESP32:PORTA/metrics`, sem Pushgateway ou envio ativo pelo ESP.
O endereco do servidor e uma referencia persistente, nao configura destino
de POST nem restringe quem pode coletar. O intervalo so passa a valer na coleta
quando o YAML e aplicado no servidor.

| Servico | Porta |
|---|---|
| Painel, Wi-Fi, logs, formulario Prometheus e download YAML | **80**, fixa |
| Endpoint HTTP `/metrics` | Configurada e salva, padrao **9120** |

Se configurar 80, `/metrics` compartilha o servidor do painel, sem um segundo
listener nessa porta. Nas demais portas, essa rota retorna 404 na 80 e o
servidor de metricas nao oferece o formulario ou as paginas do portal.
A porta anterior deixa de atender apos a aplicacao da mudanca.
Isso vale em STA, no AP **ESP32_C6_CONFIG** (`192.168.4.1`) e apos reiniciar.

Os endpoints nao possuem autenticacao: use somente rede confiavel, sem expor
o ESP a Internet. Reserve o IP no DHCP. O YAML usa o IP STA quando conectado
e o IP AP quando offline; gere novamente se o IP do alvo mudar.
A porta do servidor Prometheus, normalmente 9090, e independente da porta
de metricas do ESP.

## Configuracao do servidor

Substitua IP e porta pelos valores reais:

```yaml
scrape_configs:
  - job_name: 'esp32_c6'
    scrape_interval: 15s
    metrics_path: /metrics
    static_configs:
      - targets: ['192.168.1.42:9120']
```

Se ja houver `scrape_configs:`, adicione apenas o job, sem duplicar a secao.
Use nomes de job unicos ou agrupe os dispositivos nos targets.

## Preferences/NVS e erros

O namespace **`telemetry`**, separado de `wifi` e `ntp_cfg`, guarda:

- `settings`: registro versionado de endereco do servidor, porta e intervalo.
- `boots`: contadores de inicializacoes e resets por watchdog.

As preferencias sao carregadas antes de iniciar o listener de metricas.
Entrada invalida retorna HTTP **400** sem salvar. Falha de gravacao retorna
**503**, com aviso no formulario e na Serial, preservando a configuracao
ativa em RAM. NVS indisponivel ou configuracao invalida gera aviso na Serial;
sem configuracao valida, usa porta 9120 e intervalo 15 segundos.

Boot/watchdog sao gravados uma vez por boot, nao por coleta, e saturam em
32 bits. Se o registro de contadores nao puder ser carregado ou gravado,
essas series sao omitidas, com aviso na pagina e na Serial.
`reset_wifi` e limpeza pelo BOOT continuam apagando somente `wifi`;
preservam `telemetry`, `ntp_cfg` e arquivos do SD. Uma limpeza completa
da NVS/flash por ferramentas externas pode apagar as preferencias.

## Metricas reais publicadas

Os tres contadores UDP ficam em RAM e reiniciam em zero a cada boot. O comando de limpeza de contadores persistentes de boot/watchdog nao os altera. Respostas seriais nao contam como respostas UDP. Chamadas sem destinatario UDP conhecido nao tentam envio e nao incrementam os contadores de envio. O listener aberto nao significa que o Wi-Fi esta conectado; acompanhe tambem `esp_status_online` e as metricas Wi-Fi.

Formato Prometheus **0.0.4**, com `HELP`/`TYPE` uma vez por familia e labels
escapados. Todas as series possuem **`device`**, o MAC STA estavel, inclusive
quando a placa opera como AP. Informacao textual usa gauge com valor 1 e labels.

| Grupo | Metrica | Semantica |
|---|---|---|
| Identificacao | `esp_info` | Valor 1; labels `chip` e `model`. |
| Identificacao | `esp_firmware_version` | Valor 1; labels `version` (mesma versao automatica do comando) e `build`. |
| Identificacao | `esp_ip_address` | Valor 1; labels `ip` e `interface` (`sta`/`ap`), por interface ativa. |
| Identificacao | `esp_mac_address` | Valor 1; MAC STA no label `mac`. |
| Disponibilidade | `esp_status_online` | 1 com Wi-Fi STA conectado, 0 caso contrario; nao testa Internet. |
| Disponibilidade | `esp_uptime_seconds` | Timer monotono de 64 bits, em segundos desde boot. |
| Disponibilidade | `esp_last_restart_reason` | Valor 1; codigo numerico `esp_reset_reason_t` no label `reason`. |
| Disponibilidade | `esp_boot_count` | Inicializacoes acumuladas na NVS. |
| Wi-Fi | `esp_wifi_rssi_dbm` | RSSI em dBm; omitida sem conexao STA. |
| Wi-Fi | `esp_wifi_channel` | Canal atual; omitida sem conexao STA. |
| Wi-Fi | `esp_wifi_disconnect_count` | Eventos de desconexao desde a inicializacao do servico, inclusive tentativas e desconexoes deliberadas. |
| UDP | `esp_udp_listener_up` | Gauge: 1 quando a ultima abertura da porta UDP 4210 teve sucesso; 0 antes de abrir ou em caso de falha. Nao testa conectividade nem entrega remota. |
| UDP | `esp_udp_packets_received_total` | Counter de datagramas detectados por `parsePacket()`, incluindo os rejeitados pelo limite de tamanho. |
| UDP | `esp_udp_replies_sent_total` | Counter de respostas completas aceitas para envio local. Uma resposta conta uma vez, mesmo dividida em varios datagramas; nao confirma recebimento remoto. |
| UDP | `esp_udp_send_errors_total` | Counter de respostas com falha em `beginPacket()`, escrita ou `endPacket()` de qualquer fragmento. Cada resposta com falha conta uma vez; pode ter ocorrido envio parcial. |
| Wi-Fi | `esp_wifi_tx_power` | Limite de potencia configurado no driver em dBm; nao e consumo eletrico. Omitida sem STA ou se a leitura falhar. |
| CPU | `esp_cpu_frequency_mhz` | Frequencia atual em MHz. |
| CPU | `esp_cpu_usage_percent` | Uso agregado dos nucleos, de 0 a 100%, calculado a partir dos tempos idle. |
| CPU | `esp_task_count` | Quantidade atual de tarefas FreeRTOS. |
| CPU adicional | `esp_cpu_temperature_celsius` | Temperatura interna do chip via `temperatureRead()`, somente quando finita; nao e sensor ambiental. |
| Clima adicional | `esp_weather_data_age_seconds` | Segundos desde a ultima atualizacao valida dos dados atuais Open-Meteo. |
| Clima | `sensor_temperature_celsius` | Gauge da temperatura do ar em Porto Alegre, em Celsius, obtida do Open-Meteo; labels `source="open_meteo"` e `location="porto_alegre"`, alem de `device`. |
| Clima | `sensor_humidity_percent` | Gauge da umidade relativa do ar em Porto Alegre, de 0 a 100%, obtida do Open-Meteo; mesmos labels de origem e localizacao. |
| Memoria | `esp_heap_free_bytes` | Heap livre. |
| Memoria | `esp_heap_total_bytes` | Heap total. |
| Memoria | `esp_heap_minimum_free_bytes` | Menor heap livre desde boot. |
| Memoria | `esp_psram_free_bytes` | PSRAM livre; zero quando nao disponivel. |
| Memoria | `esp_psram_total_bytes` | PSRAM total; zero quando nao disponivel. |
| GPIO | `gpio_digital_input` | Nivel instantaneo do BOOT GPIO9, label `pin`: 0 pressionado, 1 solto. |
| GPIO | `gpio_digital_output` | Nivel instantaneo do backlight GPIO22, label `pin`; com PWM nao e brilho/duty cycle. |
| Sistema | `esp_reboot_count` | Inicializacoes acumuladas menos a primeira; inclui ciclos de energia. |
| Sistema | `esp_watchdog_resets` | Resets por watchdog geral, de interrupcao e de tarefa acumulados na NVS. |
| Sistema | `esp_free_rtos_tasks` | Mesmo numero de tarefas de `esp_task_count`. |
| Sistema | `esp_flash_usage_percent` | Tamanho do firmware / capacidade da particao em execucao, em porcentagem; nao e ocupacao de todo o chip flash. |
| Aplicacao | `application_requests_total` | Counter de requisicoes HTTP concluidas nos dois servidores. |
| Aplicacao | `application_errors_total` | Counter de respostas 4xx/5xx ou sem codigo de resposta. |
| Aplicacao | `application_response_time_ms` | Gauge da duracao do tratamento e envio da ultima requisicao concluida. |

### Limites de interpretacao

- CPU requer estatisticas FreeRTOS de runtime e duas amostras validas separadas
  por cinco segundos. A serie e omitida no aquecimento, sem suporte do SDK,
  acima de 64 tarefas ou com amostra invalida. O calculo usa runtime em
  microssegundos do ESP-IDF e normaliza a capacidade dos nucleos.
- Idade do clima e omitida antes da primeira leitura atual valida. Usa timer
  monotono de 64 bits, independente de NTP e da virada de `millis()`.
  Falhas preservam o cache e sua idade; uma leitura atual valida reinicia a
  idade mesmo se a previsao diaria falhar. Mede tempo desde a recepcao pelo
  ESP, nao a idade da observacao na origem. Cache e em RAM e reinicia no boot.
- GPIO nao conta pulsos. Nenhum modo de pino e alterado para coletar metricas.
  GPIO8 transporta dados do NeoPixel e nao e tratado como estado ligado/desligado.
- HTTP e desconexoes reiniciam no boot. O scrape atual entra nos contadores
  somente apos terminar a resposta. Tempo HTTP nao inclui toda a latencia
  do cliente, nao e media nem percentil. UDP e serial nao entram nesses contadores.
- O ESP desligado nao publica zero: use **`up`** do proprio Prometheus para
  disponibilidade. `esp_status_online` indica somente a conexao STA.

## Metricas do cartao SD

Mantida a pinagem da **Waveshare ESP32-C6-LCD-1.47 sem Touch**:
SCLK **7**, MOSI **6**, MISO **5**, CS **4**, em SPI compartilhado com TFT.
Montagem SdFat a **2 MHz**, sem formatacao automatica.

As coletas HTTP leem somente um cache numerico protegido por secao critica.
Uma tarefa `sdmetrics`, com pilha de 4096 bytes, atualiza esse cache a cada
30 segundos quando a consulta anterior terminou. Em FAT16/FAT32, a varredura
cooperativa libera o mutex SPI entre setores e cede tempo ao sistema, como a
consulta da pagina SD. Outros formatos usam a contagem nativa da SdFat e podem
pausar o display enquanto o SPI esta ocupado.

| Metrica | Tipo | Semantica |
|---|---|---|
| `sdcard_mounted` | gauge | Resultado conhecido da montagem SdFat: 1 montado, 0 falha; nao e teste fisico a cada scrape. |
| `sdcard_metrics_available` | gauge | 1 com ultima consulta de espaco valida; 0 antes da primeira consulta, em falha ou remount. |
| `sdcard_capacity_bytes` | gauge | Capacidade fisica informada pelo cartao, em bytes. |
| `sdcard_total_bytes` | gauge | Capacidade da area de clusters do sistema de arquivos, diferente da capacidade fisica. |
| `sdcard_used_bytes` | gauge | Espaco ocupado na area de clusters. |
| `sdcard_free_bytes` | gauge | Total menos usado. |
| `sdcard_usage_percent` | gauge | 100 * usado / total, com total positivo e usado <= total. |
| `sdcard_spi_frequency_hz` | gauge | Clock de montagem configurado, 2000000 Hz. |
| `sdcard_data_age_seconds` | gauge | Idade monotona em segundos do ultimo cache de espaco valido. |
| `sdcard_log_size_bytes` | gauge | Tamanho de cada log existente e legivel; label `file`: `/log.txt`, `/network_history.log`, `/network_history.previous.log`. |
| `sdcard_append_operations_total` | counter | Chamadas `appendText` gravadas e sincronizadas com sucesso desde boot; inclui comandos, rede e outros arquivos. |
| `sdcard_append_errors_total` | counter | Chamadas `appendText` com falha de acesso, abertura, escrita ou sincronizacao desde boot. |
| `sdcard_log_records_total` | counter | Registros de log acrescentados e sincronizados com sucesso desde boot. Conta uma operacao de append por registro, mesmo que contenha varias linhas. Inclui logs de comandos, rede e diagnostico. |
| `sdcard_log_errors_total` | counter | Tentativas de acrescentar registros de log com falha de acesso, rotacao, abertura, escrita ou sincronizacao desde boot. Cada tentativa falha conta uma vez. |

Os dois contadores de log sao publicados mesmo sem cache de espaco valido e
zeram no boot; nao representam a quantidade historica de linhas do cartao.
Usam o mesmo label `device` (MAC STA) de `esp_info`. Para os paineis Grafana:

```promql
sdcard_log_records_total and on(device) esp_info{model="${device}"}
```

```promql
sdcard_log_errors_total and on(device) esp_info{model="${device}"}
```

Nessas consultas, a variavel Grafana chamada `device` deve conter o valor
do label `model`, e nao o MAC. O cruzamento `on(device)` usa o MAC entre as
series. O filtro por modelo pode retornar varios dispositivos do mesmo modelo.

Capacidades, ocupacao, clock, idade e tamanhos dos logs sao omitidos sem cache
valido, nao preenchidos com zeros ficticios. Logs ausentes ou ilegíveis nao
geram series de tamanho. Contadores de append nao equivalem ao numero de
linhas historicas e nao contabilizam `writeText`, JSON ou todos os erros SD.
Nao sao persistidos; os arquivos continuam preservados no cartao.

Uma consulta SD pode levar mais de 30 segundos em cartoes lentos; nao inicia
outra tarefa ate terminar. A idade do cache permite identificar atraso.
Falha de leitura invalida o cache; falta de memoria para criar tarefa gera
aviso na Serial e `sdcard_metrics_available=0`. Remontagem invalida o cache
e cancela varreduras com dados de uma montagem anterior. Escritas concorrentes
podem alterar a ocupacao durante uma varredura: a estatistica nao e uma
fotografia transacional do sistema de arquivos.

A retirada fisica pode ser detectada somente no proximo acesso. A consulta
nao remonta automaticamente nem formata o cartao. Se o SD nao iniciou,
um comando SD, como `sd_status`, tenta monta-lo novamente. O
[diagnostico isolado](diagnosticos/SD_SPI_Diagnostico/LEIA-ME.md)
continua sem Wi-Fi, HTTP ou Prometheus.

## Metricas solicitadas sem integracao

As series abaixo nao sao publicadas ate existir fonte real, calibracao e
instrumentacao. Temperatura e umidade do Open-Meteo sao exportadas separadamente
com labels de origem e localizacao e nao representam sensores conectados na placa.
Elas aparecem somente apos a primeira consulta atual valida. Em caso de falha de
atualizacao, mantem os ultimos valores validos e a idade continua aumentando em
`esp_weather_data_age_seconds`. As consultas continuam em segundo plano, normalmente
a cada 15 minutos, sem consultar a API durante a coleta de `/metrics`.

| Grupo | Metricas | Necessidade |
|---|---|---|
| Energia | `esp_voltage_v`, `esp_current_ma`, `esp_power_w` | Medidor eletrico e leituras calibradas. |
| Bateria | `esp_battery_percent`, `esp_battery_voltage_v` | Entrada de medicao, calibracao e curva da bateria. |
| Ambiente | `sensor_pressure_hpa`, `sensor_air_quality` | Sensores locais integrados. |
| Reles | `relay_state`, `relay_activation_count`, `relay_runtime_seconds` | Mapeamento e controle de reles com registro das transicoes. |
| ADC | `gpio_analog_value` | Pinos e escalas sem conflito com os perifericos existentes. |
| MQTT | `mqtt_connected`, `mqtt_messages_sent`, `mqtt_messages_received`, `mqtt_publish_errors` | Cliente MQTT e instrumentacao de conexao, publicacao e recepcao. |

## Sensor de som e GPIO0

O sensor analógico de som RoboCore está configurado no **GPIO0** e alimentado
em **3V3**; veja [PINOUT.md](PINOUT.md#ligação-do-sensor-de-som-robocore). A tarefa
de captação alimenta o equalizador de 12 faixas da página **10**, com ganho
visual ajustável e histórico temporal alternativo de 9,6 segundos. Não há uma série Prometheus de som
implementada, nem exportação dessa leitura como `gpio_analog_value` ou decibéis.
As séries de temperatura e umidade continuam sendo dados do Open-Meteo de
Porto Alegre, identificados pelos labels de origem e localização.

## Compilacao e teste na placa

Preserve placa, flash, USB e esquema de particoes compativeis com o hardware.
As tarefas existentes continuam em [.vscode/arduino.ps1](.vscode/arduino.ps1).
Nao foi feita gravacao da placa. Verificacao sintatica nao valida link,
tamanho final do binario ou comportamento fisico de NVS/SPI/rede.

Roteiro:

1. Confira `/info`, `/wifi`, `/logs` e `/prometheus` na porta 80 em STA e AP.
2. Salve servidor, porta 9121 e intervalo 20; confira `:9121` e `20s` no YAML.
3. Apos um segundo, espere HTTP 200 em `http://IP:9121/metrics`, content-type
   Prometheus, 404 em `http://IP/metrics` e painel ainda na 80.
4. Reinicie: confira host, porta e intervalo restaurados, boots/reboots
   incrementados e watchdog sem incremento em reboot normal.
5. Troque para 80 e de volta para 9121; somente a porta ativa deve oferecer
   `/metrics`, sem mover o formulario e as paginas.
6. Envie host com protocolo, porta 0/65536, intervalo 4/3601: espere HTTP 400
   e preferencias preservadas. Erro real de NVS deve retornar 503.
7. Espere duas amostras CPU; confira RSSI/canal em STA, BOOT GPIO9 e
   temperatura interna com `temp`. Confira contadores HTTP apos acessar uma
   rota valida e uma inexistente.
8. Espere o cache SD e compare com `sd_status` e `sd_log`; confira
   espaco livre e porcentagem. O total de `sd_status` e a capacidade fisica,
   enquanto `sdcard_total_bytes` representa a area de clusters, entao os
   denominadores de espaco livre/ocupacao sao diferentes. Em outro boot sem SD, espere montagem/cache
   iguais a zero e capacidades omitidas, sem leituras fabricadas.
9. Confira tamanhos dos tres logs e contadores apos comandos/append.
   Durante tentativas de montagem e consultas de espaco, observe display, botao e HTTP.
10. Confira idade do clima ausente antes de sincronizar, crescente apos
    sucesso e preservada apos falha de consulta.
11. Aplique o YAML no servidor e confira `up{job="esp32_c6"} == 1`; desligue
    o ESP e confirme `up == 0`.
