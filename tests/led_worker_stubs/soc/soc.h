#pragma once
void fixtureGpioWrite(unsigned,unsigned);
#define REG_WRITE(address,value) fixtureGpioWrite(address,value)
