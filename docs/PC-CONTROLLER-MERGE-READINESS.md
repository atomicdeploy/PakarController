# PCController comparison and AVR convergence

## Scope and conclusion

Compared the actual installed PCController source at
`b0111ae0c80dc34149ea1044de7dd5488920301b` with PakarController
`8ce8f29eee01a979bbd42e7d3573a6e3ace9eba2` and preserved local changes.
The separate `work/repos/PCController` checkout is at `2a0e971e`; it is not
the reference for this comparison. Neither donor nor firmware was changed.

**A shared AVR core is feasible; a stock PCController image is not currently a
drop-in Pakar AVR image.** Source reuse, wire interoperability, EEPROM
compatibility, build fit and physical acceptance are five separate requirements.
This document is the convergence design and gap explanation, not a completed
firmware merge or exhaustive whole-repository certification.

Exact complete-file comparison records remain in
[the existing evidence ledger](file-comparison-evidence.json). Partial runtime
inspection is identified below and does not certify unread dependencies.
Follow-up owners remain [#6 hardware contracts](https://github.com/atomicdeploy/PakarController/issues/6),
[#3 donor review](https://github.com/atomicdeploy/PakarController/issues/3),
[#4 settings](https://github.com/atomicdeploy/PakarController/issues/4),
[#10 contextual clients](https://github.com/atomicdeploy/PakarController/issues/10)
and [#2 production acceptance](https://github.com/atomicdeploy/PakarController/issues/2).

## What already shares code

Read-only comparison of current donor/target text, with CRLF normalized to LF:

| PCController | PakarController | Observed relationship |
|---|---|---|
| `Project/ProtocolContract.h` | `libraries/BoardCore/src/pccontroller/ProtocolContract.h` | Identical generated opcode/error declarations |
| `Project/Core/MacroRing.h`, `.cpp` | `libraries/BoardCore/src/pccontroller/MacroRing.h`, `.cpp` | Identical implementation text; owning dispatch and storage still differ |
| `LocalLib/ShiftRegisters.h` | `firmware/avr/ShiftRegisters.h` | Identical declaration; different pin/bit-order implementation context |
| `LocalLib/Keys.h`, `.cpp` | `libraries/BoardCore/src/pccontroller/Keys.h`, `.cpp` | Pakar injects sampled state instead of reading the global shift driver and adds validated debounce configuration |
| `Project/ControllerEvents.h`, `.cpp` | `libraries/BoardCore/src/pccontroller/ControllerEvents.h`, `.cpp` | Same typed payload producers adapted to an event sink; UART envelope/timing is not identical |
| `Project/AutomationStore.h`, `.cpp` | `libraries/BoardCore/src/pccontroller/AutomationStore.h`, `.cpp` | Adapted EEPROM/CRC owner, nine keys instead of four, no implicit blank-EEPROM provisioning and additional commit readback |

The header, two macro-ring files and shift-register header were equal; the
other listed pairs were not. An equality check is not a complete behavior
review, compilation or physical test. Existing
complete-read ledger records and remaining actions are still authoritative.

## System-wide gap matrix

| Area | PCController | PakarController | Gap / disposition |
|---|---|---|---|
| Firmware ownership | The same `PCController.ino` and support/domain sources build physical and OS-native firmware | Separate `avr.ino` plus `PcControllerRuntime`; shared ESP application has S3 adapters | Two AVR dispatch/lifecycle implementations still drift; converge reusable domains before any image replacement |
| Host architecture | Go host, canonical JSON-RPC, protected native IPC and loadable C ABI | Rust/egui host, CLI/TUI, selected-owner bridge; Flutter and embedded Web | Pakar does not implement every Go RPC or embeddable host lifecycle; reuse contracts, not a second competing serial owner |
| Physical keys | Four front-panel keys; other shift bits are system/monitored inputs | Eight keys plus ADC6 ninth; role-dependent input order | Share sampled-key classifier, retain profile-owned meaning and physical order |
| Outputs | Relay sides/motion interlocks, user relays, PWM, safety policies | Shift-register output mask and game indicators | A shift LED mask is not a safe relay/motor controller; missing drivers must stay unavailable |
| Sensors/RF | INA219, DS18B20 role mapping, RF transmit/learning and persisted bindings | Future AVR guards reject enabling absent adapters | Useful generic donor engines remain pending; current hardware absence is not permission to delete their future contracts |
| Displays | TM1637, host-owned LCD/I2C and richer optional menu profiles | A4/A5 TM1637, six-page AVR menu plus ESP settings; optional S3 LCD owner | Different bus/pin ownership, page identities and editors; do not turn on AVR Wire while TM1637 owns A4/A5 |
| Audio/events | TonePlayer, local cue policy, MCU-timed native events/ACKs | TonePlayer/cue catalogs, companion mirroring, different timing payload | Frequency support exists, but native timestamp/ACK layouts need deliberate convergence and client tests |
| LED ownership | AVR D6 driver; constrained strip workspace shared with macros, up to 100 pixels | ESP owns table 480 or key-box 216 output; optional legacy donor LED window | Do not move the full table framebuffer/effects into a 2 KB AVR or duplicate its engine |
| Macros/automations | Shared ring and optional MCU automation engine; constrained AVR automation default off | Ring and automation default on for Pakar's smaller hardware scope | Different action sets and memory budgets; AVR-stored steps controlling ESP LEDs still lack completed cross-owner execution |
| Persistence | PC settings/RF/temperature/reset/status-profile or automation allocations | Pakar dual settings banks/automations/reset/Prog allocations; ESP scenes separately | Raw EEPROM is incompatible; preserve backups and use an explicit semantic import, never copy a stock image over it |
| Build targets | Constrained 328P, explicit Mega and native feature profiles | Pakar AVR currently assumes 328P ports; S3/ESP8266 companion profiles | Mega/native support is not inherited merely by importing headers; guard direct ports and bind timers/storage explicitly |
| User interfaces | Existing PCController hardware, settings, automation and host workflows | Table/key-role contextual Rust/Flutter/Web interfaces | Full donor settings, menus, notifications, diagnostics and visual parity remain incomplete; method presence does not mean an editor exists |
| Network/update | Host discovery/bridge/updater and protected lifecycle | ESP autonomous HTTP/WS/MQTT/OTA, remote relay and separate app-management service | More MCU networking does not establish PCController host interoperability or equal discovery coverage on every client |

The app-layer comparison above is architectural, informed by the actual host
lifecycle documentation and current mapped sources. It is not a complete read
of every Go/Rust/Dart/Web source. Unreviewed files remain pending in the ledger.

## Wire blockers: same COBS does not mean same board contract

### Framing and dispatch

Both use magic `0xA5`, COBS, CRC-8/ATM, sequence IDs and a maximum 48-byte payload.
PCController emits reserved envelope byte 1 and ignores its value when decoding;
Pakar emits zero and its current decoder rejects nonzero. These are observed
implementations, not a recommendation to add a public generation selector.

Pakar outer opcode `0x11` means AudioCue; PCController native `0x11` means PwmSet.
Pakar therefore carries donor operations in `0x56`, full-length request chunks
in `0x57`, and donor replies in `0xB0`. This is an existing transport boundary,
not native PCController wire interoperability. A current stock donor image does
not implement this Pakar tunnel. Never probe it by sending arbitrary mutations.

Default application baud also differs: donor 115200 versus Pakar 38400.
Bootloader upload baud is a separate decision, not the application UART rate.

### Payloads and timing

| Message | Current PCController | Current Pakar |
|---|---|---|
| HELLO | 16-byte native build/capability/profile record | Outer Pakar identity; inner 8-byte hardware-profile/features response |
| STATUS | Packed 48-byte sensor/output/reset telemetry | Inner 25-byte hardware-specific snapshot; separate Pakar telemetry/game messages |
| Settings | Native semantic prefix/name response; packed EEPROM structure is separately 31 bytes under current storage flags | 21-byte Pakar settings plus persisted boolean on readback |
| ACK | Request/error prefix plus four-byte MCU timestamp | Tunnel response opcode plus two-byte request/error result; transport requires that exact shape |
| Typed EVENT | Event type high bit and appended MCU timestamp added by native UART sender | Event sink queues its typed bytes; no equivalent common timestamp suffix |
| BUZZER_CHANGED | Five tone-state bytes plus four-byte send-time timestamp = 9 bytes | 13 inner bytes: five tone-state bytes, tone-start micros, observation micros |
| Front panel | 47-byte record, donor page/mode/LCD meanings | Shared offsets, Pakar page/mode meanings and absent AVR LCD content |

Pakar's 13-byte tone report is useful: it distinguishes note start from delayed
observation. Replacing it with a five-byte report would lose information. Its
existence does not make the donor's nine-byte report interchangeable. The old
five-byte-only statement in PC-CONTROLLER-CONTRACTS.md was corrected.

The donor uses a short-lived zero-copy Frame view; Pakar's public Frame owns a
48-byte array. Shared codec work must preserve synchronous lifetime, nested
replies, partial RX and stack limits; changing the typedef alone is unsafe.

## Pin and resource compatibility

| Resource | PCController 328P | Pakar ControllerBoardMini | Convergence rule |
|---|---|---|---|
| Shift chain | A0–A3; D4/D5/D7/D8 controls | Same named hardware lines | Keep wiring; explicit role-dependent input permutation |
| UART | Hardware UART0 | Hardware UART0 | Retain hardware serial and one owner |
| Buzzer | D9 / Timer1 | D9 / Timer1 | Share driver; exclude conflicting timer clients |
| TM1637 | D11 DIO, D13 CLK | A4 DIO, A5 CLK | Separate compile-time board profiles; stock pins load ISP |
| I2C | A4/A5 available for Wire | A4/A5 occupied by TM1637 | Use the existing S3 LCD/I2C owner, or a separately rewired profile |
| D10 | 1-Wire sensor bus | Optional legacy busy input; unused by buffered policy | Cannot enable both owners on one profile |
| D11–D13 | TM1637 consumes two SPI lines | Dedicated AVR ISP | Keep Pakar's ISP clear and display on A4/A5 |
| D2/D3 | RF receiver/transmitter | Reserved aliases, no commissioned RF adapter | Module/pin/interrupt review before enabling |
| D6 | AVR addressable strip | Optional diagnostic alias; main matrix is on ESP GPIO4 | No duplicate full-table engine |
| ADC6 | Not a donor front-panel key | Ninth analog-only key | Sample injection and threshold belong to Pakar hardware profile |

The donor supports explicit Mega pins, but Pakar's current direct-port paths
still need narrower MCU guards before another AVR is claimed supported.
Both 328P images remain constrained by 32 KB flash, 2 KB SRAM and 1 KB EEPROM.
No current flash/stack-fit measurement was performed in this comparison.

### EEPROM cannot be shared blindly

| Region | PCController declarations | Pakar declarations |
|---|---|---|
| Settings | Declared reservation 32–72, plus associated name/layout policy | Dual banks 32–127 |
| RF / temperature | RF 80–323; temperature 324–334 | No fitted equivalents |
| Automations | Optional dual banks 720–1003 | Dual banks 128–411 |
| Reset journal | 336–719 | 416–799 |
| Status profiles | 720–966, alternative to automations | ESP status engine; no matching AVR allocation |
| Optional menu labels | Payload 967–1022, commit at 1023 | No matching allocation |
| Durable programming latch | Part of donor settings semantics | Independent record 1008–1012 |

Pakar automation writes would overwrite donor RF records. Donor optional labels
overlap Pakar's programming latch. A preserved EEPROM byte image is a rollback
artifact, not valid settings for the other firmware. Do not silently reset or
reinterpret it on migration.

## Future shared-core shape

Keep three independent decisions rather than one growing matrix of aliases:

1. **Hardware profile:** pins, MCU/clock, timers, EEPROM partition and installed
   peripherals. Distinguish PCController Mini, Pakar Mini and any future Mega.
2. **Business role:** exactly one table-selection or first-wins policy in a
   Pakar image; donor motion/door behavior must not leak into those roles.
3. **Feature budget / transport:** constrained or expanded modules, owner
   placement, UART/buffering and host/companion framing.

```text
One reusable controller core
  codec / sampled keys / tones / segment engine / macro ring / rule executor
       |                       |                         |
  PCController profile     Pakar Mini profile      native conformance HAL
  sensor/motion policy     table OR keys policy    same production core
                               |
                          S3 companion services
                         LEDs / network / updates
```

The existing `PcControllerHardware` and `ControllerEventSink` are useful seams,
not proof that a common core already exists. Prefer extracting agreed domain
code and tests into one pinned source owner consumed by both builds. The final
repository/package arrangement needs coordination; no dependency was added here.

Do not merge the whole donor sketch, fork a third dispatcher, reintroduce
version-specific adapters or `/api/vN`, or relax peer admission to make a stock
board appear supported. Hardware capability identity and additive unversioned
contracts must drive clients. Stable physical board kinds are not release numbers.

## Ordered acceptance checklist for the future merge

- [ ] Choose the shared source owner with PCController; pin exact revisions and
  retain donor bug-fix/upstream tracking rather than periodically copying trees.
- [ ] Extract/test the codec and synchronous frame lifetime; deliberately settle
  the reserved-byte rule and opcode boundary without public schema negotiation.
- [ ] Specify one timing-capable event/ACK representation and migrate all actual
  consumers together, preserving tone start/observation and request correlation.
- [ ] Share sampled keys, tones, segments and macro/rule algorithms while keeping
  hardware profiles, display pins and input meaning explicit.
- [ ] Add compile-time pin/timer/EEPROM overlap assertions and disabled-driver
  failures for each supported profile; never enable all flags on a 328P.
- [ ] Export semantic settings before migration; define a deliberate import or
  explicit factory initialization with preserved raw flash/EEPROM rollback.
- [ ] Complete correlated cross-owner macro actions to the ESP, including
  cancel/host-loss/late-reply fencing and applied-result reporting.
- [ ] Build each affected AVR/ESP profile with measured flash, static SRAM and
  stack headroom. Reuse PCController's same-production-source native fixtures.
- [ ] Verify capabilities, settings and presentation in Rust, Flutter and Web;
  unsupported native PCController board attachment must remain rejected meanwhile.
- [ ] With production approval, verify signature, clock/BOD, ISP, Urboot, EEPROM,
  UART, all keys/outputs, display, audio, interrupted maintenance and recovery.

None of these unchecked steps was completed by writing this report. No board
was contacted, flashed, reset, or actuated. No donor code was modified, no build
was run, and no GitHub issue/PR was changed in this comparison pass.
