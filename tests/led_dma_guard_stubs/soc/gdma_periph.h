#pragma once
#define SOC_GDMA_NUM_GROUPS_MAX 1U
#define SOC_GDMA_PAIRS_PER_GROUP_MAX 5
struct gdma_signal_conn_t{
  struct{const int module;struct{const int rx_irq_id;const int tx_irq_id;}pairs[5];}groups[1];
};
extern const gdma_signal_conn_t gdma_periph_signals;
