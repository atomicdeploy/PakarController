#pragma once
#include <stddef.h>
#define MSG_DONTWAIT 1
int send(int,const void*,size_t,int);
int recv(int,void*,size_t,int);
