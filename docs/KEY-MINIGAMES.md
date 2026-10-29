# Board-owned keycontroller games

Tracked in [issue #32](https://github.com/atomicdeploy/PakarController/issues/32).
The `first-wins` board role now contains a runtime catalog; table boards remain
separate. ESP owns the game, AVR supplies debounced input, finite tones, display
and outputs. Opening any client panel does not start or reset gameplay.

## Requested-feedback implementation audit (2026-10-29)

| Request | Actual implementation and focused evidence |
| --- | --- |
| Mode descriptions and icons | Flutter numbered catalog uses distinct icons and wrapping muted second-line descriptions. Rust uses the imported detail-choice control; Web uses the shared icon/description menu. Flutter widget and isolated Web DOM tests cover the real controls. |
| Illuminated response window | Board default is 10,000 ms. Typed settings permit 100–60,000 ms; core, persistent 20,000 ms readback and Flutter editing tests pass. Existing explicit saved values are not silently overwritten. |
| Color Pattern completion | Exact player/target palette comparison over participating boxes, with per-box names/hex and match bits. All 511 nonempty masks and palette sizes 2–9 pass. No independent comparator defect was reproduced; the reported physical mismatch still needs a fresh hardware test. |
| Color Pattern introduction | **Latest request supersedes default-on: now Off by default.** Explicit opt-in retains ordered fade plus color notes, capped at 500 ms per box. Persistence tests preserve both explicit Off and On. Updating this board's saved `intro_enabled` to false remains pending ASUS access; changing source defaults is not that instance write. |
| Matching Pairs introduction | Default On, configurable per-box fade and final preview hold, with tones. Core sparse-layout/reveal tests and Flutter switch/save tests pass. |
| Simon feedback | Each correct input enters Feedback and plays its color note before the configured feedback delay/advancement. Latest defaults are three rounds of **3 → 5 → 7** steps, 300 ms lit + 120 ms gap, and 240 ms correct-entry feedback. Core progression tests and Flutter editable-feedback/growth tests cover it. |
| Flutter WebSocket-first controls | Minigame box inputs use the existing fast correlated command path, WS first, configured same-request-ID HTTP recovery. Board/boot identity, exact receipts and settings readback remain required; obsolete intents are never replayed to another board. |
| Durable errors | Real board/transport rejection reasons remain visible; settings/hold catches no longer expose Dart's `FormatException:` prefix, and a failed save keeps its draft. Web's previously exposed boot/display/timer settings are now admitted by the shared typed command validator. No claim that all future network failures are eliminated. |
| Arpeggios and success melody | Enabled color notes use one finite AVR-local 55/55/100 ms major arpeggio; disabled uses a single note. The configured full win cue follows the completing note at 240 ms, beside the 250–2249 ms rainbow. Compiled production-dispatch tests cover both and repeated wrong-pair presentations. Press-to-win retains its separate dedicated synchronized winner cue. |

Silent settings remain authoritative. Arpeggios require the paired AVR capability;
an older AVR uses the bounded single-note fallback instead. These are source,
fixture and build claims, not an audible or optical acceptance report. The user
is using the tablet and asked to retry deployment later; do not interrupt that
session or actuate a game merely to complete this audit.

For the pending instance change, verify the exact keycontroller identity and
current mode first: apply only `{"intro_enabled":false}` while Color Pattern is
selected, with the normal correlated command and saved-value readback. Do not
disable an actively selected Matching Pairs introduction or switch a person's
game underneath them. This board stores its current mode's settings, not an
independent saved configuration for every mode.

### Simon: shorter escalating rounds

Fresh Simon settings now use `rounds=3`, `sequence_length=3`,
`sequence_growth=2`, `show_ms=300`, `gap_ms=120` and `feedback_ms=240`.
The same sequence prefix grows to five then seven steps; each correct response
still gets its feedback before the next input or round. Presentation is 420 ms
per step instead of the former 700 ms. The 210 ms arpeggio fits inside the lit
interval rather than being cut off by the next note.

`sequence_growth` is configurable from 0–31 in the board settings and all three
clients; zero keeps equal-length rounds. The effective sequence length caps at
32. Missing optional growth in an existing saved file retains its former +1
behavior in RAM, without rewriting explicit saved timing/rounds. Tests cover the
complete requested progression, fixed-length and capped sequences, note and
feedback timing, persistence and invalid settings. This is additive data, not
a new API generation or version-dependent adapter.

For this particular board, the requested saved Simon patch is:

```json
{"rounds":3,"sequence_length":3,"sequence_growth":2,"show_ms":300,"gap_ms":120}
```

Apply it with exact board/boot identity and readback after the supporting image
is installed and Simon is selected. It remains pending while ASUS is unreachable
and David is using the tablet; do not silently switch another running mode.

Final local candidates for this audit (not installed): ESP32-S3 application
2,424,240 bytes, SHA-256
`6679ecb3a65032ef4ce49274a4cb5fe2bcc2e0b560741dd1d1a3c557225b06cc`;
Android APK 64,419,389 bytes, SHA-256
`72a41b4db3ad3a514efaffd89a1084bb52e1d7a8902dd4326b9956da22d01c29`.
Canonical firmware and APK builds pass. Native's ten focused minigame tests
compile and pass; its new production executable has not been packaged or
installed in this audit. AVR source is unchanged by these latest Simon/client
changes; the previously built paired AVR candidate remains separately pending.

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
| mine-field | Toggle cyan/yellow until every enabled box has the same color | 60 s |
| toggle | Toggle, latch or momentary button state | None |

Press-to-win is the initial default. Configure/reset enters Ready. Puzzles require
Start except for the explicitly selected boot restoration below;
press-to-win/toggle may start on their first input. Pause preserves time.
Matching pairs needs two or more boxes: an odd enabled count sits the highest box
out without changing its saved enabled setting. Reaction times measure ESP receipt
time, not independently measured electrical latency.

### Mine Field

Select **Mine Field**, then **Start**. Both groups in each enabled box show the
same randomly chosen color: pulsing cyan (`#00FFFF`) or steady yellow
(`#FFFF00`). At least two boxes are required; the initial arrangement is always
mixed so the round cannot begin already solved. A press toggles only that box,
and either all-cyan or all-yellow wins. Disabled boxes stay out of the puzzle.

The existing `limit_ms` setting defaults to 60 seconds (zero disables the
deadline); `show_ms` is shown as **Pulse period** for this mode and defaults to
1000 ms, bounded to 100–10000 ms. Cyan's shared pulse varies between 255 and 150
gain, never Off. Deadline warning flashes affect the indicators; on timeout,
both groups of every participating box flash red. Success uses the shared
green/rainbow result and configured winner melody. `elapsed_ms` stops at the
winning press for shortest-time comparisons; no synthetic network roundtrip is
included in that board-measured elapsed time.

Mine Field uses the existing explicit configure/control/input commands across
USB, WebSocket, MQTT and HTTP. Example configure payload:

```json
{"command":"minigame-configure","settings":{"mode":"mine-field","limit_ms":60000,"show_ms":1000}}
```

Then send `minigame-control` with `action:"start"` and `minigame-input` with a
1-based `key` and `action:"tap"` using the normal peer-bound command envelope.
Reads/opening panels never start it. The `mine_field` status object reports
cyan/yellow masks, RGB values and pulse period beside the existing live frame,
phase, timer and output rules. `latched_mask` represents yellow boxes; output
rules for latched keys, round success or failure can use the same state.

Build inclusion is controlled by `PAKAR_GAME_MINE_FIELD` (default On) and the
canonical `-KeyMiniGames` list. Old numeric mode IDs are preserved; Mine Field
is appended internally and displayed before Toggle, which remains last.
Table-role builds omit it with the other key games. No existing instance is
switched to this mode or has its colors/timing migrated automatically.

Source verification: all-mask/dual-solution engine, press-only exclusion,
persistent settings, both baked board roles, production dispatch and isolated
Web controls pass. The first-wins S3 candidate also compiles with both LED
backends and the existing device profile: application file 2,425,200 bytes,
SHA-256 `32f1a454eced20fa1efaf9916e17a72ef843aab52f98206c2e84466a78476122`.
No device installation, mode selection or physical game acceptance is claimed.
The production-hold APK candidate is 64,419,469 bytes, SHA-256
`8c8e150a8903d494c6e0bf8e8fb1062f4896954b2333e69190c8206b8b09c771`.
Flutter's analyzer and 22 game tests, native's 13 focused compiled tests and the
isolated Web/Mine Field settings interaction pass. A new native production
executable was not packaged; no running client was replaced.

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

### Color Pattern pacing and matching diagnostics (2026-10-29)

Color Pattern's optional introduction is now disabled by default at the user's
latest request. When explicitly enabled, it reveals each enabled box in at most
500 ms. Fresh fade settings use 500 ms; an older saved `intro_ms` of 1000 or more remains stored
unchanged, but its effective reveal interval is capped at 500 ms. The final
color-pattern pause is also capped at 500 ms. Reveal deadlines are absolute:
after a delayed service tick the engine skips expired steps and plays only the
current color's note, rather than extending every box or replaying old notes.
Matching Pairs retains its separately configured reveal and memorization times.

The default seven-color palette is Red, Green, Blue, Yellow, Magenta, Orange
and White. Larger palettes add Cyan and Pink. The prior default contained Blue,
Cyan and blue-heavy Violet, which could appear too similar on the fitted LEDs.
Palette notes retain their index association; persisted instance settings,
enabled boxes, pixel corrections and normal game-state colors are not rewritten.

Matching is authoritative per enabled box: its player palette value must equal
its target palette value. Brightness, blinking, RGB channel order and downstream
pixel corrections do not decide success. A `matching_mask` of 62 with an enabled
mask of 63 means boxes 2–6 match while box 1 still differs; it does not mean the
engine is waiting for a disabled box. Tests have not reproduced an independent
matching-algorithm defect. They exercise every nonempty nine-box mask, palette
sizes 2–9, duplicate Down/release, sparse layouts and the 5/6 incident shape.
This is not an independent verification of the user's physical six-box result.

For Color Pattern, `status.minigame` additionally reports `intro_step_ms` and a
`color_match` array containing only participating boxes:

```json
{"key":1,"matched":false,"player_palette":3,"target_palette":1,"player_rgb":255,"target_rgb":16711680,"player_color":"blue","target_color":"red","player_hex":"#0000FF","target_hex":"#FF0000"}
```

Palette positions and box IDs are one-based. RGB/hex/name values describe the
game's intended colors before dimming, physical group mapping and corrections;
they are not sensor measurements. A wrong wiring/group/correction map can make
different intended values appear alike, so these fields support diagnosis
without silently accepting a wrong answer or clearing the user's rules.
Actual 6/6 button/LED acceptance remains pending a fresh human test after update.

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

### Tablet picker and delivery checkpoint (2026-10-29)

The SM-X906B tablet (explicit ADB serial `R52T60C6V9B`) now has the updated
Flutter APK, SHA-256
`a0844dd01cca98e909614182b2937f5b1f238cae357fb5c6dbb0454540df8798`.
The installed Android `base.apk` was independently hashed and matched. Actual
tablet screenshots and accessibility inspection show all ten board-advertised
modes numbered in catalog order, with distinct icons and muted wrapping
descriptions. The game card has one contextual Start/Pause/Resume control;
opening a menu or refreshing the phase never starts a game. Analyzer and all
14 focused minigame tests passed, including phase-by-phase command dispatch.

David-ASUS is running the new Rust executable at its existing deployment path,
SHA-256 `5f588663afff98834f497d02279cc400272becb07c35fa268009ddb96f271a02`,
PID 13112/session 2 at observation. Its previous executable and preferences were
preserved. SSH-session-zero IPC remains unavailable; process/hash evidence is
not a completed interactive native UI or board acceptance test.

The tablet's actual Flutter firmware page exported the running ESP application
through Android's document picker: 2,347,808 bytes, SHA-256
`fe15419230b3b013d86a1a6411fc5c480c384592d68eb697f95ea0c04408f4d1`,
MD5 `a444333384f3ecca8d2ac49d04db211d`, matching `/update/info`.
The file was copied off Android and retained as rollback. Earlier Node Range
backup attempts timed out; the owned 1,310,720-byte partial is retained, not
presented as the full backup. A read-only telemetry observation reported
81,728 free internal-heap bytes, largest block 34,804, historical minimum 364;
this does not establish the cause of the intermittent HTTP 503/timeouts.

The actual tablet document picker selected the locally supplied ESP application
`keycontroller-timer-esp.bin`, 2,422,912 bytes, SHA-256
`b73c65a0fb20b6b5a6347668e4525a76bb8faae269661db34f8c2e7b0fabb078`.
Flutter displayed measured application upload bytes and then observed
`Board OTA #7 · complete`. No laptop uploader was used. This application-only
candidate includes both LED backends, the previously acknowledged ISP
capability, Ethernet, 16 MB flash/OPI PSRAM and disabled BLE. The saved board
configuration is not intentionally rewritten.

The independent post-reboot full ESP readback subsequently matched the candidate
SHA-256 above; boot changed to `8186eee9741c5d41a34946f19b3b8a49`.
ASUS and Android Wi-Fi associations were refreshed without repeating the upload.
AVR deployment has not yet completed in this continuation.
The AVR candidate with Urboot is ready at SHA-256
`fa3d01739769b43401be7d11b23b8d3546fde120a0394d1a669bac390ec14782`.
Do not claim paired deployment, wireless ADB stability, peripheral effects,
sound or safe USB removal from this intermediate receipt.

### Feedback regression and tablet maintenance candidate (2026-10-29)

The updated tablet APK is installed and its Android `base.apk` hash matches
`ed7dba272ad67409479562db98897e5e4752810087ff706ee42fe48f7330c65e`.
The professional firmware progress card distinguishes sending from programming,
uses measured stage-local byte counts, and treats verification/reconnection as
indeterminate unless the board supplies real progress. Accessibility/layout,
delivery/lifecycle and backup-controller suites passed (47 tests total), with
clean analyzer results. Urboot backup exports now bind the exact completed
request and boot rather than substituting unrelated ISP files.

The consolidated ESP candidate includes the Press-to-win audio-owner fix and
Color Pattern palette/pacing changes described here: 2,424,000 bytes, SHA-256
`b5252660a9487bb209987bc0a9afa45d70c16b76c57f2ab3a92ed3ed63405206`,
MD5 `5e4fe168943a03f5501b75705f6db705`. Both GPIO/RMT drivers, Ethernet and the
existing verified ISP profile remain included; BLE stays disabled. Core,
mode-exclusion, settings-persistence and seven production-block integration
tests passed. This is a candidate, not a completed deployment or audible proof.

Wireless ADB at `192.168.4.2:5555` returned the actual SM-X906B, installed the APK
and verified its hash. USB was absent at that observation; sustained wireless
stability is not inferred. The tablet left the AVR maintenance dialog and
returned to the Game page before a maintenance job was created (`job_id=0`).
The game also changed through Ready/Success. No uncertain write was repeated;
coordination with the person using the tablet is pending before resuming its
firmware-uploader interaction. The attached phone was left untouched.

Subsequent fresh capture found the tablet on an unsecured swipe lock; the
canonical Android helper woke and dismissed it once. No PIN was entered. Its
Game-mode picker was open. A bounded return to maintenance then stopped when
ASUS SSH at localhost:8022 refused three consecutive connection attempts.
The consolidated ESP candidate remains staged on ASUS and in Android Downloads
as `keycontroller-color-melody-esp.bin`; it has **not** been applied. The AVR
job was still zero at the last successful read. Restore ASUS access to resume
actual tablet-led deployment; neither physical sound nor six-box matching is
accepted from these candidate tests.

### Saturated colors candidate (2026-10-29)

The older last-observed running ESP image still predates the vivid game palette;
do not mistake the pending source fix for an installed image. Default chromatic
game colors now have at least one zero channel (no gray floor), including the
optional Pink slot changed from `#FF4080` to `#FF0080`. Intentional White remains
white. No saved idle/win/lose colors, brightness, corrections or RGB channel
calibration are rewritten.

FastLED's installed convenience `fill_rainbow` requests saturation 240. Normal
and diagnostic rainbow now explicitly request `CHSV(hue,255,255)` while retaining
the original hue stepping, animation clock and downstream brightness owner.
Scalar pulse/ring/chase/breathe effects do not introduce absent RGB channels.
The configured transition from idle white may still blend white temporarily;
that intentional transition is not a reason to alter a user's saved policy.

Canonical minigame, baked-role and LED-pipeline tests pass. The role test drives
the real Color Pattern engine through all nine colors, checks published RGB
(not the restored working buffer), then the real S3 wire encoder over six
brightness levels. Rainbow tests observe actual producer HSV arguments; the
HSV test double is not real FastLED conversion or physical hue evidence.
Installation and visual feedback remain deferred while David uses the tablet.

The canonical S3 first-wins build passes with both LED backends, acknowledged
ISP capability, Ethernet, 16 MB flash/OPI PSRAM and BLE disabled. Application
file: 2,424,192 bytes, SHA-256
`bfc86d3e0408f58975186760b7a5e12e440d2782f09349fad0e25174c010449d`.
This identifies the local candidate, not the board's running image.
