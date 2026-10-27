#pragma once
#include "AsyncHttpService.h"
#include "StaticAssetPolicy.h"

// Public, compiled assets only. Never map URLs to arbitrary filesystem paths.
// Reuse the tested short-write/ACK-safe response, including for HEAD.
inline void serveStaticAsset(AsyncWebServerRequest *request,const char *type,
    const void *bytes,size_t length,const char *etag,bool gzip=false){
  const String encoding=request->hasHeader("Accept-Encoding")?request->getHeader("Accept-Encoding")->value():String();
  const bool acceptable=PakarStatic::accepts(request->hasHeader("Accept-Encoding")?encoding.c_str():nullptr,gzip?"gzip":"identity");
  const String validator=request->hasHeader("If-None-Match")?request->getHeader("If-None-Match")->value():String();
  const bool unchanged=acceptable&&PakarStatic::notModified(validator.c_str(),etag);
  // Compressed-only assets return 406 instead of delivering undecodable bytes.
  // Do not retain an extra half-megabyte identity page in the OTA slot.
  const int status=!acceptable?406:unchanged?304:200;
  auto *response=new PakarHttpDetail::SnapshotResponse(status,type,
      status==200?static_cast<const char*>(bytes):nullptr,status==200?length:0,{});
  response->addHeader("Vary","Accept-Encoding");
  response->addHeader("Cache-Control",acceptable?"public,no-cache":"no-store");
  response->addHeader("X-Content-Type-Options","nosniff");
  response->addHeader("Referrer-Policy","same-origin");
  if(acceptable)response->addHeader("ETag",etag);
  if(gzip&&acceptable)response->addHeader("Content-Encoding","gzip");
  if(!acceptable)response->addHeader("Accept-Encoding",gzip?"gzip":"identity");
  // RFC 9110: a 304 has no body, and a Content-Length of 0 would falsely
  // describe the selected representation. SnapshotResponse omits it for 304.
  request->send(response);
}
