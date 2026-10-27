#pragma once
#include <Arduino.h>
#include <string.h>

// Applies only to our canonical status serializer. The sole optional field
// eligible for removal is the embedded base64 artwork, never peer identity,
// capabilities or observed controls. Its exact omission is flagged by caller.
inline bool omitUsbSnapshotArtwork(String &status){
  constexpr char marker[]=",\"icon_png_base64\":\"";
  const char *begin=strstr(status.c_str(),marker);
  if(!begin)return false;
  const char *value=begin+sizeof(marker)-1,*end=value;
  while(*end&&*end!='"'){
    const char c=*end;
    if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='+'||c=='/'||c=='='))return false;
    ++end;
  }
  if(*end!='"'||strstr(end+1,marker))return false;
  status.remove(size_t(begin-status.c_str()),size_t(end-begin)+1U);
  return true;
}
