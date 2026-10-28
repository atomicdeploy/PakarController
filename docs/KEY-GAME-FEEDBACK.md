# Keycontroller controls and audio repairs

Press to win and Toggle remain untimed by default (limit_ms=0). Neither client
shows an elapsed stopwatch for these modes. Other games retain separate saved
limits. Opening panels never starts or resets a game; applying rules does.

Rust now separates Game, Game settings, Keys and Melodies into persistent,
capability-filtered dock panels. Keys owns selection/arrangement. Normal drag
selects boxes; Ctrl/Shift adds to the base selection. Arrange changes dragging
to layout edits. Selection never sends game input. Hold/release is contextual.

## Autonomous AVR flash audio

The former ESP winner loop dispatched short kBuzzer commands which stopped the
previous note. Late servicing skipped or shortened notes. The replacement is
one finite AVR schedule through the existing kAudioStream command:

    [6,idLo,idHi,0,0,onLo,onHi,offLo,offHi,flashes,lastOnLo,lastOnHi]

The AVR validates the complete packet before replacing audio. Existing queue
storage holds the schedule: no added AVR instance RAM or audio-rate ISR.
Timer1 produces the hardware frequency; local absolute deadlines determine
notes/silence without subsequent network packets. Duplicate IDs cannot restart
the cue. Mute, explicit replacement and cancellation retain their semantics.
Playback reports ordinary stream state 2 with zero append credits; completion
reports state 3. Notes ascend C6/E6/G6/C7 using configured flash durations,
including the longer final flash.

The ESP advertises audio-flash-timeline only after the AVR HELLO acknowledges
it. It submits the schedule before network housekeeping. Other melody IDs
remain complete local AVR cues. The game_audio status object reports support,
pending, acknowledgement and error. An uncertain command is not replayed.

The separate DMA fix inherits the status pixel's existing normal RMT group
priority; the independently witnessed GDMA refill remains level 3/core 1.

## Verification and delivery

- 494 native tests passed before final contextual input-selector tightening;
  the release candidate compiles that change. Twelve box interaction tests
  include non-actuating marquee selection.
- Flutter analyzer and nine minigame tests passed. Tablet APK SHA256:
  c62bbc6e0296ff5c63ddf259de3bbcf94bb51a544dbea84a33a9df9e355fdbae.
- ASUS guarded replacement verified EXE SHA256:
  31245edc826eed1de6fd7d257c3fc0ef0bb12f5c6e7d41afc9d5b6121e10b08e.
  Actual Keys, Game, Settings and Melodies captures were inspected.
- GPIO/RMT resource and DMA guard host fixtures passed. Audio deadline, mute,
  cancellation, duplicate and all fifteen cue tests passed.
- AVR SHA256 3ceafd495530587c066a0bc46f7e9b6313bbcd47437bf7b908a19fbcf0c520db
  was programmed through Urboot, full flash verified, EEPROM preserved/verified
  and the application recovered. No fuse or bootloader write.
- One autonomous schedule received an actual AVR ACK; HTTP round trip 477 ms
  is not note-timing acceptance. Later telemetry showed uptime advancing,
  253 bytes free SRAM and zero UART errors/lost events. Audible feedback is
  required before the approved ESP update, following David's latest order.

Builds include pre-existing shared-checkout guarded shell/native-artwork work;
they are not clean-commit binary attestations. No ledcontroller update or scene
mutation is part of this delivery. Hardware/visual/audio acceptance stays open.

## Mode feedback follow-up (2026-10-28)

Mode choices in Flutter, Rust and the canonical Web app now include icons and
second-line descriptions. Unknown advertised modes retain a generic icon;
known modes do not require a particular firmware build identity.

The illuminated-target default response window is 10000 ms (`window_ms`),
adjustable from 100 to 60000 ms. Existing explicitly saved values are retained;
changing to this mode applies its defaults. Press-to-win remains untimed.

Color pattern and Matching pairs now default `intro_enabled=true`. Enabled
boxes reveal in physical box order with smooth fades and color notes, using
`intro_ms=1000` per box. Pairs then hold their preview for `preview_ms=1500`.
The reveal does not consume response time. Turning the introduction off enters
Input immediately. Color matching reports `matching_mask` and matched progress;
160 generated puzzles across two/four/six/nine and non-adjacent box layouts
complete when the displayed button/reference colors match.

Simon has a visible, sounded Feedback phase after every correct entry, using
`feedback_ms`, before accepting the next. Wrong pairs use a separate
`mismatch_ms=1200` (600..10000 ms), three visible blink periods, and the complete
incorrect cue. The game renderer's effect clock starts at the phase boundary,
not at an arbitrary point in uptime. Completing other puzzles plays the saved
winner cue once during the success rainbow; Silent remains respected.

`color_arpeggio=true` plays each color as a root/major-third/fifth arpeggio.
The AVR acknowledges support in HELLO audio-feature bit 1. Its existing
`kAudioStream` now accepts one atomic finite batch:

    [7,idLo,idHi,0,0,{HzLo,HzHi,msLo,msHi} x 1..10]

The whole batch is validated before replacing playback. It reuses the existing
circular queue and absolute local deadlines, adds no static AVR SRAM, and
rejects duplicate IDs without restarting audio. Reset/stop cancel puzzle cues
without cutting the separate first-wins reset melody.

Flutter box control remains WebSocket-first with same-request-ID HTTP recovery
when enabled. Receipt verification now checks board/boot identity separately
from admission for the next command, so an acknowledged AVR ownership fence
does not falsely revoke the connection. Known board rejections have actionable
messages; unsafe/unknown outcomes remain visible rather than being hidden or
replayed. Box input is disabled during Show/Feedback, not dispatched to fail.

Verification: 11 Flutter game tests, 6 actual loopback WebSocket/fallback tests,
6 focused native tests, core mode/exclusion/soak tests, canonical Web protocol
and mobile-width DOM tests, and AVR audio tests pass. The Web mode menu was
captured and visually inspected. New Android/native/AVR candidates build; the
RF-enabled buffered AVR uses 29828 bytes flash and 1654 bytes static SRAM.
New candidates are not installed: ASUS SSH8022 refused consecutive attempts.
The earlier autonomous-audio audible confirmation remains pending. No current
board scene, correction rule, enabled-box mask or saved mode was overwritten.
