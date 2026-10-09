# Changelog

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
