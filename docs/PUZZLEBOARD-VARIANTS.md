# PuzzleBoard and 40LED source reconciliation

Tracking: https://github.com/atomicdeploy/PakarController/issues/2.
Exact individual hashes are in `reuse-review-evidence.csv`; this is a partial
integration, not the completed all-file donor audit.

## Hardware authority

The current board's user-confirmed mapping remains AVR A4/SDA -> TM1637 DIO,
A5/SCL -> TM1637 CLK, and D9/PB1/OC1A -> buzzer. `Puzzles/LocalLib/SevenSegments.h`
agrees. Root `PuzzleBoard.ino` is an older **direct shift-register multiplexed
display**, not a TM1637 sketch. Its PC6/PC7 and other mappings must not replace
the connected board's wiring. 40LED's LED A0 and buzzer D8 also do not apply.
The ESP owns the current 480-pixel strip on D2; no second AVR effects engine is
introduced from donor strip sketches.

## Imported actual musical variations

`firmware/avr/BootMelody.cpp` now contains the complete named fourteen-cue
catalog. Existing wire IDs 0..11 and the default welcome are unchanged.

| Cue | Actual source | Schedule |
|---|---|---|
| `welcome` | `ControllerBoardMini/Puzzles/Puzzles.ino` | 1032 Hz/70 ms; silence/60; 2010/70; silence/60; 2400/120; silence/150: **530 ms** |
| `welcome-puzzleboard` (12) | root `PuzzleBoard.ino::validBeep` | 1032/70; silence/70; 2010/70; silence/70; 2400/90; silence/150: **520 ms** |
| `welcome-40led` (13) | `XS/40LED.ino::setup` | same 520 ms sequence plus final silence/70: **590 ms** |

Both added variants use the bounded nonblocking Timer1 player. Blocking
`delay()` loops, watchdog changes and donor buzzer pins are not imported.
Production source tests check all fourteen complete cue durations, the actual
note boundaries, late service and rollover; these are register doubles,
**not** an audible recording or physical verification.

ESP status advertises `avr_version` and `audio_cues` only after a versioned AVR
HELLO. Added cues require AVR >=0.2.5, not merely new ESP firmware. Older or
offline/unknown peers do not gain inferred capabilities. Native, Web and Android
offer only known advertised entries. CLI/TUI, API, cloud and Telnet use the same
cue names; framed USB uses opcode 0x11 and one byte. A 202 audio response means
**queued**, not audible completion.

## Remaining useful reconciliation (not discarded as business)

- [ ] Root PuzzleBoard: 50 ms eight-key debounce, second input bank, virtual key
  mappings, active-low output and indicator behavior need capability/wiring
  reconciliation. Its direct-display perimeter uses twelve 100 ms steps;
  Puzzles' twelve 75 ms TM1637 startup steps are retained separately.
- [ ] Root active-buzzer `errorBeep` defaults to five timed toggles after initial
  silence. Choose a verified passive-buzzer adaptation rather than assuming it
  equals the existing pitched `error` cue. Its password/countdown game is excluded.
- [x] 40LED FINAL_TABLE vs RADIOGRAM was read through EOF. Generic fractional
  progress and monotonic animation intent map to wall-clock grouped-cell fades,
  a uniform indicator pulse and configurable neighbour-aware illumination chase.
  Mode-aware owners replace its manual/automatic output split. The fixed blue
  score gradient and symmetric warm bonus meter are presentations of excluded
  score/radiogram business, not missing reusable hardware primitives.
- [x] 40LED's old line commands use unchecked conversions and small buffers.
  Eligible command/status semantics use bounded COBS/CRC and validated JSON;
  unsafe parsing and unconditional serial forwarding were not copied.
- [x] `.ino.new` has complete semantic coverage through the fully read base plus
  its complete two-hunk diff, so no unchanged region or changed line is skipped.
  Its disabled 165 scan becomes an explicit hardware-profile decision rather
  than a shared-loop comment. Pakar preserves reset-to-zero events with a reset
  revision and makes reset audio idempotent instead of suppressing zero updates.
- [x] Every PuzzleBoard XS/tools/client/server file now has an individual
  hash-bound read, integration/exclusion decision and target evidence. Hardware
  acceptance remains open where the comparison rows say so.

The root multiplex loop's digit-index increment can address element four of a
four-element array. That bug is not imported. Commented confetti/sinelon/BPM/
juggle examples are excluded by the user's explicit requirement. Rainbow and
the current ESP effect engine still require live streaming/frame-rate testing.

## XS/7Seg_Remote.ino

The complete 4,317-byte source has a hash-bound manual review, not an automatic
TM1637 import. Its single 74HC595 digit uses data 7, MR 8, clock 9, latch 12
and OE 11. The digit bits rotate one position before output; zero is blank.
The startup self-test runs two 1..9 sweeps, followed by a 500 ms final pause.
RCSwitch listens on interrupt 0 despite an inconsistent INT1 comment. Eight
low-byte codes (40, 36, 44, 34, 42, 38, 46, 33) map to actions 1..8; action 8
toggles between zero and the previous action. The remote identifier is ignored.

- [ ] Decide whether an optional RF receiver is in the current hardware scope;
  none is peer-advertised. Do not blindly claim absent hardware or reuse its pins.
- [ ] Retain an optional non-blocking digit self-test if useful for TM1637;
  its old two-sweep startup is not the current welcome presentation.
- [ ] If RF is retained, preserve the full unsigned receive value, validate
  remote identity, debounce without the blocking 200 ms loop delay, and test it.

The RF comparison is now hash-bound as a scoped improvement. Physical RF
receive/transmit and fitted-front-panel verification remain open.
