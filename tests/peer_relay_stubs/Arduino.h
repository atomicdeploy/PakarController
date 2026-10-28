#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <map>
#include <cstdio>
#define F(value) value
inline uint32_t testNow=100;
inline uint32_t millis(){return testNow;}
template<class T>T min(T a,T b){return std::min(a,b);}
class String:public std::string {
 public:
  using std::string::string;
  String()=default;String(const std::string &s):std::string(s){}
  String& operator=(const char *s){assign(s?s:"");return *this;}
  bool isEmpty()const{return empty();}
  bool endsWith(const char *s)const{const size_t n=strlen(s);return size()>=n&&compare(size()-n,n,s)==0;}
  String substring(size_t start,size_t end=std::string::npos)const{return substr(start,end==std::string::npos?end:end-start);}
  bool reserve(size_t n){std::string::reserve(n);return true;}
  bool concat(const char *s,size_t n){append(s,n);return true;}
  size_t write(uint8_t c){push_back(char(c));return 1;}
  size_t write(const uint8_t *p,size_t n){append(reinterpret_cast<const char*>(p),n);return n;}
  void toCharArray(char *out,size_t n)const{if(n){strncpy(out,c_str(),n-1);out[n-1]=0;}}
};
using TaskHandle_t=void*;
inline void (*testWorker)(void*)=nullptr;inline void *testWorkerArg=nullptr;
inline unsigned taskCreates=0,taskNotifications=0;inline bool workerTaken=false,taskCreationFails=false;
constexpr int pdPASS=1,pdTRUE=1;constexpr uint32_t portMAX_DELAY=UINT32_MAX;
inline int xTaskCreate(void (*fn)(void*),const char*,unsigned,void *arg,int,TaskHandle_t *handle){
  if(taskCreationFails)return 0;
  ++taskCreates;testWorker=fn;testWorkerArg=arg;*handle=arg;return pdPASS;
}
inline void xTaskNotifyGive(TaskHandle_t){++taskNotifications;}
inline unsigned ulTaskNotifyTake(int,uint32_t){if(workerTaken)throw std::runtime_error("fixture-idle");workerTaken=true;return 1;}
inline void vTaskDelay(unsigned ticks){testNow+=ticks;}
inline void runRelayWorker(){workerTaken=false;try{testWorker(testWorkerArg);}catch(const std::runtime_error &e){if(std::string(e.what())!="fixture-idle")throw;}}
