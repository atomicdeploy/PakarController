#pragma once
#include <cstddef>
#include <cstdint>
using gpio_num_t=int;
using rmt_channel_handle_t=void*;
using rmt_encoder_handle_t=void*;
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NO_MEM=0x101,ESP_ERR_TIMEOUT=0x107,RMT_CLK_SRC_DEFAULT=0;
constexpr int ESP_ERR_INVALID_RESPONSE=0x108;
struct rmt_tx_done_event_data_t{size_t num_symbols=0;};
using rmt_tx_done_callback_t=bool(*)(rmt_channel_handle_t,const rmt_tx_done_event_data_t*,void*);
struct rmt_tx_event_callbacks_t{rmt_tx_done_callback_t on_trans_done=nullptr;};
struct rmt_symbol_word_t{uint16_t duration0=0,level0=0,duration1=0,level1=0;};
struct rmt_tx_channel_config_t{
 gpio_num_t gpio_num=0;int clk_src=0;unsigned resolution_hz=0,mem_block_symbols=0,trans_queue_depth=0,intr_priority=0;
 struct{bool with_dma=false;}flags;
};
struct rmt_transmit_config_t{int loop_count=0;struct{int eot_level=0;bool queue_nonblocking=false;}flags;};
int rmt_new_tx_channel(const rmt_tx_channel_config_t*,rmt_channel_handle_t*);
int rmt_enable(rmt_channel_handle_t);
int rmt_tx_register_event_callbacks(rmt_channel_handle_t,const rmt_tx_event_callbacks_t*,void*);
int rmt_disable(rmt_channel_handle_t);
int rmt_del_channel(rmt_channel_handle_t);
int rmt_transmit(rmt_channel_handle_t,rmt_encoder_handle_t,const void*,size_t,const rmt_transmit_config_t*);
int rmt_tx_wait_all_done(rmt_channel_handle_t,int);
