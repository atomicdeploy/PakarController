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
#if (defined(PAKAR_S3_LED_DEFAULT_BACKEND) && PAKAR_S3_LED_DEFAULT_BACKEND==2) || (!PAKAR_S3_LED_GPIO_ENABLED && PAKAR_S3_LED_RMT_ENABLED)
#define PAKAR_TEST_USE_RMT 1
#else
#define PAKAR_TEST_USE_RMT 0
#endif
#if PAKAR_S3_LED_RMT_ENABLED
#include <driver/rmt_rx.h>
#endif
#if PAKAR_S3_LED_GPIO_ENABLED
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
bool blockReceive=false,receivedBlocked=false;
std::atomic<unsigned> workerDelays{0};
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
unsigned pinSetupStep=0;
bool pinOutput=false,channelEnabled=false,channelExists=false,encoderExists=false;
bool pmExists=false,pmAcquired=false;
std::atomic<unsigned> pmCreates{0},pmDeletes{0};
std::atomic<unsigned> channelDeletes{0},encoderDeletes{0},frees{0},taskCreates{0},queueCreates{0};
std::atomic<int> channelCreateError{0},encoderCreateError{0},payloadAllocError{0},channelDeleteError{0};
#if PAKAR_S3_LED_RMT_ENABLED
rmt_rx_event_callbacks_t rxCallbacks{};void* rxOwner=nullptr;void* rxBuffer=nullptr;
size_t rxCapacity=0;unsigned rxCorruptBit=UINT32_MAX;
bool rxNoMemory=false,rxNeverDone=false,rxDisableFails=false,rxMalformed=false;
#endif
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
 // Driver counters follow the optional nested signal-capture observation.
 // Read the driver error, not that capture's separately reported error code.
 const auto json=output.json();const auto pos=json.rfind(std::string("\"")+name+"\":");assert(pos!=std::string::npos);
 return static_cast<uint32_t>(std::stoul(json.substr(json.find(':',pos)+1)));
}
void cleanup(){stopping=true;changed.notify_all();if(worker.joinable())worker.join();for(auto* p:allocations)std::free(p);allocations.clear();finished=true;}
}
QueueHandle_t xQueueCreateStatic(unsigned capacity,size_t bytes,uint8_t*,StaticQueue_t* state){++Fixture::queueCreates;assert(capacity==1);state->bytes.resize(bytes);return state;}
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
 std::memcpy(frame,queue->bytes.data(),queue->bytes.size());queue->full=false;
 lock.unlock();
 {
  std::unique_lock<std::mutex> barrier(Fixture::mutex);
  if(Fixture::blockReceive){
   Fixture::receivedBlocked=true;Fixture::changed.notify_all();
   Fixture::changed.wait_for(barrier,std::chrono::milliseconds(500),[]{return !Fixture::blockReceive||Fixture::stopping.load();});
   assert(!Fixture::blockReceive||Fixture::stopping);
  }
 }
 return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t queue){std::lock_guard<std::mutex> lock(queue->mutex);return queue->full?1:0;}
int xTaskCreatePinnedToCore(void(*run)(void*),const char*,unsigned bytes,void* context,unsigned priority,void*,int core){
 assert(core==1&&bytes==4096&&priority>=2);assert(!Fixture::worker.joinable());
 ++Fixture::taskCreates;
 Fixture::owner=static_cast<PakarS3::LedOutput*>(context);
 Fixture::worker=std::thread([=]{try{run(context);}catch(const Fixture::Stop&){} });return pdPASS;
}
void vTaskDelay(TickType_t delay){++Fixture::workerDelays;if(Fixture::stopping)throw Fixture::Stop{};std::this_thread::sleep_for(std::chrono::milliseconds(delay));if(Fixture::stopping)throw Fixture::Stop{};}
void vTaskDelete(void*){throw Fixture::Stop{};}
void* heap_caps_malloc(size_t bytes,int caps){++Fixture::allocationCalls;assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));assert(bytes==1440);if(Fixture::payloadAllocError)return nullptr;auto* p=std::malloc(bytes);Fixture::allocations.push_back(p);return p;}
void* heap_caps_aligned_alloc(size_t align,size_t bytes,int caps){
 assert(align==32 && bytes==2032*sizeof(rmt_symbol_word_t));
 assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT|MALLOC_CAP_DMA));
#if PAKAR_S3_LED_RMT_ENABLED
 if(Fixture::rxNoMemory)return nullptr;
#endif
 auto* p=std::malloc(bytes);Fixture::allocations.push_back(p);return p;
}
void heap_caps_free(void* pointer){assert(!Fixture::isLive());++Fixture::frees;for(auto& p:Fixture::allocations)if(p==pointer)p=nullptr;std::free(pointer);}
int esp_pm_lock_create(int,int,const char*,void** handle){assert(!Fixture::pmExists);Fixture::pmExists=true;++Fixture::pmCreates;*handle=reinterpret_cast<void*>(5);return ESP_OK;}
int esp_pm_lock_acquire(void*){assert(Fixture::pmExists&&!Fixture::pmAcquired);Fixture::pmAcquired=true;return ESP_OK;}
int esp_pm_lock_release(void*){assert(!Fixture::isLive()&&Fixture::pmAcquired);Fixture::pmAcquired=false;return ESP_OK;}
int esp_pm_lock_delete(void*){assert(Fixture::pmExists&&!Fixture::pmAcquired);Fixture::pmExists=false;++Fixture::pmDeletes;return ESP_OK;}
int rmt_new_tx_channel(const rmt_tx_channel_config_t* config,rmt_channel_handle_t* channel){assert(Fixture::pinOutput&&!Fixture::channelExists);++Fixture::channelCreates;assert(config->flags.with_dma&&config->mem_block_symbols==2046&&config->intr_priority==0&&config->resolution_hz==40000000&&config->trans_queue_depth==1&&config->gpio_num==4);if(Fixture::channelCreateError)return Fixture::channelCreateError;Fixture::channelExists=true;*channel=reinterpret_cast<void*>(1);return ESP_OK;}
namespace PakarS3 {
int newCacheSafeLedChannel(const rmt_tx_channel_config_t *config,rmt_channel_handle_t *channel){return rmt_new_tx_channel(config,channel);}
LedDmaIrqReceipt ledDmaIrqReceipt(){return {PAKAR_S3_LED_RMT_ENABLED!=0,42,1,23,3};} // Actual allocator is tested separately.
}
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t* encoder){*encoder=reinterpret_cast<void*>(2);return ESP_OK;}
int rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t* config,rmt_encoder_handle_t* encoder){
 if(Fixture::encoderCreateError)return Fixture::encoderCreateError;
 assert(!Fixture::encoderExists);Fixture::encoderExists=true;
 assert(config->flags.msb_first==1);
 assert(config->bit0.level0==1&&config->bit0.level1==0&&config->bit0.duration0==12&&config->bit0.duration1==38);
 assert(config->bit1.level0==1&&config->bit1.level1==0&&config->bit1.duration0==25&&config->bit1.duration1==25);
 Fixture::byteCoding=*config;*encoder=reinterpret_cast<void*>(3);return ESP_OK;
}
int rmt_enable(rmt_channel_handle_t channel){if(channel==reinterpret_cast<void*>(1)){assert(!Fixture::channelEnabled);Fixture::channelEnabled=true;}return ESP_OK;}
int rmt_tx_register_event_callbacks(rmt_channel_handle_t,const rmt_tx_event_callbacks_t *callbacks,void *owner){Fixture::callbacks=*callbacks;Fixture::callbackOwner=owner;return ESP_OK;}
int rmt_del_channel(rmt_channel_handle_t channel){if(channel==reinterpret_cast<void*>(1)){assert(!Fixture::isLive()&&!Fixture::channelEnabled);if(Fixture::channelDeleteError)return Fixture::channelDeleteError;++Fixture::channelDeletes;Fixture::channelExists=false;}return ESP_OK;}
int rmt_del_encoder(rmt_encoder_handle_t){assert(!Fixture::isLive()&&!Fixture::channelEnabled);++Fixture::encoderDeletes;Fixture::encoderExists=false;return ESP_OK;}
#if PAKAR_S3_LED_RMT_ENABLED
int rmt_new_rx_channel(const rmt_rx_channel_config_t* config,rmt_channel_handle_t* channel){
 assert(config->gpio_num==4&&config->flags.with_dma&&config->flags.io_loop_back);
 assert(config->resolution_hz==40000000&&config->mem_block_symbols==2032&&config->intr_priority==0);
 *channel=reinterpret_cast<void*>(4);return ESP_OK;
}
int rmt_rx_register_event_callbacks(rmt_channel_handle_t,const rmt_rx_event_callbacks_t* callbacks,void* owner){
 Fixture::rxCallbacks=*callbacks;Fixture::rxOwner=owner;return ESP_OK;
}
int rmt_receive(rmt_channel_handle_t,void* buffer,size_t size,const rmt_receive_config_t* config){
 assert(config->signal_range_min_ns==100&&config->signal_range_max_ns==100000&&config->flags.en_partial_rx);
 Fixture::rxBuffer=buffer;Fixture::rxCapacity=size;return ESP_OK;
}
#endif
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
#if PAKAR_S3_LED_RMT_ENABLED
 if(Fixture::rxBuffer && !Fixture::rxNeverDone){
  const auto& frame=Fixture::frames.back();
  const size_t total=frame.size()/sizeof(rmt_symbol_word_t),half=Fixture::rxCapacity/sizeof(rmt_symbol_word_t)/2;
  assert(half==1016 && total>half*2); // Whole frame must wrap this small ring repeatedly.
  for(size_t at=0,window=0;at<total;at+=half,++window){
   const auto count=std::min(half,total-at);
   auto* chunk=static_cast<rmt_symbol_word_t*>(Fixture::rxBuffer)+(window%2)*half;
   std::memcpy(chunk,frame.data()+at*sizeof(*chunk),count*sizeof(*chunk));
   if(Fixture::rxCorruptBit>=at && Fixture::rxCorruptBit<at+count){
    auto& symbol=chunk[Fixture::rxCorruptBit-at];
    symbol=symbol.duration0==PakarS3::zeroHigh?Fixture::byteCoding.bit1:Fixture::byteCoding.bit0;
   }
   rmt_rx_done_event_data_t received{};received.received_symbols=chunk;received.num_symbols=count;
   received.flags.is_last=at+count==total;
   if(Fixture::rxMalformed){received.num_symbols=2033;received.flags.is_last=true;}
   Fixture::rxCallbacks.on_recv_done(reinterpret_cast<void*>(4),&received,Fixture::rxOwner);
   if(Fixture::rxMalformed)break;
  }
  Fixture::rxBuffer=nullptr;
 }
#endif
 rmt_tx_done_event_data_t done{Fixture::shortCompletion?480*24U:480*24U+1};assert(Fixture::callbacks.on_trans_done);Fixture::callbacks.on_trans_done(reinterpret_cast<void*>(1),&done,Fixture::callbackOwner);return ESP_OK;
}
int rmt_disable(rmt_channel_handle_t channel){
#if PAKAR_S3_LED_RMT_ENABLED
 if(channel==reinterpret_cast<void*>(4)){
  if(Fixture::rxDisableFails)return ESP_ERR_TIMEOUT;
  Fixture::rxBuffer=nullptr;return ESP_OK;
 }
#else
 (void)channel;
#endif
 std::unique_lock<std::mutex> lock(Fixture::mutex);++Fixture::stops;Fixture::changed.notify_all();
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::blockDisable||Fixture::stopping.load();});
 assert(!Fixture::blockDisable||Fixture::stopping);
 if(Fixture::disableFails)return ESP_ERR_TIMEOUT;
 Fixture::live=false;Fixture::channelEnabled=false;return ESP_OK;
}
void esp_rom_delay_us(unsigned micros){
 assert(micros==PakarS3::resetMicros);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);assert(!Fixture::live);++Fixture::latches;}
 std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void concurrentProducers(PakarS3::LedOutput& output);
void asymmetricChannels(PakarS3::LedOutput& output);
int pinStep(unsigned expected){assert(Fixture::pinSetupStep==expected);++Fixture::pinSetupStep;const auto step=++Fixture::pinCalls;return step==Fixture::pinFailure?ESP_ERR_TIMEOUT:ESP_OK;}
int gpio_reset_pin(gpio_num_t pin){assert(pin==PAKAR_S3_LED_DATA&&!Fixture::isLive()&&!Fixture::channelExists);Fixture::pinOutput=false;Fixture::pinSetupStep=0;return pinStep(0);}
int gpio_set_level(gpio_num_t pin,int value){assert(pin==PAKAR_S3_LED_DATA&&value==0);return pinStep(1);}
int gpio_set_direction(gpio_num_t pin,int mode){assert(pin==PAKAR_S3_LED_DATA&&mode==GPIO_MODE_OUTPUT);const int result=pinStep(2);if(!result)Fixture::pinOutput=true;return result;}
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
#if PAKAR_S3_LED_GPIO_ENABLED
void fixtureCriticalEnter(){
 assert(Fixture::pinOutput&&!Fixture::channelExists&&!Fixture::channelEnabled);
 std::unique_lock<std::mutex> lock(Fixture::mutex);assert(!Fixture::live);Fixture::live=true;
 if(Fixture::block){Fixture::blockedGeneration=Fixture::blockGeneration;Fixture::changed.notify_all();}
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::block||Fixture::stopping.load();});
 assert(!Fixture::block||Fixture::stopping);if(Fixture::stopping)throw Fixture::Stop{};
 Fixture::bitCount=0;Fixture::staticBytes.assign(1440,0);
}
void fixtureGpioWrite(unsigned address,unsigned value){
 assert(value==(1U<<(PAKAR_S3_LED_DATA%32)));
 if(address==(PAKAR_S3_LED_DATA<32?GPIO_OUT_W1TS_REG:GPIO_OUT1_W1TS_REG)){
  if(Fixture::bitCount)assert(static_cast<uint32_t>(Fixture::cycle-Fixture::lastRise)>=300U);
  Fixture::lastRise=Fixture::highAt=Fixture::cycle;
 }else{
  assert(address==(PAKAR_S3_LED_DATA<32?GPIO_OUT_W1TC_REG:GPIO_OUT1_W1TC_REG));
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
 const uint8_t correction[]={uint8_t(PAKAR_S3_LED_RGB_GAINS>>16),uint8_t(PAKAR_S3_LED_RGB_GAINS>>8),uint8_t(PAKAR_S3_LED_RGB_GAINS)};
 for(size_t p=0;p<480;++p)for(size_t c=0;c<3;++c){
  const uint8_t logical[]={rgb[p].r,rgb[p].g,rgb[p].b};const size_t index=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
  const uint8_t expected=uint8_t((uint32_t(logical[index])*211*correction[index]+32512U)/65025U);
#if PAKAR_TEST_USE_RMT
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
 #if PAKAR_TEST_USE_RMT
 assert(output.requestSignalCapture());assert(!output.requestSignalCapture());
 output.submit(rgb,255,true,11000);Fixture::until([&]{return Fixture::field(output,"frame_revision")==11000;});
 assert(output.json().find("\"match\":true")!=std::string::npos);
 assert(Fixture::field(output,"observed_bits")==11520);
 Fixture::rxCorruptBit=23*6*24;assert(output.requestSignalCapture());
 output.submit(rgb,255,true,11001);Fixture::until([&]{return Fixture::field(output,"frame_revision")==11001;});
 assert(output.json().find("\"match\":false")!=std::string::npos);
 assert(Fixture::field(output,"first_mismatch_bit")==23*6*24);
 Fixture::rxCorruptBit=UINT32_MAX;Fixture::rxNeverDone=true;assert(output.requestSignalCapture());
 output.submit(rgb,255,true,11002);Fixture::until([&]{return Fixture::field(output,"revision")==11002;});
 assert(output.json().find("\"ready\":true")!=std::string::npos);
 assert(output.json().find("\"match\":null")!=std::string::npos);
 Fixture::rxNeverDone=false;Fixture::rxNoMemory=true;assert(output.requestSignalCapture());
 output.submit(rgb,255,true,11003);Fixture::until([&]{return Fixture::field(output,"revision")==11003;});
 assert(output.json().find("\"ready\":true")!=std::string::npos);
 assert(output.json().find("\"match\":null")!=std::string::npos);
 Fixture::rxNoMemory=false;Fixture::rxMalformed=true;assert(output.requestSignalCapture());
 output.submit(rgb,255,true,11004);Fixture::until([&]{return Fixture::field(output,"revision")==11004;});
 assert(output.json().find("\"ready\":true")!=std::string::npos);
 assert(output.json().find("\"match\":null")!=std::string::npos);
 Fixture::rxMalformed=false;Fixture::rxDisableFails=true;assert(output.requestSignalCapture());
 output.submit(rgb,255,true,11005);Fixture::until([&]{return Fixture::field(output,"failed")==1;});
 assert(Fixture::field(output,"error_code")==ESP_ERR_INVALID_STATE);
 assert(!output.requestSignalCapture());assert(!PakarS3::LedOutput::acquireFlashPause());
 #endif
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
void colorGains(){
 // Expected bytes are derived independently from explicit fixture gain values,
 // not by reusing encodeWire or wireChannel to predict the worker's output.
 static_assert(PAKAR_S3_LED_RGB_GAINS==0xffffff || PAKAR_S3_LED_RGB_GAINS==0xffb0f0,
               "Color fixture requires neutral or the explicit former calibration");
 const uint8_t gains[]={255,PAKAR_S3_LED_RGB_GAINS==0xffffff?uint8_t(255):uint8_t(176),
                            PAKAR_S3_LED_RGB_GAINS==0xffffff?uint8_t(255):uint8_t(240)};
 PakarS3::LedOutput output;assert(output.begin());
 Fixture::until([&]{return Fixture::field(output,"transmitted")==1;});
 CRGB rgb[480];uint32_t revision=0;
 for(uint8_t brightness:{uint8_t(255),uint8_t(128)})
  for(CRGB color:{CRGB{255,255,255},CRGB{255,0,0},CRGB{0,255,0},CRGB{0,0,255},CRGB{0x12,0x34,0x56}}){
   for(auto& pixel:rgb)pixel=color;
   output.submit(rgb,brightness,true,++revision);
   Fixture::until([&]{return Fixture::field(output,"revision")==revision;});
   std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& frame=Fixture::frames.back();
   for(size_t pixel=0;pixel<480;++pixel)for(size_t c=0;c<3;++c){
    const size_t logical=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
    const uint8_t channels[]={color.r,color.g,color.b};
    const auto expected=uint8_t((uint32_t(channels[logical])*brightness*gains[logical]+32512U)/65025U);
#if PAKAR_TEST_USE_RMT
    uint8_t decoded=0;
    for(size_t bit=0;bit<8;++bit){rmt_symbol_word_t symbol;
     std::memcpy(&symbol,frame.data()+((pixel*3+c)*8+bit)*sizeof(symbol),sizeof(symbol));
     assert(symbol.level0==1&&symbol.level1==0);
     assert(symbol.duration0==PakarS3::zeroHigh||symbol.duration0==PakarS3::oneHigh);
     assert(symbol.duration0+symbol.duration1==50);
     decoded=uint8_t((decoded<<1)|(symbol.duration0==PakarS3::oneHigh));
    }
    assert(decoded==expected);
#else
    assert(frame.at(pixel*3+c)==expected);
#endif
   }
  }
 output.submit(rgb,255,false,++revision);
 Fixture::until([&]{return Fixture::field(output,"revision")==revision;});
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& frame=Fixture::frames.back();
#if PAKAR_TEST_USE_RMT
  for(size_t bit=0;bit<480*24;++bit){rmt_symbol_word_t symbol;
   std::memcpy(&symbol,frame.data()+bit*sizeof(symbol),sizeof(symbol));assert(symbol.duration0==PakarS3::zeroHigh);
  }
#else
  for(uint8_t byte:frame)assert(byte==0);
#endif
 }
 assert(Fixture::field(output,"rgb_gains")==PAKAR_S3_LED_RGB_GAINS);
 assert(output.json().find(PAKAR_S3_LED_RGB_GAINS==0xffffff?"\"rgb_gain_channels\":[255,255,255]":"\"rgb_gain_channels\":[255,176,240]")!=std::string::npos);
 Fixture::cleanup();
 puts("Actual full480-pixel worker: white/R/G/B/custom exact bytes, full/half brightness exactly once, explicit neutral/calibrated RGB gains, channel-order selection and complete Off; host output is not physical color/flicker acceptance.");
}
void cadence(){
 PakarS3::LedOutput output;assert(output.begin());
 Fixture::until([&]{return Fixture::field(output,"transmitted")==1;});
 CRGB rgb[480];for(auto& p:rgb)p={17,43,199};
 const auto delays=Fixture::workerDelays.load();
 for(uint32_t revision=1;revision<=12;++revision){
  output.submit(rgb,255,true,revision);
  Fixture::until([&]{return Fixture::field(output,"revision")==revision;});
 }
 #if PAKAR_TEST_USE_RMT
 // Peripheral completion and the idle mailbox already block this task. No
 // unconditional extra RTOS tick belongs between every successful DMA frame.
 assert(Fixture::workerDelays.load()==delays);
 #else
 // GPIO masks this core during the frame: retain the scheduling opportunity
 // for the lower-priority renderer instead of starving it with taskYIELD.
 Fixture::until([&]{return Fixture::workerDelays.load()>=delays+12;});
 #endif
 assert(Fixture::field(output,"pixels")==480);
 assert(Fixture::field(output,"minimum_frame_us")==14700);
 assert(Fixture::field(output,"maximum_wire_fps_milli")==68027);
 #if PAKAR_TEST_USE_RMT
 assert(Fixture::field(output,"dma_ring_symbols")==2046);
 #endif
 assert(Fixture::field(output,"last_us")>0);
 assert(Fixture::field(output,"maximum_us")>=Fixture::field(output,"last_us"));
 assert(Fixture::field(output,"start_interval_us")>0);
 assert(output.json().find("driver-completion-not-optical")!=std::string::npos);
 Fixture::cleanup();
 puts("Actual worker: fixed480/14.7ms wire budget, backend-appropriate yield and measured complete-frame/start-interval telemetry; host timing is not device FPS or optical proof.");
}
void latestAfterHold(){
 PakarS3::LedOutput output;assert(output.begin());
 Fixture::until([&]{return Fixture::field(output,"transmitted")==1;});
 CRGB rgb[480];for(auto& p:rgb)p={255,0,0};
 {std::lock_guard<std::mutex> lock(Fixture::mutex);Fixture::blockReceive=true;}
 output.submit(rgb,255,true,1);
 Fixture::until([]{std::lock_guard<std::mutex> lock(Fixture::mutex);return Fixture::receivedBlocked;});
 assert(PakarS3::LedOutput::acquireFlashPause());
 output.submit(rgb,255,false,2); // Newer complete Off supersedes the pending red.
 {std::lock_guard<std::mutex> lock(Fixture::mutex);Fixture::blockReceive=false;Fixture::changed.notify_all();}
 // Keep the hold long enough to exercise the real pending-before-admission
 // branch rather than merely coalescing two frames in the same mailbox.
 std::this_thread::sleep_for(std::chrono::milliseconds(15));
 assert(Fixture::count()==1);
 PakarS3::LedOutput::releaseFlashPause();
 Fixture::until([&]{return Fixture::field(output,"revision")==2;});
 assert(Fixture::count()==2); // Startup black plus Off; no stale red replay.
 assert(output.json().find("\"active\":false")!=std::string::npos);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.back();
 #if PAKAR_TEST_USE_RMT
  for(size_t bit=0;bit<480*24;++bit){rmt_symbol_word_t symbol;
   std::memcpy(&symbol,bytes.data()+bit*sizeof(symbol),sizeof(symbol));
   assert(symbol.level0==1&&symbol.level1==0&&symbol.duration0==PakarS3::zeroHigh&&symbol.duration1==PakarS3::zeroLow);
  }
 #else
  assert(bytes.size()==480*3);for(const auto byte:bytes)assert(byte==0);
 #endif
 }
 Fixture::cleanup();
 puts("Actual worker: an interrupted pre-admission frame adopts latest complete Off after a flash hold; startup black and immutable full480-pixel transmission preserved.");
}
void absent(){
 using Driver=PakarS3::LedOutput;Driver output;assert(output.begin());
 assert(output.backend()==Driver::Backend::None&&!output.switchPending());
 assert(!output.requestBackend(Driver::Backend::Gpio)&&!output.requestBackend(Driver::Backend::RmtDma));
 CRGB frame[480]{};output.submit(frame,255,true,1);
 assert(!output.canAccept()&&!output.presented(1));
 assert(Fixture::taskCreates==0&&Fixture::queueCreates==0&&Fixture::allocationCalls==0&&Fixture::pinCalls==0);
 assert(output.availableBackendsJson()=="[\"none\"]");
 assert(output.begin(Driver::Backend::RmtDma));
 assert(output.requestedBackend()==Driver::Backend::RmtDma&&output.backend()==Driver::Backend::None);
 assert(output.switchError()==ESP_ERR_NOT_SUPPORTED&&!output.switchPending());
 Fixture::cleanup();puts("No LED inclusion flags: no task, queue, allocation or pin drive; absent capability and unavailable saved preference are truthful.");
}
void unavailableSaved(){
 using Driver=PakarS3::LedOutput;using Backend=Driver::Backend;
 const auto unavailable=PAKAR_S3_LED_GPIO_ENABLED?Backend::RmtDma:Backend::Gpio;
 Driver output;assert(!Driver::backendSupported(unavailable));assert(output.begin(unavailable));
 Fixture::until([&]{return !output.switchPending()&&Fixture::field(output,"transmitted")==1;});
 assert(output.requestedBackend()==unavailable&&output.backend()==Driver::defaultBackend());
 assert(output.switchError()==ESP_ERR_NOT_SUPPORTED&&output.backendRevision()==1);
 assert(!output.requestBackend(unavailable));assert(!output.requestBackend(Backend::None));
 assert(Fixture::taskCreates==1);Fixture::cleanup();
 puts("Single-backend image: unavailable saved backend remains requested with explicit error; compiled driver safely starts without silently changing the preference.");
}
#if PAKAR_S3_LED_GPIO_ENABLED && PAKAR_S3_LED_RMT_ENABLED
void runtimeSwitch(){
 using Driver=PakarS3::LedOutput;using Backend=Driver::Backend;
 Driver output;assert(output.begin(Backend::Gpio));
 Fixture::until([&]{return Fixture::field(output,"transmitted")==1;});
 CRGB rgb[480];for(auto& p:rgb)p={17,43,199};
 output.submit(rgb,255,true,1);Fixture::until([&]{return output.presented(1);});
 std::vector<uint8_t> before;{std::lock_guard<std::mutex> lock(Fixture::mutex);before=Fixture::frames.back();}
 const auto revision=output.backendRevision();
 assert(Driver::acquireFlashPause());assert(output.requestBackend(Backend::RmtDma));
 assert(output.switchPending());std::this_thread::sleep_for(std::chrono::milliseconds(20));
 assert(output.backend()==Backend::Gpio&&Fixture::channelCreates==0);
 Fixture::setBlock(true);
 Driver::releaseFlashPause();
 Fixture::waitBlocked();
 assert(output.backend()==Backend::RmtDma&&output.switchPending()&&output.backendRevision()==revision);
 assert(Fixture::field(output,"transmitted")==2); // Init is not a completed resend.
 Fixture::setBlock(false);
 Fixture::until([&]{return output.backendRevision()>revision&&!output.switchPending()&&output.backend()==Backend::RmtDma;});
 const auto sent=Fixture::field(output,"transmitted");
 Fixture::until([&]{return Fixture::field(output,"transmitted")>=sent&&Fixture::count()>=3;});
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& data=Fixture::frames.back();assert(data.size()==11520*sizeof(rmt_symbol_word_t));
  for(size_t i=0;i<before.size();++i){uint8_t decoded=0;for(unsigned bit=0;bit<8;++bit){rmt_symbol_word_t symbol;std::memcpy(&symbol,data.data()+(i*8+bit)*sizeof(symbol),sizeof(symbol));decoded=uint8_t((decoded<<1)|(symbol.duration0==PakarS3::oneHigh));}assert(decoded==before[i]);}}
 assert(Fixture::channelExists&&Fixture::channelEnabled&&Fixture::encoderExists);
#if CONFIG_PM_ENABLE
 assert(!Fixture::pmExists&&!Fixture::pmAcquired&&Fixture::pmCreates==1&&Fixture::pmDeletes==1);
#endif
 Fixture::setBlock(true);output.submit(rgb,255,true,2);Fixture::waitBlocked();
 const auto deleted=Fixture::channelDeletes.load();assert(output.requestBackend(Backend::Gpio));
 std::this_thread::sleep_for(std::chrono::milliseconds(20));assert(output.backend()==Backend::RmtDma&&Fixture::channelDeletes==deleted);
 // Newest Off wins while switching after an immutable RMT frame.
 output.submit(rgb,255,false,3);Fixture::setBlock(false);
 Fixture::until([&]{return output.backend()==Backend::Gpio&&!output.switchPending()&&output.presented(3);});
 assert(!Fixture::channelExists&&!Fixture::channelEnabled&&!Fixture::encoderExists);
#if CONFIG_PM_ENABLE
 assert(Fixture::pmExists&&Fixture::pmAcquired&&Fixture::pmCreates==2&&Fixture::pmDeletes==1);
#endif
 assert(Fixture::channelDeletes==deleted+1&&Fixture::encoderDeletes==1);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);assert(Fixture::frames.back().size()==1440);for(auto byte:Fixture::frames.back())assert(byte==0);}
 // Explicit RMT allocation failure must retain a working GPIO fallback and
 // the failed requested identity, without retrying automatically each frame.
 Fixture::channelCreateError=ESP_ERR_NO_MEM;assert(output.requestBackend(Backend::RmtDma));
 Fixture::until([&]{return !output.switchPending()&&output.switchError()==ESP_ERR_NO_MEM;});
 assert(output.backend()==Backend::Gpio&&output.requestedBackend()==Backend::RmtDma);
 const auto attempts=Fixture::channelCreates.load();std::this_thread::sleep_for(std::chrono::milliseconds(25));assert(Fixture::channelCreates==attempts);
 Fixture::channelCreateError=0;assert(output.requestBackend(Backend::RmtDma));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::RmtDma&&output.switchError()==0;});
 // Uncertain completion + failed stop: no delete/free/reuse until a later
 // explicit retry has really stopped the former owner.
 Fixture::until([&]{return !Fixture::isLive();});Fixture::timeout=true;Fixture::disableFails=true;
 output.submit(rgb,255,true,4);Fixture::until([&]{return Fixture::field(output,"failed")==1;});
 const auto freed=Fixture::frees.load();const auto removed=Fixture::channelDeletes.load();
 assert(output.requestBackend(Backend::Gpio));Fixture::until([&]{return !output.switchPending();});
 assert(Fixture::frees==freed&&Fixture::channelDeletes==removed&&Fixture::isLive());
 assert(!Driver::acquireFlashPause());
 Fixture::timeout=false;Fixture::disableFails=false;assert(output.requestBackend(Backend::Gpio));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::Gpio&&output.presented(4);});
 assert(Fixture::frees>freed&&Fixture::channelDeletes==removed+1);
 assert(Driver::acquireFlashPause());Driver::releaseFlashPause();
 assert(Fixture::taskCreates==1&&Fixture::queueCreates==1);
 Fixture::cleanup();puts("Runtime GPIO/RMT: sole worker, frame-boundary/flash-held switching, complete replay, newest Off, release ordering, allocation fallback, requested/active truth and failed-quiesce retry passed.");
}
void runtimeResources(){
 using Driver=PakarS3::LedOutput;using Backend=Driver::Backend;
 Driver output;assert(output.begin(Backend::Gpio));
 Fixture::until([&]{return Fixture::field(output,"transmitted")==1;});
 CRGB rgb[480];for(auto& p:rgb)p={81,132,29};output.submit(rgb,255,true,11);
 Fixture::until([&]{return output.presented(11);});
 Fixture::encoderCreateError=ESP_ERR_NO_MEM;assert(output.requestBackend(Backend::RmtDma));
 Fixture::until([&]{return !output.switchPending()&&output.switchError()==ESP_ERR_NO_MEM;});
 assert(output.backend()==Backend::Gpio&&!Fixture::channelExists&&!Fixture::encoderExists&&Fixture::channelDeletes==1);
 Fixture::encoderCreateError=0;Fixture::payloadAllocError=ESP_ERR_NO_MEM;
 assert(output.requestBackend(Backend::RmtDma));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::None;});
 assert(Fixture::field(output,"error_code")==ESP_ERR_NO_MEM&&!Fixture::pinOutput);
 assert(Driver::acquireFlashPause());Driver::releaseFlashPause();
 Fixture::payloadAllocError=0;assert(output.requestBackend(Backend::Gpio));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::Gpio&&output.presented(11);});
 assert(output.requestBackend(Backend::RmtDma));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::RmtDma;});
 Fixture::until([]{return !Fixture::isLive();});
 Fixture::channelDeleteError=ESP_ERR_INVALID_STATE;const auto freed=Fixture::frees.load();
 assert(output.requestBackend(Backend::Gpio));
 Fixture::until([&]{return !output.switchPending()&&output.switchError()==ESP_ERR_INVALID_STATE;});
 assert(Fixture::frees==freed&&Fixture::channelExists&&!Fixture::channelEnabled&&!Fixture::encoderExists);
 assert(!Driver::acquireFlashPause());
 Fixture::channelDeleteError=0;assert(output.requestBackend(Backend::Gpio));
 Fixture::until([&]{return !output.switchPending()&&output.backend()==Backend::Gpio&&output.presented(11);});
 assert(!Fixture::channelExists&&!Fixture::encoderExists&&Fixture::taskCreates==1);
 Fixture::cleanup();puts("Runtime resource failures: partial encoder/channel cleanup, both-driver allocation failure, saved-frame recovery and quarantined failed deletion until explicit retry passed.");
}
#endif
int main(int argc,char** argv){
 PakarS3::LedOutput driver;
 assert(driver.backend()==PakarS3::LedOutput::Backend::None);
 assert(driver.capabilitiesJson().find("\"available_backends\":"+driver.availableBackendsJson())!=std::string::npos);
 assert(driver.json().find("\"backend\":\"none\"")!=std::string::npos);
 assert(argc==2);std::thread watchdog([]{const auto end=Fixture::Clock::now()+std::chrono::seconds(8);while(!Fixture::finished){if(Fixture::Clock::now()>end)std::_Exit(2);std::this_thread::sleep_for(std::chrono::milliseconds(10));}});
 const std::string mode=argv[1];
 if(mode=="absent")absent();
 else if(mode=="unavailable-saved")unavailableSaved();
#if PAKAR_S3_LED_GPIO_ENABLED && PAKAR_S3_LED_RMT_ENABLED
 else if(mode=="runtime-switch")runtimeSwitch();
 else if(mode=="runtime-resources")runtimeResources();
#endif
 else if(mode=="cadence")cadence();else if(mode=="latest-after-hold")latestAfterHold();
 else if(mode=="color-gains")colorGains();
 else if(mode.rfind("pin-failure-",0)==0){const auto step=unsigned(std::stoul(mode.substr(12)));assert(step>=1&&step<=3);pinSetupFailure(step);}
 else {
#if !PAKAR_TEST_USE_RMT && PAKAR_S3_LED_GPIO_ENABLED
 assert(mode=="static");staticFunctional();
#elif PAKAR_TEST_USE_RMT
 if(mode=="functional")functional();else if(mode=="timeout")failure(false);else if(mode=="short-frame")failure(false,true);else{assert(mode=="stop-failure");failure(true);}
#else
 assert(false);
#endif
 }
 watchdog.join();
 puts("Host SDK doubles only: not physical RMT/DMA,RF,OTA or optical acceptance.");
}
