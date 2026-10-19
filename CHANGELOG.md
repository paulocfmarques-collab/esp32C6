# Changelog

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
