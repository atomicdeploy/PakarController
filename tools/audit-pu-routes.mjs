import {readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('..',import.meta.url));
const donor=process.argv[2];if(!donor)throw Error('Provide the reviewed pu850-esp checkout path');
const hash=text=>createHash('sha256').update(text).digest('hex');
const main='firmware/esp8266/esp8266.ino',routes='firmware/esp8266/src/MaintenanceRoutes.h';
// Manual dispositions. Generating hashes/line references is not a substitute
// for reading donor implementations; the declared reviewed ranges are bounded.
const rows=[];
function add(line,path,method,status,target,anchor,reason,source='pu850-esp.ino'){
  rows.push({source,line,path,method,status,target,anchor,reason});
}
const exclusions={
  '/weight/display':'PU weighing UI/value format requires a load cell; none is fitted. Generic TM1637 text/number is implemented separately.',
  '/action/tare':'PU load-cell tare/calibration command; this board has no weighing hardware.',
  '/power':'PU battery/power measurements require sensors not present; telemetry does not invent voltage/current.',
  '/weight/get':'PU weight measurement protocol and load cell are absent.',
  '/receipt':'PU weighing receipts/chart records and associated business protocol are deliberately removed.',
  '/report':'Commented donor business-report endpoint, not a working feature; no weighing records exist.',
  '/pu':'PU chart/master-state protocol does not describe this AVR; actual AVR settings/status/telemetry replace applicable diagnostics.',
  '/pu_reboot':'PU-specific reboot RPC replaced by the negotiated AVR RESET MCU contract; no unauthenticated GET reset.',
  '/pu_reset':'Old PU reset pin/chart state are not copied: ESP8266 uses guarded D1/GPIO5 ISP reset, S3 uses reviewed build pin ownership and refuses ISP until its electrical-verification flag is enabled.',
  '/is_exec':'PU chart execution state has no equivalent on this table; actual game/automation state is exposed through negotiated contracts.',
  '/page_id':'PU chart page identifier is not this board\'s front-panel menu. PCController GET MENU remains the actual supported contract.',
  '/page_goto':'PU page/business navigation cannot be copied as a table action. PCController SET MENU is available through the guarded contract API.'
};
for(const [line,path,method]of [[611,'/weight/display','GET|POST'],[647,'/action/tare','POST'],[652,'/power','GET'],[657,'/weight/get','GET'],[662,'/receipt','GET|POST'],[703,'/report','GET'],[1406,'/pu','GET'],[1441,'/pu_reboot','GET|POST'],[1460,'/pu_reset','GET|POST'],[1470,'/is_exec','GET'],[1475,'/page_id','GET'],[1480,'/page_goto','POST']])add(line,path,method,path==='/report'?'commented-business':'not-applicable',null,null,exclusions[path]);
for(const [line,path,method,reason]of [
 [591,'/beep','POST','Actual AVR buzzer ACK, 1..8 bounded count. Mutating GET removed.'],
 [597,'/message/send','POST','Actual TM1637 single-pass ASCII text; existing display engine owns timing, no duplicate scrolling engine.'],
 [757,'/file/cancel','POST','Exact job/boot ownership; no blind cancellation of another client or running erase.'],
 [1040,'/debug/info','GET','Actual remote/manual OTA phase, errors, UART counters and ACK observations; no fabricated PU enum.'],
 [1048,'/debug/data','GET','Actual bounded diagnostics, no raw UART dump containing secrets.'],
 [1057,'/debug/clear','POST','Clear observed reply only; persistent update journal and non-resettable counters explicitly retained.'],
 [1064,'/eProcessState','GET','Actual remote updater state replaces PU process enum.'],
 [1068,'/eResponseStatus','GET','Actual last ACK and reply observation replaces PU response enum.'],
 [1072,'/crash','GET|POST','410: deliberate null write/division/watchdog corruption is unsafe and is not a production capability.'],
 [1082,'/datetime/get','GET','ESP software clock, UTC/source/validity; no AVR RTC is fitted.'],
 [1097,'/datetime/set','POST','Strict Gregorian UTC parse then software-clock update; no fake AVR time ACK. NTP may subsequently adjust.'],
 [1118,'/datetime/parse','POST','Strict read-only UTC validation, including leap-day checks; donor Debug gate no longer required.'],
 [1139,'/datetime/active','GET','Actual software clock snapshot; no stale PU adjusted timestamp.'],
 [1153,'/ntp/get','GET','Previously commented; reports real SNTP callback observation rather than claiming every valid epoch is NTP.'],
 [1179,'/wifi/signal','GET','Real RSSI when connected; null when offline, no fake percentage.'],
 [1198,'/wifi/country','GET','Actual SDK regulatory country/channel values.'],
 [1210,'/wifi/status','GET','Shared ESP8266/S3 STA/AP mode, connectivity, MAC/IP/netmask/gateway/DNS/channel/DHCP and AP client count. S3 DHCP is the persisted requested policy, not a lease-acquisition assertion.'],
 [1217,'/config/ap','GET','Actual AP configuration, passwords redacted. Writes remain /api/network.'],
 [1244,'/config/st','GET','Actual station configuration, passwords redacted. Writes remain /api/network.'],
 [1277,'/hostname/check','GET|POST','Validated DNS hostname and SDK lookup; refuses during remote firmware maintenance. ESP8266 has an explicit 3-second timeout; S3 uses the SDK default, not an asserted identical latency bound.'],
 [1311,'/auth_dump','GET','Authenticated username/readiness only; raw password/token dumping deliberately removed.'],
 [1325,'/sessions_dump','GET','Authenticated bounded session count, never bearer/cookie values.'],
 [1343,'/random','GET','Fresh hardware-entropy random bytes, not a live authorization token.'],
 [1358,'/username','GET','Actual local device-owner identity instead of unsupported PU user-role RPC.'],
 [1380,'/lang','GET','Persisted locale preference; translated=false explicitly, no claim of translated application parity.'],
 [1384,'/lang','POST','Bounded validated locale persisted atomically; PU language RPC not sent.']
])add(line,path,method,'adapted',routes,path==='/lang'?'server.on("/lang"':path.startsWith('/datetime/')?'for(const char *path:{"/datetime/':path==='/ntp/get'?'"/ntp/get"':path.startsWith('/config/')||path==='/wifi/status'?'"/config/':path==='/auth_dump'||path==='/sessions_dump'||path==='/username'?'"/auth_dump"':path.startsWith('/debug/')&&path!='/debug/clear'||path==='/eProcessState'||path==='/eResponseStatus'?'"/debug/info"':path==='/file/cancel'?'"/file/cancel"':'server.on("'+path+'"',reason);
add(764,'/file/download','GET|HEAD','adapted','firmware/esp8266/src/ArtifactDownload.h','"/file/download"','Allowlisted actual ESP/staged/AVR flash and EEPROM, imported Range parser, HEAD and storage read leases. Arbitrary PU filenames/full filesystem dumps removed.');
add(1307,'/login','POST','commented-replaced','firmware/esp8266/src/WebAccess.h','server.on("/login"','Commented donor stub replaced by existing authenticated expiring cookie/session login with same-origin validation.');
add(1493,'/update/info','GET','adapted',main,'server.native().on("/update/info"','Version/profile and both-board remote maintenance URLs; no misleading build-host/time identity.');
add(1513,'/info','GET','adapted',main,'server.on("/info"','Authenticated full structured ESP/AVR/client/network/clock/cloud telemetry; optional format=text.');
add(1543,'/','GET','adapted','firmware/esp8266/src/WebUi.h','Verified remote update','Real control workspace rather than donor static placeholder; access gate and actual peer state retained. Optional DHCP/static editor preserves stored station credentials and RGB preview requires fresh actual output, not a synthetic animation.');
add(22,'/auth','GET','adapted',routes,'"/auth"','Local owner auth status, no credential disclosure.','LocalLib/Authentication.cpp');
add(57,'/pass','POST','adapted',routes,'server.on("/pass"','Requires current password, persistent change and revocation of Web/Telnet sessions.','LocalLib/Authentication.cpp');
add(12,'/update','GET','commented-replaced','firmware/esp8266/src/WebUi.h','Verified remote update','Commented donor HTML placeholder replaced by real firmware settings; no second OTA engine.','LocalLib/AsyncOTA.cpp');
add(19,'/update','POST','adapted',main,'server.native().on("/update"','Integrated guarded authenticated upload, owner/disconnect fencing and platform OTA capacity/abort retained beside pinned remote download. Quiet S3 passive capture rejects new upload ownership. U_FS replacement is disabled to preserve credentials/recovery; S3 compressed-image support is not certified.','LocalLib/AsyncOTA.cpp');
add(213,'/firmware/download','GET|HEAD','imported','firmware/esp8266/src/FirmwareTransfer.cpp','void RegisterFirmwareTransferRoutes','Actual donor range/HEAD/ETag/MD5/export code imported. S3 reads offsets relative to its running application partition with mbedTLS Base64; ESP8266 aligned flash reads remain. Full flash export is disabled to protect secrets.','LocalLib/FirmwareTransfer.cpp');
add(113,'/description.xml','GET','adapted',main,'server.on("/description.xml"','Actual product/hostname root-device XML; S3 delegates escaped description generation to the adapted donor SSDP device, ESP8266 keeps its existing description. Neither advertises nonexistent SOAP/GENA services; S3 NetBIOS remains unavailable, not true.','LocalLib/SSDP.cpp');
add(22,'/ws','WebSocket','adapted',main,'if(parsed["type"]=="telemetry")','Authenticated multiple-client WebSocket retains board state and adds read-only full telemetry and pinned remote-job commands. Incoming messages are explicitly rejected during quiet S3 passive capture, before binary streaming or command dispatch.','LocalLib/AsyncWebServer.h');
rows.sort((a,b)=>a.source.localeCompare(b.source)||a.line-b.line);
const sources={};
for(const r of rows){
 const raw=readFileSync(resolve(donor,r.source));const text=raw.toString('utf8');const lines=text.split(/\r?\n/);
 if(!sources[r.source])sources[r.source]={sha256:hash(raw),reviewed:r.source==='pu850-esp.ino'?'route-registration implementation lines 560-1554':r.source==='LocalLib/AsyncWebServer.h'?'complete':r.source==='LocalLib/SSDP.cpp'?'complete':r.source==='LocalLib/Authentication.cpp'?'complete':'complete'};
 if(r.path==='/firmware/download'){if(!lines.slice(r.line-1,r.line+4).join('\n').includes(r.path))throw Error('Stale donor anchor');}
 else if(r.path==='/description.xml'){if(!lines[r.line-1].includes('FW_UPNP_SCHEMA_PATH'))throw Error('Stale schema anchor');}
 else if(!lines[r.line-1].includes(r.path))throw Error('Stale donor anchor '+r.path);
 if(r.target){const text=readFileSync(resolve(root,r.target),'utf8').replaceAll('\r\n','\n');r.target_sha256_utf8_lf=hash(text);r.target_line=text.split('\n').findIndex(l=>l.includes(r.anchor))+1;if(!r.target_line)throw Error('Missing target '+r.target+' '+r.anchor);}
}
const migration={platforms:['ESP8266','ESP32-S3'],review:'Donor registration slice 560-1554 and complete listed LocalLib sources reread against shared route implementations and current S3 conditional changes; generated hashes are bindings, not execution proof.',diagnostic_build:'Normal builds retain AVR hardware UART. Optional PAKAR_AVR_LINK_ENABLED=0 quiet commissioning omits normal AVR TX and permits an explicit USB-only passive UART2 RX18 capture at 38400/115200 baud, at most 8 seconds/128 retained bytes. The helper checks TX17 input ownership and never drives reset GPIO10. Capture is diagnostic evidence, not a compatible AVR protocol or repaired reset-loop claim.',remaining:[
 'Real authenticated LAN GET/HEAD/Range/ETag/download-byte comparison and interrupted upload/power-loss/AVR recovery acceptance remain open for both platforms.',
 'S3 ISP erase/programming remains disabled until the actual AVR voltage, reset isolation and ISP wiring are electrically verified; UART communication alone does not certify ISP.',
 'S3 NetBIOS/LLMNR are not implemented or advertised as available. mDNS/DNS-SD/SSDP/MNDP source support is not live multicast reachability proof.',
 'TUI DHCP/static editing and mobile addressing/RGB settings controls remain pending; adding shared catalog fields does not certify all-interface parity.',
 'Legacy /lang continues to report translated=false; partial Web translations are not whole application/route translation parity.',
 'S3 hostname lookup currently uses the SDK default blocking deadline; explicit asynchronous/bounded request scheduling and compressed OTA acceptance need separate work.',
 'Quiet passive capture needs actual GPIO/received-byte/timing acceptance and reset-loop causal diagnosis; it does not replace normal UART/AVR protocol delivery. Atomic capture-state checks now cover WebSocket/cloud dispatch and new async ESP/AVR upload ownership alongside HTTP/USB guards and TLS service suspension. These checks do not certify a race-free cross-task maintenance lease; production multi-client/concurrency acceptance remains open.'
]};
const result={schema:1,donor:'atomicdeploy/pu850-esp (ASA0002E)',revision:'431104072952f694a35b3a83c19f13fbb098ca5b',scope:'Route-registration slice only; not whole-project parity, translation parity, production deployment or physical certification',migration,sources,routes:rows};
writeFileSync(resolve(root,'docs/PU850-ROUTE-PARITY.json'),JSON.stringify(result,null,2)+'\n');
const md=['# PU850 / ASA0002E route adaptation','','Donor revision: '+result.revision+'. '+result.scope+'.','','Every route registration in the reviewed slice is classified below, including commented endpoints. Source hashes and current target hashes/anchors are in PU850-ROUTE-PARITY.json. No pending generic route is silently labelled equivalent; unavailable sensor/business and unsafe behaviors are explicitly excluded.','','## ESP32-S3 migration review','',migration.review,'',migration.diagnostic_build,'',...migration.remaining.map(s=>'- '+s),'','| Donor source | Route | Disposition | Current implementation / reason |','| --- | --- | --- | --- |',...rows.map(r=>'| '+r.source+':'+r.line+' | '+r.path+' '+r.method+' | '+r.status+' | '+(r.target?r.target+':'+r.target_line+' — ':'')+r.reason.replaceAll('|','/')+' |'),'','Broader repository/file parity remains tracked separately in FILE-COMPARISON.md and GitHub issue #3. Live route/TLS/board acceptance remains open.'];
writeFileSync(resolve(root,'docs/PU850-ROUTE-PARITY.md'),md.join('\n')+'\n');
console.log(rows.length+' donor registrations mapped with exact source and reviewed current target hashes.');
