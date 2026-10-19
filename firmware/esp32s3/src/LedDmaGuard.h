#pragma once
#include <driver/rmt_tx.h>

namespace PakarS3 {
struct LedDmaIrqReceipt {
  bool witnessed=false;
  int source=-1,cpu=-1,vector=-1,level=-1;
};
// Copy of the last successfully admitted channel's measured refill IRQ.
// Task-context getter; no SDK handle or private driver layout is exposed.
LedDmaIrqReceipt ledDmaIrqReceipt();
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
// Allocate an unenabled RMT-DMA channel only after the scoped safe GDMA and
// measured priority-3/core-1 refill IRQ were witnessed. SDK callback validation
// must also have succeeded; config.intr_priority alone covers only the RMT IRQ.
// As with rmt_new_tx_channel, the caller enables/owns a successful channel.
esp_err_t newCacheSafeLedChannel(const rmt_tx_channel_config_t*,rmt_channel_handle_t*);
}
