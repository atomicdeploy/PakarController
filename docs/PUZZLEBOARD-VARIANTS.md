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
- [ ] 40LED FINAL_TABLE vs RADIOGRAM: generic fractional progress interpolation,
  blue hue oscillator, symmetric warm progress and manual output auto-override
  need adaptation. Old score/bonus/fan/relay game logic is excluded, not these
  potentially reusable primitives.
- [ ] 40LED's old line commands use unchecked conversions and small buffers.
  Keep bounded COBS/CRC and validated JSON commands; retain eligible command
  semantics without copying unsafe parsing or unconditional serial forwarding.
- [ ] `.ino.new` differs in two complete diff hunks: 165 input scan is disabled,
  input mapping becomes start/clear/door, zero-value notifications are suppressed,
  and beeping conditions differ. Complete separate manual reading/variant
  decisions and tests; do not silently treat it as byte-identical to the base.
- [ ] All remaining PuzzleBoard XS/tools/client/server files still require
  individual review, integration/exclusion decisions and evidence.

The root multiplex loop's digit-index increment can address element four of a
four-element array. That bug is not imported. Commented confetti/sinelon/BPM/
juggle examples are excluded by the user's explicit requirement. Rainbow and
the current ESP effect engine still require live streaming/frame-rate testing.
