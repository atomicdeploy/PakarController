#include "UsbSnapshotProjection.h"
#include <cassert>
#include <cstdio>
int main(){
  String status=R"({"board_id":"board","capabilities":{"role":"table-selection","application":{"id":"table","icon_png_base64":"Ab09+/=="}},"cells":["white"]})";
  assert(omitUsbSnapshotArtwork(status));
  assert(status==R"({"board_id":"board","capabilities":{"role":"table-selection","application":{"id":"table"}},"cells":["white"]})");
  assert(!omitUsbSnapshotArtwork(status));
  for(const char *bad:{R"({"icon_png_base64":"abc"})",R"({"id":1,"icon_png_base64":"bad\"data"})",R"({"id":1,"icon_png_base64":"abc","other":{"id":1,"icon_png_base64":"def"}})",R"({"id":1,"icon_png_base64":"unterminated})"}){
    String value=bad,before=value;assert(!omitUsbSnapshotArtwork(value));assert(value==before);
  }
  puts("USB partial snapshot strips only one valid canonical artwork field and rejects ambiguous projections.");
}
