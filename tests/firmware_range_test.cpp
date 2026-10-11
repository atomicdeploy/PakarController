#include "FirmwareRange.h"
#include <assert.h>
#include <stdio.h>
using namespace FirmwareTransferRange;
int main() {
  FirmwareRange range{};
  assert(parseRange("bytes=0-9", 100, range) && range.start == 0 && range.end == 9 && range.partial);
  assert(parseRange("bytes=80-", 100, range) && range.start == 80 && range.end == 99);
  assert(parseRange("bytes=-10", 100, range) && range.start == 90 && range.end == 99);
  assert(parseRange("bytes=-200", 100, range) && range.start == 0 && range.end == 99);
  assert(parseRange("bytes=80-999", 100, range) && range.end == 99);
  assert(parseRange(" bytes=1-2 \r\n", 100, range) && range.start == 1 && range.end == 2);
  const char *bad[] = {"", "bytes=-", "bytes=-0", "bytes=100-", "bytes=99-98", "bytes=0-1,3-5", "bytes=1--2", "bytes=+1-2", "bytes=4294967296-", "bytes=0-18446744073709551616", "items=0-1"};
  for (const auto value : bad) assert(!parseRange(value, 100, range));
  assert(!parseRange("bytes=0-0", 0, range));
  puts("Imported PU850 production byte-range parser bounds, suffix/open ranges and overflow rejection passed.");
}
