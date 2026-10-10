# PU850 / ASA0002E route adaptation

Donor revision: 431104072952f694a35b3a83c19f13fbb098ca5b. Route-registration slice only; not whole-project parity, translation parity, production deployment or physical certification.

Every route registration in the reviewed slice is classified below, including commented endpoints. Source hashes and current target hashes/anchors are in PU850-ROUTE-PARITY.json. No pending generic route is silently labelled equivalent; unavailable sensor/business and unsafe behaviors are explicitly excluded.

| Donor source | Route | Disposition | Current implementation / reason |
| --- | --- | --- | --- |
| LocalLib/AsyncOTA.cpp:12 | /update GET | commented-replaced | firmware/esp8266/src/WebUi.h:66 — Commented donor HTML placeholder replaced by real firmware settings; no second OTA engine. |
| LocalLib/AsyncOTA.cpp:19 | /update POST | adapted | firmware/esp8266/esp8266.ino:1569 — Integrated guarded existing authenticated upload kept alongside pinned remote download. U_FS replacement disabled to preserve credentials and recovery backups. |
| LocalLib/AsyncWebServer.h:22 | /ws WebSocket | adapted | firmware/esp8266/esp8266.ino:1046 — Authenticated multiple-client WebSocket retains board state and adds read-only full telemetry and pinned remote-job commands. |
| LocalLib/Authentication.cpp:22 | /auth GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:321 — Local owner auth status, no credential disclosure. |
| LocalLib/Authentication.cpp:57 | /pass POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:327 — Requires current password, persistent change and revocation of Web/Telnet sessions. |
| LocalLib/FirmwareTransfer.cpp:213 | /firmware/download GET|HEAD | imported | firmware/esp8266/src/FirmwareTransfer.cpp:141 — Actual donor implementation imported earlier, with authorization, ranges, ETag, MD5 and bounded flash reads. Full flash export disabled to protect secrets. |
| LocalLib/SSDP.cpp:113 | /description.xml GET | adapted | firmware/esp8266/esp8266.ino:1763 — SSDP root-device XML, actual product and hostname without nonexistent weighing/UPnP services. |
| pu850-esp.ino:591 | /beep POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:354 — Actual AVR buzzer ACK, 1..8 bounded count. Mutating GET removed. |
| pu850-esp.ino:597 | /message/send POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:362 — Actual TM1637 single-pass ASCII text; existing display engine owns timing, no duplicate scrolling engine. |
| pu850-esp.ino:611 | /weight/display GET|POST | not-applicable | PU weighing UI/value format requires a load cell; none is fitted. Generic TM1637 text/number is implemented separately. |
| pu850-esp.ino:647 | /action/tare POST | not-applicable | PU load-cell tare/calibration command; this board has no weighing hardware. |
| pu850-esp.ino:652 | /power GET | not-applicable | PU battery/power measurements require sensors not present; telemetry does not invent voltage/current. |
| pu850-esp.ino:657 | /weight/get GET | not-applicable | PU weight measurement protocol and load cell are absent. |
| pu850-esp.ino:662 | /receipt GET|POST | not-applicable | PU weighing receipts/chart records and associated business protocol are deliberately removed. |
| pu850-esp.ino:703 | /report GET | commented-business | Commented donor business-report endpoint, not a working feature; no weighing records exist. |
| pu850-esp.ino:757 | /file/cancel POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:262 — Exact job/boot ownership; no blind cancellation of another client or running erase. |
| pu850-esp.ino:764 | /file/download GET|HEAD | adapted | firmware/esp8266/src/ArtifactDownload.h:11 — Allowlisted actual ESP/staged/AVR flash and EEPROM, imported Range parser, HEAD and storage read leases. Arbitrary PU filenames/full filesystem dumps removed. |
| pu850-esp.ino:1040 | /debug/info GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:372 — Actual remote/manual OTA phase, errors, UART counters and ACK observations; no fabricated PU enum. |
| pu850-esp.ino:1048 | /debug/data GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:372 — Actual bounded diagnostics, no raw UART dump containing secrets. |
| pu850-esp.ino:1057 | /debug/clear POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:382 — Clear observed reply only; persistent update journal and non-resettable counters explicitly retained. |
| pu850-esp.ino:1064 | /eProcessState GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:372 — Actual remote updater state replaces PU process enum. |
| pu850-esp.ino:1068 | /eResponseStatus GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:372 — Actual last ACK and reply observation replaces PU response enum. |
| pu850-esp.ino:1072 | /crash GET|POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:371 — 410: deliberate null write/division/watchdog corruption is unsafe and is not a production capability. |
| pu850-esp.ino:1082 | /datetime/get GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:286 — ESP software clock, UTC/source/validity; no AVR RTC is fitted. |
| pu850-esp.ino:1097 | /datetime/set POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:286 — Strict Gregorian UTC parse then software-clock update; no fake AVR time ACK. NTP may subsequently adjust. |
| pu850-esp.ino:1118 | /datetime/parse POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:286 — Strict read-only UTC validation, including leap-day checks; donor Debug gate no longer required. |
| pu850-esp.ino:1139 | /datetime/active GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:286 — Actual software clock snapshot; no stale PU adjusted timestamp. |
| pu850-esp.ino:1153 | /ntp/get GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:286 — Previously commented; reports real SNTP callback observation rather than claiming every valid epoch is NTP. |
| pu850-esp.ino:1179 | /wifi/signal GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:302 — Real RSSI when connected; null when offline, no fake percentage. |
| pu850-esp.ino:1198 | /wifi/country GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:307 — Actual SDK regulatory country/channel values. |
| pu850-esp.ino:1210 | /wifi/status GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:300 — Actual STA/AP mode, connectivity, MAC/IP/netmask/gateway/DNS/channel/DHCP and AP client count. |
| pu850-esp.ino:1217 | /config/ap GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:300 — Actual AP configuration, passwords redacted. Writes remain /api/v1/network. |
| pu850-esp.ino:1244 | /config/st GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:300 — Actual station configuration, passwords redacted. Writes remain /api/v1/network. |
| pu850-esp.ino:1277 | /hostname/check GET|POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:313 — Validated DNS hostname and bounded lookup; refuses while firmware owns maintenance. |
| pu850-esp.ino:1307 | /login POST | commented-replaced | firmware/esp8266/src/WebAccess.h:76 — Commented donor stub replaced by existing authenticated expiring cookie/session login with same-origin validation. |
| pu850-esp.ino:1311 | /auth_dump GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:321 — Authenticated username/readiness only; raw password/token dumping deliberately removed. |
| pu850-esp.ino:1325 | /sessions_dump GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:321 — Authenticated bounded session count, never bearer/cookie values. |
| pu850-esp.ino:1343 | /random GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:337 — Fresh hardware-entropy random bytes, not a live authorization token. |
| pu850-esp.ino:1358 | /username GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:321 — Actual local device-owner identity instead of unsupported PU user-role RPC. |
| pu850-esp.ino:1380 | /lang GET | adapted | firmware/esp8266/src/MaintenanceRoutes.h:343 — Persisted locale preference; translated=false explicitly, no claim of translated application parity. |
| pu850-esp.ino:1384 | /lang POST | adapted | firmware/esp8266/src/MaintenanceRoutes.h:343 — Bounded validated locale persisted atomically; PU language RPC not sent. |
| pu850-esp.ino:1406 | /pu GET | not-applicable | PU chart/master-state protocol does not describe this AVR; actual AVR settings/status/telemetry replace applicable diagnostics. |
| pu850-esp.ino:1441 | /pu_reboot GET|POST | not-applicable | PU-specific reboot RPC replaced by the negotiated AVR RESET MCU contract; no unauthenticated GET reset. |
| pu850-esp.ino:1460 | /pu_reset GET|POST | not-applicable | Old PU reset pin and chart state replaced by actual D1 ISP reset and guarded maintenance; copying the old pin is electrically wrong. |
| pu850-esp.ino:1470 | /is_exec GET | not-applicable | PU chart execution state has no equivalent on this table; actual game/automation state is exposed through negotiated contracts. |
| pu850-esp.ino:1475 | /page_id GET | not-applicable | PU chart page identifier is not this board's front-panel menu. PCController GET MENU remains the actual supported contract. |
| pu850-esp.ino:1480 | /page_goto POST | not-applicable | PU page/business navigation cannot be copied as a table action. PCController SET MENU is available through the guarded contract API. |
| pu850-esp.ino:1493 | /update/info GET | adapted | firmware/esp8266/esp8266.ino:1557 — Version/profile and both-board remote maintenance URLs; no misleading build-host/time identity. |
| pu850-esp.ino:1513 | /info GET | adapted | firmware/esp8266/esp8266.ino:1558 — Authenticated full structured ESP/AVR/client/network/clock/cloud telemetry; optional format=text. |
| pu850-esp.ino:1543 | / GET | adapted | firmware/esp8266/src/WebUi.h:66 — Real control workspace rather than donor static placeholder; access gate and actual peer state retained. |

Broader repository/file parity remains tracked separately in FILE-COMPARISON.md and GitHub issue #3. Live route/TLS/board acceptance remains open.
