# Board-owned keycontroller games

Tracked in [issue #32](https://github.com/atomicdeploy/PakarController/issues/32).
The `first-wins` board role now contains a runtime catalog; table boards remain
separate. ESP owns the game, AVR supplies debounced input, finite tones, display
and outputs. Opening any client panel does not start or reset gameplay.

| Mode | Rule | Default limit |
| --- | --- | --- |
| press-to-win | First enabled button wins; existing winner presentation | None |
| illuminated | Press the green target within its reaction window | 60 s |
| color-match | Tap to cycle button color toward indicator; hold 3 s backwards | 60 s |
| simon | Watch then repeat a growing light sequence | 60 s |
| memory-pairs | Reveal and match hidden color pairs | 60 s |
| clear-all | Turn every lit button off; measure elapsed time | None |
| color-order | Press colors in palette order; mistakes subtract time | 60 s |
| switch-pattern | Toggle to match cyan indicators; optional hard latch | 60 s |
| reaction-chase | Catch the moving target within its window | 60 s |
| toggle | Toggle, latch or momentary button state | None |

Press-to-win is the initial default. Configure/reset enters Ready. Puzzles require
Start; press-to-win/toggle may start on their first input. Pause preserves time.
Matching pairs needs two or more boxes: an odd enabled count sits the highest box
out without changing its saved enabled setting. Reaction times measure ESP receipt
time, not independently measured electrical latency.

## Transport and persistence

Use the existing `/api/command` correlated envelope over WebSocket, HTTP or USB,
with board ID, boot ID, request ID and board-clock expiry. MQTT uses the same game
handler through its existing authorization. Payload examples, omitting envelopes:

```json
{"command":"minigame-configure","settings":{"mode":"illuminated","rounds":5,"window_ms":1200,"limit_ms":60000}}
{"command":"minigame-control","action":"start"}
{"command":"minigame-input","key":2,"action":"tap"}
```

Controls: start/pause/resume/reset/stop. Inputs: tap/down/up, keys 1–9. Send Up on
release/cancel. Remote holds expire after 5 s; physical and remote levels merge.
AVR companion loss retires game outputs and requires renewed ownership. A lost
input owner pauses the ESP game, never automatically replaying or resuming it.
Reset, configure and explicit start establish an acknowledged AVR input epoch.
Old queued events and remote menu events cannot become physical game presses.
New epochs clear prior outputs; duplicate ownership grants do not clear them.
An explicit start may resume after this short barrier only within two seconds;
otherwise it remains paused for an explicit resume. The initial color note is
retained across that intentional barrier, not across arbitrary disconnects.

`status.minigame` reports catalog, readiness, mode/phase/revision, settings,
participating/held/latched masks, winner/target, rounds/progress/mistakes,
elapsed/remaining time, warning and reaction measurements. Clients verify settings
readback and preserve local drafts. Settings are validated then staged/read back/
renamed in `/minigame.json`; no frames or rounds write flash. A mode change resets
timing defaults while preserving explicit output wiring and enabled boxes.

Eight output rules select disabled/active/success/failure/held/latched/winner/lost.
Phase rules use key 0; button rules use 1–9. The game is the sole output owner;
manual output writes are rejected while it owns the AVR. Stop deasserts outputs.
These are electrical levels, not a safety-certified interlock.

TM1637 shows Ready, pause/result or MMSS countdown with warning/urgent blink.
Update progress retains priority. Shared effects provide stimulus, mismatch and
finite success rainbow; color-associated notes are finite and consumed once.

## Builds and acceptance

`tools/build-firmware.ps1 -Target esp32s3 -GameMode first-wins` includes all modes.
Use `-KeyMiniGames press-to-win,toggle` for a subset; at least one is required.
Application-only `PAKAR_GAME_*` flags do not change network/LED backends. Both AVR
and ESP must acknowledge ownership before clients enable the new controls.

After approved paired deployment, test each mode separately: Ready, start,
correct/wrong/held input, both LED groups, sound, display/countdown, reset, relay
mapping, disconnect, restart, all clients and nine simultaneous keys. Collect the
user's actual observations for each. Source tests/builds are not hardware proof.

### Candidate checkpoint (2026-10-28)

Both guarded target builds pass with all modes enabled. ESP32-S3 uses
2,221,155 / 3,342,336 bytes of its application slot. AVR uses 28,676 / 32,384
bytes of application flash and 1,654 / 2,048 bytes of static SRAM; its 394-byte
remaining static budget requires live stack/headroom observation before acceptance.
This is compile-time usage, not measured runtime heap or optical/audio proof.

| Candidate | SHA-256 |
| --- | --- |
| ESP32-S3 application | `70a9579fa18e2631f7040bac65beb1cc316da68b3d10d053037c190d8163a997` |
| AVR with Urboot | `6a339b07d249890ba90a8d519edf5f1e12a4e584ec5559ecaa899af31ed4c292` |
| Flutter APK | `4f772a3d31433a710a52c12cdf631f6419c8d299d56491552a7312bed88e91b6` |
| Rust Windows EXE | `883f25c7ca51d30cfa70867335c75e1925bc29bd50a54f834013e43facef5225` |

At that candidate checkpoint, none had been deployed. Preserve current settings,
backup and board identity; verify the connected hardware profile before flashing.
These local builds used the shared checkout with optional SSH disabled. The
minigame source commit excludes concurrent SSH work; rebuild the exact approved
deployment source/profile before claiming package-to-commit identity.

### Paired deployment (2026-10-28)

The inventoried keycontroller received the current first-wins build over local
Wi-Fi OTA. The initial updater readback timed out; no second flash was issued.
A separate complete running-image download verified SHA-256
`fe15419230b3b013d86a1a6411fc5c480c384592d68eb697f95ea0c04408f4d1`
(2,347,808 bytes). The previous image was downloaded and verified first. The
deployment retained its six enabled boxes, GPIO-static default, both compiled LED
backends, Ethernet support, disabled BLE and acknowledged AVR programming profile.
This profile-specific build includes the shared checkout's concurrent guarded
shell changes with SSH disabled; it is not an exact clean-commit attestation.

AVR image `6a339b07d249890ba90a8d519edf5f1e12a4e584ec5559ecaa899af31ed4c292`
completed the Urboot maintenance job with full flash and EEPROM readback,
`programmed`, `verified` and `application_recovered` all true. The job preserved
EEPROM and did not write fuses or bootloader bytes. Its pre-write backups remain
under the request-specific Urboot backup directory.

The APK and Windows executable from the candidate table above are installed on
the SM-X906B tablet and David-ASUS respectively. Rust replacement preserved the
previous executable and saved preferences. Both running apps were inspected via
screenshots: Flutter exposes Game mode and controls; Rust exposes the Keys game
controls with live WebSocket state. Wireless ADB to the tablet also responded.
The board advertised all ten modes, acknowledged AVR output ownership, and
reported Ready in Press to win with enabled mask 63 after the paired update.

David then began interactive testing and changed the game state. The unattended
smoke-test precondition correctly refused to overwrite that state; no test
commands were sent. Physical acceptance for each mode, audio/effect timing,
display, outputs and reconnect remains open. A native event also showed a failed
runtime LED-backend switch (error 258, GPIO-static retained); that is an open
observation, not a successful RMT switch. Do not reset or change the mode while
David is testing without coordinating first.
