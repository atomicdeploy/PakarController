#include "LedOutput.h"
#include "LedWire.h"
#include "LedDmaGuard.h"
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_attr.h>
#include <driver/gpio.h>
#include <utility>
#if PAKAR_S3_LED_GPIO_ENABLED
#include <sdkconfig.h>
#include <esp_cpu.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif
#endif

namespace PakarS3 {
#if PAKAR_S3_LED_GPIO_ENABLED
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
bool LedOutput::backendSupported(Backend selected){
  switch(selected){
    case Backend::Gpio:return PAKAR_S3_LED_GPIO_ENABLED;
    case Backend::RmtDma:return PAKAR_S3_LED_RMT_ENABLED;
    case Backend::None:return !PAKAR_S3_LED_GPIO_ENABLED && !PAKAR_S3_LED_RMT_ENABLED;
  }
  return false;
}
const char* LedOutput::backendName(Backend selected){
  switch(selected){case Backend::Gpio:return "gpio-static";case Backend::RmtDma:return "rmt-dma";default:return "none";}
}
bool LedOutput::parseBackend(const String& value,Backend& selected){
  for(auto candidate:{Backend::None,Backend::Gpio,Backend::RmtDma})
    if(value==backendName(candidate)){selected=candidate;return true;}
  return false;
}
String LedOutput::availableBackendsJson(){
  String out="[";
#if PAKAR_S3_LED_GPIO_ENABLED
  out+=F("\"gpio-static\"");
#endif
#if PAKAR_S3_LED_RMT_ENABLED
  if(PAKAR_S3_LED_GPIO_ENABLED)out+=',';
  out+=F("\"rmt-dma\"");
#endif
#if !PAKAR_S3_LED_GPIO_ENABLED && !PAKAR_S3_LED_RMT_ENABLED
  out+=F("\"none\"");
#endif
  out+=']';return out;
}
bool LedOutput::requestBackend(Backend selected){
  if(!backendSupported(selected))return false;
  std::lock_guard<std::mutex> lock(wireGate_);
  if(!queue_)return selected==Backend::None;
  if(!switchPending() && ready_.load() && backend()==selected && requestedBackend()==selected)return true;
  requestedBackend_.store(selected);requestedRevision_.fetch_add(1);return true;
}
bool LedOutput::begin(Backend initial){
  if(queue_)return requestBackend(initial);
  requestedBackend_.store(initial);requestedRevision_.store(1);
#if !PAKAR_S3_LED_GPIO_ENABLED && !PAKAR_S3_LED_RMT_ENABLED
  // Deliberately absent means absent: no pin access, allocation, queue or task.
  if(initial!=Backend::None){error_.store(ESP_ERR_NOT_SUPPORTED);switchError_.store(ESP_ERR_NOT_SUPPORTED);}
  appliedRevision_.store(1);backendRevision_.fetch_add(1);return true;
#else
  queue_=xQueueCreateStatic(1,sizeof(Frame),queueBytes_,&queueState_);
  if(!queue_){error_.store(ESP_ERR_NO_MEM);switchError_.store(ESP_ERR_NO_MEM);appliedRevision_.store(1);backendRevision_.fetch_add(1);return false;}
  // Wi-Fi runs on core0. Output is isolated on core1: the default static GPIO
  // backend masks local IRQs for one bounded changed frame; optional RMT-DMA
  // initializes its refill interrupt here. No network callbacks execute here.
  if(xTaskCreatePinnedToCore(run,"pakar-led",4096,this,2,nullptr,1)!=pdPASS){
    error_.store(ESP_ERR_NO_MEM);switchError_.store(ESP_ERR_NO_MEM);queue_=nullptr;
    appliedRevision_.store(1);backendRevision_.fetch_add(1);return false;
  }
  return true;
#endif
}
void LedOutput::submit(const CRGB *rgb,uint8_t brightness,bool enabled,uint32_t revision){
  if(!queue_ || !rgb)return;
  std::lock_guard<std::mutex> lock(producerGate_);
  ::memcpy(producer_.rgb,rgb,sizeof(producer_.rgb));producer_.brightness=brightness;
  producer_.enabled=enabled;producer_.revision=revision;
  // Capacity one: coalesce obsolete complete frames instead of replaying a
  // latency-growing animation backlog. An in-flight DMA buffer is never edited.
  xQueueOverwrite(queue_,&producer_);submitted_.fetch_add(1);
}
int LedOutput::initializeDriver(Backend selected){
  if(selected==Backend::None)return ESP_ERR_NOT_SUPPORTED;
  // esp32WS2811::_setupRMT explicitly keeps the wire idle LOW. Apply that
  // requirement before allocation/channel setup on both of our backends.
  // Preload LOW before enabling output so startup cannot expose a stale HIGH
  // GPIO latch. This is not an all-off LED frame or level-shifter guarantee.
  int pinError=gpio_reset_pin(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA));
  if(!pinError)pinError=gpio_set_level(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA),0);
  if(!pinError)pinError=gpio_set_direction(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA),GPIO_MODE_OUTPUT);
  if(pinError)return pinError;
  pinOwned_=true;
  constexpr size_t bytes=pixels*3;
  payload_=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  int error=payload_?ESP_OK:ESP_ERR_NO_MEM;
#if PAKAR_S3_LED_GPIO_ENABLED
  if(selected==Backend::Gpio){
#if CONFIG_PM_ENABLE
    if(!error)error=esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"pakar-led",&frequencyLock_);
    if(!error){error=esp_pm_lock_acquire(frequencyLock_);frequencyLocked_=!error;}
#endif
    // Pin the bit-banger clock, never adapt the data waveform to CPU scaling.
    if(!error && getCpuFrequencyMhz()!=240)error=ESP_ERR_NOT_SUPPORTED;
    return error;
  }
#endif
#if PAKAR_S3_LED_RMT_ENABLED
  // IDF5.5.5's IRAM bytes encoder refills its own bounded internal DMA ring.
  // Keep only the immutable1440-byte source internal, not46084 expanded
  // symbols. PSRAM/flash payloads are deliberately not an ISR fallback.
  rmt_tx_channel_config_t config{};config.gpio_num=static_cast<gpio_num_t>(PAKAR_S3_LED_DATA);
  config.clk_src=RMT_CLK_SRC_DEFAULT;config.resolution_hz=rmtResolutionHz;
  config.mem_block_symbols=dmaRingSymbols;config.trans_queue_depth=1;
  // All RMT channels share the group's normal interrupt priority. The onboard
  // status pixel is initialized through Arduino rmtInit (priority 0/default).
  // A fixed priority 3 here conflicts with that live group and IDF rejects
  // runtime switching with ESP_ERR_INVALID_ARG (258). Inherit its priority;
  // the independent GDMA refill IRQ remains explicitly measured at level 3
  // by LedDmaGuard, so this does not weaken the DMA timing/cache-safety gate.
  config.intr_priority=0;
  config.flags.with_dma=true; // Required, never silently fall back to bit-bang/non-DMA.
  if(!error){
    error=newCacheSafeLedChannel(&config,&channel_);
    refillCacheSafe_=!error;
  }
  rmt_bytes_encoder_config_t coding{};
  coding.bit0.level0=1;coding.bit0.duration0=zeroHigh;
  coding.bit0.level1=0;coding.bit0.duration1=zeroLow;
  coding.bit1.level0=1;coding.bit1.duration0=oneHigh;
  coding.bit1.level1=0;coding.bit1.duration1=oneLow;
  coding.flags.msb_first=1;
  if(!error)error=rmt_new_bytes_encoder(&coding,&encoder_);
  rmt_tx_event_callbacks_t callbacks{};callbacks.on_trans_done=onDone;
  if(!error)error=rmt_tx_register_event_callbacks(channel_,&callbacks,this);
  if(!error){error=rmt_enable(channel_);channelEnabled_=!error;}
  return error;
#else
  return error?error:ESP_ERR_NOT_SUPPORTED;
#endif
}
int LedOutput::releaseDriver(){
  // Called only by the worker between complete frames (including latch), or
  // after a fault. Failed quiescence keeps every possibly DMA-owned resource.
#if PAKAR_S3_LED_RMT_ENABLED
  if(!capture_.quiesce())return ESP_ERR_INVALID_STATE;
  if(channelEnabled_){const int error=rmt_disable(channel_);if(error)return error;channelEnabled_=false;}
  if(encoder_){const int error=rmt_del_encoder(encoder_);if(error)return error;encoder_=nullptr;}
  if(channel_){const int error=rmt_del_channel(channel_);if(error)return error;channel_=nullptr;}
#endif
#if PAKAR_S3_LED_GPIO_ENABLED && CONFIG_PM_ENABLE
  if(frequencyLocked_){const int error=esp_pm_lock_release(frequencyLock_);if(error)return error;frequencyLocked_=false;}
  if(frequencyLock_){const int error=esp_pm_lock_delete(frequencyLock_);if(error)return error;frequencyLock_=nullptr;}
#endif
  if(payload_){heap_caps_free(payload_);payload_=nullptr;}
  if(pinOwned_){const int error=gpio_reset_pin(static_cast<gpio_num_t>(PAKAR_S3_LED_DATA));if(error)return error;pinOwned_=false;}
  refillCacheSafe_.store(false);doneSymbols_.store(0);backend_.store(Backend::None);return ESP_OK;
}
void LedOutput::work(){
  constexpr size_t bytes=pixels*3;
  bool pending=false;
  bool initialBlack=true,previousStartKnown=false;
  uint32_t previousStartedAt=0,configuredRevision=0;
  for(;;){
    if(flashHolds_.load()){vTaskDelay(1);continue;}
    if(requestedRevision_.load()!=configuredRevision){
      uint32_t selectedRevision;Backend requested;
      {
        std::lock_guard<std::mutex> lock(wireGate_);
        if(flashHolds_.load())continue;
        selectedRevision=requestedRevision_.load();requested=requestedBackend_.load();
        inFlight_.store(true); // Includes peripheral teardown/setup, not just TX.
      }
      const Backend previous=backend();ready_.store(false);active_.store(false);
      int error=releaseDriver(),requestError=error;bool safe=!error;
      Backend selected=requested;
      if(!error){
        if(!backendSupported(selected)){
          requestError=ESP_ERR_NOT_SUPPORTED;
          selected=PAKAR_S3_LED_GPIO_ENABLED?Backend::Gpio:(PAKAR_S3_LED_RMT_ENABLED?Backend::RmtDma:Backend::None);
        }
        error=initializeDriver(selected);
        if(error){
          requestError=error;
          const int cleanup=releaseDriver();
          safe=!cleanup;if(cleanup)error=cleanup;
          else {
            // Default safe fallback is GPIO. An explicitly failed GPIO setup
            // may restore the prior RMT driver, but never drives two backends.
            const Backend fallback=PAKAR_S3_LED_GPIO_ENABLED && selected!=Backend::Gpio?Backend::Gpio:previous;
            if(fallback!=Backend::None && fallback!=selected && backendSupported(fallback)){
              selected=fallback;error=initializeDriver(selected);
              if(error){const int fallbackCleanup=releaseDriver();safe=!fallbackCleanup;if(fallbackCleanup)error=fallbackCleanup;}
            }
          }
        }
      }
      if(!error){
        backend_.store(selected);ready_.store(true);error_.store(0);
        if(initialBlack)::memset(payload_,0,bytes);
        else encodeWireBytes(current_.rgb,pixels,current_.brightness,current_.enabled,payload_);
        pending=true;inFlight_.store(false);
      }else{
        error_.store(error);
        // A failed initialization may be safely cleaned up. A failed release
        // is quarantined; a later explicit request can retry quiescence.
        if(safe)inFlight_.store(false);
      }
      switchError_.store(requestError);configuredRevision=selectedRevision;
      // Successful initialization is not a completed switch. Keep it pending
      // through the first full current-frame resend and its reset/latch below.
      if(error){appliedRevision_.store(selectedRevision);backendRevision_.fetch_add(1);}
      continue;
    }
    if(!ready_.load()){
      // Retain the newest full frame while failed; no hidden restart loop.
      xQueueReceive(queue_,&current_,pdMS_TO_TICKS(10));continue;
    }
    if(!pending){
      if(xQueueReceive(queue_,&current_,pdMS_TO_TICKS(10))!=pdTRUE)continue;
      encodeWireBytes(current_.rgb,pixels,current_.brightness,current_.enabled,payload_);
      pending=true;
    }
    {
      std::lock_guard<std::mutex> lock(wireGate_);
      if(flashHolds_.load() || requestedRevision_.load()!=configuredRevision)continue;
      // A flash hold may have interrupted this pending frame before admission.
      // Adopt the latest complete mailbox now rather than replay a stale color
      // before an already-queued Off/newer command. Never replace startup black.
      if(!initialBlack && xQueueReceive(queue_,&current_,0)==pdTRUE)
        encodeWireBytes(current_.rgb,pixels,current_.brightness,current_.enabled,payload_);
      inFlight_.store(true);
    }
    const uint32_t startedAt=micros();
    if(previousStartKnown)lastStartIntervalMicros_.store(startedAt-previousStartedAt);
    previousStartedAt=startedAt;previousStartKnown=true;
    int error=ESP_OK;
#if PAKAR_S3_LED_GPIO_ENABLED
    if(backend()==Backend::Gpio)writeStaticWire(payload_,bytes);
#endif
#if PAKAR_S3_LED_RMT_ENABLED
    if(backend()==Backend::RmtDma){
    rmt_transmit_config_t tx{};tx.loop_count=0;tx.flags.eot_level=0;tx.flags.queue_nonblocking=1;
    doneSymbols_.store(0,std::memory_order_relaxed);
    capture_.start(payload_,bytes); // RX arms before THIS immutable frame, never mid-frame.
    error=rmt_transmit(channel_,encoder_,payload_,bytes,&tx);
    if(!error)error=rmt_tx_wait_all_done(channel_,100);
    if(!error&&doneSymbols_.load(std::memory_order_relaxed)!=dataSymbols+1)error=ESP_ERR_INVALID_RESPONSE;
    if(!capture_.finish(current_.revision,!error)) {
      error_.store(ESP_ERR_INVALID_STATE);failed_.fetch_add(1);ready_.store(false);active_.store(false);
      switchError_.store(ESP_ERR_INVALID_STATE);appliedRevision_.store(configuredRevision);backendRevision_.fetch_add(1);
      if(rmt_disable(channel_)==ESP_OK)channelEnabled_=false;
      continue; // RX not quiesced: retain buffers and flash refusal.
    }
    if(error){
      // Never reuse a possibly in-flight buffer after an uncertain deadline.
      error_.store(error);failed_.fetch_add(1);ready_.store(false);active_.store(false);
      // Stop hardware before permitting flash or leaving this task. If the
      // stop itself fails, inFlight stays true and flash guards refuse writes.
      switchError_.store(error);appliedRevision_.store(configuredRevision);backendRevision_.fetch_add(1);
      if(rmt_disable(channel_)==ESP_OK){channelEnabled_=false;inFlight_.store(false);}
      continue; // Explicit retry only; never reuse a possibly in-flight payload.
    }
    // EOT keeps the pad LOW after the final data bit. Hold the frame/flash
    // ownership until the full reset interval, not merely descriptor admission.
    esp_rom_delay_us(resetMicros);
    }
#else
    (void)error;
#endif
    const uint32_t completedAt=micros(),frameMicros=completedAt-startedAt;
    lastFrameMicros_.store(frameMicros);lastCompletedAtMicros_.store(completedAt);
    if(frameMicros>maxFrameMicros_.load())maxFrameMicros_.store(frameMicros);
    inFlight_.store(false);
    transmitted_.fetch_add(1);revision_.store(current_.revision);active_.store(current_.enabled);
    if(appliedRevision_.load()!=configuredRevision){appliedRevision_.store(configuredRevision);backendRevision_.fetch_add(1);}
    pending=false;initialBlack=false;
    // GPIO masks this core for the wire frame. Yield one tick to the lower
    // priority renderer; DMA already yields while waiting for completion.
    if(backend()==Backend::Gpio)vTaskDelay(1);
    // rmt_tx_wait_all_done already blocks this task while DMA runs, and the
    // mailbox receive blocks when idle. Do not add a whole RTOS tick after
    // every14.7ms frame: it consumes part of the small60FPS scheduling margin.
    // No busy-wait/frame replay or faster-than800kHz wire clock is introduced.
  }
}
String LedOutput::capabilitiesJson()const{
  // Selected wire implementation, independent of renderer mode or output On/Off.
  // Readiness/fault/active state remains in the live driver observation below.
  const auto selected=backend();const bool rmt=selected==Backend::RmtDma;
  String value=F("{\"backend\":\"");value+=backendName(selected);value+='"';
  value+=F(",\"rmt\":");value+=rmt?F("true"):F("false");
  value+=F(",\"dma\":");value+=rmt?F("true"):F("false");
  value+=F(",\"signal_capture_supported\":");value+=rmt?F("true"):F("false");
  value+=F(",\"available_backends\":");value+=availableBackendsJson();
  value+=F(",\"runtime_switch\":");value+=(PAKAR_S3_LED_GPIO_ENABLED && PAKAR_S3_LED_RMT_ENABLED)?F("true"):F("false");
  value+='}';return value;
}
String LedOutput::json(bool compact)const{
  String value=capabilitiesJson();value.remove(value.length()-1);
  value+=F(",\"requested_backend\":\"");value+=backendName(requestedBackend());value+='"';
  value+=F(",\"switch_pending\":");value+=switchPending()?F("true"):F("false");
  value+=F(",\"switch_error\":");value+=switchError_.load();
  value+=F(",\"backend_revision\":");value+=backendRevision();
  value+=F(",\"data_gpio\":");value+=PAKAR_S3_LED_DATA;
  value+=F(",\"ready\":");value+=ready_.load()?F("true"):F("false");
  value+=F(",\"active\":");value+=active_.load()?F("true"):F("false");
  value+=F(",\"on_change_only\":true");
  if(!compact) {
  value+=F(",\"timing_profile\":\"ws2811-800khz\",\"requested_t0h_ns\":");value+=backend()==Backend::RmtDma?300U:250U;
  value+=F(",\"requested_t1h_ns\":");value+=backend()==Backend::RmtDma?625U:600U;
  value+=F(",\"requested_bit_ns\":1250,\"latch_us\":300");
  value+=F(",\"pixels\":");value+=uint32_t(pixels);
  value+=F(",\"minimum_frame_us\":");value+=minimumFrameMicros;
  value+=F(",\"maximum_wire_fps_milli\":");value+=maximumWireFpsMilli;
#if PAKAR_S3_LED_RMT_ENABLED
  value+=F(",\"dma_ring_symbols\":");value+=uint32_t(dmaRingSymbols);
#endif
  value+=F(",\"frame_timing\":{\"scope\":\"driver-completion-not-optical\",\"last_us\":");value+=lastFrameMicros_.load();
  value+=F(",\"maximum_us\":");value+=maxFrameMicros_.load();
  value+=F(",\"start_interval_us\":");value+=lastStartIntervalMicros_.load();
  value+=F(",\"completion_age_us\":");
  if(transmitted_.load())value+=uint32_t(micros()-lastCompletedAtMicros_.load());else value+=F("null");
  value+='}';
#if PAKAR_S3_LED_RMT_ENABLED
  value+=F(",\"signal_capture\":");value+=capture_.json();
#endif
  value+=F(",\"refill_cache_safe\":");value+=refillCacheSafe_?F("true"):F("false");
  const auto refillIrq=backend()==Backend::RmtDma?ledDmaIrqReceipt():LedDmaIrqReceipt{};
  value+=F(",\"refill_irq\":{\"witnessed\":");value+=refillIrq.witnessed?F("true"):F("false");
  for(const auto &field:{std::pair<const char*,int>{"source",refillIrq.source},{"cpu",refillIrq.cpu},{"vector",refillIrq.vector},{"level",refillIrq.level}}){
    value+=F(",\"");value+=field.first;value+=F("\":");
    if(refillIrq.witnessed)value+=field.second;else value+=F("null");
  }
  value+='}';
  value+=F(",\"done_symbols\":");value+=doneSymbols_.load();
  value+=F(",\"expected_symbols\":");value+=backend()==Backend::RmtDma?dataSymbols+1:0U;
  }
  value+=F(",\"submitted\":");value+=submitted_.load();
  value+=F(",\"channel_order\":\"");value+=wireChannelOrder;value+='"';
  value+=F(",\"rgb_gains\":");value+=rgbGains;
  value+=F(",\"rgb_gain_channels\":[");
  for(unsigned channel=0;channel<3;++channel){if(channel)value+=',';value+=unsigned(channelGains[channel]);}
  value+=']';
  value+=F(",\"transmitted\":");value+=transmitted_.load();value+=F(",\"failed\":");value+=failed_.load();
  value+=F(",\"flash_paused\":");value+=(flashHolds_.load()&&!inFlight_.load())?F("true"):F("false");
  value+=F(",\"revision\":");value+=revision_.load();value+=F(",\"error_code\":");value+=error_.load();value+='}';return value;
}
}
