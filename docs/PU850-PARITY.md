# PU850 / ASA0002E reuse audit

## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19

All fourteen C# uploader files, CMakeLists and both toolchains, puff.h and the
complete puff.c were read through EOF at the same pinned/current donor. This is
nineteen additional original-byte/Git-object-bound comparisons. One truncated
multi-file UploadClient output was completed with explicit overlapping chunks;
no source certificate was inferred from function names or a partial output.

Actual C# FirmwareManifest LoadAsync/SelectFor/PortableFileName and bounded
ComputeFirmwareHashesAsync are adapted into `tools/firmware-manifest.mjs`.
Existing `collectArtifacts(...,{compress:true})` invokes them for ESP8266 only,
verifies actual gzip/raw bytes before writes and emits a separate `.gz.md5` with
exact raw/compressed records. Raw source `.bin` and S3 bytes are unchanged; this
does not enable S3 gzip OTA. Existing Node zlib supplies its bounded decoder,
without importing puff's static first-use tables or duplicating a decoder/runtime.
The actual C# manifest test is ported with invalid/empty/corrupt/oversized gzip,
portable names, extra/duplicate/mismatched records and both hash failures. An
executable fixture exercises the real artifact-report consumer, checks sidecar
hashes, unchanged raw bytes and absence of a S3 gzip output. No donor build or SDK
rebuild was needed for this source/tooling change.

| Complete source: every class/function/section | Current owner, actual import, or exact remaining behavior |
|---|---|
| `ConsoleUi.cs`: fields/two constructors; Heading/Info/Success/Warning/Error/Detail; UploadProgress/DownloadProgress/TransferProgress; PollProgress/ClearProgress/Verbose/WriteLine/WriteError/Sanitize/FormatBytes/EnvironmentAllowsColor | Existing script-console owns per-stream TTY/NO_COLOR/TERM, quiet/JSON/error routing and stronger OSC/CSI/control removal; DiagnosticText owns embedded formatting. C# independent stderr coloring, 10% redirected buckets, bounded transfer bar and polling spinner are useful unmapped refinements and remain pending. Never label host sent bytes device commit. |
| `EndpointResolver.cs`: constructor/UsesLocalDns/ResolveAsync/cache; ResolvedEndpoint Rebase/UriAt/UpdateInfoUri/InfoUri | Existing hostname-only URL checks, pinned IPv4 TCP endpoint, current headers and fixed same-origin routes are retained. No arbitrary cross-origin rebase or a silent second lookup/retry. IPv6 and non-.local resolver/platform behavior remain explicit generic gaps. |
| `FirmwareManifest.cs`: entries/constructor/LoadAsync/SelectFor/ComputeMd5Async/ComputeFirmwareHashesAsync/PortableFileName/regex methods; ManifestEntry/FirmwareHashes/CalculatedFirmwareHashes; TransferLimits; exception constructors | Actual two-record parsing/portable matching/both hash checks/bounded gzip calculation imported into the real explicit ESP8266 artifact path. 16 MiB compressed/raw upper bound plus 64 KiB manifest cap; no duplicate/empty/mismatched record. Existing raw S3 validator remains authoritative. Full pair distribution/consumer/actual ESP8266 gzip upload and interrupted packaging acceptance remain pending. |
| `InfoParser.cs`: Parse; TryGetFirmwareHash overloads/TryGetDeviceFirmwareHash/private selector/IsAsciiHex/FlattenJson | Exact current typed JSON identity/hash fields replace nested/fuzzy leaf selection and malformed-JSON-to-text fallback. Generic display-only scalar flattening and legacy text inspection remain useful pending consumers, never authority for an update. |
| `Program.cs`: Main and every parse/help/selftest/watch/download/upload/cancel/unexpected/finally branch; FirstEnvironment | Existing pakar dispatcher, stdout JSON/stderr diagnostics, explicit operation plan and one process signal owner remain. No encoding/system preference mutation, bearer aliases or auto watcher. Two-stage graceful stop belongs to the pending watcher capability; hard cancel behavior and cleanup still need actual process acceptance. |
| `DownloadApp.cs`: RunAsync/DownloadOnceAsync/Finish/ReportFailure and every network/retry/error branch | Already imported Go-backed download tool supplies exact metadata/HEAD/ranges/full integrity and safe new destination. C# replace-existing move, no-hash/--no-verify success and cached-address retry are replaced by preservation and mandatory same-peer integrity. Progress/error category and selectable verified backup-before-upload still need applicable consumer integration. |
| `DownloadClient.cs`: constructor/GetMetadataAsync/DownloadAsync/ExtractExpectedMd5/EnsureHashesAgree/ValidateDownloadResponse/AddAsciiMd5Header/ValidateExpectedHash/IsMd5/ComputeMd5Async/CreateClient/CreateTimeout; metadata/result/three exception classes | Current backup validates every advertised digest, current same-peer headers/release, exact 64 KiB ranges and final MD5/SHA; rejects unknown/no hash and auto decode. C# FileMode.Create can truncate existing non-resume partial; Pakar refuses it. Actual range-advertisement check and remaining storage/proxy/redirect/transport acceptance are pending; direct donor classes are not added as a second network owner. |
| `UploadClient.cs`: constructor/UploadAsync/GetFirmwareInfoAsync/IsAcceptedResponse/CreateClient/ApplyHeaders/Truncate/RequestInfoAsync/ReadLimitedStringAsync/AppendReplaySafety/Quote; DeviceFirmwareInfo; ProgressFileContent constructor/TryComputeLength/Serialize overloads/CopyAsync; CommitTrackingMultipart overloads/property; every exception/IsSafeToRetry | Existing raw-S3 updater validates exact new-boot/slot/full SHA/JSON ACK, fixed origin/TCP/current auth and bounded replies, with no ambiguous write replay. C# request-body-start tracking is a useful future streaming transport detail, but it does not authorize retry or bypass. Current exact protected headers replace unrestricted custom headers; bounded chunked/multipart streaming and progress remain useful pending work. |
| `UploaderApp.cs`: RunAsync/full manifest/hash/preflight/backup/skip/upload/retry/verify branches; VerifyAfterRebootAsync/WriteUsefulInfo/BackupInstalledFirmwareAsync; SynchronousProgress.Report; UploaderRuntime constructor/properties | Actual paired-manifest validation now imported in packaging. Existing stricter updater refuses unknown preflight and requires new boot/full running image. Backup-before-upload, skip-identical source identity/backup policy and bounded progress/log summary remain pending; no operation is launched by this audit. NoVerify/dial re-upload/ok!-only/same-boot hash acceptance are replaced. |
| `UploaderOptions.cs`: mode/exit/header records; every option/default/parse branch and validation; EffectiveDownloadApi/DeriveDownloadApi/WriteHelp/TryParseHttpUri/TryParseHeader/TryTakeValue/TryTakeSeconds; ParseResult factories | Existing shared dispatcher/current wrapper owners preserve help, numeric bounds, safe URLs, explicit run, exact headers and environment-only Basic credentials. --no-verify, bearer/legacy environment aliases and arbitrary headers cannot weaken integrity/ownership. Full applicable timeout/help/verbose/backup/watch option parity remains pending, not business. |
| `WatchUploader.cs`: RunAsync/TryReadIdentityAsync; WatchStopController StopToken/IsStopRequested/RequestStop/TryBeginOperation/EndOperation/Dispose; identity/stable/stamp/observation records; BuildWatchMonitor constructor/WaitForStableAsync/DisposeAsync/TryCreateFileSystemWatcher/PollAsync/Capture/CaptureFile/all event callbacks/RecordEvent/MonotonicNow/IsRelevant; WatchStateMachine Current/Observe/TryGetStable/GetStatus and BuildObservation readiness/reasons | Entire generic lifecycle is useful and pending: single operation ownership, coalesced generation, monotonic debounce, graceful-then-hard stop, event-plus-poll fallback, error/readiness/status and disposal. No background watcher/automatic upload was enabled. Existing Go-derived hash-plus-file-stability backup guard is already imported. C# metadata-only polling can miss equal-size/time content changes without events, so do not downgrade that guard. |
| `SelfTests.cs`: RunAsync; all twenty top-level tests from TestManifestAsync through TestDownloadSizeBoundAsync/TestSkipIdenticalUploadAsync; ReadyBuild/DownloadOptions/QuietConsole/Md5/Gzip/True/Equal/ThrowsAsync; both FakeFirmwareServer/FakeDownloadServer constructors, all routes, header/body readers/writers/disposal/properties; TestHttpRequest | Complete 53 KB test source read, including all nested fixture branches. Actual manifest fixture now ported; actual backup/script/update tests cover corresponding preservation/hash/secret/no-replay boundaries. Remaining watcher/graceful-stop/real TCP redirect/oversized streaming cases stay pending. No test-source presence or .NET build is substituted for target execution. |
| `Uploader.csproj`: all SDK/output/framework/using/nullability/identity/version/warning/debug/determinism fields | Current Node/PowerShell owner avoids a second .NET8 application/runtime/cache. Strict source fixtures and deterministic artifact generation are retained; no donor product version or embedded debug payload is copied. Full cross-platform binary-distribution equivalence is not claimed. |
| `Uploader.sln`: format/VS versions/project GUID/path; every Debug/Release/AnyCPU mapping, solution properties and GUID | Current guarded scripts/process tasks own scoped build entrypoints. A parallel donor solution/GUID layout adds no behavior to the ported consumer; current native/client packaging acceptance remains separate. |
| `Tool/C++/CMakeLists.txt`: platform guard; executable/puff; standards/defines/libs; all MSVC/MinGW compiler/linker settings; CTest/selftest/Python detection/integration timeout/install | Existing platform/fixture/locked build owners replace a Windows-only duplicate transfer executable. WinHTTP/BCrypt/Winsock/legacy Windows support is not silently equated to current Node: exact legacy-OS/native-binary distribution acceptance remains pending. No donor compiler/runtime/static cache is built. |
| `Tool/C++/cmake/mingw-w64-x64.cmake` and `mingw-w64-x86.cmake`: system/processor/C/C++/RC compilers; all four root-path search modes | Full target-only toolchain isolation read. Existing explicit host/runtime owners replace a second MinGW cross-build; architecture/native-binary packaging remains pending if that distribution is required. No current compiler/search path is changed. |
| `Tool/C++/puff.h`: complete Mark Adler license/version/NIL/prototype contract | Original license remains in read-only donor. No source redistribution here; existing Node zlib supplies gzip/deflate and the new bounded hash validator. Do not invent an MIT attribution for puff. |
| `Tool/C++/puff.c`: full history/format notes/includes/limits/state; bits/stored; both SLOW/fast decode; huffman/construct/codes/fixed/dynamic/puff; invalid-distance option and every documented error | Existing Node zlib is the decoder, tested here for corrupt CRC, invalid/empty input and output bounds; actual Huffman/deflate coverage beyond these fixtures remains pending, not claimed from this audit. No global virgin table initialization, longjmp decoder or unsafe-distance compatibility macro is transplanted. Raw-deflate scan mode/corpus parity stays useful pending where a real consumer requires it. |

The remaining entirely unread files are the C++ transfer implementation and two
UART files. Previously certified async/main/shell/discovery/build sources still
require their requested fresh complete section/function reread. Source/runtime/
physical acceptance remain distinct; this pass made no live service/device,
release-policy, relay, scene, firmware or SDK changes.

## Exhaustive continuation: editor and Go client slice, 2026-10-19

All eleven previously unread Go files plus the three editor JSON blobs, ignored
secret example and WebSocket sample were fully read through EOF at unchanged
donor `431104072952f694a35b3a83c19f13fbb098ca5b`. All sixteen were obtained by
immutable blobs where sparse files are absent. These augment the existing ledger,
not a competing completion checklist; the earlier foundation slice is historical.

Actual `downloadFirmware` / `startDownloadRequest` / `validateDownloadResponse` /
`parseContentMD5` hash/length/resume flow and `digestUnchangedFile` stability
checks are ported into `tools/download-firmware.mjs`, invoked by the guarded
PowerShell wrapper and `pakar download`. Default invocation only plans. Explicit
`--run` reads one current peer into a new local backup; it cannot update firmware,
overwrite an existing destination or replay a failed request. A resumed prefix
must pass the final whole-image MD5; a SHA256 receipt is also computed. Each
64 KiB response must match exact Content-Range/Length and same address/board/boot;
all advertised MD5/ETag values must agree. HEAD metadata and final same-peer
status are independently read. Current centralized release admission remains
default-on with no downgrade, bypass or donor version-selected adapter.

The donor's replace-existing rename is adapted to an atomic no-overwrite hard
link of this tool's verified `.part`, followed by unlinking only that partial
name. It never links a mutable executable/compiler cache or removes another
backup. Filesystems that cannot create this link fail with the partial retained;
there is no delete/copy fallback. Authentication uses optional environment-only
Basic credentials with current server admission, never a Bearer bypass. Errors
do not include headers/bodies/secret values. Cancel stops new reads; an in-flight
existing request remains bounded by the shared eight-second request deadline.

| Complete donor file / every function or section | Actual disposition / unfinished behavior |
|---|---|
| `.vscode/arduino.json`: full board/configuration/port/sketch/output/prebuild/postbuild/preferences | Existing shared firmware wrapper owns real profiles and SDK/cache gates. New process tasks invoke it in Plan mode. Do not copy COM13, exception/stack-check disabling, old sketch name, forced gzip replacement or deleting `~local.h`. Editor compile-database generation remains pending. |
| `.vscode/c_cpp_properties.json`: version/configurations, compiler/args/mode/includes/forced includes/standards and every exported define | Complete 19 KB blob read. Hundreds of compiler-predefined macros contradict flags (`__cplusplus`, exceptions/RTTI), freeze an ESP8266 ABI and hardcode host/library paths. Use an actual current build compilation database before claiming S3 IntelliSense parity; this generic editor capability remains pending, no fake configuration copied. |
| `.vscode/settings.json`: every editor/folding/language/spelling/association/exclude/search/error/formatting setting | Actual relevant `.ino/.h/.cpp/.c/embeddedhtml` mappings, protocol vocabulary and bounded generated-output filters imported in `.vscode/settings.json`. Preserve user's editor indentation/theme/formatting; donor's broad dotfile/tilde/assembly/hex exclusions would hide policy/private examples/boot source, so only current generated directories are filtered. |
| `~secrets.h.example`: pragma/comment/token placeholder | Existing AccessConfig/physical USB recovery and environment-only client credentials own real auth. Do not introduce another compile-time Bearer secret or copy a placeholder into production. Private-header ignore rules already imported. |
| `Tool/Websocket Client.html`: all HTML/styles; initWebSocket/onOpen/onClose/onMessage/onLoad/initButton/toggle | Current Web UI socket, cancellation/reconnect, safe text and event ownership replace hardcoded PU host, innerHTML injection, unbounded dots, fixed retry and unguarded send. Generic standalone endpoint/message inspector remains useful and pending; the sample is not certified equivalent to the full UI. |
| `Tool/Go/go.mod`: module/go version, no third-party dependencies | One existing Node runtime/tool dispatcher replaces a duplicate Go application/runtime. No module/Golang install or donor build. |
| `Tool/Go/console_other.go`: enableVirtualTerminal | Existing Node TTY/NO_COLOR/TERM ownership replaces isCharacterDevice-only decision; no system terminal state mutation. |
| `Tool/Go/console_windows.go`: constants/kernel32 procedures/enableVirtualTerminal | Full GetConsoleMode/SetConsoleMode flow read. Current native CLI terminal owns Windows setup and script logger uses actual TTY state. Prove equivalent legacy-Windows console VT behavior before closing that generic compatibility gap. |
| `Tool/Go/replace_other.go`: atomicReplace | Current backup commit guarantees no overwrite rather than os.Rename replacement. It fails without deleting a prior destination. |
| `Tool/Go/replace_windows.go`: flags/MoveFileExW/atomicReplace | Current atomic hard-link creation retains no-overwrite on Windows/POSIX; unsupported filesystems fail with verified partial retained. WRITE_THROUGH rename durability has no equivalent proof yet; power-loss/storage acceptance remains open. |
| `Tool/Go/download.go`: constants/regex/remoteFirmwareInfo; firstEnvironment/validateBearer/deriveAPIFromDownloadURL/absoluteOutputPath/setRequestHeaders | Current strict hostname/peer/release/auth owners plus explicit output path replace HTTP-only endpoint derivation and bearer aliases. Parent/output/partial are checked; arbitrary external download origins and replace-existing are not added. |
| `Tool/Go/download.go`: fetchRemoteFirmwareInfo/downloadFirmware/startDownloadRequest/validateDownloadResponse/parseContentMD5/normalizeMD5/md5FromETag/canRefreshDownloadEndpoint | Actual verification/resume/partial flow imported. Current exact JSON metadata replaces fuzzy `/info` fallback; strict strong ETag, current MD5 metadata and same-peer headers are required. No DNS retry or full-flash download; bounded range reads reuse the current request owner. Redirect/auth/identity errors fail closed. |
| `Tool/Go/watch.go`: constants/error/action/observation/tracker/deduper/path-state; observe/markHandled/shouldReport/setSilent/watchFirmware/processWatchScan | Read every state and branch. Generic stable-change/coalesce/once-only semantics remain useful and pending. Automatic repeated deployment is not enabled by this source task; no watcher background process or automation created. |
| `Tool/Go/watch.go`: inspectWatchFiles/inspectWatchPath/digestUnchangedFile/validateWatchCandidate/watchBuildMarkerPath | Actual hash-plus-size/time/mode stability adopted around download verification/commit. Paired raw/compressed build marker/manifest validation remains pending in applicable packaging consumers; legacy sibling `~local.h` marker is not assumed a Pakar build lock. |
| `Tool/Go/main.go`: imports/constants/regex/error/options/manifest/resolvedEndpoint/consoleUI/progressReader; main/run/parseOptions/usage | Existing pakar dispatcher/check-only plans/logger/error categories remain the one owner. Added real download command. Generic download resume is imported; watch/force/backup-before-upload and gzip manifest options remain explicitly pending where applicable, not business exclusions. |
| `Tool/Go/main.go`: resolveEndpoint/resolveConfiguredEndpoint/resolveHTTPURL/newHTTPClient/canRefreshEndpoint/isDialFailure | Existing hostname/URL/IPv4 pinning/no proxy/no redirect owner is retained. No write retry after dial failure, fuzzy API aliases or bearer credentials. Reviewed read-only hostname recovery remains separate; this backup command never changes peer mid-operation. |
| `Tool/Go/main.go`: uploadFirmware/postFirmware/readManifest/hashFile/validatedFirmware/validateFirmware/buildMultipart/verifyReboot | Existing raw-S3 image validator, one-path upload, JSON acceptance, new-boot/readback and full SHA verification are retained. Donor continues upload on unreadable preflight and accepts same-boot matching MD5: not retained. Generic paired gzip/raw manifests, bounded decompression, backup-before-upload and 16 MiB pack validation remain useful pending imports; no donor gzip claim on S3. |
| `Tool/Go/main.go`: readLimited/parseInfo/flattenInfoJSON/findInfoValue/normalizeInfoKey/sanitizeConsole/isCharacterDevice; progressReader.Read; consoleUI style/line/info/success/warn/fail/progress/clearProgress/valueOr | Current bounded requests, exact structured identity/metadata and stronger script-console terminal sanitation/logging replace fuzzy suffix key selection and raw response diagnostics. Progress means host bytes only. Generic grouped scalar metadata display/terminal progress refinements remain pending where unmapped. |
| `Tool/Go/download_test.go`: all seven tests plus testNetworkOptions/writeTransferBuild/gzipBytes/setFirmwareHeaders/contentMD5/md5Hex/strconvItoa | Actual successful byte verification/atomic destination, exact resume and mismatch-preservation cases ported to shipped Node consumer. Added multi-range/all headers/same-peer/release/changed partial/deadline/cancel/destination race/complete partial/no-contact plan fixtures. Backup-before-upload/raw skip/metadata-fallback tests remain pending or superseded by strict current admission as above. Donor tests were read, not executed as Pakar proof. |
| `Tool/Go/main_test.go`: all nine test functions including help/cold mDNS/manifest/multipart+reboot/gzip/bearer/plain+JSON metadata/console/filename; all HTTP fixture branches | Existing script/update fixtures cover current help/plan/multipart/new-boot/error/secret boundaries. Donor-specific gzip/manifest/filename and environment-priority scenarios need applicable porting; hardcoded 20-second DNS and bearer aliases do not become public compatibility promises. |
| `Tool/Go/watch_test.go`: all nine tests; writeWatchBuild/rewriteWatchBuild | Read stable-window/no replay/coalesced newest-during-upload/deletion+recreation/sentinels/content-vs-metadata/mismatch/consecutive dedupe/missing-only-watch scenarios. Changed-file verification is exercised in actual download fixture. Remaining generic watcher tests stay pending until that real optional capability exists. |

The first download fixture failed with EBADF because exclusive creation was
write-only while the port deliberately verifies through the held descriptor.
Reconsulted exact donor download and reread its write/close/reopen/hash sequence;
changed creation to `wx+`, retaining exclusive/no-truncate ownership. Focused
fixtures then passed, including same-length partial mutation after hashing.
No real device/server/relay request, updater invocation, policy change, SDK build
or deployment was performed. Remaining unread files are C# uploader, C++ uploader/
puff/CMake and UART; previously certified source still needs its requested fresh
full-function reread. These source passes do not establish physical acceptance.

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
