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
