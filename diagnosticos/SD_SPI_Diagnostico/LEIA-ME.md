# Teste isolado do SD

Revisão documental: **02/11/2026** · [Início](../../README.md) · [Pinout](../../PINOUT.md) · [Operação](../../LEIA-ME.md)

## Relação com o sensor de som

No firmware principal, o sensor RoboCore usa o **GPIO0**, com VCC em 3V3 e GND
comum. Ele não ocupa os pinos do SD e não é utilizado por este sketch isolado.
O teste abaixo mantém somente SPI SCLK 7, MOSI 6, MISO 5 e CS 4, com CS do
display desativado. Consulte a [tabela completa de ligações](../../PINOUT.md)
antes de mudar a fiação. Não altere GPIO4..7 para conectar o sensor de som.

## Relacao com o monitoramento Prometheus

Este sketch continua **isolado**, sem portal ou Prometheus. O firmware
principal possui `/prometheus` na porta 80 e `/metrics` na porta configurada
e salva (padrao 9120). As metricas do SD usam montagem e sistema de arquivos
da SdFat; uma resposta CMD0 deste teste nao garante que essas metricas
estejam disponiveis. Gravar este diagnostico substitui o firmware principal:
nao espere atendimento HTTP durante o teste. Depois, grave novamente o
firmware principal e siga [PROMETHEUS.md](../../PROMETHEUS.md).

## Executar o teste

Abra `SD_SPI_Diagnostico.ino` como um sketch separado na Arduino IDE, selecione a mesma placa e configuracoes USB do firmware e grave por USB. Abra o monitor serial em 115200 e reinicie a placa. Este sketch substitui temporariamente o firmware; grave `ESP32C6.ino` novamente depois do teste.

Nao usa SdFat, display, Wi-Fi ou sistema de arquivos. Envia somente clocks de partida e CMD0 a 100 kHz, sem gravar ou formatar o cartao.

## Pinagem e interpretação

Os pinos padrao sao os da **ESP32-C6-LCD-1.47 sem Touch**: SCLK 7, MISO 5, MOSI 6, CS 4. A **ESP32-C6-Touch-LCD-1.47** usa SCLK 1, MISO 3, MOSI 2, CS 4; confirme o modelo fisico antes de editar as constantes. Nao teste pinagens aleatorias.

- `0x01`: o cartao respondeu ao reset SPI; isso ainda nao comprova montagem ou leitura dos arquivos.
- `0xFF` em todas as tentativas: nao houve resposta detectada. O teste sozinho nao distingue pinos incorretos, mau contato, alimentacao ou defeito.
- Outras respostas: envie o log completo para analise.

Para comparar cartoes, desligue a alimentacao antes de trocar e repita o mesmo teste. Nao formate o cartao para resolver uma falha de CMD0.

Referências oficiais: [Waveshare sem Touch](https://docs.waveshare.com/ESP32-C6-LCD-1.47)
e [Waveshare Touch](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47).
