#pragma once
#include "rmt_tx.h"
struct rmt_rx_done_event_data_t {
 const rmt_symbol_word_t* received_symbols=nullptr;size_t num_symbols=0;
 struct {bool is_last=false;} flags;
};
using rmt_rx_done_callback_t=bool(*)(rmt_channel_handle_t,const rmt_rx_done_event_data_t*,void*);
struct rmt_rx_event_callbacks_t{rmt_rx_done_callback_t on_recv_done=nullptr;};
struct rmt_rx_channel_config_t{
 gpio_num_t gpio_num=0;int clk_src=0,intr_priority=0;unsigned resolution_hz=0,mem_block_symbols=0;
 struct{bool with_dma=false,io_loop_back=false;}flags;
};
struct rmt_receive_config_t{
 unsigned signal_range_min_ns=0,signal_range_max_ns=0;
 struct {bool en_partial_rx=false;} flags;
};
int rmt_new_rx_channel(const rmt_rx_channel_config_t*,rmt_channel_handle_t*);
int rmt_rx_register_event_callbacks(rmt_channel_handle_t,const rmt_rx_event_callbacks_t*,void*);
int rmt_receive(rmt_channel_handle_t,void*,size_t,const rmt_receive_config_t*);
