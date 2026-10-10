# PU850 / ASA0002E reuse audit

Donor: `atomicdeploy/pu850-esp`, inspected/fetched main
`431104072952f694a35b3a83c19f13fbb098ca5b`. Its sparse checkout includes
`LocalLib`; tracked CI is read with `git show`, not treated as missing.
Content-hash screening is not a complete manual source review.

| Donor capability | Pakar checkpoint | Remaining work |
|---|---|---|
| AsyncWebServer.h/.cpp | Real ESPAsyncWebServer 3.12.1 + ESPAsyncTCP 2.0.0; HTTP and `/ws` share port 80. Bounded weak request queue defers hardware work to loop. Queue/disconnect/reentrancy tests and both macro builds. | Actual concurrency, disconnects and RAM stress |
| Authentication.h/.cpp, Sessions.h/.cpp | Adapted into WebAccess, AccessConfig, AccessPolicy: real authorization, bounded expiring sessions, persistent editor, rotation, physical USB recovery. Donor no-op authorization and credential/token dumps are not copied. | Real login/rotation/restart/USB recovery |
| FirmwareTransfer.h/.cpp | **Actual donor source imported**: Range/suffix/open downloads, GET/HEAD, 416, ETag/304, MD5/base64 headers, aligned flash reads. Production range parser tested. Authorization/identity adapted; full-flash secret dumps disabled. | Verify actual bytes/hash and response headers |
| AsyncOTA.h/.cpp | Integrated async `/update`, `/update/info`; raw/gzip, optional MD5, actual byte count/phase, deferred cues, exclusive owner and commit after entire multipart completion. No watchdog disabling. Firmware-only OTA preserves storage. | Live OTA, bad MD5, abort, multiple files and audible cues |
| ProductConfig.h, PRODUCT-PROFILES.md | Product/model/version identity; legacy hash/size/free metadata; stable hostname. AP/station, NTP, shell, cloud and ISP remain in one firmware. | Validate metadata consumers |
| NetDiscovery.h/.cpp | Read donor's inactive/blocking examples. Pakar uses serviced mDNS + SSDP + NetBIOS; description UDN matches SSDP UUID. | Actual multi-host discovery |
| Telnet.h/.cpp, Shell.h | Telnet/Pakar commands exist; password follows configurable credentials. **Not complete parity**: IAC negotiation, transport-aware editor, arrow/home/end and richer help remain to port. | Read/adapt actual editor/parser and test clients |
| MNDP.h/.cpp, SSDPDevice/custom SSDP | **Open**: MNDP and richer donor metadata are not integrated; standard ESP SSDP is not asserted equivalent. | Complete source review, adaptation and LAN tests |
| UART.h/.cpp, PU bulk/chart paths | PCController COBS/CRC, AVR hardware UART, polling/busy-wire flags and integrated ISP. Old PU-specific business/protocol endpoints excluded. | Complete non-business UART reconciliation |
| FS.cpp | Donor helper is inactive/commented. Pakar vault retains AVR+Urboot, EEPROM/flash backups, mappings and network/cloud/access records. | Physical persistence/backup/restore and storage-full faults |
| CrashHandler/diagnostics | `/info` exposes reset reason, firmware/version, heap, uptime and protocol. **Open**: crash capture/log rotation and detailed counters not fully reconciled. | Adapt without secret disclosures |
| Build/tooling/upload/dependency CI and Tool/* | Windows/Linux builds, both macro variants, native/desktop/Web tests and artifact CI; async versions locked in profile/CI. **Not all ported**: gzip packaging, uploader language variants, dependency workflow and opt-in deployment. | Full review, actual reuse, CI/deploy acceptance |

Downloader authorization uses configurable Basic/session access, not the
donor optional build-time Bearer bypass. Bearer-only donor upload tools need
an auth adapter; they are not assumed compatible as-is. Optional MD5 retains
older Pakar clients; when provided it precedes the firmware file in multipart
and is checked by the ESP updater.

This is an access/async migration checkpoint, **not a complete production
parity release**. PCController/PuzzleBoard/Pealayer reuse remains recorded in
`reuse-decisions.csv`. The open rows and physical acceptance gates stay open.
