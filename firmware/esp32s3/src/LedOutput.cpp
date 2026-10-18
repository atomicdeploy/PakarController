#include "LedOutput.h"
#include "LedWire.h"
#include "LedDmaGuard.h"
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_attr.h>
#if !PAKAR_S3_LED_USE_DMA
#include <sdkconfig.h>
#include <esp_cpu.h>
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif
#endif

namespace PakarS3 {
#if !PAKAR_S3_LED_USE_DMA
namespace {
portMUX_TYPE pixelMux=portMUX_INITIALIZER_UNLOCKED;
// Cycle-count technique follows FastLED's actual ESP32 core/clock_cycles.h.
// The complete payload is internal RAM. This function has no flash/library
// calls while core1 interrupts are masked; core0 Wi-Fi remains schedulable.
void IRAM_ATTR __attribute__((noinline)) writeStaticWire(const uint8_t *bytes,size_t size) {
  constexpr uint32_t mask=1U<<(PAKAR_S3_LED_DATA%32);
  constexpr uint32_t highRegister=PAKAR_S3_LED_DATA<32?GPIO_OUT_W1TS_REG:GPIO_OUT1_W1TS_REG;
  constexpr uint32_t lowRegister=PAKAR_S3_LED_DATA<32?GPIO_OUT_W1TC_REG:GPIO_OUT1_W1TC_REG;
  // Explicitly checked240MHz below:250/600ns high,1250ns period.
  portENTER_CRITICAL(&pixelMux);
  uint32_t previous=esp_cpu_get_cycle_count()-staticBitCycles;
  for(size_t i=0;i<size;++i)for(uint8_t bit=0x80;bit;bit>>=1){
    while(static_cast<uint32_t>(esp_cpu_get_cycle_count()-previous)<staticBitCycles){}
    previous=esp_cpu_get_cycle_count();
    REG_WRITE(highRegister,mask);
    const uint32_t high=(bytes[i]&bit)?staticOneHighCycles:staticZeroHighCycles;
    while(static_cast<uint32_t>(esp_cpu_get_cycle_count()-previous)<high){}
    REG_WRITE(lowRegister,mask);
  }
  while(static_cast<uint32_t>(esp_cpu_get_cycle_count()-previous)<staticBitCycles){}
  portEXIT_CRITICAL(&pixelMux);
  esp_rom_delay_us(resetMicros); // Reset/latch is low, with interrupts enabled.
}
}
#endif
std::mutex LedOutput::wireGate_;
std::atomic<uint32_t> LedOutput::flashHolds_{0};
std::atomic<bool> LedOutput::inFlight_{false};
bool IRAM_ATTR LedOutput::onDone(rmt_channel_handle_t,const rmt_tx_done_event_data_t *event,void *context){
  // SDK encoded-symbol accounting (not an optical or waveform receipt).
  static_cast<LedOutput*>(context)->doneSymbols_.store(event->num_symbols,std::memory_order_relaxed);return false;
}
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
  // Wi-Fi runs on core0. Output is isolated on core1: the default static GPIO
  // backend masks local IRQs for one bounded changed frame; optional RMT-DMA
  // initializes its refill interrupt here. No network callbacks execute here.
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
#if !PAKAR_S3_LED_USE_DMA
  constexpr size_t bytes=pixels*3;
  auto *payload=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  int error=payload?gpio_reset_pin(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA)):ESP_ERR_NO_MEM;
  if(!error)error=gpio_set_direction(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA),GPIO_MODE_OUTPUT);
  if(!error)error=gpio_set_level(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA),0);
#if CONFIG_PM_ENABLE
  esp_pm_lock_handle_t frequencyLock=nullptr;
  if(!error)error=esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"pakar-led",&frequencyLock);
  if(!error)error=esp_pm_lock_acquire(frequencyLock);
#endif
  // The shipping SDK has automatic frequency scaling disabled. When enabled,
  // the explicit maximum-frequency lock above pins the worker's timing clock.
  if(!error && getCpuFrequencyMhz()!=240)error=ESP_ERR_NOT_SUPPORTED;
  if(error){
    error_.store(error);
#if CONFIG_PM_ENABLE
    if(frequencyLock){esp_pm_lock_release(frequencyLock);esp_pm_lock_delete(frequencyLock);}
#endif
    if(payload)heap_caps_free(payload);
    vTaskDelete(nullptr);return;
  }
  ::memset(payload,0,bytes);ready_.store(true);
  bool pending=true; // Always clear480 pixels once before accepting white/color.
  for(;;){
    if(flashHolds_.load()){vTaskDelay(1);continue;}
    if(!pending){
      if(xQueueReceive(queue_,&current_,pdMS_TO_TICKS(10))!=pdTRUE)continue;
      encodeWireBytes(current_.rgb,pixels,current_.brightness,current_.enabled,payload);
      pending=true;
    }
    {
      std::lock_guard<std::mutex> lock(wireGate_);
      if(flashHolds_.load())continue;
      inFlight_.store(true);
    }
    writeStaticWire(payload,bytes);
    inFlight_.store(false);pending=false;
    transmitted_.fetch_add(1);revision_.store(current_.revision);active_.store(current_.enabled);
    vTaskDelay(1); // On-change only: never free-run or replay an animation queue.
  }
#else
  rmt_channel_handle_t channel=nullptr;rmt_encoder_handle_t encoder=nullptr;
  constexpr size_t bytes=pixels*3;
  // IDF5.5.5's IRAM bytes encoder refills its own bounded internal DMA ring.
  // Keep only the immutable1440-byte source internal, not46084 expanded
  // symbols. PSRAM/flash payloads are deliberately not an ISR fallback.
  auto *payload=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  rmt_tx_channel_config_t config{};config.gpio_num=static_cast<gpio_num_t>(PAKAR_S3_LED_DATA);
  config.clk_src=RMT_CLK_SRC_DEFAULT;config.resolution_hz=40000000;
  config.mem_block_symbols=2046;config.trans_queue_depth=1;config.intr_priority=3;
  config.flags.with_dma=true; // Required, never silently fall back to bit-bang/non-DMA.
  int error=payload?ESP_OK:ESP_ERR_NO_MEM;
  if(!error){
    error=newCacheSafeLedChannel(&config,&channel);
    refillCacheSafe_=!error;
  }
  rmt_bytes_encoder_config_t coding{};
  coding.bit0.level0=1;coding.bit0.duration0=zeroHigh;
  coding.bit0.level1=0;coding.bit0.duration1=zeroLow;
  coding.bit1.level0=1;coding.bit1.duration0=oneHigh;
  coding.bit1.level1=0;coding.bit1.duration1=oneLow;
  coding.flags.msb_first=1;
  if(!error)error=rmt_new_bytes_encoder(&coding,&encoder);
  rmt_tx_event_callbacks_t callbacks{};callbacks.on_trans_done=onDone;
  if(!error)error=rmt_tx_register_event_callbacks(channel,&callbacks,this);
  if(!error)error=rmt_enable(channel);
  if(error){
    error_.store(error);if(encoder)rmt_del_encoder(encoder);if(channel)rmt_del_channel(channel);
    if(payload)heap_caps_free(payload);
    vTaskDelete(nullptr);return;
  }
  ready_.store(true);
  // Submit complete frames only. Replaying unchanged contrast-heavy frames
  // needlessly exercises DMA refill under RF load. The renderer still sends
  // periodic repair frames and uncapped animation frames as needed.
  bool pending=true;
  ::memset(payload,0,bytes); // One startup black frame.
  for(;;){
    if(flashHolds_.load()){vTaskDelay(1);continue;}
    if(!pending){
      if(xQueueReceive(queue_,&current_,pdMS_TO_TICKS(10))!=pdTRUE)continue;
      encodeWireBytes(current_.rgb,pixels,current_.brightness,current_.enabled,payload);
      pending=true;
    }
    {
      std::lock_guard<std::mutex> lock(wireGate_);
      if(flashHolds_.load())continue;
      inFlight_.store(true);
    }
    rmt_transmit_config_t tx{};tx.loop_count=0;tx.flags.eot_level=0;tx.flags.queue_nonblocking=1;
    doneSymbols_.store(0,std::memory_order_relaxed);
    error=rmt_transmit(channel,encoder,payload,bytes,&tx);
    if(!error)error=rmt_tx_wait_all_done(channel,100);
    if(!error&&doneSymbols_.load(std::memory_order_relaxed)!=pixels*24+1)error=ESP_ERR_INVALID_RESPONSE;
    if(error){
      // Never reuse a possibly in-flight buffer after an uncertain deadline.
      error_.store(error);failed_.fetch_add(1);ready_.store(false);active_.store(false);
      // Stop hardware before permitting flash or leaving this task. If the
      // stop itself fails, inFlight stays true and flash guards refuse writes.
      if(rmt_disable(channel)==ESP_OK)inFlight_.store(false);
      vTaskDelete(nullptr);return;
    }
    // EOT keeps the pad LOW after the final data bit. Hold the frame/flash
    // ownership until the full reset interval, not merely descriptor admission.
    esp_rom_delay_us(resetMicros);
    inFlight_.store(false);
    transmitted_.fetch_add(1);revision_.store(current_.revision);active_.store(current_.enabled);
    pending=false;
    vTaskDelay(1); // Yield to other services, not a20fps/application rate cap.
  }
#endif
}
String LedOutput::capabilitiesJson()const{
  // Selected wire implementation, independent of renderer mode or output On/Off.
  // Readiness/fault/active state remains in the live driver observation below.
  return PAKAR_S3_LED_USE_DMA?
      F("{\"backend\":\"rmt-dma\",\"rmt\":true,\"dma\":true}"):
      F("{\"backend\":\"gpio-static\",\"rmt\":false,\"dma\":false}");
}
String LedOutput::json()const{
  String value=capabilitiesJson();value.remove(value.length()-1);
  value+=F(",\"ready\":");value+=ready_.load()?F("true"):F("false");
  value+=F(",\"active\":");value+=active_.load()?F("true"):F("false");
  value+=F(",\"on_change_only\":true");
  value+=F(",\"timing_profile\":\"ws2811-800khz\",\"requested_t0h_ns\":250,\"requested_t1h_ns\":600,\"requested_bit_ns\":1250,\"latch_us\":300");
  value+=F(",\"refill_cache_safe\":");value+=refillCacheSafe_?F("true"):F("false");
  value+=F(",\"done_symbols\":");value+=doneSymbols_.load();
  value+=F(",\"expected_symbols\":");value+=PAKAR_S3_LED_USE_DMA?11521U:0U;
  value+=F(",\"submitted\":");value+=submitted_.load();
  value+=F(",\"channel_order\":\"");value+=PAKAR_S3_LED_SWAP_RED_BLUE?F("bgr"):F("rgb");value+='"';
  value+=F(",\"transmitted\":");value+=transmitted_.load();value+=F(",\"failed\":");value+=failed_.load();
  value+=F(",\"flash_paused\":");value+=(flashHolds_.load()&&!inFlight_.load())?F("true"):F("false");
  value+=F(",\"revision\":");value+=revision_.load();value+=F(",\"error_code\":");value+=error_.load();value+='}';return value;
}
}
