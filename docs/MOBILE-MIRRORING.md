# Android peripheral mirroring

The Front panel page has independent switches for the actual front panel,
TM1637 display and phone-speaker buzzer. Preview switches do not capture keys
or mutate the board. Audio is opt-in and off by default. Changing volume stops
audio; enable it again explicitly. Backgrounding, audio-focus loss, disconnect,
status expiry, credential/endpoint changes, board reboot and AVR session changes
stop playback and discard pending notes. Returning to the app never replays them.

Front-panel mirroring shows the actual menu page/open state, run state and key
capture ownership, plus individually acknowledged input/output states. Display
mirroring renders the actual four segment bytes, including decimal points and
brightness; text is not guessed from the current cell number. Raw display events
update immediately, while the independent status lease remains authoritative.
Missing peripherals are not inferred from a table profile or pin count.

## Live, unversioned contract

Existing `/api/status` and `/ws` carry additive optional fields. `mirroring`
reports `avr_session`, `front_panel` and `buzzer_timestamps`. Front-panel state
exists only after the actual extended AVR game-state frame has been received.
Timestamp support comes from the AVR HELLO acknowledgment, not the ESP build.

Existing `pc-contract-event` messages now include `board_id`, `boot_id`,
`avr_session` and a monotonic `event_id`. Android accepts only the selected,
authenticated status owner. Events cannot establish identity or renew status
freshness. Duplicates/out-of-order IDs and wrong-owner events are dropped.

The existing `BuzzerChanged` payload retains Hz16, duration-ms16 and mute-byte
offsets, then appends little-endian musical-start-us32 and observed-us32 clocks.
Musical starts are derived from the actual TonePlayer deadline, not late UART
service. Pauses remain zero-Hz finite steps; zero-Hz/zero-duration is immediate
Stop and flushes pending playback. Notes are never reconstructed by polling.
Board mute remains independent of an explicitly enabled phone mirror, as in
PCController's host-speaker path.

`ToneTimeline` adapts actual PCController `buzzer_mirror.go`: 50ms jitter cushion,
wrap-safe subtraction, bounded timestamp skew, absolute deadlines and no expired
note replay. The additional observation clock accounts for AVR service delay.
Pending work is bounded to 32 notes and fails stopped on overflow.

Android's `ToneMirrorChannel.kt` synthesizes reported frequency using 48kHz
mono PCM through AudioTrack, not a fixed system beep or named melody. It uses
bounded 5ms buffers and a 3ms edge envelope. Finite monotonic note deadlines,
native audio-focus/lifecycle cleanup and a six-second receiver lease remain
effective even if Dart stalls. Session tokens fence late open/stop operations.
No recording permission, audio files, dependency cache or debug build is added.
API reference: [Android AudioTrack](https://developer.android.com/reference/android/media/AudioTrack).

Exact reported pitch is not a claim of identical piezo timbre, measured AVR
Timer1 quantization, zero Android output latency or physical synchronization.
Those require paired-device measurements.

## Evidence and remaining acceptance

- Hash-bound complete reads of current PCController buzzer scheduler and tests
  are in the existing file-comparison ledger; `mobile-mirroring` is a build gate.
- Silent Flutter tests exercise MCU rollover/cadence, expired notes, Stop flush,
  duplicates/wrong peers, AVR reconnect, lifecycle/expiry, independent preview
  controls and real loopback authenticated WebSocket event routing.
- C++ tests execute actual AVR TonePlayer and timestamp payload production code;
  hardware UART and A4/DIO, A5/CLK remain unchanged.
- The screenshot harness has FrontPanelTablet/FrontPanelPhone views with explicit
  simulated peers and an injected silent output. A screenshot is not a board test.

Pending: deploy the integrated ESP/AVR pair with backups, install on an authorized
phone, hear/measure all donor melodies and disconnect/focus/long-note cutoff,
measure segment/menu updates under LED load, integrate these fields in the S3
firmware and add equivalent native/Web audio consumers and optional bridge note
forwarding with loop prevention. No background Android service or multi-source
speaker mixing is advertised. Generic donor menus for absent motion, RF and
sensor hardware are not guessed into this board's front panel.
