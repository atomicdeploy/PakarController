// Compile the ACTUAL S3 output worker; only SDK/FreeRTOS peripherals are doubled.
#include "../firmware/esp32s3/src/LedOutput.h"
#include "../firmware/esp32s3/src/LedWire.h"
#include <esp_heap_caps.h>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace Fixture {
using Clock=std::chrono::steady_clock;
struct Stop{};
std::atomic<bool> stopping{false},finished{false};
std::thread worker;
std::mutex mutex;
std::condition_variable changed;
bool live=false,block=false,timeout=false,disableFails=false,blockDisable=false;
unsigned stops=0;
unsigned blockGeneration=0,blockedGeneration=0;
const void* buffer=nullptr;
std::vector<uint8_t> snapshot;
std::vector<std::vector<uint8_t>> frames;
std::vector<void*> allocations;
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
int xQueueOverwrite(QueueHandle_t queue,const void* frame){std::lock_guard<std::mutex> lock(queue->mutex);std::memcpy(queue->bytes.data(),frame,queue->bytes.size());queue->full=true;queue->changed.notify_one();return pdTRUE;}
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
 Fixture::worker=std::thread([=]{try{run(context);}catch(const Fixture::Stop&){} });return pdPASS;
}
void vTaskDelay(TickType_t delay){if(Fixture::stopping)throw Fixture::Stop{};std::this_thread::sleep_for(std::chrono::milliseconds(delay));if(Fixture::stopping)throw Fixture::Stop{};}
void vTaskDelete(void*){throw Fixture::Stop{};}
void* heap_caps_malloc(size_t bytes,int caps){assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));auto* p=std::malloc(bytes);Fixture::allocations.push_back(p);return p;}
void heap_caps_free(void* pointer){for(auto& p:Fixture::allocations)if(p==pointer)p=nullptr;std::free(pointer);}
int rmt_new_tx_channel(const rmt_tx_channel_config_t* config,rmt_channel_handle_t* channel){assert(config->flags.with_dma&&config->mem_block_symbols==2046&&config->intr_priority==3&&config->resolution_hz==40000000&&config->trans_queue_depth==1&&config->gpio_num==4);*channel=reinterpret_cast<void*>(1);return ESP_OK;}
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t* encoder){*encoder=reinterpret_cast<void*>(2);return ESP_OK;}
int rmt_enable(rmt_channel_handle_t){return ESP_OK;}
int rmt_del_channel(rmt_channel_handle_t){return ESP_OK;}
int rmt_del_encoder(rmt_encoder_handle_t){return ESP_OK;}
int rmt_transmit(rmt_channel_handle_t,rmt_encoder_handle_t,const void* source,size_t bytes,const rmt_transmit_config_t* tx){
 std::lock_guard<std::mutex> lock(Fixture::mutex);assert(!Fixture::live&&tx->loop_count==0&&tx->flags.eot_level==0);Fixture::live=true;Fixture::buffer=source;
 Fixture::snapshot.assign(static_cast<const uint8_t*>(source),static_cast<const uint8_t*>(source)+bytes);Fixture::frames.push_back(Fixture::snapshot);Fixture::changed.notify_all();return ESP_OK;
}
int rmt_tx_wait_all_done(rmt_channel_handle_t,int deadline){
 assert(deadline==100);std::unique_lock<std::mutex> lock(Fixture::mutex);
 if(Fixture::block){Fixture::blockedGeneration=Fixture::blockGeneration;Fixture::changed.notify_all();}
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::block||Fixture::stopping.load();});
 assert(!Fixture::block||Fixture::stopping);if(Fixture::stopping)throw Fixture::Stop{};
 assert(std::memcmp(Fixture::buffer,Fixture::snapshot.data(),Fixture::snapshot.size())==0);
 if(Fixture::timeout)return ESP_ERR_TIMEOUT;
 lock.unlock();std::this_thread::sleep_for(std::chrono::milliseconds(2));lock.lock();
 assert(std::memcmp(Fixture::buffer,Fixture::snapshot.data(),Fixture::snapshot.size())==0);Fixture::live=false;return ESP_OK;
}
int rmt_disable(rmt_channel_handle_t){
 std::unique_lock<std::mutex> lock(Fixture::mutex);++Fixture::stops;Fixture::changed.notify_all();
 Fixture::changed.wait_for(lock,std::chrono::milliseconds(500),[]{return !Fixture::blockDisable||Fixture::stopping.load();});
 assert(!Fixture::blockDisable||Fixture::stopping);
 if(Fixture::disableFails)return ESP_ERR_TIMEOUT;
 Fixture::live=false;return ESP_OK;
}

void functional(){
 PakarS3::LedOutput output;assert(output.begin());CRGB rgb[480];for(auto& p:rgb)p={255,0,0};
 Fixture::setBlock(true);output.submit(rgb,255,true,1);Fixture::waitBlocked();
 std::thread producer([&]{for(uint32_t i=2;i<=2000;++i){for(auto& p:rgb)p={static_cast<uint8_t>(i),0,0};output.submit(rgb,255,true,i);} });producer.join();
 assert(Fixture::count()==1);assert(!output.canAccept());
 std::atomic<bool> acquired{false};std::thread waiter([&]{assert(PakarS3::LedOutput::acquireFlashPause());acquired=true;});
 std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(!acquired);
 Fixture::setBlock(false);waiter.join();assert(acquired&&!Fixture::isLive());
 assert(PakarS3::LedOutput::acquireFlashPause());const auto held=Fixture::count();
 std::thread nested([]{assert(PakarS3::LedOutput::acquireFlashPause());PakarS3::LedOutput::releaseFlashPause();});nested.join();
 PakarS3::LedOutput::releaseFlashPause();std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(Fixture::count()==held);
 PakarS3::LedOutput::releaseFlashPause();Fixture::until([&]{return Fixture::field(output,"revision")==2000;});
 Fixture::until([&]{return Fixture::count()>=held+4;});assert(Fixture::field(output,"submitted")==2000);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.at(1);
  for(size_t pixel=0;pixel<480;++pixel)for(size_t bit=0;bit<24;++bit){
   rmt_symbol_word_t symbol;std::memcpy(&symbol,bytes.data()+(pixel*24+bit)*sizeof(symbol),sizeof(symbol));
   constexpr size_t redByte=PAKAR_S3_LED_SWAP_RED_BLUE?16:0;
   const bool one=bit>=redByte&&bit<redByte+8&&(static_cast<uint8_t>(2000)&(0x80U>>(bit-redByte)));
   assert(symbol.duration0==(one?PakarS3::oneHigh:PakarS3::zeroHigh));
  }
 }
 // Deliberately violate the SDK100ms wait deadline to model an unresponsive
 // driver. Wait for THIS barrier generation, not a prior TX's completion tail:
 // a normal100ms timeout/stop can correctly quiesce before the150ms guard.
 Fixture::setBlock(true);Fixture::waitBlocked();assert(!PakarS3::LedOutput::acquireFlashPause());Fixture::setBlock(false);
 const auto before=Fixture::count();Fixture::until([&]{return Fixture::count()>before+2;});
 output.submit(rgb,255,false,2001);Fixture::until([&]{return Fixture::field(output,"revision")==2001;});
 const auto off=Fixture::count();std::this_thread::sleep_for(std::chrono::milliseconds(35));assert(Fixture::count()==off);
 {std::lock_guard<std::mutex> lock(Fixture::mutex);const auto& bytes=Fixture::frames.back();for(size_t i=0;i<480*24;++i){rmt_symbol_word_t symbol;std::memcpy(&symbol,bytes.data()+i*sizeof(symbol),sizeof(symbol));assert(symbol.duration0==PakarS3::zeroHigh&&symbol.duration1==PakarS3::zeroLow);}}
 output.submit(rgb,255,true,2002);Fixture::until([&]{return Fixture::field(output,"revision")==2002;});Fixture::until([&]{return Fixture::count()>=off+3;});
 Fixture::cleanup();puts("Actual worker: immutable TX,2000 mailbox submissions/coalescing,continuous refresh,off black/sleep/on resume,boundary/nested/concurrent holds and failed-quiesce recovery passed.");
}
void failure(bool disableFails){
 PakarS3::LedOutput output;Fixture::timeout=true;Fixture::disableFails=disableFails;Fixture::blockDisable=true;CRGB rgb[480]{};assert(output.begin());output.submit(rgb,255,true,1);
 Fixture::until([&]{return Fixture::field(output,"failed")==1;});
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
 assert(argc==2);std::thread watchdog([]{const auto end=Fixture::Clock::now()+std::chrono::seconds(8);while(!Fixture::finished){if(Fixture::Clock::now()>end)std::_Exit(2);std::this_thread::sleep_for(std::chrono::milliseconds(10));}});
 const std::string mode=argv[1];if(mode=="functional")functional();else if(mode=="timeout")failure(false);else{assert(mode=="stop-failure");failure(true);}watchdog.join();
 puts("Host SDK doubles only: not physical RMT/DMA,RF,OTA or optical acceptance.");
}
