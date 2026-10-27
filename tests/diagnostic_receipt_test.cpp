#include "DiagnosticReceipt.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
int main(int argc,char **argv){
  JsonDocument state;
  state["board_id"]="esp32s3-test";state["boot_id"]="0123456789abcdef0123456789abcdef";
  state["pixels"]=480;state["command_clock_ms"]=UINT64_C(42949672951);
  state["capabilities"]["role"]="table-selection";
  state["capabilities"]["application"]["icon_png_base64"]=std::string(2400,'a');
  state["capabilities"]["acknowledged_features"].to<JsonArray>().add("pixel-diagnostic-effects");
  state["led_output"]["diagnostic"]["effect"]="breathe";
  state["led_output"]["diagnostic"]["revision"]=17;
  state["pixel_groups"]["revision"]=9;
  state["pixel_groups"]["partial"]=true;
  auto groupCounts=state["pixel_groups"]["counts"].to<JsonArray>();
  for(uint8_t i=0;i<80;++i)groupCounts.add(i?6:5);
  state["led_output"]["driver"]["backend"]="rmt-dma";
  state["pixel_corrections_revision"]=4;state["pixel_corrections_active_revision"]=7;
  state["remote_firmware"]["irrelevant"]=std::string(18000,'x');
  String raw,result;serializeJson(state,raw);
  assert(diagnosticReceiptStatus(raw,R"({"revision":4294967295,"brightness":255,"pending":true})",result));
  assert(result.length()<7168&&result.find("remote_firmware")==String::npos);
  JsonDocument observed;assert(!deserializeJson(observed,result));
  assert(observed["command_clock_ms"]==UINT64_C(42949672951));
  assert(observed["capabilities"]["application"]["icon_png_base64"].as<std::string>().size()==2400);
  assert(observed["led_output"]["diagnostic"]["revision"]==17);
  assert(observed["pixel_groups"]["revision"]==9);
  assert(observed["pixel_groups"]["counts"].size()==80);
  assert(observed["pixel_groups"]["counts"][0]==5);
  assert(observed["pixel_frame"]["pending"]==true);
  assert(observed["pixel_frame"]["revision"]==UINT32_MAX);
  assert(diagnosticReceiptStatus(raw,"null",result));
  assert(!diagnosticReceiptStatus(raw,R"({"revision":1,"pending":false,"rgb":"ff0000"})",result));
  assert(result.empty());
  state["capabilities"]["application"]["icon_png_base64"]=std::string(8000,'a');
  raw=String();serializeJson(state,raw);
  assert(!diagnosticReceiptStatus(raw,"null",result)&&result.empty());
  state["capabilities"]["application"]["icon_png_base64"]="";
  state.remove("board_id");raw=String();serializeJson(state,raw);
  assert(!diagnosticReceiptStatus(raw,"null",result));
  assert(!diagnosticReceiptStatus("{malformed","null",result));
  if(argc==2){
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    std::string captured((std::istreambuf_iterator<char>(input)),{});
    if(captured.compare(0,3,"\xef\xbb\xbf")==0)captured.erase(0,3);
    // The supplied frame is deliberately fixture metadata: this is a byte
    // budget check against a read-only capture, not a live publication claim.
    assert(diagnosticReceiptStatus(String(captured),R"({"revision":4294967295,"brightness":255,"pending":false})",result));
    printf("Captured status projection: %zu input bytes, %zu receipt status bytes (limit7168); fixture frame metadata, not a hardware test.\n",captured.size(),result.size());
  }
  puts("USB diagnostic projection preserves authoritative fields, strips unrelated payloads and rejects malformed/oversized readback without truncation.");
}
