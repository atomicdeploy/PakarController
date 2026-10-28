#include <iostream>
#include <string>
#include <cstdio>
#include "UpnpDescription.h"
#include "BoardIconAsset.h"
#define F(text) text
struct Print {
  std::string value;
  void print(const char* text){value+=text;}
  void print(unsigned number){value+=std::to_string(number);}
  void write(uint8_t byte){value+=char(byte);}
};
namespace PakarS3 {
struct SsdpDevice {
  const char *m_deviceType="urn:schemas-upnp-org:device:Basic:1",*m_friendlyName="Table & Keys <One> \"name\" 'ok'",
    *m_manufacturer="AtomicDeploy",*m_manufacturerURL="https://github.com/atomicdeploy",*m_modelName="Pakar Test",
    *m_modelNumber="0.2.8",*m_modelURL="https://github.com/atomicdeploy/PakarController",*m_serialNumber="ABCDEF",
    *m_uuid="38323636-4558-4dda-9188-cda0e6abcdef",*m_presentationURL="/";
  uint32_t m_configId=0;
  void refreshConfigId();void schema(Print&)const;
};
/*__PRODUCTION_SCHEMA__*/
}
int main(){
  using namespace PakarDiscovery;
  PakarS3::SsdpDevice device;device.refreshConfigId();const auto identity=device.m_configId;
  device.refreshConfigId();if(device.m_configId!=identity||identity>0xffffff||!identity)return 1;
  device.m_friendlyName="Changed";device.refreshConfigId();if(device.m_configId==identity)return 2;
  device.m_friendlyName="Table & Keys <One> \"name\" 'ok'";device.refreshConfigId();
  const char* a[]={"ab","c"};const char* b[]={"a","bc"};if(configId(a,2)==configId(b,2))return 3;
  Print output;device.schema(output);const auto at=output.value.find("\r\n\r\n");
  if(at==std::string::npos)return 4;
  const auto xml=output.value.substr(at+4);
  if(xml.find("<URLBase>")!=std::string::npos||xml.find("<serviceList>")!=std::string::npos)return 5;
  if(xml.find("Table &amp; Keys &lt;One&gt; &quot;name&quot; &apos;ok&apos;")==std::string::npos)return 6;
  std::cout<<xml;
  std::cout<<"ICON ";for(auto byte:kBoardIconPng)printf("%02x",byte);std::cout<<"\n";
}
