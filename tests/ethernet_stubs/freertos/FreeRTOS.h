#pragma once
using TaskHandle_t=void*;
constexpr int taskSCHEDULER_RUNNING=2;
inline TaskHandle_t ethernetCurrentTask=reinterpret_cast<void*>(1);
inline bool ethernetIsr=false,ethernetCanYield=true;
inline int xPortInIsrContext(){return ethernetIsr;}
inline bool xPortCanYield(){return ethernetCanYield;}
