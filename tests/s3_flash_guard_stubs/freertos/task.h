#pragma once
constexpr int taskSCHEDULER_SUSPENDED=0,taskSCHEDULER_NOT_STARTED=1,taskSCHEDULER_RUNNING=2;
namespace FlashGuardFixture {inline int scheduler=taskSCHEDULER_RUNNING;}
inline int xTaskGetSchedulerState(){return FlashGuardFixture::scheduler;}
