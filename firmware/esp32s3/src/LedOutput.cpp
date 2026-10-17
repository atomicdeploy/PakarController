#include "LedOutput.h"
#include "LedWire.h"
#include <esp_heap_caps.h>

namespace PakarS3 {
std::mutex LedOutput::wireGate_;
std::atomic<uint32_t> LedOutput::flashHolds_{0};
std::atomic<bool> LedOutput::inFlight_{false};
bool acquireLedFlashPause(){return LedOutput::acquireFlashPause();}
void releaseLedFlashPause(){LedOutput::releaseFlashPause();}
bool LedOutput::acquireFlashPause(){
  {std::lock_guard<std::mutex> lock(wireGate_);flashHolds_.fetch_add(1);}
  const uint32_t start=millis();
  while(inFlight_.load()){
    if(static_cast<uint32_t>(millis()-start)>=150U){releaseFlashPause();return false;}
    vTaskDelay(1);
  }
  return true; // New TX must check holds under the same gate before starting.
}
void LedOutput::releaseFlashPause(){flashHolds_.fetch_sub(1);}
bool LedOutput::begin(){
  queue_=xQueueCreateStatic(1,sizeof(Frame),queueBytes_,&queueState_);
  if(!queue_){error_.store(ESP_ERR_NO_MEM);return false;}
  // Wi-Fi runs on core0. Initialize RMT/DMA interrupts on the output worker's
  // core1; no device/network callbacks execute here and no spin/masked IRQs.
  if(xTaskCreatePinnedToCore(run,"pakar-led",4096,this,2,nullptr,1)!=pdPASS){
    error_.store(ESP_ERR_NO_MEM);queue_=nullptr;return false;
  }
  return true;
}
void LedOutput::submit(const CRGB *rgb,uint8_t brightness,bool enabled,uint32_t revision){
  if(!queue_)return;
  Frame next;::memcpy(next.rgb,rgb,sizeof(next.rgb));next.brightness=brightness;
  next.enabled=enabled;next.revision=revision;
  // Capacity one: coalesce obsolete complete frames instead of replaying a
  // latency-growing animation backlog. An in-flight DMA buffer is never edited.
  xQueueOverwrite(queue_,&next);submitted_.fetch_add(1);
}
void LedOutput::work(){
  rmt_channel_handle_t channel=nullptr;rmt_encoder_handle_t encoder=nullptr;
  constexpr size_t symbols=pixels*24+1;
  auto *encoded=static_cast<rmt_symbol_word_t*>(heap_caps_malloc(symbols*sizeof(rmt_symbol_word_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  rmt_tx_channel_config_t config{};config.gpio_num=static_cast<gpio_num_t>(PAKAR_S3_LED_DATA);
  config.clk_src=RMT_CLK_SRC_DEFAULT;config.resolution_hz=40000000;
  config.mem_block_symbols=2046;config.trans_queue_depth=1;config.intr_priority=3;
  config.flags.with_dma=true; // Required, never silently fall back to bit-bang/non-DMA.
  int error=encoded?rmt_new_tx_channel(&config,&channel):ESP_ERR_NO_MEM;
  rmt_copy_encoder_config_t copy{};
  if(!error)error=rmt_new_copy_encoder(&copy,&encoder);
  if(!error)error=rmt_enable(channel);
  if(error){
    error_.store(error);if(encoder)rmt_del_encoder(encoder);if(channel)rmt_del_channel(channel);
    if(encoded)heap_caps_free(encoded);
    vTaskDelete(nullptr);return;
  }
  ready_.store(true);
  bool have=false,blackSent=false;
  for(;;){
    if(flashHolds_.load()){vTaskDelay(1);continue;}
    const bool received=xQueueReceive(queue_,&current_,have&&(!blackSent)?0:pdMS_TO_TICKS(10))==pdTRUE;
    if(received){
      have=true;blackSent=false;
      encodeWire(current_.rgb,pixels,current_.brightness,current_.enabled,encoded);
    }
    if(!have||(!received&&blackSent))continue;
    {
      std::lock_guard<std::mutex> lock(wireGate_);
      if(flashHolds_.load())continue;
      inFlight_.store(true);
    }
    rmt_transmit_config_t tx{};tx.loop_count=0;tx.flags.eot_level=0;
    error=rmt_transmit(channel,encoder,encoded,symbols*sizeof(*encoded),&tx);
    if(!error)error=rmt_tx_wait_all_done(channel,100);
    if(error){
      // Never reuse a possibly in-flight buffer after an uncertain deadline.
      error_.store(error);failed_.fetch_add(1);ready_.store(false);active_.store(false);
      // Stop hardware before permitting flash or leaving this task. If the
      // stop itself fails, inFlight stays true and flash guards refuse writes.
      if(rmt_disable(channel)==ESP_OK)inFlight_.store(false);
      vTaskDelete(nullptr);return;
    }
    inFlight_.store(false);
    transmitted_.fetch_add(1);revision_.store(current_.revision);active_.store(current_.enabled);
    blackSent=!current_.enabled; // Explicit Off clears once, then sleeps; default On refreshes.
    vTaskDelay(1); // Yield to other services, not a20fps/application rate cap.
  }
}
String LedOutput::json()const{
  String value=F("{\"ready\":");value+=ready_.load()?F("true"):F("false");
  value+=F(",\"active\":");value+=active_.load()?F("true"):F("false");
  value+=F(",\"backend\":\"rmt-dma\",\"submitted\":");value+=submitted_.load();
  value+=F(",\"channel_order\":\"");value+=PAKAR_S3_LED_SWAP_RED_BLUE?F("bgr"):F("rgb");value+='"';
  value+=F(",\"transmitted\":");value+=transmitted_.load();value+=F(",\"failed\":");value+=failed_.load();
  value+=F(",\"flash_paused\":");value+=(flashHolds_.load()&&!inFlight_.load())?F("true"):F("false");
  value+=F(",\"revision\":");value+=revision_.load();value+=F(",\"error_code\":");value+=error_.load();value+='}';return value;
}
}
