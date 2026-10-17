#pragma once
#include "rmt_tx.h"
struct rmt_copy_encoder_config_t{};
int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*,rmt_encoder_handle_t*);
int rmt_del_encoder(rmt_encoder_handle_t);
