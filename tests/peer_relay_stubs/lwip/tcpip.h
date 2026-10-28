#pragma once
#include "dns.h"
inline err_t tcpip_callback(void (*fn)(void*),void *arg){fn(arg);return ERR_OK;}
