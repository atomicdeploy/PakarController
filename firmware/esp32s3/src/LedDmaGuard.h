#pragma once
#include <driver/rmt_tx.h>

namespace PakarS3 {
// Limit the IDF GDMA cache-safe allocation override to this task's one LED
// channel setup. Keep the scope around rmt_new_tx_channel(), not the worker.
// An ESP_OK RMT setup AND admitted() are both required before using the channel:
// IDF subsequently validates its registered callback and user-data addresses.
class LedDmaSetupScope {
 public:
  LedDmaSetupScope();
  ~LedDmaSetupScope();
  bool available() const;
  bool admitted() const;
  LedDmaSetupScope(const LedDmaSetupScope&)=delete;
  LedDmaSetupScope& operator=(const LedDmaSetupScope&)=delete;
 private:
  void* task_=nullptr;
  bool active_=false;
};
// Allocate an unenabled RMT-DMA channel only after the scoped safe GDMA
// allocation was witnessed. SDK callback validation must also have succeeded.
// As with rmt_new_tx_channel, the caller enables/owns a successful channel.
esp_err_t newCacheSafeLedChannel(const rmt_tx_channel_config_t*,rmt_channel_handle_t*);
}
