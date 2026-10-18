#pragma once
#include <esp_err.h>
using rmt_channel_handle_t=void*;
struct rmt_tx_channel_config_t{struct{unsigned with_dma:1;}flags{};};
extern "C" esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*,rmt_channel_handle_t*);
extern "C" esp_err_t rmt_del_channel(rmt_channel_handle_t);
