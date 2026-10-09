#line 1 "C:\\PC\\ESP32\\ESP32C6\\diagnosticos\\SD_SPI_Diagnostico\\LEIA-ME.md"
# Teste isolado do SD

Abra `SD_SPI_Diagnostico.ino` como um sketch separado na Arduino IDE, selecione a mesma placa e configuracoes USB do firmware e grave por USB. Abra o monitor serial em 115200 e reinicie a placa. Este sketch substitui temporariamente o firmware; grave `ESP32C6.ino` novamente depois do teste.

Nao usa SdFat, display, Wi-Fi ou sistema de arquivos. Envia somente clocks de partida e CMD0 a 100 kHz, sem gravar ou formatar o cartao.

Os pinos padrao sao os da **ESP32-C6-LCD-1.47 sem Touch**: SCLK 7, MISO 5, MOSI 6, CS 4. A **ESP32-C6-Touch-LCD-1.47** usa SCLK 1, MISO 3, MOSI 2, CS 4; confirme o modelo fisico antes de editar as constantes. Nao teste pinagens aleatorias.

- `0x01`: o cartao respondeu ao reset SPI; isso ainda nao comprova montagem ou leitura dos arquivos.
- `0xFF` em todas as tentativas: nao houve resposta detectada. O teste sozinho nao distingue pinos incorretos, mau contato, alimentacao ou defeito.
- Outras respostas: envie o log completo para analise.

Para comparar cartoes, desligue a alimentacao antes de trocar e repita o mesmo teste. Nao formate o cartao para resolver uma falha de CMD0.

Referencias oficiais: https://docs.waveshare.com/ESP32-C6-LCD-1.47 e https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47
