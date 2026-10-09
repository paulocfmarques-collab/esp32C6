# ESP32-C6 Gateway — LEIA-ME

Firmware para a **Waveshare ESP32-C6-LCD-1.47**, com display ST7789 de 172 × 320 pixels, LED RGB e microSD. Reúne console UDP, portal de configuração, cinco redes Wi-Fi, NTP, clima e monitor de conexão.

Esta atualização mantém a pinagem e o hardware dos arquivos ESP32-C6 fornecidos. Não é o firmware do ESP32-C3 com tela de 1,44 polegada.

## O que foi melhorado

- Gravação de redes confirmada por leitura, incluindo senha vazia; atualização circular de até cinco SSIDs.
- Busca e reconexão por estados, sem esperar dez segundos em um laço bloqueante por rede. O SSID procurado aparece na tela.
- AP de configuração disponível durante a busca e após perda de conexão; encerrado quando a STA conecta.
- Exclusão mútua recursiva no SPI entre display, SD e desenhos da OTA; dados da tarefa de consulta SD protegidos.
- Ping assíncrono ao gateway, gráfico das últimas 24 amostras, quedas, falhas de ping, tempo offline e histórico no SD.
- Previsão diária separada: mínima, máxima e probabilidade máxima de chuva do dia.
- Portal responsivo com informações do dispositivo e edição dos cinco perfis; senhas salvas não são enviadas ao navegador.
- Backlight PWM ajustável, redução automática após inatividade e novos comandos documentados no `help`.

## Instalação

1. Extraia o pacote e mantenha todos os fontes juntos na pasta `ESP32C6`.
2. Abra `ESP32C6.ino` na Arduino IDE.
3. Instale o pacote **esp32 by Espressif Systems 3.x**, com suporte ao ESP32-C6.
4. Instale **Arduino_GFX_Library**, **Adafruit NeoPixel** e **SdFat 2.x**. Wi-Fi, HTTP, Preferences, NTP e ArduinoOTA vêm do core.
5. Selecione a placa ESP32-C6 correspondente ao seu hardware e uma partição com suporte a OTA. Compile, escolha a porta USB e grave.
6. Abra o monitor serial em **115200 baud**.

A API de PWM usada é a do core 3.x (`ledcAttach`/`ledcWrite` por GPIO). Configurações USB e tamanho de flash devem seguir a placa física.

## Pinagem preservada

| Recurso | GPIO |
| --- | --- |
| SPI SCLK | 7 |
| SPI MOSI | 6 |
| SD MISO | 5 |
| SD CS | 4 |
| TFT CS | 14 |
| TFT DC | 15 |
| TFT reset | 21 |
| TFT backlight | 22 |
| LED RGB | 8 |
| Botão BOOT | 9 |

O painel usa offset de 34 pixels e orientação paisagem nas páginas. Não altere essa configuração para outra placa sem conferir seu esquema.

## Configuração Wi-Fi

No boot, o dispositivo abre **ESP32_C6_CONFIG**, sem senha, e inicia a busca. Conecte a esse AP e abra **http://192.168.4.1/wifi**. Preencha até cinco perfis e salve. A gravação é lida novamente antes do reinício; em caso de erro, o firmware tenta restaurar os dados anteriores.

A busca começa pelo último perfil conectado, quando conhecido, e percorre os cinco perfis, pulando posições vazias. Cada tentativa dura até 10 segundos enquanto o programa continua atendendo botão, portal e comandos. Redes ocultas também são tentadas. Sem conexão, o AP permanece aberto e uma nova rodada começa após 30 segundos. Quando conectado, acesse `/info` e `/wifi` no IP recebido pelo dispositivo.

`wifi_add:SSID:senha` permite cadastrar via UDP. Novos SSIDs usam uma lista circular; repetir um SSID atualiza sua senha. `wifi_list` devolve somente os nomes. Perfis legados `ssid`/`senha` são migrados para o primeiro slot quando possível; a lista `ssid0..4`/`pass0..4` é preservada.

## Tela e botão

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

- Pressão curta no **BOOT**: avança uma página.
- Segure por 3 segundos e solte: reinicia.
- Segure por 10 segundos e solte: limpa as redes salvas e reinicia. O firmware, os arquivos SD e o namespace `ntp_cfg` permanecem.

As páginas voltam ao relógio após 30 segundos. O brilho padrão é 80%, cai para no máximo 20% após 2 minutos e apaga após 15 minutos sem botão/comando UDP. A rede continua funcionando. Botão ou comando acordam a iluminação no valor configurado; brilho zero requer `tela_on` ou `brilho:P`.

As barrinhas usam o RSSI real: quatro acima de -60 dBm, três acima de -70, duas acima de -80 e uma acima de -90. Três/quatro são verdes, duas amarelas e uma vermelha. Não há animação artificial de sinal.

## Monitor de rede e SD

O firmware envia um ping ICMP ao gateway aproximadamente a cada 5 segundos, com timeout de 1 segundo. Verde no gráfico representa RTT; vermelho indica ausência de resposta. Um gateway pode bloquear ICMP mesmo com Wi-Fi funcionando; ping ao gateway não testa acesso à Internet.

Quedas, tempo offline e estatísticas ficam em RAM. O SD registra boot, alterações de conexão e amostras de ping em `/network_history.log`, com rotação para `/network_history.previous.log` a partir de aproximadamente 64 KiB. São mantidos dois arquivos; registros usam segundos desde o boot, não data civil. Falhas de SD são informadas por `net_monitor` e a gravação é tentada novamente após 60 segundos. O restante dos recursos funciona sem cartão.

Tela, SD e OTA usam a mesma proteção de SPI. Uma operação longa no SD ainda pode atrasar um desenho, mas não deve acessar o barramento simultaneamente com a tela. As novas páginas redesenham áreas dinâmicas em vez de limpar a tela inteira a cada atualização.

## Hora e clima

NTP inicia em segundo plano, com fuso padrão UTC-3. Ajustes de fuso/DST são persistidos em `ntp_cfg`.

O clima usa **Open-Meteo**, coordenadas de **Porto Alegre** (`-30.03`, `-51.23`) e fuso `America/Sao_Paulo`. A consulta é executada numa tarefa separada, normalmente a cada 15 minutos; após falha, tenta novamente após 1 minuto. Dados válidos anteriores permanecem no cache. A previsão mostra sua idade e aviso quando a última consulta falhou. `clima_sync` apenas solicita a atualização, sem prometer um resultado imediato.

Para mudar a cidade, altere latitude, longitude e fuso na URL de `ClimaManager.h`, junto com o nome exibido nas páginas/comandos. A conexão HTTPS mantém a configuração original sem validação de certificado.

## Console e OTA

Console UDP: **4210**, limite de entrada **255 bytes**. Consulte [COMANDOS.md](COMANDOS.md) para sintaxe, limites e exemplos. O `help` corresponde à lista atualizada.

Console serial USB: **115200 baud**, com os mesmos comandos e limite de 255 bytes por linha. Selecione Nova linha, Retorno de carro ou Ambos NL e CR no Monitor Serial. Comece por `help`; `sd_status` consulta o cartao e `tela:7` abre o painel SD com animacao de leitura. Se usar USB nativo do ESP32-C6, habilite **USB CDC On Boot** na Arduino IDE. A contagem do espaco em FAT16/FAT32 e feita em pequenos blocos para permitir atualizacoes da animacao entre leituras; em outros formatos, a contagem nativa pode pausar a tela durante a consulta.

OTA usa hostname **ESP32-C6-Gateway** e é inicializada após conexão Wi-Fi. Portal, UDP e OTA não têm autenticação configurada neste projeto; mantenha-os em uma rede confiável. Use USB para a primeira gravação.

## Organização dos fontes

| Arquivo | Responsabilidade |
| --- | --- |
| `ESP32C6.ino` | Inicialização, botão, navegação e integração dos serviços |
| `ESP32Gateway.h/.cpp` | Perfis Wi-Fi, reconexão, portal, UDP e histórico |
| `CommandProcessor.h/.cpp` | Interpretação dos comandos e diagnósticos |
| `DisplayUtil.h/.cpp` | Páginas e backlight |
| `SharedSpi.h` | Proteção compartilhada de SPI |
| `NetworkMonitor.h/.cpp` | Ping, métricas e eventos |
| `PortalUi.h` | Layout HTML/CSS do portal |
| `ClimaManager.h` | Consulta meteorológica e cache protegido |
| `SDUtil.h/.cpp` | Operações no cartão |
| `NTPUtil.h/.cpp` | Sincronização e hora local |
| `OTAManager.h` | Atualização OTA |
| `RGBLed.h/.cpp` | LED RGB |

## Validação desta entrega

Foram realizadas verificações estáticas de integração, presença de arquivos, correspondência entre comandos/help/documentação e integridade do pacote. **A compilação Arduino e os testes físicos não foram executados neste ambiente.**

Após gravar, confira: cadastrar e listar redes; reiniciar e confirmar persistência; desligar o roteador e verificar busca/AP; restabelecer a rede; abrir páginas 8/9; consultar `net_monitor`/`net_history`; testar SD e brilho; confirmar botão e OTA na placa. Se o PWM falhar, o firmware informa no serial e usa apenas iluminação ligada/desligada.

## Referências

- [Preferences — implementação oficial](https://github.com/espressif/arduino-esp32/blob/master/libraries/Preferences/src/Preferences.cpp)
- [Wi-Fi — Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [LEDC — Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html)
- [ICMP Echo — ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html)
- [Open-Meteo Forecast API](https://open-meteo.com/en/docs)
