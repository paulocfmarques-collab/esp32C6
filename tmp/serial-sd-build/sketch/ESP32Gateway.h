#line 1 "C:\\PC\\ESP32\\ESP32C6\\ESP32Gateway.h"
#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ctype.h>
#include <stdarg.h>
#include "SDUtil.h"
namespace C6NetworkConfig { constexpr int FUSO_PADRAO=-3; constexpr uint16_t UDP_PORT=4210; constexpr uint32_t WIFI_ATTEMPT_MS=10000,WIFI_RETRY_MS=30000; }
#include "DisplayUtil.h"
extern DisplayUtil display;
extern SDUtil sd;
#include "RGBLed.h"
extern RGBLed rgbLed;
#include "NetworkMonitor.h"
#include "PortalUi.h"
static String esc(const String& s){String o;for(size_t i=0;i<s.length();i++){char c=s[i];if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\'')o+="&#39;";else if(c=='"')o+="&quot;";else o+=c;}return o;}
static String statusBadge(bool ok, const char* yes, const char* no) {
 return String("<span class='badge ") + (ok ? "ok" : "warn") + "'>" + (ok ? yes : no) + "</span>";
}
static String infoRow(const char* title, const String& value) {
 return "<div class='row'><span>" + String(title) + "</span><strong>" + esc(value) + "</strong></div>";
}
static String metric(const char* title, const String& value, const char* hint) {
 return "<article class='card'><div class='label'>" + String(title) + "</div><div class='value'>" + esc(value) + "</div><small>" + String(hint) + "</small></article>";
}

class ESP32Gateway {
 WebServer server{80};WiFiUDP udp;Preferences prefs;NetworkMonitor telemetry;
 static const int MAX_REDES=5;
 String tmpS[5],tmpP[5],ssids[5],passwords[5];
 bool pendingRestart=false,apActive=false,wasOnline=false;
 uint32_t restartAt=0,phaseAt=0,retryAt=0;
 uint8_t phase=0,attempted=0,cursor=0;
 int lastUsed=-1,slot=-1;
 bool historySeen=false,historyOnline=false,historyMeasured=false;
 uint32_t historyPingAt=0;
 bool historyEnabled=true,historyFailed=false;
 uint32_t historyNextTry=0;
 void carregarRedes(String names[5],String pass[5],int& idx){
  idx=-1;if(!prefs.begin("wifi",false))return;
  String legacy=prefs.getString("ssid","");
  if(legacy.length() && prefs.getString("ssid0","").isEmpty()){
   String oldPass=prefs.getString("senha","");prefs.putString("ssid0",legacy);prefs.putString("pass0",oldPass);
   if(prefs.getString("ssid0","")==legacy && prefs.getString("pass0","")==oldPass){prefs.remove("ssid");prefs.remove("senha");}
  }
  for(int i=0;i<5;i++){names[i]=prefs.getString(("ssid"+String(i)).c_str(),"");pass[i]=prefs.getString(("pass"+String(i)).c_str(),"");}
  idx=prefs.getInt("idx",-1);if(idx<0 || idx>=5)idx=-1;prefs.end();
 }
 void openAP(){if(!apActive){apActive=WiFi.softAP("ESP32_C6_CONFIG");if(apActive)display.println("AP: 192.168.4.1");}}
 void nextProfile(){
  while(attempted<5){int base=lastUsed>=0?lastUsed:0;slot=(base+attempted++)%5;if(ssids[slot].isEmpty())continue;
   WiFi.disconnect(false,false);WiFi.begin(ssids[slot].c_str(),passwords[slot].c_str());
   display.setConnecting(ssids[slot]);display.println("Buscando: "+ssids[slot]);phaseAt=millis();phase=2;return;
  }
  WiFi.disconnect(false,false);display.setConnecting("");phase=3;retryAt=millis()+C6NetworkConfig::WIFI_RETRY_MS;openAP();
 }
 void handleInfo(){
  bool online=estaConectado();String h=portalStart("Informações",false,true);
  h+=F("<div class='top'><div><div class='eyebrow'>Visão geral</div><h1>Seu ESP32-C6, ao vivo.</h1><p class='muted'>Rede, memória e histórico de conexão.</p></div><a class='button' href='/wifi'>Configurar redes</a></div><div class='grid metrics'>");
  h+=metric("Wi-Fi",online?String("Conectado"):String("Offline"),"Conexão sem fio");
  h+=metric("Memória livre",String(ESP.getFreeHeap()/1024)+" KB","Heap disponível");
  h+=metric("Tempo ligado",String(millis()/60000)+" min","Desde o boot");
  h+=metric("Quedas",String(telemetry.drops),"Histórico em RAM");h+=F("</div><div class='grid'><article class='card'><h2>Conexão</h2>");
  h+=infoRow("SSID",online?WiFi.SSID():String("AP ESP32_C6_CONFIG"));h+=infoRow("IP",(online?WiFi.localIP():WiFi.softAPIP()).toString());
  h+=infoRow("Gateway",online?WiFi.gatewayIP().toString():String("—"));h+=infoRow("RSSI",online?String(WiFi.RSSI())+" dBm":String("—"));h+=infoRow("MAC",WiFi.macAddress());
  h+=F("</article><article class='card'><h2>Dispositivo</h2>");h+=infoRow("Modelo",ESP.getChipModel());h+=infoRow("Frequência",String(ESP.getCpuFreqMHz())+" MHz");h+=infoRow("Menor heap",String(ESP.getMinFreeHeap()/1024)+" KB");h+=infoRow("Flash",String(ESP.getFlashChipSize()/1024/1024)+" MB");h+=infoRow("Console UDP",String(C6NetworkConfig::UDP_PORT));h+=infoRow("SD",sd.getCardSizeBytes()>0?String("Disponível"):String("Indisponível"));
  h+=F("</article><article class='card'><h2>Redes salvas</h2>");String names[5],pass[5];int idx;carregarRedes(names,pass,idx);
  for(int i=0;i<5;i++)h+=infoRow(("Perfil "+String(i+1)).c_str(),names[i].length()?names[i]:String("Disponível"));
  h+=F("</article><article class='card'><h2>Monitor de rede</h2><pre style='white-space:pre-wrap;font-size:12px'>");h+=esc(monitorReport());h+=F("</pre></article></div><p class='hint section'>Atualização a cada 10 segundos. Ping ao gateway não verifica Internet.</p>");
  h+=portalEnd();server.send(200,"text/html; charset=utf-8",h);
 }
void handleConfig() {
 String ssids[MAX_REDES], senhas[MAX_REDES]; int idx;
 carregarRedes(ssids, senhas, idx);
 String h = portalStart("Redes Wi-Fi", true);
 h += F("<div class='top'><div><div class='eyebrow'>Conectividade</div><h1>Suas redes Wi-Fi</h1><p class='muted'>Até cinco conexões salvas para o seu dispositivo.</p></div><a class='button secondary' href='/info'>Ver informações</a></div>");
 h += F("<div class='notice'>Deixe a senha vazia para manter a senha de uma rede com o mesmo SSID. Para remover uma rede, apague o nome. Marque <b>Rede aberta</b> para salvar sem senha.</div><form action='/salvar' method='post' id='networks'><div class='grid'>");
 for(int i=0;i<MAX_REDES;i++) {
  String n=String(i); bool active=WiFi.status()==WL_CONNECTED && ssids[i].length() && ssids[i]==WiFi.SSID();
  h += "<article class='card'><div class='net-title'><div><div class='slot'>PERFIL 0" + String(i+1) + "</div><h2>Rede " + String(i+1) + "</h2></div>";
  h += active ? statusBadge(true,"Conectada","") : ssids[i].length() ? String("<span class='badge'>Salva</span>") : String("<span class='badge'>Disponível</span>");
  h += "</div><label class='field' for='ssid"+n+"'>Nome da rede (SSID)</label><input id='ssid"+n+"' name='ssid"+n+"' type='text' maxlength='32' autocomplete='off' placeholder='Ex.: Minha rede' value='"+esc(ssids[i])+"'>";
  h += "<label class='field' for='senha"+n+"'>Senha</label><div class='password'><input id='senha"+n+"' name='senha"+n+"' type='password' maxlength='64' autocomplete='new-password' placeholder='"+String(ssids[i].length()?"Vazia mantém a senha atual":"8 a 63 caracteres ou chave hexadecimal")+"'><button type='button' aria-controls='senha"+n+"' aria-pressed='false' onclick='togglePassword(this)'>Mostrar</button></div>";
  h += "<label class='check'><input type='checkbox' name='aberto"+n+"' value='1' onchange='toggleOpen(this)'"+String(ssids[i].length() && senhas[i].isEmpty()?" checked":"")+">Rede aberta (sem senha)</label>";
  h += "<p class='hint'>"+String(i==idx?"Última rede utilizada pelo dispositivo.":"A senha salva nunca é enviada para esta página.")+"</p></article>";
 }
 h += F("</div><div class='actions'><button type='submit' id='save'>Salvar redes e reiniciar</button><p>As configurações são verificadas antes do reinício.</p></div><p id='form-status' role='status' class='muted'></p></form>");
 h += F(R"JS(<script>
function togglePassword(b){const i=document.getElementById(b.getAttribute('aria-controls'));const show=i.type==='password';i.type=show?'text':'password';b.textContent=show?'Ocultar':'Mostrar';b.setAttribute('aria-pressed',String(show))}
function toggleOpen(c){const i=c.closest('article').querySelector('input[name^="senha"]');i.disabled=c.checked;if(c.checked)i.value=''}
document.querySelectorAll('input[name^="aberto"]').forEach(toggleOpen);
document.getElementById('networks').addEventListener('submit',function(e){let error='';this.querySelectorAll('input[name^="ssid"]').forEach(i=>{if(new TextEncoder().encode(i.value.trim()).length>32)error='O nome de cada rede deve ter até 32 bytes.'});if(error){e.preventDefault();document.getElementById('form-status').textContent=error;return}document.getElementById('save').disabled=true;document.getElementById('form-status').textContent='Gravando e verificando suas redes…'});
</script>)JS");
 h += portalEnd(); server.send(200,"text/html; charset=utf-8",h);
}
void handleSalvar() {
 auto message=[&](int code,const char* title,const String& text,bool success){
  String h=portalStart(title,true);
  h+=String("<div class='card message ")+(success?"success":"error")+"'><div class='eyebrow'>Configuração de rede</div><h1>"+String(title)+"</h1><p class='muted'>"+esc(text)+"</p>";
  h+=success?String("<p>Após o reinício, acesse o novo IP do dispositivo. Se nenhuma rede conectar, use o AP ESP32_C6_CONFIG.</p>"):String("<a class='button' href='/wifi'>Voltar às redes</a>");
  h+="</div>"+portalEnd();server.send(code,"text/html; charset=utf-8",h);
 };
 if(pendingRestart){message(409,"Reinício em andamento","Aguarde o dispositivo reiniciar.",false);return;}
 String oldS[MAX_REDES],oldP[MAX_REDES];int oldIdx;
 carregarRedes(oldS,oldP,oldIdx);
 for(int i=0;i<MAX_REDES;i++) {
  String n=String(i);
  bool open=server.hasArg("aberto"+n);
  if(!server.hasArg("ssid"+n) || (!open && !server.hasArg("senha"+n))){message(400,"Formulário incompleto","Nenhuma configuração foi alterada. Envie todos os cinco perfis.",false);return;}
  String ssid=server.arg("ssid"+n),pass=server.arg("senha"+n);ssid.trim();
  if(ssid.length()>32){message(400,"Nome de rede inválido","O SSID deve ter até 32 bytes.",false);return;}
  if(ssid.isEmpty()){tmpS[i]="";tmpP[i]="";continue;}
  if(open)pass="";else if(pass.isEmpty() && ssid==oldS[i])pass=oldP[i];
  bool hex=pass.length()==64;
  if(hex)for(size_t j=0;j<pass.length();j++)if(!isxdigit((unsigned char)pass[j])){hex=false;break;}
  if(!pass.isEmpty() && !(pass.length()>=8 && pass.length()<=63) && !hex){message(400,"Senha inválida","Use de 8 a 63 bytes ou uma chave hexadecimal de 64 caracteres.",false);return;}
  if(pass.isEmpty() && !open && (ssid!=oldS[i] || !oldP[i].isEmpty())){message(400,"Informe a senha","Para uma rede sem senha, marque Rede aberta.",false);return;}
  tmpS[i]=ssid;tmpP[i]=pass;
 }
 if(!prefs.begin("wifi",false)){message(500,"Falha ao salvar","Não foi possível abrir as configurações. Tente novamente.",false);return;}
 bool ok=true;
 for(int i=0;i<MAX_REDES;i++) {
  String sk="ssid"+String(i),pk="pass"+String(i);
  prefs.putString(sk.c_str(),tmpS[i]);prefs.putString(pk.c_str(),tmpP[i]);
  ok=(prefs.isKey(sk.c_str()) && prefs.isKey(pk.c_str()) && prefs.getString(sk.c_str(),"!missing!")==tmpS[i] && prefs.getString(pk.c_str(),"!missing!")==tmpP[i]) && ok;
 }
 prefs.putInt("idx",-1);ok=(prefs.getInt("idx",-2)==-1) && ok;
 if(!ok) {
  bool restored=true;
  for(int i=0;i<MAX_REDES;i++) {
   String sk="ssid"+String(i),pk="pass"+String(i);
   prefs.putString(sk.c_str(),oldS[i]);prefs.putString(pk.c_str(),oldP[i]);
   restored=(prefs.isKey(sk.c_str()) && prefs.isKey(pk.c_str()) && prefs.getString(sk.c_str(),"!missing!")==oldS[i] && prefs.getString(pk.c_str(),"!missing!")==oldP[i]) && restored;
  }
  prefs.putInt("idx",oldIdx);restored=(prefs.getInt("idx",-2)==oldIdx) && restored;prefs.end();
  message(500,"Falha na gravação",restored?String("As configurações anteriores foram restauradas. Tente novamente."):String("Não foi possível restaurar todos os dados. Revise os perfis antes de reiniciar."),false);return;
 }
 prefs.end();
 message(200,"Redes salvas!","Os cinco perfis foram gravados e verificados. O dispositivo vai reiniciar em instantes.",true);
 restartAt=millis()+1500;pendingRestart=true;
}


public:
 void begin(){
  carregarRedes(ssids,passwords,lastUsed);if(prefs.begin("wifi",true)){cursor=prefs.getUChar("next",0)%5;prefs.end();}WiFi.persistent(false);WiFi.setHostname("ESP32-C6-Gateway");WiFi.mode(WIFI_AP_STA);WiFi.setAutoReconnect(false);openAP();
  server.on("/",HTTP_GET,[this](){if(estaConectado())handleInfo();else handleConfig();});
  server.on("/info",HTTP_GET,[this](){handleInfo();});server.on("/wifi",HTTP_GET,[this](){handleConfig();});
  server.on("/salvar",HTTP_POST,[this](){handleSalvar();});server.begin();iniciarUDP();display.setMonitor(&telemetry);
 }
 bool estaConectado()const{return WiFi.status()==WL_CONNECTED;}
 const NetworkMonitor& monitor()const{return telemetry;}
 void processarWebServer(){
  uint32_t now=millis();telemetry.update();server.handleClient();
  if(historyEnabled && (!historySeen || historyOnline!=telemetry.connected || (telemetry.hasResult && (!historyMeasured || historyPingAt!=telemetry.measuredAt))) && int32_t(now-historyNextTry)>=0){
   String event=String(now/1000)+"s ";
   if(!historySeen)event+="BOOT ";
   if(!historySeen || historyOnline!=telemetry.connected)event+=telemetry.connected?"WiFi conectado":"WiFi desconectado";
   else event+=telemetry.replied ? "PING "+String(telemetry.rtt)+" ms" : String("PING timeout");
   event+="\n";
   if(sd.getCardSizeBytes()>0){
    bool rotated=true;
    if(sd.getFileSize("/network_history.log")>=65536){
     if(sd.exists("/network_history.previous.log"))rotated=sd.removeFile("/network_history.previous.log");
     if(rotated)rotated=sd.renameFile("/network_history.log","/network_history.previous.log");
    }
    if(rotated && sd.appendText("/network_history.log",event)){historySeen=true;historyOnline=telemetry.connected;historyMeasured=telemetry.hasResult;historyPingAt=telemetry.measuredAt;historyFailed=false;}
    else {historyFailed=true;historyNextTry=now+60000;}
   }else {historyFailed=true;historyNextTry=now+60000;}
  }
  if(pendingRestart){if(int32_t(now-restartAt)>=0)ESP.restart();return;}
  if(estaConectado()){
   if(!wasOnline){wasOnline=true;phase=0;display.setConnecting("");display.println("IP: "+WiFi.localIP().toString());
    if(slot>=0 && prefs.begin("wifi",false)){prefs.putInt("idx",slot);prefs.end();lastUsed=slot;}
    if(apActive){WiFi.softAPdisconnect(false);apActive=false;}
   }return;
  }
  if(wasOnline){wasOnline=false;phase=0;display.println("WiFi desconectado");openAP();}
  if(phase==0){attempted=0;display.println("Buscando redes...");phaseAt=now;WiFi.scanNetworks(true,true);phase=1;}
  else if(phase==1){int scan=WiFi.scanComplete();if(scan==WIFI_SCAN_RUNNING && now-phaseAt<8000)return;WiFi.scanDelete();nextProfile();}
  else if(phase==2 && now-phaseAt>=C6NetworkConfig::WIFI_ATTEMPT_MS)nextProfile();
  else if(phase==3 && int32_t(now-retryAt)>=0)phase=0;
 }
 int obterFusoHorario(){if(!prefs.begin("wifi",true))return C6NetworkConfig::FUSO_PADRAO;int f=prefs.getInt("fuso",C6NetworkConfig::FUSO_PADRAO);prefs.end();return f;}
 bool obterDstAtivo(){if(!prefs.begin("wifi",true))return false;bool b=prefs.getBool("dst",false);prefs.end();return b;}
 bool salvarFusoHorario(int v){if(!prefs.begin("wifi",false))return false;prefs.putInt("fuso",v);bool ok=prefs.getInt("fuso",99)==v;prefs.end();return ok;}
 bool salvarDstAtivo(bool b){if(!prefs.begin("wifi",false))return false;prefs.putBool("dst",b);bool ok=prefs.isKey("dst") && prefs.getBool("dst",!b)==b;prefs.end();return ok;}
 void iniciarUDP(){udp.begin(C6NetworkConfig::UDP_PORT);}
 void iniciarPortal(){openAP();}
 void pararPortal(){if(apActive){WiFi.softAPdisconnect(false);apActive=false;}}
 void responderUDP(const String& s){if(!udp.remoteIP())return;udp.beginPacket(udp.remoteIP(),udp.remotePort());udp.print(s);udp.endPacket();}
 void responderUDPPrintf(const char* format,...){char buf[768];va_list args;va_start(args,format);vsnprintf(buf,sizeof(buf),format,args);va_end(args);responderUDP(String(buf));}
 bool checarMensagensUDP(String& s){int n=udp.parsePacket();if(!n)return false;char buf[256];
  if(n>=int(sizeof(buf))){while(udp.available())udp.read(buf,sizeof(buf));responderUDP("Erro: maximo 255 bytes\n");return false;}
  int len=udp.read(buf,sizeof(buf)-1);if(len<=0)return false;buf[len]=0;s=String(buf);return true;
 }
 void forcarReinicializacaoComLimpeza(){
  if(!prefs.begin("wifi",false)){responderUDP("Erro ao abrir configuracoes\n");return;}
  bool ok=prefs.clear();prefs.end();if(!ok){responderUDP("Erro ao limpar configuracoes\n");return;}
  responderUDP("Redes e configuracoes limpas. Reiniciando...\n");display.println("Configuracao limpa");rgbLed.off();pendingRestart=true;restartAt=millis()+1000;
 }
 bool historyOk()const{return !historyFailed;}
 void handleClient(){processarWebServer();}
 bool receiveCommand(String& s){return checarMensagensUDP(s);}
 void sendMessage(String s){responderUDP(s);}
 void clearConfig(){forcarReinicializacaoComLimpeza();}
 String savedSsid(uint8_t slot)const{return slot<5?ssids[slot]:String("");}
 uint8_t nextSlot()const{return cursor;}
 String monitorReport()const{
  const auto& m=telemetry;String s=m.connected?"WiFi ONLINE\n":"WiFi OFFLINE\n";
  s+="Quedas: "+String(m.drops)+"\nOffline: "+String((unsigned long)m.offlineSeconds())+" s\nGateway: ";
  s+=!m.connected?String("sem rede"):!m.hasResult?String("aguardando"):m.replied?String(m.rtt)+" ms":String("sem resposta");
  if(m.hasResult)s+=" | ha "+String(uint32_t(millis()-m.measuredAt)/1000)+" s";
  if(m.received)s+="\nRTT min/medio/max: "+String(m.minimum)+"/"+String((unsigned long)(m.totalRtt/m.received))+"/"+String(m.maximum)+" ms";
  s+="\nPings: "+String(m.attempts)+" | Sem resposta: "+String(m.failures)+"\n";
  s+=historyFailed ? "Historico SD: indisponivel ou falha\n" : "Historico SD: ativo\n";
  for(const auto& e:m.events)if(e.length())s+=e+"\n";return s;
 }
String listarSSIDs() {
 String ssids[MAX_REDES],senhas[MAX_REDES];int idx;
 carregarRedes(ssids,senhas,idx);String result;
 for(int i=0;i<MAX_REDES;i++)if(ssids[i].length())result+=ssids[i]+"\n";
 return result.length()?result:String("\n");
}
bool adicionarRede(const String& ssid,const String& senha,String& erro) {
 if(pendingRestart){erro="Reinicio em andamento";return false;}
 if(ssid.isEmpty() || ssid.length()>32 || ssid.indexOf('\n')>=0 || ssid.indexOf('\r')>=0){erro="SSID deve conter de 1 a 32 bytes";return false;}
 bool hex=senha.length()==64;
 if(hex)for(size_t i=0;i<senha.length();i++)if(!isxdigit((unsigned char)senha[i])){hex=false;break;}
 if(!senha.isEmpty() && !(senha.length()>=8 && senha.length()<=63) && !hex){erro="Senha: 8 a 63 bytes ou 64 caracteres hexadecimais";return false;}
 String ssids[MAX_REDES],senhas[MAX_REDES];int idx;
 carregarRedes(ssids,senhas,idx);
 if(!prefs.begin("wifi",false)){erro="Falha ao abrir configuracoes";return false;}
 uint8_t next=prefs.getUChar("next",0);if(next>=MAX_REDES)next=0;
 int slot=-1;for(int i=0;i<MAX_REDES;i++)if(ssids[i]==ssid){slot=i;break;}
 bool existing=slot>=0;if(!existing)slot=next;
 String sk="ssid"+String(slot),pk="pass"+String(slot);
 uint8_t newNext=existing?next:uint8_t((slot+1)%MAX_REDES);
 prefs.putString(sk.c_str(),ssid);prefs.putString(pk.c_str(),senha);prefs.putUChar("next",newNext);
 bool ok=prefs.isKey(sk.c_str()) && prefs.isKey(pk.c_str()) &&
   prefs.getString(sk.c_str())==ssid && prefs.getString(pk.c_str())==senha && prefs.getUChar("next",255)==newNext;
 if(!ok){
  prefs.putString(sk.c_str(),ssids[slot]);prefs.putString(pk.c_str(),senhas[slot]);prefs.putUChar("next",next);
  bool restored=prefs.isKey(sk.c_str()) && prefs.isKey(pk.c_str()) && prefs.getString(sk.c_str())==ssids[slot] && prefs.getString(pk.c_str())==senhas[slot] && prefs.getUChar("next",255)==next;
  erro=restored?"Falha ao gravar; perfil anterior restaurado":"Falha ao gravar e restaurar; revise os perfis";
 }
 prefs.end();if(ok){this->ssids[slot]=ssid;this->passwords[slot]=senha;cursor=newNext;}return ok;
}

};

