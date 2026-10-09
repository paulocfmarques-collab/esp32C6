#line 1 "C:\\PC\\ESP32\\ESP32C6\\ClimaManager.h"
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <math.h>
#include <stdlib.h>
struct ClimaSnapshot {
  float temperatura=0,minima=0,maxima=0;int codigoCondicao=0,chuva=0;
  bool sincronizado=false,previsaoValida=false,ultimaFalhou=false;
  uint32_t ultimaAtualizacao=0,previsaoAtualizada=0;char data[11]={0};
 };

class ClimaManager {
public:
 using Snapshot=ClimaSnapshot;
private:
 inline static Snapshot data;
 inline static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
 inline static bool busy=false;
 inline static uint32_t attempted=0;
 static bool number(const String& block,const char* key,bool array,float& result){
  int pos=block.indexOf(String("\"")+key+"\":");if(pos<0)return false;
  pos+=strlen(key)+3;if(array){pos=block.indexOf('[',pos);if(pos<0)return false;pos++;}
  int end=pos;while(end<int(block.length()) && block[end]!=',' && block[end]!='}' && block[end]!=']')end++;
  String token=block.substring(pos,end);token.trim();char* tail=nullptr;float value=strtof(token.c_str(),&tail);
  if(tail==token.c_str() || *tail || !isfinite(value))return false;result=value;return true;
 }
 static void worker(void*){
  Snapshot next=snapshot();bool currentOk=false,dailyOk=false;
  NetworkClientSecure client;client.setInsecure();
  HTTPClient http;http.setConnectTimeout(3000);http.setTimeout(3000);http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const char* url="https://api.open-meteo.com/v1/forecast?latitude=-30.03&longitude=-51.23&current=temperature_2m,weather_code&daily=temperature_2m_min,temperature_2m_max,precipitation_probability_max&forecast_days=1&timezone=America%2FSao_Paulo";
  if(http.begin(client,url) && http.GET()==HTTP_CODE_OK){
   String payload=http.getString();int pos=payload.indexOf("\"current\":");float t,w;
   if(pos>=0){String block=payload.substring(pos,payload.indexOf('}',pos)+1);
    if(number(block,"temperature_2m",false,t) && number(block,"weather_code",false,w)){
     next.temperatura=t;next.codigoCondicao=int(w);next.sincronizado=true;next.ultimaAtualizacao=millis();currentOk=true;
    }
   }
   pos=payload.indexOf("\"daily\":");float low,high,rain;
   if(pos>=0){String block=payload.substring(pos);int time=block.indexOf("\"time\":");int start=time>=0?block.indexOf('[',time):-1;int quote=start>=0?block.indexOf('"',start):-1;
    if(quote>=0 && number(block,"temperature_2m_min",true,low) && number(block,"temperature_2m_max",true,high) && number(block,"precipitation_probability_max",true,rain) && low<=high && rain>=0 && rain<=100){
     String date=block.substring(quote+1,quote+11);
     if(date.length()==10 && date[4]=='-' && date[7]=='-'){
      next.minima=low;next.maxima=high;next.chuva=int(rain);next.previsaoValida=true;next.previsaoAtualizada=millis();date.toCharArray(next.data,sizeof(next.data));dailyOk=true;
     }
    }
   }
  }
  http.end();next.ultimaFalhou=!(currentOk && dailyOk);
  portENTER_CRITICAL(&mux);data=next;busy=false;portEXIT_CRITICAL(&mux);vTaskDelete(nullptr);
 }
public:
 static Snapshot snapshot(){portENTER_CRITICAL(&mux);Snapshot copy=data;portEXIT_CRITICAL(&mux);return copy;}
 static void atualizar(bool force=false){
  if(WiFi.status()!=WL_CONNECTED)return;uint32_t now=millis();
  portENTER_CRITICAL(&mux);uint32_t interval=(data.sincronizado && data.previsaoValida && !data.ultimaFalhou)?900000:60000;
  bool start=!busy && (force || attempted==0 || now-attempted>=interval);if(start){busy=true;attempted=now;}portEXIT_CRITICAL(&mux);
  if(start && xTaskCreate(worker,"weather",8192,nullptr,1,nullptr)!=pdPASS){portENTER_CRITICAL(&mux);busy=false;data.ultimaFalhou=true;portEXIT_CRITICAL(&mux);}
 }
 static String obterTextoCondicao(){int code=snapshot().codigoCondicao;
  if(code==0)return "Ceu limpo";if(code<=3)return "Parc. nublado";if(code>=45 && code<=48)return "Nevoeiro";
  if(code>=51 && code<=67)return "Chuva / garoa";if(code>=71 && code<=77)return "Neve";if(code>=80 && code<=82)return "Pancadas";
  if(code>=95)return "Tempestade";return "Nublado";
 }
};
