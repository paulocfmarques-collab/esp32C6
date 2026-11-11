# SD: teste isolado de leitura

**Versao 3:** alem dos testes anteriores, imprime CSD bruto, CSD_STRUCTURE, OCR
e CCS. Se a leitura do boot do volume falhar, compara CMD17 direto com argumento
em setores e argumento em bytes, sem alterar o modo da SdFat. CRC7 dos comandos
e CRC16 dos dados sao verificados. Todas as operacoes continuam somente de leitura.
Envie especialmente as linhas `[REG]` e `[RAW]`.

**Correcao da versao 2:** reabre SPI apos `card.end()` em cada rodada. Na versao 1, as
falhas das rodadas posteriores podiam decorrer do barramento encerrado;
nao demonstram incompatibilidade com 2 MHz ou 100 kHz. O PASS inicial a
400 kHz enviado pelo usuario permanece um resultado valido de leitura/montagem.

Preparado em **06/11/2026** para **Waveshare ESP32-C6-LCD-1.47 sem Touch**.

Este sketch substitui temporariamente o firmware principal. Nao inicializa Wi-Fi,
OTA, display, microfone, DHT22, portal, logs ou metricas. Nao precisa dessoldar
componentes. Eles continuam fisicamente conectados e eventualmente alimentados:
o teste elimina concorrencia de software, mas nao isola defeitos eletricos.

## Como executar

1. Na Arduino IDE, use **Arquivo → Abrir** e selecione
   [SD_Leitura_Isolada.ino](SD_Leitura_Isolada.ino) desta pasta. Abra como sketch
   separado, sem copiar para a pasta do firmware principal.
2. Mantenha a mesma placa ESP32-C6 e configuracoes USB usadas no projeto.
   Para a serial USB nativa, mantenha **USB CDC On Boot habilitado**.
3. A unica biblioteca externa necessaria e **SdFat 2.x** (2.3.0 no ambiente atual).
4. Compile e grave **por USB**. O sketch nao oferece OTA.
5. Abra o Monitor Serial em **115200 baud** e aperte **RESET** uma vez para
   capturar o log desde `=== SD LEITURA ISOLADA v3 ===`.
6. Aguarde `=== FIM: ... ===` e envie **o log inteiro**, incluindo todos os testes.

A tela deve permanecer apagada: isso e intencional. O diagnostico inicia sozinho,
nao recebe `help`, `sd_status` ou outros comandos. Para repetir, aperte RESET.
Ao terminar, regrave [ESP32C6.ino](../../ESP32C6.ino) por USB para restaurar a
aplicacao. O sketch nao apaga configuracoes NVS nem formata o cartao.

## O que verifica

| Etapa | Verificacao |
| :--- | :--- |
| CARD_BEGIN | Inicializacao SPI do cartao |
| READ_CSD | Identificacao e capacidade fisica |
| SETOR_0 | Cinco leituras do setor zero, comparando um hash de conteudo |
| MBR/layout | Inicio de volume dentro dos limites da capacidade |
| BOOT_VOLUME | Cinco leituras do inicio do volume, com comparacao de hash |
| MONTAGEM | Montagem FAT/exFAT usando o inicio identificado |
| ARQUIVOS | Abertura da raiz somente para leitura e listagem de ate dez entradas |

Repete a sequencia com clock de dados limitado a **400 kHz**, **2 MHz** e
**100 kHz**, nessa ordem, sem modificar o conteudo. A inicializacao do protocolo
usa os limites proprios da SdFat. Nenhuma chamada de formatacao, escrita de
arquivo, exclusao ou teste de gravacao e usada.

## Interpretacao

- **3/3 PASS:** as etapas testadas funcionaram sem os drivers da aplicacao.
  Favorece investigar integracao/concorrencia, mas nao garante estabilidade
  futura nem verifica todos os setores ou escrita.
- **READ_CSD FAIL:** falha de comunicacao antes de interpretar FAT ou particoes.
- **BOOT_VOLUME FAIL:** o cartao nao entregou de forma confiavel o setor do
  volume, mesmo com o restante da aplicacao inativo.
- **Hash diferente:** leituras consecutivas retornaram conteudos diferentes,
  sem escrita feita pelo teste. O hash e um indicador simples, nao uma
  verificacao completa de integridade do cartao.
- **Somente MONTAGEM FAIL:** precisamos analisar o BPB/layout e eventuais
  erros de leitura; nao confirma necessidade de formatacao.
- **RAW ARG_BYTES encontra BPB enquanto ARG_SETORES falha:** e evidencia a favor
  de investigar classificacao/enderecamento, nao prova automatica de correcao.
  Em cartoes com enderecamento por setor, o argumento multiplicado por 512 pode
  apontar a outro setor e ate retornar dados validos. Compare assinatura, BPB,
  hashes e registros. O resultado RAW nao altera o FAIL da montagem original.
- **Algumas velocidades passam:** o resultado e parcial; envie todas as etapas
  para comparar. Nao conclua defeito do cartao somente por este teste.

Pinagem fixa: SCLK **7**, MISO **5**, MOSI **6**, CS SD **4**, CS LCD **14**
mantido alto e backlight **22** desligado. Nao use em outra variante de placa
sem conferir a [documentacao de pinout](../../PINOUT.md).

O [teste antigo de CMD0](../SD_SPI_Diagnostico/LEIA-ME.md) continua disponivel,
mas uma resposta CMD0 positiva so comprova a resposta ao reset do protocolo.

Referencia de enderecamento: [especificacao da SD Association](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver6.00.pdf).
