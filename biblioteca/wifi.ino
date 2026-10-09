Preferences prefs;

// HTML da página de configuração
const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Configuração WiFi</title>
<style>
  body { font-family: Arial, sans-serif; margin: 40px; background-color: #f4f4f9; text-align: center; }
  .container { background: white; max-width: 300px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  input { width: 100%; padding: 8px; margin: 10px 0; box-sizing: border-box; }
  input[type="submit"] { background: #007bff; color: white; border: none; cursor: pointer; }
</style>
</head>
<body>
<div class="container">
  <h2>Configuração WiFi - ESP32-P4</h2>
  <form action="/salvar" method="POST">
    <label>SSID:</label>
    <input type="text" name="ssid" placeholder="Nome da rede" required>
    <label>Senha:</label>
    <input type="password" name="senha" placeholder="Senha da rede">
    <input type="submit" value="Salvar">
  </form>
</div>
</body>
</html>
)rawliteral";

void zerarConfiguracoes() 
{
  printTela("Limpando Memoria...");

  prefs.begin("wifi", false);
  prefs.clear(); 
  prefs.end();
  
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.print("WiFi zerado. Reiniciando...\n");
  udp.endPacket();

  for(int i=0; i<10; i++) {
//    digitalWrite(LED, HIGH); delay(100);
//    digitalWrite(LED, LOW); delay(100);
  }
  ESP.restart(); 
}

void iniciarPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_P4_CONFIG");
  
  printTela("Portal Ativo!");
  printTela("Wifi: ESP32_C6_CONFIG");
  printTela("IP: 192.168.4.1");

  server.on("/", HTTP_GET, []() {
      server.send(200, "text/html", htmlPage);
  });
  server.on("/salvar", HTTP_POST, salvarWifi);
  server.begin();
}

void salvarWifi() {
  printTela("Salvando rede...");
  String novoSSID = server.arg("ssid");
  String novaSenha = server.arg("senha");

  prefs.begin("wifi", false);
  prefs.putString("ssid", novoSSID);
  prefs.putString("senha", novaSenha);
  prefs.end();

  server.send(200, "text/html", "<h2>Configuracao salva! Reiniciando...</h2>");
  delay(2000);
  ESP.restart();
}

bool conectarWifi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("senha", "");
  prefs.end();

  if (ssid == "") return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  printTela("Conectando a:");
  printTela(ssid);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
//    delay(500);
//    digitalWrite(LED, !digitalRead(LED)); 
    tentativas++;
  }
//  digitalWrite(LED, LOW);
  return WiFi.status() == WL_CONNECTED;
}


