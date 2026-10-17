#pragma once
namespace FlashGuardFixture {inline bool cacheEnabled=true;}
inline bool spi_flash_cache_enabled(){return FlashGuardFixture::cacheEnabled;}
