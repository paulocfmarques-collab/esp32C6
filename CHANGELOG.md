# Changelog

## 2026-11-17

- Portal `/telas` com os 14 painéis em uma grade responsiva e atualização a cada dois segundos enquanto a aba está visível.
- Console serial e UDP separados da apresentação do LCD. Comandos de navegação continuam abrindo o painel solicitado.
- Revisados os guias de operação, ligações, comandos, métricas e diagnóstico.

## 2026-11-16

- Histórico de 30 segundos e espectro de 12 faixas na página do INMP441. Métricas de RMS, pico, limite, estado e total de eventos no Prometheus. O LED magenta indica evento sonoro com duração mínima e histerese.

- Captura do INMP441 pelo I²S a 16 kHz, com SCK no GPIO18, WS no GPIO2 e DOUT no GPIO19. A página 13 mostra nível RMS e pico em dBFS.

- Página 12 para luminosidade relativa do LDR no GPIO1, com barra, valor ADC e indicação de leitura ausente ou desatualizada. ADC contínuo compartilhado com o RoboCore no GPIO0, com taxa nominal de 20 kHz por canal.

Documentação revisada em **17/11/2026** · [Início](README.md) · [Pinout](PINOUT.md) · [Comandos](COMANDOS.md) · [Prometheus](PROMETHEUS.md)

## 2026-11-06

- `crash_info` deixa de arquivar no SD: leitura e limpeza separadas, relatorio enviado antes de apagar o core dump interno. Serial confere bytes e flush; UDP confere todos os blocos enviados. Falha de leitura, reserva de memoria ou envio preserva o registro. Resultado da limpeza informado no console. SD continua desativado; sem compilacao ou gravacao.

- SD desativado no perfil deste dispositivo em `DeviceConfig.h`: protecao em todas as entradas de arquivo, montagem e formatacao, sem tarefa de diagnostico SD ou estatisticas, historico de rede suspenso. CS4 permanece inativo para preservar o SPI da tela. Portal/display/comandos informam desativacao; autoteste marca SKIP; panic interno preservado. `sdcard_enabled=0` explicita o estado no Prometheus.
- Adicionadas `sensor_local_temperature_celsius` e `sensor_local_humidity_percent` com `device`, `location="local"` e `sensor="dht22"`, seguindo ESP32C3Mini. NaN em leitura invalida; aliases locais anteriores preservados e dados Open-Meteo mantidos separados. Sem compilacao ou gravacao.

- Diagnostico isolado v3: apos v2 informado com falha no setor 2048 em todas as velocidades, adicionados CSD/OCR brutos e comparacao CMD17 direta com argumento por setor/byte, CRC7 e verificacao CRC16. Continua sem escrita e nao altera o modo de enderecamento no principal. CRC7 validado com vetores CMD0=0x95 e CMD8=0x87; sem compilacao ou teste fisico.

- Apos falha repetida em READ_VOLUME_BOOT no principal (antes dos demais drivers), partida SD replica o caminho de leitura do isolado: encerra estado anterior, reabre SPI, 16 bytes de clocks, espera de 250 ms por tentativa e cinco leituras de setor zero/boot do volume com hash e intervalos de 20 ms. Backlight mantido apagado ate inicializacao do display. Divergencia de conteudo cancela montagem; sem escrita de diagnostico ou formatacao automatica. Causa ainda nao confirmada; teste de integracao pendente.

- Resultado do teste isolado informado pelo usuario: leitura repetida, FAT16 e listagem passaram a 400 kHz sem formatar. Corrigido erro do diagnostico v1: `card.end()` encerrava SPI antes das rodadas seguintes sem reabrir; v2 chama SPI.begin apos cada end, portanto falhas posteriores do v1 nao comprovam incompatibilidade de frequencia. Firmware principal passa a usar SdFs explicito, espera inicial ate 3 s e tentativa inicial a 400 kHz, seguida de 100 kHz/2 MHz. Integracao ainda pendente de teste pelo usuario.

- Criado `diagnosticos/SD_Leitura_Isolada`: sketch separado sem Wi-Fi, OTA ou drivers dos demais perifericos. CSD, cinco leituras repetidas do setor zero e boot do volume com hash, montagem e ate dez entradas da raiz em O_RDONLY. Tres velocidades e resumo por tentativa; sem escrita ou formatacao. Preparado para compilacao e teste pelo usuario.

- Reativada a captura de som analogico RoboCore no GPIO0, mantendo DHT22 no GPIO3. Equalizador e historico disponiveis em `tela:10`; INMP441/I2S ainda nao implementado. Sem compilacao ou gravacao.

- `sd_status` diferencia volume montado de estatisticas indisponiveis: consulta cooperativa com flag de validade e tamanho util do volume, sem converter falha em zero usado/livre. Capacidade fisica reaproveita o CSD validado na montagem; tela SD tambem identifica total indisponivel em vez de 0 MiB. Sem formatacao automatica.

## 2026-11-05

- Adicionado `sd_format:APAGAR_TUDO`, somente serial USB: inicializacao fisica independente da montagem anterior, FAT16/FAT32 via FatFormatter, buffer de 512 bytes, progresso com cessao de CPU, sincronizacao e verificacao de remontagem. SPI reservado e sensor pausado. Rejeita chamadas UDP e argumentos diferentes; nao ha formatacao automatica nem repeticao de escrita em falha. Implementado a pedido explicito do usuario; nao executado no cartao, compilacao e gravacao a cargo do usuario.

- Apos INT_WDT durante OTA, leitura DHT22 substituida por captura GPIO open-drain com limite real de 120 us por pulso e 6 ms totais; erro encerra imediatamente a captura. Checksum, conversao e validacao fora da secao critica. OTA pausa o sensor antes do desenho inicial e retoma apos erro; em sucesso aguarda reboot. Watchdogs preservados; bibliotecas externas DHT deixam de ser necessarias. Causa exata do panic nao confirmada por ELF correspondente; correcao pendente de compilacao e teste.

- Montagem SD identifica FAT diretamente no setor 0 e entradas MBR com limites de capacidade validados. Usa inicio explicito para layouts reconhecidos; registra particoes, BPB e falha de leitura do boot do volume separadamente. Nao altera tabela de particoes ou formato. Compatibilidade pendente de teste no cartao da placa.

- Inicializacao SD separada em CARD_BEGIN, READ_CSD, READ_SECTOR_0 e VOLUME_BEGIN, com ate tres tentativas (dados em 2 MHz, 400 kHz e 100 kHz). Diagnostico serial identifica a etapa e o erro de cada tentativa; a frequencia selecionada segue nas metricas. Tentativas de inicializacao e verificacoes nao escrevem nem formatam; logs normais voltam apos montagem bem-sucedida.

- SD: detalha `READ_TIMEOUT` (`0x1E`) no boot e em respostas de estado. `appendText` exige montagem bem-sucedida antes de abrir arquivos, evitando tentativa de gravacao apos `begin()` falhar. Sem formatacao nem mudanca de frequencia SPI.

- Diagnostico: `sensor_local` consulta o DHT22; `selftest` inclui leitura local e montagem SD, sem escrita de teste. Falha de montagem retorna os codigos do ultimo `begin()` tambem na resposta serial/UDP. `crash_info` identifica o relatorio como persistido, possivelmente de execucao anterior.
- Banner serial no inicio do setup identifica motivo/codigo do reset atual, data de compilacao e etapas de inicializacao, mesmo sem SD. Nao apaga o panic persistido.

- DHT22/AM2302 local em GPIO3/3V3: biblioteca Adafruit, tarefa de 2,5 s, validacao de intervalo/checksum, estado e historico de 48 tentativas com lacunas em falhas. Pagina 11 persistente, relogio com ambiente local, cabecalhos padronizados e pagina Porto Alegre separada. Metricas locais com `source="dht22",location="local"`, idade, disponibilidade e erros. Som analogico desativado (`ANALOG_PIN=-1`) apos retirada do RoboCore; INMP441 ainda pendente. Sem compilacao ou gravacao nesta revisao.


- Consulta HTTPS do clima usa `WeatherSecureClient` com DNS via `lwip_getaddrinfo`, evitando `Network.hostByName()` e a limpeza de cache DNS sem lock do core 3.3.12, associada ao assert `udp_new_ip_type` na tarefa weather quando ha callbacks NTP pendentes. Host HTTP e SNI TLS preservados, inclusive em redirecionamentos. Eventos `WEATHER_DNS_*` e `WEATHER_HTTP_*` identificam as etapas e o status HTTP. Sem alteracao dos arquivos do core instalado. Correcao pendente de compilacao e teste na placa.

## 2026-11-03

- Tela SD reutiliza estatisticas recentes das metricas e limita a varredura FAT a 15 segundos entre leituras. SPI continua liberado a cada setor, com pausa da tarefa a cada oito setores. Em falha ou limite excedido, mostra tipo/capacidade e espaco "nao calculado", sem representar falha como zero usado. Eventos `SD_INFO_START`, `SD_INFO_READY`, `SD_INFO_SPACE_UNAVAILABLE` e `SD_INFO_TASK_FAILED` no diagnostico. O limite nao interrompe uma operacao de hardware ja em andamento.

## 2026-11-02

- Monitor de som passa a usar ADC continuo/DMA a 20 kHz e FFT de 512 pontos para equalizador real de 12 faixas. Ganho visual padrao 32x, ajustavel por `som_ganho:1..64`, barras segmentadas com pico e modo temporal preservado via `som_modo:historico`. Sem biblioteca FFT externa ou gravacao de audio.
- Criado `PINOUT.md` com GPIOs, alimentação, pinos reservados e diagramas de ligação do sensor e do SPI. Revisados os guias Markdown do projeto para refletir o monitor de som contínuo, exceções de navegação, métricas disponíveis e referências cruzadas.
- Sensor RoboCore preparado no GPIO0, alimentado em 3V3: captacao continua em tarefa dedicada, grafico com timestamps numa janela de 9,6 segundos e pagina mantida aberta sem retorno automatico ou protetor. Limite de memoria fixo e sem gravacao de audio.
- Pagina 10 para monitor de som, com grafico colorido de ate 96 pontos e pico de amplitude relativa. Integrada ao BOOT, `tela:10` e `help`; a preparacao inicial da entrada desativada foi substituida pela captacao continua RoboCore no GPIO0 descrita acima. Sem valores simulados ou escala de decibeis.

## 2026-10-23

- Contadores `sdcard_log_records_total` e `sdcard_log_errors_total` para logs de comandos, rede e diagnostico, com label `device` compativel com `esp_info` e consultas Grafana documentadas.

## 2026-10-21 — Clima no Prometheus

- Exportacao de `sensor_temperature_celsius` e `sensor_humidity_percent` de Porto Alegre com labels `source="open_meteo"` e `location="porto_alegre"`. Consulta atual passa a incluir umidade relativa, validada entre 0 e 100%, com cache e idade da ultima atualizacao valida.

## 2026-10-21 — Formato parseável do help

- Padronizada a resposta de `help` como `COMANDOS` seguido de um comando por
  linha, sem descrições misturadas à lista.
- Atualizado o guia de comandos.

## 2026-10-19

- Metricas UDP: estado da abertura do listener, datagramas recebidos, respostas enviadas e erros de envio. Envios grandes verificam cada datagrama; contadores em RAM desde o boot e eventos de falha no diagnostico.

## Revisão — 2026-10-31

- Adicionado `reset_counters` para zerar somente os contadores NVS de
  inicialização e watchdog no namespace `telemetry`.
- O comando preserva as configurações Prometheus; menu `help` e documentação
  de comandos atualizados.

## Revisão documental — 2026-10-18

- Adicionado `name` à ajuda e ao guia de comandos para identificar o hostname
  configurado no dispositivo.
- Guia de comandos sincronizado com o protetor de robô e os arquivos
  `/diagnostic.log` e `/diagnostic.previous.log`, disponíveis no portal.
- Acrescentadas referências ao catálogo e ao
  [histórico consolidado](../ESP32_Documents/changelog.md).
- Nenhuma mudança de firmware ou nova validação física nesta revisão.

## 2026-10-18

- Corrigido vazamento no encerramento da tarefa de clima: objetos HTTP/HTTPS e Strings saem de escopo antes de `vTaskDelete()`, com encerramento explicito da conexao. Consulta SD tambem libera sua String antes de encerrar a tarefa. Diagnostico inclui `WEATHER_START`, `WEATHER_END` e etapas de montagem das metricas; falha na reserva de memoria retorna HTTP 503.
- Brilho PWM atualizado apenas quando o nivel muda, com evento `BACKLIGHT` no diagnostico. Paineis atualizam texto com fundo opaco e grafico por coluna, removendo apagamentos grandes a cada atualizacao para reduzir piscadas.
- Diagnostico persistente em `/diagnostic.log` e `/diagnostic.previous.log`, acessivel no portal: boot/reset, memoria, animacao, Wi-Fi, consultas de metricas e heartbeat. Escrita em tarefa separada com fila limitada e marcador RTC para consultas interrompidas por reset.
- Carinha do protetor percorre a tela na horizontal e vertical, rebatendo nas bordas sem deixar rastros, mantendo as expressoes e piscadas.
- Protetor Matrix substituido por robo vetorial com dez expressoes coloridas inspiradas na referencia, piscadas e movimento suave. Buffer de 176 x 136 pixels usado somente durante a protecao; desenho direto se faltar memoria. Mantidos o inicio apos 60 segundos e o retorno por botao/comando.

## 2026-10-16

- Corrigido o crash na partida em `SDUtil::processMetrics()`: o servico de metricas passa a ser inicializado uma unica vez em `ESP32Gateway::begin()`, em vez da rotina de busca Wi-Fi. Processamento antes da inicializacao e chamadas repetidas de `begin()` ficam protegidos.

## 2026-10-16

- Servico Prometheus orientado a objeto `MetricsService`, com `/metrics` na porta
  configurada (padrao 9120), mantendo portal, logs e configuracao na porta 80.
- Formulario `/prometheus` e download `/prometheus.yml`; endereco do servidor,
  porta de metricas e intervalo sugerido persistidos em `telemetry`, com
  validacao HTTP 400 e erro de gravacao HTTP 503.
- Metricas reais de identificacao, disponibilidade, Wi-Fi, CPU, tarefas,
  memoria, flash, BOOT, backlight e HTTP; contadores persistentes de
  inicializacoes e resets por watchdog.
- Temperatura interna da CPU e idade dos dados atuais de clima, com timer
  monotono de 64 bits; dados meteorologicos nao sao sensores locais.
- Cache SD consultado por tarefa a cada 30 segundos, sem acesso SPI por
  scrape; capacidade, ocupacao, disponibilidade, clock, idade, tamanhos de
  logs e contadores atomicos de append. Falhas invalidam a amostra em vez de
  simular espaco ocupado igual a zero.
- Varredura SD cooperativa cancelada quando muda a montagem; append confirma
  sincronizacao antes de declarar sucesso e informa falhas na Serial.
- Markdown atualizado e catalogo em `PROMETHEUS.md`, incluindo as 17 series
  sem integracao real e roteiro de validacao na placa.
- Verificacao sintatica focalizada com compilador RISC-V do core 3.3.12.
  Compilacao/link completos, tamanho final e testes na placa ficam pendentes.

## 2026-10-14

- Comando `crash_info` por serial/UDP, seguindo o Central: leitura explicita do core dump ELF, relatorio unico no SD, conferencia por releitura e apagamento interno somente apos arquivamento confirmado. Relatorio adaptado aos registradores e pilha RISC-V do ESP32-C6.

## 2026-10-12

- Inventário geral de comandos atualizado para refletir o `help` do firmware,
  incluindo controles de tela, rede, previsão e histórico do SD.
- Portal `/logs` com log de comandos e historicos de rede, atualizacao automatica e leitura limitada a 8 KiB; rota `/api/logs` restrita aos tres arquivos de log conhecidos.
- Removido o desligamento automatico da tela; protetor Matrix continuo apos 60 segundos de inatividade, com atualizacao a cada 40 ms e retorno por botao ou comando serial/UDP.
- Mantida a reducao de brilho apos 2 minutos e o desligamento manual por comando.

Mudanças desta atualização dos fontes ESP32-C6 fornecidos. A versão exibida pelo comando `version` mantém o mecanismo automático já existente.

## Em desenvolvimento — 2026-10-08

### Adicionado

- Console serial USB a 115200 baud com os mesmos comandos UDP, entrada por linha e descarte de linhas acima de 255 bytes.
- Animacao SD com cartao iluminado, feixe de leitura, particulas e barra de atividade; consulta FAT16/FAT32 em setores com liberacao do SPI para atualizar a tela.
- Monitor de rede com ping assíncrono, gráfico de 24 amostras, RTT mínimo/médio/máximo, falhas, quedas e tempo offline.
- Registro de boot, conexão e ping no SD com rotação de dois arquivos em aproximadamente 64 KiB.
- Página de previsão diária de Porto Alegre: mínima, máxima, chance de chuva, data, idade e falha de atualização.
- Comandos `wifi_add`, `wifi_list`, `net_monitor`, `net_history`, `tela`, `tela_next`, `brilho`, `tela_on`, `tela_off` e `previsao`.
- Backlight PWM, redução após 2 minutos de inatividade e desligamento após 15 minutos.
- `SharedSpi.h`, `NetworkMonitor.h/.cpp` e `PortalUi.h`.
- Documentação Markdown: `LEIA-ME.md`, `COMANDOS.md` e este `CHANGELOG.md`.

### Alterado

- Busca dos cinco perfis e reconexão executadas por estados; AP disponível durante busca e após queda.
- Portal responsivo de informações e edição dos cinco perfis, com validação e confirmação por leitura.
- Consulta de clima em tarefa separada, preservando dados válidos quando há erro.
- `clima_sync` informa solicitação assíncrona; `help` inclui os comandos novos.
- Novas páginas atualizam áreas dinâmicas para reduzir limpezas completas.
- Barrinhas RSSI verdes, amarelas ou vermelhas conforme intensidade real.
- NTP iniciado sem laço de espera; OTA inicializada após conexão STA mesmo em modo AP+STA.

### Corrigido

- SD inicializado antes dos comandos do LCD, com clocks de partida e SPI explicito em ambas as bibliotecas; SdFat usa `USER_SPI_BEGIN` para preservar a configuracao do barramento.
- Falso erro de gravação Wi-Fi causado por comparar `putString()` com comprimento mais terminador.
- Acesso simultâneo de SD, display e desenhos OTA ao SPI; proteção recursiva compartilhada.
- Leitura/escrita concorrente do cache da tarefa SD.
- Remoção de `SPI.end()` que desmontava o periférico usado pela tela.
- Identificação incorreta ESP32-C3 no `help` e `info`.
- Divisão por zero no percentual de progresso OTA e validação da contagem de clusters livres do SD.
- Credenciais de `wifi_add` ocultadas no log serial e no console da tela.

### Validação e pendências

- Verificações estáticas e integridade do pacote realizadas.
- Compilação com core ESP32 3.x, gravação, comportamento do SPI/PWM e testes físicos de Wi-Fi, SD e OTA pendentes.
- Estatísticas em RAM reiniciam no boot; histórico no SD permanece.
- Economia implementada na iluminação; não representa deep sleep da CPU.
