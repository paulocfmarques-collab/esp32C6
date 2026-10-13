#pragma once
#include "PortalUi.h"

static String logPortalPage() {
    String h = portalStart("Logs do SD", false, false, true);
    h += F(R"LOGPAGE(
<style>.log-controls{display:flex;gap:14px;align-items:center;flex-wrap:wrap}.log-controls select{font:inherit;background:var(--bg);color:var(--ink);border:1px solid var(--border);padding:10px;border-radius:9px;max-width:100%}.log-view{height:55vh;min-height:240px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:13px/1.65 ui-monospace,Consolas,monospace;background:#080f1b;padding:18px;border-radius:10px;border:1px solid var(--border)}.nav{flex-wrap:wrap}</style>
<div class='top'><div><div class='eyebrow'>Cartão SD</div><h1>Logs do dispositivo</h1><p class='muted'>Acompanhe os registros de comandos e conexão.</p></div><button id='refresh' type='button'>Atualizar agora</button></div>
<section class='card'><div class='log-controls'><label for='log-file'>Arquivo</label><select id='log-file'><option value='commands'>Comandos — log.txt</option><option value='network'>Rede — network_history.log</option><option value='previous'>Rede anterior — network_history.previous.log</option></select><label class='check'><input id='auto' type='checkbox' checked>Atualizar a cada 5 segundos</label><label class='check'><input id='follow' type='checkbox' checked>Acompanhar últimas linhas</label></div>
<p id='log-status' class='hint' role='status' aria-live='polite'>Carregando registros…</p><pre id='log-content' class='log-view' tabindex='0' aria-label='Conteúdo do log'></pre><p class='hint'>Exibe os registros mais recentes, até 8 KiB por consulta. Horários de rede indicam segundos desde a inicialização.</p></section>
<script>
const file=document.getElementById('log-file'), output=document.getElementById('log-content'), status=document.getElementById('log-status'), refresh=document.getElementById('refresh'), auto=document.getElementById('auto'), follow=document.getElementById('follow');
let busy=false, timer;
async function loadLog(){
 if(busy)return;
 busy=true;refresh.disabled=true;file.disabled=true;status.textContent='Lendo cartão…';
 const controller=new AbortController(), timeout=setTimeout(()=>controller.abort(),8000);
 try{
  const response=await fetch('/api/logs?file='+encodeURIComponent(file.value),{cache:'no-store',signal:controller.signal});
  const text=await response.text();
  if(!response.ok){output.textContent='';status.textContent=text;return;}
  output.textContent=text||'Este arquivo está vazio.';
  if(follow.checked)output.scrollTop=output.scrollHeight;
  status.textContent='Atualizado às '+new Date().toLocaleTimeString('pt-BR')+(response.headers.get('X-Log-Truncated')==='1'?' · Exibindo trecho final do arquivo.':'');
 }catch(error){status.textContent=error.name==='AbortError'?'A leitura demorou demais. Tente atualizar novamente.':'Não foi possível conectar à placa. Tente novamente.';}
 finally{clearTimeout(timeout);busy=false;refresh.disabled=false;file.disabled=false;clearTimeout(timer);if(auto.checked)timer=setTimeout(loadLog,5000);}
}
refresh.addEventListener('click',loadLog);file.addEventListener('change',()=>{output.textContent='';loadLog();});
auto.addEventListener('change',()=>{clearTimeout(timer);if(auto.checked)loadLog();});
loadLog();
</script>
)LOGPAGE");
    h += portalEnd();
    return h;
}
