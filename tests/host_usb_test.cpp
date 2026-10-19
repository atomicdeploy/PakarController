#include "../firmware/esp8266/src/HostSerial.h"
#include <cassert>
#include <cstring>
int main() {
  PakarHost::begin();
#if PAKAR_S3_HOST_TRANSPORT==2
  assert(!strcmp(PakarHost::transport(),"otg")&&PakarHost::native());
  assert((hostInitSteps==std::vector<int>{0,4,1,2,3,5}));
  assert(!PakarHost::port().reboot); // Before enumeration, not after a host DTR.
  assert(!PakarHost::connected());hostNativeConnected=true;assert(PakarHost::connected());
#elif PAKAR_S3_HOST_TRANSPORT==1
  assert(!strcmp(PakarHost::transport(),"hwcdc")&&PakarHost::native());
  assert((hostInitSteps==std::vector<int>{1,2,3}));
  assert(!PakarHost::connected());hostNativeConnected=true;assert(PakarHost::connected());
#else
  assert(!strcmp(PakarHost::transport(),"uart0")&&!PakarHost::native());
  assert(&PakarHost::port()==&Serial0&&PakarHost::connected());
  assert((hostInitSteps==std::vector<int>{3}));
#endif
}
