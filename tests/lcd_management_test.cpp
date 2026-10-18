#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LcdDisplay.h>
#include <cassert>
#include <map>
#include <functional>
#include <cstdio>
struct LcdBus:BoardCore::LcdDisplay::Bus {
  bool probe(uint8_t a)override{return a==0x27;}
  bool write(uint8_t,uint8_t)override{return true;}
} lcdBus;
struct LcdPortFixture {
  BoardCore::LcdDisplay value{lcdBus};
  bool enabled=true,up=true;unsigned presents=0,rescans=0,releases=0;
  bool configured()const{return enabled;}
  bool started()const{return up;}
  bool hostActive()const{return presents>releases;}
  const auto &display()const{return value;}
  bool present(const char *one,const char *two,uint32_t hold,uint32_t){
    assert(hold>=1&&hold<=60000);if(!value.available())return false;
    ++presents;value.setText(one,two);return true;
  }
  void release(){++releases;value.setText("ledboard","Cell 1 white");}
  void rescan(uint32_t now){++rescans;value.rescan(now*1000U);}
} lcdPort;
namespace ConfigurationManagement {
bool maintenance=false;
bool busy(){return maintenance;}
String json(const JsonDocument &d){String value;serializeJson(d,value);return value;}
}
constexpr int HTTP_GET=1,HTTP_POST=2;
struct Server {
  std::map<std::pair<std::string,int>,std::function<void()>> routes;
  std::map<std::string,String> args;int status=0;String body;
  void on(const char *p,int method,std::function<void()> handler){routes[{p,method}]=handler;}
  void send(int code,const char *,const String &value){status=code;body=value;}
  String arg(const char *key){return args.count(key)?args[key]:String();}
  bool hasArg(const char *key){return args.count(key);}
  void invoke(int method){status=0;body.clear();routes.at({"/api/display/lcd",method})();}
} server;
bool allowed=true;
bool authenticate(){if(!allowed)server.send(401,"text/plain","fixture denied");return allowed;}
#include "../firmware/esp8266/src/LcdManagement.h"
JsonDocument read(){JsonDocument d;assert(!deserializeJson(d,server.body));return d;}
int main(){
  registerLcdApi();server.invoke(HTTP_GET);assert(server.status==200);
  assert(read()["available"]==false&&read()["lines"].isNull());
  server.args={{"action","present"},{"line1","HELLO"}};
  server.invoke(HTTP_POST);assert(server.status==503&&lcdPort.presents==0);
  lcdPort.value.begin(0);for(uint32_t now=0;now<150000;now+=1000)lcdPort.value.service(now);
  server.invoke(HTTP_POST);assert(server.status==202&&lcdPort.presents==1);
  assert(read()["pending"]==true); // Admission does not claim a completed frame.
  for(uint32_t now=150000;now<300000;now+=1000)lcdPort.value.service(now);
  server.invoke(HTTP_GET);assert(read()["pending"]==false&&read()["lines"][0]=="HELLO           ");
  const auto prior=lcdPort.presents;
  for(const char *invalid:{"0","60001","-1","1x","999999",""}) {
    server.args["hold_ms"]=invalid;server.invoke(HTTP_POST);assert(server.status==400&&lcdPort.presents==prior);
  }
  server.args.erase("hold_ms");server.args["line1"]="more than sixteen characters";
  server.invoke(HTTP_POST);assert(server.status==400);
  server.args["line1"]="\x01";server.invoke(HTTP_POST);assert(server.status==400);
  server.args["line1"]="safe";ConfigurationManagement::maintenance=true;
  server.invoke(HTTP_POST);assert(server.status==409&&lcdPort.presents==prior);
  ConfigurationManagement::maintenance=false;allowed=false;
  server.invoke(HTTP_POST);assert(server.status==401&&lcdPort.presents==prior);
  allowed=true;server.args={{"action","release"}};server.invoke(HTTP_POST);
  assert(server.status==202&&lcdPort.releases==1);
  server.args={{"action","rescan"}};server.invoke(HTTP_POST);
  assert(server.status==202&&lcdPort.rescans==1&&read()["lines"].isNull()&&read()["detection_known"]==false);
  lcdPort.enabled=false;server.invoke(HTTP_POST);assert(server.status==503&&lcdPort.rescans==1);
  puts("Actual LCD HTTP handlers/status: typed bounds, auth/maintenance, admission vs completed-frame, release/rescan and absent-module cases passed with transport/bus doubles.");
}
