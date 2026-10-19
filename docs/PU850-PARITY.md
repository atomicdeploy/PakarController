# PU850 / ASA0002E reuse audit

## Exhaustive continuation: foundation slice, 2026-10-19

Fresh GitHub `main` and local sparse HEAD both resolve to
`431104072952f694a35b3a83c19f13fbb098ca5b`. The recursive GitHub tree is
untruncated: 121 entries, 109 files, no submodules, symlinks, standalone license
or binary assets. The local checkout remains sparse (`LocalLib`) and read-only;
reading an immutable Git blob does not mean its absent working-tree file exists.
The 109-path ledger agrees with both GitHub and `git ls-files`. Dependency source
is external rather than an omitted vendored tree: the pinned sketch/build/tool
manifests and lock files remain separately reviewed or pending in the same ledger.
No remote ref was advanced, checkout expanded, extra worktree/cache created or
board/app/server changed. The installed release admission policy is unaffected.

This slice fully read the following twenty previously pending files, through EOF,
including every commented section. Exact original-byte hashes, source revisions,
Git object IDs for sparse blobs, function sections, target/proof hashes and
remaining actions are in `file-comparison-evidence.json`. It is not completion of
the entire project; existing certificates are not a substitute for the requested
fresh section/function reread of their sources.

| Complete donor file / sections | Actual disposition and remaining work |
|---|---|
| `ASA0002E.h`: include graph, DebugTools, flashSettings, callback/state declarations, mode helpers, count/range/hex macros | Keep separate typed owners in NetworkConfig/AccessConfig/AsyncHttpService and hardware transport; no monolithic mutable definitions or generic `os_printf` suppression. Persistent layout/checksums remain private, never public API selectors. |
| `ASA0002E.h`: convertToHexString | Actual byte loop adapted to `DiagnosticText::encodeHex`, used by WebAccess session generation and `/random`. Caller capacity/overflow checks, unsigned input and a terminating NUL replace unchecked String growth/signed-char formatting. |
| `ASA0002E.h`: parseHexString, reflectU32, calculateCRC32, calculateReflectedCRC32 | Destination-bounded hex decode and broader reusable checksum reconciliation remain pending. reflectU32 only visits four bits (`sizeof(r)`), despite documenting 32; do not copy that defect. No change to existing packet CRC or saved-record digest/layout is authorized by this utility import. |
| `ASA0002E.h`: calculateMD5/getRandomMD5/stringMD5 | SDK running-image MD5 and real 16-byte system entropy remain the actual owners. The donor's random MD5 starts with only a 32-bit random value; both String wrappers leak their allocated output. Existing fixed-buffer session generation avoids those defects. |
| `ASA0002E.h`: isValidHostName | Existing device-name validation remains the owner. Donor permits repeated/interior dots and uses a different length rule; exact label/hostname and cross-client parity review remains open, no blind validator replacement. |
| `ASA0002E.h`: urlEncode, getBasename, getFileExtension, extractFileName, startsWith, endsWith, startsWithIgnoreCase | Generic reuse remains pending. Correct signed UTF-8 handling, mixed path separators, bounded output and dot-in-directory behavior before wiring a real consumer. These are not PU business exclusions. |
| `ASA0002E.h`: isLeapYear/isValidDate/isValidTime, monthNames/dayNames, monthStringToInt/dayOfWeek, TimeStamp constructors/set/toEpoch/fromEpoch/adjusted/isValid/toString, parseDateTime overloads/PU_DateTime | Generic calendar/clock support remains pending. Donor accepts impossible dates, lacks null/conversion checks, invalidates millis rollover, and its seconds parser does not bound width. Preserve authoritative UTC clock and Unknown state until complete validation/calendar tests and real consumers exist. |
| `ASA0002E.h`: getFormattedUptime/humanReadableBytes | Actual decomposition/unit scaling imported into DiagnosticText, consumed as additive telemetry `uptime_text`/`free_heap_text`; numeric fields remain authoritative. Fixed buffers reject truncation, suffix bound is `unit+1<count`. Uptime text describes the same sampled uint32 millis observation, not reconstructed lifetime across rollover. |
| `ASA0002E.h`: compiler/debug/serial tail | Existing explicit platform/build gates and COBS UART ownership replace CodeVision aliases and Serial-disabling debug switches. Preserve panic evidence; no ESP8266 crash hook is transplanted into S3. |
| `.gitattributes` | Pakar's existing LF text policy serves cross-platform source/scripts; explicit binary extensions remain. Donor CRLF cmd/bat/ps1 policy is not required by those interpreters. |
| `.gitignore` | Actually import donor ignored `~local.h` and `~secrets.h` patterns. Existing target/cache/output/node_modules rules cover current artifacts; product-specific uploader output paths are not created. |
| `ASA0002E.code-workspace` | Association/spell-check sections fully read; Pakar `.ino/.h/.cpp` editor associations and reviewed shared build-task ergonomics remain pending. No donor machine-specific includes are copied. |
| `ASA0002E.ino.globals.h` | Documentation-only early-options include, no declarations or executed code. Existing generated BuildInfo/current build flags own identity; no duplicate empty header is needed. |
| `defines.h` | Read Arduino/local/ignored-secret includes, product owner and serial-shell alias, plus all four Temp dependencies below. Keep explicit Pakar flags and one framed UART/shell owner; no compile-time token or secret transfer. |
| `ProductConfig.h` | Read both identity profiles/all override points; UPnP schema/service/cache/TTL/server; firmware/full-flash/auth/credential-exposure switches. Existing centralized ApplicationIdentity/PeerIdentity and platform discovery owners remain. Reconcile all generic override/default knobs in later source slices; do not advertise nonexistent SOAP/GENA, transplant product identity or add a bearer bypass. |
| `PRODUCT-PROFILES.md` | Entire identity/transfer/hash-pair/downstream policy read. Living endpoints and strict identity/auth/integrity retained by current transfer owner; exact remaining cross-language upload/resume/gzip/manifest behaviors are pending. Full-flash dump remains disabled; no version-selected adapter or automatic downgrade. |
| `LocalLib/CrashHandler.cpp/.h` | Only active code suppresses ESP8266 crash UART; custom_crash_callback/offset/counter/stack storage is wholly commented with TODO writes. Existing reset telemetry/no raw protocol UART logging is retained. Persistent bounded crash capture/rotation still needs an actual safe platform design; donor sketch is not an implementation certificate. |
| `LocalLib/FS.cpp` | Entire initFS implementation is commented. Actual SystemPlatform mounts without automatic format and FilesystemTelemetry retains truthful unavailable/error observations. Storage-full/power-loss/backup acceptance remains open. |
| `LocalLib/Public.cpp/.h` | UnPack32's four shifts are covered by existing typed protocol/telemetry byte owners; compressed Gregorian/Shamsi date/time and offset macros require future validated generic date consumers. Global year/month/ePart mutation and C++ bool/true/false replacement macros are replaced by typed state/native C++ bool. Calendar support remains useful and pending. |
| `LocalLib/Sessions.cpp/.h` | initSessions/addSession/destroySession/validateSession, empty SessionCommand and 8-slot/16-byte layout read. Existing AccessPolicy preserves bounded add/validate/revoke/clear/count; expiry and unsigned elapsed time avoid donor no-expiry/rollover mistakes, authenticated access replaces product role indexes. Empty SessionCommand has no reusable execution; protocol-origin session revoke semantics remain a separate pending contract, never an unauthenticated new route. |
| `LocalLib/RemoteCommand.cpp` | initUdpDebug/UdpService/udpSend/handleUdpReceive/executeCommand/ServiceEvent read. File is not included by donor sketch; UdpService tests a function pointer rather than calling it, receive truncates at254 bytes and accepts unauthenticated text. Existing protected HTTP/WS/COBS owners remain. Reusable bounded UDP diagnostic/network-event lifecycle is pending explicit capability/auth/ownership design and fixtures; no UDP9876 listener is added. |
| `Temp/Always.h` | Every scalar/pointer/flash alias, bit operation, byte/word/packing/reversal macro and sentinel read. Existing stdint types and protocol functions replace width-dependent aliases and repeated-evaluation/text-fragment macros. Exact UART reconciliation remains pending; no global macro compatibility layer. |
| `Temp/CommonDefine.h` | Generic readiness/results/calendar/toggle/format constants and all ADC/load-cell/DLC calibration commands/limits/brands/statuses read. Generic typed readiness/clock semantics remain pending where unmapped; load-cell commands/calibration are specifically PU weighing business. |
| `Temp/ChartIndex.h` | Access roles, chart classes, every menu/form/list/page/reserved index read. Weighing/report/printer/calibration screens are PU business; generic Wi-Fi/settings/diagnostics/terminal/password/clock UI behaviors remain pending via actual runtime owners, not numeric chart aliases or embedded donor credential hints. |
| `Temp/ESP_Master_Common.h` | Protocol revision/message/network/internet/control tables, prefixes, every numeric/string/request/bulk/file/LCD/diagnostic/programming suffix, groups/results/baud table/ResultPack read. Existing COBS/capability/ACK/ownership replaces a second legacy parser. Weight/tare/receipt/report payloads are business; generic file/clock/message/LCD/network/session/diagnostic/programmer semantics require the later full UART function review and remain pending. |

Next unread files are the three `.vscode` JSON blobs and secret *example*;
then all Go/C#/C++ uploader implementations/tests/build metadata, WebSocket
client and the two large UART files. Reread each already-certified async, OTA,
auth, shell, discovery, main sketch and tooling source section before claiming
the requested fresh exhaustive review. Known gaps remain unchecked in issues
#2/#3. Focused source fixtures are not firmware SDK compilation, deployment,
network mutation, human UI or physical acceptance.

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
| Telnet.h/.cpp, Shell.h/.cpp | Entire sources read; actual IAC/subnegotiation parser and insert/shift/CSI/SS3 editor adapted in `TelnetShell.h`. Greeting negotiation, split CR LF/NUL, backspace/delete/home/end, Ctrl-C/Ctrl-L, hidden password entry and overlong-command rejection are tested. 96-byte service budget; 60-second login and 30-minute idle expiry. | Actual Telnet clients/LAN; shared serial shell, parameter registry and asynchronous event redraw remain open |
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
