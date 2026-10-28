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
Start except for the explicitly selected boot restoration below;
press-to-win/toggle may start on their first input. Pause preserves time.
Matching pairs needs two or more boxes: an odd enabled count sits the highest box
out without changing its saved enabled setting. Reaction times measure ESP receipt
time, not independently measured electrical latency.

## Power-up and seven-segment feedback

Game settings in Flutter, Rust and Web expose these persistent options through
the existing `minigame-configure` command (also available over framed USB):

| Setting | Default | Behavior |
| --- | --- | --- |
| `boot_mode` | `first` | Select Press to win (or first compiled mode); `last` selects the last configured mode. Start a **fresh** puzzle only after AVR ownership is acknowledged. Never restore a winner, elapsed timer or held input. |
| `segment_animation` | `true` | PuzzleBoard's 12-step, 100 ms perimeter animation while Ready; notices, countdowns, menus, manual capture and maintenance retain priority. |
| `timer_beeps` | `true` | During active timed play: a 35 ms tick each elapsed second; an 80/60/80 ms caution double-beep at each elapsed minute. Untimed Press to win and Toggle remain quiet. |
| `timer_alternate_tones` | `true` | Odd seconds 1047 Hz, even seconds 880 Hz. Disabled uses 880 Hz every second. Minute caution uses 659 Hz. |

Boot selection is RAM-only; saved box enablement, palette, effects, wiring and
last-mode settings are not rewritten at boot. First-game selection retains
explicit settings if that mode was already selected, including an opted-in
timer. Later ownership loss pauses gameplay and requires explicit Resume;
it does not trigger boot restoration again. Settings edits do not auto-start.

Timer cues require the paired AVR's HELLO audio bit2 (`timer_beeps_supported`
in minigame status). Audio stream action8 has the same packed finite-note
payload as action7, but rejects with Busy while audio or a prefill owns the
player. A tick never interrupts a melody, overrides Silent, retries on rejection
or catches up after a stall/pause. The minute cue is buffered as one local
timeline, not timed using separate network requests. No extra AVR tone queue
or hardware pins are introduced; TM1637 remains A4/DIO and A5/CLK.

Source/fixture evidence is not an audible or optical test. This candidate still
needs paired ESP/AVR installation and human display/audio feedback.

The actual KeyMiniGameSettings parser/store also has a native fault fixture in
`test-key-minigames.ps1`: old files gain defaults without a write, bad mixed
patches leave every policy unchanged, save/readback preserves wiring, identical
saves do not write, and a failed rename retains the previous configuration.

### Ready source candidates (2026-10-29)

Source eaf1c3d plus settings fault fixtures 93a9a37, published in PR37. Local
ESP32-S3 and AVR builds pass; native game tests 6/6, Flutter analysis and game
tests 12/12, all-mode/exclusion core tests, actual settings persistence,
panel/audio fixtures and isolated Web tests pass. Full/onboard Web builds use
the same source and revision 4d58fc9df53cd9021f93a005bf1ae3d9df5a6b7a5fd9253a84d92e352d600dcb.

| Candidate | SHA-256 |
| --- | --- |
| ESP32-S3 application (2,407,568 bytes) | `88211d73b356444e6b2ae35f5b6a1768f5741c2c0bbfee14ca003aab989aeeab` |
| AVR + Urboot (32,768 bytes) | `fa3d01739769b43401be7d11b23b8d3546fde120a0394d1a669bac390ec14782` |
| Production-hold Android APK (64,385,613 bytes) | `ba7a160f9e57e82faf407e315505b8cd33534bfcecb2fa4a0a853716164c41ae` |

The S3 compile profile is first-wins, buffered UART, keycontroller hostname,
16 MB flash/OPI PSRAM, disabled BLE initialization, both LED drivers and optional
Ethernet. This source build does not assert electrical ISP verification or
overwrite saved per-board settings. AVR is first-wins/buffered with RF. Static
build usage: S3 2,407,415/3,342,336 bytes flash and 128,012 bytes static RAM;
AVR 29,862/32,384 bytes flash and 1,654/2,048 bytes SRAM (394 bytes headroom).
These are build figures, not runtime free-heap/stack or physical measurements.

No candidate was installed, and no board or production app was contacted.
Deployment must inventory the actual target/profile, preserve saved settings
and backups, obtain the applicable production approval, and install the paired
AVR update for idle-only timer audio. Optical/acoustic and boot/reconnect tests
remain open. GitHub's full CI/package jobs were still running at this receipt;
no CI completion or merge is inferred from local builds.

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
