#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_8BIT=1U<<2;
constexpr uint32_t MALLOC_CAP_SPIRAM=1U<<10;
namespace PakarPsramFixture {
inline bool fail=false;
inline unsigned allocations=0,releases=0;
inline uint32_t capabilities=0;
inline size_t size=0;
inline void *owned=nullptr;
}
inline void *heap_caps_malloc(size_t size,uint32_t capabilities) {
  using namespace PakarPsramFixture;
  ++allocations;PakarPsramFixture::size=size;PakarPsramFixture::capabilities=capabilities;
  assert(capabilities==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(fail)return nullptr;
  assert(owned==nullptr);owned=std::malloc(size);return owned;
}
inline void heap_caps_free(void *memory) {
  assert(memory==PakarPsramFixture::owned);++PakarPsramFixture::releases;
  std::free(memory);PakarPsramFixture::owned=nullptr;
}
