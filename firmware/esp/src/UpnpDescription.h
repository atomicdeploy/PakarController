#pragma once
#include <stdint.h>
namespace PakarDiscovery {
struct Description {
  const char *type,*name,*manufacturer,*manufacturerUrl,*model,*version,*modelUrl,*serial,*uuid,*presentation;
  unsigned iconWidth,iconHeight,iconDepth;
  uint32_t configId;
};
// Adapt PU850 SSDPDevice::schema's streaming XML escaping, not its obsolete
// URLBase or HTTP framing. Relative URLs resolve against the actual LOCATION
// used by the client, retaining AP/STA/Ethernet interface and custom port.
template<class Output> void escaped(Output& out,const char* text){
  for(;text&&*text;++text)switch(*text){
    case '&':out.print("&amp;");break;case '<':out.print("&lt;");break;
    case '>':out.print("&gt;");break;case '\"':out.print("&quot;");break;
    case '\'':out.print("&apos;");break;default:out.write(uint8_t(*text));break;
  }
}
template<class Output> void description(Output& out,const Description& d){
  out.print("<?xml version=\"1.0\" encoding=\"utf-8\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"");out.print(d.configId);
  out.print("\"><specVersion><major>1</major><minor>1</minor></specVersion><device><deviceType>");escaped(out,d.type);
  out.print("</deviceType><friendlyName>");escaped(out,d.name);
  out.print("</friendlyName><manufacturer>");escaped(out,d.manufacturer);
  out.print("</manufacturer><manufacturerURL>");escaped(out,d.manufacturerUrl);
  out.print("</manufacturerURL><modelName>");escaped(out,d.model);
  out.print("</modelName><modelNumber>");escaped(out,d.version);
  out.print("</modelNumber><modelURL>");escaped(out,d.modelUrl);
  out.print("</modelURL><serialNumber>");escaped(out,d.serial);
  out.print("</serialNumber><UDN>uuid:");escaped(out,d.uuid);
  out.print("</UDN><iconList><icon><mimetype>image/png</mimetype><width>");out.print(d.iconWidth);
  out.print("</width><height>");out.print(d.iconHeight);out.print("</height><depth>");out.print(d.iconDepth);
  out.print("</depth><url>/board-icon.png</url></icon></iconList><presentationURL>");escaped(out,d.presentation);
  out.print("</presentationURL></device></root>\r\n");
}
// Device description cache identity: field separators prevent concatenation
// collisions; 24-bit CONFIGID is independent of boot and changes with artwork.
inline uint32_t configId(const char* const* fields,unsigned count){
  uint32_t value=2166136261U;
  for(unsigned i=0;i<count;++i){for(const char* p=fields[i];p&&*p;++p){value^=uint8_t(*p);value*=16777619U;}value^=0;value*=16777619U;}
  value&=0x00ffffffU;return value?value:1;
}
}
