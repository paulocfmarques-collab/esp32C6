#line 1 "C:\\PC\\ESP32\\ESP32C6\\COMANDOS.md"
# Comandos — ESP32-C6 Gateway

Referência do `CommandProcessor.cpp` desta atualização. Envie um comando de texto por datagrama UDP para a **porta 4210** do dispositivo. No AP de configuração, use `192.168.4.1`. O limite de entrada é **255 bytes** por comando, incluindo argumentos. Nomes de comando aceitam maiúsculas/minúsculas; SSID, senha e texto mantêm a capitalização.

Também é possível enviar os mesmos comandos pela **porta serial USB a 115200 baud**. No Monitor Serial, selecione final de linha **Nova linha**, **Retorno de carro** ou **Ambos NL e CR**, digite `help` e envie. Cada linha executa um comando; as respostas aparecem no serial. Comandos recebidos pelo serial não enviam respostas ao último cliente UDP. Linhas acima de 255 bytes são descartadas inteiras, sem executar um comando truncado. Espaços em argumentos são preservados.

Para visualizar a animação de leitura do SD, envie `tela:7`. O painel mostra um cartão com feixe de leitura, partículas em órbita e uma barra de atividade enquanto consulta os dados. Em FAT16/FAT32, a consulta do espaço usado libera o SPI entre setores para atualizar a tela. A duração depende do cartão; não há atraso artificial. Em outros formatos, a contagem nativa da SdFat pode pausar a animação enquanto mantém o SPI ocupado.

Não há autenticação de comandos nesta versão. Use o console e o portal somente em uma rede confiável. Credenciais de `wifi_add` são ocultadas no log local, mas UDP não cifra a transmissão.

## Sistema

| Comando | Função |
| --- | --- |
| `help` | Lista todos os comandos. |
| `info` | Resumo de identificação, rede, memória, data e hora. |
| `status` | Estado dos serviços. |
| `uptime` | Tempo desde a inicialização. |
| `reason` | Motivo do último reset. |
| `version` | Versão gerada pelo firmware. |
| `build` | Data e hora da compilação. |
| `alive` | Confirma que o dispositivo responde. |
| `reboot` | Reinicia o processador. |
| `temp` | Temperatura interna do chip; não representa temperatura ambiente. |
| `cpu` | Informações de CPU. |
| `ram` | Memória RAM. |
| `flash` | Informações de flash. |
| `init` | Tenta inicializar novamente o SD. |
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
| `tela:N` | Seleciona a página 0 a 9; tabela abaixo. |
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

As páginas de informações retornam ao relógio após 30 segundos. Respostas de comando usam a orientação original de console por 10 segundos. `tela:N` e `tela_next` abrem o painel imediatamente.

## Economia de iluminação

Após 2 minutos sem botão/comando UDP, o backlight cai para no máximo 20%; após 15 minutos, apaga. Botão ou comando reiniciam esse período e restauram o brilho configurado. Se você definiu zero com `brilho:0` ou `tela_off`, use `tela_on` ou um `brilho` maior que zero. Esses comandos não colocam a CPU em deep sleep.

## Arquivos do histórico

- `/log.txt`: respostas de comandos que habilitam log.
- `/network_history.log`: boot, mudanças de conexão e amostras de ping; horário em segundos desde o boot.
- `/network_history.previous.log`: arquivo anterior, após rotação em aproximadamente 64 KiB.

`net_history` pode começar no meio de uma linha por retornar somente a cauda do arquivo. Eventos e estatísticas em RAM reiniciam no boot; os arquivos permanecem no cartão.
