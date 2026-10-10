# PakarController comparison receipts

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
