#pragma once
// Opt-in only. No pin is assigned or probed in normal production images.
#ifndef PAKAR_MICROSD_ENABLED
#define PAKAR_MICROSD_ENABLED 0
#endif
#if PAKAR_MICROSD_ENABLED
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Optional SD_MMC storage requires ESP32-S3"
#endif
#if !defined(PAKAR_SD_CLK) || !defined(PAKAR_SD_CMD) || !defined(PAKAR_SD_D0)
#error "Provide reviewed PAKAR_SD_CLK, PAKAR_SD_CMD and PAKAR_SD_D0 pins"
#endif
#include <SD_MMC.h>
namespace PakarStorage {
constexpr bool spare(int pin){return pin==1||pin==2||(pin>=39&&pin<=42);}
static_assert(spare(PAKAR_SD_CLK)&&spare(PAKAR_SD_CMD)&&spare(PAKAR_SD_D0)&&
    PAKAR_SD_CLK!=PAKAR_SD_CMD&&PAKAR_SD_CLK!=PAKAR_SD_D0&&PAKAR_SD_CMD!=PAKAR_SD_D0,
    "SD pins must be distinct and outside current UART/ISP/LED/LCD/Ethernet/USB/memory assignments");
inline bool mount(){return SD_MMC.setPins(PAKAR_SD_CLK,PAKAR_SD_CMD,PAKAR_SD_D0)&&SD_MMC.begin("/sdcard",true,false,10000,3);}
}
#endif
