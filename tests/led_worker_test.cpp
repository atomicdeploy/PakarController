// Compile the ACTUAL S3 output worker; only SDK/FreeRTOS peripherals are doubled.
#include "../firmware/esp32s3/src/LedOutput.h"
#include "../firmware/esp32s3/src/LedWire.h"
#include "../firmware/esp32s3/src/LedDmaGuard.h"
#include <esp_heap_caps.h>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <driver/gpio.h>
#if !PAKAR_S3_LED_USE_DMA
#include <soc/gpio_reg.h>
#endif

namespace Fixture {
using Clock=std::chrono::steady_clock;
struct Stop{};
std::atomic<bool> stopping{false},finished{false};
std::thread worker;
std::mutex mutex;
std::condition_variable changed;
bool live=false,block=false,timeout=false,disableFails=false,blockDisable=false,shortCompletion=false;
unsigned stops=0;
unsigned latches=0;
rmt_bytes_encoder_config_t byteCoding{};
rmt_tx_event_callbacks_t callbacks{};void *callbackOwner=nullptr;
unsigned blockGeneration=0,blockedGeneration=0;
const void* buffer=nullptr;
std::vector<uint8_t> snapshot;
std::vector<std::vector<uint8_t>> frames;
std::vector<void*> allocations;
uint32_t cycle=0xfffffff0U,highAt=0,lastRise=0;
unsigned bitCount=0;
PakarS3::LedOutput* owner=nullptr;
std::atomic<unsigned> producerCopies{0};
std::atomic<unsigned> pinCalls{0},channelCreates{0},allocationCalls{0};
unsigned pinFailure=0;
std::atomic<bool> validateProducers{false};
std::vector<uint8_t> staticBytes;
template<class Predicate>void until(Predicate predicate){
 const auto deadline=Clock::now()+std::chrono::seconds(2);
 while(!predicate()){assert(Clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
unsigned count(){std::lock_guard<std::mutex> lock(mutex);return static_cast<unsigned>(frames.size());}
bool isLive(){std::lock_guard<std::mutex> lock(mutex);return live;}
void setBlock(bool value){std::lock_guard<std::mutex> lock(mutex);block=value;if(value)++blockGeneration;changed.notify_all();}
void waitBlocked(){until([]{std::lock_guard<std::mutex> lock(mutex);return live&&block&&blockedGeneration==blockGeneration;});}
uint32_t field(const PakarS3::LedOutput& output,const char* name){
 const auto json=output.json();const auto pos=json.find(std::string("\"")+name+"\":");assert(pos!=std::string::npos);
 return static_cast<uint32_t>(std::stoul(json.substr(json.find(':',pos)+1)));
}
void cleanup(){stopping=true;changed.notify_all();if(worker.joinable())worker.join();for(auto* p:allocations)std::free(p);allocations.clear();finished=true;}
}
QueueHandle_t xQueueCreateStatic(unsigned capacity,size_t bytes,uint8_t*,StaticQueue_t* state){assert(capacity==1);state->bytes.resize(bytes);return state;}
int xQueueOverwrite(QueueHandle_t queue,const void* frame){
 const auto base=reinterpret_cast<uintptr_t>(Fixture::owner),source=reinterpret_cast<uintptr_t>(frame);
 assert(Fixture::owner&&source>=base&&source+queue->bytes.size()<=base+sizeof(PakarS3::LedOutput));
 if(Fixture::validateProducers){const auto* pixels=static_cast<const CRGB*>(frame);const auto value=pixels[0].r;
  assert(value==17||value==93);std::this_thread::yield();
  for(size_t i=0;i<480;++i)assert(pixels[i].r==value&&pixels[i].g==value&&pixels[i].b==value);
 }
 ++Fixture::producerCopies; // Actual submit must pass owned member storage, never a local Frame.
 std::lock_guard<std::mutex> lock(queue->mutex);std::memcpy(queue->bytes.data(),frame,queue->bytes.size());queue->full=true;queue->changed.notify_one();return pdTRUE;
}
int xQueueReceive(QueueHandle_t queue,void* frame,TickType_t delay){
 std::unique_lock<std::mutex> lock(queue->mutex);
 if(!queue->full&&delay)queue->changed.wait_for(lock,std::chrono::milliseconds(delay),[&]{return queue->full||Fixture::stopping.load();});
 if(Fixture::stopping)throw Fixture::Stop{};
 if(!queue->full)return 0;
 std::memcpy(frame,queue->bytes.data(),queue->bytes.size());queue->full=false;return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t queue){std::lock_guard<std::mutex> lock(queue->mutex);return queue->full?1:0;}
int xTaskCreatePinnedToCore(void(*run)(void*),const char*,unsigned bytes,void* context,unsigned priority,void*,int core){
 assert(core==1&&bytes==4096&&priority>=2);assert(!Fixture::worker.joinable());
 Fixture::owner=static_cast<PakarS3::LedOutput*>(context);
 Fixture::worker=std::thread([=]{try{run(context);}catch(const Fixture::Stop&){} });return pdPASS;
}
void vTaskDelay(TickType_t delay){if(Fixture::stopping)throw Fixture::Stop{};std::this_thread::sleep_for(std::chrono::milliseconds(delay));if(Fixture::stopping)throw Fixture::Stop{};}
void vTaskDelete(void*){throw Fixture::Stop{};}
void* heap_caps_malloc(size_t bytes,int caps){++Fixture::allocationCalls;assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));assert(bytes==1440);auto* p=std::malloc(bytes);Fixture::allocations.push_back(p);return p;}
void heap_caps_free(void* pointer){for(auto& p:Fixture::allocations)if(p==pointer)p=nullptr;std::free(pointer);}
int rmt_new_tx_channel(const rmt_tx_channel_config_t* config,rmt_channel_handle_t* channel){assert(Fixture::pinCalls==3);++Fixture::channelCreates;assert(config->flags.with_dma&&config->mem_block_symbols==2046&&config->intr_priority==3&&config->resolution_hz==40000000&&config->trans_queue_depth==1&&config->gpio_num==4);*channel=reinterpret_cast<void*>(1);return ESP_OK;}
namespace PakarS3 {
int newCacheSafeLedChannel(const rmt_tx_channel_config_t *config,rmt_channel_handle_t *channel){return rmt_new_tx_channel(config,channel);}
LedDmaIrqReceipt ledDmaIrqReceipt(){return {PAKAR_S3_LED_USE_DMA!=0,42,1,23,3};} // Actual allocator is tested separately.
}
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t* encoder){*encoder=reinterpret_cast<void*>(2);return ESP_OK;}
int rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t* config,rmt_encoder_handle_t* encoder){
 assert(config->flags.msb_first==1);
 assert(config->bit0.level0==1&&config->bit0.level1==0&&config->bit0.duration0==10&&config->bit0.duration1==40);
 assert(config->bit1.level0==1&&config->bit1.level1==0&&config->bit1.duration0==24&&config->bit1.duration1==26);
 Fixture::byteCoding=*config;*encoder=reinterpret_cast<void*>(3);return ESP_OK;
}
int rmt_enable(rmt_channel_handle_t){return ESP_OK;}
int rmt_tx_register_event_callbacks(rmt_channel_handle_t,const rmt_tx_event_callbacks_t *callbacks,void *owner){Fixture::callbacks=*callbacks;Fixture::callbackOwner=owner;return ESP_OK;}
int rmt_del_channel(rmt_channel_handle_t){return ESP_OK;}
int rmt_del_encoder(rmt_encoder_handle_t){return ESP_OK;}
int rmt_transmit(rmt_channel_handle_t,rmt_encoder_handle_t encoder,const void* source,size_t bytes,const rmt_transmit_config_t* tx){
 std::lock_guard<std::mutex> lock(Fixture::mutex);assert(!Fixture::live&&tx->loop_count==0&&tx->flags.eot_level==0&&tx->flags.queue_nonblocking);
 assert(encoder==reinterpret_cast<void*>(3)&&bytes==480*3);Fixture::live=true;Fixture::buffer=source;
 Fixture::snapshot.assign(static_cast<const uint8_t*>(source),static_cast<const uint8_t*>(source)+bytes);
 // Model the SDK bytes encoder's MSB-first mapping. The source is compact and
 // remains immutable while an ISR may refill either half of the DMA ring.
 std::vector<rmt_symbol_word_t> symbols;symbols.reserve(bytes*8);
 for(uint8_t value:Fixture::snapshot)for(uint8_t mask=0x80;mask;mask>>=1)
  symbols.push_back(value&mask?Fixture::byteCoding.bit1:Fixture::byteCoding.bit0);
 const auto* encoded=reinterpret_cast<const uint8_t*>(symbols.data());
 Fixture::frames.emplace_back(encoded,encoded+symbols.size()*sizeof(symbols[0]));Fixture::changed.notify_all();return ESP_OK;
}
int rmt_tx_wait_all_done(rmt_channel_handle_t,int deadline){
 assert(deadline==100);std::unique_lock<std::mutex> lock(Fixture::mutex);
 if(Fixture::block){Fixture::blockedGeneration=Fixture::blockGeneration;Fixture::changed.notify_all();}
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::block||Fixture::stopping.load();});
 assert(!Fixture::block||Fixture::stopping);if(Fixture::stopping)throw Fixture::Stop{};
 assert(std::memcmp(Fixture::buffer,Fixture::snapshot.data(),Fixture::snapshot.size())==0);
 if(Fixture::timeout)return ESP_ERR_TIMEOUT;
 lock.unlock();std::this_thread::sleep_for(std::chrono::milliseconds(2));lock.lock();
 assert(std::memcmp(Fixture::buffer,Fixture::snapshot.data(),Fixture::snapshot.size())==0);Fixture::live=false;
 rmt_tx_done_event_data_t done{Fixture::shortCompletion?480*24U:480*24U+1};assert(Fixture::callbacks.on_trans_done);Fixture::callbacks.on_trans_done(reinterpret_cast<void*>(1),&done,Fixture::callbackOwner);return ESP_OK;
}
int rmt_disable(rmt_channel_handle_t){
 std::unique_lock<std::mutex> lock(Fixture::mutex);++Fixture::stops;Fixture::changed.notify_all();
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::blockDisable||Fixture::stopping.load();});
 assert(!Fixture::blockDisable||Fixture::stopping);
 if(Fixture::disableFails)return ESP_ERR_TIMEOUT;
 Fixture::live=false;return ESP_OK;
}
void esp_rom_delay_us(unsigned micros){
 assert(micros==PakarS3::resetMicros);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);assert(!Fixture::live);++Fixture::latches;}
 std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void concurrentProducers(PakarS3::LedOutput& output);
void asymmetricChannels(PakarS3::LedOutput& output);
int pinStep(unsigned expected){assert(Fixture::pinCalls.load()==expected);const auto step=++Fixture::pinCalls;return step==Fixture::pinFailure?ESP_ERR_TIMEOUT:ESP_OK;}
int gpio_reset_pin(gpio_num_t pin){assert(pin==4);return pinStep(0);}
int gpio_set_level(gpio_num_t pin,int value){assert(pin==4&&value==0);return pinStep(1);}
int gpio_set_direction(gpio_num_t pin,int mode){assert(pin==4&&mode==GPIO_MODE_OUTPUT);return pinStep(2);}
void pinSetupFailure(unsigned step){
 Fixture::pinFailure=step;PakarS3::LedOutput output;assert(output.begin());
 Fixture::until([&]{return Fixture::field(output,"error_code")==ESP_ERR_TIMEOUT;});
 Fixture::cleanup();
 assert(Fixture::pinCalls==step&&Fixture::channelCreates==0&&Fixture::allocationCalls==0&&Fixture::frames.empty());
 assert(output.json().find("\"ready\":false")!=std::string::npos);
 assert(Fixture::field(output,"transmitted")==0&&Fixture::field(output,"failed")==0);
 assert(PakarS3::LedOutput::acquireFlashPause());PakarS3::LedOutput::releaseFlashPause();
 puts("Actual worker GPIO initialization: reset/LOW preload/output order; each failed step stops before allocation/RMT/TX; no false black-frame or ready receipt.");
}
#if !PAKAR_S3_LED_USE_DMA
void fixtureCriticalEnter(){
 assert(Fixture::pinCalls==3);
 std::unique_lock<std::mutex> lock(Fixture::mutex);assert(!Fixture::live);Fixture::live=true;
 if(Fixture::block){Fixture::blockedGeneration=Fixture::blockGeneration;Fixture::changed.notify_all();}
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::block||Fixture::stopping.load();});
 assert(!Fixture::block||Fixture::stopping);if(Fixture::stopping)throw Fixture::Stop{};
 Fixture::bitCount=0;Fixture::staticBytes.assign(1440,0);
}
void fixtureGpioWrite(unsigned address,unsigned value){
 assert(value==(1U<<4));
 if(address==GPIO_OUT_W1TS_REG){
  if(Fixture::bitCount)assert(static_cast<uint32_t>(Fixture::cycle-Fixture::lastRise)>=300U);
  Fixture::lastRise=Fixture::highAt=Fixture::cycle;
 }else{
  assert(address==GPIO_OUT_W1TC_REG);
  const uint32_t high=Fixture::cycle-Fixture::highAt;assert(high==60U||high==144U);
  if(high==144U)Fixture::staticBytes.at(Fixture::bitCount/8)|=static_cast<uint8_t>(0x80U>>(Fixture::bitCount%8));
  ++Fixture::bitCount;
 }
}
void fixtureCriticalExit(){
 std::lock_guard<std::mutex> lock(Fixture::mutex);assert(Fixture::bitCount==480*24);
 Fixture::frames.push_back(Fixture::staticBytes);Fixture::live=false;Fixture::changed.notify_all();
}
void staticFunctional(){
 PakarS3::LedOutput output;assert(output.begin());Fixture::until([&]{return Fixture::count()==1;});
 {std::lock_guard<std::mutex> lock(Fixture::mutex);for(auto b:Fixture::frames.front())assert(b==0);}
 CRGB rgb[480];for(auto& p:rgb)p={255,0,0};Fixture::setBlock(true);output.submit(rgb,255,true,1);Fixture::waitBlocked();
 for(uint32_t i=2;i<=2000;++i){for(auto& p:rgb)p={static_cast<uint8_t>(i),0,0};output.submit(rgb,255,true,i);}
 assert(!output.canAccept());std::atomic<bool> acquired{false};
 std::thread flash([&]{assert(PakarS3::LedOutput::acquireFlashPause());acquired=true;});
 std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(!acquired);Fixture::setBlock(false);flash.join();
 assert(acquired&&!Fixture::isLive());const auto paused=Fixture::count();
 assert(PakarS3::LedOutput::acquireFlashPause());PakarS3::LedOutput::releaseFlashPause();
 std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(Fixture::count()==paused);
 PakarS3::LedOutput::releaseFlashPause();Fixture::until([&]{return Fixture::field(output,"revision")==2000;});
 const auto stable=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(Fixture::count()==stable);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);for(size_t p=0;p<480;++p){const auto& f=Fixture::frames.back();
   for(size_t c=0;c<3;++c)assert(f[p*3+c]==(c==(PAKAR_S3_LED_SWAP_RED_BLUE?2U:0U)?static_cast<uint8_t>(2000):0));}}
 Fixture::setBlock(true);output.submit(rgb,255,true,2001);Fixture::waitBlocked();
 assert(!PakarS3::LedOutput::acquireFlashPause());Fixture::setBlock(false);Fixture::until([&]{return Fixture::field(output,"revision")==2001;});
 output.submit(rgb,255,false,2002);Fixture::until([&]{return Fixture::field(output,"revision")==2002;});
 const auto off=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(Fixture::count()==off);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);for(auto b:Fixture::frames.back())assert(b==0);}
 concurrentProducers(output);asymmetricChannels(output);
 Fixture::cleanup();puts("Actual static worker: stackless serialized member submission,startup black, full480 RGB encoding/cycle-wrap timing,2000 mailbox coalescing, no unchanged refresh, Off black, frame-boundary/nested holds and timeout rejection passed.");
}
#endif

void concurrentProducers(PakarS3::LedOutput& output){
 assert(PakarS3::LedOutput::acquireFlashPause());
 std::atomic<bool> start{false};
 auto producer=[&](uint8_t channel){CRGB frame[480];for(auto& pixel:frame)pixel={channel,channel,channel};
  while(!start.load())std::this_thread::yield();
  for(uint32_t i=0;i<100;++i)output.submit(frame,channel,true,4000U+uint32_t(channel)*100U+i);
 };
 Fixture::validateProducers=true;
 std::thread a(producer,17),b(producer,93);start=true;a.join();b.join();Fixture::validateProducers=false;
 CRGB final[480]{};output.submit(final,255,false,9999);
 PakarS3::LedOutput::releaseFlashPause();Fixture::until([&]{return Fixture::field(output,"revision")==9999;});
 assert(Fixture::producerCopies.load()>=2200);
}

// Upstream PR6 corrects accidental GRB packing. Exercise ACTUAL workers using
// all three unequal channels through physical pixel480, preserving our explicit
// RGB/BGR build selection and exactly-once brightness/correction.
void asymmetricChannels(PakarS3::LedOutput& output){
 CRGB rgb[480];for(size_t i=0;i<480;++i)rgb[i]={uint8_t(17*i+1),uint8_t(37*i+23),uint8_t(71*i+127)};
 output.submit(rgb,211,true,10000);Fixture::until([&]{return Fixture::field(output,"revision")==10000;});
 std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.back();
 const uint8_t correction[]={255,176,240};
 for(size_t p=0;p<480;++p)for(size_t c=0;c<3;++c){
  const uint8_t logical[]={rgb[p].r,rgb[p].g,rgb[p].b};const size_t index=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
  const uint8_t expected=uint8_t((uint32_t(logical[index])*211*correction[index]+32512U)/65025U);
#if PAKAR_S3_LED_USE_DMA
  for(size_t bit=0;bit<8;++bit){rmt_symbol_word_t symbol;
   std::memcpy(&symbol,bytes.data()+((p*3+c)*8+bit)*sizeof(symbol),sizeof(symbol));
   const bool one=expected&(0x80U>>bit);
   assert(symbol.duration0==(one?PakarS3::oneHigh:PakarS3::zeroHigh));
   assert(symbol.duration1==(one?PakarS3::oneLow:PakarS3::zeroLow));
   assert(symbol.level0==1&&symbol.level1==0);
  }
#else
  assert(bytes.at(p*3+c)==expected);
#endif
 }
}

void functional(){
 PakarS3::LedOutput output;assert(output.begin());CRGB rgb[480];for(auto& p:rgb)p={255,0,0};
 Fixture::until([&]{return Fixture::count()==1;});
 Fixture::setBlock(true);output.submit(rgb,255,true,1);Fixture::waitBlocked();
 std::thread producer([&]{for(uint32_t i=2;i<=2000;++i){for(auto& p:rgb)p={static_cast<uint8_t>(i),0,0};output.submit(rgb,255,true,i);} });producer.join();
 assert(Fixture::count()==2);assert(!output.canAccept());
 std::atomic<bool> acquired{false};std::thread waiter([&]{assert(PakarS3::LedOutput::acquireFlashPause());acquired=true;});
 std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(!acquired);
 Fixture::setBlock(false);waiter.join();assert(acquired&&!Fixture::isLive());
 assert(PakarS3::LedOutput::acquireFlashPause());const auto held=Fixture::count();
 std::thread nested([]{assert(PakarS3::LedOutput::acquireFlashPause());PakarS3::LedOutput::releaseFlashPause();});nested.join();
 PakarS3::LedOutput::releaseFlashPause();std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(Fixture::count()==held);
 PakarS3::LedOutput::releaseFlashPause();Fixture::until([&]{return Fixture::field(output,"revision")==2000;});
 const auto steady=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(30));
 assert(Fixture::count()==steady);assert(Fixture::field(output,"submitted")==2000);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.back();
  assert(bytes.size()==480*24*sizeof(rmt_symbol_word_t));
  for(size_t pixel=0;pixel<480;++pixel)for(size_t bit=0;bit<24;++bit){
   rmt_symbol_word_t symbol;std::memcpy(&symbol,bytes.data()+(pixel*24+bit)*sizeof(symbol),sizeof(symbol));
   constexpr size_t redByte=PAKAR_S3_LED_SWAP_RED_BLUE?16:0;
   const bool one=bit>=redByte&&bit<redByte+8&&(static_cast<uint8_t>(2000)&(0x80U>>(bit-redByte)));
   assert(symbol.duration0==(one?PakarS3::oneHigh:PakarS3::zeroHigh));
   assert(symbol.duration1==(one?PakarS3::oneLow:PakarS3::zeroLow));
   assert(symbol.level0==1&&symbol.level1==0);
  }
  assert(Fixture::latches==Fixture::field(output,"transmitted"));
 }
 // Deliberately violate the SDK100ms wait deadline to model an unresponsive
 // driver. Wait for THIS barrier generation, not a prior TX's completion tail:
 // a normal100ms timeout/stop can correctly quiesce before the150ms guard.
 Fixture::setBlock(true);output.submit(rgb,255,true,2001);Fixture::waitBlocked();
 assert(!PakarS3::LedOutput::acquireFlashPause());Fixture::setBlock(false);
 Fixture::until([&]{return Fixture::field(output,"revision")==2001;});
 output.submit(rgb,255,false,2002);Fixture::until([&]{return Fixture::field(output,"revision")==2002;});
 const auto off=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(35));assert(Fixture::count()==off);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.back();for(size_t i=0;i<480*24;++i){rmt_symbol_word_t symbol;std::memcpy(&symbol,bytes.data()+i*sizeof(symbol),sizeof(symbol));assert(symbol.duration0==PakarS3::zeroHigh&&symbol.duration1==PakarS3::zeroLow);}}
 output.submit(rgb,255,true,2003);Fixture::until([&]{return Fixture::field(output,"revision")==2003;});
 const auto resumed=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(Fixture::count()==resumed&&resumed==off+1);
 concurrentProducers(output);asymmetricChannels(output);
 Fixture::cleanup();puts("Actual DMA worker: stackless serialized member submission,1440-byte internal source,SDK MSB-first bytes encoding/timing,300us post-completion latch,startup black,immutable TX,2000 mailbox submissions/coalescing,no unchanged replay,off black/sleep/on resume,boundary/nested/concurrent holds and failed-quiesce recovery passed.");
}
void failure(bool disableFails,bool shortCompletion=false){
 PakarS3::LedOutput output;Fixture::timeout=!shortCompletion;Fixture::shortCompletion=shortCompletion;Fixture::disableFails=disableFails;Fixture::blockDisable=true;CRGB rgb[480]{};assert(output.begin());output.submit(rgb,255,true,1);
 Fixture::until([&]{return Fixture::field(output,"failed")==1;});
 assert(Fixture::field(output,"transmitted")==0);
 assert(Fixture::field(output,"error_code")==unsigned(shortCompletion?ESP_ERR_INVALID_RESPONSE:ESP_ERR_TIMEOUT));
 Fixture::until([&]{std::lock_guard<std::mutex> lock(Fixture::mutex);return Fixture::stops==1;});
 std::atomic<bool> answered{false},permitted{false};
 std::thread flash([&]{permitted=PakarS3::LedOutput::acquireFlashPause();answered=true;});
 output.submit(rgb,255,true,2);std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(Fixture::count()==1&&!answered);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);Fixture::blockDisable=false;Fixture::changed.notify_all();}
 flash.join();assert(permitted==!disableFails);if(permitted)PakarS3::LedOutput::releaseFlashPause();
 if(disableFails)assert(!PakarS3::LedOutput::acquireFlashPause());
 Fixture::cleanup();puts(disableFails?"Actual worker timeout+failed RMT stop: no buffer reuse and repeated flash refusal passed.":"Actual worker timeout: stopped driver before allowing flash, no buffer reuse passed.");
}
int main(int argc,char** argv){
 PakarS3::LedOutput driver;
 const std::string expected=PAKAR_S3_LED_USE_DMA?
     "{\"backend\":\"rmt-dma\",\"rmt\":true,\"dma\":true}":
     "{\"backend\":\"gpio-static\",\"rmt\":false,\"dma\":false}";
 assert(driver.capabilitiesJson()==expected);
 assert(driver.json().find(expected.substr(0,expected.size()-1))==0);
 assert(argc==2);std::thread watchdog([]{const auto end=Fixture::Clock::now()+std::chrono::seconds(8);while(!Fixture::finished){if(Fixture::Clock::now()>end)std::_Exit(2);std::this_thread::sleep_for(std::chrono::milliseconds(10));}});
 const std::string mode=argv[1];
 if(mode.rfind("pin-failure-",0)==0){const auto step=unsigned(std::stoul(mode.substr(12)));assert(step>=1&&step<=3);pinSetupFailure(step);}
 else {
#if !PAKAR_S3_LED_USE_DMA
 assert(mode=="static");staticFunctional();
#else
 if(mode=="functional")functional();else if(mode=="timeout")failure(false);else if(mode=="short-frame")failure(false,true);else{assert(mode=="stop-failure");failure(true);}
#endif
 }
 watchdog.join();
 puts("Host SDK doubles only: not physical RMT/DMA,RF,OTA or optical acceptance.");
}
