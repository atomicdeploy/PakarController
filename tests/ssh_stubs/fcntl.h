#pragma once
// Embedded socket API declaration for host syntax checks only.
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 0x4000
int fcntl(int,int,...);
