#include "../firmware/esp32s3/src/BleStorage.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace PakarS3;
using namespace BoardCore::BleWire;
int main() {
  static_assert(sizeof(BleBulkOwner)==sizeof(void*),"Do not retain an internal bulk-buffer fallback");
  static_assert(sizeof(BleBulkStorage)>=8*512+kRequest+kPacket,"All BLE application buffers must move together");
  using namespace PakarPsramFixture;
  {
    BleBulkOwner missing;
    assert(!missing&&missing.get()==nullptr&&allocations==0);
    fail=true;
    assert(!missing.begin()&&!missing&&allocations==1&&releases==0);
    assert(capabilities==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    assert(size==sizeof(BleBulkStorage)); // One explicit allocation, no DRAM fallback.
    fail=false;
    assert(missing.begin()&&missing&&allocations==2);
    auto *storage=missing.get();assert(storage);
    assert(missing.begin()&&missing.get()==storage&&allocations==2); // Stable callback lifetime.
    for(const auto &item:storage->ingress)assert(item.peer==0xffff&&item.length==0&&item.bytes[511]==0);
    storage->ingress[7].length=512;storage->ingress[7].bytes[511]=0xa5;
    assert(!storage->receiver.ready()&&!storage->sender.busy());
    std::vector<uint8_t> bytes(kRequest);
    for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<uint8_t>(i);
    uint8_t packet[kPacket]{},ack[kHeader]{};
    // Actual unchanged production framing in PSRAM storage, at minimum MTU.
    for(size_t offset=0;offset<bytes.size();) {
      const size_t count=bytes.size()-offset<13?bytes.size()-offset:13;
      header(packet,Data,4,static_cast<uint16_t>(offset),static_cast<uint16_t>(bytes.size()));
      memcpy(packet+kHeader,bytes.data()+offset,count);
      const auto result=storage->receiver.accept(packet,kHeader+count,100,ack);
      offset+=count;
      assert(result==(offset==bytes.size()?Receiver::Complete:Receiver::Accepted));
      assert(get16(ack+3)==offset);
    }
    assert(storage->receiver.ready()&&storage->receiver.size()==bytes.size());
    assert(!memcmp(storage->receiver.data(),bytes.data(),bytes.size()));
    storage->receiver.consumed();
    assert(storage->receiver.accept(packet,kHeader+2,101,ack)==Receiver::Duplicate);
    assert(storage->sender.begin(8,bytes.data(),32));
    size_t length=0;const auto *first=storage->sender.poll(100,23,length);
    assert(first&&length==20);
    uint8_t retained[kPacket];memcpy(retained,first,length);
    const auto *retry=storage->sender.poll(100+kRetryMs,517,length);
    assert(retry==first&&length==20&&!memcmp(retry,retained,length));
    storage->receiver.reset();storage->sender.reset();
    assert(!storage->receiver.ready()&&!storage->sender.busy());
    assert(storage->ingress[7].bytes[511]==0xa5&&allocations==2&&releases==0);
  }
  assert(releases==1&&owned==nullptr);
  puts("BLE application bulk storage: explicit PSRAM-only allocation, failure without DRAM fallback, stable lifetime, full8192-byte minimum-MTU framing and exact fragment retry fixtures passed; not heap/RF/OTA acceptance.");
}
