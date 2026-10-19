#pragma once
#include "LedSignal.h"
#include "BoardPins.h"
#include <Arduino.h>
#include <atomic>
#include <mutex>
#include <utility>
#include <driver/rmt_rx.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace PakarS3 {
// One loop-requested capture, owned by the LED worker at a frame boundary.
// RX DMA uses a small ring and allocation-free incremental comparison in its
// partial callbacks. No 46KB contiguous allocation, PSRAM or ISR heap/copy.
class LedSignalCapture {
 public:
  bool request() {
    unsigned old=state_.load();
    while(old==0 || old==3)if(state_.compare_exchange_weak(old,1))return true;
    return false;
  }
  void start(const uint8_t* expected,size_t bytes) {
    unsigned pending=1;if(!state_.compare_exchange_strong(pending,2))return;
    received_.store(false);error_=0;
    inspector_.reset(expected,bytes);callbackError_.store(0);
    buffer_=static_cast<rmt_symbol_word_t*>(heap_caps_aligned_alloc(32,capacity*sizeof(*buffer_),
        MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT));
    if(!buffer_){error_=ESP_ERR_NO_MEM;return;}
    rmt_rx_channel_config_t config{};config.gpio_num=static_cast<gpio_num_t>(PAKAR_S3_LED_DATA);
    config.clk_src=RMT_CLK_SRC_DEFAULT;config.resolution_hz=40000000;config.mem_block_symbols=capacity;
    // DMA RX uses the SDK GDMA interrupt allocation, not config.intr_priority.
    // Do not apply the TX-only cache-safe/level3 allocator override to this probe.
    config.flags.with_dma=true;config.flags.io_loop_back=true;
    error_=rmt_new_rx_channel(&config,&channel_);if(error_)return;
    rmt_rx_event_callbacks_t callbacks{};callbacks.on_recv_done=done;
    error_=rmt_rx_register_event_callbacks(channel_,&callbacks,this);if(error_)return;
    error_=rmt_enable(channel_);if(error_)return;enabled_=true;
    rmt_receive_config_t receive{};receive.signal_range_min_ns=100;receive.signal_range_max_ns=100000;
    receive.flags.en_partial_rx=true;
    error_=rmt_receive(channel_,buffer_,capacity*sizeof(*buffer_),&receive);
  }
  // Return false only if RX could not be quiesced: caller keeps flash blocked
  // and must NOT free a potentially DMA-owned buffer or continue transmission.
  bool finish(uint32_t revision,bool txOkay) {
    if(state_.load()!=2)return true;
    if(!error_ && txOkay) {
      const uint32_t at=millis();
      while(!received_.load(std::memory_order_acquire) && uint32_t(millis()-at)<30U)vTaskDelay(1);
      if(!received_.load(std::memory_order_acquire))error_=ESP_ERR_TIMEOUT;
    } else if(!error_)error_=ESP_ERR_INVALID_STATE;
    if(enabled_) {
      const int stopped=rmt_disable(channel_);
      if(stopped){publish({},revision,stopped,true);return false;}
      enabled_=false;
    }
    LedSignalReport report;
    if(!error_) {
      error_=callbackError_.load();
      if(!error_)report=inspector_.report();
    }
    if(channel_) {
      const int removed=rmt_del_channel(channel_);
      if(removed){publish(report,revision,removed,true);return false;}
      channel_=nullptr;
    }
    if(buffer_){heap_caps_free(buffer_);buffer_=nullptr;}
    publish(report,revision,error_);return true;
  }
  String json()const {
    std::lock_guard<std::mutex> lock(reportGate_);
    String out=F("{\"scope\":\"gpio-digital-not-optical\",\"method\":\"partial-rx-dma\",\"state\":");out+=state_.load();
    out+=F(",\"frame_revision\":");out+=revision_;
    out+=F(",\"error_code\":");out+=publishedError_;
    out+=F(",\"match\":");
    if(state_.load()!=3 || publishedError_)out+=F("null");else out+=report_.match?F("true"):F("false");
    for(const auto &field:{std::pair<const char*,uint32_t>{"observed_bits",report_.bits},
        {"expected_bits",report_.expectedBits},{"timing_errors",report_.timingErrors},
        {"first_mismatch_bit",report_.firstMismatch},{"minimum_high_ns",report_.minHighNs},
        {"maximum_high_ns",report_.maxHighNs},{"maximum_data_low_ns",report_.maxDataLowNs}}) {
      out+=F(",\"");out+=field.first;out+=F("\":");
      if(field.second==UINT32_MAX || state_.load()!=3 || publishedError_)out+=F("null");else out+=field.second;
    }
    out+='}';return out;
  }
 private:
  // Stock SDK RX is not cache-safe: its IRQ is suspended with flash cache off.
  // Keep the ordinary callback contract, not a header COMDAT in .iram1 whose
  // literals the Xtensa linker cannot place. TX's IRAM refill is unchanged.
  static bool done(rmt_channel_handle_t,const rmt_rx_done_event_data_t* event,void* owner) {
    auto* self=static_cast<LedSignalCapture*>(owner);
    const auto begin=reinterpret_cast<uintptr_t>(self->buffer_);
    const auto chunk=reinterpret_cast<uintptr_t>(event->received_symbols);
    const auto bytes=event->num_symbols*sizeof(rmt_symbol_word_t);
    if(event->num_symbols>capacity || chunk<begin || (chunk-begin)%alignof(rmt_symbol_word_t) ||
        chunk>begin+capacity*sizeof(rmt_symbol_word_t) ||
        bytes>begin+capacity*sizeof(rmt_symbol_word_t)-chunk)self->callbackError_.store(ESP_ERR_INVALID_SIZE);
    else if(!self->callbackError_.load())self->inspector_.feed(event->received_symbols,event->num_symbols);
    if(event->flags.is_last)self->received_.store(true,std::memory_order_release);
    return false;
  }
  void publish(const LedSignalReport& report,uint32_t revision,int error,bool stuck=false) {
    std::lock_guard<std::mutex> lock(reportGate_);report_=report;revision_=revision;
    publishedError_=error;state_.store(stuck?4:3);
  }
  static constexpr size_t capacity=2032; //8128bytes, two4064-byte aligned DMA halves
  std::atomic<unsigned> state_{0}; // idle,pending,capturing,complete,quiesce-failed
  std::atomic<bool> received_{false};
  rmt_channel_handle_t channel_=nullptr;rmt_symbol_word_t* buffer_=nullptr;
  bool enabled_=false;int error_=0;
  LedSignalInspector inspector_;std::atomic<int> callbackError_{0};
  mutable std::mutex reportGate_;LedSignalReport report_;uint32_t revision_=0;int publishedError_=0;
};
}
