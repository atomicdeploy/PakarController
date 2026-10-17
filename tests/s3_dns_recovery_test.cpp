#include "../firmware/esp32s3/src/DnsRecovery.h"
#include <cassert>
#include <iostream>
int main(){
 using PakarS3::DnsRecovery;
 const IPAddress working(192,168,1,1),backup(192,168,1,2);
 WiFi=WifiFixture{};WiFi.STA.dns[0]=IPAddress(IPv6);DnsRecovery missing;
 assert(!missing.service(4999,true) && !WiFi.STA.writes);
 assert(missing.service(5000,true) && WiFi.STA.dns[0]==working && WiFi.STA.writes==1);
 assert(!missing.service(10000,true) && WiFi.STA.writes==1);
 WiFi=WifiFixture{};WiFi.STA.dns[0]=IPAddress(IPv6);DnsRecovery boundary;
 assert(boundary.service(1,true,true) && WiFi.STA.writes==1); // before first NTP/DNS connection
 WiFi.STA.dns[0]=IPAddress(IPv6); // router RDNSS replaced DNS before periodic deadline
 assert(!boundary.service(100,true) && WiFi.STA.writes==1);
 assert(boundary.service(101,true,true) && WiFi.STA.dns[0]==working && WiFi.STA.writes==2);
 assert(!boundary.service(102,true,true) && WiFi.STA.writes==2); // valid DNS is not rewritten
 WiFi=WifiFixture{};WiFi.STA.dns[0]=IPAddress(IPv6);WiFi.STA.globalV6=true;DnsRecovery routed;
 assert(!routed.service(5000,true) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.STA.dns[1]=IPAddress(IPv6);WiFi.STA.globalV6=true;DnsRecovery routedBackup;
 assert(!routedBackup.service(5000,true) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.STA.dns[0]=working;DnsRecovery valid;
 assert(!valid.service(5000,true) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.STA.dns[0]=IPAddress(IPv6);WiFi.STA.dns[1]=backup;DnsRecovery promote;
 assert(promote.service(5000,true) && WiFi.STA.dns[0]==backup);
 WiFi=WifiFixture{};DnsRecovery fixed;
 assert(!fixed.service(5000,false) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.state=0;DnsRecovery offline;
 assert(!offline.service(5000,true) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.gateway=IPAddress();DnsRecovery noGateway;
 assert(!noGateway.service(5000,true) && !WiFi.STA.writes);
 WiFi=WifiFixture{};WiFi.STA.fail=true;DnsRecovery failed;
 assert(!failed.service(5000,true));WiFi.STA.fail=false;assert(failed.service(10000,true));
 WiFi=WifiFixture{};WiFi.state=0;DnsRecovery wrap;
 assert(!wrap.service(UINT32_MAX-1000,true));WiFi.state=WL_CONNECTED;assert(wrap.service(5000,true));
 std::cout<<"S3 DHCP DNS recovery: IPv4/IPv6/backup/static/offline/bounds/retry/rollover passed\n";
}
