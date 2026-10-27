#pragma once
// Host fixture only; actual AVR compilation uses avr-libc interrupt masking.
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(mode) for(bool once=true;once;once=false)
