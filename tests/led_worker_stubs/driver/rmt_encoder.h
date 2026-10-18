#pragma once
#include "rmt_tx.h"
struct rmt_copy_encoder_config_t{};
struct rmt_bytes_encoder_config_t{
 rmt_symbol_word_t bit0{},bit1{};
 struct{unsigned msb_first=0;}flags;
};
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t*);
int rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*,rmt_encoder_handle_t*);
int rmt_del_encoder(rmt_encoder_handle_t);
