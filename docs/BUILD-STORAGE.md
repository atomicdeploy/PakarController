# Low-write build workflow

## Current debug cleanup and prevention

New explicit user authorization allowed pruning the exact ignored, unused
`desktop/target/debug`: 11,852 files / 10.408 logical GiB. Its root/ancestors and
contents passed reparse, ignored/tracked, known-root-content and active-owner/
compiler checks. No compiler or debug executable was active. Installed app,
rollback executables and warm release executable hashes stayed unchanged;
free C: increased from 24.08 to 33.24 GiB. Logical size is not allocated bytes.
Generated debug output was permanently removed and can be rebuilt if needed.
Release cache, settings, source, shared SDKs and existing APK state were not deleted.

Normal Check/Test now use `--release` as Pealayer's runner does for normal builds,
sharing the same production dependency profile and canonical cache. Explicit
`-DebugArtifacts` opts into dev/test output only for essential debugging; Release
rejects that flag. Tests still run serially with isolated config and locked
dependencies. No routine whole-cache clean, automatic repeated pruning or flags
that silently weaken a debug profile. The existing 8-GiB guard remains.
`storage-hygiene.ps1 -PruneObsoleteDebug -Plan` is read-only; actual pruning needs
the explicit flag and refuses an overridden cache, links, unknown root content,
tracked files and active compilers. Inventory remains the default.

The current user also asks to defer subsequent test runs until completion or
significant progress. Mobile `-Mode ApkPreview -BuildOnly` supports compile-only
iteration without repeating analysis/tests; default acceptance still runs both.

RAM disk sizing: machine total is about 64 GiB, observed free RAM only 9.42 GiB.
Eight GiB was a whole-cache budget, not a RAM-disk minimum. An optional 1-GiB
scratch disk may cover selected temporary files after measuring their peak;
the entire cache does not need to be on RAM. Do not allocate a larger disk while
that would starve applications/compilation. A 1-GiB disk cannot contain the
observed 3.081-GiB native release dependency cache. Compiler objects/metadata,
symbols and incremental states are build footprint, not runtime app memory.
The installed eframe 0.36.2 manifest confirms its default WGPU backend plus this
app's explicit Glow feature compile both graphics stacks. This adds build cost;
it is not a justification for calling 8 GiB a runtime or scratch requirement.
Backend reduction needs explicit renderer/platform and screenshot acceptance;
it is not silently applied here merely to reclaim cache. Accessibility, fonts,
links and platform support must not be discarded as unused without review.
The current desktop executable is 24,992,768 bytes; an observed current process
working set is 158,736,384 bytes and private committed memory 393,232,384 bytes.
These are point-in-time runtime measurements, not constant upper bounds.
No RAM disk, cache relocation, paging change or shared-cache deletion was made.

Historical blocked cleanup notes below are preserved as history, not current state.

## Measured cause

### Arduino whole-cache invalidation on a new sketch helper

An S3 delivery showed a second, distinct cause of writes: CLI1.5.1 records the
entire sketch AdditionalFiles list in build.options.json and wipes/recreates the
build path when that map changes. Adding BleStorage.h changed38 entries to39;
all other properties (production FQBN/compiler flags/paths) matched. Actual cache
creation and new Wi-Fi/BLE object timestamps confirmed genuine recompilation,
not merely the seven-minute dependency scan. No explicit clean or SDK change was
requested. The wrapper now describes a reusable cache, not an assured warm hit.

Preserve the run and subsequent same-inventory/flags cache. Do not falsify options,
restore unverified deleted objects, or use the hidden discovery-skip flag; that
flag does not bypass this invalidation. A stable shared-library source layout is
an open follow-up in issue#9, not an implemented selective-cache restore or a new
parallel cache. No extra build was run merely to measure a no-op.

Exact reviewed CLI source:
[stored file inventory](https://github.com/arduino/arduino-cli/blob/01f3d4f2b/internal/arduino/builder/build_options_manager.go#L72-L81),
[comparison and wipe](https://github.com/arduino/arduino-cli/blob/01f3d4f2b/internal/arduino/builder/build_options_manager.go#L106-L170).

### Native debug artifacts (historical measurements)

The Pakar canonical Cargo target occupied 13.49 GiB: debug/test 10.408 GiB
(deps 7.847, incremental 1.947, build helpers 0.613), release 3.081 GiB.
Six retained hashed Pakar test PDBs alone were approximately 260 MiB each.
Default full debug information, retained compiler/dependency generations and
separate debug/release outputs explain most of this project's footprint.
This measures retained logical bytes, not SSD physical writes or endurance loss.

Other local footprints: Flutter installer ZIP 1.798 GiB, mobile build 0.971 GiB,
mobile .dart_tool 0.204 GiB, firmware outputs 0.090 GiB. Shared Arduino15 is
10.57 GiB (packages 7.39, internal 0.69, staging 2.43); Cargo registry is 1.465
GiB and Gradle 2.915 GiB. They are shared SDK/cache state, not safe blanket-clean
targets. Separate Pealayer cache measured 31.98 GiB and an active Pealayer GNU
test build was observed. It is outside Pakar ownership and was left untouched.

The Flutter 3.47.3 SDK installation was verified from its existing version file
and launcher. The matching downloaded installer is redundant for installed use.
Requested cleanup of Pakar's ignored debug cache and that installer (12.206 GiB
combined) was blocked by execution policy before execution. Both remain present;
no space was reclaimed and no shared toolchain/source/backup was deleted.

## Prevention

- Native dev/test profiles disable full debug information, retaining debug
  assertions, overflow checks and incremental reuse. Do not toggle compiler flags
  or dependency versions gratuitously: each variant can retain another generation.
  Opt into symbols only for a specific debugging session using
  CARGO_PROFILE_DEV_DEBUG=2 and CARGO_PROFILE_TEST_DEBUG=2, then review that cache.
- tools/build-desktop.ps1 defaults to Check (no executable link), dispatches Test
  or Release explicitly, locks dependencies and uses one desktop/target directory
  regardless of shell cwd. Preflight refuses less than 8 GiB free or more than
  8 GiB in that cache. These are preflight guards, not filesystem quotas; compilation
  can grow the cache, so a subsequent guarded invocation stops for review. Existing
  13.49 GiB cache intentionally blocks builds until obsolete artifacts are reviewed.
- Keep the warm release cache; never repeatedly cargo clean. Changing debug profile
  settings causes a one-time dev/test recompilation when those tasks are needed.
  Do not run that large rebuild merely to prove this storage-policy change.
- Firmware defaults to incremental Arduino compilation in stable per-board/link
  build/cache paths. Target only avr or esp8266 when appropriate. -Clean / --clean
  is explicit troubleshooting; AVR merged-image size checks still run when AVR is
  compiled. The build.cmd entrypoint continues to build both and run protocol tests.
- UI-only native changes do not trigger firmware or Android builds locally.
  CI still verifies all components on its own runners using the same low-symbol
  profiles and native wrapper; it does not churn this PC's cache.
- Run fixtures directly from the built executable. Maintain one stable
  outputs/PakarController/staging candidate, not another complete package per edit.
  tools/stage-desktop.ps1 accepts an existing exe, hashes source/destination and
  skips an identical copy. Changed payloads are copied once to a verified pending
  file then renamed. It never builds, copies SDKs/firmware/APKs, saves credentials,
  or replaces production. Existing historical receipts/backups remain preserved.
- Production deployment is a separate graceful IPC/draft-check step, retaining
  settings and a rollback. Avoid new full historical package copies on every edit.

## Commands

```powershell
./tools/build-desktop.ps1 -Plan
./tools/storage-hygiene.ps1
./tools/build-desktop.ps1 -Mode Test
./tools/build-desktop.ps1 -Mode Release
./tools/build-firmware.ps1 -Target esp8266 -LinkMode busy-wire
./tools/build-firmware.ps1 -Target avr -LinkMode polling -Plan
./tools/stage-desktop.ps1 -Source ./desktop/target/release/pakarcontroller.exe -StageDirectory ../../outputs/PakarController/staging
```

Linux: ./build.sh busy-wire esp8266; ./build.sh polling avr --clean is an explicit
cold-build diagnostic, not the default.

## Verification scope

tests/build_storage_test.ps1 checks real Plan dispatch for Check/Test/Release,
locked canonical targets, default budgets, board-selective firmware plans,
mode flags, explicit clean and PowerShell parsing. Cargo locked metadata parsed
the manifest without compilation. A live identical-exe staging invocation returned
Changed=false, Written=false. The oversized-cache guard actually refused a build
before spawning Cargo. Shell syntax is checked separately; CI performs real builds.
No native, firmware or APK rebuild/deployment is claimed for this policy-only turn.
The running app is unchanged; existing unit-test and hardware evidence stays
attached to its earlier source, not reattributed to this infrastructure patch.

References: [Cargo profiles](https://doc.rust-lang.org/cargo/reference/profiles.html)
and [Arduino compile flags](https://arduino.github.io/arduino-cli/1.5/commands/arduino-cli_compile/).

## Actual donor-tool follow-up

Pealayer's new storage-hygiene.ps1 was read fully at d3f11b1 and its real
Get-DirectorySummary imported into tools/storage-hygiene.ps1. Missing directories
stay explicitly missing; empty directories use zero; unreadable content fails
instead of silently undercounting. Read-only default lists logical files/GiB for
the established Cargo/Arduino/mobile/staging locations, with the existing budget.
The actual new content-equality helper is also imported into windows-build-host
and called by staging. No automatic deletion, duplicate cache, hardlink to mutable
compiler output, or guard override was introduced.

The donor's new cache-outside-checkout rule and guarded prune are documented
pending requirements, not claimed implemented. Preserve the current sole cache
and owners until an approved nonduplicating migration/cleanup boundary is resolved.
The existing oversized-cache guard still blocks native builds. Detailed reasons,
hashes, exact target/test anchors and failure recovery are in the SAME authoritative
comparison ledger and REUSE-PREFLIGHT-RECEIPT.md; issue #9 stays open.
