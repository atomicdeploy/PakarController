# PakarController comparison receipts

## RayanLamp source checkpoint

Located task Build RayanLamp Go app, id 019f8cfc-271a-72e1-8747-e3eb883de5ec,
and preserved its nongit original source. RAYANLAMP-REUSE.md records exact mappings,
inventory boundaries and concrete next-file actions. The existing authorities now
contain 4,237 paths / 67 complete comparisons / 4,170 pending / 42 known gap
files. RayanLamp contributes 2,349 inventoried paths, six full-read comparisons
and 2,343 pending comparisons. Vendor source/licenses are included.

Passed all children of node tools/ci-checks.mjs quality, including actual
RayanLamp-adapted haptics/preference/digit cases, existing Web cell selection,
settings ACK/readback/conflict/secret guards, board context/identity, localized
copy/fonts, all donor route hashes/anchors, inventory/gate negative tests,
repository/build/storage contracts. New native Rust parses/formats in edition
2024, but native tests have NOT executed: tools/build-desktop.ps1 -Mode Test
refused the unchanged >8 GiB canonical cache guard before Cargo. No bypass,
cache duplication, obsolete-cache deletion or production replacement occurred.

tools/build-firmware.ps1 -Target esp8266 passed in busy-wire mode using its warm
cache: RAM 56620/80192, IRAM 63055/65536, IROM 850240/1048576.
Candidate image SHA256:
e52958e1a682de6b510541abc29f749d4208f49f5c744cc4fc3f2b1ff049e294.
This is build proof, NOT flashed-device or Android vibration acceptance.
AVR/Android/donor builds were not run. Six exact donor hashes and the complete
normalized MIT notice were checked against the original read-only source.
No CI wait is required for the source checkpoint; CI is not claimed green.

## Repository infrastructure checkpoint

Issue #11 compares the actual full Pealayer README, CI/CD, CodeQL, Dependabot
and next-release notes. Exact retained behavior, targets/hashes, omissions and
remaining package/platform gaps are in the existing canonical ledger, not a
separate replacement inventory. New repository-governance routing makes them
discoverable before future repository tasks. Fully reread the changed donor
icons file; shared-dropdown dependencies remain an explicit UI parity gap.

Actionlint, Node syntax checks, governance/release-identity/matched-manifest
tests and the entire no-compile quality runner passed locally. Actual packaging
with tiny generated image fixtures passed all three ZIP integrity checks,
13-asset checksum verification, raw board-pair URL/hash matching, source manifest
and overwrite rejection. Dummy PE/APK/firmware bytes are not installable hardware.
The stale focused Protocol.h target certificate was reread against the exact
PCController 20c6115b donor source; only later identity/display capability additions
changed. Its tunnel/envelope/CRC bounds remain unchanged; 23 mapping checks pass.

GitHub owner-scoped API readback verifies About/topics, Discussions, merge/cleanup
and signoff settings, 17 relevant custom labels, read-only workflow defaults,
private vulnerability reporting and Dependabot security fixes. Original labels,
rulesets and secret scanning remain intact. One TLS timeout was resolved through
the user-provided local HTTPS proxy; rerun consulted live labels before writes.

No native/firmware/Android rebuild or deployment was made for repository edits.
The complete expanded CI/CD is published on PR #1; safe default-branch presentation,
CodeQL and governance are published independently. New CI/publisher execution,
native package/runtime and physical/full-product acceptance remain open. CI is
not waited on, failed checks are not described as passing, and no release/tag is
created to decorate a badge.

## Web settings and synchronized catalog checkpoint

INTERFACE-CONTRACT.md defines the single generated Rust/Web/Dart page and field
catalog, build-only classification, implemented Web adapters and explicit gaps.
Actual module/HTTP-double tests cover required fields, dirty/conflict/secret
semantics, positive ACK/readback, no write replay, late-read revocation and
capability-overriding panel preferences. All existing Web access/context/table/
appearance/identity/remote/display, branding, PU route and transport source guards
passed, as did comparison/preflight/storage checks and Rust source parsing.
ESP busy-wire build passed with 756732 flash bytes. Native Test stopped at its
canonical cache guard before Cargo; mobile tests are added, not locally run.

Computer-use inspected actual embedded HTML through the mutation-disabled,
visibly simulated fixture: search/navigation, MQTT draft preservation, labeled
Wi-Fi/secret rows, independent WebSocket/HTTP controls and responsive geometry.
Saved/inspected screenshots remain user-facing outputs; docs/screenshots/
web-device-settings.png is the small final Wi-Fi proof. No production/native
replacement, device setting write, firmware flash or physical observation.
ASUS SSH refused; ledboard.local did not resolve. CI is not waited on.

Latest donor changes are hash-bound dirty-source snapshots, not finished remote
releases. Reread board_info/status_bar/menu/icons fully; their new untracked
dropdown dependency and consumers remain named gaps. Actual PCController card
visibility/collapse/reset actions are adapted; panel ordering/icon/localization
work remains pending. Existing authoritative ledger reports 1885 paths, 53
complete-read comparisons, 1832 pending and 35 known gaps.

## Application identity checkpoint

BOARD-APPLICATION-IDENTITY.md records exact owning fields, old-client-compatible
HTTP/USB commands, source imports and immutable host identities. Actual Web
identity/access/context/branding/display/cells/remote/menu checks and production
host protocol suite pass, including paired identity migration/failed commits and
all three APP macro test profiles. Source parsing is not Rust compilation:
native Test was refused at the 13.49 GiB cache guard and production is unchanged.
CLI/USB/native unit and physical acceptance remain open in #10. Read-only
simulated browser proof is docs/screenshots/identity-settings.png, inspected;
no physical name/firmware/EEPROM write is claimed. CI is not waited on.

These are source-comparison and test-execution receipts, not a production
release certificate. Application/firmware source version remains 0.2.5.
File-by-file continuation/checklist is [issue #3](https://github.com/atomicdeploy/PakarController/issues/3);
the full production scope stays open in [issue #2](https://github.com/atomicdeploy/PakarController/issues/2).
No board flash, physical measurement or new live UI deployment was performed
for this audit. The new code changes restore donor tests and strengthen audit
checks; they do not resolve the reported firmware/UI parity gaps.

## Frozen donor inputs

| Donor | Checked-out revision | Working-tree state at refresh |
|---|---|---|
| Pealayer | `78563ff7501e13cb8e40a60dd181f69f127c550d` | Clean |
| PCController | `20c6115bb462fe5111b66cd7c21f7328a1f78892` | Clean |
| PU850-ESP / ASA0002E | `431104072952f694a35b3a83c19f13fbb098ca5b` | Clean; 63 tracked sparse files absent |
| ControllerBoardMini/Puzzles | Filesystem snapshot | Every inventoried file has original-byte SHA-256 |
| ControllerBoardMini legacy tree | Filesystem snapshot | Every inventoried file has original-byte SHA-256 |
| PuzzleBoard / XS / 40LED | Filesystem snapshot | Every inventoried file has original-byte SHA-256 |

These revisions identify the inspected local snapshots; they do not assert
that no newer remote commit exists. `generate-reuse-ledger.ps1` was rerun
against all six roots, followed by the comparative evidence validator.
Tracked Git files and non-build filesystem files are inventoried; build/cache
products are not donor source. Sparse paths remain recorded rather than erased.

## Whole-file equality receipts

Both sides of each pair were checked using SHA-256 over the original bytes.
There is no whitespace-normalization escape in these equality claims.

| Donor file | Exact Pakar target | Equal SHA-256 |
|---|---|---|
| Pealayer `src/messaging.rs` | `desktop/src/messaging.rs` | `471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573` |
| Pealayer `assets/themes/palettes.json` | `assets/themes/palettes.json` | `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501` |
| PCController `LocalLib/ShiftRegisters.h` | `firmware/avr/ShiftRegisters.h` | `397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff` |
| PCController `LocalLib/ShiftRegisters.cpp` | `firmware/avr/ShiftRegisters.cpp` | `43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133` |

All 90 Puzzles note/REST `#define` lines also match the target exactly; Pakar
adds `#pragma once`. This retains pitches, not every donor melody or caller.
For adapted files, normalized target hashes and exact source-byte hashes are
separately labeled in the comparison manifest and CSV.

## Executed checks

| Command / check | Observed result | Proof boundary |
|---|---|---|
| `cargo test --locked --release --manifest-path desktop/Cargo.toml` | 42 passed, 0 failed | Native unit tests and egui synthetic-frame interaction; not whole-application visual parity |
| Pealayer icon subset within that run | All 11 passed | Ten actual donor tests plus one adapted Pakar editor call-site test; not proof every panel uses every imported helper |
| Native palette tests within that run | All four palette/theme combinations contain 14 valid roles; surfaces/text and geometry checks pass | Native role mapping only; embedded Web palette/contract still missing |
| `pwsh -File tools/test-protocol.ps1` | All 10 C++ suites passed | Actual shared/production code with host/register/transport/filesystem doubles; not physical hardware |
| `pwsh -File tests/file_comparison_test.ps1` | Passed | Rejects stale source/target hash, missing anchor, absolute/escaping path, duplicate/orphan donor, incomplete manual-read marker, vague omission, unproven improvement and unactionable gap |
| `pwsh -File tools/generate-file-comparison.ps1` | 1,868 rows; 26 hash-bound comparisons; 1,842 pending; 16 compared files with gaps | Exhaustive inventory accounting, not exhaustive manual comparison |
| Same command with `-RequireComplete` | Correctly rejected: 1,842 un-compared files and 16 known parity gaps | Source-comparison release gate; physical/deployment acceptance remains separate even after it passes |
| `pwsh -File tools/check-reuse-ledger.ps1` | Integrity passed; 1,863 rows unresolved under older reuse-acceptance schema | Older ledger states are deliberately not promoted by this new comparison report |
| `pwsh -File tools/verify-versions.ps1` | ESP, AVR, native and Android sources match 0.2.5 | Not installed-device identity |
| `node tests/web_display_test.mjs` | Passed | Embedded Web display controls, defaults and payload contract |
| `node tests/web_access_test.mjs` | Passed | Sign-in/rotation/rename/reconnect/sign-out mocked Web contracts |
| `node tests/boot_audio_test.mjs` | Passed | Welcome versus actual update-success routing; not audible timing |
| `git diff --check` | Passed | Patch whitespace only |

The ten C++ suites are protocol, Telnet/editor, busy-line, TonePlayer/BootMelody,
device identity, access policy, access persistence, async dispatch, firmware
byte-range parsing and front-panel presentation. The tone suite executes all
fourteen current catalog cues, including distinct 530/520/590 ms welcomes,
late deadlines, rollover, queue capacity and mute/unmute using register doubles.

## What changed because of this comparison

- Restored Pealayer's actual icon/picker regression block. Most of the large
  icon-file diff was missing tests, not hundreds of missing production features.
- Strengthened palette checks without pretending the Web contract exists.
- Corrected older reuse documentation: pointers and source presence do not
  establish complete behavioral or all-surface parity.
- Added a comparative gate and CI checks. Every omitted behavior in a reviewed
  file names exact code and a concrete reason; useful unresolved code remains
  pending, not a convenient exclusion.

Current PCController's TM1637 ACK/reconnect recovery, Puzzles' generic scheduler
and key callback/debounce contract, shared native/Web appearance, configurable
LED channel order, generic PU shell parameters/events/utilities and serial shell
remain actionable findings in `FILE-COMPARISON.md`. The report must not be used
to claim that Pakar is already superior overall.

Earlier CI for `f190c9cd1cb7a2cd6df304ad99fd00a31472e126` completed successfully
in runs [38031399766](https://github.com/atomicdeploy/PakarController/actions/runs/38031399766)
and [38031396790](https://github.com/atomicdeploy/PakarController/actions/runs/38031396790).
Those runs predate these restored tests/audit checks and do not certify this
checkpoint. New CI status is recorded in the GitHub tracking issue.

## Native Windows restoration checkpoint

The comparison now includes 31 hash-bound full-file reviews (1837 pending,
18 known parity-gap files). Newly read files are the actual toast renderer,
Windows quick-action catalog, identical shell-icon generator, application
shortcut model/tests and Windows shell suite. Generic rebinding/lifecycle gaps
were exposed, not relabeled business exclusions. The established workspace
picker call-site was re-reviewed after shell changes; its actual picker and
tests remain unchanged.

Executed: 70 Rust tests; locked release build; comparison generation and
adversarial comparison/ledger tests; Web display/access and boot routing tests;
authenticated CLI fixture; all component source versions 0.2.5.
Local real-profile Windows runtime emitted a positive Shell shown callback;
fullscreen toggled and restored through IPC in the same owner. No board writes
occurred; ASUS SSH refuses connection and ledboard.local remains offline.
Full native/runtime evidence and concrete remaining source mappings are in
`WINDOWS-DESKTOP-AUDIT.md`. Exact-commit CI is tracked in issue #5; source/local
tests are not automatically a passed GitHub build or complete production release.

## Current-hardware PCController contract checkpoint

Source 0.2.6: 23 focused donor-file mappings against PCController
20c6115bb462fe5111b66cd7c21f7328a1f78892, with current source/target
hashes and line anchors in pccontroller/SOURCE-MAPPING.json. Donor hash
verification ran against the clean canonical checkout. Separate legacy
inventory remains 31 comparisons, 1837 pending, 16 known parity-gap files;
the two TM1637 recovery gaps now have implemented/tested source mappings.

Executed locally: all production-code C++ component tests including PC
contracts, feature-off profile, 73 Rust tests, locked Windows release build,
authenticated built-CLI contract fixture, Web/access/boot tests, source
version check, comparison/ledger/adversarial checks, both firmware variants.
Initial e75a26a AVR busy-wire 26672 flash bytes/1203 static RAM; polling 26386/1201.
ESP busy-wire 595560 IROM bytes/49772 static RAM, polling 595348/49764;
both IRAM 63055 including 32768 instruction cache reservation. Each merged
AVR plus Urboot image is 32768 bytes.

Fixture proof includes torn writes, chunks, late receipts and polling mirrors;
it is not physical EEPROM, audible melody or electrically correct TM1637
proof. No board writes. New exact-commit CI and hardware/surface acceptance
remain tracked in PC-CONTRACT-TRACKING.md and the focused GitHub issue.

Follow-up imports actual ControllerEvents header/implementation and restores
exact five-byte key/buzzer payloads; no private UART timestamp extension.
Final AVR busy-wire 26696 flash bytes/1221 static RAM (827 bytes for locals);
polling 26410/1219 (829 bytes). ESP measurements unchanged.
The e75a26a Linux firmware job built both boards but failed host compilation
on an indirect offsetof dependency; AutomationStore now explicitly includes
stddef.h. Android CI passed for e75a26a, not automatically for the follow-up.

The f2e38a7 follow-up passed all three GitHub jobs (firmware, Windows desktop,
Android preview) in run 38044317586. A later capacity fix adds authenticated
terminal receipt consumption after client response validation; source tests
exercise 32 transport cycles and 12 consecutive built-CLI settings reads.
The new fix is not automatically certified by the earlier green run.
Final response-validation tests cover all eight macro selectors, rejecting
ACK-for-query, status-for-mutation and zero-code ERROR. Built-CLI fixtures
exercise both query/download and mutation replies with request-bound expected
opcodes. These are protocol/fixture tests, not physical execution proof.

## Donor-drift and Web appearance checkpoint

Pealayer d3f11b1: 494 inventoried files, 20 complete comparisons, 474 pending
and ten reviewed gap files. Overall 1,869 / 42 / 1,827 / 23. Actual new
storage summary/equality code and all five appearance functions/four donor
tests are mapped in the SAME evidence manifest. Fully reading App/styles does
not close their pending complete-file comparisons. Exact generic omissions and
connected appearance transport remain visible, not relabeled business logic.

Executed locally: ten actual appearance/shipped-browser tests, existing Web
access/display/cells/remote tests, PU route/hash tests, donor ledger and comparison
adversarial tests, live required-source gate, storage summary/equality, build Plan,
screenshot source guards and source-version checks. Real browser screenshots and
fixed-geometry/390-pixel overflow/persistence checks are SIMULATED loopback proof.
See WEB-APPEARANCE.md and REUSE-PREFLIGHT-RECEIPT.md for failures and exact recovery.

Only affected ESP busy-wire compiled, through tools/build-firmware.ps1 without
Clean and without SDK/library installation. Actual installed cores: ESP8266 3.1.2,
MiniCore 3.1.3; libraries FastLED 3.10.6, WebSockets 2.7.2, PubSubClient 2.8,
ArduinoJson 7.4.3, ESP Async WebServer 3.12.1, ESP Async TCP 2.0.0, matching CI locks.
RAM 56,072 / 80,192 bytes, IRAM 63,055 / 65,536 (includes 32,768 cache reservation),
IROM 694,512 / 1,048,576. This is static link output, not measured runtime heap,
latency or physical reliability. Artifact build/esp8266-busy-wire/esp8266.ino.bin:
744,160 bytes; SHA-256
01ad9287377289c6b7eebf94e6e6a2d35eca9fc3245480214fce0bf69a8540fc.
Shipped Web source UTF8-LF SHA-256
fd18909f94a4ca13d5cdd367f6cc7a356644dfc0bfd245cc4e976beb4c1db9b5.

Previous ecf882d CI runs 38059703363 and 38059700392 both passed all three jobs.
New checkpoint CI must be checked separately. SSH localhost:1022 still refuses
and ledboard.local still does not resolve; no firmware flash or board verification.
Native app, private preferences, AVR image, Android package and donor builds were
not replaced/rebuilt. Existing oversized cache/blocked cleanup remain unchanged.

## Device-context evidence

Tracking #10. Actual PCController availability and Pealayer board-info owners were
read and mapped; status-bar readiness slice remains screened. Three new complete
comparisons and all preserved changed-target anchors pass normalized hash checks.
1884 inventoried paths, 51 complete comparisons, 1833 pending, 32 known gap files.
No whole-repository equivalence or superiority claim.

Production context C++ tests pass both default Table and Table-disabled profile.
The shipped Web code passes individual ACK, no-context, expiry, cross-board,
mode-kind and malformed geometry/schema tests. Existing protocol, device, access,
pixel, display, update, appearance and source-guard tests pass. Rust source parses;
Rust tests/types/runtime are NOT established because the native wrapper stops at
its unchanged 8 GiB cache-budget guard. No running native app replacement.

Firmware static build results:

| Profile | AVR flash / RAM | ESP RAM / IRAM / IROM |
|---|---|---|
| busy-wire | 26906 / 1225 bytes | 56284 / 63055 / 719216 bytes |
| polling | 26620 / 1223 bytes | 56284 / 63055 / 719004 bytes |

SHA-256 of build candidates, not deployed identity:

- AVR busy-wire merged image: `6c7552e3ae98668045add8a3156ae422d0dc84a2afd630b88276a6b57641e178`
- AVR polling merged image: `4bc5fb7963863673d1d736693800dc1ca1ef162c4669eb0a52dca25d12e465b4`
- ESP busy-wire: `8aadebacc965d45d728510417ee53ac08ab25d5a2240d61cdf620bb0af299af5`
- ESP polling: `d7f308e218293b50c965223dd0af847090db289a23c09a427c4059341d186803`

Read-only simulated Web browser screenshots were actually inspected and retained
as `docs/screenshots/context-{controller,unacknowledged,table}.jpg`.
No board commands, firmware/EEPROM writes or native desktop screenshots occurred.
Owned temporary tabs and identity-verified fixture processes were closed/stopped.
SSH localhost:1022 refused; ledboard.local port 80 unreachable. CI not waited on.
# Persian source and Web rendering checkpoint

Owning issues #4 (locale/settings), #3 (donor inventory), #9 (native storage).
Three actual complete-read Pealayer files certified: src/ui/i18n.rs,
web_ui/src/i18n.ts, tests/i18n_surface_audit_test.rs. Configuration function
excerpts are explicitly NOT certified as a whole source file. Inventory now has
1888 paths, 61 complete comparisons, 1827 pending and 39 compared gap files.
The Pealayer subset has 509 paths, 36 compared and 473 pending. No full-import or
whole-repository superiority claim; concrete omission reasons remain per file.

Local `node tools/ci-checks.mjs quality` passed all children, including actual Web
module tests, font/generator checks, locale/RTL/application-copy ownership,
settings ACK/readback/secret/conflict guards, donor hashes/anchors, route mapping,
access/authentication, appearance, table, identity and repository contracts.
Those tests are source/DOM/HTTP-double evidence, not native renderer or hardware.

Actual embedded Web read-only fixture capture was inspected with lang=fa,
dir=rtl, Vazirmatn loaded, no horizontal overflow; joined Persian settings hints,
navigation, placeholders and status are visible. Retained refined screenshot:
outputs/PakarController/localization/persian-settings-refined.jpg (local output).

Native wrapper Test refused its unchanged oversized-cache guard before Cargo.
Native glyph/shaping/registry-retention/locale tests are added but NOT executed;
old running app is preserved. No obsolete-artifact cleanup approval received.
ESP busy-wire candidate build passed: RAM 56620/80192, IRAM 63055/65536,
IROM 844784/1048576. Image SHA-256:
2676c89f113a846112f61e3febdbbacce97cd2107789067c18cf8e070d5afd5e.
Not deployed. AVR unchanged; Flutter SDK absent on PATH, no APK build. SSH 1022
refused and ledboard.local unresolved. CI was not waited on. Full donor mappings
and pending dynamic/native/mobile translation acceptance: PERSIAN-PARITY.md.

# RayanLamp / acknowledged mobile preview checkpoint

Extended actual RayanLamp digit maps into Dart; no unlicensed YekanBakh asset.
PCController's ready/advertised contract and Pealayer's actual branding fallback
semantics now also govern mobile. Existing six RayanLamp complete-read files remain
six, not a claim that all 2349 paths are imported. Overall inventory: 4237 paths,
67 complete-read comparisons, 4170 pending, 42 known gaps. Full omissions remain
in the authoritative evidence JSON and issue #3; mobile pending surfaces are in
MOBILE-CONTEXT.md.

Flutter analyzer has no issues and 29 tests passed through build-mobile.ps1.
Actual simulated renderer capture with loaded Vazirmatn/Phosphor inspected at
outputs/PakarController/mobile/table-selection-complete.png. Image SHA256:
60e2077ed3a842b966b5e73a7ba3d13c5f35e9cba4d455ade360b19af2e7b148.
All 80 cells/stagger/dark alternation/amber selection/hidden indicators are visible.
First clipped capture was preserved, not overwritten. This is not a phone device.

Existing installed SDK was found from mobile README, correcting the earlier PATH
limitation without another download. Preview APK built in the warm canonical cache
and copied once to outputs/PakarController/mobile/PakarController-preview.apk.
SHA256: 4aabc188bdab5f984b064e17ec417f85e467436d8e08d0eb94eaf984fbbeb1b4.
Version 0.2.7; debug-template signing, NOT production. Mobile cache 1.021 GiB.
No native-cache cleanup/bypass, running desktop replacement or hardware flash.
Hardware access is still unavailable (SSH refused, hostname resolution timed out).
CI not waited on. Native, physical, mobile locale/settings and full donor parity
remain open; no product-wide completion or superior-repository certificate.

# Mobile device editors and actual PreferencesTab mapping

Owning issues #4/#3/#2. Fully read actual Pealayer PreferencesTab.tsx, 593 lines,
original-byte SHA256 bada7d4b1cdd332fc51bcbc00ac59544c860f7012eca7d425e477ef932170c27.
One new parity-gap comparison: contract-driven navigation/groups/typed controls
adapted to Flutter with real Pakar save/readback ownership. Concrete remaining
appearance, generic controls, localization, file-picking and dynamic-option code
is not declared unnecessary. Total 4237/68/4169/43; RayanLamp still six reads.

Analyzer clean; all 46 Flutter tests passed through build-mobile.ps1, including
phone/tablet layout, conflict/no-replay/secret semantics and delayed credential/
rename replies after view closure with same/different connection owners. Actual
simulated renderer captures load all three fonts and assert zero mutations:

- Tablet SHA256 b25da59a2cec4dcb7320eaff0353764d1ac6762dad99cef64e87bc6fb0d0253d.
- Phone SHA256 3459f61adb2e23e6df15912fec9c6c52b0f7a4300b8b5a9d639496bf09f1a233.

Both images inspected; original captures preserved; repository copies verified.
Warm preview APK SHA256 8b01492724b48daf0d42ed3666f82144c6e7d6aa55d26614eabd25502f00f596.
Debug identity signing, not production; 0.2.7 source versions remain consistent.
One stable preview destination, prior APK retained by recoverable rename. No new
SDK/cache/clean or native/firmware build; mobile cache 1.027 GiB.

Source quality suite passed all children. New strict-mode comparison fixtures
also pass with zero specialist routes and absent optional advantage text;
full generation/read-only Check verified after structural report refresh.
Native compilation/runtime, physical devices, full mobile preferences/firmware/
locale and all remaining donor files stay unchecked. See MOBILE-SETTINGS.md.
