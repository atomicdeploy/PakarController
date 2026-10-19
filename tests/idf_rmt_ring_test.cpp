// Actual pinned IDF encoders and EOF handler, bounded host DMA descriptor model.
// This cannot measure RF, ISR latency, waveform quality or physical LED output.
#include "../firmware/esp32s3/src/LedWire.h"
#include "../firmware/esp32s3/src/LedSignal.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct rmt_symbol_word_t {
  uint32_t duration0:15,level0:1,duration1:15,level1:1;
};
static_assert(sizeof(rmt_symbol_word_t)==4,"The actual SDK symbol is four bytes");
using esp_err_t=int;
constexpr int ESP_OK=0;
using rmt_encode_state_t=unsigned;
constexpr rmt_encode_state_t RMT_ENCODING_RESET=0,RMT_ENCODING_COMPLETE=1,
    RMT_ENCODING_MEM_FULL=2,RMT_ENCODING_WITH_EOF=4;
constexpr unsigned DMA_DESCRIPTOR_BUFFER_OWNER_CPU=0,DMA_DESCRIPTOR_BUFFER_OWNER_DMA=1;
constexpr unsigned SOC_RMT_MEM_WORDS_PER_CHANNEL=48;
struct rmt_channel_t;
using rmt_channel_handle_t=rmt_channel_t*;
struct rmt_encoder_t {
  size_t (*encode)(rmt_encoder_t*,rmt_channel_handle_t,const void*,size_t,rmt_encode_state_t*)=nullptr;
  esp_err_t (*reset)(rmt_encoder_t*)=nullptr;
};
using rmt_encoder_handle_t=rmt_encoder_t*;
struct rmt_group_t {int spinlock=0;struct {void* regs=nullptr;}hal;};
struct rmt_channel_t {
  void* dma_chan=nullptr;rmt_group_t* group=nullptr;int channel_id=0;
  size_t mem_block_num=1;rmt_symbol_word_t* hw_mem_base=nullptr;
};
struct rmt_dma_descriptor_t {
  struct {size_t size=0,length=0;unsigned owner=0,suc_eof=0;}dw0;
  rmt_symbol_word_t* buffer=nullptr;rmt_dma_descriptor_t* next=nullptr;
};
struct rmt_tx_trans_desc_t {
  rmt_encoder_handle_t encoder=nullptr;const void* payload=nullptr;size_t payload_bytes=0;
  int loop_count=0;size_t transmitted_symbol_num=0;
  struct {unsigned eot_level:1,encoding_done:1,need_eof_mark:1;}flags{};
};
struct rmt_tx_channel_t {
  rmt_channel_t base;rmt_symbol_word_t* dma_mem_base_nc=nullptr;
  size_t mem_off_bytes=0,mem_end=0,ping_pong_symbols=0;
  rmt_dma_descriptor_t* dma_nodes_nc=nullptr;rmt_tx_trans_desc_t* cur_trans=nullptr;
};
using gdma_channel_handle_t=void*;
struct gdma_event_data_t {uintptr_t tx_eof_desc_addr=0;};
unsigned appendCalls=0;
int gdma_append(gdma_channel_handle_t){++appendCalls;return ESP_OK;}
void rmt_ll_enable_interrupt(void*,unsigned,bool){assert(false&&"DMA fixture entered non-DMA path");}
uint8_t hal_utils_bitwise_reverse8(uint8_t value){
  uint8_t result=0;for(unsigned bit=0;bit<8;++bit){result=uint8_t((result<<1)|(value&1U));value>>=1;}return result;
}
#define __containerof(pointer,type,member) reinterpret_cast<type*>(reinterpret_cast<char*>(pointer)-offsetof(type,member))
#define MIN(a,b) std::min((a),(b))
#define RMT_GET_NON_CACHE_ADDR(address) (address)
#define RMT_LL_EVENT_TX_THRES(channel) (1U<<(channel))
#define portENTER_CRITICAL_ISR(mux) ((void)(mux))
#define portEXIT_CRITICAL_ISR(mux) ((void)(mux))
#define unlikely(condition) (condition)
#define ESP_DRAM_LOGE(...) std::abort()
#define TAG "rmt"
#include "idf_rmt_ring_sdk.inc"

namespace {
constexpr size_t kPixels=480,kDataSymbols=kPixels*24,kHalf=1023;
struct Rgb {uint8_t r,g,b;};
bool same(const rmt_symbol_word_t& a,const rmt_symbol_word_t& b){
  return a.duration0==b.duration0&&a.duration1==b.duration1&&a.level0==b.level0&&a.level1==b.level1;
}
bool equal(const std::vector<rmt_symbol_word_t>& a,const std::vector<rmt_symbol_word_t>& b){
  return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),same);
}
enum class Fault {None,DelayWithinWindow,DelayPastReuse,DuplicateEof,CpuOwner,InvalidLength};
struct Ring {
  rmt_group_t group;std::vector<rmt_symbol_word_t> memory;
  std::array<rmt_dma_descriptor_t,2> descriptors;
  rmt_tx_channel_t channel;rmt_tx_trans_desc_t transaction;
  rmt_bytes_encoder_t bytes{};rmt_copy_encoder_t copy{};
  std::vector<rmt_symbol_word_t> observed;
  unsigned windows=0,callbacks=0;bool ownerError=false,lengthError=false,ended=false;
  explicit Ring(size_t half=kHalf):memory(half*2){
    channel.base.dma_chan=this;channel.base.group=&group;channel.dma_mem_base_nc=memory.data();
    channel.ping_pong_symbols=half;channel.dma_nodes_nc=descriptors.data();
    channel.cur_trans=&transaction;
    bytes.base.encode=rmt_encode_bytes;bytes.base.reset=rmt_bytes_encoder_reset;bytes.flags.msb_first=1;
    bytes.bit0={PakarS3::zeroHigh,1,PakarS3::zeroLow,0};
    bytes.bit1={PakarS3::oneHigh,1,PakarS3::oneLow,0};
    copy.base.encode=rmt_encode_copy;copy.base.reset=rmt_copy_encoder_reset;
  }
  void start(const std::vector<uint8_t>& payload,const std::vector<rmt_symbol_word_t>& symbols,bool useCopy){
    // Exact ring/offset/reset sequencing from IDF rmt_tx_do_transaction.
    // Hardware register/start calls are deliberately not simulated as proof.
    for(size_t i=0;i<2;++i){
      auto& descriptor=descriptors[i];descriptor.buffer=memory.data()+i*channel.ping_pong_symbols;
      descriptor.dw0.size=channel.ping_pong_symbols*sizeof(rmt_symbol_word_t);
      descriptor.dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;descriptor.dw0.suc_eof=1;
      descriptor.next=&descriptors[(i+1)%2];
    }
    transaction.encoder=useCopy?&copy.base:&bytes.base;
    transaction.payload=useCopy?static_cast<const void*>(symbols.data()):payload.data();
    transaction.payload_bytes=useCopy?symbols.size()*sizeof(symbols[0]):payload.size();
    transaction.encoder->reset(transaction.encoder);
    channel.mem_off_bytes=0;channel.mem_end=channel.ping_pong_symbols*2;
    transaction.transmitted_symbol_num=rmt_encode_check_result(&channel,&transaction);
    channel.mem_end=channel.ping_pong_symbols;appendCalls=0;
  }
  void eof(rmt_dma_descriptor_t* descriptor){
    gdma_event_data_t event{reinterpret_cast<uintptr_t>(descriptor)};
    assert(!rmt_dma_tx_eof_cb(this,&event,&channel));++callbacks;
  }
  void run(Fault fault=Fault::None){
    auto* descriptor=&descriptors[0];
    if(fault==Fault::CpuOwner)descriptor->dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
    if(fault==Fault::InvalidLength)descriptor->dw0.length=descriptor->dw0.size+4;
    rmt_dma_descriptor_t* deferred=nullptr;
    for(unsigned bound=0;bound<32&&descriptor;++bound){
      // Model auto_update_desc/owner_check, not the SDK's encoded count.
      if(descriptor->dw0.owner!=DMA_DESCRIPTOR_BUFFER_OWNER_DMA){ownerError=true;return;}
      const size_t size=descriptor->dw0.length;
      if(!size||size>descriptor->dw0.size||size%sizeof(rmt_symbol_word_t)){lengthError=true;return;}
      auto* next=descriptor->next; // DMA has obtained the next link before EOF ISR.
      for(size_t i=0;i<size/sizeof(rmt_symbol_word_t);++i){
        const auto symbol=descriptor->buffer[i];observed.push_back(symbol);
        if(!symbol.duration0&&!symbol.duration1){ended=true;break;}
      }
      descriptor->dw0.owner=DMA_DESCRIPTOR_BUFFER_OWNER_CPU;++windows;
      if((fault==Fault::DelayWithinWindow||fault==Fault::DelayPastReuse)&&windows==1)deferred=descriptor;
      else{
        // The first delayed ISR is ordered before the second ISR and before
        // descriptor reuse. At/past that window this model correctly underruns.
        if(deferred&&fault==Fault::DelayWithinWindow){eof(deferred);deferred=nullptr;}
        if(fault!=Fault::DelayPastReuse||!deferred)eof(descriptor);
        if(fault==Fault::DuplicateEof&&windows==1)eof(descriptor);
      }
      if(ended){assert(!next);return;}
      descriptor=next;
    }
    assert(false&&"DMA link did not terminate within a bounded frame");
  }
};
void complete(const std::vector<uint8_t>& payload,const std::vector<rmt_symbol_word_t>& expected,bool useCopy,Fault fault){
  Ring ring;ring.start(payload,expected,useCopy);ring.run(fault);
  auto wire=expected;wire.push_back({0,0,0,0});
  assert(!ring.ownerError&&!ring.lengthError&&ring.ended);
  assert(equal(ring.observed,wire));assert(ring.transaction.transmitted_symbol_num==kDataSymbols+1);
  assert(ring.windows==12&&ring.callbacks==12&&appendCalls==10);
  // Each half ends at a non-byte boundary; all 480 pixels, including tail,
  // have been compared, not just one ideal whole-frame expansion.
  assert(ring.observed.size()==11521);
}
void scenarios(unsigned pattern){
  std::array<Rgb,kPixels> rgb{};
  for(size_t i=0;i<rgb.size();++i)
    rgb[i]=pattern==0?Rgb{0,0,0}:pattern==1?Rgb{255,255,255}:
      Rgb{uint8_t(i*73U+11U),uint8_t(i*19U+7U),uint8_t(i*127U+3U)};
  std::vector<uint8_t> payload(kPixels*3);
  PakarS3::encodeWireBytes(rgb.data(),rgb.size(),255,true,payload.data());
  std::vector<rmt_symbol_word_t> expected(kDataSymbols+1);
  PakarS3::encodeWire(rgb.data(),rgb.size(),255,true,expected.data());expected.resize(kDataSymbols);
  for(bool useCopy:{false,true}){
    complete(payload,expected,useCopy,Fault::None);
    complete(payload,expected,useCopy,Fault::DelayWithinWindow);
    Ring late;late.start(payload,expected,useCopy);late.run(Fault::DelayPastReuse);
    assert(late.ownerError&&!late.ended&&late.windows==2&&late.observed.size()==kHalf*2);
    Ring owner;owner.start(payload,expected,useCopy);owner.run(Fault::CpuOwner);
    assert(owner.ownerError&&owner.observed.empty());
    Ring length;length.start(payload,expected,useCopy);length.run(Fault::InvalidLength);
    assert(length.lengthError&&length.observed.empty());
    Ring duplicate;duplicate.start(payload,expected,useCopy);duplicate.run(Fault::DuplicateEof);
    auto wire=expected;wire.push_back({0,0,0,0});
    assert(duplicate.ended&&!equal(duplicate.observed,wire));
    // A software total can still claim the full frame despite a skipped
    // descriptor window. This is fault injection, NOT observed board causality.
    assert(duplicate.transaction.transmitted_symbol_num==kDataSymbols+1);
    // The duplicate advances to the still-unread other half. The next normal
    // callback then overwrites the first refill before DMA reaches it, losing
    // two windows, not merely one. Retain the exact modeled length assertion.
    if(duplicate.observed.size()!=kDataSymbols+1-kHalf*2)
      fprintf(stderr,"Injected duplicate EOF: encoded=%zu modeled_wire=%zu windows=%u\n",duplicate.transaction.transmitted_symbol_num,duplicate.observed.size(),duplicate.windows);
    assert(duplicate.observed.size()==kDataSymbols+1-kHalf*2&&duplicate.windows==10);
  }
}
void boundaries(){
  // Exercise encoding-complete exactly at a half/full boundary (deferred EOF),
  // and a frame completed on initial encode (no refill, final callbacks remain).
  // 2032 symbols is a legal native ring; unlike a2048-symbol convenience
  // boundary it does not exceed the4095-byte descriptor limit.
  for(size_t count:{size_t(126),size_t(127),size_t(254),size_t(255)}){
    std::vector<uint8_t> payload(count,0xa6);std::vector<rmt_symbol_word_t> expected(count*8);
    for(size_t i=0;i<expected.size();++i)expected[i]=(payload[i/8]&(0x80>>(i%8)))?
      rmt_symbol_word_t{PakarS3::oneHigh,1,PakarS3::oneLow,0}:
      rmt_symbol_word_t{PakarS3::zeroHigh,1,PakarS3::zeroLow,0};
    for(bool useCopy:{false,true}){
      Ring ring(1016);ring.start(payload,expected,useCopy);ring.run();
      auto wire=expected;wire.push_back({0,0,0,0});
      assert(ring.ended&&!ring.ownerError&&equal(ring.observed,wire));
      assert(ring.transaction.transmitted_symbol_num==count*8+1);
      assert(ring.callbacks==ring.windows);
      if(count==126)assert(appendCalls==0&&ring.callbacks==1);
    }
  }
}
}
void signalCaptureChecks(){
  std::array<Rgb,kPixels> rgb{};for(size_t i=0;i<rgb.size();++i)rgb[i]={uint8_t(i),uint8_t(i*7),uint8_t(i*31)};
  std::vector<uint8_t> expected(kPixels*3);PakarS3::encodeWireBytes(rgb.data(),rgb.size(),255,true,expected.data());
  std::vector<rmt_symbol_word_t> wire(kDataSymbols+1);PakarS3::encodeWire(rgb.data(),rgb.size(),255,true,wire.data());
  assert(PakarS3::inspectLedSignal(wire.data(),wire.size(),expected.data(),expected.size()).match);
  auto shortFrame=PakarS3::inspectLedSignal(wire.data(),23*6*24,expected.data(),expected.size());
  assert(!shortFrame.match && shortFrame.bits==23*6*24 && shortFrame.firstMismatch==23*6*24);
  const auto bit=23*6*24;auto saved=wire[bit];
  wire[bit].duration0=saved.duration0==PakarS3::zeroHigh?PakarS3::oneHigh:PakarS3::zeroHigh;
  wire[bit].duration1=50-wire[bit].duration0;
  auto corrupt=PakarS3::inspectLedSignal(wire.data(),wire.size(),expected.data(),expected.size());
  assert(!corrupt.match && corrupt.bits==11520 && corrupt.firstMismatch==bit);
  wire[bit]=saved;wire[bit].duration1=150;
  auto gap=PakarS3::inspectLedSignal(wire.data(),wire.size(),expected.data(),expected.size());
  assert(!gap.match && gap.timingErrors==1 && gap.maxDataLowNs==3750);
  wire[bit]=saved;wire.back()={};wire[kDataSymbols-1].duration1=4000; // Last low includes reset, not corrupt data.
  assert(PakarS3::inspectLedSignal(wire.data(),wire.size(),expected.data(),expected.size()).match);
  wire.insert(wire.begin(),{4000,0,0,0}); // Idle before first high is harmless.
  assert(PakarS3::inspectLedSignal(wire.data(),wire.size(),expected.data(),expected.size()).match);
  // RX callbacks recycle a two-half ring. Comparison state must survive every
  // chunk, including a symbol with LOW first and HIGH second at the boundary.
  for(size_t window:{size_t(1),size_t(1016)}){
    PakarS3::LedSignalInspector incremental;incremental.reset(expected.data(),expected.size());
    for(size_t at=0;at<wire.size();at+=window)
      incremental.feed(wire.data()+at,std::min(window,wire.size()-at));
    assert(incremental.report().match && incremental.report().bits==11520);
  }
  std::vector<rmt_symbol_word_t> shifted(wire.size()-1);
  for(size_t i=0;i<shifted.size();++i){
    shifted[i]={i?wire[i].duration1:4000U,0,wire[i+1].duration0,1};
  }
  PakarS3::LedSignalInspector shiftedInspector;shiftedInspector.reset(expected.data(),expected.size());
  for(size_t at=0;at<shifted.size();at+=1016)
    shiftedInspector.feed(shifted.data()+at,std::min(size_t(1016),shifted.size()-at));
  assert(shiftedInspector.report().match);
  puts("Digital capture decoder: complete480 pixels, recycled partial ring, first23-cell cutoff, full-length corruption, inter-bit gap and initial/final idle checked; no analogue/optical proof.");
}
int main(){
  for(unsigned pattern=0;pattern<3;++pattern)scenarios(pattern);
  boundaries();
  signalCaptureChecks();
  puts("Actual IDF5.5.5 bytes/copy + EOF functions: full480 varied/black/white frame,12 DMA windows/10 refills, half/full boundary EOF, initial completion, timely delayed ISR and injected late/duplicate/owner/length faults checked.");
  puts("SOFTWARE MODEL ONLY: duplicate EOF can leave encoded_count11521 despite corrupted modeled stream; no physical/RF/ISR deadline/OTA/optical acceptance.");
}
