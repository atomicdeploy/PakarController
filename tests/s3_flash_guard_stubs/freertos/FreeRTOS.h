#pragma once
namespace FlashGuardFixture {inline bool isr=false,canYield=true;}
inline bool xPortInIsrContext(){return FlashGuardFixture::isr;}
inline bool xPortCanYield(){return FlashGuardFixture::canYield;}
