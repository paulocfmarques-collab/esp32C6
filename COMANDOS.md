# Comandos — ESP32-C6 Gateway

Revisão documental: **02/11/2026** · [Pinout e ligação do sensor](PINOUT.md). Histórico:
[CHANGELOG.md](CHANGELOG.md); [catálogo geral](../ESP32_Documents/COMANDOS.md).

Referência do `CommandProcessor.cpp` desta atualização. Envie um comando de texto por datagrama UDP para a **porta 4210** do dispositivo. No AP de configuração, use `192.168.4.1`. O limite de entrada é **255 bytes** por comando, incluindo argumentos. Nomes de comando aceitam maiúsculas/minúsculas; SSID, senha e texto mantêm a capitalização.

Também é possível enviar os mesmos comandos pela **porta serial USB a 115200 baud**. No Monitor Serial, selecione final de linha **Nova linha**, **Retorno de carro** ou **Ambos NL e CR**, digite `help` e envie. Cada linha executa um comando; as respostas aparecem no serial. Comandos recebidos pelo serial não enviam respostas ao último cliente UDP. Linhas acima de 255 bytes são descartadas inteiras, sem executar um comando truncado. Espaços em argumentos são preservados.

No VS Code, use **Terminal > Executar Tarefa > ESP32C6: Monitor serial** e informe
a porta USB `COM` da placa. A tarefa configura 115200 baud. Encerre o monitor antes
de executar **ESP32C6: Compilar e gravar USB**. Consulte
[LEIA-ME.md](LEIA-ME.md#compilar-e-gravar-pelo-vs-code) para compilacao e gravacao.

A resposta de `help` começa com `COMANDOS` e apresenta cada comando em uma linha,
com argumentos após `:`. O formato sem descrições junto à lista facilita o
parseamento automático.

Para visualizar a animação de leitura do SD, envie `tela:7`. O painel mostra um cartão com feixe de leitura, partículas em órbita e uma barra de atividade enquanto consulta os dados. Em FAT16/FAT32, a consulta do espaço usado libera o SPI entre setores para atualizar a tela. A duração depende do cartão; não há atraso artificial. Em outros formatos, a contagem nativa da SdFat pode pausar a animação enquanto mantém o SPI ocupado.

Não há autenticação de comandos nesta versão. Use o console e o portal somente em uma rede confiável. Credenciais de `wifi_add` são ocultadas no log local, mas UDP não cifra a transmissão.

## Sistema

### Prometheus pela web

Nao foram criados comandos serial/UDP para Prometheus. Use `/prometheus` no
painel da **porta 80** para salvar endereco do servidor, porta de metricas
(padrao **9120**) e intervalo nas Preferences `telemetry`. O servidor coleta
`/metrics` na porta salva; aplique no Prometheus o YAML baixado da pagina.
`reset_wifi` e a limpeza por BOOT preservam essas preferencias.

Os contadores `application_*` medem requisicoes HTTP, nao comandos deste
console. `temp`, `sd_status` e `sd_log` ajudam a comparar as leituras;
o cache SD pode estar ate uma consulta atrasado. Se o SD nao iniciou, um
comando SD tenta monta-lo novamente; isso invalida o cache ate uma nova
consulta, sem reaproveitar dados de uma montagem anterior.
Veja [PROMETHEUS.md](PROMETHEUS.md) para todas as metricas e limitacoes.

| Comando | Função |
| --- | --- |
| `help` | Lista todos os comandos. |
| `name` | Retorna o nome configurado no dispositivo. |
| `info` | Resumo de identificação, rede, memória, data e hora. |
| `status` | Estado dos serviços. |
| `uptime` | Tempo desde a inicialização. |
| `reason` | Motivo do último reset. |
| `crash_info` | Mostra o ultimo reset e consulta o core dump. Salva e confere um relatorio `/crash_AAAAMMDD_HHMMSS.txt` no SD antes de apagar o registro interno; sem hora, usa `/crash_SEM_NTP_<millis>.txt`. Em caso de falha no SD, preserva o registro interno. Requer core dump ELF em flash habilitado no core/SDK e particao de core dump. Caso contrario, informa que o recurso nao esta habilitado. No C6, mostra registradores e pilha RISC-V para decodificacao com o ELF correspondente. |
| `version` | Versão gerada pelo firmware. |
| `build` | Data e hora da compilação. |
| `alive` | Confirma que o dispositivo responde. |
| `reboot` | Reinicia o processador. |
| `reset_counters` | Zera somente os contadores NVS persistentes de boot/watchdog no namespace `telemetry`; preserva as configurações Prometheus. |
| `temp` | Temperatura interna do chip; não representa temperatura ambiente. |
| `cpu` | Informações de CPU. |
| `ram` | Memória RAM. |
| `flash` | Informações de flash. |
| `init` | Alias de `reason`; mostra o motivo do último reset. |
| `heap` | Memória disponível. |
| `heap_min` | Menor quantidade de heap livre desde o boot. |
| `chip_info` | Identificação do chip. |
| `flash_info` | Detalhes da flash. |
| `health` | Diagnóstico resumido. |
| `selftest` | Autoteste dos recursos disponíveis. |

## Rede

| Comando | Função |
| --- | --- |
| `wifi_add:SSID:senha` | Grava ou atualiza uma rede, confirmando por leitura. Novo SSID ocupa a próxima posição circular; após cinco, substitui a mais antiga nessa ordem. |
| `wifi_list` | Retorna somente os SSIDs salvos, um por linha; lista vazia retorna uma linha vazia. |
| `net_monitor` | Estado Wi-Fi, RTT ao gateway, mínimo/média/máximo, falhas de ping, quedas, tempo offline e eventos recentes. |
| `net_history` | Retorna os últimos 2.000 bytes do histórico atual no SD. |
| `net_info` | Informações da conexão. |
| `mac` | Endereço MAC. |
| `reset_wifi` | Apaga o namespace de redes salvas e reinicia; preserva o firmware, o SD e as configurações NTP. |
| `rssi` | Intensidade recebida em dBm. |
| `ip` | IP da conexão STA. |
| `ssid` | SSID conectado. |
| `channel` | Canal Wi-Fi. |
| `wifi_status` | Estado da conexão STA. |

## Tela e iluminação

| Comando | Função |
| --- | --- |
| `tela:N` | Seleciona a página 0 a 10; tabela abaixo. |
| `som_ganho:V` | Ganho visual de 1 a 64, padrao 32. Realca sinal e ruido, sem alterar o ganho eletrico do sensor. |
| `som_modo:equalizador` | Mostra as 12 faixas reais de frequencia na pagina 10, modo padrao. |
| `som_modo:historico` | Mostra o historico temporal de volume de 9,6 segundos na pagina 10. |
| `tela_next` | Avança para a próxima página. |
| `brilho:P` | Ajusta o backlight de 0 a 100%; padrão 80%. Usa PWM no GPIO 22. |
| `tela_off` | Coloca o brilho em zero, mantendo processador e rede ativos. |
| `tela_on` | Liga o backlight em 80% e reinicia o período de inatividade. |

## Horário

| Comando | Função |
| --- | --- |
| `time` | Hora local. |
| `date` | Data local. |
| `ntp_status` | Estado da sincronização. |
| `set_fuso:X` | Define fuso inteiro UTC entre -12 e +14; padrão -3. |
| `dst_on` | Ativa acréscimo de uma hora. |
| `dst_off` | Desativa horário de verão. |
| `set_time:AAAA-MM-DD HH:MM:SS` | Ajusta a hora local manualmente, com validação de calendário; NTP pode corrigi-la depois. |

## LED RGB

| Comando | Função |
| --- | --- |
| `led_on` | Acende o LED. |
| `led_off` | Desliga o LED e interrompe o efeito em andamento. |
| `led_breath` | Inicia o efeito de respiração. |
| `led_pisca:P:I` | Pisca de 1 a 100 pulsos; intervalo de 1 a 5.000 ms. |
| `led_blink:I` | Pisca continuamente; intervalo de 50 a 60.000 ms. |

## Cartão SD

| Comando | Função |
| --- | --- |
| `sd_status` | Estado, tipo, capacidade, uso e espaço livre. |
| `sd_list[:/pasta]` | Lista a raiz ou uma pasta; resposta limitada a 1.000 bytes. |
| `sd_read:/arquivo` | Lê até os últimos 1.000 bytes do arquivo. |
| `sd_write:/arquivo:texto` | Cria ou substitui o conteúdo do arquivo. |
| `sd_append:/arquivo:texto` | Acrescenta texto e quebra de linha. |
| `sd_del:/arquivo` | Remove um arquivo. |
| `sd_mkdir:/pasta` | Cria uma pasta. |
| `sd_rmdir:/pasta` | Remove uma pasta vazia. |
| `sd_log` | Lê os últimos 1.000 bytes de /log.txt. |
| `sd_clear_log` | Remove /log.txt. |
| `sd_test` | Testa gravação, leitura e remoção de arquivo temporário. |

## Clima

| Comando | Função |
| --- | --- |
| `clima` | Temperatura e condição meteorológica atuais de Porto Alegre. |
| `clima_age` | Idade dos dados meteorológicos em cache. |
| `clima_sync` | Solicita consulta em segundo plano; resultado posterior em clima/previsao. |
| `previsao` | Mínima, máxima, chance de chuva, data e idade da previsão diária, incluindo aviso de falha na última consulta. |

## Exemplos

```text
wifi_add:MinhaRede:MinhaSenha123
wifi_add:RedeAberta:
wifi_list
net_monitor
net_history
tela:8
tela:9
brilho:60
previsao
sd_read:/network_history.previous.log
set_fuso:-3
```

Em `wifi_add`, o primeiro `:` depois do SSID separa a senha. A senha pode conter `:`; SSIDs que contêm `:` devem ser cadastrados no portal. SSID: 1 a 32 bytes; senha: vazia para rede aberta, 8 a 63 bytes ou 64 caracteres hexadecimais. Um SSID já existente é atualizado sem avançar o cursor. O comando confirma a gravação; a rede será usada na próxima reconexão, ou após `reboot`.

No portal, senha vazia preserva a senha anterior somente quando o SSID não muda. Marque **Rede aberta** para retirar a senha. Apague o SSID para remover o perfil. Salvar pelo portal verifica os cinco perfis e reinicia.

## Páginas

| Número | Página |
| --- | --- |
| 0 | Relógio, clima atual e sinal Wi-Fi |
| 1 | NTP e serviços — parte 1 |
| 2 | NTP e serviços — parte 2 |
| 3 | Rede — parte 1 |
| 4 | Rede — parte 2 |
| 5 | Sistema |
| 6 | Cinco redes salvas |
| 7 | Cartão SD |
| 8 | Monitor de rede e gráfico de latência |
| 9 | Previsão diária |
| 10 | Som: equalizador de 12 faixas ou historico temporal, com ganho ajustavel |

O monitor de som abre com `tela:10` ou pelo BOOT. O sensor analogico de som da
RoboCore fica configurado no GPIO0 (`SoundSensorConfig::ANALOG_PIN = 0` em
`SoundMonitor.h`). Com a placa desligada, ligue VCC a 3V3, GND a GND e a saida
de sinal analogico ao GPIO0. Nao alimente o sensor em 5 V nesta ligacao direta.

Tabela de GPIOs e diagrama da ligação: **[PINOUT.md](PINOUT.md#ligação-do-sensor-de-som-robocore)**.
O adaptador nao atende modulos somente digitais ou I2S. Use `ANALOG_PIN = -1`
para desativar a leitura se o sensor nao estiver conectado; uma entrada solta
pode produzir leituras sem significado.

A captacao usa ADC continuo/DMA a 20 kHz em tarefa dedicada, inclusive fora
da pagina. Uma FFT de 512 amostras remove o nivel DC e usa janela Hann para
agrupar a energia em 12 faixas de aproximadamente 80 Hz a 10 kHz. A resolucao
da FFT e aproximadamente 39 Hz. As barras segmentadas tem subida rapida,
queda suave e marcador de pico. Ha compressao visual para destacar sons fracos.

O ganho padrao e 32x; envie `som_ganho:64` para aumentar ou `som_ganho:8`
para reduzir. O ganho e aplicado somente na visualizacao, antes da compressao
do equalizador: nao altera o sinal eletrico nem melhora a relacao sinal/ruido.
`ADC SATURADO` indica saturacao na entrada; reduza a intensidade do sinal,
pois reduzir o ganho visual nao recupera uma amostra que ja saturou.

Use `som_modo:historico` para o grafico temporal de 9,6 segundos, com ate 96
pontos de amplitude pico a pico agrupados aproximadamente a cada 100 ms.
O ganho tambem se aplica a esse grafico. `som_modo:equalizador` retorna as
faixas de frequencia. Ganho e modo ficam em RAM e voltam a 32x/equalizador
no boot. A tela atualiza aproximadamente a cada 100 ms. Nao ha valores
simulados, gravacao de audio, medicao em decibeis ou calibracao de sonometro.

A pagina de som permanece aberta enquanto selecionada, sem retorno automatico
ao relogio nem entrada no protetor de tela. Use BOOT, `tela_next` ou `tela:N`
para sair. A reducao automatica de brilho continua ativa.

Referencias: [sensor RoboCore](https://www.robocore.net/sensor-ambiente/sensor-de-som-microfone)
e [GPIO/ADC do ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/gpio.html).

As páginas de informações retornam ao relógio após 30 segundos, exceto o monitor de som. Respostas de comando usam a orientação original de console por 10 segundos. `tela:N` e `tela_next` abrem o painel imediatamente.

## Economia de iluminação

Fora da página de som, após 60 segundos sem botão ou comando serial/UDP, o protetor de robô roda continuamente, alternando dez expressões coloridas a cada 4,2 segundos, com piscadas e movimento suave. A tela não desliga automaticamente. Após 2 minutos, o backlight cai para no máximo 20%, inclusive no monitor de som. Botão ou comando encerram o protetor e restauram o brilho configurado. Se você definiu zero com `brilho:0` ou `tela_off`, use `tela_on` ou um `brilho` maior que zero. Esses comandos não colocam a CPU em deep sleep.

## Arquivos do histórico

- `/log.txt`: respostas de comandos que habilitam log.
- `/network_history.log`: boot, mudanças de conexão e amostras de ping; horário em segundos desde o boot.
- `/network_history.previous.log`: arquivo anterior, após rotação em aproximadamente 64 KiB.
- `/diagnostic.log` e `/diagnostic.previous.log`: diagnóstico persistente de
  inicialização, memória, animação, Wi-Fi, métricas e heartbeat. Consulte a
  categoria Diagnóstico na página de logs; não há comando `diagnostic` novo.

`net_history` pode começar no meio de uma linha por retornar somente a cauda do arquivo. Eventos e estatísticas em RAM reiniciam no boot; os arquivos permanecem no cartão.

Escrita e exclusão no SD não pedem confirmação. `crash_info` pode apagar o
dump interno após verificar a cópia; preserve o ELF correspondente. Esta
revisão de documentação não compila nem grava a placa.
