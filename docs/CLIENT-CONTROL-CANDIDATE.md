# Client controls and host presentation candidate

Tracking: [issue #26](https://github.com/atomicdeploy/PakarController/issues/26).
This checkpoint prepares updates; it does not record a board or client deployment.

## Implemented and locally verified

- Diagnostic pixels and outgoing correction slots are explicitly different address
  spaces. Handoff resolves the saved source map, converts UI numbering exactly
  once, and requires a choice for ambiguous/missing mappings. No saved correction
  index, rule ordering, EEPROM or filesystem data is migrated.
- Flutter and native correction preview is enabled by default, but opening the
  page is passive. Editing uses a bounded RAM lease and exact active-map readback.
  Save is the durable operation. Web uses the same existing preview controller.
- Flutter correction read/preview/save works through the generic USB API as well
  as the correlated WebSocket/HTTP path. **Native correction management remains
  Wi-Fi-only**; its panel does not claim USB parity.
- Effects controls follow peer-bound snapshots while open; confirmed readback
  updates the visible values. A changed boot retires the editor. Flutter has a
  shared sixteen-name palette, HSV controls, optional hex and session recents.
- Flutter pinch owns two-finger gestures without consuming normal cell holds.
  Pan stays bounded; release snaps smoothly and new wheel/fit input cancels a
  pending snap. Narrow-phone footer values ellipsize and retain full details.
- Removed repeated physical-output narration from cell success messages and
  added a source regression guard. Logs/protocol still distinguish acceptance.
- Keycontroller Web appearance mounts in the canonical workspace, not the removed
  `.stack` container. Its live dropdown changed twice without reopening in the
  isolated browser fixture; this is not hardware acceptance.
- Host presentations share one compositor, mapping, correction pass and output
  driver. Cells/groups, bulk updates, persistent saves, temporary overlays and
  complete RGB frames use a fenced common command. See
  [HOST-PRESENTATION.md](HOST-PRESENTATION.md) and the explicit-run examples.
- Melody/beep/relay commands use typed WebSocket commands with actual AVR handler
  acknowledgement. AVR starts the tone or latches outputs before ACK; ESP services
  local input before optional outbound housekeeping. No acoustic latency claim.

egui/eframe stays **0.36.2**. The current dock, Phosphor and theme dependency
family requires that type family; forcing core 0.37 alone is not a coherent
upgrade. This is a compatibility deferral, not a toolchain/MSRV limitation.

## Evidence

- Flutter analyzer clean; 84 focused widget/model tests, 5 actual local
  WebSocket/HTTP transport tests and 2 USB correction transport tests passed.
  Final pointer-retirement repair additionally passed all 7 viewport tests;
  the Android candidate below includes that repair.
- Native canonical Check/Release passed; 18 correction and 10 realtime tests
  passed. The latter includes the AVR fast-command test.
- Canonical LED pipeline, audio, USB API/receipt and host-presentation tests
  passed. Full captured USB status with host summary fits the existing budget.
- Nine affected Web suites passed, including actual extracted renderer behavior,
  preview lifecycle, command recovery, workspace/peer guards and gzip round-trip.
- Source versions agree at 0.2.8. Donor comparisons retain explicit omissions;
  neither a current hash nor a test anchor closes broader parity work.

## Built artifacts (local candidates)

| Artifact | SHA-256 |
| --- | --- |
| `build/esp32s3-buffered/esp32s3.ino.bin` | `9920bc9de5d07f8cd8dec169bd707d15eecdc8ced5f9e4fe73ff2447e7c3c755` |
| `build/avr-buffered/avr.ino.with_bootloader.bin` | `15874f171a62eb39ce4acee55cdd732c06a3f57180dcbe3eb96cbd01e1701d5f` |
| `desktop/target/release/pakarcontroller.exe` | `4ec7b28d21f3f9d4e5d8208456b15b5d8edfdc07018fdf6c867b3ab28612823d` |
| `mobile/build/app/outputs/flutter-apk/app-release.apk` | `81006c55469a7d97f8055d73b9ec3ba949257cf16bdba51b07fc81f20c053ba5` |

S3 candidate: table-selection, buffered UART, 16 MB flash/OPI PSRAM, GPIO4,
both LED backends/GPIO default, BLE disabled, Ethernet included, ISP included,
preserved recovery bundle. Flash 2,312,627 / 3,342,336 bytes; static RAM 138,524 /
327,680 bytes. These are build figures, not free runtime heap.

AVR candidate: table-selection/buffered/no RF; 28,668 bytes flash and 1,488 bytes
static SRAM. Do not apply this profile to a radio-enabled keycontroller. The
preserved recovery bundle was not silently replaced with this candidate.

Android uses the existing preview signing identity and production-hold build
flag. It is not a new store release or evidence of installation.

## Before deployment acceptance

Identify the actual target and its profile, retain settings/rules/scene, then
perform an authorized update. Verify actual diagnostic-to-correction handoff,
preview/save/discard, streamed effects controls, pinch/pan on phone/tablet,
and audible command latency. Run the host examples only with explicit permission
to change visible output. The example Snake is a host-owned autonomous trail,
not a complete interactive game; the protocol permits richer host mini-apps.

Production issue #2 stays open. No board reboot, flash, scene mutation, app
replacement or optical/audio acceptance occurred during this checkpoint.
