// Source-bound integration checks complement the compiled policy/SDK-double
// fixtures; they do NOT emulate mDNS, IGMP, socket source routing or LAN peers.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL(`../${p}`,import.meta.url),'utf8');
const interfaces=read('firmware/esp32s3/src/DiscoveryInterfaces.h');
const discovery=read('firmware/esp32s3/src/Discovery.h');
const ssdp=read('firmware/esp32s3/src/SsdpDevice.cpp');
const mndp=read('firmware/esp/src/MndpService.h');
const build=read('tools/build-firmware.ps1');
function checkInterfaces(source){
  assert.match(source,/#if defined\(PAKAR_S3_ETHERNET_ENABLED\) && PAKAR_S3_ETHERNET_ENABLED\s+#include <ETH.h>/);
  assert.match(source,/ETH.started\(\) && ETH.connected\(\) && ETH.linkUp\(\) && ETH.hasIP\(\)/);
  assert.match(source,/discoveryAddress\(ETH.subnetMask\(\)\)/);
  assert.match(source,/ETH.macAddress\(entry.mac\)/);
  assert.match(source,/discoveryAddress\(WiFi.softAPSubnetMask\(\)\)/);
  assert(source.indexOf('entry.kind="ethernet"')<source.indexOf('entry.kind="station"'));
}
checkInterfaces(interfaces);
assert.throws(()=>checkInterfaces(interfaces.replace(' && ETH.hasIP()','')));
assert.throws(()=>checkInterfaces(interfaces.replace('WiFi.softAPSubnetMask()','WiFi.subnetMask()')));
assert.match(discovery,/if \(!activeDiscoveryInterfaces\(\).count\) return false/);
assert.match(discovery,/bool mndpReady\(\) const \{ return mndp_.running\(\); \}/);
assert.match(ssdp,/m_ethernetSeen != ethernet/);
assert.match(ssdp,/if \(preferred == ethernet\) m_ethernetJoined = ethernet/);
assert.match(ssdp,/joinMulticast\(ip,SSDP_MULTICAST_ADDR\)/);
assert.match(ssdp,/m_joinRetryAt=retry \? now\+1000UL : 0/);
assert.equal((ssdp.match(/sendAllByebye\(m_ethernetJoined\)/g)||[]).length,2);
assert.match(ssdp,/postNotifyUpdate\(m_ethernetJoined\)/);
assert.match(ssdp,/joined.forRemote\(discoveryAddress\(remote\)\)/);
assert.match(mndp,/seenCount_!=interfaces.count/);
assert.match(mndp,/if \(seen_\[i\]!=interfaces.entries\[i\].address\) changed=true/);
assert.match(mndp,/sendInterface\(hostname,boardId,target,port,true,interfaces.entries\[i\]\)/);
assert.match(mndp,/packet.tlv\(1,interface.mac,6\)/);
assert.match(mndp,/packet.text\(16,interface.kind\)/);
assert.match(mndp,/discoveryAddress\(interface.address\)/);
assert.match(mndp,/retryAt_=retry\?now\+1000UL:0/);
assert.match(build,/\[switch\] \$S3Ethernet/);
assert.match(build,/if \(\$S3Ethernet -and \$Target -ne 'esp32s3'\) \{ throw/);
assert.match(build,/if \(\$S3Ethernet\) \{ \$flags \+= ' -DPAKAR_S3_ETHERNET_ENABLED=1' \}/);
console.log('Ethernet discovery/build source integration checks passed; physical multicast and ingress routing remain unverified.');
