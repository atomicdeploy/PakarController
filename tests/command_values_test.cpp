#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <initializer_list>
#include <ArduinoJson.h>
#include "CommandValues.h"
#include "CloudCommandGuard.h"

using namespace PakarCommand;
static JsonDocument parse(const char *json){JsonDocument d;assert(!deserializeJson(d,json));return d;}
static BoardCore::CloudCommand::Result admission(JsonObjectConst o){
  BoardCore::CloudCommand::Guard guard;
  assert(guard.setIdentity("esp32s3-907069eac800","123456789abcdef0123456789abcdef0"));
  const String command=stringValue(o,"command");
  return guard.admit(command.c_str(),nullptr,nullptr,nullptr,0,1792250000U,0);
}
int main(){
  using Result=BoardCore::CloudCommand::Result;
  auto nested=parse("{\"meta\":{\"command\":\"cell\",\"id\":80,\"state\":\"off\",\"value\":99,\"enabled\":true},\"command\":\"status\",\"id\":1,\"state\":\"red\",\"enabled\":false}");
  auto o=nested.as<JsonObjectConst>();
  assert(stringValue(o,"command")=="status"&&admission(o)==Result::Allowed);
  assert(integerValue(o,"id",-1)==1&&stringValue(o,"state")=="red");
  assert(!booleanValue(o,"enabled")&&integerValue(o,"value",-7)==-7);
  assert(stringValue(o,"unknown").empty());
  auto onlyNested=parse("{\"meta\":{\"command\":\"status\",\"id\":80,\"state\":\"red\",\"enabled\":true}}");
  o=onlyNested.as<JsonObjectConst>();
  assert(stringValue(o,"command").empty()&&admission(o)!=Result::Allowed);
  assert(integerValue(o,"id",-1)==-1&&stringValue(o,"state").empty()&&!booleanValue(o,"enabled"));

  auto duplicate=parse("{\"command\":\"cell\",\"command\":\"status\",\"id\":80,\"id\":1,\"state\":\"off\",\"state\":\"red\",\"enabled\":true,\"enabled\":false}");
  o=duplicate.as<JsonObjectConst>();
  assert(stringValue(o,"command")=="status"&&admission(o)==Result::Allowed);
  assert(integerValue(o,"id",-1)==1&&stringValue(o,"state")=="red"&&!booleanValue(o,"enabled"));
  auto reverse=parse("{\"command\":\"status\",\"command\":\"cell\"}");
  assert(stringValue(reverse.as<JsonObjectConst>(),"command")=="cell");
  assert(admission(reverse.as<JsonObjectConst>())==Result::MissingEnvelope);
  auto escaped=parse("{\"comm\\u0061nd\":\"sta\\u0074us\",\"text\":\"Hello\\nworld\"}");
  assert(stringValue(escaped.as<JsonObjectConst>(),"command")=="status");
  assert(stringValue(escaped.as<JsonObjectConst>(),"text")=="Hello\nworld");
  assert(admission(escaped.as<JsonObjectConst>())==Result::Allowed);
  for(const char *json:{"{\"command\":\"status\\u0000cell\"}","{\"command\":true}","{\"command\":1}","{\"command\":null}","{\"command\":{\"value\":\"status\"}}"}){
    auto d=parse(json);o=d.as<JsonObjectConst>();
    assert(stringValue(o,"command").empty()&&admission(o)!=Result::Allowed);
  }
  for(const char *json:{"{\"id\":\"1junk\"}","{\"id\":\"1\"}","{\"id\":1.5}","{\"id\":true}","{\"id\":null}","{\"id\":[]}"}){
    auto d=parse(json);assert(integerValue(d.as<JsonObjectConst>(),"id",-777)==-777);
  }
  JsonDocument bounds;bounds["low"]=LONG_MIN;bounds["high"]=LONG_MAX;
  assert(integerValue(bounds.as<JsonObjectConst>(),"low",0)==LONG_MIN);
  assert(integerValue(bounds.as<JsonObjectConst>(),"high",0)==LONG_MAX);
  bounds["overflow"]=static_cast<uint64_t>(LONG_MAX)+1U;
  assert(integerValue(bounds.as<JsonObjectConst>(),"overflow",-777)==-777);
  for(const char *json:{"{\"enabled\":1}","{\"enabled\":\"true\"}","{\"enabled\":\"1\"}","{\"enabled\":null}","{\"enabled\":{\"value\":true}}"}){
    auto d=parse(json);assert(!booleanValue(d.as<JsonObjectConst>(),"enabled"));
  }
  auto yes=parse("{\"enabled\":true}");assert(booleanValue(yes.as<JsonObjectConst>(),"enabled"));
  JsonObjectConst absent;
  assert(stringValue(absent,"command").empty()&&integerValue(absent,"id",-1)==-1&&!booleanValue(absent,"enabled"));
  puts("Command values: actual typed top-level helpers agree with cloud admission; nested/duplicate/escaped-NUL/type confusion fixtures passed.");
}
