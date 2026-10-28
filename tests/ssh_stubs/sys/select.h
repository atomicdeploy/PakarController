#pragma once
#if defined(_WIN32)
#include <winsock2.h>
#else
#include_next <sys/select.h>
#endif
