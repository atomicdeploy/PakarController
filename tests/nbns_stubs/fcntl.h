#pragma once
#include "lwip/sockets.h"
constexpr int F_GETFL=1,F_SETFL=2,O_NONBLOCK=4;
inline int nbnsFcntl(int fd,int operation,int flags){if(operation==F_GETFL)return 0;if(operation==F_SETFL){NbnsFixture::sockets.at(fd).nonblocking=(flags&O_NONBLOCK)!=0;return 0;}return -1;}
#define fcntl nbnsFcntl
