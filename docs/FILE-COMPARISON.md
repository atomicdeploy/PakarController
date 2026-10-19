# PakarController donor comparison

Whole-repository superiority is **not established**. This report exposes retained code, scoped improvements, omissions and gaps; it does not turn missing reviews into exclusions.

Routing: 1515 paths have no specialist capability match. They remain visible through `consult-reuse.ps1 -Concern donor-audit` and issue #3; lack of a keyword match is NOT an exclusion. Every CSV row includes capability routing and owning issue links.

Inventory: 4697 files; 315 complete source-file comparisons; 4382 pending comparisons; 254 compared files with known parity gaps. Header/implementation pairs can describe the same gap. Missing sparse files remain pending.

Every inventoried path, source revision/hash, target/proof file and line, omitted code, disposition and concrete reason is in `file-comparison.csv`. Hash-bound manual decisions are in `file-comparison-evidence.json`. Donor SHA-256 values bind original bytes; target/proof SHA-256 values bind UTF-8 without BOM with LF newlines. Anchors are recomputed into exact line numbers. Test-code presence is not test execution; execution receipts are in `FILE-COMPARISON-RECEIPTS.md`.

## Coverage

| Donor | Inventoried | Compared | Pending | Files with gaps |
|---|---:|---:|---:|---:|
| ControllerBoardMini | 19 | 2 | 17 | 0 |
| ControllerBoardMini/Puzzles | 18 | 12 | 6 | 7 |
| esp32WS2811 | 24 | 16 | 8 | 9 |
| PCController | 1517 | 93 | 1424 | 82 |
| Pealayer | 639 | 54 | 585 | 41 |
| PU850-ESP | 109 | 109 | 0 | 96 |
| PuzzleBoard | 22 | 0 | 22 | 0 |
| RayanLamp | 2349 | 29 | 2320 | 19 |

## Exact reviewed-file mappings and omission decisions

### ControllerBoardMini: `Timer/LocalLib/Tasks.cpp`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `3f5ef23f7a8052d8086b2803343e3e22bb9751924b7760077950f4b0bd2cc196`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### ControllerBoardMini: `Timer/LocalLib/Tasks.h`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `5d838a7fb53c799160d55a65b22546ab2e749eb30f8412ed5d08e9ce44300fed`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### ControllerBoardMini/Puzzles: `LocalLib/Keys.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9ceb7e764ef9d4d5feb62b2c7a22eed59d614b7b56432fae87ce87b4aa434578`.

- Target: `firmware/avr/avr.ino:482 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:466 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=void boardKeyGesture(]`.
- Retained: Active-low physical plus virtual shift input state and candidate/stable transition tracking; AVR reports releases as dirty state; press handling is mode-specific.
- Retained: Actual PCController per-key callback/gesture classifier now feeds all nine inputs; no game action waits for click classification.
- Omitted/replaced code: `DEBOUNCE_MS = 50`. Disposition: **pending**. Reason: Actual PCController classifier now uses macro-configurable PAKAR_KEY_DEBOUNCE_MS (default 20ms) with tested Down/Up, click and hold timing. Physical switch-bounce measurements remain pending; faster is not certified electrically.
- Omitted/replaced code: `Key virtual readCurrentState/onStateChanged, setCallback, bool/getCurrentState interface`. Disposition: **replaced**. Reason: Imported PCController per-key classifier, setCallback and setEventCallback are retained. updateSample separates the input driver from gesture classification and supports all nine shift/ADC keys; subclass-specific raw reads are replaced by the actual board sample adapter.
- Omitted/replaced code: `Raw Puzzles lower-four input order`. Disposition: **pending**. Reason: Current PCController-based register reader reverses those bits. Physical key identity must decide the mapping; source reuse alone cannot.
- Remaining: Add input mapping/debounce configuration and bounce/press/release tests; preserve reusable callback capability or record a reviewed alternative contract.

### ControllerBoardMini/Puzzles: `LocalLib/Keys.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `caa21fe97e3df262f0821bd582b60f2895a14ec193d7d6fec13321afe4c5a46d`.

- Target: `firmware/avr/avr.ino:482 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:466 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=void boardKeyGesture(]`.
- Retained: Active-low physical plus virtual shift input state and candidate/stable transition tracking; AVR reports releases as dirty state; press handling is mode-specific.
- Retained: Actual PCController per-key callback/gesture classifier now feeds all nine inputs; no game action waits for click classification.
- Omitted/replaced code: `DEBOUNCE_MS = 50`. Disposition: **pending**. Reason: Actual PCController classifier now uses macro-configurable PAKAR_KEY_DEBOUNCE_MS (default 20ms) with tested Down/Up, click and hold timing. Physical switch-bounce measurements remain pending; faster is not certified electrically.
- Omitted/replaced code: `Key virtual readCurrentState/onStateChanged, setCallback, bool/getCurrentState interface`. Disposition: **replaced**. Reason: Imported PCController per-key classifier, setCallback and setEventCallback are retained. updateSample separates the input driver from gesture classification and supports all nine shift/ADC keys; subclass-specific raw reads are replaced by the actual board sample adapter.
- Omitted/replaced code: `Raw Puzzles lower-four input order`. Disposition: **pending**. Reason: Current PCController-based register reader reverses those bits. Physical key identity must decide the mapping; source reuse alone cannot.
- Remaining: Add input mapping/debounce configuration and bounce/press/release tests; preserve reusable callback capability or record a reviewed alternative contract.

### ControllerBoardMini/Puzzles: `LocalLib/pitches.h`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `0443f50675e84f826b7688dda6a1c822d666fe6413b055a8920cbacd83c12fc4`.

- Target: `firmware/avr/pitches.h:3 [UTF8-LF sha256=9ee408d89e25e5f99d0d56ff096b8ccb00f3737551599105f01ca8215018a23f; anchor=#define NOTE_B0]`.
- Retained: All donor NOTE_B0 through NOTE_DS8 and REST macro values retained; target adds only #pragma once.
- No production-code omission claimed for this reviewed file.
- Remaining: Other donor melody definitions still require individual inventory and integration; retaining pitches is not all-melody proof.

### ControllerBoardMini/Puzzles: `LocalLib/RotaryEncoder.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `86a2e78c3139b444db51157d2429968f3f33a7cd88c44658cda08a249351162d`.

- Target: `firmware/avr/BoardPins.h:34 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=constexpr uint8_t RcReceive]`.
- Retained: D2/D3 ownership is recorded in BoardPins, but no target rotary encoder implementation is claimed.
- Omitted/replaced code: `RotaryEncoder TWO03 object, setupRotaryEncoder and D3 CHANGE interrupt tick`. Disposition: **pending**. Reason: No encoder has been confirmed in the current hardware inventory. D2/D3 are also reserved for optional RC; silently enabling donor interrupts would create an ownership conflict. This is deferred, not approved permanent deletion.
- Omitted/replaced code: `Commented alternate RotaryEncoder constructor/attachInterrupt prototype`. Disposition: **not-applicable**. Reason: This block is commented out in the donor and never executes; active encoder behavior still remains an open hardware/feature decision.
- Remaining: Confirm fitted encoder/RC hardware and pin ownership; retain configurable encoder support where applicable.

### ControllerBoardMini/Puzzles: `LocalLib/RotaryEncoder.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `7e6eba6e24b29ac6b1d72a35f9153a5c722ccc3f3648c79b6ae4c0074191cdee`.

- Target: `firmware/avr/BoardPins.h:34 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=constexpr uint8_t RcReceive]`.
- Retained: D2/D3 ownership is recorded in BoardPins, but no target rotary encoder implementation is claimed.
- Omitted/replaced code: `RotaryEncoder TWO03 object, setupRotaryEncoder and D3 CHANGE interrupt tick`. Disposition: **pending**. Reason: No encoder has been confirmed in the current hardware inventory. D2/D3 are also reserved for optional RC; silently enabling donor interrupts would create an ownership conflict. This is deferred, not approved permanent deletion.
- Omitted/replaced code: `Commented alternate RotaryEncoder constructor/attachInterrupt prototype`. Disposition: **not-applicable**. Reason: This block is commented out in the donor and never executes; active encoder behavior still remains an open hardware/feature decision.
- Remaining: Confirm fitted encoder/RC hardware and pin ownership; retain configurable encoder support where applicable.

### ControllerBoardMini/Puzzles: `LocalLib/SevenSegments.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `e66094dc5cee382a71f87d043f7029eb359bc8fe9ff05f3a8f8d7715c3ed2dd2`.

- Target: `firmware/avr/BoardPins.h:33 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Clock = I2cScl]`.
- Target: `firmware/avr/SevenSegments.h:11 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void showSegments(]`.
- Retained: A4/SDA DIO and A5/SCL CLK pin ownership; segment-level output via current compact driver.
- Omitted/replaced code: `External TM1637Display wrapper and its complete external-library public API`. Disposition: **pending**. Reason: The compact driver replaces basic raw/text/number output but full external library parity, including leading-zero/dot/position APIs, has not been audited. Correct pins alone do not justify dropping those capabilities.
- Remaining: Review exact donor library revision/API and either add useful capabilities or justify each missing entry; close matching PCController recovery gap.

### ControllerBoardMini/Puzzles: `LocalLib/Tasks.cpp`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `3f5ef23f7a8052d8086b2803343e3e22bb9751924b7760077950f4b0bd2cc196`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### ControllerBoardMini/Puzzles: `LocalLib/Tasks.h`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `5d838a7fb53c799160d55a65b22546ab2e749eb30f8412ed5d08e9ce44300fed`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### ControllerBoardMini/Puzzles: `LocalLib/TonePlayer.cpp`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `97954eecd111d4c1bd092488ee395b7ec8eb4630ed507c9177af9fba6d434fc5`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=0de3422d83061153abb89170ce3ef94a7acb973cc46314d0c9054b92284f5e6d; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:39 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:50 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9f2aea4eadd0a4112ba62d4bdac906a9749026aa4e69404c2ff3169667f204df`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=0de3422d83061153abb89170ce3ef94a7acb973cc46314d0c9054b92284f5e6d; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:39 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:50 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `0f7fec7879abd26dbf2d56df28441727ea76fcb9e093f009d39f20cae77832bb`.

- Target: `firmware/esp8266/src/LedGrid.cpp:26 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:120 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: Final effects remain in the shared ESP engine: ESP8266 D2 or migrated ESP32-S3 GPIO4, with 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: S3 now defaults to BGR wire order per the explicit red/blue-swap request, with a build flag for RGB; actual encoder/worker fixtures cover both. Logical API colors remain RGB. Donor BRG is not assumed equivalent; other configurable orders and independent physical R/G/B tests remain pending.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `395096029df7c507782f40c5b32c8b1dffbf2db392cb26cd2b02c824d6a36949`.

- Target: `firmware/esp8266/src/LedGrid.cpp:26 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:120 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=uint8_t brightness_ = 255]`.
- Target: `firmware/esp32s3/src/LedOutput.cpp:144 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=config.flags.with_dma=true]`.
- Target: `firmware/esp32s3/src/LedWire.h:63 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=void encodeWire(]`.
- Target: `firmware/esp8266/src/LoopMessages.h:26 [UTF8-LF sha256=0539837793778de69839ea205fce43bda14f1734866dee5c158389f48e3837ab; anchor=class LoopMessages]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: Final effects remain in the shared ESP engine: ESP8266 D2 or migrated ESP32-S3 GPIO4, with 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: S3 now defaults to BGR wire order per the explicit red/blue-swap request, with a build flag for RGB; actual encoder/worker fixtures cover both. Logical API colors remain RGB. Donor BRG is not assumed equivalent; other configurable orders and independent physical R/G/B tests remain pending.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Test-code evidence: `tests/led_worker_test.cpp:1 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=Compile the ACTUAL S3 output worker]`.
- Test-code evidence: `tests/led_scheduler_test.cpp:7 [UTF8-LF sha256=e3967d3e77a7073dc002bf476f1b628e022ac6c32cab87387da9dfb529d29a07; anchor=int main()]`.
- Scoped advantage: Actual worker/mailbox tests establish source-level frame ownership and quiescence, not optical or SDK acceptance.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### esp32WS2811: `docs/WS2811__V1.4_EN_18112019092521.pdf`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `44ba93102860081868a5a547f0d6fc85e67b0807297caffbe6ab8b9595fc2bdd`.

- Target: `firmware/esp32s3/src/LedWire.h:29 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t zeroHigh=12]`.
- Target: `firmware/esp32s3/src/LedSignal.h:52 [UTF8-LF sha256=e48205a8a5f00fdc78cc02748ae8d1297c8f22621d2f1152da206f723244f6e3; anchor=LedSignalReport inspectLedSignal(]`.
- Target: `firmware/esp32s3/src/LedSignalCapture.h:17 [UTF8-LF sha256=377642675c28d6cf624a3e1c5690c5dff90bf46799fce628aa6eadb2217db86c; anchor=class LedSignalCapture]`.
- Retained: All eight pages read as text and rendered/visually inspected, including the electrical/timing tables, cascade/bit-order diagrams, 5V/12V circuits, package tables and revision history. Page3 timing drives the 40MHz RMT profile: zero300/950ns, one625/625ns, period1250ns; retain300us reset and the user's explicit R/B swap. Do not copy a divider from the older donor driver.
- Retained: Added optional whole-frame RX-DMA digital-pad capture on existing GPIO4. Compare all11520 bits to this immutable TX frame using an8128B partial ring and stateful comparison with no ISR heap/copy; reject the contiguous46112B design against the actual31732B largest free block. Report first mismatch/short frame/timing errors. Allocate only on explicit request and release after bounded completion; capture errors remain Unknown, and failed RX quiesce prevents buffer reuse/flash. RX uses the ordinary SDK GDMA allocator rather than the TX-only witnessed priority3 override (intr_priority is unused for DMA RX); observation can itself overrun, so a mismatch alone is not proof of transmitter fault. No additional wire, effect, matrix mapping, automatic capture cadence or second renderer.
- Omitted/replaced code: `Mechanical SOP8/MSOP8 footprint dimensions and complete application circuit drawings`. Disposition: **not-applicable**. Reason: Reference documentation was fully reviewed, but this firmware/client repository does not design or manufacture the existing strip's PCB/package footprint. Original donor PDF is preserved; do not invent a board-identification or power-circuit configuration API from those dimensions.
- Omitted/replaced code: `Electrical input threshold, signal conditioning, downstream power and actual controller/LED grouping verification`. Disposition: **pending**. Reason: The datasheet specifies VIH>=0.7VDD and shows both one-LED and three-series-LED outputs. Digital GPIO loopback cannot validate voltage at strip DIN, cable/reflection margin, downstream reset/reshaping, exact chip revision or physical address count. User optical first23-cell failure remains authoritative; require deployed pad captures plus physical/analogue measurements as needed.
- Test-code evidence: `tests/idf_rmt_ring_test.cpp:210 [UTF8-LF sha256=67b7f1107160e49db96f06907edb16dc540095bf4aaf6555041d49f47362cf00; anchor=void signalCaptureChecks()]`.
- Test-code evidence: `tests/led_worker_test.cpp:359 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=Fixture::rxCorruptBit=23*6*24]`.
- Remaining: Deploy the version-consistent candidate, capture full-frame output under real Wi-Fi load, verify all480 addressed pixels and obtain user optical feedback. Timing margin adjustment is a candidate fix, not a proven root cause or physical completion.

### esp32WS2811: `LICENSE`

Verdict: **equivalent-file**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `66b37e608859c67bf93a0821f4636c4289a2cd64f5c0858db0b8e02f7a5d1284`.

- Target: `docs/licenses/esp32WS2811-MIT.txt:3 [UTF8-LF sha256=66b37e608859c67bf93a0821f4636c4289a2cd64f5c0858db0b8e02f7a5d1284; anchor=Copyright (c) 2017 Bert Melis]`.
- Retained: Actual MIT license notice copied, including Bert Melis copyright and complete permission/warranty text. No legacy RMT function body is copied; separate Neil Kolban/Matthias Folte attribution is recorded in the comparison.
- No production-code omission claimed for this reviewed file.

### esp32WS2811: `src/Effects/Aurora.cpp`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `7f169ec9c5b57f858d9d79c5059e97b3be4130930f52257691c7940f39a49dea`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Aurora visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Aurora painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful six weighted moving waves remain unported. Original allocates Colour per wave/pixel, frees scalar new Colour with delete[], repeatedly replaces heap waves and delays20ms. Port into bounded loop-owned wave state and shared effect capabilities/API/USB/persistence/all-client selectors; no allocator bug, heap churn or hidden50fps cap.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Aurora with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Aurora.h`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `d175558be657cb27501c5687ad56b5f9f37dd6cb7490ba0de42298a628fb7aff`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Aurora visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Aurora painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful six weighted moving waves remain unported. Original allocates Colour per wave/pixel, frees scalar new Colour with delete[], repeatedly replaces heap waves and delays20ms. Port into bounded loop-owned wave state and shared effect capabilities/API/USB/persistence/all-client selectors; no allocator bug, heap churn or hidden50fps cap.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Aurora with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Autumn.cpp`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `45e281ccc376bc13033723bc0b5258c399bd92043ae68db4ce376103ebe83876`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Autumn visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Autumn painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful random-origin palette-spread interpolation remains unported. Original origin can be read before first initialization, divides by steps/numLeds/2 without positive bounds and advances by loop count. Port validated dimensions/positive durations and elapsed-time interpolation into the existing renderer/catalog.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Autumn with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Autumn.h`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `16d75c16e482c44b52feae753ec1ccdf3ab35fc50707e7f6a713462059ac544f`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Autumn visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Autumn painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful random-origin palette-spread interpolation remains unported. Original origin can be read before first initialization, divides by steps/numLeds/2 without positive bounds and advances by loop count. Port validated dimensions/positive durations and elapsed-time interpolation into the existing renderer/catalog.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Autumn with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Circus.cpp`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `039da26c04c5a6cf2438d42372f1f9d9d03748db1aee5b21fc8a0dedd0085c5b`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Circus visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Circus painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful random twelve-colour repaint remains unported. Adapt requested repaint interval as a monotonic deadline, not delay(interval) or another effect task, and expose through the shared acknowledged effect catalog; not a new default or automatic flashing diagnostic.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Circus with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Circus.h`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `50e04e72e5fac9ab5130d83178a9962397f8634418dc6d8a25dee2327ba6e454`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful Circus visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `Circus painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful random twelve-colour repaint remains unported. Adapt requested repaint interval as a monotonic deadline, not delay(interval) or another effect task, and expose through the shared acknowledged effect catalog; not a new default or automatic flashing diagnostic.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement Circus with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/Colour.cpp`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `6bf0bac344bd60971564e66252e5d23b964606c6b4ec67c77e68d1186a313f58`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete RGB/HSV implementation/header and palette read through EOF. Canonical RGB/FastLED rendering retains byte colours, saturated arithmetic and HSV rainbow without a second colour engine. No old class-level API or exact palette parity is falsely promised.
- Omitted/replaced code: `colour legacy implementation/classes`. Disposition: **replaced**. Reason: Duplicate Colour/ColourHSV object and degree/percent float API, basic-colour wrapper names and custom saturated addition are replaced by existing FastLED CRGB/HSV/rainbow/qadd8 and validated canonical RGB bytes. Do not claim donor class/API drop-in or copy a second colour representation. Exact hue/palette aesthetics belong with the separately pending optional effects.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### esp32WS2811: `src/Effects/Colour.h`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `f77e8491309317965731d826b1d20033185807f0f411f888bbb38958e1ad59b3`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete RGB/HSV implementation/header and palette read through EOF. Canonical RGB/FastLED rendering retains byte colours, saturated arithmetic and HSV rainbow without a second colour engine. No old class-level API or exact palette parity is falsely promised.
- Omitted/replaced code: `colour legacy implementation/classes`. Disposition: **replaced**. Reason: Duplicate Colour/ColourHSV object and degree/percent float API, basic-colour wrapper names and custom saturated addition are replaced by existing FastLED CRGB/HSV/rainbow/qadd8 and validated canonical RGB bytes. Do not claim donor class/API drop-in or copy a second colour representation. Exact hue/palette aesthetics belong with the separately pending optional effects.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### esp32WS2811: `src/Effects/Effect.cpp`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `2c27fa91967898ba4f4234c53889adfea11f3bd14a62147d64fdcb01837e6c2d`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete base-class/task source read through EOF. Existing pattern start/replace/stop remains one loop renderer, no per-effect task or blocking sleeps; waveform driver remains independently owned.
- Omitted/replaced code: `lifecycle legacy implementation/classes`. Disposition: **replaced**. Reason: Per-effect FreeRTOS task creation/deletion, repeated cleanup of a non-cleared task handle and endless effect loops are replaced by existing loop-owned setPattern/stopPattern/service. Preserve single renderer and explicit frame publication without sleeping or a second engine; this is not an ABI-compatible installation of the old library.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### esp32WS2811: `src/Effects/Effect.h`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `d8b948d7d5ddef65ee40f162b5cd1f52e7aff7a525c94dfa11f1846b053aed71`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete base-class/task source read through EOF. Existing pattern start/replace/stop remains one loop renderer, no per-effect task or blocking sleeps; waveform driver remains independently owned.
- Omitted/replaced code: `lifecycle legacy implementation/classes`. Disposition: **replaced**. Reason: Per-effect FreeRTOS task creation/deletion, repeated cleanup of a non-cleared task handle and endless effect loops are replaced by existing loop-owned setPattern/stopPattern/service. Preserve single renderer and explicit frame publication without sleeping or a second engine; this is not an ABI-compatible installation of the old library.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### esp32WS2811: `src/Effects/SnowSparkle.cpp`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `c70e8d8c7a43b87c4932dd57adb952278670df136f6a7a5349e7df1586dc8ed7`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful SnowSparkle visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `SnowSparkle painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful background/white sparkle/fade remains unported. Replace dynamic Sparkle pointer array and per-sparkle new/delete with fixed slots, wall-clock fades and shared acknowledged effect catalog, not delay(10) or another task.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement SnowSparkle with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/Effects/SnowSparkle.h`

Verdict: **parity-gap**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `a86ecaaf9b2e9a41fdb53aa047883ed1f72acabc9909c164aab10bade3aab4ce`.

- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Target: `firmware/esp8266/src/LedGrid.h:31 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=enum Pattern : uint8_t]`.
- Retained: Complete source/header read through EOF; useful SnowSparkle visual algorithm retained as an explicit unimplemented parity gap, NOT claimed imported or advertised by a board.
- Omitted/replaced code: `SnowSparkle painting algorithm and all-surface capability exposure`. Disposition: **pending**. Reason: Useful background/white sparkle/fade remains unported. Replace dynamic Sparkle pointer array and per-sparkle new/delete with fixed slots, wall-clock fades and shared acknowledged effect catalog, not delay(10) or another task.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:35 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=for(const effect of ['Aurora','Autumn','Circus','SnowSparkle'])]`.
- Remaining: Implement SnowSparkle with bounded nonblocking state in the one renderer and shared discovery/API/USB/persistence/all-client catalog; verify source, SDK and physical behavior separately.

### esp32WS2811: `src/esp32WS2811.cpp`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `79fd1b8141cc8eb1473dcb1c25f754b0fbe71cab76af7a311fd10db0269e6511`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete original driver/API read through EOF. Existing stage/show/get-published-frame, full480 RGB, dedicated worker, coalescing and immutable TX ownership mapped function by function. Newly adapt explicit idle-LOW policy to both backends: reset, preload LOW, enable output, then allocate/init; each failed GPIO step stops before RMT/heap/TX with no false ready/black receipt.
- Omitted/replaced code: `driver legacy implementation/classes`. Disposition: **replaced**. Reason: Legacy driver/rmt.h configuration, original-ESP32 channel-memory topology, task-static 24-symbol-per-pixel expanded buffer, hardcoded GRB and ESP_ERROR_CHECK bodies are replaced, not installed alongside IDF5. Existing compact internal1440-byte bytes-encoder/SDK ring, scoped IRQ ownership, full-frame mailbox and recoverable failures retain the useful transport discipline. The donor's extra terminator is allocated but excluded from its write count; keep our explicit300us low latch instead.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### esp32WS2811: `src/esp32WS2811.h`

Verdict: **scoped-improvement**. Revision: `0222ef025f2b8e03f8c8aef35d63c545a98ad579`. Original-byte SHA-256: `f74e535287f38aee87bb9108fb85ebabb99f2421cd2828ecaef757b52ebb119e`.

- Target: `firmware/esp32s3/src/LedOutput.cpp:86 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=void LedOutput::work(){]`.
- Target: `firmware/esp32s3/src/LedOutput.h:58 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=Frame producer_{};]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:386 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=void LedGrid::showStaged() {]`.
- Target: `firmware/esp32s3/src/LedWire.h:32 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t resetMicros=300;]`.
- Target: `docs/ESP32-WS2811-REUSE.md:1 [UTF8-LF sha256=56bf08b491e47cd61e040229ff2aa8a6b2a88b75d1f2636a49f0b91af15455c1; anchor=# esp32WS2811 source comparison]`.
- Retained: Complete original driver/API read through EOF. Existing stage/show/get-published-frame, full480 RGB, dedicated worker, coalescing and immutable TX ownership mapped function by function. Newly adapt explicit idle-LOW policy to both backends: reset, preload LOW, enable output, then allocate/init; each failed GPIO step stops before RMT/heap/TX with no false ready/black receipt.
- Omitted/replaced code: `driver legacy implementation/classes`. Disposition: **replaced**. Reason: Legacy driver/rmt.h configuration, original-ESP32 channel-memory topology, task-static 24-symbol-per-pixel expanded buffer, hardcoded GRB and ESP_ERROR_CHECK bodies are replaced, not installed alongside IDF5. Existing compact internal1440-byte bytes-encoder/SDK ring, scoped IRQ ownership, full-frame mailbox and recoverable failures retain the useful transport discipline. The donor's extra terminator is allocated but excluded from its write count; keep our explicit300us low latch instead.
- Test-code evidence: `tests/led_worker_test.cpp:216 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void pinSetupFailure(unsigned step)]`.
- Test-code evidence: `tests/esp32_ws2811_reuse_test.mjs:8 [UTF8-LF sha256=7ffe888d46cab74523687561e3ad104e21e400960615a358e3338cd3db149c5b; anchor=function driverContract(source)]`.
- Scoped advantage: Retain modern IDF5 compact immutable DMA source and explicit failures/LOW/latch, one renderer and bounded memory instead of legacy driver globals, per-effect tasks, blocking delays or duplicate colour ownership.
- Remaining: Current480-pixel waveform/RF/refill/optical acceptance remains open; source hashes/fixtures are not current hardware proof.

### PCController: `docs/UPDATE-PROGRESS.md`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `20f907ac3426d5a9b40501ba2241a4308d0220204b8888576f1f995a3e401332`.

- Target: `firmware/esp8266/src/NtfyService.h:11 [UTF8-LF sha256=bb48440e92ad82b77cda39c0f9d22847e24ec00b55b175bee8455c28e38fb248; anchor=class NtfyService]`.
- Target: `firmware/esp32s3/src/SharedNtfyService.cpp:2 [UTF8-LF sha256=9891b2416284488b86c1aa00141b69727ad6699268c069902c504d4e3d1576be; anchor=#include <firmware/esp8266/src/NtfyService.cpp>]`.
- Target: `firmware/esp8266/src/NtfyNotice.h:7 [UTF8-LF sha256=a58636a85b08b0837302455dbe7e38ef0dc4f2bfbbb288db3e7c326f2828007a; anchor=class NtfyNotice]`.
- Target: `firmware/esp8266/src/NtfyService.cpp:73 [UTF8-LF sha256=d34bb2c00485dab92d5b3d6e5b883c8e278c710608dd67855b7735702f604258; anchor=void NtfyService::worker]`.
- Target: `firmware/esp8266/src/UpdateProgress.h:33 [UTF8-LF sha256=cbf19051f9f71c1b5ff99cd024faa1a87e12a211bc6001a413cdfde6a84e8640; anchor=class Progress]`.
- Target: `firmware/esp8266/esp8266.ino:2198 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=String localUpdateJson() {]`.
- Target: `firmware/esp8266/src/RemoteFirmware.h:67 [UTF8-LF sha256=9c959a74a7d59909f38c48844e191a089cf1849663e78caa4a3fafcaff6b2251; anchor=PakarUpdate::Observation progress() const]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/update-firmware.ps1:5 [UTF8-LF sha256=76718dc025e115ce85ef86ae1014e875b9fcbc7c629276ff07a893868f75a56f; anchor=[ValidateSet('auto','local','remote')]]`.
- Target: `firmware/esp8266/src/UpgradeLifecycle.h:14 [UTF8-LF sha256=cf5fd9189df181d4f23e3a80b4e219012e5aa589bd2ced02ec879e78b57908bf; anchor=class Lifecycle]`.
- Target: `firmware/esp8266/src/LocalOtaReceipt.h:10 [UTF8-LF sha256=1611a77b4ea63137ab901eae30b0354b903bd2d8520d51b70a54e8680ccac445; anchor=class LocalOtaReceipt]`.
- Target: `firmware/esp8266/src/AvrIsp.h:63 [UTF8-LF sha256=d817ff9373fd1dc6a0612d2ce9e5a5f93cdc974e2c95ecb32767eb8404fa8da4; anchor=void report(]`.
- Target: `mobile/lib/upgrade_observation.dart:3 [UTF8-LF sha256=8feabc680770f062a09ec3a5e7e72d1e9aad814335f630ddc00be3770790e8d7; anchor=class UpgradeObservation]`.
- Target: `desktop/src/firmware_progress.rs:11 [UTF8-LF sha256=a35d8ed50dbd2b657138e4a0eca981ad9098e604a36857282b87568f9a781996; anchor=pub struct Upgrade]`.
- Target: `firmware/esp8266/src/WebUi.h:889 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function upgradeObservation(]`.
- Retained: Complete canonical donor documentation read through EOF and original-byte SHA matched the inventory. Adapted the actual measured-stage/unknown-denominator/verified-completion/no-write-replay contract, not the donor's AVRDUDE parser: local HTTP raw file bytes, remote download stage bytes, coherent async-to-loop snapshots, finite AVR scheduled text leases and separate client reboot/readback verification.
- Retained: Reread the complete canonical update-progress document for actual0.2.8 migration. Explicit exact-board maintenance admits only the older producer's update/backup observations, never ordinary controls; the new boot still passes the default release policy. Missing BOTH newer producer headers is confined to that selected recovery and retains same-address, whole MD5 and before/after board/boot checks. Failed manifest admission can preserve zero image metadata only with the exact pinned manifest/job/boot/target/zero bytes. Added backpressure-aware multipart chunks and explicit upload pacing; no automatic mutation retry. Real fixtures cover these boundaries. Actual HTTPS stage22/new boot/full running SHA and separate Internet control/WSS passed; final direct LAN WS interruption and the false completion_verified bit remain open, not fabricated success.
- Retained: Local /update/info now uses boot-captured immutable image metadata rather than whole-image hashing on every progress poll. Status and WebSocket status/ping remain readable during local OTA; mutating commands remain inhibited. No authentication is enabled by the updater; it requires explicit auth-off readback before and after.
- Retained: Automatic network tooling prefers verified local Async HTTP POST, otherwise advertised pinned Internet stage/separate apply before any write. Full new-boot running-image SHA256, same-peer WebSocket, auth-off and selected/running VALID slot are checked. No automatic fallback/replay after uncertain writes, no random IP/factory credentials or duplicate image files.
- Retained: Loop-owned upgrade phases/byte counters are coalesced without duplicate notices. Receiving100% is not completion: local OTA commits a readback/rename checksum receipt, then another boot verifies image size/MD5/integrity before write-ahead once-only terminal notice. ISP backup/program/readback advance counters only after actual work. Silent=false was persisted and read back on the live AVR without test playback.
- Retained: Native, Flutter and Web accept peer/current-boot scoped incremental progress, deduplicate revisions and retain newer events across older full snapshots without renewing command-state freshness or resetting panels/selections. Exact byte ratios drive bars; stage percent remains floor(done*100/total), with unknown and unverified terminal values explicit.
- Retained: Reuse the same finite measured-stage/verified-result lifecycle for opt-in S3 ntfy HTTPS publishing. One volatile latest notice, bounded frequency/expiry, worker-owned I/O outside flash callbacks, no automatic POST retry, and no incoming command/flash authority. Web/native/mobile board firmware settings share typed cloud ACK/readback and write-only tokens; existing credentials migrate transactionally. Source/host tests and SDK builds are not actual ntfy delivery or phone/OTA coexistence acceptance.
- Omitted/replaced code: `AVRDUDE unbuffered stderr/bar parser and Windows taskbar/toast lifecycle`. Disposition: **pending**. Reason: This slice implements the ESP's integrated HTTP/Internet updater and finite board display producer, not the host AVRDUDE executor. The actual native/mobile taskbar/toast and durable progress/outcome consumers remain useful work; no all-client parity or parser import is claimed.
- Omitted/replaced code: `Progress on the AVR display while that AVR is held in ISP RESET`. Disposition: **not-applicable**. Reason: The AVR cannot execute TM1637 updates while held in RESET. Preserve Prog priority and show that stage through ESP LCD/network clients instead; do not add bus contention or release RESET to simulate progress.
- Omitted/replaced code: `Automatic USB flashing fallback`. Disposition: **pending**. Reason: A missing/uncertain LAN reply must not select a guessed COM port, chip, flash profile or erase/write operation. Existing inventoried USB tooling remains separate; a future automatic path must identify the physical chip and preserve rollback/settings before selecting a write.
- Test-code evidence: `mobile/test/ntfy_settings_test.dart:12 [UTF8-LF sha256=1904fef52317a35ffd7f8e455af0491bb4e2410a763619a1544842227412bb7f; anchor=void main()]`.
- Test-code evidence: `tests/update_progress_test.cpp:25 [UTF8-LF sha256=7e5badfe3e170d3070d7612509f6429cdfc4f69e4e279921c946aeb77919dbab; anchor=Concurrent reader never observes]`.
- Test-code evidence: `tests/front_panel_test.cpp:43 [UTF8-LF sha256=318ba761cdc2548b25e45b9114480ff761b6e994f21e0ca3a15ce19c20e2533a; anchor=The ESP's actual progress encoder]`.
- Test-code evidence: `tests/update_tool_test.mjs:31 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=for(const scenario of]`.
- Test-code evidence: `tools/test-update.ps1:15 [UTF8-LF sha256=a666f20d3c0d9cd2b7bef2a2c0d31adc25c6dd29255f28c582cbe7a31aa8a8d9; anchor=$taskPanelOutput=]`.
- Test-code evidence: `tests/upgrade_lifecycle_test.cpp:13 [UTF8-LF sha256=05666a12671410cd91962727be01c78f9c1d6ef7efe0bddd9c20b607e25e40df; anchor=int main()]`.
- Test-code evidence: `tests/local_ota_receipt_test.cpp:41 [UTF8-LF sha256=47927d52af8bd7eac6b103976e7f299b19acb068954cc8939e415cc8ee7c2904; anchor=int main()]`.
- Test-code evidence: `mobile/test/upgrade_progress_test.dart:31 [UTF8-LF sha256=147873b173659e5f6ba4373ce143774778f6eee3132a78e4580687764c735ba5; anchor=void main()]`.
- Test-code evidence: `mobile/test/upgrade_transport_test.dart:13 [UTF8-LF sha256=7e57323c4456d86d22e349a66826862699ee1eb192e65e5dbf14b2aba37243fc; anchor=void main()]`.
- Test-code evidence: `tests/upgrade_web_test.mjs:10 [UTF8-LF sha256=fb6ee4f1497ef9c1f7e0a54780ed047744d4b2bd7635a096f5f290bf48c8b5b8; anchor=const observe=]`.
- Test-code evidence: `tests/update_recovery_test.mjs:1 [UTF8-LF sha256=4ae889ba4d73c63aaa378cbd5769dd0118ca064aed44b56928ae4a074da37a74; anchor=Explicit maintenance bootstrap]`.
- Test-code evidence: `tools/test-protocol.sh:76 [UTF8-LF sha256=f28c11e234eda8b71ec4dfea0362eb4d02651fabf4323738203c4451e9afed41; anchor=for upgrade_case in]`.
- Remaining: Read and validate remaining donor update event/UI/native executor implementations. Host/loopback fixtures and an SDK build do not establish flash, interrupted-power, network contention, display ACK/presentation or optical acceptance.
- Remaining: Actual candidate OTA/new-boot receipts and human matrix/audio timing remain separate gates, not inferred from host tests. Raw UART holds matrix output and defers cloud writes to protect programming.

### PCController: `LocalLib/I2cLcd.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `0ee650ad4febe97d1dfbe3b15dece079611f5bec1b71b6a414a84bc5fa9e4b72`.

- Target: `libraries/BoardCore/src/LcdDisplay.h:10 [UTF8-LF sha256=93bad5e225c75ff7604e9dd31db8564a518ed53d56a721a81d7f143b31b87d1e; anchor=class LcdDisplay]`.
- Target: `firmware/esp32s3/src/LcdPort.cpp:3 [UTF8-LF sha256=97830b8c55a9a5214edfd537ab8426696f79625ab7cf30183b14066b19ef9c94; anchor=void LcdPort::begin(]`.
- Target: `firmware/esp8266/src/LcdManagement.h:3 [UTF8-LF sha256=eb800ee4c334b22504f37bd841bda58a7562052df2c7226ff19bc23a454e652d; anchor=String lcdStatusJson()]`.
- Target: `desktop/src/display.rs:7 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=pub struct LcdObservation]`.
- Target: `mobile/lib/peer_context.dart:180 [UTF8-LF sha256=5f520bd21d49ac1460fd3cbd24f4713ccddce0168d78046774a19a181c68962d; anchor=bool lcdObservationReady(]`.
- Target: `firmware/esp8266/src/WebUi.h:810 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function lcdObservation(]`.
- Retained: Complete actual donor source read through EOF. Adapted common PCF8574 0x27/0x3F discovery, 4-bit HD44780 wake/control bytes,16x2 ASCII row normalization/padding, desired frame cache and backlight bit mapping into a transport-independent production class. S3 owns dedicated GPIO15/16 hardware I2C so the confirmed AVR A4/A5 TM1637 and ISP pins remain untouched.
- Omitted/replaced code: `Blocking50ms/4.5ms initialization, ignored expander return values and cache validity after failed writes`. Disposition: **replaced**. Reason: Initialization is a paced loop-owned state machine. Every expander write must ACK; no full frame/available status until all initialization and both rows complete. NACK revokes readiness, bounded retry reinitializes/replays current desired text. Missing modules cannot become initialized merely because initialize() returned.
- Omitted/replaced code: `Flash-string overload and LCD clear/backlight/generic bus controls on all client surfaces; host-owned LCD prompt/priority/offline policy`. Disposition: **pending**. Reason: This slice implements standalone physical discovery/init/default cell/update text, finite REST presentation and exact completed-frame previews. It does not claim the full donor host presenter, raw I2C bridge, every app editor or USB LCD mutation parity. CompactI2c remains available for a future explicit nonconflicting AVR wiring/build profile after flash/RAM review.
- Omitted/replaced code: `Unrestricted address scan, geometry and backpack pinout inference`. Disposition: **replaced**. Reason: Retain donor16x2 and explicit common backpack mapping/addresses only. A generic expander ACK cannot identify LCD glass, geometry, contrast or optical success; no broad scanner or unsupported OLED/TFT/20x4 capability is advertised.
- Test-code evidence: `tests/lcd_display_test.cpp:47 [UTF8-LF sha256=69c315bd5b749166cf69088e9dc8dea2f1610e11ef2134877c17140b6f68ac37; anchor=for(unsigned position=1;position<=246;]`.
- Test-code evidence: `tests/lcd_web_test.mjs:12 [UTF8-LF sha256=6abf8aedbb70f02cdfae0adfc7511aa9e3651cbdc1c22e885bdfca92bf79bdfb; anchor=for(const key of []`.
- Test-code evidence: `mobile/test/lcd_observation_test.dart:6 [UTF8-LF sha256=3eef7afae9cc8304b33e32afe6c56f51996193e89a618bbfce345804c589e593; anchor=LCD preview requires its own complete]`.
- Test-code evidence: `desktop/src/display.rs:34 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=fn lcd_requires_backpack_initialization_and_complete_current_frame()]`.
- Test-code evidence: `tests/lcd_management_test.cpp:72 [UTF8-LF sha256=d1080e4369cdb3dddd4dc2b3966c0ca0aa2fda58f134865d381c940c4cb50441; anchor=Actual LCD HTTP handlers/status:]`.
- Scoped advantage: Checked incremental initialization and whole-frame admission replace unconditional readiness; display availability is distinct from bus ACK and from physical visibility.
- Remaining: Actual module/power/pull-up safety, deployed S3 I2C and AVR TM1637 ACK/frame presentation, unplug/replug/reboot, content/contrast and simultaneous LED/W5500 traffic acceptance remain open. ASUS SSH8022 refuses; no hardware actuation or optical claim. Direct AVR A4/A5 LCD substitution is NOT enabled while those pins own TM1637. LCD REST controls and previews exist; full native/mobile/USB LCD editors and donor-wide host presentation policy remain pending.

### PCController: `LocalLib/I2cLcd.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `e0122c1e54da7c41e6035b424f4537a94f75dba5fcbc8ad06a29029cb0fb2104`.

- Target: `libraries/BoardCore/src/LcdDisplay.h:10 [UTF8-LF sha256=93bad5e225c75ff7604e9dd31db8564a518ed53d56a721a81d7f143b31b87d1e; anchor=class LcdDisplay]`.
- Target: `firmware/esp32s3/src/LcdPort.cpp:3 [UTF8-LF sha256=97830b8c55a9a5214edfd537ab8426696f79625ab7cf30183b14066b19ef9c94; anchor=void LcdPort::begin(]`.
- Target: `firmware/esp8266/src/LcdManagement.h:3 [UTF8-LF sha256=eb800ee4c334b22504f37bd841bda58a7562052df2c7226ff19bc23a454e652d; anchor=String lcdStatusJson()]`.
- Target: `desktop/src/display.rs:7 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=pub struct LcdObservation]`.
- Target: `mobile/lib/peer_context.dart:180 [UTF8-LF sha256=5f520bd21d49ac1460fd3cbd24f4713ccddce0168d78046774a19a181c68962d; anchor=bool lcdObservationReady(]`.
- Target: `firmware/esp8266/src/WebUi.h:810 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function lcdObservation(]`.
- Retained: Complete actual donor source read through EOF. Adapted common PCF8574 0x27/0x3F discovery, 4-bit HD44780 wake/control bytes,16x2 ASCII row normalization/padding, desired frame cache and backlight bit mapping into a transport-independent production class. S3 owns dedicated GPIO15/16 hardware I2C so the confirmed AVR A4/A5 TM1637 and ISP pins remain untouched.
- Omitted/replaced code: `Blocking50ms/4.5ms initialization, ignored expander return values and cache validity after failed writes`. Disposition: **replaced**. Reason: Initialization is a paced loop-owned state machine. Every expander write must ACK; no full frame/available status until all initialization and both rows complete. NACK revokes readiness, bounded retry reinitializes/replays current desired text. Missing modules cannot become initialized merely because initialize() returned.
- Omitted/replaced code: `Flash-string overload and LCD clear/backlight/generic bus controls on all client surfaces; host-owned LCD prompt/priority/offline policy`. Disposition: **pending**. Reason: This slice implements standalone physical discovery/init/default cell/update text, finite REST presentation and exact completed-frame previews. It does not claim the full donor host presenter, raw I2C bridge, every app editor or USB LCD mutation parity. CompactI2c remains available for a future explicit nonconflicting AVR wiring/build profile after flash/RAM review.
- Omitted/replaced code: `Unrestricted address scan, geometry and backpack pinout inference`. Disposition: **replaced**. Reason: Retain donor16x2 and explicit common backpack mapping/addresses only. A generic expander ACK cannot identify LCD glass, geometry, contrast or optical success; no broad scanner or unsupported OLED/TFT/20x4 capability is advertised.
- Test-code evidence: `tests/lcd_display_test.cpp:47 [UTF8-LF sha256=69c315bd5b749166cf69088e9dc8dea2f1610e11ef2134877c17140b6f68ac37; anchor=for(unsigned position=1;position<=246;]`.
- Test-code evidence: `tests/lcd_web_test.mjs:12 [UTF8-LF sha256=6abf8aedbb70f02cdfae0adfc7511aa9e3651cbdc1c22e885bdfca92bf79bdfb; anchor=for(const key of []`.
- Test-code evidence: `mobile/test/lcd_observation_test.dart:6 [UTF8-LF sha256=3eef7afae9cc8304b33e32afe6c56f51996193e89a618bbfce345804c589e593; anchor=LCD preview requires its own complete]`.
- Test-code evidence: `desktop/src/display.rs:34 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=fn lcd_requires_backpack_initialization_and_complete_current_frame()]`.
- Test-code evidence: `tests/lcd_management_test.cpp:72 [UTF8-LF sha256=d1080e4369cdb3dddd4dc2b3966c0ca0aa2fda58f134865d381c940c4cb50441; anchor=Actual LCD HTTP handlers/status:]`.
- Scoped advantage: Checked incremental initialization and whole-frame admission replace unconditional readiness; display availability is distinct from bus ACK and from physical visibility.
- Remaining: Actual module/power/pull-up safety, deployed S3 I2C and AVR TM1637 ACK/frame presentation, unplug/replug/reboot, content/contrast and simultaneous LED/W5500 traffic acceptance remain open. ASUS SSH8022 refuses; no hardware actuation or optical claim. Direct AVR A4/A5 LCD substitution is NOT enabled while those pins own TM1637. LCD REST controls and previews exist; full native/mobile/USB LCD editors and donor-wide host presentation policy remain pending.

### PCController: `LocalLib/SevenSegments.cpp`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `34e0e8a481bc5febed33dc5610d783bb093c8be23fdbc4de796d066c1d389769`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:100 [UTF8-LF sha256=73b97393db8937820b153c4b776014e2dda672c579a179f133777b1fe5107997; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:291 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:527 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/SevenSegments.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `e58e1783404f494ea7b1852780b6485cbfda872781b9159b9ee29426edae9240`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:100 [UTF8-LF sha256=73b97393db8937820b153c4b776014e2dda672c579a179f133777b1fe5107997; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Omitted/replaced code: `New donor ATmega328P-only fast-port guard and AVR validation removal`. Disposition: **replaced**. Reason: Pakar AVR target remains explicitly ATmega328P, with its correct PC4/PC5 ports and retained sentinel/brightness validation on all branches. Removing checks only to mirror donor performance changes would weaken bounds without measured need. Other AVR targets are not advertised; future ports must bind their own pins and guard.
- Test-code evidence: `tests/pc_contract_test.cpp:291 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:527 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/ShiftRegisters.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f3223125a4f9f285cc4678fb4956007ede42ad351adff3616bac0331426b65ce`.

- Target: `firmware/avr/ShiftRegisters.cpp:1 [UTF8-LF sha256=43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133; anchor=#include "ShiftRegisters.h"]`.
- Retained: Actual active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection retained. Full reread of the current donor shows its direct-port optimization is now specifically guarded for ATmega328P; Pakar still uses __AVR__ and is no longer byte-identical.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- Omitted/replaced code: `ATmega328P-specific direct-port guard and generic AVR fallback`. Disposition: **pending**. Reason: Current hardware is ATmega328P, so behavior is unchanged here; future AVR builds must narrow Pakar's broad __AVR__ guard and test alternate pin maps before enabling them. Do not advertise portability from native-only tests.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/ShiftRegisters.h`

Verdict: **equivalent-file**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff`.

- Target: `firmware/avr/ShiftRegisters.h:32 [UTF8-LF sha256=397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff; anchor=class ShiftRegisters]`.
- Retained: Byte-identical donor file, including eight active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- No production-code omission claimed for this reviewed file.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/Tasks.cpp`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `267cfee4c78ceb42a426a5db5faa4e0707a41def64cfd394c3976045e4900bbc`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### PCController: `LocalLib/Tasks.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `eeafda75fd9b4e0830c77d31546b047a326da63448b9bb73800b65c2394569de`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:174 [UTF8-LF sha256=bc8d17a6f841b9cfb7103d13569dbea932db8ec57174b44af8fe5064de9542da; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
- Retained: Fully read original donor. Actual PCController context/occupancy/count/cancel/clear/deadline code adapted in existing BoardCore implementation; Puzzles/Timer ten-slot capacity restored. Actual AVR loop services the scheduler and uses it for the100ms Reset delay, canceled on Prog entry.
- Retained: Cancellation/clear suppress later due work, callback-added/replaced slots wait for next outer pass, recursive update is blocked, zero/exact/wrapped deadlines work and half-range delays reject.
- Omitted/replaced code: `Puzzles startsAt==0 sentinel, strict now > deadline, null callbacks and post-callback itemCount decrement; PCController eight slots, implicit millis-only addTask and same-pass additions`. Disposition: **replaced**. Reason: Use newer explicit occupancy/pre-callback retirement, ten-slot capacity, optional shared command clock, half-range bound and eligibility mask. Zero/wrap/reentrant/canceled/replaced/new callback regressions execute actual production code; no heap or callback batch copy.
- Omitted/replaced code: `Puzzles Callback(void), both global taskManager and PCController serviceTasks wrapper; unused Puzzles index/full/empty helpers`. Disposition: **replaced**. Reason: Keep typed context callbacks, count/clear API inside BoardCore and one AVR-owned instance, explicitly serviced in loop. No unsafe function-pointer cast or cross-board global allocation. This is an adapted capability, not drop-in ABI aliases. Slots remain reusable indices; owners discard canceled handles.
- Test-code evidence: `tests/tasks_test.cpp:61 [UTF8-LF sha256=1fc35f2e57d21d42048e392a28bdb09c123d7cf84a3a4c9a45c03534dc19977e; anchor=Reset-owner use]`.
- Test-code evidence: `tests/tasks_wiring_test.mjs:11 [UTF8-LF sha256=e8cf37f8f07119bf2a8ce85d706883fe220d69337257fbdab70db745f69e20c0; anchor=No parallel uncancellable reset timer]`.
- Scoped advantage: Restore tested actual scheduler consumption and ten-slot capacity; no zero deadline ambiguity or recursive same-pass scheduling. Loop-only service must run within half the clock and callbacks must not block.
- Remaining: Physical watchdog/Prog/reset-versus-ISP and cooperative service latency acceptance; clock doubles/source/compile do not prove board timing. Other donor features remain independently pending.

### PCController: `LocalLib/TonePlayer.cpp`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `08b41c4240ec899a3184525a864740b2762e7e675d75302c0adbdb27b4a6e511`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=0de3422d83061153abb89170ce3ef94a7acb973cc46314d0c9054b92284f5e6d; anchor=startsAt = stepEndsAt_]`.
- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Target: `firmware/avr/BootMelody.cpp:36 [UTF8-LF sha256=f163fcef8c0647ce0f0696c219b5d178e8815ff3df82bf2c5dec6c5c11400433; anchor=case kCueAttention:]`.
- Target: `libraries/BoardCore/src/AttentionCues.h:13 [UTF8-LF sha256=02693199ab57f58a0531f46d8afa05ccd46dbd4c28b8db50b55bf5e4d1cff7b6; anchor=class Upload]`.
- Target: `tools/test-audio.ps1:18 [UTF8-LF sha256=41500a0595fe030dd36f70175b56ee6f16ff62fe3bd1b2d9bf9bb29e94f90b30; anchor=Audio ·]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Retained: Silent changes increment tone revision immediately (including no active note); unmute follows donor behavior and does not restart an interrupted note. Timeline metadata remains available for independently configured host mirroring; all fourteen cues stay electrically silent while muted.
- Retained: The existing Timer1/nonblocking player now also owns a finite 240ms attention cue, additive ID14; cue count is ACK-gated across all clients. Silent/Prog behavior and original melodies remain unchanged.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:39 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:50 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.setMuted(false)]`.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Test-code evidence: `tests/tone_player_test.cpp:129 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=fifteen finite cues]`.
- Test-code evidence: `tests/attention_cues_test.cpp:10 [UTF8-LF sha256=2b6bd229d5c9b9c5bdc233b97b88ca4d3422e34fad2dcb20dbb11db46732b3b0; anchor=Late disconnect cannot replace completion]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; mute changes publish immediately; unmute no longer restarts an interrupted note.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.

### PCController: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `edb2f40cfdfb37d1c08867c121c617beb8f9f53ad2583855ae2218a07eb9fd89`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=0de3422d83061153abb89170ce3ef94a7acb973cc46314d0c9054b92284f5e6d; anchor=startsAt = stepEndsAt_]`.
- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Target: `firmware/avr/BootMelody.cpp:36 [UTF8-LF sha256=f163fcef8c0647ce0f0696c219b5d178e8815ff3df82bf2c5dec6c5c11400433; anchor=case kCueAttention:]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Retained: Silent changes increment tone revision immediately (including no active note); unmute follows donor behavior and does not restart an interrupted note. Timeline metadata remains available for independently configured host mirroring; all fourteen cues stay electrically silent while muted.
- Retained: The existing Timer1/nonblocking player now also owns a finite 240ms attention cue, additive ID14; cue count is ACK-gated across all clients. Silent/Prog behavior and original melodies remain unchanged.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:39 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:50 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=player.setMuted(false)]`.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Test-code evidence: `tests/tone_player_test.cpp:129 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=fifteen finite cues]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; mute changes publish immediately; unmute no longer restarts an interrupted note.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.

### PCController: `mobile/android/app/src/main/AndroidManifest.xml`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `728e6808793b9ad82baf19d6e4c98a8fec68bce6b83d0fff0da5c7f31d48b9c4`.

- Target: `mobile/android/app/src/main/AndroidManifest.xml:5 [UTF8-LF sha256=5d7feefe177117b4bd37c845efec4a328fb44a6a0147b9870fa21781a6a18280; anchor=REQUEST_INSTALL_PACKAGES]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:37 [UTF8-LF sha256=b6623d6be8e23e57532a5afc210a32ad4ca787cd40fb7d66d5796147917a322a; anchor=appUpdates = AppUpdateChannel]`.
- Retained: Actual REQUEST_INSTALL_PACKAGES and nonexported installer result receiver registration adapted to Pakar application identity. Existing app package and launcher artwork remain unchanged.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/app_updates_test.dart:190 [UTF8-LF sha256=847413faf38f07ad3da23ed127fcf41ff796ee576430babe588c06a53a1c2a94; anchor=native bridge]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/DiscoveryChannel.kt`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b4b829b34dfd99d1cadcd05b4d15b23e1277d6d8ffa82090f2abfd4843d48c5c`.

- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/AppUpdateDiscoveryChannel.kt:16 [UTF8-LF sha256=671c6d58afc2c594f4bd7df89cbd7d98b233b43c7be14fc5697109924bdd4678; anchor=class AppUpdateDiscoveryChannel]`.
- Target: `mobile/lib/app_update_discovery.dart:58 [UTF8-LF sha256=ac6157940d99d94c21a32e7baebe1e7d05d868fb432016f63cd2efad74fd2fc8; anchor=verifyAppUpdatePeer]`.
- Retained: Actual Android NSD browse/resolve/registration and multicast lock lifecycle adapted to app-package-only service. Dart uses bounded SSDP/MNDP messages plus exact package manifests; raw advertisements never grant install or board control.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/app_update_discovery_test.dart:37 [UTF8-LF sha256=202d5265b03db20e13768806bf617a944ddd3a70f7241d17e559c3c319bdede9; anchor=Actual manifest GET]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `5bf90a090b5975f5dc3b7e09ed47ba09e9dad8bf812515902b24cacc35e9a618`.

- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:30 [UTF8-LF sha256=b6623d6be8e23e57532a5afc210a32ad4ca787cd40fb7d66d5796147917a322a; anchor=private val firmwareMaximum = 0x330000 - 1]`.
- Target: `mobile/lib/documents.dart:19 [UTF8-LF sha256=205c298afff227e61f4ddea3b071f65012e3ccc118cbd62f1de5dea8452eab1e; anchor=class AndroidDocuments]`.
- Target: `mobile/lib/firmware_image.dart:8 [UTF8-LF sha256=274b96b95c959594b8720f40d4998dc6e307ed8fe31e82093c19b581ea4a0654; anchor=abstract final class FirmwareImagePolicy]`.
- Retained: Actual canonical 8269-byte MainActivity read completely through EOF by the mobile owner and evidence owner. Retains real ACTION_OPEN_DOCUMENT/ACTION_CREATE_DOCUMENT, single selection, bounded executor-based input reads, cancellation, one outstanding document operation, one-use random destination token consumed before write, explicit abandon without deletion, finite byte export and close-time token/pending cleanup. Pakar package/channel names replace PCController identity, not its document ownership contract.
- Retained: The obsolete generic2097152-byte intake/export ceiling is replaced by the same exclusive0x330000 S3 application-slot ceiling used by Dart firmware admission. Native SAF remains only a bounded byte/document provider: selected-board chip/hash-header/capacity/SHA checks happen in the firmware owner before upload. A selected or exported document never automatically programs ESP or AVR.
- Omitted/replaced code: `AutomationBridge.channel, automation MethodChannel and cleanup`. Disposition: **pending**. Reason: The donor's generic Android automation bridge is not part of this SAF/image-limit change. App-to-app remote commands and intent authorization require their actual implementation/dependency review; no document channel or BLE integration is claimed to substitute for it.
- Omitted/replaced code: `PreferencesChannel and preferences.launch from onNewIntent`. Disposition: **pending**. Reason: Pakar has its own preferences UI but does not yet import this donor's OS preference-launch/new-intent lifecycle. Opening SAF or retaining Flutter state does not implement that useful Android contract.
- Omitted/replaced code: `UpgradeChannel construction/lifecycle and whole-activity discovery/tone/app-update parity`. Disposition: **pending**. Reason: Pakar separately owns AppUpdateChannel, peer-discovery, BLE and tone channels; this row certifies only the inspected SAF adaptation, not full donor upgrade/automation/discovery/mirroring equivalence or provider/OS behavior. Keep their owning rows and remaining acceptance rather than infer parity from class names.
- Test-code evidence: `mobile/test/documents_test.dart:64 [UTF8-LF sha256=641aee7d7a005c5c309683cf98154f571c1e3c398dd5674fc68b020807807df0; anchor=S3 document intake above 2MiB uses one bounded slot policy]`.
- Test-code evidence: `mobile/test/documents_test.dart:38 [UTF8-LF sha256=641aee7d7a005c5c309683cf98154f571c1e3c398dd5674fc68b020807807df0; anchor=Changed owner while choosing export abandons token; never writes or deletes]`.
- Test-code evidence: `mobile/test/firmware_image_test.dart:215 [UTF8-LF sha256=81f47d2e4a07740c1dab37cf55849a8a5c91eddf47631e7b631da5dad7b6ca4b; anchor=Wrong chip or observed small slot cannot reach manual mutation]`.
- Remaining: Verify actual Android document providers, cancellation, process/activity destruction, exact selected-byte upload/export and one-use-token failure behavior on the authorized phone. Dart MethodChannel fixtures are not native SAF/provider or physical OTA proof. Finish useful omitted automation/preferences/new-intent/upgrade lifecycle imports with exact source evidence.

### PCController: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/UpgradeChannel.kt`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `c35459e58af7b325f450aa54e20a797efadde5006da1683f370500b2ffea3cc4`.

- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/AppUpdateChannel.kt:34 [UTF8-LF sha256=27841afb11ebc904de95f1c2ad1f40363a707d2f8bee5a9868b7f0a45a0cef6f; anchor=class AppUpdateChannel]`.
- Retained: Actual private staging/storage/size/hash/self-package/signer/newer-version/minSDK/ABI validation and Android PackageInstaller user-consent/receipt flow. Added SAF import/export, bounded native queue, same digest immediately before install and actual installed SHA/version verification after restart; pending-user-action is never success.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/app_updates_test.dart:81 [UTF8-LF sha256=847413faf38f07ad3da23ed127fcf41ff796ee576430babe588c06a53a1c2a94; anchor=verified]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/lib/connection_preferences.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7c4c3a679ede250bb44effc96ccaf8a33a49f15e3ec8a1e1f5dfca082d64c72f`.

- Target: `mobile/lib/connection_preferences.dart:54 [UTF8-LF sha256=dc20e6ec07521d724c97b793b69d36613a9c0b6e980f0184105a41e844bc5318; anchor=class ConnectionPreferences]`.
- Target: `mobile/lib/main.dart:587 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=_saveWorkspaces]`.
- Retained: Complete donor read: preserve secure password migration and last explicit connection profile; adapt nonsecret connection draft restoration and explicit Disconnect/transport intent without automatically selecting discovered paths. Pristine settings fields follow async profile loading, edited fields retain the draft.
- Omitted/replaced code: `Mandatory/nonempty default password and restoring secret drafts`. Disposition: **replaced**. Reason: Current board authentication is optional. Existing secure credential ownership remains separate; workspace persistence excludes passwords/tokens and does not require credentials before the board does.
- Test-code evidence: `mobile/test/workspace_lifecycle_test.dart:13 [UTF8-LF sha256=da99bd0f4eb2b7cac91106da4673cab150929a028dcc0001a675c51f8fb148a9; anchor=connection]`.
- Remaining: Actual Android restart/background lifecycle and full profile UX acceptance remain open.

### PCController: `mobile/lib/device_settings.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `16c5f8ec88c242baa279bc42bdf4c81024f4473eccc62ec8e317267937a9ab2c`.

- Target: `mobile/lib/device_settings.dart:222 [UTF8-LF sha256=0c10e5c712bae2b4fe2f706326115ee703f07e426a64a50728c09575efb71e9a; anchor=class DeviceSettingsWorkspace]`.
- Target: `mobile/lib/device_settings_page.dart:18 [UTF8-LF sha256=143c7e774de6f90a48a1bcdbdf37c3241a9f86106b04768befa1b9c6f3d02774; anchor=class DeviceSettingsPage]`.
- Retained: Complete donor read: adapt actual SettingsDraft.observe saved/value/conflict and owner/epoch/ACK/readback semantics to persisted public-only patches after fresh reads. Preserve original baselines over repeated reconnects so conflicts cannot silently become saveable; secret replacement/clear intent and pending operations are never serialized.
- Omitted/replaced code: `Persisting secret fields and uncertain write intents`. Disposition: **replaced**. Reason: Secrets stay in the existing secure/memory-only owners; pending writes are revoked, never automatically replayed after reconnect.
- Test-code evidence: `mobile/test/device_settings_test.dart:72 [UTF8-LF sha256=f9887ced8439ac531c05348bc39f5ab7344c6386eade342d18809a50a2741751; anchor=draft]`.
- Remaining: Installed mobile reboot/reconnect, public-draft conflict UX and broad generic settings parity remain open.

### PCController: `mobile/lib/devices_page.dart`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `077c722018b5849d5d972f228b326151da15cc4238c3cd54052219df4d8d59db`.

- Target: `mobile/lib/devices_page.dart:59 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=List<DeviceListEntry> unifiedDevices(]`.
- Target: `mobile/lib/devices_page.dart:382 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=Future<void> connect(DeviceListEntry entry)]`.
- Target: `mobile/lib/device_registry.dart:115 [UTF8-LF sha256=aa10bd84de1e6bc16e5c9d455db6ef3e97251276fa6a41184eebeb0bcd99ad3f; anchor=Future<SavedDevice> save(]`.
- Target: `mobile/lib/workspace_widgets.dart:82 [UTF8-LF sha256=18e0b0755aaebbd42c571e2c0a7cee18d42f7a62e789f3d0aa1ccb8c40c3c747; anchor=class WorkspaceSection]`.
- Target: `mobile/lib/app_theme.dart:15 [UTF8-LF sha256=f414102bbefe21dc4f42cdec4a41176192c3409e4595af4c97e2e96a40aac956; anchor=ThemeData studioTheme()]`.
- Retained: Complete actual donor DevicesPage read through EOF. Retains local/remote profile editing, bounded normalized endpoints, masked credential replacement, save/reload failure handling, confirmation before forgetting and explicit selected-peer handoff. Adapts actual StatefulBuilder form/busy ownership instead of a static visual mock.
- Retained: Replaces separate saved/nearby lists with a searchable and filterable unified directory. Saved display names survive discovery; unavailable saved profiles dim without disabling manual retry. Public compatible discovery never authorizes board controls. One connect action performs a fresh capability/board/boot read, prompts only on401 and verifies submitted credentials before saving/selecting. Empty passwords are valid client preferences; unknown secrets are never replaced with a default guess.
- Retained: One Studio theme, reusable page header, icon-labelled sections, badges, constrained detail width and stable input borders now serve the directory, connection preferences, device settings, firmware and bridge. Bridge has persistent in-memory Session/MQTT/WebSocket/Routing navigation rather than a long mixed setup/status dump. Screenshot fixtures consume that actual same theme.
- Omitted/replaced code: `Two independent saved/discovered lists and permanent unverified advertisement labels`. Disposition: **replaced**. Reason: They duplicate one physical board across protocols/profiles and do not perform a compatibility check. The unified row retains saved naming, aggregates sources, uses bounded public probes and dims stale/unreachable saved intent.
- Omitted/replaced code: `Mandatory password at save and immediate username/password form for new discovered profiles`. Disposition: **replaced**. Reason: The user explicitly requires optional credentials and no prompt until needed. New profiles save without a password; existing profile edits allow keep/replace/clear. A protected API read's401 triggers a bounded sign-in flow, not a hardware-command replay.
- Omitted/replaced code: `Donor page-local plain card/text presentation`. Disposition: **replaced**. Reason: Shared Studio roles and reusable workspace components prevent page/capture styling drift. Flutter adapts the inspected Pealayer PreferencesTab header/group pattern, not TSX widgets or media business.
- Test-code evidence: `mobile/test/devices_page_test.dart:371 [UTF8-LF sha256=53febe7f492d28a40f71e336eceb1d83f5a87994e6315089ec08b4eb7333c23f; anchor=Credentials requested only after challenge:]`.
- Test-code evidence: `mobile/test/devices_page_test.dart:84 [UTF8-LF sha256=53febe7f492d28a40f71e336eceb1d83f5a87994e6315089ec08b4eb7333c23f; anchor=Actual registry accepts optional secrets]`.
- Scoped advantage: Deduplicated saved/discovered device management with optional secure credentials, challenge-driven verified selection and adaptive shared workspace styling, exercised at phone/tablet widths.
- Remaining: Install the built APK and inspect real phone/tablet navigation, field focus, profile edits, stored credential access and discovery/connection under network loss. This scoped source/test comparison is not full application/donor parity or human acceptance.

### PCController: `mobile/lib/discovery.dart`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `fc37b6638aee7b3078b3822d8b221374066d68b40908d815968a06d0c3f351da`.

- Target: `mobile/lib/discovery.dart:400 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=Future<DiscoveredDevice?> verifyDiscoveredDevice(]`.
- Target: `mobile/lib/discovery.dart:39 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=bool sameBoard(DiscoveredDevice other)]`.
- Target: `mobile/lib/devices_page.dart:48 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=String? preferredOrigin(ConnectionMethod method)]`.
- Retained: Complete actual donor source read. Adapted scan concurrency guard, bounded four-worker public compatibility probes, source-pinned SSDP response locations, duplicate-header rejection, packet/TLV bounds, metadata-only discovery and failure cleanup. Pakar public description.xml plus capability-bearing status replace donor healthz/public-directory endpoints; scanning never sends credentials or mutations.
- Retained: Added one session catalog keyed by stable board identity/UDN, normalized MAC, canonical origins and observed IPs rather than endpoint+protocol. Protocols and hostname aliases merge; explicit conflicting board IDs cannot merge on a reused DHCP address. Distinct origins are probed before cross-channel coalescing so an Android DNS failure cannot suppress a working IP advertisement. Failed scans revoke availability without discarding remembered metadata.
- Retained: Reread the exact complete donor discovery.dart and resolver.go at b0111ae0 with matching source hashes. Preserve its bounded per-origin public probes while expanding saved/advertised aliases before board coalescing. Keep current-sweep exact-origin observations separate from identity/history: AP, STA and Ethernet IP/scheme/port do not normalize into each other, and aliases do not inherit availability. The route selector retains explicit saved/selected origin through rescans and Connect uses that exact origin only; unsaved default preference chooses a currently observed hostname when available. Existing no-redirect, auth-on-challenge, board/boot checks and no mutation replay remain.
- Omitted/replaced code: `Donor healthz and old PCController upnp/public.json directory fallback; pccontroller/pealayer/controller-mobile product list`. Disposition: **replaced**. Reason: Those are host/media service contracts, not Pakar ESP firmware endpoints. Require actual Pakar public XML identity or the acknowledged living board capability/status contract. Advertising a generic HTTP service does not create a compatible board.
- Omitted/replaced code: `MNDP vendor product/API-port TLVs 0xff01/0xff02`. Disposition: **replaced**. Reason: The actual current Pakar MndpService advertises model TLV12=PakarController, MAC TLV1 and board ID TLV11 with its fixed HTTP port80 on both ESP builds. Parse only that product, not arbitrary router names. Do not pretend the donor's distinct host-service extension is already emitted by this firmware.
- Omitted/replaced code: `localEndpoints exclusion for a self-advertised mobile service`. Disposition: **not-applicable**. Reason: Pakar Android's loopback-only bridge does not advertise a Pakar firmware description. Public compatibility filtering rejects it; discovery does not imply an app responder or a generic remote host service.
- Test-code evidence: `mobile/test/discovery_transport_test.dart:144 [UTF8-LF sha256=11fcdd2c1e4f8068571743f6d5f5d4f69ddd6badde00a86786574ab3ac4d8ab7; anchor=Real read-only XML/status probes: no credentials, redirects or mutations]`.
- Test-code evidence: `mobile/test/devices_page_test.dart:115 [UTF8-LF sha256=53febe7f492d28a40f71e336eceb1d83f5a87994e6315089ec08b4eb7333c23f; anchor=Identity/IP/MAC merge preserves channels and saved names]`.
- Test-code evidence: `mobile/test/devices_page_test.dart:245 [UTF8-LF sha256=53febe7f492d28a40f71e336eceb1d83f5a87994e6315089ec08b4eb7333c23f; anchor=Exact interface selection survives discovery]`.
- Test-code evidence: `mobile/test/discovery_transport_test.dart:15 [UTF8-LF sha256=11fcdd2c1e4f8068571743f6d5f5d4f69ddd6badde00a86786574ab3ac4d8ab7; anchor=Saved multi-interface aliases are probed separately]`.
- Scoped advantage: One verified compatible-device catalog across discovery protocols, bounded secret-free probes, stale availability retirement and conflict-aware IP/MAC identity coalescing, with real HTTP and UI fixtures.
- Remaining: Verify actual Android multicast/NSD, AP+STA/W5500 aliases, DHCP moves and routers; public XML is compatibility metadata, not cryptographic identity or command authorization. On-device installation and human acceptance remain open in issue2.

### PCController: `mobile/lib/firmware_page.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `40b64996112f85e7acfb3f5c7db19c473cbfc0a018c72387f1447a4e386be626`.

- Target: `mobile/lib/firmware_page.dart:335 [UTF8-LF sha256=47062a176a76c93de964e710682b19b28c6fd1f3fd8f5ebaee525244f0dc681b; anchor=avrMaintenance()]`.
- Target: `mobile/lib/firmware_automation_page.dart:9 [UTF8-LF sha256=b0e65486832e9c4d72c846626444a819d3a76ff5d7456986ef168c384a25db67; anchor=class FirmwareAutomationPage]`.
- Target: `mobile/lib/firmware_automation.dart:18 [UTF8-LF sha256=f61aa58c32f3988a405504aa97ad7a3115577a751d5bf221859f46eb41803b06; anchor=class FirmwareAutomation]`.
- Retained: Actual run/confirmation/file-pick/cancel/no-uncertain-replay ownership adapted to FirmwarePage. New hash-bound ISP/Urboot maintenance dialog replaces unpinned shared-image programming. Persistent exact-board/application foreground automation owns source feeds outside page lifetime; automatic Apply is an explicit policy on the existing verified manifest transaction, not raw shared AVR staging.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/firmware_automation_test.dart:301 [UTF8-LF sha256=2046ae0d8c835d0dbd598890f28fe965f0227dcfc8cc0323ab3009f618453f01; anchor=Lost update ACK persists consumed digest]`.
- Test-code evidence: `mobile/test/firmware_page_test.dart:129 [UTF8-LF sha256=a8f8dcbddecdb44e81142714c1f7874dbd99de08961b516cc3b81ab4970c885a; anchor=Contextual real firmware layout]`.
- Test-code evidence: `mobile/test/update_pages_test.dart:45 [UTF8-LF sha256=f79717fe73ad600a6ad04b6191f6ae986a26d6e9d89ea61f26c99205421b76d3; anchor=Raw ESP offer exposes immediate upload]`.
- Test-code evidence: `mobile/test/capture_test.dart:33 [UTF8-LF sha256=c62fe3c9f6e73a0f4e0fae84925332c5dde99822f6e5680e95dc8c5dcb0cb06c; anchor=Explicit simulated renderer capture]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/lib/firmware.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a9e1ecce00ae674541490e64733accdfc882dcec8ff78d1c594d0ff89d5b9c86`.

- Target: `mobile/lib/firmware.dart:178 [UTF8-LF sha256=190ddce7b1bbf298a74464ea8202287ead801bfe8725ab4d0af39993d5a6c30e; anchor=class FirmwareController]`.
- Target: `mobile/lib/firmware_image.dart:8 [UTF8-LF sha256=274b96b95c959594b8720f40d4998dc6e307ed8fe31e82093c19b581ea4a0654; anchor=abstract final class FirmwareImagePolicy]`.
- Target: `mobile/lib/documents.dart:22 [UTF8-LF sha256=205c298afff227e61f4ddea3b071f65012e3ccc118cbd62f1de5dea8452eab1e; anchor=int maximum = FirmwareImagePolicy.maximumBytes]`.
- Target: `mobile/lib/firmware_page.dart:304 [UTF8-LF sha256=47062a176a76c93de964e710682b19b28c6fd1f3fd8f5ebaee525244f0dc681b; anchor=maximum: controller.imageMaximum(esp),]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:30 [UTF8-LF sha256=b6623d6be8e23e57532a5afc210a32ad4ca787cd40fb7d66d5796147917a322a; anchor=private val firmwareMaximum = 0x330000 - 1]`.
- Target: `mobile/lib/api.dart:187 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Stream<DeviceStatus> get statusUpdates]`.
- Retained: Completely read the current canonical donor. Retain exact full AVR image/Urboot staging, explicit remote stage/apply/job pinning, capability/connection ownership and no uncertain write replay. Extend its actual manual updater with independently verified HTTP(S) file download, upload byte callbacks and raw-length/MD5 OTA headers; progress uses the existing peer event stream.
- Retained: FirmwareImagePolicy now owns the platform-specific S3/ESP8266/AVR byte/header/capacity policy used by HTTP intake, selected SAF document, manual upload and firmware export. FirmwareController.imageMaximum/validateImage consume only selected-board observed capacity; free heap/total flash are not fabricated slot sizes. Android's outer document ceiling matches the S3 exclusive slot bound, while AVR staging remains exactly32768 and never automatic programming.
- Omitted/replaced code: `Remaining donor mechanisms outside this delivery slice`. Disposition: **pending**. Reason: Direct Android USB-host flashing and a production Android background firmware service remain unsupported, not silently replaced by Wi-Fi. AVR raw staging is not programming. Automatic verified-manifest Apply now requires an explicit persisted exact-board/application policy and a durable consumed-digest receipt; this does not provide an unguarded raw-image auto-program path.
- Test-code evidence: `mobile/test/firmware_delivery_test.dart:104 [UTF8-LF sha256=2a5a1ff4950f3806d522d2afe76050884aff03a7f3f0556abc329f25a0042976; anchor=HTTP file verifies hash before one upload]`.
- Test-code evidence: `mobile/test/firmware_image_test.dart:24 [UTF8-LF sha256=81f47d2e4a07740c1dab37cf55849a8a5c91eddf47631e7b631da5dad7b6ca4b; anchor=Actual migrated 2220352-byte S3 image fits]`.
- Test-code evidence: `mobile/test/documents_test.dart:64 [UTF8-LF sha256=641aee7d7a005c5c309683cf98154f571c1e3c398dd5674fc68b020807807df0; anchor=S3 document intake above 2MiB uses one bounded slot policy]`.
- Test-code evidence: `mobile/test/firmware_page_test.dart:129 [UTF8-LF sha256=a8f8dcbddecdb44e81142714c1f7874dbd99de08961b516cc3b81ab4970c885a; anchor=Contextual real firmware layout]`.
- Remaining: Issue #7: install on the authorized phone and verify actual upload/download/external OTA and OS background/reboot behavior. Package/fixture evidence is not human acceptance.

### PCController: `mobile/lib/unified/app_upgrade.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a13fdcf3c07e79544fef87b66226cc035ffc76e9dfd3841107c6b79bd0793ec0`.

- Target: `mobile/lib/app_client_management.dart:30 [UTF8-LF sha256=482a7add2e30ee6f7d4939f2d74002449721e1d0c2f86018fb19ce59c9a0ae9c; anchor=class AppClientManagement]`.
- Target: `mobile/lib/app_updates.dart:20 [UTF8-LF sha256=9e530545db419ac781b94329a940bb4e02fcd9f421405ec2766a0f520f6346e4; anchor=class AppUpdates]`.
- Target: `mobile/lib/app_update_coordinator.dart:19 [UTF8-LF sha256=91d78cccce1be01f0d6622519a562e78311d58128a29a79958e25fcaf590cd37; anchor=class AppUpdateCoordinator]`.
- Target: `mobile/lib/app_updates_page.dart:7 [UTF8-LF sha256=b39d5324451ac08cb54774eadabbf85b07bbce069cb1f63eef196492d2dedc4d; anchor=class AppUpdatesPage]`.
- Retained: Actual bounded native-channel package RPC, 16KiB ordered chunks, manifest-pinned peer pull/push, measured progress and separate local installer consent. Coordinator adds allowlisted public preferences, exact-session/origin HTTP/WS/Socket.IO offers, serialized write-ahead digest receipts and same-manifest automatic peer pulls.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/app_client_management_test.dart:41 [UTF8-LF sha256=8822963db7cc2832e639691f2e62b1d30b43d9ce0a1eeede5a10b4c91b613baf; anchor=Enrollment credentials and command receipts]`.
- Test-code evidence: `mobile/test/app_updates_test.dart:190 [UTF8-LF sha256=847413faf38f07ad3da23ed127fcf41ff796ee576430babe588c06a53a1c2a94; anchor=native bridge refuses oversized chunks]`.
- Test-code evidence: `mobile/test/app_update_coordinator_test.dart:315 [UTF8-LF sha256=210cc87ecda82de047c4cd7308d099cd75e5869d823e3ccd1f7475f1a01ab6f9; anchor=Write-ahead receipt precedes download]`.
- Test-code evidence: `mobile/test/update_pages_test.dart:93 [UTF8-LF sha256=f79717fe73ad600a6ad04b6191f6ae986a26d6e9d89ea61f26c99205421b76d3; anchor=App updates are usable offline]`.
- Test-code evidence: `mobile/test/capture_test.dart:33 [UTF8-LF sha256=c62fe3c9f6e73a0f4e0fae84925332c5dde99822f6e5680e95dc8c5dcb0cb06c; anchor=Explicit simulated renderer capture]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/lib/unified/peer_upgrades.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `97ca9e2cc9a29b2277279c4ad1ed286f959c96886c6778be2293f6f651ad9521`.

- Target: `mobile/lib/app_updates_peer.dart:32 [UTF8-LF sha256=7c2ad80e5bd0cc6de7f5053096b207c0424d7ba43f739e742ba219afc2a5de5e; anchor=class AppUpdatePeers]`.
- Target: `mobile/lib/app_update_discovery.dart:209 [UTF8-LF sha256=ac6157940d99d94c21a32e7baebe1e7d05d868fb432016f63cd2efad74fd2fc8; anchor=class AppUpdateDiscovery]`.
- Retained: Actual approved short-lived capability package-only HTTP service and foreground advertisement lifecycle, adapted to /api/rpc and verified self-package discovery. No browser-origin writes or remote install method. Same installed APK can replicate to multiple peers; each receiver stages/verifies and locally consents to installation.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/app_updates_test.dart:9 [UTF8-LF sha256=847413faf38f07ad3da23ed127fcf41ff796ee576430babe588c06a53a1c2a94; anchor=peer]`.
- Test-code evidence: `mobile/test/app_update_discovery_test.dart:168 [UTF8-LF sha256=202d5265b03db20e13768806bf617a944ddd3a70f7241d17e559c3c319bdede9; anchor=Verified same-instance routes coalesce]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `mobile/lib/unified/preferences.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `fb4552dba17b3ec1bd999b6f43a05c2d85838fc490da4caec4d0b5316e5f9239`.

- Target: `mobile/lib/pixel_diagnostic.dart:70 [UTF8-LF sha256=49b37ca6d6a14472da189800dd846acdebed7816fadb0ac22230c11d0cb2c3c6; anchor=class PixelDiagnosticWorkspace]`.
- Target: `mobile/lib/main.dart:587 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=_saveWorkspaces]`.
- Retained: Complete donor read: adapt bounded enum/preferences restore and separate phone mirroring options into the existing ordered snapshot writer. Preserve visual/volume/follow-mute preferences without restarting PCM sessions, tones or commands.
- Omitted/replaced code: `Per-keystroke sequential preference writes and PC-only branding`. Disposition: **replaced**. Reason: Use one content-equal coalesced writer, preserving the requested low-write policy and Pakar identity.
- Omitted/replaced code: `Complete donor appearance/launcher/theme options and workspace manager`. Disposition: **pending**. Reason: This task fixes lost state; unrelated useful preference surfaces require exact subsequent implementation/review, not an implicit parity claim.
- Test-code evidence: `mobile/test/workspace_lifecycle_test.dart:135 [UTF8-LF sha256=da99bd0f4eb2b7cac91106da4673cab150929a028dcc0001a675c51f8fb148a9; anchor=Front-panel drafts and diagnostic preferences]`.
- Remaining: Android installation and human acceptance remain open.

### PCController: `mobile/lib/unified/transport.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `5263044d7abac3865b6dcc8887fa6c11ad9a1c89b8c4e2374ffcd3bcc72014c5`.

- Target: `mobile/lib/pixel_diagnostic.dart:101 [UTF8-LF sha256=49b37ca6d6a14472da189800dd846acdebed7816fadb0ac22230c11d0cb2c3c6; anchor=previous.board != board || previous.boot != boot]`.
- Target: `desktop/src/ui/cell_pixels.rs:77 [UTF8-LF sha256=def5207ed7e8a9824fdb03968af731b572956a285ec0b8ff607d8329d1073aa9; anchor=pub fn observe_at(]`.
- Target: `desktop/src/ui/cell_table.rs:748 [UTF8-LF sha256=c9369637a2243a44bdc458a96b5edd738a61a63cbc929c1f25fd57241e89f241; anchor=pub(super) fn current_pixel_frame(]`.
- Target: `desktop/src/ui/shell.rs:725 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=value["pixel_preview"]]`.
- Target: `mobile/lib/pixel_diagnostic_page.dart:98 [UTF8-LF sha256=7dee854f79664e26284a634ae400ef64177d16afd31b1e20fcd499861b1f1dc1; anchor=final observedOff = observation != null]`.
- Target: `mobile/test/pixel_diagnostic_page_test.dart:79 [UTF8-LF sha256=a232eae17d69673ba4200a3d38507e6e9d5791a2d93b7ca64c27f97c1e380550; anchor=Reopening an all-off observation keeps colour edits as drafts]`.
- Target: `mobile/lib/api.dart:1183 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<DeviceStatus> status()]`.
- Target: `mobile/lib/panel_inventory.dart:7 [UTF8-LF sha256=44a5b9ab096b211cff43e42c8d2d5ccd7956572ac3c740df80fa72ee5d5f4e8b; anchor=class PanelInventory]`.
- Target: `mobile/lib/ble_codec.dart:55 [UTF8-LF sha256=763b72f5db99b3b26fb7ef004807b0cb3cb924ac0b3acf184ac52e1e4f00733c; anchor=class BleAssembler]`.
- Target: `mobile/lib/ble_transport.dart:25 [UTF8-LF sha256=1c51f1ee672365f71bed78a358e9522adcb7f26ff92055732abb48c2519b66fb; anchor=class PakarBleTransport]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/BleControllerChannel.kt:26 [UTF8-LF sha256=a0d1fb5f512da49f784d560c1f2d190adef6dd68c4ec15a5de9acd75be9d27f0; anchor=class BleControllerChannel]`.
- Target: `mobile/lib/connection_preferences.dart:54 [UTF8-LF sha256=dc20e6ec07521d724c97b793b69d36613a9c0b6e980f0184105a41e844bc5318; anchor=class ConnectionPreferences]`.
- Target: `mobile/lib/devices_page.dart:81 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=class DevicesPage]`.
- Retained: Actual PeerSession refresh single-flight and reset-before-await socket ownership adapted to shared status reads, per-socket cleanup and retained previously acknowledged layout inventory. Reads retire before and after writes; a pre/during-write result cannot settle post-ACK confirmation. Fresh individual capabilities still own actuation. The pinned http abortable GET path additionally cancels transport/body reads on deadline/retirement instead of retaining the donor Future.timeout-only limitation.
- Retained: Failure-driven review rejects older same-boot HTTP replies as superseded rather than returning cached data as a fresh observation; older WS frames cannot reset liveAge or publish into a revoked UI lease. Ordered real-loopback producer regressions retain equal board-clock captures as valid, without inventing a sequence number. Board/application-keyed panel elements retire old application mode/cue shells while retaining the user's per-board workspace; offline melody slots keep bounds and both controls disabled until actual fresh cues return. These are scoped lifecycle adaptations, not complete donor mobile parity.
- Retained: Bluetooth adaptation: fully read canonical PeerSession transport source at SHA5263044d..., retain single-flight reads, generation/route identity and reset-before-await retirement in the selected BLE API. Android native GATT is an SDK implementation because no reusable BLE transport exists in these donors. Verify INFO then full status, retain stable board aliases and saved selected route, keep app updater/remote management independent, and never substitute board HTTP/DNS/WebSocket for BLE. Existing UI/workspace, command ACK, settings and mirroring consumers are reused.
- Retained: Reread the complete current donor Rejected/Uncertain and generation-bound PeerSession through EOF. Adapt its explicit rejection versus missing-confirmation distinction to Pakar's unversioned commands: sanitized public reasons, negative2xx receipt rejection, no new-intent replay, persistent attempt-owned command failure independent of status reads, and explicit pixel-owner rebind after reconnect. This is a scoped error/lifecycle adaptation, not donor RPC/media/PWM parity.
- Retained: Reread complete donor transport through EOF, especially PeerSession._install generation-owned output invalidation. PixelDiagnosticWorkspace now retires previous board/boot observations before parsing replacement metadata while preserving cursor, colour and scroll drafts. PixelDiagnosticPage restores retained all-off presentation even when the initial semantic revision is deduplicated; colour edits after reopening all-off remain drafts until explicit Show. No main/API transport changes, command replay or new simulated product path.
- Retained: PeerSession._install same-generation presentation retention and pwmReadbackFresh live-owner conjunction reviewed for native RGB flicker. Rust now retains the last actual RGB across null/pending USB captures without renewing its timestamp; present semantic/diagnostic changes, malformed pixels, peer/boot/route and mapping changes still revoke it. Rendering depends on the current live peer rather than a shorter five-second pixel timer; command confirmation still requires fresh raw readback. A fixed RGB preview label/age tooltip and IPC diagnostics make sample age/availability observable. This adapts observation ownership, not donor PWM actuation.
- Retained: Reread canonical transport.dart through EOF at unchanged SHA5263044d. Adapt PeerSession generation-bound full-state acceptance and single-flight ownership into command admission that accepts a fresh same-board/boot WebSocket status while the joined background HTTP GET stalls. The original two-second command-clock freshness and foreground/authorization checks remain; the superseded real read socket is aborted without replay. Correct a donor limitation rather than copying it: command HTTP connection/headers and body now share the existing four-second receipt deadline, with AbortableRequest cancellation on timeout. Pre-dispatch status timeout explicitly states that no control command was sent; a dispatched write timeout remains uncertain. Twelve-second ordinary write deadlines use the same bounded helper. This is not all donor RPC/PWM/artifact or Android runtime acceptance.
- Omitted/replaced code: `Unified media/PWM/relay/RPC/artifact/feed contracts and all donor transport tests`. Disposition: **pending**. Reason: This scoped connection/ownership/retained-layout adaptation does not import the donor's other device domains, numbered compatibility aliases or certify all transports. Remaining useful generic contracts need individual source and hardware review.
- Omitted/replaced code: `Bluetooth physical and bulk-file acceptance`. Disposition: **pending**. Reason: The new adapter covers actual advertised JSON routes and does not copy media/PWM/relay or numbered protocol aliases. Android GATT/permissions and S3 RF/heap/coexistence need real-device acceptance. Direct binary firmware/raw streaming and automatic board-feed policy over BLE are not yet advertised; manual pinned remote firmware jobs remain available. OS process death can still require read-only reacquisition; preserving an Activity GATT is not an Android background service guarantee.
- Test-code evidence: `mobile/test/command_admission_transport_test.dart:24 [UTF8-LF sha256=f12cd6d07ea7da513d0b7779a8a694b6fbb54780d6e2a1f20972c89bd93b9b4a; anchor=Fresh same-boot WS admission overtakes a stalled background HTTP read]`.
- Test-code evidence: `mobile/test/api_test.dart:72 [UTF8-LF sha256=7a8f6830d333e3458b3fbdbf7ff680549f58dc2955366d2aea1b1640d0052201; anchor=A write retires]`.
- Test-code evidence: `desktop/src/ui/cell_pixels.rs:247 [UTF8-LF sha256=def5207ed7e8a9824fdb03968af731b572956a285ec0b8ff607d8329d1073aa9; anchor=fn usb_capture_time_is_not_replaced_by_chunk_completion_time()]`.
- Test-code evidence: `mobile/test/panel_inventory_test.dart:68 [UTF8-LF sha256=86f9fcd19d81e09e02191ed445c75220fa79424cd1e84314abb0283d70c1f9f0; anchor=testWidgets]`.
- Test-code evidence: `mobile/test/read_deadline_transport_test.dart:71 [UTF8-LF sha256=5a1a7076e101ece8add3c151ca5dbd3d8f81d5bd0302fca3db7da0c6be9cb8d7; anchor=stalled]`.
- Test-code evidence: `mobile/test/realtime_transport_test.dart:13 [UTF8-LF sha256=ef8a5a39abe37c40e3393cdde5b3f012dbc5a7adfc2e2cd104571ca04cd5de7e; anchor=Older same-boot WS producer]`.
- Test-code evidence: `mobile/test/selection_batch_network_test.dart:217 [UTF8-LF sha256=666769b01c6b3468061219109c98ffeaed7b83ca4866180e28afb902f84a77dc; anchor=offline slots stay stable]`.
- Test-code evidence: `mobile/test/ble_transport_test.dart:232 [UTF8-LF sha256=20e59050dfd5ad7b5474cf67841654a4bcb3bcb1e83f6bc701ce10a314c8d154; anchor=Offline BLE status and command use GATT only]`.
- Test-code evidence: `mobile/lib/command_failure.dart:34 [UTF8-LF sha256=f7b51a04b5f1bd430523303299d466e5e8117b922da9e8449b18d9ffe83f2aa1; anchor=class DeviceCommandRejected]`.
- Test-code evidence: `mobile/test/command_failure_test.dart:124 [UTF8-LF sha256=b6e282c4ee6ef999e7792ac8c4ccf57c7794c62d70cc74cae33cc59dd3dfea46; anchor=Atomic pixel $issue propagates a specific failure]`.
- Test-code evidence: `mobile/test/live_pixel_probe_test.dart:15 [UTF8-LF sha256=728143695ef5326fc95e889ff327d5d623dbd836996de051e201a2835fd44d85; anchor=Explicit live 480-pixel command and full-frame readback]`.
- Remaining: Issue #10: phone lifecycle/scroll/visual acceptance, all-client contention and additional donor transport contracts remain separate. Loopback cancellation is not Android radio/runtime proof.
- Remaining: The39 focused source/loopback/widget tests and APK build do not prove Android execution. Explicit live probes record failed WS/network/bootstrap attempts and stop on changed boot identity; no pixel success or physical acceptance is inferred. Phone pairing and a verified on-device command remain open.

### PCController: `mobile/lib/unified/workspace_chrome.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f00ed672c1f72f1a66fb65cff1cfe0a89dea5f7b6222f154db00a7c52c2e1a82`.

- Target: `mobile/lib/workspace_activity_strip.dart:7 [UTF8-LF sha256=58f77deeef8c0ca5eea12a6124e39af7d46ba1543e1c9705b033f9fae6fc50e4; anchor=class WorkspaceActivityStrip]`.
- Target: `mobile/lib/main.dart:1330 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=: WorkspaceActivityStrip(]`.
- Retained: Fully read actual ActivityStrip through EOF. Adapted stable GlobalKey targets, post-frame selected activity reveal (180ms after selection), minimum48px touch controls, semantic selected state, tooltip, reserved one-pixel border and horizontal scrolling phone navigation. Pakar uses discovered string IDs rather than fixed donor indices; the current per-board palette replaces donor per-activity hardcoded colors.
- Omitted/replaced code: `vertical activity layout and muted/per-activity color treatment`. Disposition: **pending**. Reason: Wide Pakar currently retains its existing NavigationRail. New phone navigation imports the donor horizontal implementation; a separate wide navigation adaptation is still required for full chrome parity.
- Omitted/replaced code: `Fixed Live/Channels/Panel/Studio/Devices/Firmware/Events indices`. Disposition: **replaced**. Reason: Pakar's acknowledged contextual pages use stable string IDs, including Table and Output; copying unrelated media/controller pages would invent unavailable controls.
- Test-code evidence: `mobile/test/output_panel_test.dart:13 [UTF8-LF sha256=1b7a3034588aeaa3e575881e902a1564a46c675fe2574ca38e88896a6d22e188; anchor=Phone activity navigation keeps eight panels reachable without squeezing]`.
- Remaining: Verify deployed phone navigation and selected activity reveal; complete contextual wide navigation and other workspace chrome parity separately. Source widget tests are not installed or physical acceptance.

### PCController: `mobile/test/appearance_layout_test.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `68980d69428fcfb3847cdc7bcfca15f57d2f4001fe6171aefe2552fc4bb38553`.

- Target: `mobile/test/table_layout_test.dart:216 [UTF8-LF sha256=e410115e7e5dae033596efbbd47de71c7d541aa4a9ab95334e3d15f2e2c0ac8b; anchor=for (final size in [const Size(380, 1100), const Size(1400, 1100)])]`.
- Target: `mobile/lib/table_layout.dart:9 [UTF8-LF sha256=8146736d5eefd60002468faaf321d4771a60df58b6b74ccd8f2894d313e7fcf9; anchor=({int column, int row, double x, double y}) tableCellPosition(int id)]`.
- Retained: Read the complete actual donor test file and adapted its explicit tester.view physicalSize/devicePixelRatio setup, teardown restoration, real widget coordinate assertions, exception checks and isolated fixture disposal to the Pakar table geometry tests. The donor does not define Pakar's physical 80-cell table.
- Retained: Pakar tests exercise all 80 cell bounds at phone/tablet widths in both LTR and RTL, cell1 at the upper-left origin, cell11 half below/right, alternate half-cell row offsets, no overlapping hit targets, horizontal panning, selection and preservation of an independently reversed physical LED mapping.
- Omitted/replaced code: `Appearance preference persistence, telemetry compact/roomy layout and connection toggle cases`. Disposition: **pending**. Reason: The new fixture covers the user's physical table geometry, not donor-wide appearance or connection parity. Those independent behavior cases require their own applicable Pakar tests; this review does not certify them by analogy.
- Omitted/replaced code: `Donor Locked relay action-style scenario`. Disposition: **pending**. Reason: Pakar's current TablePage has no relay control. Retain capability/lock restrictions for a future acknowledged compatible peripheral; do not invent a relay merely to reuse its widget.
- Test-code evidence: `mobile/test/table_layout_test.dart:267 [UTF8-LF sha256=e410115e7e5dae033596efbbd47de71c7d541aa4a9ab95334e3d15f2e2c0ac8b; anchor=reason: 'Cell $id must not cover cell $other or steal its taps']`.
- Remaining: Confirm the physical row pitch and alternating-versus-cumulative horizontal stagger with the user; the current preview assumes alternating half-cell horizontal offsets and 1.5-cell row pitch so square touch targets cannot overlap.
- Remaining: Install the new APK on the actual phone and obtain visual/touch acceptance; six passing layout tests and a simulated capture do not establish phone, physical table, LED mapping or donor-wide UI acceptance.

### PCController: `mobile/test/unified_transport_test.dart`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `22e8e24b6c40a006d475448a9364398aef26d322e97d71809a1cc1bb102550f9`.

- Target: `mobile/lib/api.dart:1183 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<DeviceStatus> status()]`.
- Retained: Read complete donor tests. Adapt shared refresh and retained verified inventory expectations into actual API race and widget element/scroll-position tests. Real raw TCP stalls exercise pinned HTTP cancellation separately from widget binding.
- Omitted/replaced code: `Full unified donor runtime/relay/firmware/PWM test matrix`. Disposition: **pending**. Reason: Connection-specific adaptation is not execution/import of every donor test or the corresponding device-domain implementation.
- Test-code evidence: `mobile/test/api_test.dart:42 [UTF8-LF sha256=7a8f6830d333e3458b3fbdbf7ff680549f58dc2955366d2aea1b1640d0052201; anchor=Concurrent status consumers]`.
- Test-code evidence: `mobile/test/panel_inventory_test.dart:68 [UTF8-LF sha256=86f9fcd19d81e09e02191ed445c75220fa79424cd1e84314abb0283d70c1f9f0; anchor=testWidgets]`.
- Test-code evidence: `mobile/test/read_deadline_transport_test.dart:71 [UTF8-LF sha256=5a1a7076e101ece8add3c151ca5dbd3d8f81d5bd0302fca3db7da0c6be9cb8d7; anchor=stalled]`.
- Test-code evidence: `mobile/test/realtime_transport_test.dart:13 [UTF8-LF sha256=ef8a5a39abe37c40e3393cdde5b3f012dbc5a7adfc2e2cd104571ca04cd5de7e; anchor=Older same-boot WS producer]`.
- Test-code evidence: `mobile/test/selection_batch_network_test.dart:217 [UTF8-LF sha256=666769b01c6b3468061219109c98ffeaed7b83ca4866180e28afb902f84a77dc; anchor=offline slots stay stable]`.
- Remaining: Issue #10: full donor matrix and real mobile runtime acceptance remain open.

### PCController: `Project/AddressableLeds.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `43417181b23cab40b8a58ec26159ca025fb41d5bdbaf52d20a46bfc926fee1b0`.

- Target: `libraries/BoardCore/src/CellBatch.h:9 [UTF8-LF sha256=29e1f77c397a4c530c236aa6a05598983e556b7bfc963beea0f69233ad38c8af; anchor=struct CellBatch]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:54 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=bool LedGrid::setCells(]`.
- Target: `firmware/esp8266/src/LedGrid.h:51 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=bool setCells(]`.
- Target: `libraries/BoardCore/src/TableCellMapping.h:8 [UTF8-LF sha256=8a4d0d13b7779d8a54c74bb95cafcd871814423ee42909bcd5ebbd6006213ba0; anchor=constexpr uint8_t logicalForPhysical]`.
- Target: `firmware/esp32s3/src/LedOutput.cpp:116 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=bool pending=true;]`.
- Target: `firmware/esp32s3/src/LedWire.h:29 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t zeroHigh=12]`.
- Target: `firmware/esp8266/src/LedRenderAdmission.h:7 [UTF8-LF sha256=a6dc69b69cba74db1fff54fd75b8940bf88958404f6c3ac971b27797d5b13c50; anchor=inline bool uartWindowReady(]`.
- Target: `firmware/esp8266/esp8266.ino:989 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=uint8_t defaultPhysicalCell]`.
- Target: `tools/build-firmware.ps1:20 [UTF8-LF sha256=24584210e1f306e53c78f1db8b52ed0d7765d0950eb67b4034ff41f284bb39e7; anchor=CellChaseOnBoot]`.
- Target: `firmware/esp32s3/src/LedOutput.h:13 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=#if !defined(PAKAR_S3_LED_USE_DMA)]`.
- Target: `firmware/esp32s3/src/LedDmaGuard.h:16 [UTF8-LF sha256=6418b38a2773c592ccd48dd9c2cbfaaa1ff0a8f0aab99437e68c1a7c419a2a65; anchor=class LedDmaSetupScope]`.
- Target: `firmware/esp32s3/src/LedDmaGuard.cpp:68 [UTF8-LF sha256=c2c2b90ee34d7919d22c0e4dc6c651c077b9acc0ed833090aab57e2c335e1d7e; anchor=newCacheSafeLedChannel]`.
- Target: `mobile/lib/pixel_diagnostic.dart:182 [UTF8-LF sha256=49b37ca6d6a14472da189800dd846acdebed7816fadb0ac22230c11d0cb2c3c6; anchor=class PixelProbe]`.
- Target: `mobile/lib/pixel_diagnostic_page.dart:13 [UTF8-LF sha256=7dee854f79664e26284a634ae400ef64177d16afd31b1e20fcd499861b1f1dc1; anchor=class PixelDiagnosticPage]`.
- Target: `mobile/lib/api.dart:234 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<DeviceStatus> pixelDiagnostic(]`.
- Target: `mobile/tool/pixel_probe.dart:65 [UTF8-LF sha256=b27e1a4ed0a664ef350400d452eff575463f12a975049e29ac70f3dd88d5d21a; anchor=Future<void> main(]`.
- Target: `firmware/esp8266/src/CloudAnimationProgress.h:8 [UTF8-LF sha256=97f560579ba0bb5f9172c29e92078471b6e8cbe64b5d961de2740a44cc5f714d; anchor=bool serviceDuringCloud(]`.
- Target: `desktop/src/pixel_diagnostic.rs:8 [UTF8-LF sha256=a2df3dfb52a8ebc19091060a3a8671b92ddfcf9a9da37b0b631561232e555c98; anchor=pub struct Probe]`.
- Target: `desktop/src/native_workspace.rs:48 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=pub pixel_probe:]`.
- Target: `desktop/src/ui/shell.rs:1346 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=fn send_pixel_probe(]`.
- Retained: Actual canonical complete AddressableLeds source/header read through EOF, including staged RGB versus show ownership. The selection adapter validates all80 cells before one loop-owned state mutation; no per-cell network pacing, show calls, persistence or AVR fan-out. This is a scoped adaptation of stage/show discipline, not a copied AVR timing sender.
- Retained: ESP owns synchronized300ms wall-clock interpolation and one shared published frame. Single-cell actions use the same renderer method; static fallback remains immediate. Partial/invalid selections never reach renderer state.
- Retained: Stress regression reproduced/fixed unrelated committed raw pixels reverting to stale logical targets on an abandoned partial stream. Both renderer modes and10000 full80-cell host batches per mode pass; host CPU timing is not physical throughput.
- Retained: Complete donor reread at matching raw hash. User cable pattern has a tested80-cell bijection. Board-owned logical80-to1 chase composes one six-pixel-red frame every750ms with no skip/repeat/fades. GPIO and DMA workers sleep between submitted complete frames, with startup black and renderer-owned repair refresh. Physical acceptance remains open.
- Retained: Backend selector is now a low-write application-only generated header, not a global compiler define. Plan remains read-only and both backend plans share identical SDK/compiler arguments; same canonical cache and no forced clean. SDK recompilation/cache-hit timing and physical driver acceptance remain separately measured.
- Retained: Mobile pixel diagnostics adapts clear/stage/show through the existing desktop PKAR frame format: single pixel or six aligned pixels, arbitrary RGB, bounded previous/next/direct position, full480 black on every move and All off. Same-board/boot acknowledged effects mode, one pending write, exact published RGB/revision readback and no reconnect replay; no AVR sender/timing code copied into Flutter.
- Retained: Operational diagnostic repair moves clear/stage/show into one optional pixel-diagnostic board command, no AVR fan-out or persistence. Correlated full RGB readback uses deduplicated same-ID WS/HTTP recovery; older firmware keeps the explicitly labelled raw socket path. Compact controls retain confirmed cursor on failure, expose six physical indices and verified logical labels, and keep board-keyed draft cursor/colour. Loopback/page/renderer evidence does not certify phone or optical acceptance.
- Retained: The donor stage/show ownership and startup clear are adapted to one immutable480-pixel worker mailbox. The explicit S3 RMT-DMA recovery scales/swaps logical RGB once into1440 internal bytes; IDF5.5.5's MSB-first bytes encoder fills its bounded DMA ring instead of allocating46084 expanded-symbol bytes. Wait completion and300us low latch precede buffer release; disabled startup still sends a complete black frame. Static GPIO remains separately selectable; no optical acceptance is inferred.
- Retained: Audited all1440 output bytes and the actual pinned IDF ring. RMT priority3 covers completion only; a scoped GDMA adapter now derives its exact TX source and requires measured IRQ3/core1. Same-owner rendering advances between cloud writes, without a second effects engine or optical-success claim.
- Retained: Shared diagnostic observations carry board/boot-scoped revision/index/count/RGB/off through normal status pushes. Native and mobile controls adopt newer other-client observations without writes, ignore duplicate/older observations and retain only draft cursor/colour/group. Native atomic controls now match mobile colour, single/six-pixel stepping, sectors and All off; exact readback excludes pending frames. Real two-client WS and widget fixtures are separate from installed-phone and optical acceptance.
- Omitted/replaced code: `Fixed AVR D6/PORTD assembly, 16MHz interrupt masking, MacroQueue workspace and100-pixel/8-bit strip configuration`. Disposition: **replaced**. Reason: The final480-pixel renderer belongs to the ESP32-S3 GPIO4 (or ESP8266D2), not a second AVR strip. Reusing AVR registers/timing or truncating to100 pixels would conflict with confirmed hardware and memory ownership. Existing bounded PC contract remains a separate compatibility window.
- Omitted/replaced code: `Donor native HAL showStrip and feature-profile brightness/renderedPixels buffers`. Disposition: **replaced**. Reason: The existing ESP output worker owns wire encoding/brightness and complete immutable frame copies. Host doubles test logical RGB and wire encoding separately; there is no second donor native HAL or duplicate brightness scale.
- Omitted/replaced code: `Physical channel order/timing, RMT-DMA refill latency and full-buffer optical correctness`. Disposition: **pending**. Reason: The user confirmed pixels remained lit although the static backend reported complete transmissions and all1440 published RGB bytes were zero. That is a failed physical all-off acceptance, not a successful command. The explicit RMT recovery had350/700ns highs; the earlier candidate used250/600ns highs. After reading all eight WS2811V1.4 pages, the current RMT profile uses300/625ns highs and950/625ns LOW,1250ns bits and300us reset. New optional whole-frame GPIO4 RX-DMA digital capture remains undeployed/unverified and cannot measure strip DIN voltage. A witnessed task-scoped GDMA allocation requests cache-safe refill using the already-IRAM SDK graph; the ring still refills and partition flash guards remain. Incomplete SDK symbol accounting now fails closed; this is still not wire readback. Require visual all-black, whole-cell mapping, waveform/RF/refill stress and fault recovery; counters/error0 alone are not waveform or latch proof.
- Test-code evidence: `tests/led_grid_test.cpp:179 [UTF8-LF sha256=e39aa67fd0ff4e5395249f1ee11a8237ffdeea9925065dec6638185594783ccc; anchor=Entire selection validation]`.
- Test-code evidence: `tests/led_grid_stress_test.cpp:12 [UTF8-LF sha256=812517dc4c2973487d67d4cd294c149dcb66b527fc85716363f87c723fe5fa4a; anchor=constexpr unsigned iterations=10000;]`.
- Test-code evidence: `tests/led_worker_test.cpp:315 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void functional()]`.
- Test-code evidence: `tests/led_dma_guard_test.cpp:38 [UTF8-LF sha256=d69b5b0bd26348aabd176350120d11199748f2aceada1d39c113078719fa80c0; anchor=int main(]`.
- Test-code evidence: `tools/test-led-dma-guard.ps1:14 [UTF8-LF sha256=f35494c76825ab2deccfe40759e9a7853142dc30875ffaa813f648d67de1b2e5; anchor=--wrap=gdma_new_ahb_channel]`.
- Test-code evidence: `tests/cell_chase_wiring_test.mjs:8 [UTF8-LF sha256=e071f0a389df2fd27942b8b33b7b27b24d8e19a7d327c0986c930de770529790; anchor=const preset=]`.
- Test-code evidence: `tests/build_storage_test.ps1:41 [UTF8-LF sha256=683f1926231ec42b3dacbc0ba7e2b7cb03315a375bde499bed31b2e9c1467c7a; anchor=$taskStaticS3]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_test.dart:61 [UTF8-LF sha256=ffd4ba61f030157ce0a2dbd508d266ef928b89200972f54df59df819c10ca511; anchor=Every raw position clears]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_page_test.dart:414 [UTF8-LF sha256=a232eae17d69673ba4200a3d38507e6e9d5791a2d93b7ca64c27f97c1e380550; anchor=Pending writes cannot repeat]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_transport_test.dart:97 [UTF8-LF sha256=253d183118520a978fbea6b6470cca2917fe6b45688bf6982601fbe86825f152; anchor=one complete frame, no replay]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_command_test.dart:157 [UTF8-LF sha256=dc315906d40ff502aa39343f9a17bec4af8092527bb0e34c82c1059135445173; anchor=Atomic pixel command]`.
- Test-code evidence: `tests/mobile_pixel_probe_plan_test.ps1:8 [UTF8-LF sha256=823dd963abd25514c0557bfc351dd23d24111f6de85a17461b91a7abd7f8287d; anchor=canonical compile-only plan]`.
- Test-code evidence: `tests/idf_rmt_ring_test.cpp:147 [UTF8-LF sha256=67b7f1107160e49db96f06907edb16dc540095bf4aaf6555041d49f47362cf00; anchor=void complete(]`.
- Test-code evidence: `tests/idf_rmt_ring_sdk.inc:30 [UTF8-LF sha256=21407218c9fef3c223d298d4fc2084d9ab07b15a4de77a4cb43bd25c75c43d91; anchor=static size_t rmt_encode_bytes(]`.
- Test-code evidence: `tests/idf_rmt_ring_provenance.json:2 [UTF8-LF sha256=882d1ef64d39ff9d99e303b7f997c1b7e6f11db82ae52f11c0a8b56a1075736f; anchor="sdk":]`.
- Test-code evidence: `tests/led_grid_test.cpp:50 [UTF8-LF sha256=e39aa67fd0ff4e5395249f1ee11a8237ffdeea9925065dec6638185594783ccc; anchor=void cloudAnimationProgress(]`.
- Test-code evidence: `desktop/src/pixel_diagnostic.rs:162 [UTF8-LF sha256=a2df3dfb52a8ebc19091060a3a8671b92ddfcf9a9da37b0b631561232e555c98; anchor=fn pixel_diagnostic_remote_observation_is_ordered_and_never_actuates()]`.
- Test-code evidence: `desktop/src/realtime.rs:286 [UTF8-LF sha256=56b3672d6080bde9e3b13f23426c49e3d6c57e8b3d05ec357947593644a41c83; anchor=fn realtime_pixel_diagnostic_requires_exact_complete_readback_and_typed_intent()]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_command_test.dart:16 [UTF8-LF sha256=dc315906d40ff502aa39343f9a17bec4af8092527bb0e34c82c1059135445173; anchor=Two real WS clients observe the same diagnostic]`.
- Scoped advantage: One validated atomic board intent replaces client fan-out, without duplicating the effects engine or changing API generations.
- Remaining: Physical pixel/group placement and Android phone acceptance remain open. Mobile widgets and loopback transport are separate evidence from board buffer and optical correctness; no donor-wide completion is implied.
- Remaining: User reports first23 cells working and two-second pauses every5s; old cloud service measured2552ms. Actual-IDF encoder/EOF models compare all480pixels across12descriptor windows, but fault injection proves encoded_count11521 is not waveform proof. Require deployed IRQ receipt, live timing and optical confirmation after approved OTA.

### PCController: `Project/AddressableLeds.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a28009e6211056f0cdb1071e7232d6d277a27cf9dc8e3e9951be9830745f18d4`.

- Target: `libraries/BoardCore/src/CellBatch.h:9 [UTF8-LF sha256=29e1f77c397a4c530c236aa6a05598983e556b7bfc963beea0f69233ad38c8af; anchor=struct CellBatch]`.
- Target: `firmware/esp8266/src/LedGrid.cpp:54 [UTF8-LF sha256=6dac8cda402c30e122b31fa2dad370ba80a5c0c71a0e4ecf6a3cf786b9aceaad; anchor=bool LedGrid::setCells(]`.
- Target: `firmware/esp8266/src/LedGrid.h:51 [UTF8-LF sha256=449ebd6c7a4f0422dd7d724e86c02bf5ab5df649ab05099049eb82aaf3017392; anchor=bool setCells(]`.
- Target: `libraries/BoardCore/src/TableCellMapping.h:8 [UTF8-LF sha256=8a4d0d13b7779d8a54c74bb95cafcd871814423ee42909bcd5ebbd6006213ba0; anchor=constexpr uint8_t logicalForPhysical]`.
- Target: `firmware/esp32s3/src/LedOutput.cpp:116 [UTF8-LF sha256=f362d4843264517e7b4e533cf843cde77ef5d3d6cbd1b74ff1859b8ba63be587; anchor=bool pending=true;]`.
- Target: `firmware/esp32s3/src/LedWire.h:29 [UTF8-LF sha256=502971b3109cf654ca0d8c657250a40d16548d676d1fd7ae176d5436b993d4ad; anchor=inline constexpr uint16_t zeroHigh=12]`.
- Target: `firmware/esp8266/src/LedRenderAdmission.h:7 [UTF8-LF sha256=a6dc69b69cba74db1fff54fd75b8940bf88958404f6c3ac971b27797d5b13c50; anchor=inline bool uartWindowReady(]`.
- Target: `firmware/esp8266/esp8266.ino:989 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=uint8_t defaultPhysicalCell]`.
- Target: `tools/build-firmware.ps1:20 [UTF8-LF sha256=24584210e1f306e53c78f1db8b52ed0d7765d0950eb67b4034ff41f284bb39e7; anchor=CellChaseOnBoot]`.
- Target: `firmware/esp32s3/src/LedOutput.h:13 [UTF8-LF sha256=57b7df755cce7404b0add0f2ebd2eeac99239e1aec9a08a1d4bb3f07ef78fb80; anchor=#if !defined(PAKAR_S3_LED_USE_DMA)]`.
- Target: `firmware/esp32s3/src/LedDmaGuard.h:16 [UTF8-LF sha256=6418b38a2773c592ccd48dd9c2cbfaaa1ff0a8f0aab99437e68c1a7c419a2a65; anchor=class LedDmaSetupScope]`.
- Target: `firmware/esp32s3/src/LedDmaGuard.cpp:68 [UTF8-LF sha256=c2c2b90ee34d7919d22c0e4dc6c651c077b9acc0ed833090aab57e2c335e1d7e; anchor=newCacheSafeLedChannel]`.
- Target: `mobile/lib/pixel_diagnostic.dart:182 [UTF8-LF sha256=49b37ca6d6a14472da189800dd846acdebed7816fadb0ac22230c11d0cb2c3c6; anchor=class PixelProbe]`.
- Target: `mobile/lib/pixel_diagnostic_page.dart:13 [UTF8-LF sha256=7dee854f79664e26284a634ae400ef64177d16afd31b1e20fcd499861b1f1dc1; anchor=class PixelDiagnosticPage]`.
- Target: `mobile/lib/api.dart:234 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<DeviceStatus> pixelDiagnostic(]`.
- Target: `mobile/tool/pixel_probe.dart:65 [UTF8-LF sha256=b27e1a4ed0a664ef350400d452eff575463f12a975049e29ac70f3dd88d5d21a; anchor=Future<void> main(]`.
- Target: `desktop/src/pixel_diagnostic.rs:8 [UTF8-LF sha256=a2df3dfb52a8ebc19091060a3a8671b92ddfcf9a9da37b0b631561232e555c98; anchor=pub struct Probe]`.
- Target: `desktop/src/native_workspace.rs:48 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=pub pixel_probe:]`.
- Target: `desktop/src/ui/shell.rs:1346 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=fn send_pixel_probe(]`.
- Retained: Actual canonical complete AddressableLeds source/header read through EOF, including staged RGB versus show ownership. The selection adapter validates all80 cells before one loop-owned state mutation; no per-cell network pacing, show calls, persistence or AVR fan-out. This is a scoped adaptation of stage/show discipline, not a copied AVR timing sender.
- Retained: ESP owns synchronized300ms wall-clock interpolation and one shared published frame. Single-cell actions use the same renderer method; static fallback remains immediate. Partial/invalid selections never reach renderer state.
- Retained: Stress regression reproduced/fixed unrelated committed raw pixels reverting to stale logical targets on an abandoned partial stream. Both renderer modes and10000 full80-cell host batches per mode pass; host CPU timing is not physical throughput.
- Retained: Complete donor reread at matching raw hash. User cable pattern has a tested80-cell bijection. Board-owned logical80-to1 chase composes one six-pixel-red frame every750ms with no skip/repeat/fades. GPIO and DMA workers sleep between submitted complete frames, with startup black and renderer-owned repair refresh. Physical acceptance remains open.
- Retained: Backend selector is now a low-write application-only generated header, not a global compiler define. Plan remains read-only and both backend plans share identical SDK/compiler arguments; same canonical cache and no forced clean. SDK recompilation/cache-hit timing and physical driver acceptance remain separately measured.
- Retained: Mobile pixel diagnostics adapts clear/stage/show through the existing desktop PKAR frame format: single pixel or six aligned pixels, arbitrary RGB, bounded previous/next/direct position, full480 black on every move and All off. Same-board/boot acknowledged effects mode, one pending write, exact published RGB/revision readback and no reconnect replay; no AVR sender/timing code copied into Flutter.
- Retained: Operational diagnostic repair moves clear/stage/show into one optional pixel-diagnostic board command, no AVR fan-out or persistence. Correlated full RGB readback uses deduplicated same-ID WS/HTTP recovery; older firmware keeps the explicitly labelled raw socket path. Compact controls retain confirmed cursor on failure, expose six physical indices and verified logical labels, and keep board-keyed draft cursor/colour. Loopback/page/renderer evidence does not certify phone or optical acceptance.
- Retained: The donor stage/show ownership and startup clear are adapted to one immutable480-pixel worker mailbox. The explicit S3 RMT-DMA recovery scales/swaps logical RGB once into1440 internal bytes; IDF5.5.5's MSB-first bytes encoder fills its bounded DMA ring instead of allocating46084 expanded-symbol bytes. Wait completion and300us low latch precede buffer release; disabled startup still sends a complete black frame. Static GPIO remains separately selectable; no optical acceptance is inferred.
- Retained: Shared diagnostic observations carry board/boot-scoped revision/index/count/RGB/off through normal status pushes. Native and mobile controls adopt newer other-client observations without writes, ignore duplicate/older observations and retain only draft cursor/colour/group. Native atomic controls now match mobile colour, single/six-pixel stepping, sectors and All off; exact readback excludes pending frames. Real two-client WS and widget fixtures are separate from installed-phone and optical acceptance.
- Omitted/replaced code: `Fixed AVR D6/PORTD assembly, 16MHz interrupt masking, MacroQueue workspace and100-pixel/8-bit strip configuration`. Disposition: **replaced**. Reason: The final480-pixel renderer belongs to the ESP32-S3 GPIO4 (or ESP8266D2), not a second AVR strip. Reusing AVR registers/timing or truncating to100 pixels would conflict with confirmed hardware and memory ownership. Existing bounded PC contract remains a separate compatibility window.
- Omitted/replaced code: `Donor native HAL showStrip and feature-profile brightness/renderedPixels buffers`. Disposition: **replaced**. Reason: The existing ESP output worker owns wire encoding/brightness and complete immutable frame copies. Host doubles test logical RGB and wire encoding separately; there is no second donor native HAL or duplicate brightness scale.
- Omitted/replaced code: `Physical channel order/timing, RMT-DMA refill latency and full-buffer optical correctness`. Disposition: **pending**. Reason: The user confirmed pixels remained lit although the static backend reported complete transmissions and all1440 published RGB bytes were zero. That is a failed physical all-off acceptance, not a successful command. The explicit RMT recovery had350/700ns highs; the earlier candidate used250/600ns highs. After reading all eight WS2811V1.4 pages, the current RMT profile uses300/625ns highs and950/625ns LOW,1250ns bits and300us reset. New optional whole-frame GPIO4 RX-DMA digital capture remains undeployed/unverified and cannot measure strip DIN voltage. A witnessed task-scoped GDMA allocation requests cache-safe refill using the already-IRAM SDK graph; the ring still refills and partition flash guards remain. Incomplete SDK symbol accounting now fails closed; this is still not wire readback. Require visual all-black, whole-cell mapping, waveform/RF/refill stress and fault recovery; counters/error0 alone are not waveform or latch proof.
- Test-code evidence: `tests/led_grid_test.cpp:179 [UTF8-LF sha256=e39aa67fd0ff4e5395249f1ee11a8237ffdeea9925065dec6638185594783ccc; anchor=Entire selection validation]`.
- Test-code evidence: `tests/led_grid_stress_test.cpp:12 [UTF8-LF sha256=812517dc4c2973487d67d4cd294c149dcb66b527fc85716363f87c723fe5fa4a; anchor=constexpr unsigned iterations=10000;]`.
- Test-code evidence: `tests/led_worker_test.cpp:315 [UTF8-LF sha256=878091cb9945be9030df4da9b48b7135e5e3c39ed0c0db6dbfcf5e062fa9ea85; anchor=void functional()]`.
- Test-code evidence: `tests/led_dma_guard_test.cpp:38 [UTF8-LF sha256=d69b5b0bd26348aabd176350120d11199748f2aceada1d39c113078719fa80c0; anchor=int main(]`.
- Test-code evidence: `tools/test-led-dma-guard.ps1:14 [UTF8-LF sha256=f35494c76825ab2deccfe40759e9a7853142dc30875ffaa813f648d67de1b2e5; anchor=--wrap=gdma_new_ahb_channel]`.
- Test-code evidence: `tests/cell_chase_wiring_test.mjs:8 [UTF8-LF sha256=e071f0a389df2fd27942b8b33b7b27b24d8e19a7d327c0986c930de770529790; anchor=const preset=]`.
- Test-code evidence: `tests/build_storage_test.ps1:41 [UTF8-LF sha256=683f1926231ec42b3dacbc0ba7e2b7cb03315a375bde499bed31b2e9c1467c7a; anchor=$taskStaticS3]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_test.dart:61 [UTF8-LF sha256=ffd4ba61f030157ce0a2dbd508d266ef928b89200972f54df59df819c10ca511; anchor=Every raw position clears]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_page_test.dart:414 [UTF8-LF sha256=a232eae17d69673ba4200a3d38507e6e9d5791a2d93b7ca64c27f97c1e380550; anchor=Pending writes cannot repeat]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_transport_test.dart:97 [UTF8-LF sha256=253d183118520a978fbea6b6470cca2917fe6b45688bf6982601fbe86825f152; anchor=one complete frame, no replay]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_command_test.dart:157 [UTF8-LF sha256=dc315906d40ff502aa39343f9a17bec4af8092527bb0e34c82c1059135445173; anchor=Atomic pixel command]`.
- Test-code evidence: `tests/mobile_pixel_probe_plan_test.ps1:8 [UTF8-LF sha256=823dd963abd25514c0557bfc351dd23d24111f6de85a17461b91a7abd7f8287d; anchor=canonical compile-only plan]`.
- Test-code evidence: `desktop/src/pixel_diagnostic.rs:162 [UTF8-LF sha256=a2df3dfb52a8ebc19091060a3a8671b92ddfcf9a9da37b0b631561232e555c98; anchor=fn pixel_diagnostic_remote_observation_is_ordered_and_never_actuates()]`.
- Test-code evidence: `desktop/src/realtime.rs:286 [UTF8-LF sha256=56b3672d6080bde9e3b13f23426c49e3d6c57e8b3d05ec357947593644a41c83; anchor=fn realtime_pixel_diagnostic_requires_exact_complete_readback_and_typed_intent()]`.
- Test-code evidence: `mobile/test/pixel_diagnostic_command_test.dart:16 [UTF8-LF sha256=dc315906d40ff502aa39343f9a17bec4af8092527bb0e34c82c1059135445173; anchor=Two real WS clients observe the same diagnostic]`.
- Scoped advantage: One validated atomic board intent replaces client fan-out, without duplicating the effects engine or changing API generations.
- Remaining: Physical pixel/group placement and Android phone acceptance remain open. Mobile widgets and loopback transport are separate evidence from board buffer and optical correctness; no donor-wide completion is implied.

### PCController: `Project/AutomationStore.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `09abbdbfe7fb317e7a690bb5783fe5eaca3809bb24fb0cbc8b6baf7bcc06dc5c`.

- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.cpp:223 [UTF8-LF sha256=22e9cdf2018d0247ef1c6192b50c8fb6f991ccbaecfdcbf07a00a76fe1756223; anchor=bool AutomationStore::commit(]`.
- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.cpp:310 [UTF8-LF sha256=22e9cdf2018d0247ef1c6192b50c8fb6f991ccbaecfdcbf07a00a76fe1756223; anchor=uint8_t AutomationExecutor::dispatch(]`.
- Retained: Complete actual donor implementation/header and dependency EEPROM ownership layout were read, together with complete current target and source adaptation diffs. Retains dual-bank committed-generation selection, per-record/header CRC, stable twelve-byte UART record encoding, mutation validation, ascending-slot executor, event/window budgets and recursion protection.
- Retained: Adapted actual CRC calls to shared WireCodec, supports all nine physical keys, leaves blank EEPROM volatile until an explicit edit and verifies committed bank readback before replacing active state. Architecture guard preserves this AVR EEPROM owner and native fixtures while excluding ESP8266/ESP32 transport proxies; no ESP EEPROM emulation or duplicated persistent owner.
- Omitted/replaced code: `Immediate clear/write when both EEPROM banks are invalid; publishing active generation before verifying committed bank`. Disposition: **replaced**. Reason: Pakar retains a volatile empty state until an explicit user mutation and verifies the actual inactive-bank commit before promoting it. This preserves existing EEPROM and allows failure reporting instead of falsely accepting a torn write.
- Omitted/replaced code: `Four-key eventValue bound and donor UART CRC namespace`. Disposition: **replaced**. Reason: This board has nine confirmed inputs; valid key IDs0..8 replace donor0..3. Shared WireCodec provides the same CRC algorithm without importing a second UART protocol engine.
- Omitted/replaced code: `Compiling AVR EEPROM owner into ESP transport targets`. Disposition: **replaced**. Reason: Both ESP architectures proxy PCController operations to the AVR. The observed S3 SDK EEPROM.update error exposed an overly narrow ESP8266-only guard; the guard now excludes both families and their architecture macros. AVR/host fixture code remains intact.
- Test-code evidence: `tests/pc_contract_test.cpp:144 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=void macroAndAutomations() {]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:69 [UTF8-LF sha256=538f7c02dbd9184c0e500ea65bb14f7be924ef98d8f5a88b0e26c58938be534a; anchor=assert.match(automationStore,]`.
- Remaining: Run exact-source AVR firmware and integrated ESP32-S3/ESP8266 builds, physical automation/EEPROM readback and interruption tests on the actual board. Existing host fixtures/source syntax checks are not physical EEPROM, power-loss, actuator or integrated SDK acceptance.

### PCController: `Project/AutomationStore.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `adc48b05b237bac5093e748e820d06515efe991e4fef2a696dbdbecd262f658a`.

- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.h:63 [UTF8-LF sha256=881d09b14a807e8907209e6e14aa87d630313051180f01975b88d0c21b64d98f; anchor=class AutomationStore {]`.
- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.h:141 [UTF8-LF sha256=881d09b14a807e8907209e6e14aa87d630313051180f01975b88d0c21b64d98f; anchor=class AutomationExecutor {]`.
- Retained: Complete actual donor/current header read. Retains event/action enums and allowed flags, packed twelve-byte UART record static assertion, compact stored record/header layouts, dual-bank API, executor budgets and explicit action callback. Adds initialized_ to represent volatile blank storage truthfully.
- Omitted/replaced code: `Unrelated ProjectConfig include and donor RF/reset/status-profile EEPROM allocation`. Disposition: **replaced**. Reason: Pakar uses its existing explicit disjoint settings/automation/Prog layout, not the donor's other-board persistent image. The new architecture exclusion applies to the implementation, not the portable record contract; optional hardware actions remain gated by actual runtime capabilities.
- Test-code evidence: `tests/pc_contract_test.cpp:162 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=AutomationRecord rec{]`.
- Remaining: Run exact-source AVR firmware and integrated ESP32-S3/ESP8266 builds, physical automation/EEPROM readback and interruption tests on the actual board. Existing host fixtures/source syntax checks are not physical EEPROM, power-loss, actuator or integrated SDK acceptance.

### PCController: `Project/BootMelody.cpp`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a0bc9d58746804186b58a34a6e553a84e33325050b54f59199150f67ba101881`.

- Target: `firmware/avr/BootMelody.cpp:3 [UTF8-LF sha256=f163fcef8c0647ce0f0696c219b5d178e8815ff3df82bf2c5dec6c5c11400433; anchor=void playBootMelody]`.
- Target: `libraries/BoardCore/src/AudioCues.h:22 [UTF8-LF sha256=e63a6df1db8d7183cca16be68b56e60d6996f6c73670b430249233022a00b2cd; anchor=case Protocol::kCueAttention:]`.
- Retained: Actual b0111ae0 header/implementation read through EOF; preserve the exact 1032/2010/2400Hz 70/60/70/60/120/150ms nonblocking welcome schedule and replace-current-queue semantics. The existing Timer1 player also owns additive finite attention/update melodies; no delay loop, boot replay or frequency approximation introduced.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/tone_player_test.cpp:55 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const uint16_t durations[kCueCount]]`.
- Test-code evidence: `tests/attention_cues_test.cpp:31 [UTF8-LF sha256=2b6bd229d5c9b9c5bdc233b97b88ca4d3422e34fad2dcb20dbb11db46732b3b0; anchor=Finite update notices:]`.
- Scoped advantage: Existing deadline-preserving player retains donor welcome while exposing explicit ACK-gated finite attention without blocking ESP services.
- Remaining: Live AVR/phone frequency/cadence, Silent/Prog and connected S3 update acceptance remain open; ASUS SSH8022 refused. No source fixture certifies physical acoustics.

### PCController: `Project/BootMelody.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `61a3a28c977fcac58aa0fe8f7f4cdf224ac12375ffd0b791841925ad5bf2325f`.

- Target: `firmware/avr/BootMelody.h:8 [UTF8-LF sha256=241996ca436e5336a8cb878b468069d2574f87b29edfa99b4f6115d1183f0af4; anchor=void playBootMelody]`.
- Target: `libraries/BoardCore/src/AudioCues.h:22 [UTF8-LF sha256=e63a6df1db8d7183cca16be68b56e60d6996f6c73670b430249233022a00b2cd; anchor=case Protocol::kCueAttention:]`.
- Retained: Actual b0111ae0 header/implementation read through EOF; preserve the exact 1032/2010/2400Hz 70/60/70/60/120/150ms nonblocking welcome schedule and replace-current-queue semantics. The existing Timer1 player also owns additive finite attention/update melodies; no delay loop, boot replay or frequency approximation introduced.
- Omitted/replaced code: `../LocalLib/TonePlayer.h include path`. Disposition: **replaced**. Reason: Pakar Arduino sketch is flat; use its existing TonePlayer.h without copying another hardware owner.
- Test-code evidence: `tests/tone_player_test.cpp:55 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const uint16_t durations[kCueCount]]`.
- Test-code evidence: `tests/attention_cues_test.cpp:31 [UTF8-LF sha256=2b6bd229d5c9b9c5bdc233b97b88ca4d3422e34fad2dcb20dbb11db46732b3b0; anchor=Finite update notices:]`.
- Scoped advantage: Existing deadline-preserving player retains donor welcome while exposing explicit ACK-gated finite attention without blocking ESP services.
- Remaining: Live AVR/phone frequency/cadence, Silent/Prog and connected S3 update acceptance remain open; ASUS SSH8022 refused. No source fixture certifies physical acoustics.

### PCController: `Project/CompactI2c.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `75d2ec54409851ae84499c3a4d42ae485284c966abdd323c0b755ca7d5f09e2c`.

- Target: `libraries/BoardCore/src/LcdDisplay.h:10 [UTF8-LF sha256=93bad5e225c75ff7604e9dd31db8564a518ed53d56a721a81d7f143b31b87d1e; anchor=class LcdDisplay]`.
- Target: `firmware/esp32s3/src/LcdPort.cpp:3 [UTF8-LF sha256=97830b8c55a9a5214edfd537ab8426696f79625ab7cf30183b14066b19ef9c94; anchor=void LcdPort::begin(]`.
- Target: `firmware/esp8266/src/LcdManagement.h:3 [UTF8-LF sha256=eb800ee4c334b22504f37bd841bda58a7562052df2c7226ff19bc23a454e652d; anchor=String lcdStatusJson()]`.
- Target: `desktop/src/display.rs:7 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=pub struct LcdObservation]`.
- Target: `mobile/lib/peer_context.dart:180 [UTF8-LF sha256=5f520bd21d49ac1460fd3cbd24f4713ccddce0168d78046774a19a181c68962d; anchor=bool lcdObservationReady(]`.
- Target: `firmware/esp8266/src/WebUi.h:810 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function lcdObservation(]`.
- Retained: Complete actual donor source read through EOF. Preserved bounded transaction results, explicit master setup and timeout policy through the real S3 Wire1 adapter at100kHz/5ms transaction timeout. Transport interface isolates controller-specific registers instead of pretending AVR TWI can control an ESP32-S3 bus.
- Omitted/replaced code: `AVR TWI register implementation, native AVR bridge receive buffer and host-proxied arbitrary I2C operations`. Disposition: **replaced**. Reason: The S3 uses actual native hardware I2C on separate pins, not the AVR connector already carrying non-I2C TM1637. A private adapter has no public arbitrary-bus mutation path. Copying AVR registers or enabling Wire on AVR A4/A5 would steal the display pins.
- Omitted/replaced code: `Flash-string overload and LCD clear/backlight/generic bus controls on all client surfaces; host-owned LCD prompt/priority/offline policy`. Disposition: **pending**. Reason: This slice implements standalone physical discovery/init/default cell/update text, finite REST presentation and exact completed-frame previews. It does not claim the full donor host presenter, raw I2C bridge, every app editor or USB LCD mutation parity. CompactI2c remains available for a future explicit nonconflicting AVR wiring/build profile after flash/RAM review.
- Omitted/replaced code: `Unrestricted address scan, geometry and backpack pinout inference`. Disposition: **replaced**. Reason: Retain donor16x2 and explicit common backpack mapping/addresses only. A generic expander ACK cannot identify LCD glass, geometry, contrast or optical success; no broad scanner or unsupported OLED/TFT/20x4 capability is advertised.
- Test-code evidence: `tests/lcd_display_test.cpp:47 [UTF8-LF sha256=69c315bd5b749166cf69088e9dc8dea2f1610e11ef2134877c17140b6f68ac37; anchor=for(unsigned position=1;position<=246;]`.
- Test-code evidence: `tests/lcd_web_test.mjs:12 [UTF8-LF sha256=6abf8aedbb70f02cdfae0adfc7511aa9e3651cbdc1c22e885bdfca92bf79bdfb; anchor=for(const key of []`.
- Test-code evidence: `mobile/test/lcd_observation_test.dart:6 [UTF8-LF sha256=3eef7afae9cc8304b33e32afe6c56f51996193e89a618bbfce345804c589e593; anchor=LCD preview requires its own complete]`.
- Test-code evidence: `desktop/src/display.rs:34 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=fn lcd_requires_backpack_initialization_and_complete_current_frame()]`.
- Test-code evidence: `tests/lcd_management_test.cpp:72 [UTF8-LF sha256=d1080e4369cdb3dddd4dc2b3966c0ca0aa2fda58f134865d381c940c4cb50441; anchor=Actual LCD HTTP handlers/status:]`.
- Scoped advantage: Checked incremental initialization and whole-frame admission replace unconditional readiness; display availability is distinct from bus ACK and from physical visibility.
- Remaining: Actual module/power/pull-up safety, deployed S3 I2C and AVR TM1637 ACK/frame presentation, unplug/replug/reboot, content/contrast and simultaneous LED/W5500 traffic acceptance remain open. ASUS SSH8022 refuses; no hardware actuation or optical claim. Direct AVR A4/A5 LCD substitution is NOT enabled while those pins own TM1637. LCD REST controls and previews exist; full native/mobile/USB LCD editors and donor-wide host presentation policy remain pending.

### PCController: `Project/CompactI2c.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `54cad57a41a6f5ef80f8117b8bf2bad8121330565a52ec202482b6c8150deef6`.

- Target: `libraries/BoardCore/src/LcdDisplay.h:10 [UTF8-LF sha256=93bad5e225c75ff7604e9dd31db8564a518ed53d56a721a81d7f143b31b87d1e; anchor=class LcdDisplay]`.
- Target: `firmware/esp32s3/src/LcdPort.cpp:3 [UTF8-LF sha256=97830b8c55a9a5214edfd537ab8426696f79625ab7cf30183b14066b19ef9c94; anchor=void LcdPort::begin(]`.
- Target: `firmware/esp8266/src/LcdManagement.h:3 [UTF8-LF sha256=eb800ee4c334b22504f37bd841bda58a7562052df2c7226ff19bc23a454e652d; anchor=String lcdStatusJson()]`.
- Target: `desktop/src/display.rs:7 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=pub struct LcdObservation]`.
- Target: `mobile/lib/peer_context.dart:180 [UTF8-LF sha256=5f520bd21d49ac1460fd3cbd24f4713ccddce0168d78046774a19a181c68962d; anchor=bool lcdObservationReady(]`.
- Target: `firmware/esp8266/src/WebUi.h:810 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function lcdObservation(]`.
- Retained: Complete actual donor source read through EOF. Preserved bounded transaction results, explicit master setup and timeout policy through the real S3 Wire1 adapter at100kHz/5ms transaction timeout. Transport interface isolates controller-specific registers instead of pretending AVR TWI can control an ESP32-S3 bus.
- Omitted/replaced code: `AVR TWI register implementation, native AVR bridge receive buffer and host-proxied arbitrary I2C operations`. Disposition: **replaced**. Reason: The S3 uses actual native hardware I2C on separate pins, not the AVR connector already carrying non-I2C TM1637. A private adapter has no public arbitrary-bus mutation path. Copying AVR registers or enabling Wire on AVR A4/A5 would steal the display pins.
- Omitted/replaced code: `Flash-string overload and LCD clear/backlight/generic bus controls on all client surfaces; host-owned LCD prompt/priority/offline policy`. Disposition: **pending**. Reason: This slice implements standalone physical discovery/init/default cell/update text, finite REST presentation and exact completed-frame previews. It does not claim the full donor host presenter, raw I2C bridge, every app editor or USB LCD mutation parity. CompactI2c remains available for a future explicit nonconflicting AVR wiring/build profile after flash/RAM review.
- Omitted/replaced code: `Unrestricted address scan, geometry and backpack pinout inference`. Disposition: **replaced**. Reason: Retain donor16x2 and explicit common backpack mapping/addresses only. A generic expander ACK cannot identify LCD glass, geometry, contrast or optical success; no broad scanner or unsupported OLED/TFT/20x4 capability is advertised.
- Test-code evidence: `tests/lcd_display_test.cpp:47 [UTF8-LF sha256=69c315bd5b749166cf69088e9dc8dea2f1610e11ef2134877c17140b6f68ac37; anchor=for(unsigned position=1;position<=246;]`.
- Test-code evidence: `tests/lcd_web_test.mjs:12 [UTF8-LF sha256=6abf8aedbb70f02cdfae0adfc7511aa9e3651cbdc1c22e885bdfca92bf79bdfb; anchor=for(const key of []`.
- Test-code evidence: `mobile/test/lcd_observation_test.dart:6 [UTF8-LF sha256=3eef7afae9cc8304b33e32afe6c56f51996193e89a618bbfce345804c589e593; anchor=LCD preview requires its own complete]`.
- Test-code evidence: `desktop/src/display.rs:34 [UTF8-LF sha256=e623569692f080bb41e7dbd148fc657217d387853d1a0c28280a812f37af819c; anchor=fn lcd_requires_backpack_initialization_and_complete_current_frame()]`.
- Test-code evidence: `tests/lcd_management_test.cpp:72 [UTF8-LF sha256=d1080e4369cdb3dddd4dc2b3966c0ca0aa2fda58f134865d381c940c4cb50441; anchor=Actual LCD HTTP handlers/status:]`.
- Scoped advantage: Checked incremental initialization and whole-frame admission replace unconditional readiness; display availability is distinct from bus ACK and from physical visibility.
- Remaining: Actual module/power/pull-up safety, deployed S3 I2C and AVR TM1637 ACK/frame presentation, unplug/replug/reboot, content/contrast and simultaneous LED/W5500 traffic acceptance remain open. ASUS SSH8022 refuses; no hardware actuation or optical claim. Direct AVR A4/A5 LCD substitution is NOT enabled while those pins own TM1637. LCD REST controls and previews exist; full native/mobile/USB LCD editors and donor-wide host presentation policy remain pending.

### PCController: `Project/ResetTelemetry.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `8699f6cb75170fb62ee94e24ce0faa74bdfb51021b9eba6fb95c61013619199c`.

- Target: `firmware/avr/ResetTelemetry.cpp:26 [UTF8-LF sha256=402fae6bb17c44bf5263df8e17f08f77e82a05706c1fbffd900dd6bf8fcb76bd; anchor=void capturePakarResetCause()]`.
- Retained: Actual .init3/.noinit MCUSR capture, Urboot r2 fallback, clear/reset-watchdog order, CRC/commit-last 64-slot boot journal and signed generation ordering imported from donor b0111ae0; complete donor header, EEPROM layout, codec and reset fixture also read.
- Retained: Pakar adds idempotent initialization, EEPROM commit readback and automatic welcome suppression for watchdog, brownout and unknown reset causes. Existing telemetry remains 46 bytes; optional B5 carries cause, boot count and readback-valid state.
- Omitted/replaced code: `Donor EEPROM journal placement and UartProtocol CRC dependency`. Disposition: **replaced**. Reason: Pakar settings/automation occupy bytes32..411 and Prog starts1008. Reserve416..799 with static bounds and use the existing identical shared CRC rather than copying the donor RF/EEPROM layout or a second protocol engine.
- Omitted/replaced code: `Donor host-platform NativeHal reset source`. Disposition: **not-applicable**. Reason: This service targets the ATmega328P. Host fixtures inject causes explicitly; no native host reset observation is represented as MCU data.
- Test-code evidence: `tests/avr_reset_telemetry_test.cpp:41 [UTF8-LF sha256=cbd986325dfbaf5583a4964a50ef873dbe7a2798ff70190571f88a152f33585a; anchor=for(int cut=0;cut<7;++cut)]`.
- Remaining: Verify actual early startup/Urboot disassembly, ISP recovery and live UART reset-cause/count readback; EEPROM doubles and an SDK build are not physical power, reset-loop or healthy-board acceptance.

### PCController: `Project/ResetTelemetry.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7ad43276b93bc31248848ad80cb4c2f5f40500dd33bf736ddeb6c2d5114256a9`.

- Target: `firmware/avr/ResetTelemetry.h:7 [UTF8-LF sha256=67232d226c8066dbe6fe7e5a91ccde087d024007e84692c513b583333489f44e; anchor=class ResetTelemetry]`.
- Retained: Actual single MCU-owned reset service, cause() and count() interface retained; adds persisted() and a bounded welcome-reset policy without moving EEPROM work into early startup.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/avr_reset_telemetry_test.cpp:27 [UTF8-LF sha256=cbd986325dfbaf5583a4964a50ef873dbe7a2798ff70190571f88a152f33585a; anchor=first.begin();assert(first.count()==1 && EEPROM.writes==writes);]`.
- Remaining: Live board reset history and quiet recovery acceptance remain unverified until ASUS and the connected hardware are reachable.

### PCController: `Project/Runtime/FrontPanelRuntime.inc.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4b61348838f59e54efec329f8c9ba9dc3239e96139da114801a04a46867a78ed`.

- Target: `firmware/avr/FrontPanel.cpp:252 [UTF8-LF sha256=d90acc624b6d9e9248238eb5015a573bb3081bde5546b9bdc3d1e14661f3dea4; anchor=void FrontPanel::service(]`.
- Target: `firmware/avr/PcControllerRuntime.cpp:446 [UTF8-LF sha256=cd62f7447553664a06de9b55bdb18371d26972b63833d80035e795d367669694; anchor=espTouchedAt_]`.
- Target: `firmware/esp8266/src/SemanticPanel.h:29 [UTF8-LF sha256=8cd5384e86c8201e42d6d80b96eacd84668bcb3a7ee88bacef5addb4be1bc958; anchor=class Presenter]`.
- Target: `firmware/esp8266/src/SemanticPanelService.h:56 [UTF8-LF sha256=eb57a4681c9238663102707b17c1005ba2479e359bde8e4c544b6ccfee02abd2; anchor=void serviceSemanticPanel(]`.
- Retained: Actual complete1379-line donor read through EOF at b0111ae0c80dc34149ea1044de7dd5488920301b. Adapts holdMenuLabel650ms, label/value cadence, finite flash-message expiry, editor/programming priority and showHostSegmentText one-pass terminal-blank/explicit repeat behavior. Confirmed A4/A5 TM1637 mapping and explicit capture remain unchanged.
- Retained: A bounded loop-owned semantic presenter retains cell/pixel/effect/winner/output context, timed notices, one-pass marquee, slow/double-fast blink and measured update progress. Requested WS/USB events remain separate from actual AVR segment/ACK observations. Periodic offers do not renew the15-second physical ESP-menu inactivity lease.
- Omitted/replaced code: `relay/motion/door/BT/RF/sensor/PWM-specific display pages and EEPROM editors`. Disposition: **pending**. Reason: Read but not fitted or individually acknowledged on this table. Safety-sensitive actuator/sensor behavior remains capability-gated and needs separate matching-hardware review; no full editor/automation parity is claimed.
- Omitted/replaced code: `full generic EEPROM menu hierarchy/order/visibility and transactional save/discard parity`. Disposition: **pending**. Reason: Retaining current table owners and adapting display arbitration does not import every generic donor transaction; useful remaining behavior remains issue6 work, not an accepted exclusion.
- Test-code evidence: `tests/front_panel_test.cpp:42 [UTF8-LF sha256=318ba761cdc2548b25e45b9114480ff761b6e994f21e0ca3a15ce19c20e2533a; anchor=int main()]`.
- Test-code evidence: `tests/esp_front_panel_test.cpp:60 [UTF8-LF sha256=58220f795fcb6d16277308285f37f7cede5af7f431ceafd675ad8bfe6f3c5013; anchor=int main()]`.
- Test-code evidence: `tests/semantic_panel_test.cpp:10 [UTF8-LF sha256=2b28c99ae967228e678d9bca3332b051f7727008b46d74163d4873c6df619d9c; anchor=int main()]`.
- Remaining: Verify affected policy/AVR/client builds and actual TM1637 notice readability, update priority, manual/capture ownership and expiry after coordinated delivery. Source fixtures, enqueue/ACK and raw masks do not establish physical visual acceptance.

### PCController: `Project/SettingsStore.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7309fd36f0680722a4260e8f1251ae5a0fc67e99fc60f2426bb1294f789e87d4`.

- Target: `firmware/esp8266/esp8266.ino:1075 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=void serviceScenePersistence(bool flush)]`.
- Target: `firmware/esp8266/src/LedPreferences.h:29 [UTF8-LF sha256=303c1469003fec6fc2777ce459c1fae872c73d67241871463b7a32135bd51aac; anchor=inline bool applyLedBootPreferences(]`.
- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Target: `desktop/src/ui/device_settings.rs:35 [UTF8-LF sha256=43c0eb207de03c597ad69d10671655569ee1bc25ba16298869bc65c742d710db; anchor=fn avr_page(]`.
- Target: `desktop/src/device_settings.rs:498 [UTF8-LF sha256=9e62958ec6114027601efe1f4a4ea045d1fb665ce7ab96b8bf78a3fc744f065b; anchor=pub fn finish_avr(]`.
- Target: `firmware/esp8266/src/SceneStore.h:17 [UTF8-LF sha256=87e7cd891c154505d7febfc69c2ea78b6c9d92eb1e110dcbb34779e62eb00907; anchor=template <class Filesystem> class SceneStore]`.
- Target: `firmware/esp8266/src/LedScene.h:5 [UTF8-LF sha256=c26330789f8cedb1cf0caa3e525a46f2359d07477baa363a2bdbefe9a5349654; anchor=inline SceneState captureLedScene]`.
- Target: `firmware/esp8266/src/AvrSceneSync.h:5 [UTF8-LF sha256=1290d8c0e691b767574890002086d80fd4a41bd595b5349e691979bd50694a3f; anchor=class AvrSceneSync]`.
- Retained: Reread the complete matching SettingsStore.cpp/header. Adapt actual markDirty/service1500ms/unchanged-byte wear reduction to ESP-owned SceneStore, with a bounded10s maximum edit delay and10s failed-write retry cooldown. The new private96-byte record stores all80 logical white/red/off states, mode/effect, brightness, output/static selection and front-panel cursor/winner. No animation, stream or chase frame becomes a per-frame flash write.
- Retained: Replace the previous white-every-boot policy with stage/close/exact-readback/atomic-rename scene persistence. Read the old6-byte brightness record only for migration; it remains untouched. Restore the validated scene and mapping before first LED publication, keep safe white initial defaults if no valid scene, and expose pending/saved/error/write counters. AVR reacquisition and its SetMode-induced reset revision are baselines, not new Reset/key actions. App caches never restore hardware authority or replay queued writes.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Retained: Desktop Device Settings now edits every supported Pakar AVR record field in one explicit draft: Silent/welcome/remember-page flags, display brightness, ADC6 threshold, telemetry period, short AVR name, visible/default pages and packed page order. Preserve the donor MCU ownership, validation and EEPROM wear/no-op rules, not its incompatible relay/motion/temperature record. USB and fenced HTTP share one GET/SET/GET transaction; compare the fresh complete baseline before SET, require exact ACK and persisted byte-for-byte readback, and reject stale peer/capability/programming owners without replay.
- Retained: Actual production scene owner now uses the donor dirty/deferred settings mechanism, separate from AVR EEPROM. Host filesystem faults, first-frame mapped restore and baseline-only AVR resynchronization are explicitly exercised.
- Omitted/replaced code: `Donor AVR EEPROM address/layout,14-page motion-alias normalization, relay/PWM/motion/sensor restore and temperature/name records`. Disposition: **replaced**. Reason: LED preferences live in ESP LittleFS, not the donor AVR record. Copying the address/layout or actuator settings would collide with the separate Pakar AVR owner and imply absent hardware. Applicable board settings have an independent historical manifest; this narrow mapping neither replaces it nor certifies those full current-source contracts.
- Omitted/replaced code: `Donor EEPROM record ownership and complete unrelated settings normalization`. Disposition: **replaced**. Reason: The ESP scene now has its own validated bounded record,1500ms dirty coalescing,10s maximum delay, changed-record skipping and failure backoff. AVR peripheral/menu settings retain their separate EEPROM owner; copying relay/PWM/sensor layout would corrupt unrelated hardware contracts.
- Omitted/replaced code: `Physical interrupted flash/power-loss durability and optical restore acceptance`. Disposition: **pending**. Reason: Actual production SceneStore fixtures now cover stage/open/short-write/close-corruption/read/rename failures, exact unchanged/frame write counts, all record-byte corruption, rollover and reboot roundtrip. These filesystem doubles and atomic LittleFS rename do not prove physical abrupt-power flash durability or emitted colors.
- Test-code evidence: `tests/led_grid_test.cpp:105 [UTF8-LF sha256=e39aa67fd0ff4e5395249f1ee11a8237ffdeea9925065dec6638185594783ccc; anchor=Boot policy accepts existing files without restoring a saved animation]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:120 [UTF8-LF sha256=538f7c02dbd9184c0e500ea65bb14f7be924ef98d8f5a88b0e26c58938be534a; anchor=sceneStore.begin]`.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Test-code evidence: `desktop/src/board_silent.rs:213 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_settings_full_apply_requires_fresh_baseline_ack_and_exact_persistent_readback()]`.
- Test-code evidence: `desktop/src/board_silent.rs:178 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_settings_require_each_changed_hardware_capability_and_no_programming_latch()]`.
- Test-code evidence: `desktop/src/device_settings.rs:658 [UTF8-LF sha256=9e62958ec6114027601efe1f4a4ea045d1fb665ce7ab96b8bf78a3fc744f065b; anchor=fn avr_editor_owns_dirty_draft_pending_receipt_and_same_peer_readback()]`.
- Test-code evidence: `tests/scene_store_test.cpp:106 [UTF8-LF sha256=c194c7ae984cbadea39c4fb96219fdbb9a9284bc55dd737c1314812f7f7f1046; anchor=Continuous edits have bounded save delay]`.
- Scoped advantage: Verified semantic scene restore replaces unconditional white boot; safe initial white and read-only legacy brightness migration remain. Animation/stream bytes never become per-frame persistence writes.
- Remaining: Verify guarded SDK builds and installed scene/mode/output restore across actual reboot. Physical abrupt-power interruption and optical success remain separate from host filesystem failure fixtures.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.
- Remaining: Native full-record editor is source-only pending installed UI/physical AVR USB and Wi-Fi readback/reboot acceptance. Flutter and embedded Web still expose only the earlier Silent setting plus live-control links, not this full AVR field editor. The pre-write baseline comparison is optimistic, not an atomic multi-client CAS contract.

### PCController: `Project/SettingsStore.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `ccdcfb6e1ab186abd6a4f0b5a302bb14bab295c9a0bd9b4a306036fa32f1eaf3`.

- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Target: `desktop/src/ui/device_settings.rs:35 [UTF8-LF sha256=43c0eb207de03c597ad69d10671655569ee1bc25ba16298869bc65c742d710db; anchor=fn avr_page(]`.
- Target: `desktop/src/device_settings.rs:498 [UTF8-LF sha256=9e62958ec6114027601efe1f4a4ea045d1fb665ce7ab96b8bf78a3fc744f065b; anchor=pub fn finish_avr(]`.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Retained: Desktop Device Settings now edits every supported Pakar AVR record field in one explicit draft: Silent/welcome/remember-page flags, display brightness, ADC6 threshold, telemetry period, short AVR name, visible/default pages and packed page order. Preserve the donor MCU ownership, validation and EEPROM wear/no-op rules, not its incompatible relay/motion/temperature record. USB and fenced HTTP share one GET/SET/GET transaction; compare the fresh complete baseline before SET, require exact ACK and persisted byte-for-byte readback, and reject stale peer/capability/programming owners without replay.
- Omitted/replaced code: `Donor relay, motion, PWM, temperature,14-page and boot restore field layouts`. Disposition: **pending**. Reason: The current hardware has a separate validated21-byte Pakar profile. Preserve all its fields and settings ownership; do not copy unrelated peripheral storage or overwrite EEPROM regions. Generic donor editors remain pending where hardware/capabilities later match.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Test-code evidence: `desktop/src/board_silent.rs:213 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_settings_full_apply_requires_fresh_baseline_ack_and_exact_persistent_readback()]`.
- Test-code evidence: `desktop/src/board_silent.rs:178 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_settings_require_each_changed_hardware_capability_and_no_programming_latch()]`.
- Test-code evidence: `desktop/src/device_settings.rs:658 [UTF8-LF sha256=9e62958ec6114027601efe1f4a4ea045d1fb665ce7ab96b8bf78a3fc744f065b; anchor=fn avr_editor_owns_dirty_draft_pending_receipt_and_same_peer_readback()]`.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.
- Remaining: Native full-record editor is source-only pending installed UI/physical AVR USB and Wi-Fi readback/reboot acceptance. Flutter and embedded Web still expose only the earlier Silent setting plus live-control links, not this full AVR field editor. The pre-write baseline comparison is optimistic, not an atomic multi-client CAS contract.

### PCController: `Project/StatusLedController.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b68da7e21af4396027c0f7ed7267d4c778292da47c19f1b9172bb4547ed1f53d`.

- Target: `firmware/esp32s3/src/StatusRgb.h:154 [UTF8-LF sha256=2959b720925b61c747b91f9d9dbc6fa5ffb9968cddfbc0fc48e8a5fb3b4da01b; anchor=bool service(uint32_t now, Writer writer)]`.
- Target: `desktop/src/status_rgb.rs:29 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=pub fn observe(]`.
- Target: `mobile/lib/status_rgb.dart:52 [UTF8-LF sha256=5d36a3437fdc52fb467bceee9599575b4f47799fe2944adf2fd10ddc1c7caec8; anchor=class StatusRgbPreview]`.
- Target: `firmware/esp8266/src/WebUi.h:826 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function acceptStatusRgb(]`.
- Retained: Full header/implementation read through EOF. Actual absolute elapsed/period evaluation, idempotent state ownership and already-scaled rendered RGB reporting adapted into the S3's board-owned semantic engine. Added bounded 300ms eased state transitions, explicit pin safety and driver-accepted output revision; no independent client-side animation reconstruction.
- Retained: Native/Web/mobile now merge compact observed RGB revisions only for the selected exact board/boot. Duplicate/older/foreign revisions cannot overwrite newer RGB, renew command/hardware freshness or replace cells/layout. Complete status snapshots retain only newer same-peer RGB; peer changes discard it. Selected transport capabilities gate settings with no silent Wi-Fi fallback, and writes require saved ACK plus exact persisted readback rather than a local preview.
- Omitted/replaced code: `PwmController/EEPROM/StatusProfileAddress dependencies and sixteen-bit phase clock`. Disposition: **replaced**. Reason: Those target AVR RGB PWM wiring and EEPROM allocation, not the identified S3 addressable pixel. Existing S3 persisted configuration owns enable/brightness, writer acceptance owns reported output, and the 32-bit elapsed timer advances without per-loop phase drift.
- Omitted/replaced code: `profile/setProfile/defaultProfile, applyRequested/cancelEffect and generic playCue descriptors`. Disposition: **pending**. Reason: Fixed semantic Flash/Cycle/Transition patterns and state priorities are now adapted, including real black blink intervals, held-black shutdown and bounded driver-failure retries. This does not implement every manually controllable donor RGB behavior. Preserve pending custom profiles/cues/descriptors and their exact owner tests rather than replacing them with optimistic UI controls.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:73 [UTF8-LF sha256=f48af12963f643feca43b77ed5edd181e22981afb761305cf947d5ad053fa6f7; anchor=No duplicate state events]`.
- Test-code evidence: `desktop/src/status_rgb.rs:157 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=fn compact_rgb_updates_only_the_matching_peer_and_newer_rgb_not_hardware_clock]`.
- Test-code evidence: `mobile/test/status_rgb_test.dart:7 [UTF8-LF sha256=908b1d6f4f4832dea297e0e36b9ab87500754569b595938d3cecfb95870bb61f; anchor=compact RGB event changes only fresh matching RGB]`.
- Test-code evidence: `tests/interface_settings_test.mjs:91 [UTF8-LF sha256=ecb4542141e12df8ab5409f1154cbe5e39466e5e23c3c39301f36eb6c17a77f2; anchor=Compact events merge only RGB]`.
- Remaining: Implement useful generic omitted RGB profile/manual/cue contracts; measure physical GPIO/RGB order and latency under simultaneous matrix/network/AVR work. Native writer doubles are not optical acceptance.

### PCController: `Project/StatusLedController.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `afbc70e3e8674c80d779168c37dc7430ab058e35baf2705b4b45cc11dd4e22ca`.

- Target: `firmware/esp32s3/src/StatusRgb.h:12 [UTF8-LF sha256=2959b720925b61c747b91f9d9dbc6fa5ffb9968cddfbc0fc48e8a5fb3b4da01b; anchor=class StatusRgb]`.
- Target: `firmware/esp8266/esp8266.ino:1463 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=void serviceStatusRgb(uint32_t now)]`.
- Target: `firmware/esp32s3/src/StatusRgbConfig.h:13 [UTF8-LF sha256=8ae7dd8a7868db38ff2a31c7034c3c9674c8bf860ee4b9aae4f559d2f125ed29; anchor=class StatusRgbConfig]`.
- Target: `desktop/src/status_rgb.rs:68 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=pub struct Configuration]`.
- Target: `mobile/lib/status_rgb.dart:116 [UTF8-LF sha256=5d36a3437fdc52fb467bceee9599575b4f47799fe2944adf2fd10ddc1c7caec8; anchor=class StatusRgbSettings]`.
- Retained: Full header/implementation read through EOF. Adapted nonblocking lifecycle state, absolute elapsed/period animation, repeat-state phase preservation, brightness-scaled rendered-byte reporting and explicit board-owned configuration into PakarS3::StatusRgb. Fixed descriptors now add donor flash/cycle/one-shot transition plus pulse/double-blink: fault and shutdown preempt routine states, verified success expires with the existing update lifecycle, offline USB is healthy and simultaneous AP/network availability is distinct.
- Omitted/replaced code: `AVR PwmController channels13..15/Power signal driver, EEPROM profile records and uint16 clock`. Disposition: **replaced**. Reason: The S3 owns a separate explicitly confirmed addressable pixel, not PCController PWM channels or the AVR EEPROM map. StatusRgbConfig now persists enable/pin/confirmation/brightness with pending readback/rename and identical-write suppression; uint32 elapsed arithmetic avoids the AVR clock restriction. Native/Web/mobile require complete board readback and explicit GPIO38/48 confirmation; no electrical pin probing or optimistic persisted success is performed.
- Omitted/replaced code: `19-condition editable profile catalog, temporary cue composition/priority and retained manual descriptors`. Disposition: **pending**. Reason: Static, breathe, flash, color-cycle and one-shot transition now exist in fixed semantic profiles, with additive pulse and double-blink. User-editable descriptors, cue composition, manual ownership, profile persistence and native/Web/mobile/front-panel editors remain useful work; fixed profiles do not certify full donor parity.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:76 [UTF8-LF sha256=f48af12963f643feca43b77ed5edd181e22981afb761305cf947d5ad053fa6f7; anchor=New mode cross-fades from actual frame]`.
- Test-code evidence: `tests/s3_status_rgb_config_test.cpp:24 [UTF8-LF sha256=f7b011a64d197ff17f1184fd50ceb6ab1eb91c4bef590ad7fe6e0490cfc06517; anchor=No identical write]`.
- Test-code evidence: `desktop/src/status_rgb.rs:131 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=fn saved_ack_requires_exact_persisted_config_readback_not_optimistic_preview]`.
- Test-code evidence: `mobile/test/status_rgb_test.dart:40 [UTF8-LF sha256=908b1d6f4f4832dea297e0e36b9ab87500754569b595938d3cecfb95870bb61f; anchor=RGB settings require explicit complete observations and safe GPIO]`.
- Remaining: Finish generic manual/profile/cue capabilities with persisted ACK/readback across all clients; verify physical onboard output and safety/error/update state priorities under real peer loss.

### PCController: `Project/StatusLedMath.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `997e15f12ca1d51689567f3a15b2f3b716261173878797cd5e68866289724740`.

- Target: `firmware/esp32s3/src/StatusRgb.h:234 [UTF8-LF sha256=2959b720925b61c747b91f9d9dbc6fa5ffb9968cddfbc0fc48e8a5fb3b4da01b; anchor=static uint8_t scale(]`.
- Target: `desktop/src/status_rgb.rs:2 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=clients never synthesize]`.
- Target: `mobile/lib/status_rgb.dart:9 [UTF8-LF sha256=5d36a3437fdc52fb467bceee9599575b4f47799fe2944adf2fd10ddc1c7caec8; anchor=Color? statusRgbColor(]`.
- Target: `firmware/esp8266/src/WebUi.h:825 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function statusRgbOutput]`.
- Retained: Full original source read through EOF. Actual unsigned byte-exact scale(value, level) and interpolate(from, to, phase) arithmetic copied into the S3 semantic onboard-pixel engine, independent of the GPIO4 matrix.
- Omitted/replaced code: `phaseDeadline distributing 64 phases in a modulo-16-bit AVR clock`. Disposition: **replaced**. Reason: S3 evaluates the present phase directly from uint32 elapsed milliseconds rather than scheduling 64 deadline steps. Late service does not slow the animation; native tests cover real production arithmetic and millis wrap, not optical output.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:69 [UTF8-LF sha256=f48af12963f643feca43b77ed5edd181e22981afb761305cf947d5ad053fa6f7; anchor=Already-scaled output bytes]`.
- Test-code evidence: `mobile/test/status_rgb_test.dart:70 [UTF8-LF sha256=908b1d6f4f4832dea297e0e36b9ab87500754569b595938d3cecfb95870bb61f; anchor=physical RGB is observed not synthesized or double dimmed]`.
- Test-code evidence: `tests/interface_settings_test.mjs:46 [UTF8-LF sha256=ecb4542141e12df8ab5409f1154cbe5e39466e5e23c3c39301f36eb6c17a77f2; anchor=Actual emitted RGB is not brightness-scaled again]`.
- Remaining: Verify exact optical RGB order, configured onboard GPIO and animation timing on the identified S3 under simultaneous Wi-Fi/matrix load; source and writer acceptance are not physical light proof.

### PCController: `Project/UartProtocol.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `bd2d1b8586622e7cc5a2712402b8832519745c7403b0326097dfd4dda07efb2f`.

- Target: `libraries/BoardCore/src/SerialLink.h:26 [UTF8-LF sha256=99373bc4c090e34324cfd2ae8c94d6666c227ad7e157a47fbd85125aac6c13a0; anchor=UartFlow::receiveBudget(serial_.available())]`.
- Target: `libraries/BoardCore/src/UartFlow.h:25 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=constexpr uint16_t serviceBudget = 64]`.
- Retained: Actual entry-occupancy receive snapshot, drop-until-delimiter recovery, bounded framing and synchronous response behavior adapted to shared SerialLink. Added64-byte cap for ESP32's larger RX ring and explicit negative-read handling; CRC/COBS code unchanged.
- Retained: Hardware UART remains on AVR; buffered unsolicited sends require enough TX ring space for a complete maximum frame. Failed game-state sends keep stateDirty armed. Busy-wire/polling behavior and default retained.
- Omitted/replaced code: `Zero-copy Frame view, in-place RX decode and externally exposed macro TX scratch`. Disposition: **pending**. Reason: Actual donor member TX scratch and streaming writeCobs are now adapted, preserving byte-exact packets and partial-write boundary repair. Current AVR FrontPanel transmit path falls from about652 to502 stack bytes including TX interrupt, against588 bytes remaining after globals; this is a focused ELF estimate, not whole-program high-water proof. Owning RX Frame and in-place decode optimization remain pending; no mutable macro scratch API is exposed.
- Omitted/replaced code: `framingErrors(), crcErrors(), responseErrors() separate counters`. Disposition: **pending**. Reason: Existing shared combined errors and S3 hardwareErrors are retained. Separate counter taxonomy and peer/client telemetry need full validated layout adaptation; this bounded flow change does not invent or renumber remote fields.
- Omitted/replaced code: `Event high-bit timestamp, BuzzerChanged and timed Ack/ErrorResponse suffixes`. Disposition: **pending**. Reason: Donor raw operation/schema values differ from the established Pakar envelope and explicit PCController adapter. Blindly appending suffixes would change ACK matching and nested contract semantics. Read full ProtocolRuntime/ControllerEvents and clients, then integrate timestamped projection through the explicit adapter.
- Omitted/replaced code: `advisory unknown reserved envelope byte acceptance`. Disposition: **replaced**. Reason: Pakar deliberately requires its fixed zero reserved byte and valid CRC, per living API fail-closed integrity policy. No numbered generation negotiation or compatibility alias is introduced. Existing protocol_test preserves CRC-correct nonzero rejection; optional opcodes still reach semantic dispatch.
- Test-code evidence: `tests/serial_link_test.cpp:52 [UTF8-LF sha256=c103ffb2c7530bb4c85373433b123fe072bb6649f2d97b82f70fa08d404eeae8; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: Buffered AVR profile is distinct and explicitly rejects ESP8266 targeting; no blind write replay.
- Remaining: Paired S3 application integration, reconnect/readback ownership and physical stress; build flag is not hardware acceptance.
- Remaining: Complete zero-copy/counter/timestamp projection pending above; actual donor protocol registry is not imported wholesale.

### PCController: `Project/UartProtocol.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `287a95a65a9b9655e64a15ece051bc792a06564b648a1b53eed4f373c564e458`.

- Target: `libraries/BoardCore/src/SerialLink.h:26 [UTF8-LF sha256=99373bc4c090e34324cfd2ae8c94d6666c227ad7e157a47fbd85125aac6c13a0; anchor=UartFlow::receiveBudget(serial_.available())]`.
- Target: `libraries/BoardCore/src/UartFlow.h:25 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=constexpr uint16_t serviceBudget = 64]`.
- Retained: Fully read actual UART API and synchronous payload lifetime contract. Shared SerialLink keeps AVR HardwareSerial ownership, separate RX/TX frame storage and handler/opaque context; Stream constructor preserves the S3 explicit pins/buffers.
- Omitted/replaced code: `Zero-copy Frame view, in-place RX decode and externally exposed macro TX scratch`. Disposition: **pending**. Reason: Actual donor member TX scratch and streaming writeCobs are now adapted, preserving byte-exact packets and partial-write boundary repair. Current AVR FrontPanel transmit path falls from about652 to502 stack bytes including TX interrupt, against588 bytes remaining after globals; this is a focused ELF estimate, not whole-program high-water proof. Owning RX Frame and in-place decode optimization remain pending; no mutable macro scratch API is exposed.
- Omitted/replaced code: `framingErrors(), crcErrors(), responseErrors() separate counters`. Disposition: **pending**. Reason: Existing shared combined errors and S3 hardwareErrors are retained. Separate counter taxonomy and peer/client telemetry need full validated layout adaptation; this bounded flow change does not invent or renumber remote fields.
- Omitted/replaced code: `Event high-bit timestamp, BuzzerChanged and timed Ack/ErrorResponse suffixes`. Disposition: **pending**. Reason: Donor raw operation/schema values differ from the established Pakar envelope and explicit PCController adapter. Blindly appending suffixes would change ACK matching and nested contract semantics. Read full ProtocolRuntime/ControllerEvents and clients, then integrate timestamped projection through the explicit adapter.
- Omitted/replaced code: `advisory unknown reserved envelope byte acceptance`. Disposition: **replaced**. Reason: Pakar deliberately requires its fixed zero reserved byte and valid CRC, per living API fail-closed integrity policy. No numbered generation negotiation or compatibility alias is introduced. Existing protocol_test preserves CRC-correct nonzero rejection; optional opcodes still reach semantic dispatch.
- Test-code evidence: `tests/serial_link_test.cpp:52 [UTF8-LF sha256=c103ffb2c7530bb4c85373433b123fe072bb6649f2d97b82f70fa08d404eeae8; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: Bounded large-ring RX, negative availability/read rejection and paired buffered build mode without busy GPIO.
- Remaining: Optimize zero-copy/storage only with full shared consumer and AVR stack/RAM proof; no claim of donor-wide protocol parity.
- Remaining: Finish separate counters and timestamped event contracts, and physical UART/LED/OTA contention acceptance.

### PCController: `Tools/Bootloader/Urboot-Custom/build.mjs`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `5e5607d54af43ead28eddb747e48b6e2450dd45ef7c47202203c4f5cfc8991ab`.

- Target: `tools/prepare-avr-image.mjs:65 [UTF8-LF sha256=0d4e2a5a87967031393a6079e07938c7612e6fbfcb0d38c9a2a3ae419e86d525; anchor=export function mergeUrboot(]`.
- Target: `tools/build-firmware.ps1:178 [UTF8-LF sha256=24584210e1f306e53c78f1db8b52ed0d7765d0950eb67b4034ff41f284bb39e7; anchor='prepare-avr-image.mjs']`.
- Retained: Read the complete donor builder and README. Adapted actual Intel HEX/checksum parsing, AVR jump decoding/encoding, wrapped reset RJMP, ELF __bad_interrupt unused-vector proof, metadata-selected application trampoline, erased-gap fill, full-flash binary/HEX round trip and exact source hash validation into the canonical AVR build. No chip, fuse, EEPROM or UART operation is performed by packaging.
- Retained: Found and corrected Pakar stock merged image reset vector bypassing its included vector Urboot. Preserve stock MiniCore UART0 autobaud, reset-register r2, EEPROM and chip-erase features with a pinned 384-byte image; never silently substitute the donor custom 512-byte backend.
- Omitted/replaced code: `u8.0.1 custom 512-byte backend, progress hook and TM1637 D13/PB5 CLK, D11/PB3 DIO implementation`. Disposition: **pending**. Reason: This board uses A5 CLK/A4 DIO and the stock 384-byte application ceiling. Do not deploy a wrong-pin backend or silently shrink available application space. Adapt the backend/pin profile and revalidate flash budget before any optional custom bootloader installation.
- Omitted/replaced code: `Stock source bootstrap/download, destructive profile-local object recreation and optional feature-size matrix`. Disposition: **replaced**. Reason: Use the installed MiniCore stock artifact with exact known HEX hash and the actual existing AVR compiler/ELF. Rebuilding unrelated bootloader variants, deleting object directories and downloading another core would add writes and does not fix the selected boot chain.
- Test-code evidence: `tests/avr_boot_image_test.mjs:14 [UTF8-LF sha256=6cae18f7420e6309045f6b25b741373698f4d306dfa6c3f8c8b126425f9e712d; anchor=assert.equal(decodeJump(result.merged,0),0x7e80)]`.
- Test-code evidence: `tests/avr_boot_image_test.mjs:20 [UTF8-LF sha256=6cae18f7420e6309045f6b25b741373698f4d306dfa6c3f8c8b126425f9e712d; anchor='Application input is never patched in place']`.
- Remaining: Physical ISP signature/fuse/flash/EEPROM backup and byte verification; correct the measured clock/BOD profile before deployment; prove reset enters Urboot and returns to the application, then real hardware UART round-trip.
- Remaining: Optional custom progress backend adaptation for A4/A5 with explicit profile/size and physical display acceptance.

### PCController: `Tools/Bootloader/Urboot-Custom/README.md`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `2ec873e7f23b56a6997a0c83de1e258ae2ce9b75cf1eb7cf08b40da5aaa68090`.

- Target: `tools/prepare-avr-image.mjs:65 [UTF8-LF sha256=0d4e2a5a87967031393a6079e07938c7612e6fbfcb0d38c9a2a3ae419e86d525; anchor=export function mergeUrboot(]`.
- Target: `tools/build-firmware.ps1:178 [UTF8-LF sha256=24584210e1f306e53c78f1db8b52ed0d7765d0950eb67b4034ff41f284bb39e7; anchor='prepare-avr-image.mjs']`.
- Retained: Read the complete donor builder and README. Adapted actual Intel HEX/checksum parsing, AVR jump decoding/encoding, wrapped reset RJMP, ELF __bad_interrupt unused-vector proof, metadata-selected application trampoline, erased-gap fill, full-flash binary/HEX round trip and exact source hash validation into the canonical AVR build. No chip, fuse, EEPROM or UART operation is performed by packaging.
- Retained: Found and corrected Pakar stock merged image reset vector bypassing its included vector Urboot. Preserve stock MiniCore UART0 autobaud, reset-register r2, EEPROM and chip-erase features with a pinned 384-byte image; never silently substitute the donor custom 512-byte backend.
- Omitted/replaced code: `u8.0.1 custom 512-byte backend, progress hook and TM1637 D13/PB5 CLK, D11/PB3 DIO implementation`. Disposition: **pending**. Reason: This board uses A5 CLK/A4 DIO and the stock 384-byte application ceiling. Do not deploy a wrong-pin backend or silently shrink available application space. Adapt the backend/pin profile and revalidate flash budget before any optional custom bootloader installation.
- Omitted/replaced code: `Stock source bootstrap/download, destructive profile-local object recreation and optional feature-size matrix`. Disposition: **replaced**. Reason: Use the installed MiniCore stock artifact with exact known HEX hash and the actual existing AVR compiler/ELF. Rebuilding unrelated bootloader variants, deleting object directories and downloading another core would add writes and does not fix the selected boot chain.
- Test-code evidence: `tests/avr_boot_image_test.mjs:14 [UTF8-LF sha256=6cae18f7420e6309045f6b25b741373698f4d306dfa6c3f8c8b126425f9e712d; anchor=assert.equal(decodeJump(result.merged,0),0x7e80)]`.
- Test-code evidence: `tests/avr_boot_image_test.mjs:20 [UTF8-LF sha256=6cae18f7420e6309045f6b25b741373698f4d306dfa6c3f8c8b126425f9e712d; anchor='Application input is never patched in place']`.
- Remaining: Physical ISP signature/fuse/flash/EEPROM backup and byte verification; correct the measured clock/BOD profile before deployment; prove reset enters Urboot and returns to the application, then real hardware UART round-trip.
- Remaining: Optional custom progress backend adaptation for A4/A5 with explicit profile/size and physical display acceptance.

### PCController: `Tools/Controller/docs/Go-Embedding-API.md`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4d8ff85c9c44092100d4f349f20eb5493d1516d064dd03d0f6b8d1010e7b6795`.

- Target: `docs/API-POLICY.md:1 [UTF8-LF sha256=ed842b37b6ea174166559175e84947251af11d6b5cdcde026548b70a7002373a; anchor=# One living API contract]`.
- Target: `tools/check-living-contract.mjs:7 [UTF8-LF sha256=28e62dedeac509186ca6009aa99c7cc5a561357f9fcda9005c5ff2308c146c1e; anchor=export function violations]`.
- Target: `desktop/src/capabilities.rs:152 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=pub fn known(]`.
- Retained: Actual living unversioned contract policy, additive optional data, stable feature IDs and strict critical identity/auth/routing/replay/deadline/integrity rejection applied across firmware and all clients.
- Omitted/replaced code: `host.New/Start/Stop, Host.Call/Events and foreign-function host lifecycle ownership`. Disposition: **pending**. Reason: Pakar is ESP/AVR plus Rust/Flutter, not the Go embedded host. Importing the entire Go lifecycle without a shared-runtime adapter would create a second UART owner. Generic typed host lifecycle, notifications and library integration remain useful and pending; only the exact API policy is completed here.
- Test-code evidence: `tests/living_contract_test.mjs:26 [UTF8-LF sha256=55c421193701d61071019dccee676a0f8e7388518992dbb30d0ac8c3ddcbb676; anchor=Living-contract guard negative fixtures]`.
- Test-code evidence: `tests/web_context_test.mjs:89 [UTF8-LF sha256=69e5d441087ed22663b727aab6c76eb26501545c3e9d49c291486e8426397c79; anchor=Additive metadata does not select another generation]`.
- Remaining: Compare and integrate applicable canonical host lifecycle and typed RPC/event ownership through an explicit adapter; not certified from this policy-only change.

### PCController: `Tools/Controller/internal/appconfig/buzzer_runtime.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `cea9f315404028716f708a2a7ffe3fc71950a3ccb2996673708d977517af0e3f`.

- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Omitted/replaced code: `Full board/host/both/none mirroring routes, runtime overrides, OS backend/executable resolution and watched app configuration`. Disposition: **pending**. Reason: Reuse the known/unknown/pending/verified board-setting ownership, not an unimplemented host audio backend. Actual host routing parity needs independent implementation and acoustic acceptance; this change must not mute Windows notifications by accident.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.

### PCController: `Tools/Controller/internal/artifacts/download_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `9640f5646f2de3adec801157e2cad162632d7a9af3e249a5b8cb30715289992e`.

- Target: `mobile/lib/firmware_automation.dart:18 [UTF8-LF sha256=f61aa58c32f3988a405504aa97ad7a3115577a751d5bf221859f46eb41803b06; anchor=class FirmwareAutomation]`.
- Target: `mobile/lib/app_updates.dart:583 [UTF8-LF sha256=9e530545db419ac781b94329a940bb4e02fcd9f421405ec2766a0f520f6346e4; anchor=readAppUpdateBytes]`.
- Retained: Actual finite body/length/failure download fixtures adapted to real Dart HTTP listeners and bounded catalog/transfer streams, plus no write on failed pin/receipt save. Not a Go runtime or production TLS/flash proof.
- Omitted/replaced code: `Remaining generic donor/platform acceptance outside this bounded update slice`. Disposition: **pending**. Reason: Android foreground services/background uptime, split APKs and signing-key rotation, direct Android USB-host firmware programming, generic host RPC parity and actual Android/LAN/TLS/ESP/AVR physical acceptance remain separate. Exact self-package signer sets and newer version codes fail closed; no silent installer or uncertain programming replay.
- Test-code evidence: `mobile/test/firmware_automation_test.dart:270 [UTF8-LF sha256=2046ae0d8c835d0dbd598890f28fe965f0227dcfc8cc0323ab3009f618453f01; anchor=Cannot save consumed receipt]`.
- Test-code evidence: `mobile/test/app_updates_test.dart:422 [UTF8-LF sha256=847413faf38f07ad3da23ed127fcf41ff796ee576430babe588c06a53a1c2a94; anchor=absolute response deadline]`.
- Remaining: Issue #7/#2/#3: actual installed Android peer transfer/OS consent/remote endpoint/board flash and human acceptance; source tests and staged APK are not deployment.

### PCController: `Tools/Controller/internal/artifacts/download.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `13c3aa60d33cc41276c2c7a44312562043e4ffb647a485954dddfd6d6ecc6088`.

- Target: `desktop/src/firmware_source.rs:54 [UTF8-LF sha256=6ab2407369d6f7c1beba668fd2548bbe6f89186cdbfb93ac43b04267cd0f4a6d; anchor=fn download(]`.
- Target: `desktop/src/maintenance.rs:111 [UTF8-LF sha256=c3e6cd35e527273d83eebc597e081b1d4405d5255581e132952d3505baf389a0; anchor=pub fn validate_image_for_board(]`.
- Target: `mobile/lib/firmware_image.dart:68 [UTF8-LF sha256=274b96b95c959594b8720f40d4998dc6e307ed8fe31e82093c19b581ea4a0654; anchor=void validateFirmwareImage(]`.
- Target: `mobile/lib/firmware.dart:385 [UTF8-LF sha256=190ddce7b1bbf298a74464ea8202287ead801bfe8725ab4d0af39993d5a6c30e; anchor=Future<String> fileUrl(]`.
- Retained: Read the full downloader and preserve finite timeout, independent source credentials, bounded byte reader, real Content-Length/unknown denominator, redirect refusal, SHA-256 verification and image validation before any upload. Small firmware images remain in bounded RAM rather than creating a new disk artifact cache.
- Retained: Native download/local selection/upload and Flutter HTTP/local SAF/export now share platform-aware application-image admission rather than an arbitrary2MiB transport ceiling. S3 E9/segment/chip9/hash-appended headers and strict less-than0x330000 slot bounds accept the migrated2220352-byte image; an observed smaller selected-board OTA capacity narrows admission before any upload. ESP8266 retains its independent less-than1MiB policy, and AVR requires exactly32768 bytes. Full running-image/signature/device verification remains separate from these host header/intake guards.
- Omitted/replaced code: `Remaining donor mechanisms outside this delivery slice`. Disposition: **pending**. Reason: Persisted multi-kind artifact store, automatic resume/redirects and donor public-only DNS destination policy are not transplanted into explicit user-selected local/LAN file downloads. Remote offers instead require an operator-configured exact HTTPS origin. Generic remote public artifact browsing/storage and resumed-transfer parity remain pending.
- Test-code evidence: `desktop/src/firmware_source.rs:244 [UTF8-LF sha256=6ab2407369d6f7c1beba668fd2548bbe6f89186cdbfb93ac43b04267cd0f4a6d; anchor=fn firmware_source_real_http_download_is_verified_before_one_pinned_upload]`.
- Test-code evidence: `desktop/src/maintenance.rs:525 [UTF8-LF sha256=c3e6cd35e527273d83eebc597e081b1d4405d5255581e132952d3505baf389a0; anchor=fn s3_image_policy_accepts_actual_large_raw_image_but_not_slot_or_header_mismatch]`.
- Test-code evidence: `mobile/test/firmware_image_test.dart:133 [UTF8-LF sha256=81f47d2e4a07740c1dab37cf55849a8a5c91eddf47631e7b631da5dad7b6ca4b; anchor=Hashed HTTP S3 source above 2MiB uploads once]`.
- Test-code evidence: `mobile/test/firmware_delivery_test.dart:104 [UTF8-LF sha256=2a5a1ff4950f3806d522d2afe76050884aff03a7f3f0556abc329f25a0042976; anchor=HTTP file verifies hash before one upload]`.
- Remaining: Issue #7: real endpoint TLS/throughput/loss and malformed image tests on hardware; host tests cannot establish flash/power-loss success.

### PCController: `Tools/Controller/internal/artifacts/selfupdate_windows.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f5a8272234eb32a91299203197aca85d0a147e360db768869b9a4005ff660e5f`.

- Target: `desktop/src/client_install.rs:268 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=pub fn helper(]`.
- Retained: Actual finite Windows process wait adapted with one pinned process handle plus executable path validation; verified current canonical donor matches inventory, alternate donor differs only in console/journal presentation.
- Omitted/replaced code: `Donor-wide installer/recovery/platform acceptance outside this self EXE slice`. Disposition: **pending**. Reason: Force-kill and automatic deletion of rollback are deliberately not copied: a live editor or unverified new process must remain recoverable. Media/libmpv, archive/GitHub extraction, interactive TUI console inheritance do not apply to this GUI raw same-origin executable. Generic signed installer/removal inventory, unattended uninstall and true two-new-version helper/Windows-session/OS acceptance remain pending. Preference UI uses actual existing Pakar donor widgets; no full donor GUI parity is claimed.
- Test-code evidence: `desktop/src/client_management.rs:865 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=release_policy_checks_numeric_build_semver_platform_origin_and_integrity]`.
- Test-code evidence: `desktop/src/client_install.rs:551 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=journals_bind_all_transaction_paths_and_parent_without_shell_commands]`.
- Remaining: Issue #7: live secure enrollment, release/telemetry/command receipt and safe replacement proof separated from source fixtures; Android foreground-only and self-uninstall local/OS-consent boundaries remain.

### PCController: `Tools/Controller/internal/artifacts/store.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `1f167ddab14067f939649a96c57432981a2d0074ad8b7c1ada37948da8d51a39`.

- Target: `services/client-management/service.mjs:90 [UTF8-LF sha256=279a7a9d9e699831b0d19128ab89aac1610d1c8cdc3994c99f8f9c12e8baa795; anchor=export async function createService]`.
- Target: `tools/publish-client-release.mjs:24 [UTF8-LF sha256=30d707b4032dae9d2ab01b8ea6bc322658ef6eee314647af02f90e42fb8d4b37; anchor=const metadata=]`.
- Retained: Actual streaming bounded Put, digest/type checks, fsync before immutable publication, owned path/descriptor checks adapted to the Node SQLite registry and content-addressed package route.
- Omitted/replaced code: `Generic donor artifact store beyond self application packages`. Disposition: **pending**. Reason: This temporary service stores Windows self-executable and Android self-APK only; generic board artifact kinds/current pointers, resumed downloads and Go os.Root API are not copied. Node loopback-only service uses an exclusive trusted-writer directory, same-open handle and NOFOLLOW; clients always verify exact digest/bytes. Background Android services and two-installed-version acceptance remain pending.
- Test-code evidence: `services/client-management/service.test.mjs:10 [UTF8-LF sha256=a8f7d7976b732ef66fc3be887e7d657254409a1b94e9620c329b53ad8067feb5; anchor=actual management HTTP]`.
- Remaining: Issue #7: actual server/client rollout receipts and Android installed consent/multi-device acceptance; no arbitrary shell execution or uninstall of unrelated packages.

### PCController: `Tools/Controller/internal/discovery/discovery_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `41b18ac33a9eae75dbcd9ac7a20722117090e98c5a8bad6eb12114a5f955e792`.

- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Retained: Read entire test file. Adapt actual loopback-no-network, source-bound SSDP, explicit protocol status and secret-free public metadata assertions to Rust socket/parser tests.
- Omitted/replaced code: `WSDiscovery/NetBIOS/broadcast, rich TXT normalization and public enrichment test matrix`. Disposition: **pending**. Reason: These donor tests exercise adapters not yet present in Pakar. Their behavior remains pending rather than replacing assertions with always-success fixtures.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:649 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_mndp_uses_explicit_application_port_not_guessed_port80]`.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:685 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_loopback_registration_starts_no_discovery_transports]`.
- Remaining: Issue #2: WSDiscovery/NetBIOS/broadcast, rich TXT normalization and public enrichment test matrix. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/discovery/discovery.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4ca50ce622f998f1922a828350ad71aea5fa5dee7506069ca3df2e857536198e`.

- Target: `mobile/lib/discovery.dart:165 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=class DeviceDiscovery]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/DiscoveryChannel.kt:15 [UTF8-LF sha256=0c1215ebc188d720fe9c181a475efac7567d6b5b884de5e8d97a2d443593bd61; anchor=class DiscoveryChannel]`.
- Target: `mobile/lib/devices_page.dart:81 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=class DevicesPage]`.
- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Retained: Fully read source through EOF. Adapted actual mDNS/DNS-SD single transport intent, metadata-only discovery, bounded search lifetime/cleanup, M-SEARCH headers, validated origins and explicit authentication separation into Android discovery.
- Retained: Android NsdManager replaces Go zeroconf; serialized resolves, scan generation ownership, multicast-lock acquisition/release and lifecycle cleanup are real native code. Local/remote saved device types describe transport, never fabricate capabilities. Actual peer ACK remains authoritative.
- Retained: Canonical discovery.go783lines fully reread through EOF. Android NSD/SSDP/MNDP mappings remain metadata-only partial adaptations; advertisements do not grant authenticated board ownership.
- Retained: Reread all 783 lines. Native opt-in advertiser/browser adapts actual single mDNS/DNS-SD transport, per-protocol startup/failure reporting, persistent instance identity, alive/byebye and nonsecret public metadata. Android retains actual NSD discovery; a native USB owner can now publish compatible HTTP/WS APIs.
- Omitted/replaced code: `Advertiser/UpdateText/SSDP alive/byebye, public enrichment/mergeInstances and TXT metadata normalization`. Disposition: **pending**. Reason: Android bridge is deliberately authenticated loopback-only. Publishing a LAN-reachable phone URL would be false; an opt-in LAN/TLS listener and advertisement lifecycle need a separate implementation. Advertised names alone cannot merge authenticated board identity. PublicInfo enrichment dependencies are not yet fully reviewed.
- Omitted/replaced code: `WSDiscovery, broadcast and NetBIOS adapter consumers`. Disposition: **pending**. Reason: The current phone implements requested DNS-SD/SSDP/MNDP browsing, not every PCController discovery wire adapter. Android-specific permission/network ownership and complete adapter tests remain useful work.
- Omitted/replaced code: `PCController product service type and Go zeroconf goroutines`. Disposition: **replaced**. Reason: Android's platform NSD resolves Pakar/HTTP/PCController metadata without importing a Go runtime or copying numbered product contract names. Advertisements never assert generic PCController RPC compatibility.
- Omitted/replaced code: `ActiveProtocols/Failures degraded startup; instance.id SSDP UUID;64-record255-byte secret-filtered TXT refresh; eight-slot enrichment and multi-product browsing`. Disposition: **pending**. Reason: Phone still has no donor LAN advertiser/UpdateText lifecycle, public enrichment/identity merge or complete WSDiscovery/broadcast/NetBIOS consumers. Native USB-only mode must invoke no discovery/public HTTP enrichment. Never merge verified identity from names or claim merely configured responders started.
- Omitted/replaced code: `UpdateText rich board metadata and public.go/public_test.go enrichment, WSDiscovery/broadcast/NetBIOS, Android LAN advertising`. Disposition: **pending**. Reason: Native advertises stable listener metadata and actual owner state over HTTP/WS, not the donor rich TXT update loop. public.go dependencies and whole manager.go are not fully read/imported. Android still deliberately has only its foreground loopback server. Additional adapters and Android listener ownership need real implementations and device/LAN acceptance, not a fabricated completeness claim.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:201 [UTF8-LF sha256=1e650a1f6897c67ce9e87de864ff28ed98e0a818ca95ba6b413897b5cd99a905; anchor=Actual loopback bridge requires bearer]`.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:649 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_mndp_uses_explicit_application_port_not_guessed_port80]`.
- Test-code evidence: `mobile/test/bridge_peer_test.dart:86 [UTF8-LF sha256=04688eaf3a76042bd60b380b7d631657828e922e9c740c55626563890800e498; anchor=MNDP application endpoints use vendor API port]`.
- Remaining: Issue #2: UpdateText rich board metadata and public.go/public_test.go enrichment, WSDiscovery/broadcast/NetBIOS, Android LAN advertising. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/discovery/mndp.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `6faf511e686c83114025020a25d2da6ff0960240c969dd3872c6cf18ff20baf3`.

- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Target: `mobile/lib/discovery.dart:348 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=DiscoveredDevice? parseMndp]`.
- Retained: Read through EOF. Port actual big-endian MNDP vendor product/API-port TLVs, application-scoped locally administered virtual MAC, bounded records, 30s announcement and rate-limited query response to Rust. Dart consumes explicit API port and separate instance ID.
- Omitted/replaced code: `Full multi-interface broadcast/IPv6/network-change acceptance`. Disposition: **pending**. Reason: Parser/socket fixture is not proof that real adapters, Windows firewall and Android multicast allow discovery across the actual LAN. Rebinding after adapter changes is not yet implemented.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:649 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_mndp_uses_explicit_application_port_not_guessed_port80]`.
- Test-code evidence: `mobile/test/bridge_peer_test.dart:86 [UTF8-LF sha256=04688eaf3a76042bd60b380b7d631657828e922e9c740c55626563890800e498; anchor=MNDP application endpoints use vendor API port]`.
- Remaining: Issue #2: Full multi-interface broadcast/IPv6/network-change acceptance. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/discovery/protocols.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `8566438c89961f85f6aeec3f5df237e81bea253204460b35ba41ee86b68589d3`.

- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Retained: Entire file read for comparison. Requested SSDP/mDNS/DNS-SD/MNDP adapters now exist in native sharing; this file's additional SOAP WSDiscovery/custom UDP/NetBIOS adapters are not falsely described as ported.
- Omitted/replaced code: `WSDiscovery SOAP, custom UDP broadcast and NetBIOS name service`. Disposition: **pending**. Reason: These are useful pending adapters, not media/PU business exclusions. They need explicit service binding, bounded parsers and no-network fixtures before publication.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:649 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_mndp_uses_explicit_application_port_not_guessed_port80]`.
- Remaining: Issue #2: WSDiscovery SOAP, custom UDP broadcast and NetBIOS name service. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/hostbridge/buzzer_mirror_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `d92d0826cd9665520087a3b5704ed874ee5c605695d571975b3c15b7a54aa84a`.

- Target: `mobile/lib/mirroring.dart:76 [UTF8-LF sha256=6fb0cc41bc30499df99ea974ae21d5b168325132bf6ae93ee961e0a5ea16b267; anchor=class ToneTimeline]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/ToneMirrorChannel.kt:22 [UTF8-LF sha256=48934806ea64dc02be68fbdf73978d65337fdb36dd94482749c14422aaf8dd20; anchor=class ToneMirrorChannel]`.
- Target: `firmware/avr/PcControllerRuntime.cpp:619 [UTF8-LF sha256=cd62f7447553664a06de9b55bdb18371d26972b63833d80035e795d367669694; anchor=put32(audio + 5, s.audioStartedMicros);]`.
- Target: `firmware/avr/PcControllerRuntime.h:49 [UTF8-LF sha256=8436a3626b62d089b567851b1199d0e7eb5320d868d0ca51deda1794d9c0bc7e; anchor=void companionHeartbeat]`.
- Retained: Fully read actual current donor through EOF. Adapted uint32 MCU timestamp wrap, 50ms playout delay, 250ms skew bound, absolute note deadlines, source-generation cancellation, independent opt-in and distinct failure transitions into real Android mirroring.
- Retained: Donor cadence/stop/reconnect tests adapted to a silent injectable Flutter output. Actual PCM Android renderer uses bounded 5ms buffers, exact requested Hz, click-reducing envelope, finite deadline, focus/lifecycle cancellation and an independent six-second native receive lease.
- Retained: Canonical buzzer_mirror_test.go539lines fully reread through EOF; existing Flutter fixture mappings remain bounded historical adaptations, not execution of refreshed Go tests.
- Retained: Current PCController source/test reread at b0111ae0. Companion heartbeat now admits finite changed-note telemetry without granting a desktop/host menu session. Android keeps independent opt-in audio, adds explicit follow-board-mute, suppresses repeated musical-step restarts and rebuffers fresh short jittered notes.
- Omitted/replaced code: `Go multi-source pending/replacement sorting, Linux KDMKTONE/external beep drivers and bridge ingress`. Disposition: **replaced**. Reason: Android mirrors only the explicitly selected peer. Independent session tokens and selected board/boot/AVR-session ownership replace multi-source scheduling; Android AudioTrack PCM replaces incompatible PC-speaker drivers. No background service or remote note ingestion is silently enabled.
- Omitted/replaced code: `Native receiver physical waveform/latency and complete all-host audio-domain parity`. Disposition: **pending**. Reason: No connected ADB phone or physical board acceptance yet. Native/Web do not yet consume the new timestamped audio stream; ESP32-S3 integration remains separate pending work. Phone synthesis matches reported pitch, not piezo timbre or measured Timer1 quantization.
- Omitted/replaced code: `Fresh10ms/1ms recovery; scheduler handoff; stale-note rejection; no backend reprobe; same-source playback/stop/reconnect purge fixtures`. Disposition: **pending**. Reason: Current Flutter fixtures do not implement all refreshed short-note recovery/source-wide scheduler assertions. These useful regression imports remain pending with silent outputs. No donor suite or physical audio was run by this review.
- Test-code evidence: `mobile/test/mirroring_transport_test.dart:58 [UTF8-LF sha256=7fb349b093ec0ff52c7e244043a5de48048d4a5108608132e97a63e416d311eb; anchor=Actual authenticated WebSocket]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:83 [UTF8-LF sha256=32e38d19bd9aefad76f78d921936329e12cfc1de8eed24cf704a83ea6e3f5d89; anchor=Actual donor cadence]`.
- Test-code evidence: `tests/pc_contract_test.cpp:315 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=Appended actual musical start]`.
- Test-code evidence: `tests/pc_contract_test.cpp:333 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=void companionMirroring()]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:127 [UTF8-LF sha256=32e38d19bd9aefad76f78d921936329e12cfc1de8eed24cf704a83ea6e3f5d89; anchor=Mute follow is optional]`.
- Remaining: Install APK on authorized phone, deploy integrated ESP/AVR together after backups; measure welcome cadence, background/focus/long-tone/disconnect cutoff, all actual front-panel keys and segment masks.
- Remaining: Complete S3 shared firmware integration, multi-source/background receiver if requested and network bridge note forwarding without echo/replay.
- Remaining: Current ASUS SSH8022 unavailable: new firmware/app deployment, physical frequency/cadence/stop/focus acceptance remain open. Native/Web host-speaker consumers are not implemented by this slice.

### PCController: `Tools/Controller/internal/hostbridge/buzzer_mirror.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `dcb9bf3c1a31cb6cc06f4f5ed52def32b7388fd4d33587e233476a3a76e1f032`.

- Target: `mobile/lib/mirroring.dart:76 [UTF8-LF sha256=6fb0cc41bc30499df99ea974ae21d5b168325132bf6ae93ee961e0a5ea16b267; anchor=class ToneTimeline]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/ToneMirrorChannel.kt:22 [UTF8-LF sha256=48934806ea64dc02be68fbdf73978d65337fdb36dd94482749c14422aaf8dd20; anchor=class ToneMirrorChannel]`.
- Target: `firmware/avr/PcControllerRuntime.cpp:619 [UTF8-LF sha256=cd62f7447553664a06de9b55bdb18371d26972b63833d80035e795d367669694; anchor=put32(audio + 5, s.audioStartedMicros);]`.
- Target: `firmware/avr/PcControllerRuntime.h:49 [UTF8-LF sha256=8436a3626b62d089b567851b1199d0e7eb5320d868d0ca51deda1794d9c0bc7e; anchor=void companionHeartbeat]`.
- Retained: Fully read actual current donor through EOF. Adapted uint32 MCU timestamp wrap, 50ms playout delay, 250ms skew bound, absolute note deadlines, source-generation cancellation, independent opt-in and distinct failure transitions into real Android mirroring.
- Retained: Donor cadence/stop/reconnect tests adapted to a silent injectable Flutter output. Actual PCM Android renderer uses bounded 5ms buffers, exact requested Hz, click-reducing envelope, finite deadline, focus/lifecycle cancellation and an independent six-second native receive lease.
- Retained: Canonical buzzer_mirror.go435lines fully reread through EOF. Existing Android timeline/peer-owner mappings remain partial retained adaptations, not parity with the refreshed scheduler/backend lifecycle.
- Retained: Current PCController source/test reread at b0111ae0. Companion heartbeat now admits finite changed-note telemetry without granting a desktop/host menu session. Android keeps independent opt-in audio, adds explicit follow-board-mute, suppresses repeated musical-step restarts and rebuffers fresh short jittered notes.
- Omitted/replaced code: `Go multi-source pending/replacement sorting, Linux KDMKTONE/external beep drivers and bridge ingress`. Disposition: **replaced**. Reason: Android mirrors only the explicitly selected peer. Independent session tokens and selected board/boot/AVR-session ownership replace multi-source scheduling; Android AudioTrack PCM replaces incompatible PC-speaker drivers. No background service or remote note ingestion is silently enabled.
- Omitted/replaced code: `Native receiver physical waveform/latency and complete all-host audio-domain parity`. Disposition: **pending**. Reason: No connected ADB phone or physical board acceptance yet. Native/Web do not yet consume the new timestamped audio stream; ESP32-S3 integration remains separate pending work. Phone synthesis matches reported pitch, not piezo timbre or measured Timer1 quantization.
- Omitted/replaced code: `Fresh short-note/scheduler/timer rebuffering; source-wide queued-plan rebasing; original receipt-time and ready backend reuse without KDMKTONE reprobe`. Disposition: **pending**. Reason: Current mobile ToneTimeline spends age from its cushion and can discard delayed short notes; no rebaseForStart equivalent exists. Import bounded fresh-note recovery with silent fixtures without rehabilitating stale observations. Native backend reuse is useful when implemented; unrelated settings must not probe live speakers.
- Test-code evidence: `mobile/test/mirroring_transport_test.dart:58 [UTF8-LF sha256=7fb349b093ec0ff52c7e244043a5de48048d4a5108608132e97a63e416d311eb; anchor=Actual authenticated WebSocket]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:83 [UTF8-LF sha256=32e38d19bd9aefad76f78d921936329e12cfc1de8eed24cf704a83ea6e3f5d89; anchor=Actual donor cadence]`.
- Test-code evidence: `tests/pc_contract_test.cpp:315 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=Appended actual musical start]`.
- Test-code evidence: `tests/pc_contract_test.cpp:333 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=void companionMirroring()]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:127 [UTF8-LF sha256=32e38d19bd9aefad76f78d921936329e12cfc1de8eed24cf704a83ea6e3f5d89; anchor=Mute follow is optional]`.
- Remaining: Install APK on authorized phone, deploy integrated ESP/AVR together after backups; measure welcome cadence, background/focus/long-tone/disconnect cutoff, all actual front-panel keys and segment masks.
- Remaining: Complete S3 shared firmware integration, multi-source/background receiver if requested and network bridge note forwarding without echo/replay.
- Remaining: Current ASUS SSH8022 unavailable: new firmware/app deployment, physical frequency/cadence/stop/focus acceptance remain open. Native/Web host-speaker consumers are not implemented by this slice.

### PCController: `Tools/Controller/internal/hostbridge/discovery_metadata_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `cb26b3fba521fe61d2b7bce2b1fa6a019dc6d04d72bb82df541e4ef63c1bbcd3`.

- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Retained: Read complete tests. Port board-vs-host identity and available-route capability assertions, including suppression of unsupported OTA/ISP/streaming on the relay.
- Omitted/replaced code: `All donor rich host metadata, peer caps and discovery refresh scheduling assertions`. Disposition: **pending**. Reason: Those metadata producers and UpdateText lifecycle are not fully implemented; fixture identity and real socket tests do not close donor parity or real LAN acceptance.
- Test-code evidence: `desktop/src/bridge.rs:1195 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_status_keeps_board_identity_and_only_available_route_capabilities]`.
- Test-code evidence: `mobile/test/bridge_peer_test.dart:86 [UTF8-LF sha256=04688eaf3a76042bd60b380b7d631657828e922e9c740c55626563890800e498; anchor=MNDP application endpoints use vendor API port]`.
- Remaining: Issue #2: All donor rich host metadata, peer caps and discovery refresh scheduling assertions. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/hostbridge/discovery_metadata.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `524f6e31069355fb8f7968e507775d4a567964aee41001722a325cc8c06e99a0`.

- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Target: `libraries/BoardCore/src/PeerCompatibility.h:27 [UTF8-LF sha256=530d8de10f3715db8cb3404dc511634152bcea12c14d9e6ebb578645307fd26b; anchor=namespace BoardCore { namespace PeerCompatibility {]`.
- Target: `firmware/esp8266/src/ClientTelemetry.h:18 [UTF8-LF sha256=71f78fe48b0208aabe6505d9cf5986484ee8d5e77c1fea8a7b2737afecfb112a; anchor=class ClientTelemetry {]`.
- Target: `desktop/src/peer_identity.rs:379 [UTF8-LF sha256=4895d4ce1dc5f77e1e866600349baef3ba131e24d23970e254a968442e663dfe; anchor=pub fn sanitize_report(]`.
- Target: `mobile/lib/peer_identity.dart:28 [UTF8-LF sha256=615bc3e694748a9260d5df7359f27f6573731e1397d8b549fb45d11d4c4843c4; anchor=class PeerIdentity]`.
- Retained: Read entire source. Preserve instance.id vs physical board identity and push-fed nonsecret current-owner metadata: bridge path/hostname are additive, board_id/boot_id/hostname remain downstream-owned; current status is fed without independent serial acquisition.
- Retained: Adapt the donor's observed-vs-declared metadata separation and bounded nonsecret push reports across the board, native and mobile peer registries. Shared release-range admission is a new user requirement, not copied donor version negotiation; no exact-build adapter or numbered route is introduced.
- Omitted/replaced code: `Complete discoveryMetadata rich TXT, five-second coalesced refresh and all HostIdentity fields`. Disposition: **pending**. Reason: Fixed nonsecret listener TXT suffices for compatible endpoint discovery, while full board state is served from the selected owner. Rich changing TXT and cross-product public enrichment remain useful pending imports; no board secret is advertised.
- Test-code evidence: `desktop/src/bridge.rs:1288 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_network_repeater_confirms_upstream_usb_readback_and_preserves_both_hops]`.
- Test-code evidence: `desktop/src/bridge.rs:1195 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_status_keeps_board_identity_and_only_available_route_capabilities]`.
- Test-code evidence: `tests/peer_compatibility_test.cpp:29 [UTF8-LF sha256=1a99a731b1c980523c52bbdfa21d0d4bdd034d9c80e7dcfd1a91d92765953df3; anchor=assert(numericAppAllowed(0,2,8));]`.
- Test-code evidence: `tests/client_telemetry_test.cpp:48 [UTF8-LF sha256=46a225d40e6ec3c2acc60ace8467f9deafc6fef2d3984c5aadc2b50f02290cf9; anchor=assert(detail[0]["observed_ip"].isNull()]`.
- Test-code evidence: `desktop/src/peer_identity.rs:592 [UTF8-LF sha256=4895d4ce1dc5f77e1e866600349baef3ba131e24d23970e254a968442e663dfe; anchor=fn reports_drop_secrets_and_never_replace_observed_ip()]`.
- Remaining: Issue #2: Complete discoveryMetadata rich TXT, five-second coalesced refresh and all HostIdentity fields. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.
- Remaining: Issue #2: Actual default-on/disabled-build peer admission and multicast discovery between installed releases remain live acceptance. Direct clients are reported to the MCU; a bridge keeps downstream reports locally. Forwarding those indirect reports through the exclusive USB owner requires a bounded correlated report queue; do not relabel indirect reported IPs as MCU-observed.

### PCController: `Tools/Controller/internal/hostbridge/lifecycle.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `30454374bc87aeed2d4a536f27a24aa8c382920e9047b40d68a42faef47990c4`.

- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Target: `desktop/src/main.rs:1555 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=SerialCommand::Bridge(target]`.
- Retained: Read through EOF. Preserve primary selected owner and explicit Disconnect/revocation; sharing never opens another serial handle or acquires a board from discovery. Bridge listener starts only on opt-in and closes through owned stop lifetime.
- Omitted/replaced code: `Complete host suspend/resume lifecycleInstruction service/Windows power-event integration`. Disposition: **pending**. Reason: Current bridge is scoped to the existing app runtime/connection Gate, not a new always-running service. Listener suspend/resume behavior and platform power callbacks require additional integration and actual host acceptance.
- Test-code evidence: `desktop/src/bridge.rs:1288 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_network_repeater_confirms_upstream_usb_readback_and_preserves_both_hops]`.
- Remaining: Issue #2: Complete host suspend/resume lifecycleInstruction service/Windows power-event integration. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/hostbridge/manager.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b34517a1c30084d2362e951638e0039d78abd45a5128b62342bb717b6c44b1b2`.

- Target: `desktop/src/firmware_feed.rs:228 [UTF8-LF sha256=4eee358ef83fb6c6a5bd17d1ebed3683fd203939f2ff3feab7575c1a5a9f330f; anchor=pub struct Wire]`.
- Target: `mobile/lib/firmware_feed.dart:106 [UTF8-LF sha256=fb8f8c35c609def5a1da0997a16175baefcc0f568439df0aea1a6c00b801ab55; anchor=class FirmwareFeedWire]`.
- Retained: Adapt the fully read actual outbound WebSocket and genuine Socket.IO client paths: explicit protocol, Bearer header, Engine.IO open, namespace connect, ping/pong, exact event tuple, bounded sessions and no command replay. Native and Android accept only fresh session/board/boot-bound SHA-pinned offers from the configured download origin, not generic remote RPC.
- Omitted/replaced code: `Remaining donor mechanisms outside this delivery slice`. Disposition: **pending**. Reason: Generic reciprocal JSON-RPC/Socket.IO command routing, board mirroring, hotkeys/webhooks, durable generic command queues and the rest of this host integration manager remain pending. Foreground firmware automation now supports bounded read-only feed reconnect with fresh sessions and persisted consumed digests; that is not generic command replay or full host-manager parity.
- Test-code evidence: `desktop/src/firmware_feed.rs:719 [UTF8-LF sha256=4eee358ef83fb6c6a5bd17d1ebed3683fd203939f2ff3feab7575c1a5a9f330f; anchor=fn firmware_feed_actual_websocket_and_socketio_sessions_deliver_once]`.
- Test-code evidence: `mobile/test/firmware_delivery_test.dart:272 [UTF8-LF sha256=2a5a1ff4950f3806d522d2afe76050884aff03a7f3f0556abc329f25a0042976; anchor=Actual WS/Socket.IO listener deduplicates offers]`.
- Remaining: Issue #2/#7: verify the owner's actual authenticated WSS/Socket.IO endpoint and emitted firmware event. Loopback protocol fixtures are not Internet/TLS server interoperability or Android background uptime.

### PCController: `Tools/Controller/internal/hostbridge/peer_response_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b77fb20899b6e43e3ea8bebf0b49e0ab92849eb45f7ba6fe8803508c76c03c28`.

- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Retained: Read through EOF. Adapt malformed/foreign identity rejection, same-ID receipt retrieval and command mismatch tests to the existing Pakar packet; actual HTTP and WS tests share the same observed board.
- Omitted/replaced code: `Full donor JSON-RPC error/result ambiguity and pending peer-session tests`. Disposition: **pending**. Reason: The donor IPC envelope is not exposed by this selected-owner bridge; full PCController manager interoperability and its tests are still useful pending work.
- Test-code evidence: `desktop/src/bridge.rs:1288 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_network_repeater_confirms_upstream_usb_readback_and_preserves_both_hops]`.
- Test-code evidence: `desktop/src/bridge.rs:1160 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_http_recovery_uses_existing_packet_form_contract]`.
- Remaining: Issue #2: Full donor JSON-RPC error/result ambiguity and pending peer-session tests. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/hostbridge/peer_response.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7e711f22ce5a8e0c01f7a935e0f0091ed1d305e04a7563aae57a772059f391ef`.

- Target: `desktop/src/bridge.rs:103 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=pub struct Runtime]`.
- Target: `desktop/src/usb_table.rs:160 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=pub struct Intent]`.
- Retained: Read complete source. Adapt exact request-ID correlation, ambiguous/malformed result as uncertain, consume-before-dispatch and no replay. New SerialCommand::Bridge queues through the sole existing USB owner and returns actual same-sequence ACK plus readback, not queue acceptance.
- Omitted/replaced code: `Generic PCController peerRPCSession JSON-RPC/WebSocket/Socket.IO response envelope`. Disposition: **pending**. Reason: Pakar uses its existing living /api/command and board/boot/deadline contract, not an unimplemented promise of generic PCController RPC compatibility. Full manager.go route/authority/transport adapters remain pending review.
- Test-code evidence: `desktop/src/bridge.rs:1288 [UTF8-LF sha256=be11df49c5a9ec38ee0f7121ddaba2e9f74db92979ffd7270cf0e3480820347b; anchor=fn bridge_network_repeater_confirms_upstream_usb_readback_and_preserves_both_hops]`.
- Test-code evidence: `desktop/src/usb_table.rs:240 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn actual_usb_worker_preempts_large_preview_and_confirms_compact_board_readback]`.
- Remaining: Issue #2: Generic PCController peerRPCSession JSON-RPC/WebSocket/Socket.IO response envelope. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### PCController: `Tools/Controller/internal/hostui/update_progress_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `421091e24db939664eea6534d5f12357173b75e4dbe196037bac03fb1bda9bab`.

- Target: `desktop/src/firmware_progress.rs:78 [UTF8-LF sha256=a35d8ed50dbd2b657138e4a0eca981ad9098e604a36857282b87568f9a781996; anchor=pub struct Observation]`.
- Target: `mobile/lib/firmware_progress.dart:8 [UTF8-LF sha256=51631d76da67f123c5419eb44d4d688c11ee162bf602ba6769e366a840d7348d; anchor=class FirmwareProgress]`.
- Retained: Read the complete actual donor tests; preserve measured-only percentage, terminal state and not-conflating transfer with flash completion assertions in native/Dart/Web fixtures. Existing notification tests are not represented as ported.
- Omitted/replaced code: `Remaining donor mechanisms outside this delivery slice`. Disposition: **pending**. Reason: Donor notification title/milestone coalescing and OS taskbar tests remain pending until their actual native presentation adapter is integrated.
- Test-code evidence: `desktop/src/firmware_progress.rs:370 [UTF8-LF sha256=a35d8ed50dbd2b657138e4a0eca981ad9098e604a36857282b87568f9a781996; anchor=fn firmware_progress_remote_counts_are_not_flash_percentage_or_another_boot]`.
- Test-code evidence: `mobile/test/firmware_delivery_test.dart:81 [UTF8-LF sha256=2a5a1ff4950f3806d522d2afe76050884aff03a7f3f0556abc329f25a0042976; anchor=Actual progress widget displays indeterminate verification]`.
- Remaining: Issue #7: complete actual OS notification/taskbar adapter and corresponding fixtures; no broad donor test-suite parity.

### PCController: `Tools/Controller/internal/hostui/update_progress.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a98f677b1d3975ecb734db675ea3623d59380b0c6eac8b51f6ea86d3fc32f757`.

- Target: `desktop/src/firmware_progress.rs:121 [UTF8-LF sha256=a35d8ed50dbd2b657138e4a0eca981ad9098e604a36857282b87568f9a781996; anchor=pub fn observed(]`.
- Target: `mobile/lib/firmware_progress.dart:8 [UTF8-LF sha256=51631d76da67f123c5419eb44d4d688c11ee162bf602ba6769e366a840d7348d; anchor=class FirmwareProgress]`.
- Target: `firmware/esp8266/src/WebUi.h:895 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function firmwareUpdateObservation(s)]`.
- Retained: Adapt actual explicit measured stage counters, indeterminate non-transfer stages, separate verification/completion and terminal state handling. Rust retains external board update/local OTA state in HTTP/WS/USB DTOs; Flutter consumes the existing status stream, and Web renders that same source. No transfer 100 percent is treated as completed flashing.
- Omitted/replaced code: `Remaining donor mechanisms outside this delivery slice`. Disposition: **pending**. Reason: This slice does not port the donor's per-operation milestone toast tracker, Windows taskbar COM ordering or terminal OSC progress presenter. Existing native notifications remain, but complete progress-notification/OS presentation parity is pending.
- Test-code evidence: `desktop/src/firmware_progress.rs:352 [UTF8-LF sha256=a35d8ed50dbd2b657138e4a0eca981ad9098e604a36857282b87568f9a781996; anchor=fn firmware_progress_observes_external_ota_without_inventing_completion]`.
- Test-code evidence: `mobile/test/firmware_delivery_test.dart:47 [UTF8-LF sha256=2a5a1ff4950f3806d522d2afe76050884aff03a7f3f0556abc329f25a0042976; anchor=External board OTA and remote jobs]`.
- Remaining: Issue #7: verify live external OTA progress, reboot/readback and error/cancellation presentation on both native and Android; source/fixtures are not physical update or display acceptance.

### PCController: `Tools/Controller/internal/installer/desktop_ownership.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `8bb637e17aabb002114694ea783e2631fbb2b32e12bf285a60ec840d232c75b3`.

- Target: `desktop/src/client_install.rs:268 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=pub fn helper(]`.
- Retained: Actual explicit desktop installation ownership adapted to self-only canonical EXE/profile/owner UUID marker, current Windows SID checks and path-bound update journal; no discovery grants removal ownership.
- Omitted/replaced code: `Donor-wide installer/recovery/platform acceptance outside this self EXE slice`. Disposition: **pending**. Reason: Force-kill and automatic deletion of rollback are deliberately not copied: a live editor or unverified new process must remain recoverable. Media/libmpv, archive/GitHub extraction, interactive TUI console inheritance do not apply to this GUI raw same-origin executable. Generic signed installer/removal inventory, unattended uninstall and true two-new-version helper/Windows-session/OS acceptance remain pending. Preference UI uses actual existing Pakar donor widgets; no full donor GUI parity is claimed.
- Test-code evidence: `desktop/src/client_management.rs:865 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=release_policy_checks_numeric_build_semver_platform_origin_and_integrity]`.
- Test-code evidence: `desktop/src/client_install.rs:551 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=journals_bind_all_transaction_paths_and_parent_without_shell_commands]`.
- Remaining: Issue #7: live secure enrollment, release/telemetry/command receipt and safe replacement proof separated from source fixtures; Android foreground-only and self-uninstall local/OS-consent boundaries remain.

### PCController: `Tools/Controller/internal/ipcjson/artifacts_http.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7110d10ee2ca3e7bf6c45041a50a5c5184b044c0014577b0e517294c95228d51`.

- Target: `services/client-management/service.mjs:90 [UTF8-LF sha256=279a7a9d9e699831b0d19128ab89aac1610d1c8cdc3994c99f8f9c12e8baa795; anchor=export async function createService]`.
- Target: `tools/publish-client-release.mjs:24 [UTF8-LF sha256=30d707b4032dae9d2ab01b8ea6bc322658ef6eee314647af02f90e42fb8d4b37; anchor=const metadata=]`.
- Retained: Actual artifact descriptor and same-open-handle HTTP serving adapted to explicit filename/hash identity, exact bytes/ranges and no privileged public inventory.
- Omitted/replaced code: `Generic donor artifact store beyond self application packages`. Disposition: **pending**. Reason: This temporary service stores Windows self-executable and Android self-APK only; generic board artifact kinds/current pointers, resumed downloads and Go os.Root API are not copied. Node loopback-only service uses an exclusive trusted-writer directory, same-open handle and NOFOLLOW; clients always verify exact digest/bytes. Background Android services and two-installed-version acceptance remain pending.
- Test-code evidence: `services/client-management/service.test.mjs:10 [UTF8-LF sha256=a8f7d7976b732ef66fc3be887e7d657254409a1b94e9620c329b53ad8067feb5; anchor=actual management HTTP]`.
- Remaining: Issue #7: actual server/client rollout receipts and Android installed consent/multi-device acceptance; no arbitrary shell execution or uninstall of unrelated packages.

### PCController: `Tools/Controller/internal/ipcjson/rest_routes_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `729b980122ce5e3b3024544c9229d52ce03647bf195d62958fa388304d44fbe8`.

- Target: `libraries/BoardCore/src/ApiPolicy.h:8 [UTF8-LF sha256=ee7dcf1c76d2d1703ae2d8d11ab0feff19650f08e8cfddfa03faac17ba6a163c; anchor=inline bool numberedRoute]`.
- Target: `firmware/esp8266/src/WebAccess.h:36 [UTF8-LF sha256=7c87b0f4e3f4cc849da343e7da39fa946ee68716a7181b0a2ee1241cb4579cab; anchor=ApiPolicy::numberedRoute]`.
- Retained: Actual TestVersionedCoreAPIRoutesAreRejected test cases adapted into the C++ helper exercised by ESP async middleware; generalized to all numbered API roots and pre-auth rejection.
- Omitted/replaced code: `TestCanonicalRESTRouteInventory, TestRESTAppActionCannotInjectCoordinatorNavigationMetadata, typed app launch/action outcome lifecycle`. Disposition: **pending**. Reason: These exercise the full PCController Go host coordinator, not the ESP handlers. Applicable generic typed action/outcome/auth ownership and full real multiplexer route inventory still need explicit Pakar contracts and tests. The narrower source/wire tests here do not claim end-to-end async server acceptance.
- Test-code evidence: `tests/protocol_test.cpp:64 [UTF8-LF sha256=a63655790934d54477020e7efd81217fb07678df58fdde5307ecb865978d2e27; anchor=Actual donor TestVersionedCoreAPIRoutesAreRejected cases]`.
- Test-code evidence: `tests/living_contract_test.mjs:26 [UTF8-LF sha256=55c421193701d61071019dccee676a0f8e7388518992dbb30d0ac8c3ddcbb676; anchor=pre-auth 404 wiring]`.
- Remaining: Port applicable real top-level route inventory, typed app outcomes and coordinator-owned metadata spoof tests after mapping the actual Pakar host coordinator.

### PCController: `Tools/Controller/internal/lanresolver/resolver_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b03498f9df5a9f8e5bc3c94eabec71ab4fb412b53c1fb1729fe63a45ec1ba3b9`.

- Target: `mobile/test/selection_batch_network_test.dart:19 [UTF8-LF sha256=666769b01c6b3468061219109c98ffeaed7b83ca4866180e28afb902f84a77dc; anchor=test(]`.
- Target: `desktop/src/peer_http.rs:583 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn local_address_cache_accepts_only_verified_tcp_peer_not_advertised_ip]`.
- Retained: Adapted actual cached-address reuse and changed-DHCP-address scenarios with injected lookups/dials. Added actual loopback HTTP Host identity, authentication/timeout retention, selection/batch/readback and melody fixtures.
- Retained: Native fixtures exercise actual reqwest loopback TCP, cached subsequent resolution, original HTTP Host and explicit port, provenance rejection, finite cache/replacement and invalidation. They are not Windows mDNS, Android DNS or DHCP physical acceptance.
- Omitted/replaced code: `Go net.TCPConn placeholders, netip addresses and go test runtime`. Disposition: **replaced**. Reason: Tests exercise the Dart resolver used by this application and a real local HTTP listener; Go donor tests are not executed as Flutter evidence. Physical phone/Wi-Fi and audible melody acceptance remain separate.
- Test-code evidence: `mobile/test/lan_resolver_transport_test.dart:12 [UTF8-LF sha256=30279615272e766fefe9d14104e90596962b5924a61ae975da4778c53042703f; anchor=void main()]`.
- Test-code evidence: `mobile/test/selection_batch_network_test.dart:19 [UTF8-LF sha256=666769b01c6b3468061219109c98ffeaed7b83ca4866180e28afb902f84a77dc; anchor=test(]`.
- Test-code evidence: `desktop/src/peer_http.rs:622 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn local_address_cache_bounds_replaces_and_invalidates_without_ttl_or_authority]`.
- Scoped advantage: Adds failed-resolution-only eviction and retains no mutation replay.
- Remaining: Focused Flutter fixtures pass; actual phone/LAN, physical color and audible melody acceptance remain open.

### PCController: `Tools/Controller/internal/lanresolver/resolver.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `8dec2b65850f9bfecf71c510f9f29392cc09def0e9590568d61ae982707cb4d7`.

- Target: `mobile/lib/lan_resolver.dart:8 [UTF8-LF sha256=d7d1589f6fca4c5385acd88f9b3dec98504c82e32558a35d17dbc89202eaed66; anchor=class LanResolver]`.
- Target: `mobile/lib/api.dart:151 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=final LanResolver resolver;]`.
- Target: `mobile/lib/discovery.dart:106 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=class DiscoveryCatalog]`.
- Target: `mobile/lib/devices_page.dart:62 [UTF8-LF sha256=47b896ab775091042ee0d9ff0607969ec7885e6fa40638ea494dc0a903d947e7; anchor=catalog]`.
- Target: `desktop/src/peer_http.rs:155 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=pub fn device_client_builder]`.
- Target: `desktop/src/main.rs:639 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=peer_http::remember_address]`.
- Retained: Actual complete donor resolver and tests read from canonical PCController; adapted successful-address caching, retaining URL hostname/HTTP Host/TLS SNI rather than replacing the public identity with an IP.
- Retained: Flutter HTTP and WebSocket share one resolver. TCP connect failures may trigger bounded read-only relookup but retain the last successful address; actual DNS SocketException/empty answer invalidates it. HTTP/auth/timeout failures do not evict or trigger discovery/replay. Cached route data never grants board/boot ownership.
- Retained: Actual complete resolver reread at unchanged source hash for measured ASUS .local latency. A separate bounded32-host native cache accepts only actual response TCP remote_addr after valid board/boot and requested-hostname checks, retains URL Host/SNI, never trusts advertised IP and never grants command ownership. Fresh no-idle-pool clients reuse verified routing. Only typed dial/resolution failures mark a retained hint for later relookup; HTTP/auth/body timeouts do not discard it. Successful verified TCP replacement clears the bounded lookup mark; no mutation is replayed.
- Retained: Actual pinned tungstenite0.30 TLS source was inspected for original-URI SNI and normal root verification. Native WS now dials numeric endpoints or the same actual-TCP cache (including verified configured DNS aliases; .local labels still match the board). A missing hint waits quietly for the existing selected HTTP worker: no duplicate WS HTTP/DNS runtime or resolver thread. TCP timeout and absolute TLS/HTTP-upgrade deadline precede handshake; selected-route revocation is checked on each IO. Both Plain and Rustls use bounded runtime reads. Actual loopback seeding/Host, empty-cache/no-socket, mismatch/revocation and deadline fixtures cover this adaptation; TLS validation and physical LAN remain separate proof.
- Omitted/replaced code: `Donor forget(key,cached) on TCP dial error`. Disposition: **replaced**. Reason: The user explicitly requires retaining cached hostname routing until actual name-resolution failure. A failed dial performs read-only lookup without evicting the previous route; successful new-address dialing may replace it. No HTTP mutation is replayed.
- Omitted/replaced code: `Go mutex, process-global resolver, Linux platform NBNS fallback and unbounded address candidates`. Disposition: **replaced**. Reason: Dart isolate ownership and a shared per-API resolver replace Go synchronization/global state. DNS and each dial have deadlines, successful cache is bounded64 hosts and candidates2; platform discovery remains separate. Android OS/mDNS resolution and DHCP movement still need actual phone/LAN acceptance; no platform helper or multicast success is inferred.
- Test-code evidence: `mobile/test/lan_resolver_transport_test.dart:12 [UTF8-LF sha256=30279615272e766fefe9d14104e90596962b5924a61ae975da4778c53042703f; anchor=void main()]`.
- Test-code evidence: `mobile/test/selection_batch_network_test.dart:19 [UTF8-LF sha256=666769b01c6b3468061219109c98ffeaed7b83ca4866180e28afb902f84a77dc; anchor=test(]`.
- Test-code evidence: `desktop/src/peer_http.rs:711 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn local_verified_address_override_preserves_http_hostname_and_explicit_port]`.
- Scoped advantage: Retains the requested cache policy without treating authentication or generic HTTP failures as DNS failure.
- Remaining: Verify physical Android DNS-SD/mDNS resolution, reconnect/DHCP change, TLS hostname identity and retained discovery records under real network loss. Preserve explicit Disconnect and identity fencing.

### PCController: `Tools/Controller/internal/link/request_admission_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `0148ea4aac3cf230050045756a4308157adf786a2b6ca2d4fd99bcf665a622c1`.

- Target: `desktop/src/connection.rs:25 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=pub struct Gate]`.
- Target: `desktop/src/main.rs:1455 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=fn serial_worker(]`.
- Target: `desktop/src/usb_table.rs:42 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=pub struct Operation]`.
- Retained: Fully read all489lines through EOF. Adapted queued-intent revocation/no-late-write and ownership-before/after admission assertions as Rust selected-method epoch tests. The production Gate keeps revoked owners until their resource leases drop and never admits a competing route or a second USB handle.
- Retained: New main fixture invokes the actual HTTP worker against a live loopback listener while USB is selected, asserting zero accepted connections and zero failure events. This is an unexecuted source fixture reference until the guarded owner runs tests, not live USB or electrical proof.
- Retained: Reread current canonical Session, fakePort and request_admission_test.go. Adapt actual control-over-background admission between unsent USB snapshot chunks, without overtaking an in-flight request or post-ACK verification. The production Rust worker now validates matching sequence/opcode ACK plus compact same-boot board-observed state, rather than delaying confirmation for all480 RGB pixels, mappings and icon metadata. Full captures still own capability/peripheral freshness and actual RGB; partial readback does not renew those observations. Missing companion uses read-only full verification, never mutation replay. Production-worker fragmented-port and wrong-boot/scalar/state fixtures cover this scoped adaptation, not all donor reset/open/750ms strip contracts.
- Omitted/replaced code: `Priority/background strip scheduling,750ms ACK bound, active-opcode wait diagnostics and delayed/concurrent exact ACK matching`. Disposition: **pending**. Reason: USB control priority, correlated ACK and compact board readback now have production-worker fakePort coverage. Full donor750ms strip-specific deadline, delayed/concurrent request matrix and OS acquisition/close/reset ownership still require dedicated adaptation and acceptance.
- Omitted/replaced code: `PulseDTR cancellation/restoration/write exclusion and live physical port acquisition/late-close tests`. Disposition: **pending**. Reason: This pass intentionally adds no automatic reset and keeps AVR/ISP/audio deferred. Inactive DTR on Rust open is source configuration, not measured electrical behavior; OS acquisition/cancellation/retryable-close ownership still needs dedicated fake-handle and real-host acceptance.
- Test-code evidence: `desktop/src/connection.rs:95 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=fn handoff_waits_for_all_actual_old_resources_and_rejects_stale_epoch()]`.
- Test-code evidence: `desktop/src/connection.rs:83 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=fn usb_offline_never_admits_network_and_disconnect_revokes_everything()]`.
- Test-code evidence: `desktop/src/main.rs:3780 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=fn usb_selected_http_worker_never_contacts_even_a_live_network_endpoint()]`.
- Test-code evidence: `desktop/src/usb_table.rs:240 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn actual_usb_worker_preempts_large_preview_and_confirms_compact_board_readback()]`.
- Test-code evidence: `desktop/src/usb_table.rs:201 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn compact_usb_readback_validates_all_values_without_renewing_full_observations()]`.
- Scoped advantage: User-required exclusive Wi-Fi-or-USB lifecycle replaces Both rather than silently falling back; physical resources retire before the new method opens. Source evidence only until guarded tests and runtime observations.
- Remaining: Run guarded native tests and verify selected USB causes no HTTP/DNS/SSDP/telemetry or second physical connection; validate bounded route retirement, dirty-draft protection and retired outcomes in actual runtime. Do not infer whole donor parity or electrical acceptance from Gate tests.

### PCController: `Tools/Controller/internal/link/session_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `ff6c7b21f72bfb944d232c87210f8ffa139dfe016e8b5756f2582604489fffc4`.

- Target: `tests/protocol_test.cpp:47 [UTF8-LF sha256=a63655790934d54477020e7efd81217fb07678df58fdde5307ecb865978d2e27; anchor=Adapt donor matching-identity/request assertions]`.
- Target: `desktop/src/terminal_ui.rs:1345 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn server_reboot_between_preflight_and_post_stops_without_replay()]`.
- Target: `desktop/src/peer_http.rs:1019 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_settings_pin_every_nested_request_and_verify_same_boot()]`.
- Target: `desktop/src/usb_table.rs:42 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=pub struct Operation]`.
- Retained: Actual fakePort matching/mismatched request and identity scenarios read fully and adapted to C++ peer validation and native same-host reboot/pinned-header/409 no-replay HTTP fixtures.
- Retained: Old receipts stay in Events, not new peer's confirmed output.
- Retained: Full session_test.go reread through EOF and live hash reverified. Admission revocation, peer mismatch and no-replay assertions adapted to actual native GUI HTTP loopback fixtures, not claims of importing Go fakePort acquisition/reset/ACK routing.
- Retained: Reread complete donor at unchanged live hash; mismatch/admission assertions now also execute real WebAccess middleware and AsyncHttpService with SDK/transport/auth doubles. Retired queued probe never reaches the simulated hardware call or replays. This does not validate Basic-auth parsing, physical ISP or downstream AVR epoch.
- Retained: Reread current canonical Session, fakePort and request_admission_test.go. Adapt actual control-over-background admission between unsent USB snapshot chunks, without overtaking an in-flight request or post-ACK verification. The production Rust worker now validates matching sequence/opcode ACK plus compact same-boot board-observed state, rather than delaying confirmation for all480 RGB pixels, mappings and icon metadata. Full captures still own capability/peripheral freshness and actual RGB; partial readback does not renew those observations. Missing companion uses read-only full verification, never mutation replay. Production-worker fragmented-port and wrong-boot/scalar/state fixtures cover this scoped adaptation, not all donor reset/open/750ms strip contracts.
- Omitted/replaced code: `serialOpenWithPending/late acquisition/configuration cancellation/close retry, preclose ordering and terminal error ownership; corresponding fakePort tests`. Disposition: **pending**. Reason: Useful serial lifecycle/quarantine has no equivalent in HTTP-only TUI. Adapt with actual Rust USB owner before discovery/reconnect; never close another client's port.
- Omitted/replaced code: `priority requestAdmission, sequence/opcode-matched ACK/Error/events, reserved macro sequence, boot HELLO retry and recoverable decoder overflow tests`. Disposition: **pending**. Reason: HTTP fencing is not downstream AVR ACK/decoder ownership. These production behaviors and exact tests remain pending with USB/UART host integration.
- Omitted/replaced code: `compact HELLO schemas/build stamp and DTR reset/inactive control-line tests`. Disposition: **pending**. Reason: Build stamp is not board/boot identity; numbered public generation negotiation conflicts with living API. Adapt additive identity and explicit one-shot reset policy with serial owner, without schema selectors/reset-on-connect.
- Test-code evidence: `desktop/src/terminal_ui.rs:1415 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn shared_cli_mutation_inherits_pinned_epoch_headers()]`.
- Test-code evidence: `tests/peer_fence_test.mjs:23 [UTF8-LF sha256=b5ebf8bad8769de3f9c8b22c9d4e0815644266e30a4e64c600f09dafd505f973; anchor=Upload callbacks precede middleware]`.
- Test-code evidence: `desktop/src/peer_http.rs:1048 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_preflight_reboot_or_capability_loss_never_dispatches_mutation()]`.
- Test-code evidence: `desktop/src/peer_http.rs:1081 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_readback_from_new_boot_is_not_applied()]`.
- Test-code evidence: `desktop/src/peer_http.rs:1140 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_unfenced_mutation_fails_before_network_and_preserves_correlation()]`.
- Test-code evidence: `desktop/src/peer_http.rs:1065 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_server_boot_conflict_is_not_replayed()]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:110 [UTF8-LF sha256=224df11ea7e8a2198e172337822b8d0843398e751458e754c69f0accf807a019; anchor=Real async queue + real fence recheck]`.
- Test-code evidence: `desktop/src/usb_table.rs:240 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn actual_usb_worker_preempts_large_preview_and_confirms_compact_board_readback()]`.
- Test-code evidence: `desktop/src/usb_table.rs:201 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn compact_usb_readback_validates_all_values_without_renewing_full_observations()]`.
- Remaining: GUI HTTP and Web pin adoption implemented as bounded slices; native WebSocket/USB/mobile/standalone CLI/cloud fencing, remembered pairing and AVR-only boot identity remain pending; ESP32-S3 application not integrated.
- Remaining: Port remaining useful serial lifecycle/priority/ACK/reset tests; fixtures/source/compile are not physical acceptance.

### PCController: `Tools/Controller/internal/link/session.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `1311fdcde94b2ef8ed06db63ff874a255c135dc1969709e6f43ff4b5c5c8235b`.

- Target: `desktop/src/terminal_ui.rs:773 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn command_request(]`.
- Target: `mobile/lib/api.dart:1219 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=bool _acceptIdentity(]`.
- Target: `mobile/lib/main.dart:154 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=api.onPeerChanged = ()]`.
- Target: `mobile/lib/bridge.dart:327 [UTF8-LF sha256=14bc9c91fb4dd852ab7ef52e2ab7d79c83ad3607a463a8fb72e667faaff7fc57; anchor=watchdog = Timer.periodic]`.
- Target: `libraries/BoardCore/src/PeerFence.h:31 [UTF8-LF sha256=9f1ab63252bd881d2fc786e230a18db331e66708a1bd3ccd0f096d2d2b5d49d5; anchor=inline bool accepts(]`.
- Target: `firmware/esp8266/src/WebAccess.h:91 [UTF8-LF sha256=7c87b0f4e3f4cc849da343e7da39fa946ee68716a7181b0a2ee1241cb4579cab; anchor=bool peerAllowed(]`.
- Target: `firmware/esp8266/esp8266.ino:1610 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=bool authenticate()]`.
- Target: `assets/web/peer-http.js:90 [UTF8-LF sha256=04dd5da95667de82bc556e005db5fc72b7d31ebafbda7b605590e8b80b120aa6; anchor=function assertOwner(peer)]`.
- Target: `desktop/src/peer_http.rs:338 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=pub fn admit(]`.
- Target: `desktop/src/main.rs:659 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=fn scoped_http_request(]`.
- Target: `desktop/src/usb_table.rs:146 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=pub struct Admission]`.
- Target: `mobile/lib/command_readback.dart:6 [UTF8-LF sha256=385f0a578be88fa4c2d5fccbcff8871f02d794b26d96f566e3146e7931414615; anchor=bool commandReadbackMatches(]`.
- Target: `desktop/src/cell_batch.rs:11 [UTF8-LF sha256=c84a71183e77cfa6cf4cade8a79cf103d51d3277ff24120a695f13b7e5eb3e0f; anchor=pub struct Correlation]`.
- Target: `desktop/src/realtime.rs:8 [UTF8-LF sha256=56b3672d6080bde9e3b13f23426c49e3d6c57e8b3d05ec357947593644a41c83; anchor=pub struct Intent]`.
- Target: `mobile/lib/command_wire.dart:18 [UTF8-LF sha256=936dcce26bf42522f11ffd1c2c7c864161edb8a8f22ce0d17a40a15e2eb21eda; anchor=String commandPacket(]`.
- Target: `libraries/BoardCore/src/CommandReceipts.h:8 [UTF8-LF sha256=13bfe0feee207ee0d3c5d2d98111728e9c7ef99c5628206aa96f4b503d0ccd5c; anchor=class CommandReceipts]`.
- Target: `mobile/lib/table_workspace.dart:11 [UTF8-LF sha256=7167553e1f06bd93e60d6403ee6ac6268af8b2b72a840368902b99f698786149; anchor=class TableWorkspace]`.
- Target: `libraries/BoardCore/src/BleWire.h:31 [UTF8-LF sha256=e6b8a769227fed14658e25d2f21be2773e2607902abd79cf91cd03eda22c7230; anchor=class Receiver]`.
- Target: `firmware/esp32s3/src/BleService.h:70 [UTF8-LF sha256=5554293eb8f56b1f8300441ec74e63a2185a6a5ec16c39916fdb8d6ef67b8e0f; anchor=class BleService]`.
- Target: `firmware/esp32s3/src/BleStorage.h:21 [UTF8-LF sha256=1b0b11d0d1f7f398699629ccdbd32d11e3308aef5f738f5963fcfd1d176d81e8; anchor=class BleBulkOwner]`.
- Target: `firmware/esp8266/src/BleRoutes.h:202 [UTF8-LF sha256=33c4470a196f59016b86d39a25015579bff5f02401c1c7d791b44370a285e91f; anchor=String bleRequest(]`.
- Target: `services/remote-panel/gateway.mjs:11 [UTF8-LF sha256=38caebfa7b6f923b9214f0e83f069f88bf401f9abde8a165ee4c7f42357ec3f6; anchor=export class Gateway]`.
- Target: `services/remote-panel/service.mjs:13 [UTF8-LF sha256=7999fa23d55db9a7760350308356bb90c4133d1ea670e064ef38f4c5f1c8adce; anchor=export function createPanel]`.
- Target: `desktop/src/peer_identity.rs:359 [UTF8-LF sha256=4895d4ce1dc5f77e1e866600349baef3ba131e24d23970e254a968442e663dfe; anchor=pub fn usb_introduction_error(]`.
- Retained: Actual Session.Request closing/context checks before and after admission adapted to TUI atomic stop checks before/after fresh preflight; no post-dispatch replay.
- Retained: Full source read. Fail-closed identity checks strengthened with HTTP server-side board/boot fences; donor HELLO/build stamp is NOT a boot epoch.
- Retained: Before/after admission ownership checks also adapted to Web's shared pinned HTTP preflight and post-response generation validation. Existing session.go full review/live 1bf4d52 hash remains unchanged; this is not import of Go serial acquisition or request-priority scheduling.
- Retained: Full session.go reread through EOF and live source hash reverified. Actual acquireWrite/requestAdmission before/after admission closing checks adapted to native HTTP queued generation, deadline and post-preflight ownership checks. Shared native/TUI strict Pin validation removes the duplicate validator; pinned clients refuse redirects.
- Retained: Reread complete donor at unchanged live hash for firmware admission/dispatch ownership. Supplied HTTP headers are checked for all methods including hardware-affecting GET probe; actual loop dispatch rechecks the same request before SPI/reset. Header-free compatibility is deliberately unfenced, not ownership proof.
- Retained: Fully reread current session.go and session_test.go for Android networking. Adapted actual SetBeforeClose/notifyBeforeClose synchronous retirement into mobile HTTP/WebSocket board/boot acceptance, root state revocation and discarded prior firmware owners. Learned critical identity cannot silently disappear into legacy readiness. A one-second bridge watchdog is independent from the chosen telemetry interval, and retired MQTT callbacks cannot publish through a replacement client.
- Retained: Canonical b0111ae0 session.go916lines fully reread through EOF; old1bf4d52 review remains historical. Existing bounded HTTP/mobile ownership targets do not imply current whole serial-session parity. Frozen native transport targets were separately reviewed: selected-method epoch Gate/Stamp/Lease, before-write lease checks, actual USB handle drop before handoff, HTTP/WS lease retention, no idle HTTP connection pool, no Both fallback and stale-event retirement.
- Retained: Matched opcode/sequence admission and no-replay principles now also govern actual USB table commands. UI-captured connection stamp, physical-open session, board/boot pin and enqueue time survive the queue unchanged; admission is checked before queueing and before writing. Frozen snapshot chunks validate nonce/token/offset/size, retain capture-start freshness, and a new same-peer capture after ACK verifies requested values. This is a scoped Rust adaptation, not all donor priority/reset/AVR contracts.
- Retained: Mobile explicit Disconnect retires polling, retries and old live callbacks. Cell/output/mode/effect/brightness mutations require fresh preflight and post-ACK readback; failed confirmation stops bulk without optimistic success. Android direct USB is not implemented or offered as a working connection.
- Retained: Actual last-published 480-pixel RGB now travels in the same bounded frozen USB capture through the existing Frame parser, with capture-start freshness, matching mapping and current-peer admission. USB readback also synchronizes observed effect; there is no Wi-Fi fallback or fabricated six-pixel preview. This newer source passes197 native tests but is not yet deployed because the ASUS tunnel failed.
- Retained: Stress audit fixes same-count foreign receipts: immutable local operation ID, exact selection/state/effect, Pin and route survive HTTP/USB admission/result. UI compares its own correlation; USB rejects payload/admission mismatch. Full donor priority scheduling is not claimed.
- Retained: Reread the full current Session source for the live ASUS queue failure. Actual grantNextLocked highest-priority/strict-greater FIFO selection is now adapted to a bounded64-entry native HTTP waiting window: foreground commands overtake unsent status/mapping and optional configuration work, without preempting an in-flight request. Mutations retain8s expiry and revocable generation/board/boot fencing; read-only work has a separate24s bound. Atomic batches reuse the worker's fresh preflight rather than running it twice, retaining final admission, one POST, ACK and same-boot readback. This does not import USB sequence allocation, DTR/reset defaults or serial priority scheduling.
- Retained: Actual Session admission/matched-ACK/no-replay principles now guard WebSocket-first native/mobile commands. One random intent ID, bounded boot-relative deadline and exact packet digest survive same-intent HTTP recovery. The loop-owned RAM receipt cache consumes before mutation and never evicts an unexpired ID. Immediate same-board state accompanies each ACK; other clients receive immediate state pushes. Older HTTP reads cannot roll newer state back. Flutter keeps both scroll controllers and a fixed result slot through pending/completed writes without renewing expired telemetry. This scoped adaptation does not import serial sequence allocation, reset defaults or all donor contracts.
- Retained: Separate command authority from board-keyed Flutter user workspace state: lifecycle pauses timers, not a healthy socket; resume probes the same socket before HTTP fallback; silent sockets have independent WS freshness and bounded reconnect. Stale or foreign leases disable writes without destroying selection, search, drafts or scroll. Hidden settings no longer issue periodic network/AVR reads. No old mutation is queued or replayed.
- Retained: Reread current canonical Session, fakePort and request_admission_test.go. Adapt actual control-over-background admission between unsent USB snapshot chunks, without overtaking an in-flight request or post-ACK verification. The production Rust worker now validates matching sequence/opcode ACK plus compact same-boot board-observed state, rather than delaying confirmation for all480 RGB pixels, mappings and icon metadata. Full captures still own capability/peripheral freshness and actual RGB; partial readback does not renew those observations. Missing companion uses read-only full verification, never mutation replay. Production-worker fragmented-port and wrong-boot/scalar/state fixtures cover this scoped adaptation, not all donor reset/open/750ms strip contracts.
- Retained: Reread actual Session admission/ACK ownership for the ten-minute ASUS failure. Native immutable reads use a verified-token four-chunk window and one absolute8s capture/freshness deadline, not a premature2s per-fragment timeout. Mutation ACK/deadline/no-replay rules are unchanged. Capture progress is a bounded typed owner-IPC observation, not toast identity; verified same-board presentation survives read failure and wire reacquisition without renewing old hardware authority.
- Retained: Bluetooth slice: fully reread canonical b0111ae0 Session source (SHA1311fdcde...), not the old ../repos copy. Adapted RequestWithPriority/acquireWrite/WithRequestWriteGuard, consumed single-inflight admission and synchronous close fencing to S3 main-loop GATT ingress and targeted replies. Existing CommandReceipts and shared business/configuration/update handlers remain authoritative; fragment ACK is not a command receipt.
- Retained: Memory-pressure review retains those exact ownership/wire rules while moving all eight BLE ingress slots and the unchanged whole Receiver/Sender into one PSRAM-only BleBulkStorage lifetime allocation before BLEDevice::init. Missing storage prevents BLE startup/admission, with no scarce-DRAM fallback. Fences/disable/reconnect reset session state but never free/replace storage underneath callbacks. Internal mutexes/atomics/callback owners, INFO bytes and SDK/LED timing-critical memory remain internal; pure BleWire protocol source is unchanged.
- Retained: Actual Arduino-ESP32 3.3.12 cores/esp32/esp32-hal-psram.c was read through EOF (original-byte SHA2562f47f8b635de8ffb64d160576a2705b063235fb9f54e01b98e8d2bf3f48352bd). Its explicit MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT allocation/no-internal-fallback boundary is used by the new Pakar owner. The Go donor does not implement S3 PSRAM or NimBLE allocation; this platform adaptation does not transplant or relabel serial buffers, and does not override the prebuilt SDK's internal-only NimBLE pool/host-stack settings.
- Retained: Reread canonical Session source at SHA1311fdcde94b2ef8ed06db63ff874a255c135dc1969709e6f43ff4b5c5c8235b for the native mapping failure. Adapt its before/after admission and matched-response ownership principles: a current pinned mapping GET validates the response producer board/boot plus the unexpired, still-current scope, without an unrelated HTTP status preflight. Foreign or incomplete producer envelopes reject without fallback. An absent envelope is discarded before the original status -> mapping -> status legacy fences run; no stale mapping survives a changed board/boot. Six actual loopback HTTP fixtures passed via the canonical native gui_mapping filter. ASUS build12 runtime separately confirmed80 mappings with no active incidents on the selected current peer; that is not a long-soak, pixel RGB, or physical cell-order acceptance.
- Retained: BLE lifecycle remains a scoped S3 SDK adaptation, not an unreviewed donor feature: missing/corrupt settings default Off; runtime enable persists intent without entering unbounded SDK initialization in the loop; disable retires late owners and honestly requires reboot to release retained SDK pools. Compile exclusion preserves loaded intent and rejects enabling unavailable hardware. Pure real-owner fixtures cover missing/corrupt records, reboot state, exclusion and request admission; no claim of SDK init timeout or BLE/TLS/OTA coexistence.
- Retained: Temporary Internet panel: canonical b0111ae0 Session source reread through EOF and blob SHA1311fdcde reverified, distinct from older ../repos checkout SHAe171bf35. Adapted matched-request/ACK ownership, pre-dispatch board/boot admission and synchronous disconnect retirement to existing MQTT CloudCommandGuard. Each QoS0/non-retained mutation is sent once and requires correlated ACK plus fresh same-boot readback. Retained data grants no control lease. Real HTTP/WebSocket auth and Origin tests cover this slice, not serial/AVR/physical LED acceptance.
- Retained: Canonical Session source reread fully for USB health; RemoteError/decodeRemoteError preserve actual correlated opcode and device code. Rust USB introduction now distinguishes invalid report/capacity, Busy, safety rejection and Unsupported instead of classifying all failures as release mismatch. Malformed replies remain explicit; no admission override or retry added. Actual ASUS reconnect passed on the current ESP; this change does not claim the historical rejection cause was reproduced.
- Omitted/replaced code: `serialOpenWithPending/late acquisition/configuration cancellation/close retry, preclose ordering and terminal error ownership; corresponding fakePort tests`. Disposition: **pending**. Reason: Useful serial lifecycle/quarantine has no equivalent in HTTP-only TUI. Adapt with actual Rust USB owner before discovery/reconnect; never close another client's port.
- Omitted/replaced code: `priority requestAdmission, sequence/opcode-matched ACK/Error/events, reserved macro sequence, boot HELLO retry and recoverable decoder overflow tests`. Disposition: **pending**. Reason: HTTP fencing is not downstream AVR ACK/decoder ownership. These production behaviors and exact tests remain pending with USB/UART host integration.
- Omitted/replaced code: `compact HELLO schemas/build stamp and DTR reset/inactive control-line tests`. Disposition: **pending**. Reason: Build stamp is not board/boot identity; numbered public generation negotiation conflicts with living API. Adapt additive identity and explicit one-shot reset policy with serial owner, without schema selectors/reset-on-connect.
- Omitted/replaced code: `WithRequestWriteGuard final transport admission;750ms strip ACK/background priority and active-opcode slot diagnostics`. Disposition: **pending**. Reason: Selected-method lease checks precede normal Rust serial writes, retaining the old physical-owner lease until resource drop. Native HTTP now adapts donor queued priority selection, but USB request-priority scheduling, reserved macro sequence and single-inflight opcode/sequence ACK ownership remain pending; OS-inflight bytes are non-retractable. Rust USB/PnP close/open cancellation, timing and no-network handoff still require runtime acceptance.
- Omitted/replaced code: `Bluetooth transport is not Go serial acquisition or a complete firmware-file transport`. Disposition: **pending**. Reason: S3 has no Classic/SPP/audio hardware. UART DTR/reset/acquisition/quarantine and macro-priority mechanisms are not GATT operations; existing serial implementation is preserved, not removed. Direct BLE firmware binary transfer/raw pixel streaming, authenticated encrypted/bonded BLE, full donor serial suites and real phone/RF/coexistence/mirroring acceptance remain pending in issue #20; remote firmware jobs reuse the existing updater and require its network route.
- Test-code evidence: `desktop/src/terminal_ui.rs:1077 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn same_hostname_reboot_revokes_drafts_and_old_receipts()]`.
- Test-code evidence: `desktop/src/peer_identity.rs:489 [UTF8-LF sha256=4895d4ce1dc5f77e1e866600349baef3ba131e24d23970e254a968442e663dfe; anchor=fn usb_introduction_error_preserves_actual_board_reason_without_suggesting_policy_bypass()]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:37 [UTF8-LF sha256=1e650a1f6897c67ce9e87de864ff28ed98e0a818ca95ba6b413897b5cd99a905; anchor=Same-host board/boot replacement retires firmware owners synchronously]`.
- Test-code evidence: `tests/peer_fence_test.mjs:23 [UTF8-LF sha256=b5ebf8bad8769de3f9c8b22c9d4e0815644266e30a4e64c600f09dafd505f973; anchor=Upload callbacks precede middleware]`.
- Test-code evidence: `tests/web_peer_http_test.mjs:24 [UTF8-LF sha256=c9c8062ace7ecba708eb2619eac096bc2cca8ee1f7cf4777794c1f824a46e9ab; anchor=Same-host reboot between draft creation and preflight]`.
- Test-code evidence: `desktop/src/peer_http.rs:1098 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_revocation_during_preflight_stops_before_write()]`.
- Test-code evidence: `desktop/src/peer_http.rs:1118 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_revocation_after_dispatch_reports_uncertainty_not_success()]`.
- Test-code evidence: `desktop/src/main.rs:674 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=// The mapping response identifies its own producer.]`.
- Test-code evidence: `desktop/src/peer_http.rs:943 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_enveloped_read_needs_no_http_status_preflight()]`.
- Test-code evidence: `desktop/src/peer_http.rs:957 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_foreign_or_previous_boot_response_is_never_adopted()]`.
- Test-code evidence: `desktop/src/peer_http.rs:968 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_incomplete_or_invalid_envelope_is_not_legacy()]`.
- Test-code evidence: `desktop/src/peer_http.rs:979 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_revoked_inflight_result_is_not_applied()]`.
- Test-code evidence: `desktop/src/peer_http.rs:991 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_expired_queue_does_not_contact_device()]`.
- Test-code evidence: `desktop/src/peer_http.rs:999 [UTF8-LF sha256=91cc8c623f63d712d9cbd12ce0cf18e4badb66c6a17490d73e8549af10d92bcd; anchor=fn gui_mapping_without_envelope_retains_both_legacy_status_fences()]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:110 [UTF8-LF sha256=224df11ea7e8a2198e172337822b8d0843398e751458e754c69f0accf807a019; anchor=Real async queue + real fence recheck]`.
- Test-code evidence: `desktop/src/usb_table.rs:446 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn table_write_is_exact_boot_fenced_and_post_ack_values_are_not_assumed()]`.
- Test-code evidence: `mobile/test/connection_lifecycle_test.dart:253 [UTF8-LF sha256=6b61da6c530ec463187fa09d6906931350d0367918f86772d1124bc1863387b7; anchor=Lost post-ACK readback stops bulk without discarding a fresh workspace]`.
- Test-code evidence: `desktop/src/ui/cell_table.rs:1180 [UTF8-LF sha256=c9369637a2243a44bdc458a96b5edd738a61a63cbc929c1f25fd57241e89f241; anchor=Even identical values require the exact operation ID]`.
- Test-code evidence: `desktop/src/realtime.rs:371 [UTF8-LF sha256=56b3672d6080bde9e3b13f23426c49e3d6c57e8b3d05ec357947593644a41c83; anchor=fn realtime_actual_ws_worker_delivers_receipt_without_http_or_poll_tick()]`.
- Test-code evidence: `mobile/test/live_realtime_test.dart:8 [UTF8-LF sha256=6af0113cf86b6f20fc9e28405bf7fbeb26a66805b5cd12f9bdb4ba39571a00f8; anchor=Explicit opt-in, real board mutation]`.
- Test-code evidence: `mobile/test/realtime_transport_test.dart:11 [UTF8-LF sha256=ef8a5a39abe37c40e3393cdde5b3f012dbc5a7adfc2e2cd104571ca04cd5de7e; anchor=Real loopback transport]`.
- Test-code evidence: `mobile/test/table_layout_test.dart:57 [UTF8-LF sha256=e410115e7e5dae033596efbbd47de71c7d541aa4a9ab95334e3d15f2e2c0ac8b; anchor=Cell commands and incoming state preserve both scroll axes and canvas position]`.
- Test-code evidence: `tests/cloud_command_guard_test.cpp:28 [UTF8-LF sha256=2de44ca916fe1670eba11edeb706e88dfdafc4994d0a9d07301a279ef71c0581; anchor=Receipts::Conflict]`.
- Test-code evidence: `mobile/test/workspace_lifecycle_test.dart:406 [UTF8-LF sha256=da99bd0f4eb2b7cac91106da4673cab150929a028dcc0001a675c51f8fb148a9; anchor=Background/resume retains selection]`.
- Test-code evidence: `mobile/test/realtime_transport_test.dart:74 [UTF8-LF sha256=ef8a5a39abe37c40e3393cdde5b3f012dbc5a7adfc2e2cd104571ca04cd5de7e; anchor=HTTP refresh cannot impersonate]`.
- Test-code evidence: `desktop/src/usb_table.rs:240 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn actual_usb_worker_preempts_large_preview_and_confirms_compact_board_readback()]`.
- Test-code evidence: `desktop/src/usb_table.rs:201 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn compact_usb_readback_validates_all_values_without_renewing_full_observations()]`.
- Test-code evidence: `desktop/src/usb_table.rs:397 [UTF8-LF sha256=49918e95eeed6198a5b387a38596ad54118b8d405cd9b5d930d7b6d5f2fd0186; anchor=fn actual_usb_worker_windows_read_latency_does_not_expire_large_capture]`.
- Test-code evidence: `tests/ble_wire_test.cpp:15 [UTF8-LF sha256=bf5675db9debf06160e8ef72d59e7c764bc3f88cde508cfcbb32a989f004c34e; anchor=int main()]`.
- Test-code evidence: `tests/ble_firmware_test.mjs:99 [UTF8-LF sha256=76d3a6d2a35ec9aec5bce1ab5efb1aeaf721ac85845494dfd5a05f9db3c54eaa; anchor=BLE firmware integration source guards]`.
- Test-code evidence: `tests/ble_storage_test.cpp:51 [UTF8-LF sha256=22f4c0e4fc0846492ddaa6c04a01e113ace607d86e9edeac1d2c69b8ee46a349; anchor=Do not retain an internal bulk-buffer fallback]`.
- Test-code evidence: `tests/ble_storage_stubs/esp_heap_caps.h:18 [UTF8-LF sha256=f6ff5f7843125cd7e780df39fd9d31006af9ef23070641ec580288235750365c; anchor=assert(capabilities==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))]`.
- Test-code evidence: `services/remote-panel/gateway.test.mjs:85 [UTF8-LF sha256=20609006ef509c878a6dcc32305c65496f0a8681b0cb011a2e86483f57d3502d; anchor=Real HTTP mutation receives one board ACK/readback]`.
- Remaining: HTTP/Web/mobile and selected USB table ownership are implemented bounded slices, not full serial/cloud parity. Remembered pairing, standalone CLI/cloud fencing, USB mapping edits, live acceptance of the newer USB pixel telemetry and AVR-only boot identity remain pending. Firmware and native USB table are live on ASUS; physical cell placement/color acceptance remains separate.
- Remaining: Port remaining useful serial lifecycle/priority/ACK/reset tests; fixtures/source/compile are not physical acceptance.
- Remaining: Measure S3 internal8-bit heap free/largest/minimum after explicit BLE PSRAM allocation and with real GATT/Wi-Fi/OTA activity. Verify missing-PSRAM startup leaves Wi-Fi/USB usable and no callback outlives its owner. Allocator/protocol fixtures and estimated BSS savings are not heap/radio/coexistence or physical programming acceptance.

### PCController: `Tools/Controller/internal/netpolicy/http_url.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f6e1c89bc499c380e2a2a1ac3c164ea31cfaf19df52667541a6b94dbcf2b0715`.

- Target: `tools/benchmark-led-api.mjs:8 [UTF8-LF sha256=67c184a28a6450dd81f41576ab7f1582092e3c151d4d0af6e9d8415d5f4c3d7f; anchor=export async function runLedApiBenchmark]`.
- Target: `tools/check-ethernet-live.mjs:61 [UTF8-LF sha256=33e72a4a0a29bbbaba321485b5c98bb1936e2c1e2f58fed1e66d22e2a83905c1; anchor=export async function checkEthernet(]`.
- Retained: Complete actual source read. Operator-configured HTTP authority validation, credential/fragment/whitespace/backslash rejection and bounded port/hostname checks adapted to the explicit live LED/API benchmark.
- Retained: Reconsulted full canonical source for the Ethernet live probe. Adapts operator-configured hostname authority checks to a bounded no-proxy/no-redirect HTTP probe with actual TCP peer pinning, board/boot fencing and exactly one optional ESP-only stage request; an unknown mutation result is never replayed.
- Omitted/replaced code: `Go public-artifact URL scope, literal-IP admission and public DNS/address transport pinning`. Disposition: **replaced**. Reason: This explicit operator-run benchmark must reach a configured LAN hostname and rejects literal IPs. It is not an artifact downloader or public-source SSRF policy. Board/boot fencing and strict current table/driver identity govern all mutations; existing firmware download policy remains separately tracked.
- Test-code evidence: `tests/led_api_benchmark_test.mjs:4 [UTF8-LF sha256=36b7450c99b94102dbca67e77b5b2ffde419b27fcaf80b456f80609003ce5383; anchor=runLedApiBenchmark]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Scoped advantage: Bounded concurrency, request deadlines, no mutation replay, strict-count/all-cell readback and explicit progress checks reject stale or frozen output.
- Remaining: Run against the actual hostname-resolved board in both rendering modes and compiled backends; loopback API results are not S3 latency, RF or optical proof.
- Remaining: Actual W5500 local serving and Ethernet-only pinned Internet staging remain open; synthetic/injected responses and loopback HTTP are not hardware or WAN proof.

### PCController: `Tools/Controller/internal/netpolicy/proxy_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `c9dcf35ffcdca6a34c98ab293b656e5928e498ef40403912834a9253c4ff2077`.

- Target: `mobile/test/lan_resolver_transport_test.dart:12 [UTF8-LF sha256=30279615272e766fefe9d14104e90596962b5924a61ae975da4778c53042703f; anchor=void main()]`.
- Retained: Adapted actual donor local suffix/private range and do-not-invent-proxy cases to Dart selector tests, plus real loopback HTTP cache/Host and TLS trust/hostname fixtures.
- Omitted/replaced code: `Go environment mutation/NO_PROXY deduplication and ParsePublicHTTPURL cross-policy tests`. Disposition: **replaced**. Reason: Scoped Dart selector does not mutate/deduplicate the process environment. Remote firmware public-source filtering is independently tracked and tested; passing these client fixtures is not that policy or a physical board deployment.
- Test-code evidence: `mobile/test/lan_resolver_transport_test.dart:12 [UTF8-LF sha256=30279615272e766fefe9d14104e90596962b5924a61ae975da4778c53042703f; anchor=void main()]`.
- Scoped advantage: Actual transport tests are isolated from the widget binding's intentional HTTP400 override; no certificate-verification bypass.
- Remaining: Retain on-device reconnect/discovery and physical output/audio acceptance; public localhost TLS fixture credentials must never become production assets.

### PCController: `Tools/Controller/internal/netpolicy/proxy.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `9bb5d0bd6d5df70398cac4c1a3b4bda75dd9b4fb6c17a59382fb3c0ed5982d53`.

- Target: `mobile/lib/lan_resolver.dart:201 [UTF8-LF sha256=d7d1589f6fca4c5385acd88f9b3dec98504c82e32558a35d17dbc89202eaed66; anchor=String controllerProxyFor]`.
- Retained: Complete canonical donor source and proxy_test.go read through EOF. Adapted exact loopback/private/shared/link-local address ranges and local hostname suffixes into per-client controllerProxyFor; LAN HTTP/WebSocket never accidentally traverses a dependency-download proxy, while remote endpoint proxy selection remains caller-owned.
- Omitted/replaced code: `Process-wide NO_PROXY/no_proxy environment merging, Go httpproxy/runtime types`. Disposition: **replaced**. Reason: Flutter uses a scoped HttpClient proxy callback and Dart's environment selector for remote origins, not global process environment mutation or a new fixed proxy URL. Go public-source rejection is a separate policy and is not claimed ported by this local bypass adapter.
- Test-code evidence: `mobile/test/lan_resolver_transport_test.dart:12 [UTF8-LF sha256=30279615272e766fefe9d14104e90596962b5924a61ae975da4778c53042703f; anchor=void main()]`.
- Scoped advantage: Fixes a demonstrated real HTTP timeout: ledboard.local was sent to this host's environment proxy without consulting its resolver. No global proxy setting is changed.
- Remaining: Physical Android/local-network and remote-proxy acceptance remain open. Cached route is transport metadata, never peer authority; preserve certificate/SNI and same-board readback.

### PCController: `Tools/Controller/internal/programmer/preflash.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `520de1d7431f3062d0603636b89098d1e41c7701a2f44f65b88d45cfe49a2c07`.

- Target: `firmware/esp8266/src/MaintenanceRoutes.h:284 [UTF8-LF sha256=9f486926615fdec3d6e7c6ad32e384816e66811674aec2a9f6b34697233e83cf; anchor=bool prepareRemotePair(]`.
- Target: `firmware/esp8266/src/UrbootRoutes.h:82 [UTF8-LF sha256=7d41ea5354537c6738c751fa01848cc09d558e2d9d0900bf5e975ad3adcdd6c5; anchor=bool runUrboot(]`.
- Retained: Read entire actual donor through EOF. Preserve complete backup before production writes, cancellation before mutation and post-backup candidate reinspection. The network bridge forwards the existing guarded ESP coordinator's raw bytes/status/receipts; it does not replace the programmer or turn staging acceptance into a completed flash.
- Retained: Adapt actual backup-before-write and candidate reinspection to the ESP-owned Urboot operation: full flash/EEPROM capture and durable hash validation before the first UART page write, then exact image digest reinspection, protected boot equality and complete flash/EEPROM readback. Existing durable Prog arm precedes this recovery capture, as in the existing integrated ISP policy; ordinary settings and boot bytes are not programmed by Urboot.
- Omitted/replaced code: `Host Urclock/USBasp executor, backup archive manifest and development skip policy`. Disposition: **pending**. Reason: Retain the existing ESP-owned integrated ISP coordinator and mandatory production backup rather than adding a competing serial/programming owner. Host archival restore/executor parity needs a separate full dependency and hardware review; it is not claimed by transparent HTTP forwarding.
- Test-code evidence: `desktop/src/bridge_transfer.rs:377 [UTF8-LF sha256=fbb13202d86296f1766d0e20bcd9e4a520a8b13d4988890e5a5273a20a5469fe; anchor=fn bridge_transfer_head_truncation_and_stale_mutation_preserve_truth]`.
- Test-code evidence: `tests/urboot_integration_test.mjs:16 [UTF8-LF sha256=3f4501099cd3f7a3d00e220dc731131d5e550bbf68e4f8f7934783b4fb5475cd; anchor=routes.indexOf('Image changed after backup')]`.
- Remaining: Issue #2: real S3 ISP/Urboot same-image jobs now verify full flash/EEPROM, fresh capture, application UART return and Prog release (docs/REMOTE-MAINTENANCE.md). Changed-image UART, dead-UART/power-loss recovery and broader electrical/physical acceptance remain open; old stored files alone are not fresh capture proof.

### PCController: `Tools/Controller/internal/programmer/restore_preflash_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `6dabd79c406b49dce87866fa792bc557220b98515606041cc33099193d567abe`.

- Target: `firmware/esp8266/src/MaintenanceRoutes.h:284 [UTF8-LF sha256=9f486926615fdec3d6e7c6ad32e384816e66811674aec2a9f6b34697233e83cf; anchor=bool prepareRemotePair(]`.
- Retained: Read complete actual donor tests. Retain failure/no-flash, unchanged candidate and EEPROM-reset raw-backup-before-arm distinctions in the existing coordinator review. New actual socket tests cover stale peer before dispatch, valid multipart/binary transfer, HEAD and truncation; these do not claim execution of the donor's Go programmer tests or physical backup acceptance.
- Omitted/replaced code: `Host Urclock/USBasp executor, backup archive manifest and development skip policy`. Disposition: **pending**. Reason: Retain the existing ESP-owned integrated ISP coordinator and mandatory production backup rather than adding a competing serial/programming owner. Host archival restore/executor parity needs a separate full dependency and hardware review; it is not claimed by transparent HTTP forwarding.
- Test-code evidence: `desktop/src/bridge_transfer.rs:377 [UTF8-LF sha256=fbb13202d86296f1766d0e20bcd9e4a520a8b13d4988890e5a5273a20a5469fe; anchor=fn bridge_transfer_head_truncation_and_stale_mutation_preserve_truth]`.
- Remaining: Issue #2: actual AVR reset/upload/program, new recovery capture, EEPROM/Urboot preservation and power-loss acceptance remain open. Stored backup downloads are not fresh physical capture or programming proof.

### PCController: `Tools/Controller/internal/programmer/urclock_retry.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `de0a9f15fbda4ee883766be025de372ae731741f5baf161f84e5a8dad837d202`.

- Target: `firmware/esp8266/src/UrbootProtocol.h:16 [UTF8-LF sha256=7b70536811b4b6b6e905c5e83e47926cf342bb2aca9e95c244eb76d83fb767f9; anchor=bool connect()]`.
- Target: `firmware/esp32s3/src/AvrUart.h:49 [UTF8-LF sha256=8118d17ace765adfa406820740f3ac60019658ec5bce9b85eae091baf0253858; anchor=void releaseRaw()]`.
- Retained: Fully read the actual retry policy. Preserve its strict distinction between bounded read-only synchronization and irreversible writes: only synchronization repeats; a short write, malformed page response or timeout retires the operation, never retries or falls back to ISP. The same S3 UART owner restores framed baud/parser state on all exits.
- Omitted/replaced code: `Windows COM sharing-error matching and delayed backup subprocess retries`. Disposition: **pending**. Reason: The embedded uploader owns UART1 directly, not an AVRDUDE COM handle. COM release diagnostics remain relevant to a future host Urclock/USB adapter, not this ESP hardware-UART transport. No write retry is imported.
- Test-code evidence: `tests/urboot_protocol_test.cpp:55 [UTF8-LF sha256=7e44fd173e586163f9807bb4d335c3de86c340b53d62d1c60eaaf5397aa081a3; anchor=p.badReply=true]`.
- Test-code evidence: `tests/urboot_protocol_test.cpp:52 [UTF8-LF sha256=7e44fd173e586163f9807bb4d335c3de86c340b53d62d1c60eaaf5397aa081a3; anchor=p.silent=true]`.
- Remaining: Issue #6: actual stock Urboot synchronization, full backup, same-image programming/readback, EEPROM preservation and application recovery passed on ledboard (docs/REMOTE-MAINTENANCE.md). Changed-image UART/interrupted-write recovery and native/mobile method selectors remain open; emulator success alone never certifies these gates.

### PCController: `Tools/Controller/internal/secretstore/backend_windows.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7a44ddcb48bc8823b1c3322e4eb100b082d720ab3cf83f5e95eb7b74ecd039fe`.

- Target: `desktop/src/client_secrets.rs:27 [UTF8-LF sha256=796f892a99cce1215097a19a7a0c05305f5befea686e2d8584e3b7c42ab366ef; anchor=pub fn get(]`.
- Target: `desktop/src/client_management.rs:278 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=pub fn newer(]`.
- Retained: Actual current-user Credential Manager Get/Set, explicit missing-vs-failure, bounded byte payload, CredFree and no plaintext fallback adapted to enrollment credentials.
- Omitted/replaced code: `Donor-wide installer/recovery/platform acceptance outside this self EXE slice`. Disposition: **pending**. Reason: Force-kill and automatic deletion of rollback are deliberately not copied: a live editor or unverified new process must remain recoverable. Media/libmpv, archive/GitHub extraction, interactive TUI console inheritance do not apply to this GUI raw same-origin executable. Generic signed installer/removal inventory, unattended uninstall and true two-new-version helper/Windows-session/OS acceptance remain pending. Preference UI uses actual existing Pakar donor widgets; no full donor GUI parity is claimed.
- Test-code evidence: `desktop/src/client_management.rs:865 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=release_policy_checks_numeric_build_semver_platform_origin_and_integrity]`.
- Test-code evidence: `desktop/src/client_install.rs:551 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=journals_bind_all_transaction_paths_and_parent_without_shell_commands]`.
- Remaining: Issue #7: live secure enrollment, release/telemetry/command receipt and safe replacement proof separated from source fixtures; Android foreground-only and self-uninstall local/OS-consent boundaries remain.

### PCController: `Tools/Controller/internal/tui/table.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `ecd399e2bf7718f3eb7b62e49f9dd884c0fb179e3d907bd15b238c01d31501e3`.

- Target: `desktop/src/terminal_ui.rs:471 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn window(total: usize, visible: usize, cursor: usize)]`.
- Retained: Actual tableWindow cursor-centred viewport arithmetic is adapted and exhaustively tested across total/visible/cursor cases. Reuses native logical selection/navigation and Unicode column clipping.
- Omitted/replaced code: `Donor gocui DataTable groups/control-tone rendering, visibleLine mouse mapping and richer table columns`. Disposition: **pending**. Reason: Only the viewport math is currently integrated; gocui APIs require crossterm adaptation, while generic groups/tones/mouse and richer settings/mapping editors remain pending. No full donor table parity is claimed.
- Test-code evidence: `desktop/src/terminal_ui.rs:1131 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn actual_donor_table_window_keeps_cursor_in_resized_viewport()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### PCController: `Tools/Controller/web/src/card-layout-controls.tsx`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `de67f2ac9dd8037e354b98227616ed8c207379dbc8205692587bc07a6392be83`.

- Target: `assets/web/device-settings.js:165 [UTF8-LF sha256=d71e9797948b19a436210332ce17b9dab783076145dd695c4798d0de078515b4; anchor=function panelToggle(]`.
- Target: `docs/INTERFACE-CONTRACT.md:50 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## Known useful parity gaps, not accepted exclusions]`.
- Retained: Actual CardLayoutEditor hidden-card show/reset and CardLayoutFrame hidden/collapsed actions adapted to the embedded workspace Panels dropdown and panel headers. Persisted preferences skip identical writes; lost capability overrides visibility.
- Omitted/replaced code: `editing/onToggleEditing and pointer-capture onReorderStart/Move/End/onLostPointerCapture; order/CSSProperties; keyboardReorderOffset dependency`. Disposition: **pending**. Reason: Show/hide/collapse/reset does not restore panel-order editing. Read actual pointer-reorder.ts completely; its stable-target and RTL keyboard helper need compatible per-container order, focus, persistence and stationary-pointer tests. Existing cell-order editing is a separate contract.
- Omitted/replaced code: `CardLayoutCopy localization and Lucide Check/ChevronDown/Eye/EyeOff/GripVertical/RotateCcw toolbar glyphs`. Disposition: **pending**. Reason: Standalone embedded controls preserve labels and actions but not the entire localized vector-icon toolbar. Bind the established controller icon catalog and localization rather than loading the React/Lucide runtime or claiming glyph parity.
- Omitted/replaced code: `React generic component props and ReactPointerEvent/CSSProperties wrappers`. Disposition: **replaced**. Reason: The ESP serves a standalone embedded document, not the donor React runtime. Existing live DOM cards retain their handlers when settings cards are reparented; no duplicate React state is introduced.
- Test-code evidence: `tests/interface_settings_test.mjs:123 [UTF8-LF sha256=ecb4542141e12df8ab5409f1154cbe5e39466e5e23c3c39301f36eb6c17a77f2; anchor=Capability loss must override stored panel visibility]`.
- Remaining: Restore edit-mode panel ordering, actual icon/localization contract and pointer/keyboard tests; source/HTTP doubles and simulated browser proof are not physical acceptance.

### PCController: `Tools/Controller/web/src/peripheral-availability.test.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a5d7366279057e0868478846c6bbe92eda4209a6bdbcbc16e1c149e57dfe035e`.

- Target: `tests/web_context_test.mjs:36 [UTF8-LF sha256=69e5d441087ed22663b727aab6c76eb26501545c3e9d49c291486e8426397c79; anchor=const manifest=]`.
- Target: `mobile/test/peer_context_test.dart:46 [UTF8-LF sha256=4f5aa508262f3aad124a15611cf4b4e05b3eb680f1beed9bc3fd1b3538ba0720; anchor=No peer ACK cannot be inferred]`.
- Target: `mobile/test/widget_test.dart:111 [UTF8-LF sha256=5956761df52915c53e5a4b4375a4d5837d3d6c0d0342aaa23e786675d23d87cc; anchor=Unacknowledged board has no Table]`.
- Target: `mobile/test/hardware_page_test.dart:40 [UTF8-LF sha256=ab9a188243999b3c114156d5558428c4a568cd33495f7bc7bed9933a134ba884; anchor=Hardware owns every acknowledged capability exactly once]`.
- Target: `tests/device_context_test.cpp:4 [UTF8-LF sha256=fc0347de3ae5b4967d79a6191f3310a6853f1dbb21aa4bbace3f307e36e7d09d; anchor=int main(]`.
- Target: `desktop/src/capabilities.rs:474 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=fn serial_manifest_is_individual_and_table_modes_are_profile_scoped]`.
- Retained: Actual disconnected, missing status and capability-free cases adapted into shipped-Web tests. Independent peripheral ACKs, expiry, cross-kind and malformed geometry/schema cases added. C++ tests execute actual HELLO/display-ACK policy.
- Retained: Equivalent Flutter fixture cases execute actual capability parsing, unsupported schema/geometry, display measured ACK, per-feature cards, identity change and phone/tablet navigation; API fixtures reject pending reads/queued writes after owner or credential changes.
- Retained: Flutter hardware-page regressions move capabilities from Overview into Hardware, retain dimmed board-scoped badges without granting fresh ACK authority, expose capability-only profiles without requiring network telemetry, and cover phone/tablet text scaling. Existing selected-peer parsing and PanelInventory remain the sole data owners.
- Omitted/replaced code: `Vitest/liveSnapshot/emptySnapshot fixture ownership`. Disposition: **replaced**. Reason: Node VM executes embedded Web code, C++ executes firmware policy, and Rust has USB adapter tests. React/Vitest would target a nonexistent React owner in this embedded surface.
- Omitted/replaced code: `Valid/invalid INA219, temperature, PWM, LCD, Bluetooth, relay/RF/statusLED fixture cases`. Disposition: **pending**. Reason: Restore exact bounded-invalid-value cases with the genuine PCController adapter; those peripherals are currently absent, not replaced by synthetic zero fixtures.
- Test-code evidence: `tests/web_context_test.mjs:87 [UTF8-LF sha256=69e5d441087ed22663b727aab6c76eb26501545c3e9d49c291486e8426397c79; anchor=Unsupported geometry is not forced]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=fc0347de3ae5b4967d79a6191f3310a6853f1dbb21aa4bbace3f307e36e7d09d; anchor=int main(]`.
- Remaining: Rust tests remain cache-budget blocked; generic adapter sensor/profile tests and physical/native acceptance remain #10.

### PCController: `Tools/Controller/web/src/peripheral-availability.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `fcf2e397564c843711fd64db2bd2d820552180ac4f5abdf0d44ccbbb86be7a84`.

- Target: `desktop/src/capabilities.rs:164 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=pub fn has(]`.
- Target: `mobile/lib/peer_context.dart:38 [UTF8-LF sha256=5f520bd21d49ac1460fd3cbd24f4713ccddce0168d78046774a19a181c68962d; anchor=static PeerContext fromStatus]`.
- Target: `mobile/lib/cell_pixel_preview.dart:12 [UTF8-LF sha256=5cc0f113cc45f19ad523fb1ea99f07ad4f4097ae7412dbbebe6a0fffd9e063f4; anchor=CellPixelPreview? cellPixelPreview]`.
- Target: `mobile/lib/main.dart:723 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=Future<bool> _command]`.
- Target: `libraries/BoardCore/src/DeviceContext.h:30 [UTF8-LF sha256=3e940f4ee3d09c482486b7321648f776ae6c480e92aaa564beb5dca12d9bfbfa; anchor=inline uint16_t features(]`.
- Target: `firmware/esp8266/src/WebUi.h:803 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function peerAvailability(]`.
- Retained: Actual connected AND have_status AND advertised-capability conjunction adapted to selected-peer fresh manifests. No feature inference from hostname, pixel count, catalog, defaults or online alone.
- Retained: TM1637 additionally requires fresh measured ACK; Text and Buzzer have independent HELLO support. Native/Web hide unrelated controls and gate domain commands.
- Retained: Reread peripheral-availability.ts completely for Flutter recovery: the existing freshness deadline still owns commands; resuming a suspended socket preserves only a still-fresh observation and stale resume starts its recovery read immediately. Six fixed pixel indicator slots use the published frame map or explicitly labelled board diagnostic state; rainbow without sampled RGB never reuses old semantic white. This adapts availability/unknown semantics, not donor hardware-specific sensor layouts.
- Retained: Flutter now adapts the same ready AND advertised schema-1 conjunction, table geometry and individual ACK policy; selected owner revisions reject old replies and retained dialogs. Unknown/stale peers have only client connection/overview navigation.
- Retained: Reread the actual availability implementation/tests and Pealayer board information UI. Add optional board-owned LED backend/RMT/DMA capability data to native, Flutter and Web without deriving it from chip model, output enabled or renderer mode. Missing/mistyped/contradictory metadata remains unknown; native serialization and USB compact readback retain the full manifest. Existing driver observation separately owns readiness/activity/faults.
- Omitted/replaced code: `Snapshot/status/hello fields and compact status flag fallback`. Disposition: **replaced**. Reason: Pakar JSON and append-only USB share a schema-1 normalized manifest. AVR HELLO and display-status ACK are aggregated on ESP; copying PCController bit values would alias different protocols.
- Omitted/replaced code: `bounded, ina219, pwm, temperatureLED, temperatureBTAudio, lcd, bluetoothAudio, relays, rf, statusLED and invalid measurement fields`. Disposition: **pending**. Reason: Those drivers are absent on the current pair. A real PCController adapter must preserve their detection, bounds and invalid-value distinctions later. No fabricated controls are displayed.
- Test-code evidence: `tests/web_context_test.mjs:60 [UTF8-LF sha256=69e5d441087ed22663b727aab6c76eb26501545c3e9d49c291486e8426397c79; anchor=No inferred board kind or controls]`.
- Test-code evidence: `mobile/test/cell_pixel_preview_test.dart:29 [UTF8-LF sha256=6a77394bb38b4411e526ee1a9c0468158e2a7664d574a9bd6fea29ad20c0f5be; anchor=Board diagnostic overrides stale white semantics using physical mapping]`.
- Test-code evidence: `mobile/test/peer_context_test.dart:62 [UTF8-LF sha256=4f5aa508262f3aad124a15611cf4b4e05b3eb680f1beed9bc3fd1b3538ba0720; anchor=Independent ACKs and measured display gate]`.
- Test-code evidence: `mobile/test/widget_test.dart:285 [UTF8-LF sha256=5956761df52915c53e5a4b4375a4d5837d3d6c0d0342aaa23e786675d23d87cc; anchor=Peer ACK loss removes table and independent cards]`.
- Test-code evidence: `mobile/test/api_test.dart:140 [UTF8-LF sha256=7a8f6830d333e3458b3fbdbf7ff680549f58dc2955366d2aea1b1640d0052201; anchor=Changed endpoint discards an in-flight status]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=fc0347de3ae5b4967d79a6191f3310a6853f1dbb21aa4bbace3f307e36e7d09d; anchor=int main(]`.
- Remaining: Native execution and generic PCController profile/discovery adapter remain #10/#6 work; source/C++/Web proof is not native or physical acceptance.

### PCController: `Tools/Controller/web/src/quick-header-preferences.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7a3e955ddc771cf27f9e545c11ea992a1342a7ed5eaeba9d9a5b95d00dacd2bc`.

- Target: `firmware/esp8266/src/WebUi.h:767 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function workspaceSettingsRecord]`.
- Retained: Completely read actual allowlisted normalized local-preferences and unavailable-storage behavior; adapt bounds/type validation and storage-disabled retention to passive board/application Web workspaces.
- Omitted/replaced code: `Full donor quick-header action/catalog UI`. Disposition: **pending**. Reason: This reuses normalization/persistence, not every quick-header control; individual capability-gated donor controls need separate mapping.
- Test-code evidence: `tests/web_workspace_test.mjs:115 [UTF8-LF sha256=0c36a09e6b43cbb54286356f82778393b44d70fb7e608ce2eaa8633125ef7145; anchor=Disabled browser storage still retains this session workspace]`.
- Remaining: Actual browser storage/reload acceptance remains open.

### PCController: `Tools/Controller/web/src/status-frame-batcher.test.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `61d9972be27e3ab35db5012600e8a7b5c8507815edeb55b80764357d4658d222`.

- Target: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Retained: Full source read through EOF. Actual fake-clock callback Map/cancelled-list and deliberately delivered cancelled callback scenarios ported to the actual Web streaming source.
- Retained: Generation replacement and idempotent disposal scenarios strengthened to assert stale callback cannot disarm the newer handle; new pacing, buffering, peer loss, partial-send and final-page/back-cache cases exercise real source.
- Omitted/replaced code: `100 status updates, 72-sample history, ordered batch/error-then-success and metric snapshot assertions`. Disposition: **pending**. Reason: These test a still-useful telemetry reducer not implemented in the latest-only LED scheduler. No whole-suite parity is claimed; add them alongside generic status/history imports.
- Test-code evidence: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Test-code evidence: `tests/web_access_test.mjs:90 [UTF8-LF sha256=83c06839b0cfb57d8660ad9e66600b7ad8b9a799bf70fdf42755997e3f78cd02; anchor=Reboot revokes the actual running browser stream]`.
- Scoped advantage: Real two-chunk packet and embedded module byte equality plus partial-send no-replay coverage; not hardware acceptance.
- Remaining: Adapt donor status/history/error scenarios when those contracts are implemented; actual board throughput/reboot/buffering stress remains a separate physical gate.

### PCController: `Tools/Controller/web/src/status-frame-batcher.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f600267bf09d925eaf9e5cc3d752574a0a3fbc46350782c614440300c08799e4`.

- Target: `assets/web/frame-stream.js:9 [UTF8-LF sha256=3d0ab90616b173570e5618be76aded339754caa92961868a1d9b43a02e3c6d90; anchor=function createFrameStream]`.
- Target: `firmware/esp8266/src/WebUi.h:1470 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function readStreamPolicy]`.
- Retained: Full source read through EOF. Actual injected clock, generation admission, one scheduled callback, cancellation and disposal structure adapted to the mechanically embedded latest-frame Web stream.
- Retained: Stale callbacks check generation before clearing the timer handle, strengthening the donor against disarming a replacement owner. Observed board/boot and exact socket ownership replace build/name-only identity for this bounded slice.
- Omitted/replaced code: `pendingStatus history queue and applyStatusFrameBatch ordered metrics/error reducer`. Disposition: **pending**. Reason: All status arrivals are useful for telemetry/history, but stale LED actuations must not be replayed. This latest-only send scheduler does not implement the donor telemetry reducer; retain it as separate useful work.
- Omitted/replaced code: `sampleFrom, metricSamplesAfterSnapshot, controllerSnapshotIdentity and peripheralAvailability imports`. Disposition: **pending**. Reason: Metric history and authoritative snapshot resets remain useful and need typed peer contracts. I2C supply/current sensor consumers must stay gated on boards without those sensors; stronger boot fencing alone is not complete metric parity.
- Omitted/replaced code: `TypeScript/React clock module ownership`. Disposition: **replaced**. Reason: Standalone embedded Web consumes one generated JavaScript source and timer clock; requestAnimationFrame would add a display refresh cap to LED streaming. No second React toolchain is necessary for this firmware page.
- Test-code evidence: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Test-code evidence: `tests/web_access_test.mjs:90 [UTF8-LF sha256=83c06839b0cfb57d8660ad9e66600b7ad8b9a799bf70fdf42755997e3f78cd02; anchor=Reboot revokes the actual running browser stream]`.
- Scoped advantage: Explicit 60/default, 0/uncapped policy, bounded complete-frame buffering, no catch-up bursts, partial-send uncertainty and generation-before-handle checks; browser evidence only.
- Remaining: Port generic telemetry history/status reducer and sensor availability contracts with their own authoritative peer state; validate physical throughput, queued WebSocket epoch fencing, multi-client arbitration and all-client streaming parity.

### PCController: `Tools/Dependencies/network.mjs`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `975d766f3a9cfb08eb3f292e0a00ae441be80af176401d3d84b931a939534ced`.

- Target: `tools/benchmark-led-api.mjs:14 [UTF8-LF sha256=67c184a28a6450dd81f41576ab7f1582092e3c151d4d0af6e9d8415d5f4c3d7f; anchor=PAKAR_BENCH_USERNAME]`.
- Retained: Complete actual dependency-network helper read. Retained secret-safe diagnostics and caller-owned environment/proxy configuration principles in the opt-in benchmark, with credentials excluded from command arguments/report.
- Omitted/replaced code: `withDirectFallback retry after failure and subprocess environment stripping`. Disposition: **replaced**. Reason: A benchmark cell/mode mutation may already have applied when its response is lost. Retrying on a direct route would duplicate actuation; the benchmark stops on uncertainty without replay or automatic restoration. It invokes no dependency subprocess and changes no global proxy environment.
- Test-code evidence: `tests/led_api_benchmark_test.mjs:4 [UTF8-LF sha256=36b7450c99b94102dbca67e77b5b2ffde419b27fcaf80b456f80609003ce5383; anchor=runLedApiBenchmark]`.
- Scoped advantage: Unsafe reuse of dependency retry semantics for hardware mutations is explicitly rejected with no-replay regression coverage.
- Remaining: Dependency-download proxy fallback itself remains a separate tooling requirement; this actuation benchmark does not claim to implement it.

### PCController: `Tools/VirtualBoard/tests/firmware_controls_test.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4facf265f8593b83f7610ae99ce58050013226cf1aa5a4d6e60bdbd6e5cc399e`.

- Target: `tests/tm1637_bus_test.cpp:15 [UTF8-LF sha256=0bd8312952f6b58447d12126c698e835290add115b91a753a6b6d6efcbc7d031; anchor=void testDisplayBrightnessFade()]`.
- Target: `tests/display_stubs/Arduino.h:17 [UTF8-LF sha256=b6c81c35395db97127f88f449e51f3cef884592bafe85406f381a73a37f20c5b; anchor=namespace display_mock]`.
- Target: `tests/display_stubs/avr/io.h:5 [UTF8-LF sha256=a5dc33f85c2dde21843165ae0e64bd64bec0ed44c3fdc002c805e29b5e310e63; anchor=struct DisplayRegister]`.
- Target: `firmware/avr/SevenSegments.cpp:77 [UTF8-LF sha256=73b97393db8937820b153c4b776014e2dda672c579a179f133777b1fe5107997; anchor=pinMode(BoardPins::Tm1637Data, INPUT_PULLUP)]`.
- Retained: Actual require helper, testDisplayBrightnessFade and testDisplayAckDetectionAndRecovery adapted to the separate Pakar keepalive service and A4/A5 hardware ownership; decode actual data/address/control bytes rather than inferring frame recovery from cached values.
- Retained: Canonical firmware_controls_test.cpp639lines fully reread through EOF; rechecked unchanged Pakar require/fade/ACK-recovery subset and A4/A5 fixtures. No all-controls parity or physical acceptance is inferred.
- Omitted/replaced code: `Side A/B directional relay and motion-door policy matrix, temperature role swapping, retired MOVE alias and DS18B20 absent-pullup tests`. Disposition: **not-applicable**. Reason: This fitted board has no directional motors/door policy or DS18B20. Do not enable these or copy donor relay pin mapping; future generic adapters remain feature-flagged with their own acceptance.
- Omitted/replaced code: `Full key double-click/fast-repeat/16-bit rollover, transition smoothing, context-sensitive leaf navigation/settings normalization and tone timer/queue cases`. Disposition: **pending**. Reason: Pakar has existing key/tone/front-panel suites, but these complete donor assertions have not all been transplanted or compared behavior-by-behavior. A display-only import is not proof of those generic contracts.
- Omitted/replaced code: `testDisplayNumericBoundaries/testEveryEasedBytePair; calibrated expandByte12 and EMA/deadband assertions`. Disposition: **pending**. Reason: Pakar display-bus fixtures do not contain new donor numeric overflow-to-dashes or exhaustive65536 transition-pair assertions. Generic display/transition tests remain pending. Temperature-ROM EEPROM threshold/CRC tests remain inapplicable while no DS18B20 is fitted; do not transplant its EEPROM layout.
- Test-code evidence: `tools/test-display.ps1:7 [UTF8-LF sha256=e3c3a230cb93757edfaac1e2ea208695a9cfdfb6b4cd613c657c632b8fb25e51; anchor=foreach ($taskVariant]`.
- Test-code evidence: `tools/test-protocol.sh:205 [UTF8-LF sha256=f28c11e234eda8b71ec4dfea0362eb4d02651fabf4323738203c4451e9afed41; anchor=for display_flags]`.
- Scoped advantage: Exercise both actual AVR register and Arduino fallback branches; assert ninth-slot input/pull-up, preserved other port bits, exact replay, each NACK byte, display-off restoration and 32-bit rollover. Physical/stuck-low line diagnosis remains unverified.
- Remaining: Import/compare the remaining generic donor key/transition/menu/tone assertions against actual hardware-matched consumers. Physically validate display disconnect/reconnect and supply/timing; register/GPIO doubles are not electrical proof.

### PCController: `Tools/VirtualBoard/tests/uart_protocol_test.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `56c0474a5f0a7475340ec22e718b262b61a449820eb958403cd47bc36b1497c7`.

- Target: `tests/serial_link_test.cpp:1 [UTF8-LF sha256=c103ffb2c7530bb4c85373433b123fe072bb6649f2d97b82f70fa08d404eeae8; anchor=Adapt actual PCController Tools/VirtualBoard/tests/uart_protocol_test.cpp]`.
- Retained: Fully read all seven actual donor scenarios. Adapted representative/consecutive-zero/max48 payload, optional opcode dispatch, nested ACK stability, split RX with synchronous TX, oversized noise/yield/delimiter recovery and malformed/CRC rejection to actual shared SerialLink.
- Retained: Added2048-byte ring/64-byte pass, negative occupancy/read, failed-write/no-automatic-replay, explicit S3 Stream setup preservation, ESP32 counters and all three flow-profile readiness assertions.
- Omitted/replaced code: `testBuzzerPushCarriesMCUTimestamp and separate framing/CRC counter assertions`. Disposition: **pending**. Reason: Timestamped raw donor events and separate counter layout are not yet adapted to Pakar clients/contracts. Port those exact assertions with their production projection instead of deleting useful behavior or declaring parity.
- Omitted/replaced code: `testUnknownReservedEnvelopeByteDoesNotBlockSemanticFrames`. Disposition: **replaced**. Reason: Pakar fixed reserved-zero integrity rejection is intentionally retained and covered in existing protocol_test.cpp. Optional opcode semantic-dispatch test is adapted independently; no product generation or alias introduced.
- Omitted/replaced code: `framePayloadScratch macro mutation test fixture`. Disposition: **replaced**. Reason: SerialLink exposes no mutable TX scratch API. The test exercises split RX plus an actual synchronous ACK between halves and nested response stability, preserving the ownership behavior without fabricating a missing macro scratch interface.
- Test-code evidence: `tests/serial_link_test.cpp:52 [UTF8-LF sha256=c103ffb2c7530bb4c85373433b123fe072bb6649f2d97b82f70fa08d404eeae8; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: One stable tiny test executable reused for all modes; no donor build or debug cache.
- Remaining: Port remaining useful counter/timestamp tests when their actual production path is implemented.
- Remaining: Loopback C++ tests do not prove S3 driver compile, physical link, overflow stress or zero loss.

### Pealayer: `.github/dependabot.yml`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fa4791635251ad7d5be26ce70d71a8e59aa7b814a4f09f2b5b749d8a49908225`.

- Target: `.github/dependabot.yml:1 [UTF8-LF sha256=136887ba77b56544d0bcd367a31d2daaa7637ecdb8b997d63df6cb552d6f08e8; anchor=version: 2]`.
- Retained: Actual donor weekly grouped github-actions and Cargo updates, with Cargo path adapted to the existing desktop workspace.
- Omitted/replaced code: `npm updates at /web_ui`. Disposition: **not-applicable**. Reason: Pakar has no /web_ui/package.json or Vite runtime. Embedded Web/tests use Node built-ins; Flutter's actual /mobile Pub dependency contract is covered instead.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=c28cb1484b89e72a30d6d1971dcf2c1be42d29ccf1d57108ce72ad3b3ed4db33; anchor=console.log('Repository governance]`.
- Scoped advantage: Adds grouped Pub updates for the actual Android client and label routing while retaining weekly grouped Actions/Cargo updates.

### Pealayer: `.github/workflows/ci-cd.yml`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `bba9a295ee7b59e318f76e0b6f6a80f0aec631e249f7d8c798f6c92c0bb7fd8c`.

- Target: `.github/workflows/build.yml:2 [UTF8-LF sha256=b352090f2790ff875580ed2b985667accb7a6dcc1e9a675c8c551f430438e2f1; anchor=name: "🚀 Build · Test · Release"]`.
- Target: `tools/build-desktop.ps1:47 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=--test-threads=1]`.
- Target: `tools/ci-checks.mjs:250 [UTF8-LF sha256=f34ddef9726512638c8c5d2fcd3ec1402ba695831a1d0bfe8f3c744c5a92784b; anchor=export function runChecks]`.
- Target: `tools/prepare-release.mjs:16 [UTF8-LF sha256=c5dcb698c397b30ca0ed076437a695838366aa0a82ed3c580df3cf6380d03306; anchor=export function firmwareManifest]`.
- Retained: Actual donor branch/PR/tag/dispatch/merge-group triggers, concurrency policy, pinned actions without checkout credentials, health-before-build jobs, timeouts, Rust cache reuse, locked tests/builds, packaging, curated notes, manifest/checksums/provenance and aggregate results adapted to current Windows/ESP/AVR/Android targets.
- Retained: Release content is gated on all builds, created as an immutable draft prerelease, with two correctly matched board pairs and raw HTTPS update manifests. Untrusted PRs do not execute publishing code with a write token.
- Retained: Actual donor serial native test execution, isolated test configuration and RUST_BACKTRACE=1 restored. Runtime harness flags are separated from compile jobs; immediate panic output remains available even if a later test aborts before its summary.
- Omitted/replaced code: `LIBMPV_WINDOWS_RELEASES_API_URL, MPV_DLL_PATH, Windows GNU libmpv linking, Linux/macOS mpv dependencies and standalone pealayer-downloader`. Disposition: **not-applicable**. Reason: Pakar deliberately excludes the media runtime and standalone media utility. Copying this setup would introduce forbidden mpv and unrelated package dependencies.
- Omitted/replaced code: `macOS Intel/Apple silicon and Linux native platform matrix/packages`. Disposition: **pending**. Reason: Current accepted native target is Windows; firmware and Android use Linux runners. Full donor reread includes the new scripts/run-ci-rust-tests.sh macOS sampling helper and 15-minute native test step. Linux/macOS native matrix and diagnostic sampling remain useful pending work; they require actual Pakar platform coverage, not donor's unrelated acceptance.
- Omitted/replaced code: `UPX checksum-locked packing and packed native package smoke/resource checks`. Disposition: **pending**. Reason: Existing Windows wrappers preserve donor resource/donor/cache gates and CLI runtime tests, but Pakar has no accepted compressed native package probe. Do not call a source or CLI test full packed GUI runtime acceptance.
- Omitted/replaced code: `branch-tag and archive suffix creation, softprops update-existing release`. Disposition: **replaced**. Reason: User requests stable names without random/time suffixes. Version releases keep stable package names; release enumeration must succeed and an existing release is not overwritten.
- Test-code evidence: `tests/build_storage_test.ps1:17 [UTF8-LF sha256=683f1926231ec42b3dacbc0ba7e2b7cb03315a375bde499bed31b2e9c1467c7a; anchor=Actual donor serial test contract]`.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=c28cb1484b89e72a30d6d1971dcf2c1be42d29ccf1d57108ce72ad3b3ed4db33; anchor=console.log('Repository governance]`.
- Remaining: The historical 8849ab8 menu failure was subsequently fixed; 5d02b07 Build 38141677102 and CodeQL 38141677110 pass. This is not production/deployment proof. Validate the unexecuted draft publisher, useful additional native targets, donor macOS diagnostics and optional compressed package/runtime verification.

### Pealayer: `.github/workflows/codeql.yml`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fc3c298cf966d77f50b575614b4c26f24f1606459f07749a46880cd482f1cb53`.

- Target: `.github/workflows/codeql.yml:2 [UTF8-LF sha256=71540360da38622faaa237eec4f1276d5b3f67853754d8a221149e8c0f2726ab; anchor=name: "🛡️ Security · CodeQL"]`.
- Retained: Actual donor Actions and JavaScript matrix, no-build security-extended queries, pinned checkout/CodeQL actions, no persisted checkout credentials, PR/main/schedule/dispatch/merge-group triggers, cancellation, timeouts and fail-closed aggregate summary.
- Omitted/replaced code: `Pealayer React/Vite names, packages:read permission and branded prose`. Disposition: **replaced**. Reason: Pakar analyzes embedded Web/scripts and Actions, not Pealayer React. Package read permission is unnecessary for these none-build scans; Rust/C++ coverage is explicitly not invented.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=c28cb1484b89e72a30d6d1971dcf2c1be42d29ccf1d57108ce72ad3b3ed4db33; anchor=console.log('Repository governance]`.

### Pealayer: `assets/themes/palettes.json`

Verdict: **equivalent-file**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501`.

- Target: `assets/themes/palettes.json:2 [UTF8-LF sha256=11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501; anchor="studio"]`.
- Retained: Byte-identical Studio/Native light/dark JSON catalog, all fourteen semantic roles.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/ui/palette.rs:71 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn imported_palettes_have_every_required_role()]`.
- Test-code evidence: `tests/web_appearance_test.mjs:57 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=embedded catalog exactly matches]`.
- Remaining: Embedded Web now consumes all four actual palettes and fourteen roles. Mobile catalog use and native/Web shared preference synchronization remain pending; byte-identical asset retention is not all-surface parity.

### Pealayer: `build.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `68fb1bdd9e7c7523ab303e37001d3a5252dcb03abe85e489432fe913be7b2e0f`.

- Target: `desktop/build.rs:16 [UTF8-LF sha256=61bd58fc81c6b05dda0474cc4419a119dad4ee4c948e90383b97ff2b924ac780; anchor=brand_image::native_icon_bytes(brand_image::icon_bytes())]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Application PE/window/tray master derivatives are separated from actual semantic quick-action resource generation; master/helper changes trigger rebuild.
- Omitted/replaced code: `emit_build_metadata; APPLICATION_BRAND JSON/APP_* precedence/filename validation; version/InternalName/OriginalFilename/copyright/provenance fields`. Disposition: **pending**. Reason: Fixed current identity exists; validated external branding and full generic metadata/provenance remain useful imports.
- Omitted/replaced code: `macOS brew/mpv probing and rpath flags`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free; media linker dependencies violate that scope.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `docs/releases/next.md`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `71f81fa503d0494a85dd700e16cabdb23e1a6dcd6abbb0abc1af47000584b3f7`.

- Target: `docs/releases/next.md:1 [UTF8-LF sha256=cecaadeb61a7daaec709130503f49a0bb98cde4408af69bc6abe5837b7756fcd; anchor=# PakarController development packages]`.
- Retained: Actual donor curated preview notes explicitly distinguish a development milestone from stable-release, physical and platform acceptance.
- Omitted/replaced code: `Pealayer media/audio/timeline milestone inventory and archived media screenshot evolution links`. Disposition: **replaced**. Reason: These are donor-specific release facts, not transferable Pakar features. Record actual controller/interface packages and link Pakar issue #2 instead.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=c28cb1484b89e72a30d6d1971dcf2c1be42d29ccf1d57108ce72ad3b3ed4db33; anchor=console.log('Repository governance]`.

### Pealayer: `docs/REVIEW-COMMENT-AUDIT.md`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `7a91d9fa8778383ee37e33d861ceebc5ab210e46bedab6af8be82d56c27523cc`.

- Target: `tools/audit-github-conversations.mjs:59 [UTF8-LF sha256=d2b9dff0e72c8fa58a5982cde925e550bd159c22d207ac267f5991c68d57656a; anchor=export function collect]`.
- Target: `docs/GITHUB-COORDINATION.md:1 [UTF8-LF sha256=cbfea73af3d4c87e0d3bb7829940bc36c9bbc004797aae66e5314a5286fafd4b; anchor=# Conversation-aware integration]`.
- Retained: Actual exhaustive review-thread/review-summary/standalone-comment audit procedure adapted into read-only paginated GitHub collection, hash-bound manual scope decisions, unresolved/outdated preservation and quota-versus-completed-review distinction. Used on every state of Pakar, Pealayer and PCController.
- Omitted/replaced code: `Historical media thumbnails, MPV surface and DLL basename, timeline/cue migration, audio delay and media picking fixes`. Disposition: **not-applicable**. Reason: Pakar is an mpv-free controller and does not own donor playback/media/timeline models. Generic staging safety, keyboard ownership, error correlation, authentication and service-worker origin lessons remain applicable and tracked independently.
- Omitted/replaced code: `Complete semantic disposition of every historical issue/review and full DNS-rebinding, actuator/persistence, UI/PWA/physical acceptance`. Disposition: **pending**. Reason: All pages are collected but fetching does not mean reading or implementing them. Only explicitly hash-bound reviewed scopes are marked; every unread record and source-file import remains pending.
- Test-code evidence: `tests/github_conversation_audit_test.mjs:22 [UTF8-LF sha256=d71ee1e531770b756ab7f5269f9b6cb6d1bf75f7d17cd6c5e71e1529712ae7bd; anchor=Paginated GitHub audit]`.
- Remaining: Read and reconcile pending records in the refreshed 794-record conversation corpus; seventeen full-record and two comment scopes now have explicit semantic decisions. Continue applicable source imports and exact source/test mapping. Verify RayanLamp repository when supplied; do not invent one or mutate donor comments from Pakar.

### Pealayer: `docs/superpowers/plans/2026-09-28-windows-taskbar-and-tray-suite.md`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `033a82778e4ed8258bbbf99600d16039f523043386e302032d6df8849624ea9f`.

- Target: `desktop/src/platform/windows_tray.rs:17 [UTF8-LF sha256=5f847993b6177d97f3b5fa6930f6b1acfd6e6e26dfb4fb51c58730f0d8a4ed2d; anchor=struct TooltipSync]`.
- Target: `desktop/src/platform/windows_notifications.rs:103 [UTF8-LF sha256=0b39032fd9b88d51514b9cb39f0def7377016a6f073247404c1b1ccb67dcf550; anchor=fn tooltip_data(]`.
- Target: `desktop/src/main.rs:2955 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=event_log.failure("Windows tray tooltip", &error)]`.
- Retained: Full donor plan read through EOF as source requirements, not trusted commands. Generic existing tray lifecycle/menu/disposal retained; actual windows.rs NIM_MODIFY/TIP sections read and adapted to the owned GUID rather than duplicate registration. Tooltip Shell acceptance is separated from object presence; failed writes retain pending latest state with bounded retry/incident recovery and observed restart epoch invalidation.
- Omitted/replaced code: `Playback percent/error/paused inputs, media thumbnail toolbar Previous/Play/Next/Mute and video-only iconic clipping`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free and owns controller operations, not playback/chapter/video. No synthetic media percentages or nonexistent actions are imported; actual pending-operation activity remains.
- Omitted/replaced code: `Full root taskbar initialization/retry, thumbnail toolbar lifecycle, visible tray/menu/notification clicks and all Windows shell suite acceptance`. Disposition: **pending**. Reason: This fixes only GUID-bound tooltip synchronization on an already owned tray object. Mock clocks, NOTIFYICONDATA assertions and observed restart messages cannot certify complete root shell readiness or user-visible action delivery.
- Omitted/replaced code: `Direct donor NIM_ADD/uID1 and popup window ownership`. Disposition: **replaced**. Reason: The pinned tray library already owns HWND, registered GUID, menus, icon updates, restart recreation and disposal. A second NIM_ADD/global icon could conflict with its identity. Adapt direct NIM_MODIFY to the actual registered GUID; no guessed numeric internal ID or library source patch.
- Test-code evidence: `desktop/src/platform/windows_tray.rs:236 [UTF8-LF sha256=5f847993b6177d97f3b5fa6930f6b1acfd6e6e26dfb4fb51c58730f0d8a4ed2d; anchor=fn failed_tooltip_retries_latest_value_without_false_acceptance_or_flood]`.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:147 [UTF8-LF sha256=0b39032fd9b88d51514b9cb39f0def7377016a6f073247404c1b1ccb67dcf550; anchor=fn tooltip_uses_the_registered_guid_and_bounded_utf16]`.
- Test-code evidence: `tests/windows_tray_test.mjs:37 [UTF8-LF sha256=541da356a3c3e376e2eacfa10dbaf068925074ca5c77a005ccf71121eb5639cb; anchor=Actual GUID-bound tooltip]`.
- Remaining: Actual installed tooltip Shell acceptance and physical Explorer restart/visibility; full root shell and action acceptance. Complete windows.rs file stays independently pending; only2364..2421/2483..2513 sections read for this adaptation.

### Pealayer: `docs/verification/LOCAL-WORKSPACE-HYGIENE.md`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `c024125307f6e466c924e1b8874f869a575b085b2a05bb4493bfd1d1e9f38975`.

- Target: `docs/REUSE-PREFLIGHT.md:42 [UTF8-LF sha256=13521024e5b18f314abcc71af4e9c722262a7cb04c17f03155b1e88d0cf0972e; anchor=Mandatory sequence on every task]`.
- Retained: Actual canonical-cache/stable-staging/preservation/blocked-operation and separate runtime-acceptance guidance read and adopted in mandatory continuation policy and tooling.
- Retained: Fully reread the current donor through EOF: shared-cache rejection/hash-copy/storage inventory, routine symbol/incremental policy, debug-only guarded cleanup, measured scratch-RAM sizing and skipped-test versus bundle-integrity distinction. Actual read-only directory summary/equality helper and mandatory low-write/preservation guidance remain; donor historical cleanup/deployment results are not Pakar facts.
- Omitted/replaced code: `Historical Pealayer paths, exact relocation/cleanup/deployment/media-clock events`. Disposition: **not-applicable**. Reason: These are donor historical facts, not Pakar installations. Copying them would misrepresent ownership or completed cleanup.
- Omitted/replaced code: `Same-volume rename/hardlink-aware relocation and reusable host profile/peer updater workflow`. Disposition: **pending**. Reason: These are useful generic mechanisms but no Pakar cache relocation or complete updater deployment is implemented by this policy. Current blocked cleanup stays blocked; no rerouting through another shell.
- Omitted/replaced code: `Mandatory donor lookup was previously absent from executable build/stage/capture paths`. Disposition: **replaced**. Reason: Capability lookup with source/target/test hash gates now runs before those actions; other task surfaces are required by AGENTS.md, not falsely claimed universally sandbox-enforced.
- Omitted/replaced code: `New source-checkout cache prohibition and explicit guarded target prune`. Disposition: **pending**. Reason: Canonical cache relocation requires an owner-preserving migration without duplication. Existing cleanup was blocked and cannot be rerouted through this helper. No reclaim or reduced physical SSD wear is claimed.
- Remaining: Complete applicable generic host/relocation/updater tooling after ownership checks; extend executable gates when additional capability mappings are fully reviewed.

### Pealayer: `docs/WINDOWS-SHELL-RECOVERY.md`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `57fbd904e18d8f0036dd67e2c866d357ab282075689a1cbf6b38a64270875492`.

- Target: `desktop/src/platform/windows_tray.rs:14 [UTF8-LF sha256=5f847993b6177d97f3b5fa6930f6b1acfd6e6e26dfb4fb51c58730f0d8a4ed2d; anchor=Shell acceptance, not object presence]`.
- Target: `desktop/src/ui/shell.rs:733 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=value["tray_shell"]]`.
- Target: `tools/replace-desktop.ps1:42 [UTF8-LF sha256=13e5893983bf422449d0c35b94305a39bb662a1663511a3706125bb8e9fc484d; anchor=function OwnerStatus]`.
- Target: `tools/observe-desktop.ps1:28 [UTF8-LF sha256=c2adb041290e63d2190c6e807c095b9a79bf3c201d46f5ffc085636c1d4daf5e; anchor=function OwnerStatus]`.
- Retained: Full donor document read through EOF. Apply its concrete distinction between working IPC/service and actual shell/GUI readiness: expose tray Shell acceptance separately, retain failures and recover only after accepted synchronization, verify open drafts before graceful replacement and keep actual runtime proof separate.
- Retained: Actual already-owned ASUS replacement/capture helpers were reread and their PID/path/hash/session/no-draft/graceful-exit/atomic-rollback checks adapted into reusable explicit-parameter replacement and600s USB/Wi-Fi observation tools. Both preserve settings and reject unknown owners; observation captures actual message-hub and Windows Shell counters, buffers samples in RAM and writes bounded progress only on changes. This imports recovery boundaries, not all donor shell features.
- Omitted/replaced code: `Timeline navigation/follow/playhead/ruler/toolbar-order repairs and host-specific libmpv/Cafe deployment`. Disposition: **not-applicable**. Reason: Explicitly excluded timeline/mpv/media-seat domain and unrelated host identities. Generic no-nested-egui-lock principle, owned preferences and graceful replacement remain relevant.
- Omitted/replaced code: `Full shared shell-action JSONL launch/forward/GUI/helper evidence and Jump List helper-readiness acceptance`. Disposition: **pending**. Reason: Pakar already has typed IPC and native dialogs but does not contain this complete per-action stage receipt or full actual Explorer dispatch matrix. New tooltip acceptance diagnostics do not substitute for it.
- Omitted/replaced code: `Upload progress broken-pipe panic regression and disconnected-writer acceptance`. Disposition: **pending**. Reason: Actual Pakar CLI best-effort output has a separate implementation; this document read does not compare donor CLI writer sources/tests or certify all upload-output paths.
- Test-code evidence: `tests/windows_tray_test.mjs:37 [UTF8-LF sha256=541da356a3c3e376e2eacfa10dbaf068925074ca5c77a005ccf71121eb5639cb; anchor=Actual GUID-bound tooltip]`.
- Remaining: Import/test applicable shell-action stage diagnostics and output writer contracts from actual donor implementation, then verify user-visible native actions. Do not mark complete from this focused tooltip recovery.

### Pealayer: `README.md`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `4e78a6c486bfc7d78adc6d34552bddfd67c89997edb8d2bae0b46e70c705dcb6`.

- Target: `README.md:7 [UTF8-LF sha256=4ef7f2d6862ad8145fb849ea471f5c35c0c7ee9345ccfa250200319568bd7476; anchor=Connected boards. One contextual control workspace.]`.
- Target: `tools/generate-readme-artwork.mjs:1 [UTF8-LF sha256=2162d1ad3760618260eb9d659dfebb6651ad1e5f5b4eb2c37c12cde1ef7ff8d2; anchor=README uses the same bytes]`.
- Retained: Pealayer's actual centered logo/title/tagline, badge row, visual preview, section navigation, architecture, build, docs, contribution and license structure adapted to Pakar.
- Omitted/replaced code: `Media engine, NLE cue/timeline walkthroughs, playback APIs/shortcuts and libmpv installation sections`. Disposition: **not-applicable**. Reason: These describe Pealayer media business/runtime rather than Pakar board control. README links actual controller APIs, wiring and release/acceptance instead; no mpv is linked.
- Omitted/replaced code: `Donor platform and latest-release badges, cinema-grade completion language and media screenshots`. Disposition: **replaced**. Reason: Unsupported OS/release claims and unrelated images would misrepresent this repo. Use the shared application logo and real embedded Web screenshot labeled simulated, explicit PR/source and physical acceptance boundaries.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=c28cb1484b89e72a30d6d1971dcf2c1be42d29ccf1d57108ce72ad3b3ed4db33; anchor=console.log('Repository governance]`.
- Scoped advantage: Canonical cross-interface icon bytes and explicit proof/acceptance boundaries prevent a polished landing page from claiming an unaccepted release.

### Pealayer: `scripts/capture-visible-window-windows.ps1`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `94bc5aed91407818905412d50dde72ea669f3c799ef56a043dd0a81befdce47f`.

- Target: `tools/capture-window-windows.ps1:75 [UTF8-LF sha256=c9f48604a224599ea0252638558e80f4025beb4b53834112af9e48af03ae73d0; anchor=public static extern bool PrintWindow]`.
- Retained: Actual donor PrintWindow P/Invoke, minimized/window-bounds validation, DPI awareness, System.Drawing PNG lifecycle and read-only behavior.
- Omitted/replaced code: `Get-Process -Name foreground filter`. Disposition: **replaced**. Reason: Explicit expected PID and window-owner revalidation remove ambiguity between production and fixtures; screen fallback checks foreground ownership without changing focus.
- Omitted/replaced code: `RenderOccludedWindow branching and unconditional Bitmap.Save`. Disposition: **replaced**. Reason: Capability-gated egui framebuffer is tried first, then PrintWindow/BitBlt and verified foreground pixels. Blank images and overwrites are rejected.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=b3f330885285dca9d8386c1d8f39a6346569d168a04bde700ff7bd34f0a15ac1; anchor=for(const forbidden]`.
- Scoped advantage: PID-bound, no-overwrite capture with an app-owned framebuffer and automatic read-only Win32 fallback; runtime evidence is separate from source guards.

### Pealayer: `scripts/check-crate-boundary.mjs`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `da9b469ac596e6eb1999e4d499742a51ebf289335285549569116c1124963e38`.

- Target: `tools/check-crate-boundary.mjs:10 [UTF8-LF sha256=4a8fbcecaf424460c051de61d5e44658532ec64527460ce21d4d86ec95c0550e; anchor=export function checkCrateBoundary]`.
- Retained: Actual donor module extraction/intersection, thin executable and MSVC duplicate resource linkage guards imported/adapted. Native build wrapper and CI execute this read-only guard before compiling.
- Omitted/replaced code: `pealayer::startup::run and main.rs/lib.rs module intersection assertions`. Disposition: **replaced**. Reason: Pakar has one binary today. The adapted tool detects optional lib.rs and then enforces zero executable modules, module ownership and delegation to Pakar's library namespace. Today's single binary additionally rejects duplicate declarations without requiring a nonexistent Pealayer startup symbol.
- Omitted/replaced code: `MSVC winres library/bin duplicate resource.lib guard`. Disposition: **replaced**. Reason: Both original resource.lib patterns are retained and run for today's binary as well as a future shared-library layout. No need to defer these guards until the architecture changes.
- Test-code evidence: `tests/crate_boundary_test.mjs:4 [UTF8-LF sha256=ebf1cbc5410a3f67a7b704d17415801e68daeba961253f8b60a183fdb5fec17f; anchor=const single =]`.
- Scoped advantage: Executes useful donor guards immediately on the current single-binary layout, rejects duplicate declarations, and automatically activates library ownership/thin-entry checks when a library appears. No compiler or generated fixture writes.

### Pealayer: `scripts/libmpv-windows.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ab6e48f3e0a55a2d86ad1dbb19c01a23c751891cb5a7ed61875a21b1e52645e5`.

- Target: `tools/windows-build-host.ps1:16 [UTF8-LF sha256=7da123594866f591b490db084496b4bef75b2db3f6e0ac5b425b8d86e9401e9b; anchor=function Get-PakarCargoTargetDirectory]`.
- Target: `tools/windows-build-host.ps1:46 [UTF8-LF sha256=7da123594866f591b490db084496b4bef75b2db3f6e0ac5b425b8d86e9401e9b; anchor=function Test-PakarFileContentEqual]`.
- Retained: Actual Get-PealayerWindowsHostPaths, Get-PealayerCargoTargetDirectory and Get-PealayerFileIdentity adapted to Pakar names/locations. Environment expansion and relative overrides retained; native build and staging now call these helpers.
- Retained: New actual Test-PealayerFileContentEqual imported as Test-PakarFileContentEqual and called by staging: missing files fail, size mismatch skips hashing, equal lengths require SHA-256 equality.
- Omitted/replaced code: `Resolve-PealayerLibmpv, Copy-PealayerLibmpvRuntime and Set-PealayerLibmpvBuildEnvironment`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free; importing DLL aliases, GNU/MSVC mpv imports, LIBMPV_DIR and native linker flags would restore an excluded media dependency.
- Omitted/replaced code: `Save-PealayerWindowsHostProfile generic host provenance and generated Cargo-config ownership protection`. Disposition: **pending**. Reason: Saved host/compiler/cache provenance and refusal to overwrite user-managed configuration remain useful. Pakar currently resolves its established cache/explicit override but does not persist this generic host profile.
- Omitted/replaced code: `Program-root shared target default across separate source worktrees`. Disposition: **pending**. Reason: Pakar keeps its existing single canonical desktop/target cache without moving or duplicating it. A future managed-worktree build needs a shared Pakar-owned cache; copying Pealayer's owner path would corrupt ownership.
- Omitted/replaced code: `New Get-PealayerCargoTargetDirectory rejection of all source-checkout cache paths`. Disposition: **pending**. Reason: Pakar's existing sole desktop/target is inside the checkout. Applying this rejection now would strand the warm cache or force a duplicate cold build. An owned same-volume relocation and wrapper/config migration are required first; the oversized-cache guard remains active and blocked cleanup is not bypassed.
- Omitted/replaced code: `Copy-PealayerFileLowWrite delete-destination-first copy and PreferHardLink`. Disposition: **replaced**. Reason: Pakar retains its checksum-verified pending-copy then rename. The prior candidate is preserved until verification; a mutable executable is not hardlinked to compiler output. mpv immutable-runtime aliases are explicitly excluded, not a reason to omit generic content comparison.
- Test-code evidence: `tests/reuse_preflight_test.ps1:83 [UTF8-LF sha256=842c7d0149c2717f7378116063712f4b719941152d89bd43cdc31e60b148437d; anchor=Actual donor relative cache resolution]`.
- Test-code evidence: `tests/storage_hygiene_test.ps1:15 [UTF8-LF sha256=7e6d4223563e3e5f2b641928c87cda91fa06312815434d63d46b76602cd49dca; anchor=Actual imported equal-content helper]`.
- Remaining: Adapt generic saved host provenance/config ownership checks without mpv, and keep one owned cache if worktrees are later authorized.

### Pealayer: `scripts/package-probe-windows.ps1`

Verdict: **equivalent-file**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f`.

- Target: `tools/package-probe-windows.ps1:27 [UTF8-LF sha256=71604ab464776891dbc95db7123218c22b3ede09ce2835fe4659d75c6d0b9d5e; anchor=function Invoke-PackageProbe]`.
- Target: `tools/replace-desktop.ps1:68 [UTF8-LF sha256=13e5893983bf422449d0c35b94305a39bb662a1663511a3706125bb8e9fc484d; anchor=$reply=Invoke-PackageProbe]`.
- Retained: Byte-identical actual donor process-handle probe: no-shell hidden process, async redirected stdout, 30-second wait, explicit exit code, output return and Dispose. Optional staging probe calls this helper.
- Retained: Desktop replacement now also calls the retained process probe for before/after owner status and its one graceful quit. Actual ASUS inspection showed direct GUI-subsystem invocation could report false IPC failure; the retained probe read the same ready owner successfully. Live replacement preserved settings/rollback and recovered USB/80-cell mapping on the same ESP boot.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/reuse_preflight_test.ps1:87 [UTF8-LF sha256=842c7d0149c2717f7378116063712f4b719941152d89bd43cdc31e60b148437d; anchor=Actual imported short-lived process stdout]`.
- Remaining: Donor timeout throws without terminating its child. Full GUI/runtime package smoke remains separate from CLI identity.

### Pealayer: `scripts/package-windows.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `5651709cfb58c2f1e0d7052b3ca604df154563ce552498a066c6e67b8f9baedf`.

- Target: `tools/windows-build-host.ps1:62 [UTF8-LF sha256=7da123594866f591b490db084496b4bef75b2db3f6e0ac5b425b8d86e9401e9b; anchor=function Assert-PakarPackageSourceStable]`.
- Target: `tools/stage-desktop.ps1:38 [UTF8-LF sha256=1bbfe87fe8c9e6936e140c7911c5493f5937eceac1402ca386661dcabb40c4bd; anchor=Invoke-PackageProbe]`.
- Retained: Fully reread all 302 current donor lines through EOF and actual Pakar windows-build-host/stage-desktop helpers. Actual Assert-PackageSourceStable remains adapted without Web exclusions; optional ExpectedCommit checks run before/after build/stage. Imported bounded Process probe and file-identity helper are used by staging; identical hashes skip payload copies.
- Omitted/replaced code: `SkipTests selecting npm build:bundle rather than build, web_regression_tests skipped versus web_bundle_integrity verified manifest fields`. Disposition: **pending**. Reason: The current donor explicitly separates bundle integrity from omitted regression suites. Pakar staging does not run npm or build an external Web bundle; a complete package/manifest pipeline is still pending. Firmware source/embedded Web checks are not reported as those package operations.
- Omitted/replaced code: `libmpv runtime/import-library packaging and pealayer-downloader media companion`. Disposition: **not-applicable**. Reason: The user explicitly requested an mpv-free controller, not the donor media player/downloader. Board remote firmware contracts remain required separately.
- Omitted/replaced code: `Restore-PackageWebDistribution git restore and recursive generated-Web cleanup`. Disposition: **replaced**. Reason: Pakar staging does not generate or mutate a tracked Web distribution. Blind donor restoration could overwrite user work and is not copied.
- Omitted/replaced code: `Branding, Persian font/Web bundle packaging, Win32 resource and quick-action icon verification, UPX validation`. Disposition: **pending**. Reason: These are useful generic package capabilities. Existing app source contains icons/font but current staging does not validate/package all donor facilities.
- Omitted/replaced code: `Embedded --build-info/--smoke-test checks, GITHUB_SHA invalidation and host-manifest artifact/provenance/validation schema`. Disposition: **pending**. Reason: Current app lacks equivalent embedded source identity and full runtime smoke. Optional Git source-stability and CLI-help checks are narrower and cannot certify the old executable's source commit.
- Omitted/replaced code: `New Copy-PealayerFileLowWrite calls for executable/downloader/font and runtime aliases`. Disposition: **replaced**. Reason: Staging now calls the actual imported content-equality helper, skips identical candidates, and keeps verified pending-copy/rename rather than deleting the prior candidate first. No excluded mpv/downloader payload is created. Useful branding/font/Web packaging remains separately pending.
- Omitted/replaced code: `Timestamp package metadata and native optional Run`. Disposition: **pending**. Reason: User excludes dated/random project/package names, not necessarily diagnostic time fields. Preserve useful manifest provenance and bounded explicit launching in a future adapted package pipeline.
- Remaining: Import full mpv-free package manifest, embedded identity, resource/icon/font/Web validation and appropriate generic runner; do not call staging a full package release.

### Pealayer: `scripts/run-windows.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `013e5c67e227ed851f8a3d26b27654e38c8816edca2965381bbb1b48d10a750d`.

- Target: `tools/build-desktop.ps1:33 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Get-PakarCargoTargetDirectory]`.
- Retained: Shared resolver call and locked native build flow mapped to Pakar wrapper. Existing canonical cache is reused; explicit Check/Test/Release and storage/reuse gates avoid unnecessary links.
- Omitted/replaced code: `RustBin machine/user/toolchain executable fallback selection and rustc -vV host detection`. Disposition: **pending**. Reason: Pakar currently uses installed Cargo from PATH. Alternate host/toolchain executable fallback and recorded host triple are reusable infrastructure and remain unported.
- Omitted/replaced code: `libmpv host resolution/environment/profile and DLL copy`. Disposition: **not-applicable**. Reason: These operations configure the excluded mpv renderer. Pakar uses its own mpv-free linked dependencies.
- Omitted/replaced code: `BuildOnly versus launch forwarding with ApplicationArguments`. Disposition: **pending**. Reason: Pakar wrapper builds only. Generic built-executable validation and configurable run forwarding should become a Pakar runner; manual launching is not equivalent.
- Omitted/replaced code: `Set-Location to repository root`. Disposition: **replaced**. Reason: Absolute manifest and resolved target paths preserve caller cwd without introducing a second cache.
- Test-code evidence: `tests/build_storage_test.ps1:15 [UTF8-LF sha256=683f1926231ec42b3dacbc0ba7e2b7cb03315a375bde499bed31b2e9c1467c7a; anchor=One canonical target]`.
- Remaining: Import generic Windows runner/toolchain fallback and host-triple provenance without mpv.

### Pealayer: `scripts/storage-hygiene.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `0d8fdeb3e15fb52b917784d4cb3875c68bc12c6c6ed78cd16f2ff8ee3561d89c`.

- Target: `tools/storage-hygiene.ps1:11 [UTF8-LF sha256=0d1af57a83775e2670a5ac222973970dfc3bfc2c2e1bde1a1753523b444bdb5c; anchor=function Get-DirectorySummary]`.
- Target: `tools/build-mobile.ps1:82 [UTF8-LF sha256=4939c6bec5ca62b8d188d258f8a56f8c337a7f16b75ffa0b59d379b890bf535a; anchor=$taskCache=Get-DirectorySummary]`.
- Retained: Actual Get-DirectorySummary imported into tools/storage-hygiene.ps1 with Pakar canonical cache, board cache, mobile build and stable staging paths. Read-only by default and warns at the existing 8 GiB budget; logical sizes explicitly distinguished from disk allocation or wear.
- Retained: build-mobile.ps1 now calls the actual imported summary for its sole mobile/build cache and verifies free space before compilation. Existing SDK path comes from mobile README; no auto-install, dependency upgrade, cache relocation or clean.
- Omitted/replaced code: `PruneRepositoryTarget exact-child/git-ignore/allowed-child/running-process checks and recursive deletion`. Disposition: **replaced**. Reason: New explicit user authorization permits obsolete debug-only deletion. Adapt the actual donor exact-child, ignore, allowed-content, reparse and writer checks to the owned desktop/target/debug; add tracked-file refusal and Plan. Never remove the target root, release cache, settings or rollback. The verified 10.408 GiB debug tree was pruned; stronger guards keep the read-only default intact.
- Omitted/replaced code: `Pealayer target/shared-cache/staging paths and 48 GiB warning`. Disposition: **replaced**. Reason: Pakar uses its actual existing sole cache, Arduino and Flutter products and stable outputs staging, with the established 8 GiB warning. Importing another project's directories would misstate ownership.
- Omitted/replaced code: `Silently ignored recursive read errors and empty Measure-Object Sum`. Disposition: **replaced**. Reason: Unmeasured paths must fail visibly rather than understate storage. Empty directories use zero bytes instead of a missing StrictMode Sum property.
- Test-code evidence: `tests/storage_hygiene_test.ps1:10 [UTF8-LF sha256=7e6d4223563e3e5f2b641928c87cda91fa06312815434d63d46b76602cd49dca; anchor=Actual imported recursive summary]`.
- Test-code evidence: `tests/storage_hygiene_test.ps1:28 [UTF8-LF sha256=7e6d4223563e3e5f2b641928c87cda91fa06312815434d63d46b76602cd49dca; anchor=Default inventory stays read-only]`.
- Test-code evidence: `tests/reuse_preflight_test.ps1:97 [UTF8-LF sha256=842c7d0149c2717f7378116063712f4b719941152d89bd43cdc31e60b148437d; anchor=Mobile plan retains reviewed gates]`.
- Remaining: Debug-only cleanup is now authorized and implemented. Canonical nonduplicating RAM-disk/cache relocation, the newer donor Cargo-metadata/cache-tag proof for shared-cache migration, full fixture safety matrix and exact allocated-vs-logical accounting remain pending; no RAM disk has been allocated. Donor storage/CI files were reread while dirty concurrent peer work, not misrepresented as a new clean donor commit.

### Pealayer: `scripts/update-screenshots-windows.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `48507f6e9395c8c312c5557db729d05ed8d6a83e980d57a9b01e77ea525db287`.

- Target: `tools/capture-window-windows.ps1:75 [UTF8-LF sha256=c9f48604a224599ea0252638558e80f4025beb4b53834112af9e48af03ae73d0; anchor=public static extern bool PrintWindow]`.
- Retained: Actual PrintWindow(PW_RENDERFULLCONTENT), near-uniform-black sampler, GetWindowDC/BitBlt/screen fallback and PID/dimension validation imported. Pakar fixture supplies isolated profile/selected surfaces; app-owned framebuffer capture does not depend on WGC.
- Omitted/replaced code: `SetForegroundWindow, BringWindowToTop, SetWindowPos, ShowWindow, SetCursorPos, mouse_event, SendMouseWheel/Click/RightClick`. Disposition: **replaced**. Reason: Capture remains read-only and must not steal focus, resize or inject desktop input. Startup CLI sets the application's own surface; native gestures belong to the supported computer-use APIs.
- Omitted/replaced code: `DwmGetWindowAttribute extended bounds and logical/client coordinate transformations`. Disposition: **replaced**. Reason: Per-monitor-v2 physical GetWindowRect is kept consistent with full window-DC capture, avoiding a mismatched crop origin. Internal framebuffer needs no desktop coordinates.
- Omitted/replaced code: `Locale/Theme arrays, PreferencesTab/Icon, AppName/Branding, WorkspaceDockLayout and per-surface matrix manifest`. Disposition: **pending**. Reason: Config-driven Pakar CLI surfaces/isolated profiles exist, but the full multi-locale/theme/icon/width matrix and hashed per-capture manifest runner remain useful unported harness work.
- Omitted/replaced code: `temporary GUID profile cleanup, process CloseMainWindow/Kill and generated_at_utc`. Disposition: **replaced**. Reason: Stable named evidence/profiles are retained for debugging rather than deleted. Only verified fixture processes are stopped; production replacement uses graceful typed IPC and avoids timestamp/suffix deliverables.
- Omitted/replaced code: `native resize/scroll/client-coordinate scenario parameters`. Disposition: **pending**. Reason: Model/painter tests and selected-surface captures do not replace the full interactive narrow/wide scenario matrix. Native live gesture acceptance remains separate.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=b3f330885285dca9d8386c1d8f39a6346569d168a04bde700ff7bd34f0a15ac1; anchor=for(const forbidden]`.
- Remaining: Complete generic locale/theme/icon/width capture matrix and manifest receipts; verify native interaction scenarios.

### Pealayer: `src/application_shortcuts.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `1f173bef8acc2904eac5b0a91407aa4f7629ce65c88ea3a8ac78f8c13321d203`.

- Target: `desktop/src/ui/shell.rs:780 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=egui::Key::F11]`.
- Retained: Default Preferences Ctrl+, and device configuration Ctrl+Shift+, navigation exist; F11 fullscreen is restored with the shared typed dispatcher. These are not equivalent to the donor persisted rebinding model.
- Omitted/replaced code: `ApplicationShortcuts persisted fields, parse_shortcut, entries, validate, action_for_event and their non-media tests`. Disposition: **pending**. Reason: The foundation hardcoded its accelerators and omitted the reusable binding model. No accepted reason justifies leaving rebinding out. The actual donor depends on KeyChord and hardware_shortcuts match/key-code helpers, which must be ported and tested with controller-specific typed actions rather than guessed labels.
- Omitted/replaced code: `take_frame_step_shortcut and frame-step tests`. Disposition: **not-applicable**. Reason: These consume Ctrl+Left/Right for media frame stepping and timeline transport. Those business domains were explicitly removed; Pakar must not hijack ordinary text word navigation for nonexistent video frames.
- Omitted/replaced code: `containing_media_folder and local-media path tests`. Disposition: **not-applicable**. Reason: This resolves media files/streams, not board identities or firmware files. Firmware selection already uses explicit native file dialogs; introducing a nonexistent media folder action would be misleading.
- Omitted/replaced code: `media_information and media_folder accelerators`. Disposition: **pending**. Reason: The media operations cannot be retained as-is, but their reusable information/folder-navigation affordances need explicit hardware-monitor and application-folder action decisions. They are not silently treated as completed controller mappings.
- Remaining: Restore validated persistent rebinding, exact-modifier matching, repeat/release rejection and text-edit protection; separately reconcile global/hardware shortcut ownership.

### Pealayer: `src/branding.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ee6ef94e685a86e72258e4609692a9fefa516d5935d75d15a5abe0ee42ada3c1`.

- Target: `desktop/src/branding.rs:63 [UTF8-LF sha256=100940d0ee954ecfeb059dba83245c37c4e1e325d9309c7c82870e4eb870870a; anchor=pub fn icon_data_from_bytes]`.
- Target: `desktop/src/brand_image.rs:25 [UTF8-LF sha256=ab71607dcde16d7af14b0a292982f902186dacf6b5b78b3f4c983e37464428e9; anchor=pub fn native_icon_bytes]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `desktop/src/branding.rs:8 [UTF8-LF sha256=100940d0ee954ecfeb059dba83245c37c4e1e325d9309c7c82870e4eb870870a; anchor=pub fn sync_peer]`.
- Target: `mobile/lib/main.dart:1109 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=peer.application?.icon ?? bundledApplicationArtwork]`.
- Target: `mobile/lib/application_artwork.dart:2 [UTF8-LF sha256=a1ee8fc15b0aa9111a5c416fcf74b3d05093b22e47b76d7744837f175f7b5ef4; anchor=const applicationArtworkBase64]`.
- Retained: Actual icon_data_from_bytes imported unchanged; one embedded PNG supplies native/Web artwork without disk caches.
- Retained: Actual native_icon_bytes seven-frame ICO encoding/master preservation/Lanczos3 resampling and shell derivative test adapted to shared supplied bytes and PNG-only decoder.
- Retained: Actual sync_native_window_icon source-signature cache, multi-viewport icon publication and tray refresh lifecycle adapted to fresh selected-peer Application metadata; no per-frame decode or disk cache.
- Retained: Flutter now shares the exact embedded native/Web PNG fallback instead of a different Phosphor glyph, with bounded peer-application title and inline PNG overrides and default-artwork decode fallback. Android launcher equivalent and runtime icon preferences remain pending.
- Retained: Current 447-line donor and actual Pakar branding/ICO conversion were reread completely. Shared image decoding, seven-frame ICO master preservation and source-signature/multi-viewport mechanisms survive; current native window and artwork preferences still have explicit gaps.
- Omitted/replaced code: `resolved_icon_path/state_env/non_empty_path, builtin presets, icon_bytes precedence/fallback and override/preset tests`. Disposition: **pending**. Reason: Useful runtime preferences need a bounded shared board/client transport; local filesystem media-state paths cannot be copied blindly to ESP.
- Omitted/replaced code: `sync_native_window_icon, icon_source_signature/revision metadata/multi-viewport/tray sync and tests`. Disposition: **replaced**. Reason: sync_peer now adapts the cache/multi-viewport lifecycle to peer Application metadata, shared header/About and tray artwork/Show label. Native runtime acceptance is still blocked by the cache guard; full icon preferences/tests remain open.
- Omitted/replaced code: `PlaybackIconState/from_player/playback_state_is_unambiguous`. Disposition: **not-applicable**. Reason: Video state business expressly removed; hardware state icons require a separate validated state contract.
- Omitted/replaced code: `Current apply_native_window_icon shared Preferences/main HWND path, native window_icons::apply fallback, (state,revision,HWND) sync stamp and file-metadata change detection`. Disposition: **pending**. Reason: Pakar currently caches selected peer Application metadata and emits viewport icon commands; it does not implement donor native HWND registration/replacement lifecycle, file override metadata or preset precedence. Preserve bounded inline peer artwork and import applicable native lifecycle only after Windows tests; playback business remains excluded separately.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=40b2c9c8389d03eb8aab309a7d4e15bcba7c1624365090a00f4ccddfcf436886; anchor=Actual Web application identity]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `src/cli.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `412b553fcdcd4436075af6bc93c956a4b7d9ac93a552a3458f2afabb8aa1448f`.

- Target: `desktop/src/cli_output.rs:21 [UTF8-LF sha256=712ce831a3d90b59d66cb0b4c499951c4492a34aa8c8b645368b0c667abbab54; anchor=pub fn parse_cli_output(]`.
- Retained: Actual format_help_message and PrintHelp/PrintVersion flag branches adapted with native short/long help/version assertions; terminate before config, network, GUI ownership and windows. Share device command help with existing TUI instead of duplicate stale strings.
- Omitted/replaced code: `Media target/chapter/playlist/seek/rate/player associations`. Disposition: **not-applicable**. Reason: User explicitly requires an mpv-free controller; media/timeline/cue business must not replace board control.
- Omitted/replaced code: `Embedded build identity, scoped peer/instance routing, unified remote request fallback, automatic verified host deployment/update status, headless Web-only backend and complete shortcut/command parser tests`. Disposition: **pending**. Reason: These generic contracts are useful but not completed by static help output. Current Pakar typed GUI IPC and board command routes do not prove full donor CLI parity.
- Test-code evidence: `desktop/src/cli_output.rs:33 [UTF8-LF sha256=712ce831a3d90b59d66cb0b4c499951c4492a34aa8c8b645368b0c667abbab54; anchor=fn actual_donor_short_and_long_help_version_flags()]`.
- Scoped advantage: Top-level flags do not consume matching tokens inside device command payloads; board commands retain their own parsing. CLI endpoint/user default to saved GUI preferences rather than a conflicting hostname.
- Remaining: Complete generic CLI parser/validation, command/IPC parity, embedded build identity and verified app-update contracts; media-only switches are explicitly excluded. Validate actual packaged help/version exit without creating windows and preserve Preferences/rollback when replacing the app.

### Pealayer: `src/diagnostics.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `92b065a4d8ff01ae3b4a81fa11fb6e5b671f85bb1f667be6af389e692fef9f71`.

- Target: `desktop/src/cli_console.rs:23 [UTF8-LF sha256=3019c88d40d96574b6d72eace694ac69f8aaceb1477c3a08e9698a54da04ab25; anchor=fn write_output(]`.
- Target: `desktop/src/main.rs:3444 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=cli_println!("{reply}")]`.
- Target: `desktop/src/terminal_ui.rs:949 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=cli_eprintln!(]`.
- Retained: Read complete152-line6485-byte donor through EOF. Actual cli_println/cli_eprintln, cli_output, write_cli_line and disconnected_cli_output_returns_an_error_without_panicking are adapted into cli_console. All primary native stdout/stderr call sites use fallible writes, including IPC, help/version, shell errors, management and TUI startup. Partial print support retains the existing table text interface without adding newline artifacts.
- Omitted/replaced code: `record_shell_action / append_shell_event bounded diagnostic journal`. Disposition: **pending**. Reason: Useful shell action provenance is separate from console output. Integrate a bounded privacy-safe journal with current app-management ownership before certifying the entire donor; do not log command payloads, credentials or media URLs.
- Omitted/replaced code: `panic_report_path / write_report / install_panic_reporter and bounded UTF8 panic-report test`. Disposition: **pending**. Reason: Pakar incident deduplication is not a crash reporter. Adapt safe profile resolution, process/thread/build identity and bounded latest-report retention separately; do not claim generic runtime diagnostics restored by a console fix.
- Omitted/replaced code: `peer-active thumbnail-cache path selection`. Disposition: **replaced**. Reason: No mpv or media-thumbnail cache exists in this controller. Future crash/shell journals must use the Pakar profile diagnostic directory and selected process identity, never a donor media cache.
- Test-code evidence: `desktop/src/cli_console.rs:82 [UTF8-LF sha256=3019c88d40d96574b6d72eace694ac69f8aaceb1477c3a08e9698a54da04ab25; anchor=fn disconnected_pipe_returns_an_error_without_panicking]`.
- Test-code evidence: `desktop/src/cli_console.rs:75 [UTF8-LF sha256=3019c88d40d96574b6d72eace694ac69f8aaceb1477c3a08e9698a54da04ab25; anchor=fn inherited_output_and_partial_tui_lines_are_preserved]`.
- Test-code evidence: `tests/windows_tray_test.mjs:13 [UTF8-LF sha256=541da356a3c3e376e2eacfa10dbaf068925074ca5c77a005ccf71121eb5639cb; anchor=assert.doesNotMatch(app,]`.
- Remaining: Finish bounded panic and shell-action reporting. Actual CLI/IPC pipes, detached GUI console absence and destination replacement must be verified separately from source assertions.

### Pealayer: `src/main.rs`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `452eb004c52bdcff6936de5602cfdddf5c925d58db9e1e48220fc9a21075b696`.

- Target: `desktop/src/main.rs:2 [UTF8-LF sha256=d086310a68ee04885d573c290e36bd5b0f7992509d9339129652647f1760285f; anchor=#![cfg_attr(windows, windows_subsystem = "windows")]]`.
- Target: `desktop/src/cli_console.rs:37 [UTF8-LF sha256=3019c88d40d96574b6d72eace694ac69f8aaceb1477c3a08e9698a54da04ab25; anchor=pub fn prepare(]`.
- Target: `desktop/src/application_identity.rs:3 [UTF8-LF sha256=c37a32df5c00a1cc559c58d226b87d6e9756e7d837b56488646bd0ecc54f26bb; anchor=pub const WINDOWS_APP_ID]`.
- Target: `desktop/src/platform/windows_jump_list.rs:13 [UTF8-LF sha256=d8009f9017e829ad9d3b7d4563acd8604ec92262d3ef2e4b6dac73c90bee20d9; anchor=wide_null(crate::application_identity::WINDOWS_APP_ID)]`.
- Target: `desktop/build.rs:37 [UTF8-LF sha256=61bd58fc81c6b05dda0474cc4419a119dad4ee4c948e90383b97ff2b924ac780; anchor=.set("CompanyName", application_identity::APP_NAME)]`.
- Retained: Actual complete six-line entrypoint read through EOF at5a6961ce. Import windows_subsystem=windows into the Pakar executable, preventing the OS from creating an empty console on ordinary GUI launches. GUI, shortcuts and updater helpers never allocate or attach a console. Explicit CLI/TUI commands may attach only to an existing parent console; inherited file/pipe handles are preserved.
- Retained: Use one prefix-free PakarController native identity for window startup, process AppUserModelID, Jump List and executable FileDescription/ProductName/CompanyName/InternalName. Existing updater child CREATE_NO_WINDOW flags remain. This row certifies the tiny entrypoint adaptation, not whole-file Windows Shell parity.
- Omitted/replaced code: `pealayer::startup::run() library delegation`. Disposition: **replaced**. Reason: Pakar currently has a single authoritative binary, not the donor media library. Keep its existing main runtime and no duplicate process globals. The imported crate-boundary guard enforces library delegation if a lib.rs is later introduced; creating a second runtime solely to copy a six-line entrypoint would violate ownership.
- Test-code evidence: `tests/windows_startup_test.ps1:24 [UTF8-LF sha256=3ab90d3410719896189b93df4358a00602aefca8cc948f3ebfdd079ba707433c; anchor=Reject $taskBytes]`.
- Test-code evidence: `tests/windows_tray_test.mjs:9 [UTF8-LF sha256=541da356a3c3e376e2eacfa10dbaf068925074ca5c77a005ccf71121eb5639cb; anchor=const consolePolicy]`.
- Test-code evidence: `tools/package-probe-windows.ps1:1 [UTF8-LF sha256=71604ab464776891dbc95db7123218c22b3ede09ce2835fe4659d75c6d0b9d5e; anchor=function Assert-PakarWindowsGuiExecutable]`.

### Pealayer: `src/messaging.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `44ba995614f2acc3230f6adc13c00500e6844db81aa36d5e60e7eeb9e90d6f9f`.

- Target: `desktop/src/messaging.rs:81 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=pub fn publish(]`.
- Target: `desktop/src/ui/shell.rs:711 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=value["messages"]]`.
- Target: `desktop/src/platform/notifications.rs:102 [UTF8-LF sha256=a7abb77a1a319025ffefc8a5ad94f51a59b191b04e59a2414e96b30c290f06e7; anchor=pub struct ConnectionNotice]`.
- Retained: Earlier MessageHub remains byte-identical to historical source hash 471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573: request validation, bounded queue, expiry/upsert/dismiss/revision/source identity and both original tests. The current 226-line donor was completely reread; new detailed diagnostics remain pending, not falsely described as current file equivalence.
- Retained: The shared bounded/upsert message hub now has truthful native owner-IPC snapshot exposure for actual toast monitoring. Host connection alerts require2s stable state to avoid transient loss/recovery pairs; command freshness still changes immediately. Distinct diagnostic causes remain in the existing incident ledger. This does not import the still-pending detailed-toast payload/disclosure feature.
- Omitted/replaced code: `Current Toast.details optional serialized payload, publish_with_details, public publish_detailed and bounded-diagnostics preserving-last-good-toast regression`. Disposition: **pending**. Reason: Pakar has no detailed-diagnostics payload yet. Import the 8192-character pre-mutation validation and preserving-last-good-toast regression together with disclosure UI, retaining existing distinct-event identity/upsert behavior.
- Test-code evidence: `desktop/src/messaging.rs:171 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=fn upsert_dismiss_and_expire_are_shared()]`.
- Test-code evidence: `desktop/src/platform/notifications.rs:217 [UTF8-LF sha256=a7abb77a1a319025ffefc8a5ad94f51a59b191b04e59a2414e96b30c290f06e7; anchor=fn brief_status_and_avr_flaps_do_not_generate_disconnect_recovery_pairs]`.
- Remaining: Import current bounded detailed diagnostics and native disclosure; historical hub equivalence does not establish current Web/TUI/IPC notification-surface parity.

### Pealayer: `src/network.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `22abaf1ca05c64d7e694b7b1d8216904df23cb84f47a0d23c6e91e78d40b86f2`.

- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Target: `desktop/src/ui/preferences.rs:263 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=Enable API bridge / repeater]`.
- Retained: Read through EOF. Actual NetworkInterface::show address enumeration/filtering reused for listener-bound IPv4 LAN advertisement, without guessed /24 routes.
- Omitted/replaced code: `discover_bind_targets rich interface chooser, address_detail/interface_presentation and parse_bind_addresses multiple binds`. Disposition: **pending**. Reason: Preferences currently provide one explicit numeric listen address plus port; donor adapter chooser/multiple binds are useful pending UI rather than excluded media logic. No interface reconfiguration or firewall mutation is authorized implicitly.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:685 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_loopback_registration_starts_no_discovery_transports]`.
- Remaining: Issue #2: discover_bind_targets rich interface chooser, address_detail/interface_presentation and parse_bind_addresses multiple binds. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### Pealayer: `src/platform/windows_quick_actions.rs`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `35f6de708a0e402f6958a4d8feeead5037b42174286050ddc98c494d4173e4cb`.

- Target: `desktop/src/platform/windows_quick_actions.rs:15 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=pub const WINDOWS_QUICK_ACTIONS]`.
- Retained: Actual typed action/resource catalog, equality traits, explicit negative resource-index selection, Preferences, Exit and generic Toggle fullscreen. Controller actions share the launch/IPC dispatcher and resource compiler.
- Omitted/replaced code: `Play / Pause, Previous chapter, Next chapter, Mute / Unmute media quick actions`. Disposition: **not-applicable**. Reason: Pakar intentionally has no mpv playback, chapters or media volume. These would call removed media business methods. Connect, device settings, refresh, workspaces and About are real controller navigation actions instead; generic fullscreen is retained, not misclassified as media-only.
- Test-code evidence: `desktop/src/platform/windows_quick_actions.rs:69 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=fn controller_shell_actions_have_unique_native_resource_ids]`.
- Remaining: Visible Windows Jump List clicks remain a manual acceptance gate; typed launch forwarding and resource uniqueness are tested.

### Pealayer: `src/platform/windows_shell_icons.rs`

Verdict: **equivalent-file**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a`.

- Target: `desktop/src/platform/windows_shell_icons.rs:45 [UTF8-LF sha256=e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a; anchor=pub fn shell_icon_ico]`.
- Retained: Byte-identical actual Phosphor rasterizer, centered glyph coverage, all seven ICO resolutions, bottom-up BGRA, DWORD-aligned AND masks and explicit directory offsets. Pakar build.rs keeps it for semantic quick-action assets; application PE/window/tray artwork now uses the shared master and actual Pealayer branding conversion.
- No production-code omission claimed for this reviewed file.
- Remaining: User-configurable icon packs and runtime state-icon transitions are a separate unresolved integration; retaining the generator does not implement that editor.

### Pealayer: `src/server/discovery.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `a707a76b8424353a304ec28e344e467e89823b81c1f4dca321ec3b7060069c6c`.

- Target: `desktop/src/platform/lan_discovery.rs:27 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=pub struct Advertiser]`.
- Target: `desktop/src/platform/discovery.rs:18 [UTF8-LF sha256=b9bdc05d5fc2ba1b37e0e634c9812387c1a39d1e2584e30401f72bef7f5d3828; anchor=pub fn ssdp_locations]`.
- Retained: Entire source read. Actual mdns-sd registration/browse/monitor, socket2 reusable UDP, bounded candidate queue/workers, SSDP/MNDP codecs and expiry registry adapted. Replace parked immortal lifetime with stop-owned daemon/threads; reject auto hostname suffix on NameChange and publish failure. Actual API-port metadata used in Dart too.
- Omitted/replaced code: `Pealayer cross-product public enrichment/identity merging and full native adapter-change recovery`. Disposition: **pending**. Reason: Do not discover unrelated media servers as compatible Pakar board APIs or claim generic Pealayer RPC. Dynamic interfaces and multi-NIC/IPv6 acceptance remain pending; source fixture is not router/firewall proof.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:649 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_mndp_uses_explicit_application_port_not_guessed_port80]`.
- Test-code evidence: `desktop/src/platform/lan_discovery.rs:668 [UTF8-LF sha256=023df5b70dedce5ed0d46ade991e5672ab9a406ee97e3c382f3ab22f223a4a82; anchor=fn bridge_ssdp_requires_source_location_binding_and_unique_headers]`.
- Remaining: Issue #2: Pealayer cross-product public enrichment/identity merging and full native adapter-change recovery. Core native USB/network command sharing has source/socket fixtures; actual ASUS/LAN/Android/physical acceptance and full opaque firmware/binary streaming forwarding remain open.

### Pealayer: `src/ui/board_info.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ab1c99e674e902e37626963e2dd5cf7d993965334411d5e33dafd1ea2bf79e51`.

- Target: `desktop/src/capabilities.rs:7 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=pub struct Manifest]`.
- Target: `desktop/src/ui/shell.rs:566 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=fn synchronize_context(]`.
- Target: `desktop/src/ui/device_settings.rs:21 [UTF8-LF sha256=43c0eb207de03c597ad69d10671655569ee1bc25ba16298869bc65c742d710db; anchor=pub(crate) fn available(]`.
- Target: `docs/DEVICE-CONTEXT.md:71 [UTF8-LF sha256=ad4669cbe23b48b4f6ee22634c21c015cd5b9767b8006d7316ad04d4ee99648b; anchor=## Exact reuse and remaining useful code]`.
- Target: `desktop/src/board_silent.rs:74 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=pub struct Transaction]`.
- Target: `desktop/src/ui/preferences.rs:154 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_controls(]`.
- Target: `mobile/lib/api.dart:1247 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<BoardSound> boardSound(]`.
- Target: `mobile/lib/board_sound_preferences.dart:10 [UTF8-LF sha256=abe3bc09c93503a1d3e56d138268bb27e4940a028b536a30eaa85c6f63c60509; anchor=class BoardSoundPreferences]`.
- Target: `firmware/esp8266/src/WebUi.h:1254 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function boardSoundTransaction(]`.
- Retained: Reread all 1280 lines through EOF. Prior authoritative overview/capability/front-panel adaptation remains; shared dropdown dependency is now imported, but full typed board settings and ACK consumers remain pending.
- Retained: Numeric display controls are independent of Text; peer title follows acknowledged name and lost capabilities cancel Table work.
- Retained: Reread complete donor source through EOF. Adapt MCU-owned persistent Silent bit independent of host/Windows notification audio, capability-acknowledged controls, explicit unknown/pending/verified states and full-record preservation. Native Preferences Board page uses existing PCController GET/SET/GET with real AVR ACK and persistent readback over the selected fenced HTTP or USB owner only; Web and Flutter Preferences use the same living contract. No unsupported buzzer is invented, no automatic uncertain mutation replay, and unchanged persisted flags skip EEPROM writes.
- Omitted/replaced code: `TABS/draw persistence; overview typed board/profile/protocol/build/EEPROM identity; optional_row/yes_no`. Disposition: **pending**. Reason: Existing settings/dialog owners retained; complete typed PCController identity/profile/readback and persistence/localization tests require a real adapter. Current manifest is not that universal adapter.
- Omitted/replaced code: `settings/settings_grid/setting_control/checkbox_setting/slider_setting/enum_combo/bit_checkbox/relay_mask_editor`. Disposition: **pending**. Reason: Full EEPROM draft/write/readback, menu/reset/reboot and relay/PWM/RF/LCD/macro editors remain useful pending contracts. Current driver absence hides them but does not certify generic helpers imported. The Silent checkbox is now adapted into actual native/Web/Flutter Preferences with full-record preservation and ACK/readback; the remaining editors and broader donor field set stay pending.
- Omitted/replaced code: `capability_badges/capability_column_count/capability_pill/state_badge/unavailable and responsive tests`. Disposition: **pending**. Reason: Availability semantics adapted; entire donor badge geometry, localized empty states and responsive/keyboard consumer tests remain unverified.
- Omitted/replaced code: `FRONT_PANEL_KEYS/key_icon fixed K1-K4 previous/next/decrease/increase`. Disposition: **not-applicable**. Reason: Our board has nine shift-register/ADC keys; four hardcoded identities would be false. Matching future adapters can advertise them.
- Omitted/replaced code: `front_panel_key_button and acknowledged menu/key/layout readback`. Disposition: **pending**. Reason: Nine-key state exists, but full remotely navigable front-panel/menu/key commands need per-board descriptors and donor ACK tests. Incompatible labels do not justify dropping reusable command helpers.
- Omitted/replaced code: `seven_segment_preview, section/row drawing`. Disposition: **replaced**. Reason: Existing Pakar seven_segment/dialog primitives own preview and geometry. A duplicate renderer would fork styling; fresh peer gates wrap the current owners. Native paint acceptance remains pending.
- Omitted/replaced code: `DropdownUiExt / dropdown::ComboBox board enum selector integration`. Disposition: **pending**. Reason: The tracked shared dropdown module is now fully read and imported, with native consumers and donor tests. This does not implement every generic consumer contract in this file: render-rate policy, workspace/language/direction actions and complete board EEPROM enum ownership remain independently pending. Native execution and visual acceptance are reported separately.
- Test-code evidence: `tests/web_context_test.mjs:143 [UTF8-LF sha256=69e5d441087ed22663b727aab6c76eb26501545c3e9d49c291486e8426397c79; anchor=Native wiring is source-checked only]`.
- Test-code evidence: `desktop/src/capabilities.rs:435 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=fn no_profile_or_individual_ack_means_no_controls]`.
- Test-code evidence: `tests/tone_player_test.cpp:45 [UTF8-LF sha256=09451f69a27d36472ebc99d13158d3989733373297dd7b4f3e54adb55ff7aeeb; anchor=const auto beforeMute]`.
- Test-code evidence: `tests/pc_contract_test.cpp:115 [UTF8-LF sha256=bce8d58cf8ff4f2ef97d82aeea929443e1c25ffa2ff1be9ab37c684587e7cab7; anchor=silentRequest[22]]`.
- Test-code evidence: `desktop/src/board_silent.rs:236 [UTF8-LF sha256=c043af00f09e18b54b60399ec725f41a41bb74ec405251d92b70771b00059078; anchor=fn board_silent_preserves_other_settings]`.
- Test-code evidence: `mobile/test/board_sound_test.dart:173 [UTF8-LF sha256=a2f372123560c0f6252c98468adb7a3d2f38736ce2edc5865f2cfc5823738604; anchor=Silent API uses actual AVR ACK/readback]`.
- Test-code evidence: `tests/board_silent_test.mjs:9 [UTF8-LF sha256=08cc93dd72639af64eed0d8d29c179393d6c363d411dff42618d7557cd048579; anchor=for (const mode of []`.
- Remaining: Typed PCController adapter, donor tests and native runtime proof remain #10/#3/#6. No whole-file equivalence or application superiority claimed.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.
- Remaining: Focused fixtures/builds are source evidence only. Physical ASUS USB/Wi-Fi, reboot persistence, muted welcome/menu/cue audio and subsequent unmute require the actual AVR; SSH8022 is currently unavailable. Full donor host routing/backend/configuration parity is not established.

### Pealayer: `src/ui/color_picker.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fe2bf96c968d83874903f5d06085775f5787cf24467fb0c7be3fcf53077c1aa4`.

- Target: `desktop/src/ui/color_picker.rs:25 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=pub fn remember_color(]`.
- Target: `desktop/src/ui/color_picker.rs:240 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=pub fn accent_picker(]`.
- Target: `desktop/src/ui/preferences.rs:234 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=super::color_picker::accent_picker]`.
- Target: `desktop/src/ui/i18n.rs:133 [UTF8-LF sha256=9281ab56aabd5fee932aaf46fb3bf67423702d3244947ca2be3f9a6548cdf901; anchor="Recent colors"]`.
- Retained: Reread all 550 current donor lines through EOF. Actual session-only12-color MRU deduplication, recent/shared swatches, integrated HEX-field swatch, full-width Custom choice, independently anchored palette/validated temporary HEX draft and prior donor tests remain adapted. The newer integrated Custom expansion/density behavior is pending; current production accent consumer is not a complete Preferences contract.
- Omitted/replaced code: `palette_contents baseline/draft ownership and integrated color_dropdown one-popup Custom expansion, dynamic screen-height clamp/growth, explicit arrow-key opening, control_density geometry/chevron`. Disposition: **pending**. Reason: Fully reread all 550 current donor lines through EOF. Pakar still uses its previously imported independently anchored Custom palette/validated HEX draft and fixed row geometry. The newer single-popup expansion and dynamic density mechanics require the real shared preference/density dependency and adapted focus/geometry tests before replacing those working owners.
- Omitted/replaced code: `Pealayer asset path, session ID and product-specific shared-colors heading`. Disposition: **replaced**. Reason: Uses the actual already imported shared palette asset, Pakar context ID and localized generic headings without disk persistence for recent colors.
- Omitted/replaced code: `Full System accent mode and shared native/Web/mobile Preferences contract`. Disposition: **pending**. Reason: This source module is imported, but the current persisted RGB-string consumer is not Pealayer dynamic OS-accent mode or cross-surface preferences ownership. The selected Preferences Accent branch was inspected; that entire dependency file is not certified by this review.
- Test-code evidence: `desktop/src/ui/color_picker.rs:373 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=fn accent_custom_row_opens_separate_palette_without_mutating_on_open()]`.
- Test-code evidence: `tests/native_dropdown_test.mjs:1 [UTF8-LF sha256=fb892411e1339a82d7de1be2df090f86ea46b71750fd15a482bf796afad468da; anchor=Wiring/provenance checks]`.
- Remaining: Import the full applicable shared Preferences contract and System accent ownership across surfaces; execute native behavior tests and capture the deployed palette. Source guards are not runtime or visual evidence.

### Pealayer: `src/ui/dialog.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `8a396d67862578f8727e43a952e82120b20c9c582ebcc5366ab0968ba8f704d8`.

- Target: `desktop/src/ui/cell_table.rs:426 [UTF8-LF sha256=c9369637a2243a44bdc458a96b5edd738a61a63cbc929c1f25fd57241e89f241; anchor=fn manager_selector(]`.
- Target: `firmware/esp8266/src/WebUi.h:179 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=/* Fixed manager slots:]`.
- Retained: Read all 952 donor lines through EOF. navigation_control's fixed allocation, clipped centered text and inside-stroke painting are adapted to manager_selector. Compact cell/row/state slots use the existing shared neutral animated cell_surface; state-dependent Button Frames cannot resize their text/content geometry. Selected widget metadata is retained explicitly.
- Retained: Current complete 1236-line donor and complete mapped Pakar dialog.rs were reread by the status/front-panel agent; historical bounded geometry, inside-stroke navigation, equal action sizing, RTL adjustments, numeric modifier/clipboard/wheel and scrolling mappings remain intact. This is source comparison, not native test execution.
- Omitted/replaced code: `navigation_control's 80px minimum, left-aligned icon/title/metadata slots and selected bg_fill tint`. Disposition: **replaced**. Reason: A compact cell needs a centered number in 32x28; row/action slots are 96/104x28. The user requires neutral shared selection, not a tinted navigation side rail. Fixed text positions and inside strokes are preserved; existing side-rail helper remains unchanged.
- Omitted/replaced code: `singleline_text_edit/workspace_overlay_layer/preferred_host/centered_default_rect/bounded_geometry/opaque frame/action buttons/action rows/navigation detail/section/compact_row/setting_row/scroll_column/fit_scroll and their donor tests`. Disposition: **pending**. Reason: Existing imported dialog helpers are present, but this scoped geometry fix does not certify each call site or host/resize/action/localization test. Useful generic code is not excluded; full consumer and test reconciliation remains issue #3 work.
- Omitted/replaced code: `numeric_stepper/numeric_adjustment/adjust_numeric/numeric_slider_wheel/parse_numeric_paste/receive_numeric_paste/numeric_context_menu and seven pointer/paste/wheel tests`. Disposition: **pending**. Reason: Helpers already exist in the imported module; bounded input geometry, paste ownership, wheel tail suppression and context behavior need their actual donor runtime tests restored and controller settings coverage. Hover selector acceptance is not numeric-control parity.
- Omitted/replaced code: `Current stable_toggle, fixed-slot close_button, confirmation_modal AlertDialog focus restoration/top-modal Escape/non-destructive backdrop, inline_caption_edit and density-aware singleline_text_edit`. Disposition: **pending**. Reason: These useful generic helpers are absent from Pakar and must be imported with geometry/accessibility/focus regression tests while preserving Pakar's RTL input alignment. Existing Preferences and device dialogs remain custom implementations, not equivalent implementations of the new helpers.
- Test-code evidence: `desktop/src/ui/cell_table.rs:1286 [UTF8-LF sha256=c9369637a2243a44bdc458a96b5edd738a61a63cbc929c1f25fd57241e89f241; anchor=fn manager_hit_slots_text_and_wrap_never_follow_hover_or_stroke_width()]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:11 [UTF8-LF sha256=c77c09c64eeceed32560cea3b1b0a0ebe361fefd936a6383fe7590c5ac983aa4; anchor=Manager must not allocate state-dependent selectable Frames]`.
- Remaining: Execute the added actual native painter test at narrow/wide widths with animation on/off, then capture/deploy through guarded workflows. Native Test is currently blocked by the canonical >8 GiB cache; Web checks are source/DOM-double evidence, not browser pixels or physical deployment.
- Remaining: Complete the named generic dialog call-site/test reconciliation in issue #3; no whole-file behavioral equivalence or whole-project superiority claim.

### Pealayer: `src/ui/dropdown.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `134f1613df05c9db44f8ccd178981a8eca4fb6c254ced6466229afca66b40ee9`.

- Target: `desktop/src/ui/dropdown.rs:368 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=pub trait DropdownUiExt]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=pub fn draw(]`.
- Target: `desktop/src/ui/preferences.rs:232 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=super::dropdown::ComboBox::from_id_salt("theme")]`.
- Target: `desktop/src/ui/device_settings.rs:90 [UTF8-LF sha256=43c0eb207de03c597ad69d10671655569ee1bc25ba16298869bc65c742d710db; anchor=super::dropdown::ComboBox]`.
- Target: `desktop/src/ui/shell.rs:1474 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=super::dropdown::ComboBox]`.
- Retained: Reread all 646 current donor lines through EOF against the retained target. Actual focus owner, enabled/selected entries, search focus, Arrow/Home/End/Enter navigation, disabled skipping, inside focus stroke, nested menu framing and closed-popup cleanup remain imported. The prior three native tests remain adapted to fixed32px Pakar rows; newer dynamic density/chevron and inline-expansion behavior is explicitly pending.
- Retained: Current exclusive-transport slice reread all646 donor lines again. Unified connection draw uses actual menu_ui/DropdownUiExt enabled/selected/focus registration, one lifecycle action, lazy selected-USB OS enumeration and closed-popup cleanup. Old dual-popup proof anchors were replaced with real new contextual/lifecycle tests, not kept as fictitious surviving tests; runtime execution remains separate.
- Retained: Revalidated original-byte donor SHA134f1613 at unchanged revision5a6961ce after fully reading all646 donor lines, the entire control_density dependency and all659 retained target lines plus native_dropdown wiring test. Staleness was a misplaced target-shell hash in the source SHA field, not a donor change. Corrected source/target binding only; dynamic density, chevron, inline expansion and the newer navigating guard remain explicit parity gaps, with no new runtime acceptance claim.
- Omitted/replaced code: `control_density::height/paint_chevron in trigger and choice geometry, temporary interact_size restoration, choice_with_close reusable expansion rows`. Disposition: **pending**. Reason: Fully reread all 646 current donor lines through EOF. Pakar retains fixed32px rows and the previous explicit keyboard/search-focus owner, but lacks the new dynamic density and inline expansion API. Current scoped advantage remains source-ID/texture handling, not full newer donor parity.
- Omitted/replaced code: `menu_ui navigating guard limited to trigger, registered choices or registered search`. Disposition: **pending**. Reason: Current complete-read comparison finds the retained target checks same-layer focus only. The donor additionally leaves arrows to unrelated popup editors. Port this generic focus ownership improvement and add a non-search editor assertion; it is not media business logic or an accepted exclusion.
- Omitted/replaced code: `Pealayer active-dropdown ID`. Disposition: **replaced**. Reason: Product-specific context key renamed to Pakar. No generic helper or donor assertion removed.
- Test-code evidence: `desktop/src/ui/dropdown.rs:440 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_arrows_seed_focus_skip_disabled_commit_enter_and_cancel_escape()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:519 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_search_keeps_text_focus_until_arrow_enters_choices()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:585 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_color_rows_are_full_width_equal_height_and_hover_stable()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:415 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn from_label_keeps_locked_egui_trigger_identity()]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:262 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=fn one_contextual_control_lazy_usb_scan_and_busy_guard()]`.
- Test-code evidence: `tests/native_dropdown_test.mjs:1 [UTF8-LF sha256=fb892411e1339a82d7de1be2df090f86ea46b71750fd15a482bf796afad468da; anchor=Wiring/provenance checks]`.
- Scoped advantage: Adds the locked egui from_label trigger identity for existing labeled settings and actual connection-control keyboard ownership with busy-port rejection. Clears texture ownership before native assertions to avoid a second panic masking a failure. Native execution and production visual acceptance must be reported separately from source wiring.
- Remaining: Integrate current donor control_density height/chevron dependency and configurable density through real persisted native/Web/mobile preferences, then port current dynamic-height geometry assertions; preserve existing focus/ID/selection behavior.
- Remaining: Port the donor navigating guard so unregistered editors in the popup retain their arrow keys, with a focused non-search editor regression; retain choice_with_close inline expansion as useful pending behavior.

### Pealayer: `src/ui/i18n.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `811a48221c47c903c1d70ca904ead90a7a6b7f5ee0ba9a5f26a7e60dd30a75c3`.

- Target: `desktop/src/ui/i18n.rs:6 [UTF8-LF sha256=9281ab56aabd5fee932aaf46fb3bf67423702d3244947ca2be3f9a6548cdf901; anchor=pub(crate) fn configure_ui_fonts]`.
- Target: `desktop/src/ui/dialog.rs:18 [UTF8-LF sha256=b3786964dd0c375f0f73325a4046b3fc0e3967ace4e7e0c4f66b6f52d66eda47; anchor=pub fn singleline_text_edit]`.
- Target: `desktop/src/ui/preferences.rs:236 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=AppLanguage::Persian]`.
- Target: `docs/PERSIAN-PARITY.md:48 [UTF8-LF sha256=d1f873fb589b406316d77e9ca2fcaf652fe8773b1534063504fc1ab516b506da; anchor=## Exact source-to-target mapping]`.
- Retained: Actual donor platform/UI face loading, Vazirmatn and Phosphor registration, application dictionary, identity-preserving visual_text, script detection, input alignment and horizontal/vertical direction helpers retained and wired into real native controls.
- Retained: Monospace Persian fallback and preference-driven priority refresh added without discarding later widget font registrations. Actual donor contextual glyph test adapted to both families; saved Unicode and peer identifiers never manually reversed.
- Omitted/replaced code: `configure_preferred_fonts/install_ui_fonts UiFontFamily cache, normal/monospace font-family preferences, named pealayer-status-strong and pealayer-status-mono-strong emphasis faces, newer generic hardware/effects/status dictionary additions`. Disposition: **pending**. Reason: Fully reread all 1665 current donor lines through EOF. Pakar retains its existing registry-preserving language font installation and both-family Persian fallback; it does not yet expose these font-family preferences or named status emphasis faces. Generic hardware/action/status translations still need actual corresponding Pakar consumers and localization tests; only media/timeline-specific copy remains excluded.
- Omitted/replaced code: `Media playback, cue/timeline reveal and keyframe-specific dictionary keys`. Disposition: **not-applicable**. Reason: They describe explicitly excluded media business; no corresponding controller surface exists. No generic UI/tooling capability is excluded on that basis.
- Omitted/replaced code: `Manage folders, Manage folder, New folder, Expand folders, Collapse folders, Move to folder, Delete folder, Confirm delete, Channels will be moved to Ungrouped, pending folder change and attached-board folder configuration copy`. Disposition: **pending**. Reason: Generic grouping behavior is useful but not yet integrated with actual board cell/group contracts. No fake folder actions or completion claim; retain on issue #4 for real grouping and localized acceptance.
- Omitted/replaced code: `Separate ttf_parser coverage assertions and exhaustive native surface translations`. Disposition: **pending**. Reason: Avoid introducing a duplicate font parser dependency: actual renderer glyph/contextual and font-retention tests pass in 8849ab8 Windows CI after headless output repair. The run completes 114 passed/1 unrelated menu-label failure; no abort. Additional Unicode, production rendering, device-settings and status/transport copy remain unaccepted.
- Test-code evidence: `desktop/src/ui/i18n.rs:1458 [UTF8-LF sha256=9281ab56aabd5fee932aaf46fb3bf67423702d3244947ca2be3f9a6548cdf901; anchor=fn contextual_persian_in_labels_and_monospace_inputs]`.
- Test-code evidence: `desktop/src/ui/i18n.rs:1420 [UTF8-LF sha256=9281ab56aabd5fee932aaf46fb3bf67423702d3244947ca2be3f9a6548cdf901; anchor=fn changing_language_preserves_widget_fonts]`.
- Test-code evidence: `tests/localization_test.mjs:42 [UTF8-LF sha256=f4daf6129230087ad05ba906b6bb777586dc10a432d395064adbd4f967bac0c5; anchor=Peer text identical to a dictionary key]`.
- Remaining: Inspect real deployed Persian inputs/menus/dialogs and finish application-copy/generic grouping parity. Actual 8849ab8 font-retention/shaping tests pass, but full native release remains open with one menu-label assertion; local cache budget remains enforced.

### Pealayer: `src/ui/icons.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `cd56a5849a0be621d8472b440c9cbac8b91bcaf603d228cb6148bba1eb554fc9`.

- Target: `desktop/src/ui/icons.rs:573 [UTF8-LF sha256=d9e95867777af0771f64322fef26958558c4c976cd3bdec5377a41754550a8da; anchor=pub fn searchable_workspace_icon_picker(]`.
- Target: `desktop/src/ui/shell.rs:2382 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=icons::searchable_workspace_icon_picker]`.
- Retained: Fully reread all1190 current donor lines through EOF. Production Phosphor vocabulary, stable names/kind fallback, searchable list/grid picker, UTF-8-safe match highlighting, bounded popups and disclosure/submenu helpers remain retained. New catalog/density/localized-title/fixed-header-slot behavior is separately pending, not certified by the prior eleven tests.
- Retained: Actual donor regression tests restored: eleven tests, including interactive egui-frame search/select/reopen, both themes and popup width.
- Omitted/replaced code: `control_icon_catalog with icon_catalog.rs complete1500+ repertoire and new alias tests; translated_icon_name/localized search and preset title tests; searchable_icon_button_sized/with_slot and fixed_header geometry test; density chevrons`. Disposition: **pending**. Reason: Pakar retains its prior Phosphor presets/search/list/grid helpers, not the expanded donor catalog or new localized title/density/fixed-slot consumer contracts. icon_catalog.rs remains an independently pending dependency; do not copy unavailable presets or claim the new tests imported merely by reading this file.
- Omitted/replaced code: `every_native_icon_editor_uses_the_shared_searchable_picker donor-only editor path list`. Disposition: **replaced**. Reason: That assertion references Pealayer media/4D editor files not present in this product. Replaced only its call-site assertion with Pakar's actual workspace editor; the other ten tests were imported. This does not excuse missing generic editor features elsewhere.
- Omitted/replaced code: `DropdownUiExt, dropdown_choice, register_choice and dropdown::ComboBox in searchable icon pickers`. Disposition: **replaced**. Reason: Actual donor search-focus registration, list choices, custom grid registration and lifecycle wrapper are imported against the fully tracked dropdown source. Product context IDs are adapted; all eleven existing icon tests remain. Native execution is separate proof, and UI-wide picker consumer parity remains pending.
- Test-code evidence: `desktop/src/ui/icons.rs:831 [UTF8-LF sha256=d9e95867777af0771f64322fef26958558c4c976cd3bdec5377a41754550a8da; anchor=fn pakar_workspace_editor_uses_the_actual_shared_searchable_picker()]`.
- Test-code evidence: `desktop/src/ui/icons.rs:960 [UTF8-LF sha256=d9e95867777af0771f64322fef26958558c4c976cd3bdec5377a41754550a8da; anchor=fn icon_picker_search_click_filter_select_and_reopen_work_in_both_forms()]`.
- Remaining: Imported compact/control picker helpers are not all used by current panels; UI-wide application/action parity needs separate file reviews.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.

### Pealayer: `src/ui/menu.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `a0196878c7b76b898d0a5791b122181d9045d3fbbe9f56de52bc43242edf2d9d`.

- Target: `desktop/src/ui/menu.rs:9 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=pub fn top_menu_button<R>(]`.
- Target: `desktop/src/ui/shell.rs:807 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=menu::top_menu_button(ui, crate::ui::i18n::text("File")]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=pub fn draw(]`.
- Target: `docs/NATIVE-MENU-COMPARISON.md:32 [UTF8-LF sha256=7319a38240f9c23a43f5a18c2f20d05b0c9d5ff273cf0e8f28b2ba98aa1aabd8; anchor=## Exact donor menu mapping and remaining decisions]`.
- Retained: Actual top_menu_button body imported with only application state ID/public visibility adaptation; all four production menu headings use active-menu sibling hover switching.
- Retained: Actual Button::shortcut_text idiom replaces space-padded primary labels in all six shortcut rows; separate right-aligned muted atoms are supplied by the same egui 0.36.2 dependency. Existing dispatch/Phosphor submenu retained; user-requested height policy unchanged.
- Retained: Earlier independent WebSocket/USB popup adaptation is superseded by the user-required single selected Wi-Fi-or-USB dropdown/lifecycle. Retained actual selector persistence, manual OS hardware paths, lazy factual enumeration and explicit Connect/Disconnect; no Both mode or automatic fallback. Complete donor readiness and fixed endpoint geometry remain pending.
- Retained: Current complete 883-line donor and full mapped Pakar menu.rs were reread by the status/front-panel agent. The actual top-menu helper and six shortcut_text consumers remain retained; this does not implement the newly added endpoint trigger.
- Omitted/replaced code: `TrackMenuState, track_menu_state, format_track_label; video/recent/close/media info/folder/timeline and Audio/Subtitles/Chapters blocks; track_menu_is_contextual_to_media_and_discovered_tracks and test_format_track_label_variations`. Disposition: **not-applicable**. Reason: These require media tracks, chapters, video/timeline state or a media undo stack expressly excluded from Pakar. Their disabled Undo/Redo placeholders are not functional controller actions. Generic config-folder/device information actions remain pending separately.
- Omitted/replaced code: `draw persisted application_shortcuts and Edit configuration action`. Disposition: **pending**. Reason: Current fixed accelerators match dispatch but are not the donor's validated persisted rebinding model. Restore parser/dependencies/non-media tests and controller action ownership; shortcut color/alignment does not certify rebinding.
- Omitted/replaced code: `draw ordered_workspace_profiles/active selection/reset save acknowledgment`. Disposition: **pending**. Reason: Manager exists but inline ordered active-profile selection/state and immediate reset persistence are not restored. Requires profile identity/persistence acceptance rather than labels alone.
- Omitted/replaced code: `draw Help Language/Direction/Keyboard Shortcuts menus`. Disposition: **pending**. Reason: Existing English/Persian preference lacks complete System language and Auto/LTR/RTL/configurable bindings contracts. Nonfunctional entries must not be shown as completed features.
- Omitted/replaced code: `estop_button, draw_estop_release_dialog and draw shared-latch readiness/hide cluster`. Disposition: **pending**. Reason: Generic vector control, authoritative readiness, release confirmation/cancel/skip preference and synchronized remote release remain useful safety imports. Remove only media pause; adapt actual hardware latch contracts and tests.
- Omitted/replaced code: `draw connection requested/connecting/connected status/manual discovery and remote connection/browser/download actions`. Disposition: **pending**. Reason: Pakar has connection/typed actions and firmware tools, but full donor lifecycle/readiness/authenticated endpoint adaptation is not compared or accepted. Media remote browser cannot be blindly substituted for firmware management.
- Omitted/replaced code: `DropdownUiExt selected workspace/language/direction entries and new unified dropdown lifecycle`. Disposition: **pending**. Reason: The tracked shared dropdown module is now fully read and imported, with native consumers and donor tests. This does not implement every generic consumer contract in this file: render-rate policy, workspace/language/direction actions and complete board EEPROM enum ownership remain independently pending. Native execution and visual acceptance are reported separately.
- Omitted/replaced code: `Current header_endpoint_combo fixed 154-pixel clipped trigger/full 240-pixel popup and density/font/disabled-hover geometry tests`. Disposition: **pending**. Reason: Pakar now has one unified method trigger and selected-method details, not independent route popups. It reuses240px popup choices but not the donor154px endpoint trigger/dynamic-density geometry assertions. Compare actual long-text/font/scale layouts before claiming geometry parity; no duplicate or pretend-ready control should be added to match labels.
- Test-code evidence: `desktop/src/ui/menu.rs:69 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=fn shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes()]`.
- Test-code evidence: `desktop/src/ui/menu.rs:192 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=fn sibling_hover_switches_only_an_already_open_menu_and_clears_closed_state()]`.
- Test-code evidence: `tests/native_menu_test.mjs:12 [UTF8-LF sha256=80ac61041de322c2c0e2c0fad379a74e6e89d02724f1b80f2d9fa6200c7ef937; anchor=actual donor helper must match after application ID/visibility/whitespace adaptation]`.
- Test-code evidence: `tests/native_render_lifecycle_test.mjs:31 [UTF8-LF sha256=655bb0a62d3144718b1b13d350fb7e416006b6c468e7e1445bbd2ebcb7c946dc; anchor=const compact=source=>source.replace]`.
- Remaining: Restore remaining useful generic menu contracts above against actual dependencies; see exact function-level mapping in NATIVE-MENU-COMPARISON.md. Native CI paint/pointer tests and running-app visual acceptance are separate from source guards.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.

### Pealayer: `src/ui/palette.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `c169db6a61b79ebb2ce00f0ca442305eefeea7b34740720a2cdf6424d6fef85d`.

- Target: `desktop/src/ui/palette.rs:36 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=pub fn apply(]`.
- Retained: Native semantic visual-role mapping and actual palette JSON; appearance geometry is not overwritten.
- Omitted/replaced code: `WebAppearanceState and donor native/Web shared-preferences assertions`. Disposition: **pending**. Reason: Pakar lacks the shared native/Web preferences contract. These are useful generic requirements, not media business logic; omitting them cannot be justified as superior.
- Omitted/replaced code: `Donor default Native palette`. Disposition: **replaced**. Reason: Pakar chooses Studio as its product default. Both palettes remain available; this is a preference decision, not a measured improvement.
- Test-code evidence: `desktop/src/ui/palette.rs:101 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn donor_surface_text_roles_and_geometry_are_preserved_in_both_themes()]`.
- Remaining: Embedded Web consumes the same JSON catalog through a stale-generation check. Complete mobile catalog use and live native/Web preference broadcast/readback; pure donor host-resolution tests do not prove connected-host synchronization.

### Pealayer: `src/ui/preferences.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `1fa2482a389fee84c79eef3d69ad8cc0ecb922c319fd222219af3e90d7e6f965`.

- Target: `desktop/src/ui/preferences.rs:67 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn sync_connection_owner(]`.
- Target: `desktop/src/ui/preferences.rs:181 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn connection_summary(]`.
- Target: `desktop/src/ui/device_settings.rs:35 [UTF8-LF sha256=43c0eb207de03c597ad69d10671655569ee1bc25ba16298869bc65c742d710db; anchor=fn avr_page(]`.
- Target: `desktop/src/ui/shell.rs:1515 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=pub(super) fn draw_led_settings(]`.
- Retained: Fully reread all2308 actual donor lines through EOF, including draft lifecycle, helper ownership, control rendering and tests. Pakar retains saved/config/previewed baselines, live preview, dirty close confirmation and rail/body/footer layout; this is a narrow adaptation, not complete Preferences equivalence.
- Retained: Adapted actual PreferencesDraft::sync_external_appearance triple-baseline merge to sync_connection_owner. External selected method/enabled/endpoint/user/port update all three baselines while preserving unrelated dirty edits. Preferences now shows only the selected method's summary and delegates clean drafts to the single canonical connection dialog; unrelated Save cannot restore stale connection owner fields.
- Retained: Adapt the existing donor draft separation and clean-navigation rule to a contextual Preferences Board tab for every acknowledged device, not only buzzer boards. Device settings opens only after PC preferences are saved/discarded; no MCU draft is copied into AppConfig. The LED settings page invokes the actual shared LED controls rather than a duplicate engine or link-only placeholder; USB settings header shows its selected port only.
- Omitted/replaced code: `NativePreferencesController helper process/HWND ownership, independent exit wake/watch, owned chrome and peer-active embedded authority`. Disposition: **pending**. Reason: Pakar uses its existing deferred egui viewport rather than the donor helper process. A matching appearance or native-dialog option does not establish owner-window, helper-exit cancellation or focus/chrome equivalence; import only after actual process/interop dependencies and host lifecycle tests are compared.
- Omitted/replaced code: `External ConfigFileWatcher90ms debounce, shared server config authority and complete dirty external-file conflict merge`. Disposition: **pending**. Reason: The actual triple-baseline algorithm is adapted only to the live connection owner fields. Other persisted/external fields and watcher/readback lifecycle are not covered by this change and must not overwrite unrelated drafts.
- Omitted/replaced code: `preferences_contract typed sections/groups, content-derived responsive rows, inline checkbox and ChoiceSlider/MultiSelect/numeric-step controls, density/font/motion parity`. Disposition: **pending**. Reason: Existing Pakar controls implement only part of the donor editor. The full typed-contract dependency and dynamic layout/control tests were not imported in this slice; no all-controls or visual parity claim.
- Omitted/replaced code: `Config-file open/copy/edit/folder menus, complete reload/import/export ownership, timed footer feedback and full save/preview failure lifecycle matrix`. Disposition: **pending**. Reason: Basic import/export and dirty Save/Discard behavior already exist, but do not implement the donor's complete picker/config authority, transient status and helper-exit matrix. Generic file-picker/control mechanisms remain useful, not excluded media business.
- Omitted/replaced code: `Playback/track/subtitle/history/media-association options and mpv audio refresh`. Disposition: **not-applicable**. Reason: The requested controller is explicitly mpv-free and has no media playback domain. Generic preference lifecycle, navigation, picker and rendering mechanisms remain independently pending rather than excluded with those options.
- Test-code evidence: `desktop/src/ui/preferences.rs:556 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn external_connection_owner_updates_all_baselines_without_losing_unrelated_drafts()]`.
- Test-code evidence: `desktop/src/ui/preferences.rs:592 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn connection_summary_is_contextual_and_dirty_preferences_cannot_open_another_editor()]`.
- Test-code evidence: `desktop/src/ui/preferences.rs:522 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn board_settings_navigation_preserves_unsaved_application_preferences()]`.
- Scoped advantage: One connection owner instead of conflicting Preferences editors, while preserving unrelated drafts and the donor's triple-baseline merge mechanism.
- Remaining: Compare/import the pending generic helper/controller/config-watcher and typed-control dependencies, then verify native visual/navigation/focus and dirty concurrent-owner behavior in the installed app. Guarded Rust tests are not live USB or human acceptance.

### Pealayer: `src/ui/status_bar.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `e407025a1ee959230634f8350de20d58432313d6dd0f734ada735b8fc9fec4db`.

- Target: `desktop/src/ui/status_bar.rs:77 [UTF8-LF sha256=35ce96f77e9f79608efe62881c2c32409a6d3cbdf809e4b1d0f3c4bca0f17dd5; anchor=fn hide_menu(]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=pub fn draw(]`.
- Target: `docs/CONNECTION-CONTROLS.md:1 [UTF8-LF sha256=002ee046e9c206a4cc241ad2b9033971b4933959d8e14f43958848403879f21d; anchor=# Front-panel transport dropdowns]`.
- Target: `firmware/esp8266/src/WebUi.h:825 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function statusRgbOutput]`.
- Target: `desktop/src/status_rgb.rs:46 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=pub fn retain(]`.
- Target: `mobile/lib/status_rgb.dart:52 [UTF8-LF sha256=5d36a3437fdc52fb467bceee9599575b4f47799fe2944adf2fd10ddc1c7caec8; anchor=class StatusRgbPreview]`.
- Retained: Current complete source read through EOF independently by the status/front-panel and network/UI agents; selected target changes reviewed again during evidence integration. Actual per-item hide context menus, persistent visibility choices, bar-level restore menu and small truncating message labels remain adapted. Transport controls remain usable offline without claiming hardware readiness.
- Retained: Actual status_led_display_color direct emitted RGB semantics, 20x22 Widget::StatusRgb slot, circle radius6, hover metadata/copy-color/hide menu and no-double-dimming test adapted into native status bar. Web uses the same authoritative observed channels, individual availability/output and fresh-peer guards; no locally reconstructed status animation.
- Retained: Added compact physical RGB observations across native/Web/mobile without full-workspace refresh: exact board/boot, output validity and monotonic RGB revision gate each update. Observed RGB never renews the whole board lease, alters selected cells or replays old animation. Native and Flutter dedicated settings/preview modules require explicit board observations, persisted ACK/readback and safe onboard-pin confirmation; generic donor custom profile/cue editors remain pending.
- Omitted/replaced code: `current_status_message update-manager priority and OSD lifetime; fullscreen exclusion; workspace label; i18n/RTL layout`. Disposition: **pending**. Reason: Pakar retains a status message and item menu, but does not yet share authoritative update/OSD lifetime, workspace text or localized/RTL/fullscreen policy. Restore actual dependencies and test rather than claim full-file parity.
- Omitted/replaced code: `draw_hardware_status board_ready/coordinator/active_transport distinction and telemetry voltage/current/temperature/warnings`. Disposition: **pending**. Reason: Pakar peer freshness/ACK gates and actual S3 physical RGB mirror now exist, but generic coordinator, sensor and warning inventories still need actual adapters before display. Native dropdown live indicators and local pixel-writer acceptance are not independent physical measurement.
- Omitted/replaced code: `prepared_timeline, media-rate/fps mode, hardware timeline revision/feedback; estop_active label`. Disposition: **not-applicable**. Reason: Prepared media/timeline playback is expressly excluded from this mpv-free controller. No E-stop contract is acknowledged by the current pair; reusable latch safety remains tracked in menu comparison, not fabricated as present.
- Omitted/replaced code: `Pealayer hardcoded connection dot RGB`. Disposition: **replaced**. Reason: Pakar transport controls use its existing shared palette green/current text roles; menu state stays readable in both themes without tinting unrelated hardware controls.
- Omitted/replaced code: `UI render-rate toggle/context policy, latest dropdown-backed FPS menu, new widget ordering/dividers/fonts/value ranges/chart history/update operation ownership`. Disposition: **pending**. Reason: The tracked shared dropdown module is now fully read and imported, with native consumers and donor tests. This does not implement every generic consumer contract in this file: render-rate policy, workspace/language/direction actions and complete board EEPROM enum ownership remain independently pending. Native execution and visual acceptance are reported separately.
- Test-code evidence: `desktop/src/ui/status_bar.rs:218 [UTF8-LF sha256=35ce96f77e9f79608efe62881c2c32409a6d3cbdf809e4b1d0f3c4bca0f17dd5; anchor=fn rgb_requires_observed_output_and_does_not_double_dim]`.
- Test-code evidence: `tests/interface_settings_test.mjs:46 [UTF8-LF sha256=ecb4542141e12df8ab5409f1154cbe5e39466e5e23c3c39301f36eb6c17a77f2; anchor=Actual emitted RGB is not brightness-scaled again]`.
- Test-code evidence: `desktop/src/status_rgb.rs:157 [UTF8-LF sha256=3dbe61cf121e44b14fe047427fbe9d0b8e60735baa445ad3116310560631965e; anchor=fn compact_rgb_updates_only_the_matching_peer_and_newer_rgb_not_hardware_clock]`.
- Test-code evidence: `mobile/test/status_rgb_test.dart:70 [UTF8-LF sha256=908b1d6f4f4832dea297e0e36b9ab87500754569b595938d3cecfb95870bb61f; anchor=physical RGB is observed not synthesized or double dimmed]`.
- Test-code evidence: `tests/connection_controls_test.mjs:1 [UTF8-LF sha256=574c605348414b7f65ce48f3e92821ab1abdf41f05ccb4be1d6f7b5526d6a38b; anchor=Source wiring guard only.]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:371 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=fn lifecycle_is_one_explicit_action_and_disconnect_retains_port()]`.
- Remaining: Run native popup/pointer tests, deploy and capture actual controls; restore useful generic status lifetime/readiness/localization contracts in issues #3/#10.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.

### Pealayer: `src/ui/toasts.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `30d4ff8c9a30b783f9513a3508787e9ca233881bb3a9bda2e1b9f9cd1bc9c989`.

- Target: `desktop/src/ui/toasts.rs:4 [UTF8-LF sha256=8772d049fab0fba597bd7f4ba4403c7d7bd122f6027442e25f32da11d8f058f6; anchor=pub fn draw(ctx:]`.
- Retained: Actual donor root-only overlay: four newest messages, severity icons and semantic palette roles, bounded width, scrollable text, title, dismiss and deadline-driven repaint.
- Omitted/replaced code: `interop::get_live_config() accessor`. Disposition: **replaced**. Reason: Pakar owns the same live palette through config::live(). The OS-native notification route is separate; adding an explicit in-app enable preference does not remove the donor overlay when enabled.
- Omitted/replaced code: `Current optional Toast.details collapsible disclosure and localized details_label`. Disposition: **pending**. Reason: The complete current donor was reread against Pakar's actual renderer. Pakar still renders title/message only. Import bounded detailed diagnostics together with MessageHub validation and regression tests; earlier overlay behavior is retained but is not current whole-file parity.
- Remaining: This native overlay mapping is not shared Web/TUI message-delivery parity.

### Pealayer: `src/ui/workspace_profiles.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `410d274f51e5e7db7ce81582d36166a97240b097e993cec5fee8707a95183e2f`.

- Target: `mobile/lib/output_panel.dart:14 [UTF8-LF sha256=2ac6dae78e14f1b965de658ac94f7da466c1269c795c011c6ca407fdf7e7142f; anchor=class OutputPanel]`.
- Target: `mobile/test/output_panel_test.dart:93 [UTF8-LF sha256=1b7a3034588aeaa3e575881e902a1564a46c675fe2574ca38e88896a6d22e188; anchor=Output order is bounded, durable and independent of table selection]`.
- Target: `mobile/lib/table_workspace.dart:11 [UTF8-LF sha256=7167553e1f06bd93e60d6403ee6ac6268af8b2b72a840368902b99f698786149; anchor=class TableWorkspace]`.
- Target: `mobile/lib/main.dart:594 [UTF8-LF sha256=91ec67982b4d85fe7d5360c0dee66c8857571aad4e602a990f44d833f4fb272a; anchor=Future<void> _saveWorkspaces()]`.
- Target: `desktop/src/capabilities.rs:67 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=pub struct Presentation]`.
- Target: `desktop/src/ui/shell.rs:566 [UTF8-LF sha256=bb07c8adf22c32a151560813bcf63e8c68b0a23e12f1048764adf78e231d20e8; anchor=fn synchronize_context]`.
- Target: `desktop/src/ui/layout.rs:159 [UTF8-LF sha256=d368dc9cd57542eb9ff6170853619c79fc154f6d77a74be16db9c56fb9d4782c; anchor=presentation_context()]`.
- Target: `desktop/src/native_workspace.rs:87 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=pub struct Store]`.
- Target: `desktop/src/config.rs:168 [UTF8-LF sha256=f141aa4b44219e89bd0a89ed2d0097acfe95bc35b692449bdb3e13be298745ae; anchor=pub native_workspaces]`.
- Target: `mobile/lib/table_workspace.dart:154 [UTF8-LF sha256=7167553e1f06bd93e60d6403ee6ac6268af8b2b72a840368902b99f698786149; anchor=class FrontPanelWorkspace]`.
- Target: `desktop/src/workspace_layout.rs:6 [UTF8-LF sha256=2b6ad2be7a19a61268d0ce85c87537feacb68e53586297fc4d186d17787afeec; anchor=pub struct WorkspaceLayout]`.
- Retained: Complete live donor read through EOF at matching original-byte hash. Adapt the actual stable-ID profile_choices/draw ownership rule to board-keyed Flutter workspaces, rather than connection-attempt keys or telemetry-dependent widget lifetime. User selection/search/effect/indicator choice and both scroll axes survive transient leases, reboot, explicit reconnect and bounded background UI-cache restoration; snapshots never persist a command, authorization or observed device state.
- Retained: Reread the same complete live donor at unchanged SHA256 for the Android screen-off regression. Actual user changes now notify a bounded debounced UI snapshot writer, including indicators, multiple selection, transition, Find/selection, activity and both scroll axes. One ordered coalescing writer prevents late old saves, skips identical snapshots and permits retry after failure. Reattachment uses the latest actual offsets rather than stale scroll-end PageStorage values. Healthy sockets remain open across backgrounding; a silent pre-suspend socket gets a 750 ms read-probe recovery without resetting the workspace or replaying commands. Colour actions and selection controls are separate ordered rows.
- Retained: Actual stable-ID workspace ownership adapted to immutable board identity rather than mutable friendly names or current telemetry. Retained route/board/boot-scoped ACK-only panel inventory is presentation, never live hardware authority. Native dock, selection, menus and measured panel extent survive transient lease/capability loss; new peer/boot/explicit disconnect revalidates appropriately.
- Retained: Reread complete unchanged donor. Native durable board/application sessions now store sanitized dock/active tabs, egui scroll/collapse memory, unified selection/find/cursor and safe front-panel drafts, bounded16 records, equal-snapshot skips,1s sampling/debounce and5s minimum save interval. Flutter extends its existing ordered350ms/2s writer with front-panel, diagnostic and public settings drafts; reconnect requires fresh identity/readback and never restores pending actuation. Native capture/apply_workspace_profile and config structs were additionally reviewed as function slices only, not certified whole-file reads.
- Retained: Reread complete unchanged workspace_profiles.rs and actual canonical restore/reveal, capture/apply function slices in donor layout.rs/app.rs (those whole files remain uncertified). Native WorkspaceLayout now separates the durable full dock and explicit hidden-panel intent from its capability-filtered projection. Reconnect/partial ACK/reboot cannot save omitted panels as user closures; missing legacy defaults recover once, explicit close-all persists, reveal restores semantic sibling/split slots and unfolds ancestors. No missing ACK grants a control lease.
- Retained: Reread complete workspace_profiles.rs and scoped layout.rs card/drag/tab sections (whole layout.rs remains pending). OutputPanel extracts scene, LED output and rainbow from TablePage using the same live callbacks. Stable scene/output/diagnostics card IDs and explicit handle-only Arrange reordering persist through the existing board-owned workspace writer. Diagnostics opens the existing guarded PixelDiagnosticPage; no duplicated command engine or startup actuation. Table selection, cell transitions, mapping and scroll ownership remain unchanged.
- Omitted/replaced code: `draw workspace manager metadata CRUD, icon picker, profile reordering and overwrite/restore menu commands`. Disposition: **pending**. Reason: Useful generic workspace-management UI remains an open donor import. This regression repair adapts stable identity and user-state ownership only; it does not certify the native/Web workspace manager, add a replacement profile editor or copy egui widgets into Flutter.
- Omitted/replaced code: `All donor profile dialog/tab presets, media-specific panels and workspace action parity`. Disposition: **pending**. Reason: Stable identity and transient-layout preservation are a scoped adaptation, not all donor profile editor/defaults/import-export UI. Media-only panels remain outside this controller; generic profile actions still need exact comparisons.
- Omitted/replaced code: `Unsaved native privileged network/cloud/AVR form drafts`. Disposition: **pending**. Reason: They remain intentionally invalidated on peer revocation until durable public-only patches can be rebased against fresh baseline/secret ownership. General per-peer workspace controls and safe presentation drafts are persisted; this is not all editor-draft parity.
- Test-code evidence: `mobile/test/workspace_lifecycle_test.dart:513 [UTF8-LF sha256=da99bd0f4eb2b7cac91106da4673cab150929a028dcc0001a675c51f8fb148a9; anchor=Workspace follows board identity]`.
- Test-code evidence: `desktop/src/capabilities.rs:315 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=fn presentation_retains_only_prior_acks_without_reviving_live_authority]`.
- Test-code evidence: `desktop/src/capabilities.rs:365 [UTF8-LF sha256=4dbae022a44ca658decc88dcd4bb322dbd4cf454a5d7056094ab8392b4014973; anchor=fn measured_panel_height_preserves_scroll_only_for_same_board_panel_and_width]`.
- Test-code evidence: `desktop/src/native_workspace.rs:225 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=fn workspace_roundtrip_preserves_unified_selection_drafts_and_scroll_without_authority]`.
- Test-code evidence: `mobile/test/workspace_lifecycle_test.dart:135 [UTF8-LF sha256=da99bd0f4eb2b7cac91106da4673cab150929a028dcc0001a675c51f8fb148a9; anchor=Front-panel]`.
- Test-code evidence: `desktop/src/workspace_layout.rs:103 [UTF8-LF sha256=2b6ad2be7a19a61268d0ce85c87537feacb68e53586297fc4d186d17787afeec; anchor=fn reconnect_and_reboot_restore_splits_active_tabs_and_hidden_choices]`.
- Remaining: Actual Android installation, background/process pressure, Wi-Fi/router loss and human layout/selection acceptance remain open. The newer prior APK was staged but not installed because no authorized ADB target was available; never attribute its source checks to the running phone. Complete generic workspace-management imports separately; source fixtures are not phone deployment or donor-wide parity.
- Remaining: Issue #10/#3: runtime visual/scroll acceptance and remaining generic workspace dialog/action parity remain open.
- Remaining: Installed native restart/reconnect/scroll and Android process-pressure/human acceptance remain separate from serialization fixtures. Native window geometry and full named workspace manager CRUD remain pending.

### Pealayer: `src/update.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `909e8aa4f903d1a8a63589fd2e371d77ff7b8c6c6bcb3fc9949609ab55440652`.

- Target: `desktop/src/client_management.rs:278 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=pub fn newer(]`.
- Target: `desktop/src/client_install.rs:268 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=pub fn helper(]`.
- Target: `desktop/src/ui/preferences.rs:94 [UTF8-LF sha256=ce0d5b61d484880803203cf8cd9342314f4b20d297c5c0935b56b174e4607d49; anchor=fn app_management(]`.
- Retained: Actual download_update / prepare_self_update / run_update_helper / inspect_executable / create_update_file adapted into streamed SHA256/PE validation, explicit current-user managed marker, pinned PID/path/helper journal and graceful-exit/health receipt.
- Omitted/replaced code: `Donor-wide installer/recovery/platform acceptance outside this self EXE slice`. Disposition: **pending**. Reason: Force-kill and automatic deletion of rollback are deliberately not copied: a live editor or unverified new process must remain recoverable. Media/libmpv, archive/GitHub extraction, interactive TUI console inheritance do not apply to this GUI raw same-origin executable. Generic signed installer/removal inventory, unattended uninstall and true two-new-version helper/Windows-session/OS acceptance remain pending. Preference UI uses actual existing Pakar donor widgets; no full donor GUI parity is claimed.
- Test-code evidence: `desktop/src/client_management.rs:865 [UTF8-LF sha256=c5d56638ef1f784ffe9549846de3f4cacec332fe0284d78fedf491440800e9cb; anchor=release_policy_checks_numeric_build_semver_platform_origin_and_integrity]`.
- Test-code evidence: `desktop/src/client_install.rs:551 [UTF8-LF sha256=f3055251874225f674703a2d94ea90b26013246ff8c326275dfc7a184bdde286; anchor=journals_bind_all_transaction_paths_and_parent_without_shell_commands]`.
- Remaining: Issue #7: live secure enrollment, release/telemetry/command receipt and safe replacement proof separated from source fixtures; Android foreground-only and self-uninstall local/OS-consent boundaries remain.

### Pealayer: `tests/config_persistence_test.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `e06205e3e023e47e8a8c646e881044ce967e793628f7d52159fbbdf589a6f0d7`.

- Target: `desktop/src/native_workspace.rs:34 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=pub struct Snapshot]`.
- Retained: Completely reread actual test. Adapt fresh-session serde roundtrip and persisted layout/state assertions to the actual native board/application workspace store, using actual egui ScrollArea state and DockState. No media dependencies or per-test temporary-directory cleanup copied.
- Omitted/replaced code: `Donor playback/media config fields`. Disposition: **not-applicable**. Reason: The requested controller is mpv-free; this fixture maps reusable persistence assertions, not media runtime restoration.
- Test-code evidence: `desktop/src/native_workspace.rs:259 [UTF8-LF sha256=7b4cf18add15622b4f9f7d01217294efcc4c6e0cc87a4b69f08782a8905e025b; anchor=fn same_peer_reboot_restores_only_its_workspace_and_other_peers_cannot_inherit_it]`.
- Remaining: Actual installed GUI restart/scroll acceptance and complete unrelated configuration field coverage remain open.

### Pealayer: `tests/i18n_surface_audit_test.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `504768bc9cf2fda32a8dab8b0f50d573c74e45001734f45c7ff3120f36333f63`.

- Target: `tests/localization_test.mjs:81 [UTF8-LF sha256=f4daf6129230087ad05ba906b6bb777586dc10a432d395064adbd4f967bac0c5; anchor=Adapt Pealayer tests/i18n_surface_audit_test.rs]`.
- Retained: Actual donor known-raw-caption prohibition and required localized-boundary audit pattern adapted to real shell menu captions, not renamed timeline/media files. Added peer text exclusion and reversible placeholder/language tests.
- Omitted/replaced code: `Literal media cue/keyframe/player forbidden-string fixtures`. Disposition: **not-applicable**. Reason: Pakar has no those media surfaces; substituting their filenames would manufacture coverage. Controller-specific boundaries and font assets are tested instead.
- Omitted/replaced code: `Complete native/Web raw-caption audit coverage`. Disposition: **pending**. Reason: The current test protects selected imported boundaries; it is not exhaustive. Remaining native settings, transport/status and dynamic Web copy are explicitly pending, and native Cargo tests are blocked by cache budget.
- Test-code evidence: `tests/localization_test.mjs:67 [UTF8-LF sha256=f4daf6129230087ad05ba906b6bb777586dc10a432d395064adbd4f967bac0c5; anchor=for(const key of]`.
- Remaining: Expand ownership audit as each real controller surface is translated; execute actual native renderer tests and acceptance instead of treating source guards as runtime proof.

### Pealayer: `tests/windows_shell_suite_test.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `3f1f8f7709dd7b0e9dad2c58d1b345e9a7b054bb1bcfbcff9c6aede6598791c6`.

- Target: `desktop/src/platform/windows_notifications.rs:144 [UTF8-LF sha256=0b39032fd9b88d51514b9cb39f0def7377016a6f073247404c1b1ccb67dcf550; anchor=assert!(Hook::new(42).is_err())]`.
- Retained: Actual Preferences action/resource catalog is validated; invalid HWND registration rejects rather than claiming success; owner exclusion is tested. Live notification acceptance and shown-callback evidence are recorded independently.
- Omitted/replaced code: `Playback-duration error/paused percentages and Play/Pause thumbnail/tray labels`. Disposition: **not-applicable**. Reason: The tests model removed video playback, not device transfer progress. Pakar uses indeterminate taskbar activity only for a real pending settings/maintenance request; inventing a media percentage would publish false progress.
- Omitted/replaced code: `PealayerApp::default and media playback_time/duration/is_paused assignments`. Disposition: **replaced**. Reason: Pakar is an executable-only mpv-free controller; creating the donor media engine is inappropriate. Actual controller IPC/live smoke and direct invalid-HWND tests exercise its own lifetime instead.
- Omitted/replaced code: `ensure_shell_initialized retry and repeated shell-lifecycle acceptance`. Disposition: **pending**. Reason: Tray creation failures are visible and close-to-tray cannot hide an app without a tray, but complete root-taskbar initialization/retry and Explorer restart acceptance are not covered by the current tests.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:139 [UTF8-LF sha256=0b39032fd9b88d51514b9cb39f0def7377016a6f073247404c1b1ccb67dcf550; anchor=fn utf16_limits_preserve_complete_surrogates_and_termination]`.
- Remaining: Add root-taskbar lifecycle/reinitialization and hidden-window/tray/notification-click acceptance without fabricating successful HWND installation.

### Pealayer: `web_ui/scripts/appearance.test.mjs`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `35f269e9463e49dbc41572683ca374be8674a93e0e97c84240838d34dc950ddc`.

- Target: `tests/web_appearance_test.mjs:1 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=First four tests port the actual Pealayer]`.
- Retained: All four actual donor contract tests ported: Neutral helper default, preference merge without unrelated-field loss/resolution leakage, host/offline theme/accent and repeated native event/explicit-theme handling. Additional shipped-code tests cover all palette roles, cancel/apply, contrast, identical-write skip, media events, malformed storage, cross-tab updates and session-only storage failure.
- Omitted/replaced code: `volume:37 unrelated media setting fixture`. Disposition: **replaced**. Reason: The unrelated value is endpoint:http://ledboard.local instead. Its preservation and original-object immutability assertions are retained; no actual test contract is removed.
- Omitted/replaced code: `Direct TypeScript test import`. Disposition: **replaced**. Reason: Tests import the actual adapted JavaScript functions; browser tests additionally execute the shipped embedded source, avoiding a separate mock appearance engine.
- Test-code evidence: `tests/web_appearance_test.mjs:30 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=live host theme events leave System preference unchanged]`.
- Remaining: Test execution proves module and simulated browser contracts, not connected native event transport or physical hardware.

### Pealayer: `web_ui/src/App.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ba2d7a35e2f887ec556b681cca24aaa6f263295208cbc80678bc248b61949845`.

- Target: `firmware/esp8266/src/WebUi.h:752 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function createWorkspaceStorage]`.
- Target: `assets/web/device-settings.js:93 [UTF8-LF sha256=d71e9797948b19a436210332ce17b9dab783076145dd695c4798d0de078515b4; anchor=function settingsObserve]`.
- Retained: Completely reread matching actual donor. Import/adapt readStoredJson/persistJson pending-map/timer/pagehide and per-surface scroll patterns into immutable board_id+application.id workspace storage. Selection/find/focus, presentation/panels, public drafts, settings navigation and scroll restore passively; equal writes skip, blocked storage retains in-memory state, and changed fresh settings keep their conflict baseline.
- Omitted/replaced code: `Cached player/runtime state, media navigation and React/Ant Design surface ownership`. Disposition: **replaced**. Reason: Adapt generic persistence to embedded DOM ownership with a workspace-only allowlist; no playback engine, stale live authority, modes/cells, notifications, boot identities or pending command replay.
- Omitted/replaced code: `Cross-origin browser workspace migration and full donor multi-surface navigation`. Disposition: **pending**. Reason: Browser localStorage is origin-scoped; renamed-origin migration and generic remaining surface actions require explicit export/import UX. No broad App.tsx parity claim.
- Test-code evidence: `tests/web_workspace_test.mjs:94 [UTF8-LF sha256=0c36a09e6b43cbb54286356f82778393b44d70fb7e608ce2eaa8633125ef7145; anchor=Repeated reconnect cannot silently accept the changed baseline]`.
- Remaining: Browser layout/actual reconnect/reboot and physical LED acceptance remain open; DOM doubles are not visual/device proof.

### Pealayer: `web_ui/src/appearance.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `b521c20a1f10e6f818978bcb09948c689b74caaf72df18bdec56f2f566f8db58`.

- Target: `assets/themes/appearance.js:4 [UTF8-LF sha256=62a7ebba994ad44f4e2d647bb97684aac9b563104f3f707b43573289e514ecdd; anchor=export function mergeAppearance]`.
- Target: `tools/generate-web-appearance.mjs:32 [UTF8-LF sha256=2318e70ce657ac29e2e4c1da18381cf7493179c23ca4368edf6b8828a73e5ff6; anchor=replaceBlock('// BEGIN SHARED APPEARANCE']`.
- Target: `firmware/esp8266/src/WebUi.h:9 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=// BEGIN SHARED APPEARANCE]`.
- Retained: Fully reread current source through EOF. The five previously adapted helpers remain in the target: mergeAppearance, resolvedAccent, resolvedAppearanceTheme, accentForeground and paletteName. Existing theme/palette/accent host resolution and luminance semantics remain; newer animation/density/font preference fields and font CSS helpers are explicitly pending, not claimed imported.
- Retained: Generated into the shipped WebUi.h with the shared palette asset; --check rejects stale embedding. Browser preferences dialog validates theme/palette/accent, protects drafts, handles storage failure and cross-tab/System events without device writes or repetitive identical storage writes.
- Omitted/replaced code: `ui_animation_mode, ui_control_density, ui_font_family, monospace_font_family merge fields; ControlDensity/controlDensityHeight 24/28/32/36 geometry; uiFontCss and monoFontCss`. Disposition: **pending**. Reason: New current donor behavior was fully read during mobile-network build preflight. Existing embedded target does not consume these client preferences and does not bundle PealayerUbuntu/PealayerHack; copying their font names would imply unavailable assets. Add actual shared settings/asset support and cross-client tests before claiming parity.
- Omitted/replaced code: `AppearanceState TypeScript interface and generic T return annotation`. Disposition: **replaced**. Reason: ESP serves standalone JavaScript, not a TypeScript/React build. Browser-facing enum/color validation rejects untrusted stored values. New fields/helpers are separately tracked pending rather than silently dropped.
- Omitted/replaced code: `Connected host AppearanceState broadcast/native-Web shared preference path`. Disposition: **pending**. Reason: Pure functions accept host state and tests preserve donor semantics, but standalone browser controls are browser-local today. No actual native appearance event/readback transport is integrated in this pass; System fallback is labeled browser-local.
- Test-code evidence: `tests/web_appearance_test.mjs:22 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=System theme and accent preserve donor host resolution]`.
- Test-code evidence: `tests/web_appearance_test.mjs:66 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=dialog draft/cancel never changes]`.
- Remaining: Implement native/Web/mobile shared preference transport and live authoritative host events, while keeping device parameters separate from client appearance; verify multi-client broadcast/readback.

### Pealayer: `web_ui/src/components/HeaderBar.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ad53ee284d969054f69b65e8447a313498b5928eae0aed2564e95f8c71e70d04`.

- Target: `tools/generate-branding.mjs:3 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor=Pakar embeds it for the standalone ESP surface]`.
- Target: `firmware/esp8266/src/WebUi.h:228 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=<!-- BEGIN SHARED BRAND HEADER -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `firmware/esp8266/src/WebUi.h:949 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function renderApplicationIdentity]`.
- Target: `assets/web/device-settings.js:120 [UTF8-LF sha256=d71e9797948b19a436210332ce17b9dab783076145dd695c4798d0de078515b4; anchor=function settingsCanClose()]`.
- Target: `docs/INTERFACE-CONTRACT.md:8 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## One catalog, distinct transport and platform adapters]`.
- Retained: Actual resolved-artwork img/alt binding adapted to standalone header/login, replacing diamond and font fallback.
- Retained: Firmware name/title/artwork binding replaces fixed header branding; stale/disconnected peer resets title/header/favicon/touch artwork and validates bounded inline PNG.
- Retained: Actual retained Modal/draft lifecycle adapted to grouped device settings; separate same-instance WebSocket actions preserve HTTP refresh and reject obsolete socket callbacks. Native Draft/SecretEdit/Write validation/readback semantics are reused.
- Retained: Current 223-line donor was fully reread. Existing shared artwork/draft mapping is retained; accessible Enter/Space connection-pill activation, responsive overflow copies and capability-specific browser controls are not implied by that mapping.
- Omitted/replaced code: `Header collapse; arbitrary remote connectionTarget/draftTarget endpoint modal; localization/RTL`. Disposition: **pending**. Reason: The new grouped device-settings modal and same-instance WebSocket dropdown are not arbitrary remote endpoint selection or localized navigation. Standalone-device origin/auth and preferences adapters still need porting and tests.
- Omitted/replaced code: `capability-aware Dropdown install/update/share/fullscreen/notifications/haptics/audio/wake-lock cluster`. Disposition: **pending**. Reason: Requires actual WebPlatform integration and ownership/permission tests; not excluded media business.
- Omitted/replaced code: `Current keyboard-activatable connection-pill, collapsible action overflow parity and capability-aware web-feature preference switches`. Disposition: **pending**. Reason: Pakar must adapt actual endpoint lifecycle, capability/permission detection and keyboard semantics rather than presenting nonfunctional shortcuts. Shared static artwork and device-settings drafts do not provide these generic browser/header behaviors.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=40b2c9c8389d03eb8aab309a7d4e15bcba7c1624365090a00f4ccddfcf436886; anchor=Actual Web application identity]`.
- Test-code evidence: `tests/interface_settings_test.mjs:33 [UTF8-LF sha256=ecb4542141e12df8ab5409f1154cbe5e39466e5e23c3c39301f36eb6c17a77f2; anchor=Failed saves preserve the draft]`.
- Remaining: Complete generic omissions and missing editors named in INTERFACE-CONTRACT.md; native/mobile/deployed acceptance remains open.

### Pealayer: `web_ui/src/components/PreferencesTab.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `30a63cc1db61a46346c2d4c30ce011dc74081a8a6da2e29853e56d39811a0de1`.

- Target: `mobile/lib/device_settings_page.dart:18 [UTF8-LF sha256=143c7e774de6f90a48a1bcdbdf37c3241a9f86106b04768befa1b9c6f3d02774; anchor=class DeviceSettingsPage]`.
- Target: `mobile/lib/device_settings.dart:413 [UTF8-LF sha256=0c10e5c712bae2b4fe2f706326115ee703f07e426a64a50728c09575efb71e9a; anchor=class DeviceSettingsSession]`.
- Retained: Actual contract-driven sections/selected-section navigation, grouped labeled typed controls, contextual options, retained draft on section changes, descriptions, load/save status and reload adapted to Flutter DeviceSettingsPage using the existing shared Pakar catalog. Tablet uses a searchable navigation rail; phone uses wrapping section chips. Actual Phosphor repertoire supplies verified icon codepoints.
- Retained: Actual valueAtPath/updatePaths structured settings ownership adapted to separate public drafts and secret actions. DeviceSettingsSession uses the fully read canonical Pakar native/Web validation, observe/conflict and positive acknowledgment/exact public readback workflow rather than generic optimistic UI success. In-memory secrets never enter saved client preferences; one POST body per explicit operation, no mutation replay.
- Omitted/replaced code: `updatePaths optimistic autosave to /api/config and playback.preference_set`. Disposition: **replaced**. Reason: These are Pealayer state/RPC contracts, not Pakar persisted board endpoints. Password replacement and network restart require explicit confirmation, positive accepted/updated ACK and exact public readback. No optimistic physical save success or automatic write replay.
- Omitted/replaced code: `Ant Design components and ServerFilePicker transport`. Disposition: **replaced**. Reason: Android Flutter cannot import TSX/Ant Design components directly. Actual section/group/control logic is adapted to Material controls with the shared Studio palette, Vazirmatn and verified Phosphor glyphs. Mobile firmware now uses the fully read ServerFilePicker selection/ownership contract with native Android SAF; exact mapping and remaining generic consumers have their own comparison row.
- Omitted/replaced code: `AccentColorPicker, color swatches, appearance live preview, NumericValueControl steps/defaults, multi_select rich options, replacement_list add/delete, Collapsible groups, ServerFilePicker and refreshAudioOutputs two-stage option refresh`. Disposition: **pending**. Reason: Device Wi-Fi/cloud/identity/access editors are implemented, not the entire application preference framework. Generic appearance, collection editors, defaults/step controls, reusable file picking and dynamic options remain useful and require actual client/device capability contracts and tests before importing their controls.
- Omitted/replaced code: `tr(locale, ...) complete control/description localization`. Disposition: **pending**. Reason: Vazirmatn is loaded and the actual RayanLamp digit map normalizes numeric fields, but the new settings copy is English. Full mobile locale/direction must be audited and rendered before claiming Persian preference parity.
- Omitted/replaced code: `timelineWheelPreferences merge, playback audio-output RPC and media-extension preference consumers`. Disposition: **not-applicable**. Reason: Timeline/cue/media playback business was explicitly excluded from Pakar. Their generic dynamic-option and file-picker mechanisms remain pending above; they are not excluded as media business.
- Omitted/replaced code: `Current ColorDropdown custom/accent popup, discrete choice_slider indexed preview/commit and NumericValueControl persisted per-field adjustment steps`. Disposition: **pending**. Reason: Current 563-line donor was completely reread. These generic typed controls are not in the mobile device settings implementation; shared appearance controls, structured batch changes and keyboard/locale acceptance must be imported with their dependencies instead of claiming the earlier section/group adaptation covers them.
- Test-code evidence: `mobile/test/device_settings_test.dart:307 [UTF8-LF sha256=f9887ced8439ac531c05348bc39f5ab7344c6386eade342d18809a50a2741751; anchor=Settings write $behavior: one POST]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:251 [UTF8-LF sha256=e40bce43e135cc5f4508b37cca7e2cd4351852609101774636e55e2bf12c1177; anchor=Acknowledged $route handoff after old view closes]`.
- Test-code evidence: `mobile/test/capture_test.dart:181 [UTF8-LF sha256=c62fe3c9f6e73a0f4e0fae84925332c5dde99822f6e5680e95dc8c5dcb0cb06c; anchor=Capture must never issue a device mutation]`.
- Remaining: Mobile firmware/update/backup and Android picker workflows are implemented in the separate ServerFilePicker row; physical acceptance remains. Complete connection/discovery controls, unsaved Android-back/quit protection, client appearance/localization, generic typed preference controls and applicable donor dependencies. Verify physical phone/tablet configuration readback and hostname/credential handoffs; current API lacks transactional compare-and-set and encrypted HTTP transport.

### Pealayer: `web_ui/src/components/ServerFilePicker.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `df5b59007e0b726c815bed0eafcce87e4f5a6e4a59e8c79a1414f2cc7be89316`.

- Target: `mobile/lib/documents.dart:19 [UTF8-LF sha256=205c298afff227e61f4ddea3b071f65012e3ccc118cbd62f1de5dea8452eab1e; anchor=class AndroidDocuments]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:48 [UTF8-LF sha256=b6623d6be8e23e57532a5afc210a32ad4ca787cd40fb7d66d5796147917a322a; anchor=Intent.ACTION_OPEN_DOCUMENT]`.
- Target: `mobile/lib/firmware_page.dart:15 [UTF8-LF sha256=47062a176a76c93de964e710682b19b28c6fd1f3fd8f5ebaee525244f0dc681b; anchor=class FirmwarePage]`.
- Target: `mobile/lib/firmware.dart:178 [UTF8-LF sha256=190ddce7b1bbf298a74464ea8202287ead801bfe8725ab4d0af39993d5a6c30e; anchor=class FirmwareController]`.
- Target: `mobile/lib/api.dart:884 [UTF8-LF sha256=b7e7a2d629a7023cd429d4833f8a8c962826b4ffebc80eecfb600d0ac0cf6cc2; anchor=Future<Uint8List> maintenanceBytes]`.
- Retained: Fully read actual typed single-file selection, disabled/unselected/loading state, extension filtering, cancellation, stale-result protection and explicit acceptance. Adapted into Flutter AndroidDocuments + native SAF; firmware imports are validated and separately confirmed before any upload. Parent navigation is locked during maintenance confirmation/picking and writes; selected bytes never automatically program either board.
- Retained: Canonical Pakar desktop maintenance and actual firmware routes were read, not fabricated mobile endpoints. FirmwareController pins remote job/boot/hash/targets and verifies a fresh identity before apply/cancel; single-dispatch bounded HTTP refuses redirects/replay. FirmwarePage gates each ISP/OTA domain on individual live feature ACK, distinguishes stage/apply/completion, and exposes real full flash/EEPROM exports.
- Omitted/replaced code: `React/Ant Design modal/table row handlers, server /api/fs/browse, Directory.path/parent_path and AbortController network browse`. Disposition: **replaced**. Reason: Android SAF owns user-granted document navigation, search, single selection and cancellation. ESP has no arbitrary host filesystem browse contract; inventing that endpoint or accepting Windows paths would be incorrect. The native picker reads bounded bytes on its executor; captured board ownership is checked after picker/HTTP awaits. Only a one-use user-created destination token can be exported, with no broad storage permissions.
- Omitted/replaced code: `initialFile parent path, remembered folder, custom icon table/size labels and full localized picker controls`. Disposition: **pending**. Reason: SAF system navigation is available, but mobile app-owned retained-folder hints and a complete localized review/preview workflow still need actual provider/phone tests. Donor custom server file-browser behavior remains useful for native/Web preference consumers; this Android adaptation is not whole-app picker parity.
- Omitted/replaced code: `Current FileLocationSelect location/drive/network-root selector and disabled-while-loading navigation`. Disposition: **pending**. Reason: Current donor was completely reread. The new FileLocationSelect dependency is not yet fully reviewed/imported for native/Web consumers. Android SAF provides provider-owned navigation but does not establish parity with the donor server's filesystem location contract.
- Test-code evidence: `mobile/test/documents_test.dart:38 [UTF8-LF sha256=641aee7d7a005c5c309683cf98154f571c1e3c398dd5674fc68b020807807df0; anchor=Changed owner while choosing export]`.
- Test-code evidence: `mobile/test/firmware_test.dart:249 [UTF8-LF sha256=aa539cf52ac6e02285dfb4de32034d5eadd44d8a3d5ed66714071a5f66710f1f; anchor=Lost mutating GET probe]`.
- Test-code evidence: `mobile/test/firmware_page_test.dart:164 [UTF8-LF sha256=a8f8dcbddecdb44e81142714c1f7874dbd99de08961b516cc3b81ab4970c885a; anchor=Cancelled file selection]`.
- Test-code evidence: `mobile/test/capture_test.dart:186 [UTF8-LF sha256=c62fe3c9f6e73a0f4e0fae84925332c5dde99822f6e5680e95dc8c5dcb0cb06c; anchor=Firmware capture must never issue a device mutation]`.
- Remaining: Verify Android provider cancellation/rotation/process death and real import/export; actual board update/readback/power-loss acceptance and production APK signing remain open. Generic donor file-browser consumers and full locale/direction still require review. Shared manual AVR staging has no advertised hash/CAS; pinned remote job is preferred and manual limitation is explicit.

### Pealayer: `web_ui/src/i18n.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `331d2545379e36ea717fcf2876508315437bc5abe49923170f50a4f1bb1b47fc`.

- Target: `assets/web/localization.js:19 [UTF8-LF sha256=75db2917045580ed8dfd63eb2ecd42e54511b33e337f9855e50def8742259e6e; anchor=function uiTr(text,values)]`.
- Target: `tools/generate-font-assets.mjs:43 [UTF8-LF sha256=4c283d08fdf37cca888c768252b351b3ac4692c0ff0c6f8e7e436b52382102c5; anchor=const dictionary={}]`.
- Target: `firmware/esp8266/src/WebUi.h:614 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=// BEGIN SHARED LOCALIZATION]`.
- Retained: Fully reread all 630 current donor lines through EOF. Actual fa-dictionary fallback semantics remain reused for application copy, rather than translating arbitrary runtime data. Applicable native dictionary subset generates into the canonical Web module and actual firmware HTML; the new generic hardware/status/font/control vocabulary is useful pending consumer work, not an automatic dictionary import.
- Retained: Added system/explicit language and Auto/LTR/RTL preferences, owned-template text/placeholder/accessibility snapshots, per-browser low-write persistence, shared font route and immutable peer-name boundary.
- Retained: Persian Web regression slice: controller captions now include preferences, network/cloud/access/firmware and cell dialogs; capability mode labels participate in dictionary generation. Live telemetry and dynamic accessibility labels refresh in the selected language, real addresses release stale caption ownership, option values remain protocol-stable, and Persian text omits Latin letter spacing. Read-only actual HTML browser inspection verified mobile-width preferences/settings and reversible persistent locale switching; this is not live board acceptance.
- Omitted/replaced code: `Media-library, player, playback, cue/timeline and libmpv-specific Web dictionary/surfaces`. Disposition: **not-applicable**. Reason: Those surfaces belong to excluded media business and would add unrelated controls, not controller capabilities.
- Omitted/replaced code: `Remaining generic Web copy and native/Web/mobile preference synchronization`. Disposition: **pending**. Reason: The controller subset is integrated, not the entire donor Web feature set. Preferences/settings/cell captions and live status now have focused regression and read-only mobile-width browser evidence. Other dynamic errors, confirmations and advanced diagnostics, live-board and Android locale acceptance remain; browser locale is not silently written to another client or firmware.
- Test-code evidence: `tests/localization_test.mjs:45 [UTF8-LF sha256=f4daf6129230087ad05ba906b6bb777586dc10a432d395064adbd4f967bac0c5; anchor=No identical storage writes]`.
- Test-code evidence: `tests/localization_test.mjs:43 [UTF8-LF sha256=f4daf6129230087ad05ba906b6bb777586dc10a432d395064adbd4f967bac0c5; anchor=assert.equal(attrs.placeholder]`.
- Remaining: Finish remaining dynamic errors/confirmations/advanced diagnostic copy and controller-safe shared preference contracts; execute live-device and Android locale tests. Mobile-width actual Web fixture inspection is complete for preferences/settings, not hardware or exhaustive translation acceptance.

### Pealayer: `web_ui/src/preferenceFiles.ts`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `4580d1396b36f5a907cad6cf2f33a38486c3a7e6948a3bb222974e1b8a2a4af4`.

- Target: `mobile/lib/documents.dart:6 [UTF8-LF sha256=205c298afff227e61f4ddea3b071f65012e3ccc118cbd62f1de5dea8452eab1e; anchor=bool preferenceFileAllowed]`.
- Retained: Actual case-insensitive dot-extension suffix rule translated directly to Dart preferenceFileAllowed and used by the real Android firmware picker. Additional bounds/name and image-format checks do not treat filename as authenticity.
- Omitted/replaced code: `preferenceFileParent path regex`. Disposition: **replaced**. Reason: SAF content URIs are provider-managed identities, not slash-delimited filesystem paths. Android owns the directory picker; deriving a parent by string slicing would select the wrong resource. Host/server parent navigation remains represented in the ServerFilePicker pending row, not silently excluded.
- Test-code evidence: `mobile/test/documents_test.dart:8 [UTF8-LF sha256=641aee7d7a005c5c309683cf98154f571c1e3c398dd5674fc68b020807807df0; anchor=Actual donor extension rule is case-insensitive]`.
- Remaining: Physical Android document-provider verification is separate from exact helper equivalence.

### Pealayer: `web_ui/src/webPlatform.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `d6ee8217c05e96be2e20f9a40ab4c46249d57f58871c661a31c33e2310a92f0e`.

- Target: `tools/generate-branding.mjs:17 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor='FAVICON':]`.
- Target: `firmware/esp8266/src/WebUi.h:140 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=<!-- BEGIN SHARED BRAND FAVICON -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Same resolved artwork binds favicon/touch/header before login; ESP embeds data instead of requesting a protected host filesystem icon.
- Retained: Current 383-line donor was completely reread. Existing favicon/header/touch artwork mapping remains valid; this is not browser-platform parity.
- Omitted/replaced code: `storedBoolean/supportsStandaloneMode/useWebPlatform capabilities/events; install/share/toggleFullscreen/enableNotifications/applyUpdate; settings setters/signalInteraction`. Disposition: **pending**. Reason: Generic browser capabilities/lifecycle/preferences need controller-specific integration and tests; static branding does not certify them.
- Omitted/replaced code: `registerPealayerServiceWorker/update/controllerchange, wake-lock cancellation/visibility and offline badge`. Disposition: **pending**. Reason: PWA manifest/high-resolution artwork/cache ownership and streaming-specific wake-lock state remain open.
- Omitted/replaced code: `mediaTitle/current_video state icon resolution, MediaSession video metadata/play/pause/seek/chapter handlers`. Disposition: **not-applicable**. Reason: Video title/track/session business is excluded; generic browser capabilities are still pending.
- Omitted/replaced code: `Current app_icon_revision-qualified touch/manifest URL refresh, dynamic application-title metadata and explicit media-action callback retirement/position clearing`. Disposition: **pending**. Reason: Generic artwork revision/title refresh must be adapted to board-owned application identity and actual PWA resources. Retained stale media callbacks and position cleanup illustrate useful lifecycle patterns, not permission to expose playback actions on a controller. Generic event ownership must be tested in Pakar's real contexts.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### PU850-ESP: `.gitattributes`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d7fdbf0bc704830296234f2143889503714b9798941d52de7d8c4de593862fcc`.

- Target: `.gitattributes:1 [UTF8-LF sha256=613bba2c932de973fc1d5d117b0ea5d59d338fd0e9c9cd37b5b8afc37b05ec39; anchor=* text=auto eol=lf]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: cmd/bat/ps1 CRLF; sh/yml/yaml LF
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Donor script-specific CRLF policy`. Disposition: **replaced**. Reason: Current LF text/binary rules work with the actual cross-platform interpreters; no CRLF runtime requirement.

### PU850-ESP: `.github/dependabot.yml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `f418e551baccfcd1b21210c0c45fd5d4e75de0906e4e9aef8977e1a6368b9265`.

- Target: `.github/dependabot.yml:3 [UTF8-LF sha256=136887ba77b56544d0bcd367a31d2daaa7637ecdb8b997d63df6cb552d6f08e8; anchor=package-ecosystem: github-actions]`.
- Retained: Read exact ecosystem/schedule/group settings and retain Pakar's existing pinned Action/Cargo/pub maintenance. Firmware registry pins use the new dedicated checker instead of pretending Dependabot understands Arduino board/library indices.
- Omitted/replaced code: `Unconditional latest-success/first-binary auto-flash and donor-specific test/product packaging`. Disposition: **replaced**. Reason: Use exact successful main source/run/artifact identity and existing Pakar jobs. Automatic writes require an explicitly authorized runner/environment and repository opt-in.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Execute actual new GitHub jobs; verify bot policy and authorized runner/environment before activation.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.github/workflows/build.yml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `49d608184ca52242cc845026d05384081669eec57007733f43ede09539c79cb6`.

- Target: `.github/workflows/build.yml:102 [UTF8-LF sha256=b352090f2790ff875580ed2b985667accb7a6dcc1e9a675c8c551f430438e2f1; anchor=name: "🔧 ESP32-S3 + ESP8266 + AVR"]`.
- Target: `tools/firmware-report.mjs:40 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function artifactMarkdown]`.
- Retained: Adapt actual artifact size/checksum/download summary and staged build workflow mechanisms into existing Pealayer-derived build.yml plus firmware-report.mjs. Retain separate matched firmware outputs and draft-release provenance; no external progress image or rewritten timestamp header.
- Omitted/replaced code: `Unconditional latest-success/first-binary auto-flash and donor-specific test/product packaging`. Disposition: **replaced**. Reason: Use exact successful main source/run/artifact identity and existing Pakar jobs. Automatic writes require an explicitly authorized runner/environment and repository opt-in.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Execute actual new GitHub jobs; verify bot policy and authorized runner/environment before activation.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.github/workflows/tooling.yml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `06b31df1731e5ab45fb0f1c714f95c46470cf1006f6c50150dc9a461d95a52d0`.

- Target: `.github/workflows/build.yml:70 [UTF8-LF sha256=b352090f2790ff875580ed2b985667accb7a6dcc1e9a675c8c551f430438e2f1; anchor=name: "🧰 Scripts · ${{ matrix.os }}"]`.
- Target: `.github/actionlint.yaml:3 [UTF8-LF sha256=5b3c6b07ca38aaadb14a1d6eb2d35ac83b705f065901fdafba8210afc9be4960; anchor=pakar-deploy]`.
- Retained: Adapt actual tooling verification intent into Windows/Linux/macOS fixture/plan matrix before product jobs. Use one Node/PowerShell owner with explicit Windows-only utility boundaries; no donor-client build/test execution is falsely certified.
- Omitted/replaced code: `Unconditional latest-success/first-binary auto-flash and donor-specific test/product packaging`. Disposition: **replaced**. Reason: Use exact successful main source/run/artifact identity and existing Pakar jobs. Automatic writes require an explicitly authorized runner/environment and repository opt-in.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Execute actual new GitHub jobs; verify bot policy and authorized runner/environment before activation.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.github/workflows/update-dependencies.yml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `b157e47dc961e437bdf4aa22c1f95a31f38a99cff01b6544edff4563c011e16a`.

- Target: `.github/workflows/update-dependencies.yml:2 [UTF8-LF sha256=deaf864e3f11a7abb6f6c85c2958fef8deb6800338eb389ffd46504473951fca; anchor=name: "📦 Firmware dependency updates"]`.
- Target: `tools/firmware-dependencies.mjs:77 [UTF8-LF sha256=903bd1fdd6e50218cab22db4fc21fb64aee44da2b025faf329fcee4703506ab1; anchor=export function applyUpdates]`.
- Retained: Adapt scheduled registry check and guarded pin-change PR proposal into update-dependencies.yml. Report-only default, exact locked pin file, draft/no-auto-merge, pinned Actions and no device or SDK writes.
- Omitted/replaced code: `Unconditional latest-success/first-binary auto-flash and donor-specific test/product packaging`. Disposition: **replaced**. Reason: Use exact successful main source/run/artifact identity and existing Pakar jobs. Automatic writes require an explicitly authorized runner/environment and repository opt-in.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Execute actual new GitHub jobs; verify bot policy and authorized runner/environment before activation.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.github/workflows/upload.yml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `478acaff966c65549cd691c160dee24a635136b87f082b0cd3477710bf3ca8b7`.

- Target: `.github/workflows/upload.yml:2 [UTF8-LF sha256=aa6b4a47d77ea9890aea6099deb6580e55a08c5c5fe2872bb999c5b7e59179a5; anchor=name: "📡 Verified firmware deployment"]`.
- Target: `tools/deploy-artifact.mjs:8 [UTF8-LF sha256=0710b78d5359b0d6ece4dd48f9129acebe1809c370ed4bb6b6767fd4917bf55d; anchor=export function trustedBuild]`.
- Retained: Adapt completed-build artifact-to-device flow into opt-in upload.yml/deploy-artifact.mjs. Verify exact repository/head/event/main workflow/run/SHA plus clean artifact report/hash before invoking the same current updater; no guessed latest/first binary.
- Omitted/replaced code: `Unconditional latest-success/first-binary auto-flash and donor-specific test/product packaging`. Disposition: **replaced**. Reason: Use exact successful main source/run/artifact identity and existing Pakar jobs. Automatic writes require an explicitly authorized runner/environment and repository opt-in.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Execute actual new GitHub jobs; verify bot policy and authorized runner/environment before activation.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.gitignore`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `4e8df980915336db111442a9099dfefd1e01287b7925073f804e1726012e2059`.

- Target: `.gitignore:10 [UTF8-LF sha256=9bb316be389a528f2ed74afc2ab6c63e4cb33a30364b21a00c074af12df9262f; anchor=~secrets.h]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: logs/prerequisites/objects/link/PCH/library/executable/debug/kernel/IDE/build/SDK/private-header/Node/uploader/workflow rules
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Donor product uploader output directories`. Disposition: **replaced**. Reason: Current target build/target and service node_modules exclusions apply; actual ~local.h/~secrets.h patterns are imported.

### PU850-ESP: `.vscode/arduino-cli.yaml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `862e8979a82f1ecf8226b1762e72c265b15bb126275db3fabb8563c7473e6320`.

- Target: `tools/firmware-dependencies.json:2 [UTF8-LF sha256=4a7dc6ef881b1100856419608f5eceaf5c748a7a890c1834fa48250985efe3c1; anchor="platforms"]`.
- Target: `tools/firmware-dependencies.mjs:34 [UTF8-LF sha256=903bd1fdd6e50218cab22db4fc21fb64aee44da2b025faf329fcee4703506ab1; anchor=export function installationPlan]`.
- Retained: Retain official ESP8266 registry URL and extend it in one central pin file for ESP32-S3/MiniCore. Explicit install adds only missing configured registry URLs, never initializes/replaces operator SDK configuration.
- Omitted/replaced code: `PU-specific board/profile and separate dependency/config write paths`. Disposition: **replaced**. Reason: Reuse applicable exact versions/official registries in one reviewed pin owner. Preserve operator config and Pakar hardware flags; no silent downgrade, duplicate Arduino_JSON API, unsupported library additions or automatic SDK initialization.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run live registry check and review any candidate version changes with full build/hardware acceptance; pins were not upgraded by this slice.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.vscode/arduino.json`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d350ffb36676dca266ae2bb3eca20620f9eabca922ff7d5d271ba8617c3acca4`.

- Target: `.vscode/tasks.json:5 [UTF8-LF sha256=2681847c279d80dc8b8f0683feb126746498f73e1cd2eed4b9e9071c0ebf846d; anchor=Pakar: firmware plan]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: configuration/board/sketch/port/output/prebuild/postbuild/buildPreferences
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Old host/profile/postbuild commands`. Disposition: **pending**. Reason: Actual shared wrapper/tasks preserve profiles/cache gates; no COM13, disabled exception/stack guards, forced gzip/clean or deletion. Current compiler-database/editor execution remains pending.
- Remaining: Actual shared wrapper/tasks preserve profiles/cache gates; no COM13, disabled exception/stack guards, forced gzip/clean or deletion. Current compiler-database/editor execution remains pending.

### PU850-ESP: `.vscode/c_cpp_properties.json`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `264b32b11c64efad4d8198700184d40806e8ec7a60ad7e41c0aab9f91b48d661`.

- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: version/configurations; compilerPath/compilerArgs/intelliSenseMode; every includePath/forcedInclude/cStandard/cppStandard/exported define
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Current compilation database and S3 IntelliSense`. Disposition: **pending**. Reason: Full 19KB blob read; donor hardcodes host SDK/ABI and contradictory compiler macros. Derive actual current compile flags/database before claiming this generic capability.
- Remaining: Full 19KB blob read; donor hardcodes host SDK/ABI and contradictory compiler macros. Derive actual current compile flags/database before claiming this generic capability.

### PU850-ESP: `.vscode/prebuild.cmd`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c4473c19a749ee6e70d5f321959524162806e7a2456175966ccf3ae918a14afc`.

- Target: `tools/firmware-report.mjs:50 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=const source_sha=execFileSync]`.
- Retained: Adapt actual editor preflight entrypoint to the shared Windows launcher and build wrapper. The same target/link flags and generated-file checks now apply to editor, local CLI and CI calls.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.vscode/prebuild.sh`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d2c1857bd842c3219b50a858b4a6331afdc9018eadc7c610c7bf1e87f9ae076a`.

- Target: `tools/firmware-report.mjs:50 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=const source_sha=execFileSync]`.
- Retained: Adapt actual shell preflight/delegation into pakar.sh/build.sh and the same generated-asset/donor build guards used on Windows. No second editor-only compiler path.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `.vscode/settings.json`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `416ebf8eacc1ee229f1f76cd2830b58520e0c44d04977bfab3cfb474cc0e32db`.

- Target: `.vscode/settings.json:5 [UTF8-LF sha256=2373a0d9d9a7af54872b0ae854ccf1ff95116ef8f80aebfad513fe3031914d5f; anchor="*.ino": "cpp"]`.
- Target: `.vscode/tasks.json:5 [UTF8-LF sha256=2681847c279d80dc8b8f0683feb126746498f73e1cd2eed4b9e9071c0ebf846d; anchor=Pakar: firmware plan]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: every editor indentation/folding/default language/spelling/association/exclude/search/error/formatting setting
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Full current editor integration/acceptance`. Disposition: **pending**. Reason: Actual relevant associations/vocabulary/generated-path filters imported. Preserve personal indentation/theme/format settings; broad dotfile/assembly/hex hiding would conceal current source/policy. Actual editor acceptance remains open.
- Remaining: Actual relevant associations/vocabulary/generated-path filters imported. Preserve personal indentation/theme/format settings; broad dotfile/assembly/hex hiding would conceal current source/policy. Actual editor acceptance remains open.

### PU850-ESP: `~secrets.h.example`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `46ffefb855ae7f6c22d6b78ea7cebffe627a0ddae89cbf7918b4c80dcc9cda2a`.

- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: pragma, provisioning comments and token placeholder
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Separate compile-time Bearer credential owner`. Disposition: **replaced**. Reason: Use existing physical-USB AccessConfig and environment-only client credentials, no additional bearer bypass or copied placeholder. Ignore patterns are imported.

### PU850-ESP: `ASA0002E.code-workspace`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d90e55ab89ecdc3052d1a5863472e6b6524d343ddbda4ff4e15b95b88ab256b4`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: folder declaration; every files.associations entry; cSpell words
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Editor associations and vocabulary`. Disposition: **pending**. Reason: Import applicable Pakar associations/shared build ergonomics in a later editor slice; no foreign machine paths.
- Remaining: Import applicable Pakar associations/shared build ergonomics in a later editor slice; no foreign machine paths.

### PU850-ESP: `ASA0002E.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `773bfaf35ae5099f4676653fbf0ed326b508e1107828ac72337e0b0f2db8a93b`.

- Target: `libraries/BoardCore/src/DiagnosticText.h:11 [UTF8-LF sha256=70fc9b47f889046b09b97e70bc07c24e5a9c3e7b1fc4e3c0e4626c03ce63afb5; anchor=inline bool encodeHex(]`.
- Target: `firmware/esp8266/src/WebAccess.h:292 [UTF8-LF sha256=7c87b0f4e3f4cc849da343e7da39fa946ee68716a7181b0a2ee1241cb4579cab; anchor=BoardCore::DiagnosticText::encodeHex(bytes]`.
- Target: `firmware/esp8266/src/MaintenanceRoutes.h:192 [UTF8-LF sha256=9f486926615fdec3d6e7c6ad32e384816e66811674aec2a9f6b34697233e83cf; anchor=e[F("uptime_text")]=uptimeText]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Actual convertToHexString/getFormattedUptime/humanReadableBytes loops/decomposition/scaling imported as fixed-buffer DiagnosticText and invoked by live session, /random and telemetry owners.
- Retained: Every section/function read through EOF: include/debug graph; flashSettings/state/callbacks/mode/count/range/hex macros; convertToHexString/parseHexString; reflectU32/calculateCRC32/calculateReflectedCRC32; calculateMD5/getRandomMD5/stringMD5; isValidHostName/urlEncode/getBasename/getFileExtension/extractFileName/startsWith/endsWith/startsWithIgnoreCase; isLeapYear/isValidDate/isValidTime/monthNames/dayNames/monthStringToInt/dayOfWeek; TimeStamp constructors/set/toEpoch/fromEpoch/adjusted/isValid/toString/PU_DateTime/parseDateTime overloads; getFormattedUptime/humanReadableBytes; compiler/debug/serial tail
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Remaining hex decode/checksum/hostname/URL/path/string/calendar utilities`. Disposition: **pending**. Reason: Every function decision is detailed in the foundation table. Generic consumers remain useful and pending; fix destination bounds, four-bit reflect loop, UTF-8/path issues, impossible-date validation, parse widths, conversion failures and millis rollover before reuse.
- Test-code evidence: `tests/diagnostic_text_test.cpp:8 [UTF8-LF sha256=7483fd51656227b54d835d9c9430a605c1d7828d44fc302bd76bec45341033f5; anchor=uint8_t all[256]]`.
- Remaining: Every function decision is detailed in the foundation table. Generic consumers remain useful and pending; fix destination bounds, four-bit reflect loop, UTF-8/path issues, impossible-date validation, parse widths, conversion failures and millis rollover before reuse.

### PU850-ESP: `ASA0002E.ino.globals.h`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `733c09461007e10e55de248dffd1eecf6e8e615b6d5be57de59cd00cb859444f`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: pragma once and early Arduino CLI options documentation; no declarations
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Empty product-named globals header`. Disposition: **replaced**. Reason: Current generated build identity and explicit build flags own options; the donor file has no implementation.

### PU850-ESP: `build.cmd`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a2361ded706871a3f67f25ef572cb73cdf025e98c838185fb1346f589f4b4f72`.

- Target: `build.cmd:3 [UTF8-LF sha256=be1a218e732b904841258bb25cd017e7422f51fab737c290992aca16a8f2afae; anchor=pakar.cmd]`.
- Target: `tools/pakar.mjs:35 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export function legacyBuildArguments]`.
- Target: `tools/script-console.mjs:14 [UTF8-LF sha256=2243be58e4dce21a7592865121e94ebdb64f21286f3333358c8259bb2bdedafb; anchor=export function createLogger]`.
- Target: `pakar.cmd:8 [UTF8-LF sha256=d09fcf080f3134ef52b4d286427b1dfe42897ca28ea18595cb89405829c7fa1d; anchor=tools\pakar.mjs]`.
- Retained: Retain actual PU argument/help/preflight/build-stage semantics behind a small Windows launcher. Firmware mode/target validation and diagnostics now share tools/pakar.mjs with Bash instead of maintaining a second compiler implementation.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `build.sh`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `56af585e0239c59a74c5ba580b00195c2131f96cdf88394e8c93e3824d4d48f8`.

- Target: `build.sh:5 [UTF8-LF sha256=ddc2c39d73af52a2a233fb6ba8c17c9e281c5ba095455e39c0b55b227c607985; anchor=pakar.sh]`.
- Target: `tools/pakar.mjs:35 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export function legacyBuildArguments]`.
- Target: `pakar.sh:8 [UTF8-LF sha256=0e5554e3fb7a614178d57a8a39f081862a1ac382e5e4d87e9806b56d4512c680; anchor=exec node tools/pakar.mjs]`.
- Retained: Retain PU mode/target preflight and explicit build stages through the same portable dispatcher and guarded compiler wrapper. Artifact size/hash and optional gzip reporting live in firmware-report.mjs instead of shell parsing.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `defines.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d04aedb14faa7caa7b47d438605ec82da2b8281ce62bc6cfd0a4dd8db0d715cf`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: Arduino; USE_LOCALH; ignored secret inclusion; ProductConfig; serial-shell alias; every Temp dependency
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic Temp network/file/date/session/UI/programmer contracts`. Disposition: **pending**. Reason: All four dependency blobs are fully read; reconcile actual UART functions and current owners without copying macro protocol, secret or serial alias.
- Remaining: All four dependency blobs are fully read; reconcile actual UART functions and current owners without copying macro protocol, secret or serial alias.

### PU850-ESP: `LocalLib/AsyncOTA.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `3d681d678b58e5e615448acddc52fd623914c8ec4a76f80f5bbb75cd34c3aaec`.

- Target: `firmware/esp32s3/src/OtaStage.h:9 [UTF8-LF sha256=38f573b619915ff3c09062d4198aef6d2cc4b44a25fb72ec187edb33a8f22f9b; anchor=class S3OtaStage]`.
- Target: `firmware/esp8266/src/RemoteFirmware.cpp:325 [UTF8-LF sha256=ad02224ab50dde4e3f34367003d9c9ddad3198722cb5aac7bcb51bd935f81f4a; anchor=bool RemoteFirmware::validateEspStage()]`.
- Target: `firmware/esp8266/src/MaintenanceRoutes.h:290 [UTF8-LF sha256=9f486926615fdec3d6e7c6ad32e384816e66811674aec2a9f6b34697233e83cf; anchor=if(plan.targets==RemotePolicy::Esp && !programmingLatched) return true;]`.
- Target: `firmware/esp8266/src/ArtifactDownload.h:37 [UTF8-LF sha256=407bf97d8731516b66c90424a47e181434fe3126be34dba675c0b70ea6382961; anchor=remoteFirmware.readStagedEsp]`.
- Target: `firmware/esp32s3/src/DnsCoreLock.cpp:13 [UTF8-LF sha256=5b4540c3bab1666c2b1da6b940f580b67a4dc66ce9e19ecddffcee9d08d30f2a; anchor=extern "C" void __wrap_dns_clear_cache]`.
- Target: `firmware/esp8266/src/AsyncHttpService.h:123 [UTF8-LF sha256=4adbe46485571c92c0061e008ef3044d71a580f7f62d3cb1a1b3c19117eee40b; anchor=AsyncURIMatcher::exact(path)]`.
- Target: `firmware/esp32s3/src/DnsRecovery.h:13 [UTF8-LF sha256=dd0edc8470b2d8073176e8328bae8bba5b3be84137381a6874f990f7bc511869; anchor=class DnsRecovery]`.
- Target: `firmware/esp8266/src/RemoteFirmware.h:107 [UTF8-LF sha256=9c959a74a7d59909f38c48844e191a089cf1849663e78caa4a3fafcaff6b2251; anchor=Declaration order keeps that client alive]`.
- Target: `firmware/esp32s3/src/LedFlashGuard.cpp:37 [UTF8-LF sha256=8e09da7e63706aa1927eaba068a7a74a32f651840d1ce597977242d67fc1ce1f; anchor=esp_err_t IRAM_ATTR __wrap_esp_partition_write(]`.
- Target: `firmware/esp8266/src/CloudServices.cpp:405 [UTF8-LF sha256=6ebf48bd97162f8b6da46e52a71024443327cf18cea558360a45fb0db4f14eb5; anchor=void CloudServices::handleCommand(]`.
- Target: `firmware/esp8266/src/CloudServices.cpp:284 [UTF8-LF sha256=6ebf48bd97162f8b6da46e52a71024443327cf18cea558360a45fb0db4f14eb5; anchor=bool CloudServices::publishMqtt(]`.
- Target: `firmware/esp8266/src/CloudManagement.h:86 [UTF8-LF sha256=ce2aa666dc0a6856ab32d43104f8f3c921e168d3bdcb84704e9a0c82f23f81bb; anchor=bool updateCloudConfiguration(]`.
- Target: `firmware/esp8266/src/NetworkPath.h:56 [UTF8-LF sha256=cab7eb5a6e7985dfb8e5120b19991b9d434bcb4f083d5509678636b5adbda662; anchor=inline bool service()]`.
- Target: `libraries/BoardCore/src/CloudCommandGuard.h:33 [UTF8-LF sha256=2259897f8275dbd6eb7e0254b00dfc589e88c9af35dc7d2267cd749f4cb12a3d; anchor=class Guard]`.
- Target: `firmware/esp8266/src/CommandValues.h:8 [UTF8-LF sha256=5ab14e5c77b2c6eb2182592fcdf32526b50bef2a3b6904fb1e8aff2fd2b7b5de; anchor=namespace PakarCommand]`.
- Target: `firmware/esp8266/src/EthernetConfig.h:11 [UTF8-LF sha256=f8ef3f89121954e8bc553f9b7ade608f60e9acc66fc87d88f438fb0d197645cd; anchor=class EthernetConfigStore]`.
- Target: `firmware/esp8266/src/EthernetManagement.h:70 [UTF8-LF sha256=3e8ad92323184f1cbf1a6fb0a0d0c873ee46a7be710dc638fe82ada3362d45bf; anchor=bool updateEthernetConfiguration(]`.
- Target: `firmware/esp32s3/src/EthernetPort.cpp:93 [UTF8-LF sha256=b2ef56e69ae00a5d74c6d1fb90b7cd1dbdcaf2757f440b05f52abc28938c8a0a; anchor=bool EthernetPort::ensureInitialized(]`.
- Target: `firmware/esp8266/src/EthernetManagement.h:3 [UTF8-LF sha256=3e8ad92323184f1cbf1a6fb0a0d0c873ee46a7be710dc638fe82ada3362d45bf; anchor=String ethernetStatusJson(bool compact=false) {]`.
- Target: `tools/check-ethernet-live.mjs:61 [UTF8-LF sha256=33e72a4a0a29bbbaba321485b5c98bb1936e2c1e2f58fed1e66d22e2a83905c1; anchor=export async function checkEthernet(]`.
- Target: `firmware/esp8266/src/CloudServices.cpp:256 [UTF8-LF sha256=6ebf48bd97162f8b6da46e52a71024443327cf18cea558360a45fb0db4f14eb5; anchor=const uint32_t completedAt=millis()]`.
- Target: `firmware/esp8266/src/FilesystemTelemetry.h:31 [UTF8-LF sha256=6e0cdf19a541d7b60eb682729f5cc4436cde5d8ce3dfd2322077708dee3f0d9a; anchor=class Observation]`.
- Retained: Actual full AsyncOTA.cpp/header and FirmwareTransfer.cpp/header read through EOF. Retains the donor's authenticated chunked application-image OTA and explicit finalization/reboot workflow in the existing integrated /update handler; this is not an additional programmer firmware.
- Retained: S3-specific remote download stages directly into the inactive OTA partition because the identified image exceeds LittleFS capacity. Bounded1024-byte writes, sequential erase, exact transfer length/SHA, SDK image validation, explicit journaled boot selection, persisted-stage revalidation and actual running-image hash checks replace ESP8266-only FS staging. Quiet ESP-only preparation/finish does not send AVR commands; integrated builds retain paired Prog/backup/version gates.
- Retained: The publisher now accepts a raw hashed S3 application and buffered profile with optional AVR component. Ready S3 stages retain bounded download/Range support and block competing local OTA. Firmware-download response bounds are captured once, removing repeated whole-image verification from the S3 TCP chunk callback.
- Retained: Actual S3 crash dump traced the Internet stage failure to Arduino NetworkManager DNS cache clearing outside the lwIP core lock, invoking an SNTP UDP callback. A project-only linker adapter locks just cache clearing, respects existing TCPIP/lock ownership, and leaves DNS/TLS/SNI/CA verification unchanged; no SDK file, broad socket lock or insecure/IP-only fallback is used. This is a platform fix, not code present in the ESP8266-only PU donor.
- Retained: Standalone Internet adaptation removes AVR dependence from unlatched ESP-only prepare/finish in integrated builds, retains fresh image/SHA/boot checks and every paired backup/Prog gate, and serializes deferred cloud/network/Ethernet settings with update admission. Existing MQTT/WSS sessions carry cheap job progress/cancel during staging without new TLS handshakes or full-image telemetry captures. Cloud status streams bounded chunks without increasing RX buffers; top-level typed commands, board/boot/deadline and bounded cross-channel replay admission precede dispatch. These are modern Pakar additions, not claims that the PU donor had these security contracts.
- Retained: Reconsulted complete donor for optional W5500. Added explicit recovery after initialization failure, without resetting a healthy driver on cable loss or retrying in the service loop. Cheap status distinguishes configured/initialized/carrier/IP with null disconnected addresses. These native ETH/lwIP additions preserve the existing shared OTA/network owner, not donor W5500 functionality.
- Retained: Shared local S3/8266 upload callbacks now publish atomic finite start/commit/failure intent; only the loop sends UART, without audio ACK waits or fallback replay. Remote job notices are one-shot and permit a cooperative500ms lead before Prog. Audio delivery never authorizes erase or firmware success. Physical cue/phone acceptance remains open while ASUS SSH8022 is unavailable.
- Retained: Reread the actual donor updater for measured counters and completion callbacks. Adapted exact optional raw-image size headers with overflow/short-file rejection before boot commit, coherent atomic progress snapshots, read-only HTTP/WS progress during upload, boot-cached info without per-poll flash hashing, and loop-only finite TM1637/LCD progress. Added hostname-first auto local/remote tooling with auth-off pre/postflight, explicit stage/apply and full same-board new-boot image SHA readback; no unknown-write fallback.
- Retained: Failure-driven bounded hardening, not donor MQTT parity: PU has no reusable MQTT service. Pakar's actual ESP32 TCP connect limit is now independent of retained TLS/CA checks, MQTT retry starts10s AFTER connection completion, and wrap-safe measured attempt/failure/last/max counters are exposed additively. DNS remains synchronous; no claim of a fully nonblocking cloud worker. Existing host fixtures cover20s failure/backoff/wrap, not actual broker/TLS timing. Automatic updater diagnostics now identify read stage/path and transport code without publishing query secrets; no new write retries.
- Retained: Actual ASUS ten-minute failures led to a further scoped scheduler correction: cloud writes cooperate between256-byte payload fragments with strictly read-only USB servicing and nonrecursive/partial-packet guards; individual socket/DNS calls remain synchronous. DNS-only refresh updates future lookup configuration without retiring established TCP, while real interface/address/gateway changes still revoke sessions. Buffered AVR replies are drained after cloud work before peer expiry. Aggregate IO/close-cause and route/resolver counters distinguish causes; actual host fixtures cover write progress/reentry/short writes and Wi-Fi/Ethernet DNS-only transitions. These are source/fixture and separately recorded runtime observations, not missing PU MQTT code imported or full production acceptance.
- Retained: Periodic telemetry invoked four littlefs size traversals and LED flash-read holds. Boot-before-render diagnostic caching now exposes known/age/error while storage admission retains independent fresh checks. Explicit refresh refuses active LED/maintenance; no periodic scan or cached admission. This is a measured modern scheduler correction, not donor MQTT/W5500 parity.
- Omitted/replaced code: `ESP8266 raw FS linker symbols, filesystem OTA, global updater cancellation and watchdog disable/PU busy-status callbacks`. Disposition: **replaced**. Reason: S3 has named OTA partitions and esp_ota handles; global cancellation could affect another owner. Filesystem OTA would erase runtime network/auth/recovery data and remains disabled. PU-specific UART callbacks do not match the AVR protocol, and the quiet profile must remain silent.
- Omitted/replaced code: `Robust physical LAN OTA, Internet fetch/apply, interrupted-power recovery, boot rollback and concurrency/soak matrix`. Disposition: **pending**. Reason: Actual corrected multipart LAN upload passed. Three CRC-validated crash/backtrace diagnoses led to scoped DNS core-lock, HTTPClient socket-lifetime and cross-core callback-publication fixes, with actual implementation fixtures. Internet job7 subsequently staged a changed1853328-byte image, then the installed ASUS native CLI observed exact Ready identity/hash/length and received positive separate Apply acknowledgment. New boot13acc69e049a90569d631f1bc7950281 reports Complete, SHA29fca337..., matching running MD5 and selected/running app0 VALID. Same-boot short checks observe AP/station, automatic NTP and all480 white buffer pixels. Earlier truncated/empty HTTP reads remain failed evidence; this does not establish robust export, power-loss/rollback, optical or concurrent-client soak acceptance. No AVR write/acceptance.
- Omitted/replaced code: `All-client progress/outcome persistence, bounded DNS/TLS opening and complete staged export concurrency`. Disposition: **pending**. Reason: Existing native IPC acceptance did not expose a durable dispatch outcome; HTTP synchronous connect/DNS still has SDK-bound blocking. Range/lease integration has static/SDK checks but still needs live slow-client contention and credential/owner isolation testing.
- Test-code evidence: `tests/remote_s3_test.cpp:125 [UTF8-LF sha256=022b23a8102d9750d319d0e8f9c77dd558ab3fe647d1d9f196573bdf602c09e9; anchor=Actual S3 RemoteFirmware]`.
- Test-code evidence: `tests/remote_tools_test.mjs:11 [UTF8-LF sha256=4a38cbf8a24751720b35f940e894a64bcd1689b1402972feeb69fa079603e760; anchor=const solo=manifest]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:26 [UTF8-LF sha256=538f7c02dbd9184c0e500ea65bb14f7be924ef98d8f5a88b0e26c58938be534a; anchor=const flashRead =]`.
- Test-code evidence: `tests/s3_dns_lock_test.cpp:12 [UTF8-LF sha256=0c28b63dfedb4224d1ba1fde2f9ac98179515b2fdc8858fa25282b882315dc50; anchor=reset();nested=true;dns_clear_cache();]`.
- Test-code evidence: `tests/async_http_test.cpp:43 [UTF8-LF sha256=aa31ba27f15b74ba5cd1f61133bc261776c09637b22a4f7d6365b9208867fc97; anchor=Reproduce installed SDK3.12.1]`.
- Test-code evidence: `tests/access_stubs/ESPAsyncWebServer.h:105 [UTF8-LF sha256=0d11c69100d4101b8e0f26997e71760077b212d30827ddbbfcff222b4649b120; anchor=if(onPause)onPause();]`.
- Test-code evidence: `tests/s3_dns_recovery_test.cpp:23 [UTF8-LF sha256=c41d7526e9983da068a7c23db8dcad9cf0b4589b4f94ca1ad08d939e56a1fa8a; anchor=DnsRecovery promote;]`.
- Test-code evidence: `tests/remote_stubs/ESP8266HTTPClient.h:22 [UTF8-LF sha256=b9e28f67f69f78a8e29c84c7f00e578de287c4e6d51a250bda9164fa4d6f9796; anchor=Actual S3 end() retains _client]`.
- Test-code evidence: `tests/remote_stubs/ESP8266WiFi.h:8 [UTF8-LF sha256=ffc45f3a106c75d848cd5e059e428fa2ae5b671021eee197d4c95f3f082e5da1; anchor=inline static std::set<const WiFiClient*> live;]`.
- Test-code evidence: `tests/s3_flash_guard_test.cpp:12 [UTF8-LF sha256=64f42d3285941d68b5692c45a5c2ecb7f3357b633e5fcf6e4014d356b9da3334; anchor=namespace FlashGuardFixture]`.
- Test-code evidence: `tests/esp_only_maintenance_test.mjs:73 [UTF8-LF sha256=723d50c3f84c31206afe429e520f2e8c5bbdd03c8b925eded0e02ce4b5cdd784; anchor=ESP-only maintenance source guards passed]`.
- Test-code evidence: `tests/cloud_command_guard_test.cpp:17 [UTF8-LF sha256=2de44ca916fe1670eba11edeb706e88dfdafc4994d0a9d07301a279ef71c0581; anchor=int main()]`.
- Test-code evidence: `tests/cloud_services_test.cpp:65 [UTF8-LF sha256=902ba004bd9158f651211337094ef4a2c42b55e769f85429e3973ab849b7f515; anchor=int main()]`.
- Test-code evidence: `tests/cloud_config_test.cpp:46 [UTF8-LF sha256=ee28a50d8eab647040d30e24fb34cb61249ad7ba443ecbbcd7c4f7aba696d7bc; anchor=int main()]`.
- Test-code evidence: `tests/cloud_management_test.cpp:48 [UTF8-LF sha256=9f7af77a1b1e1218ba169c215f99ecf9b72ba2d48ca4e72e798706f9e979cf35; anchor=int main()]`.
- Test-code evidence: `tests/command_values_test.cpp:17 [UTF8-LF sha256=99f8a78b05de3080ab8bfc32ce623094a13b1f807f4f43a987210de1e05375bc; anchor=int main()]`.
- Test-code evidence: `tests/ethernet_config_test.cpp:19 [UTF8-LF sha256=488e7b462188564e5721c4f617a2cd4fa2782b41fea810a92b82095dd069d0bf; anchor=int main()]`.
- Test-code evidence: `tests/ethernet_management_test.cpp:67 [UTF8-LF sha256=c1d6ede0956e7ff1e1b43e5a0e15b4819f84d33afc7a6d8e83ed6fda8fa942a6; anchor=int main()]`.
- Test-code evidence: `tests/s3_ethernet_port_test.cpp:76 [UTF8-LF sha256=0985aaf25e5ec0d41ae3fcbbb6ef41766a8d2bd07bdb2e388e390b9721191452; anchor=No background retry/reset storm.]`.
- Test-code evidence: `tests/ethernet_management_test.cpp:76 [UTF8-LF sha256=c1d6ede0956e7ff1e1b43e5a0e15b4819f84d33afc7a6d8e83ed6fda8fa942a6; anchor=auto offline=snapshot(ethernetStatusJson());]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Test-code evidence: `tests/cloud_services_test.cpp:159 [UTF8-LF sha256=902ba004bd9158f651211337094ef4a2c42b55e769f85429e3973ab849b7f515; anchor=A slow failed connect must wait a full interval AFTER completion]`.
- Test-code evidence: `tests/filesystem_telemetry_test.cpp:17 [UTF8-LF sha256=03ecd7136734db8e599edda4fc62ef61bc651563c967abd138027d1ff58f2fc6; anchor=Thousands of ordinary telemetry snapshots]`.
- Test-code evidence: `tests/filesystem_telemetry_test.mjs:19 [UTF8-LF sha256=80c2d691da0170ac9c4993eeeb28209213ea7d24a1903bece882c20cd70fc969; anchor=cachedTelemetry(routes,main)]`.
- Scoped advantage: Explicit inactive-slot staging with independent apply, instead of an image that cannot fit its filesystem or an upload that chooses boot before separate consent.
- Remaining: Changed-image Internet stage/separate native CLI Apply/new-boot VALID, automatic NTP, complete1853328-byte boot-fenced running SHA and ASUS GUI exclusive hostname Wi-Fi online readback pass for the quiet S3 profile; final image was not USB-flashed. Complete raw export needed29 exact206 ranges and four read retries, not reliable single-stream/client resume. Finish intermittent empty/partial HTTP diagnosis, discovery/AP HTTP recovery, external-power/optical and OTA concurrency/abort/power-loss acceptance. Verify cloud commands without the ASUS relay independently. Preserve distinct USB/LAN/Internet proof and keep AVR, donor-wide parity and production issue2 acceptance open.
- Remaining: Latest standalone/ETH integration still requires actual ESP MQTT/WSS broker connectivity, authenticated remote command/telemetry/configuration, absent-AVR Internet image stage/apply/reboot, unplugged-PC recovery and W5500 DHCP/static/link-loss acceptance. Host/source fixtures and SDK builds do not establish those physical/Internet properties. Native/Web/mobile Ethernet editors now have ACK/readback fixtures and built client packages, but current installed-client and physical Ethernet acceptance remain pending.

### PU850-ESP: `LocalLib/AsyncOTA.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a05952f7ca3241aa840e024ff01560cea3d74226b003e2781efe95b4a533d388`.

- Target: `firmware/esp8266/esp8266.ino:318 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=std::atomic<uint32_t> otaRebootAt{0};]`.
- Target: `firmware/esp8266/src/AsyncHttpService.h:107 [UTF8-LF sha256=4adbe46485571c92c0061e008ef3044d71a580f7f62d3cb1a1b3c19117eee40b; anchor=explicit AsyncHttpService(uint16_t port) : native_(port) {}]`.
- Retained: Complete original header reread through EOF. Retains authenticated chunked application-image upload, explicit update completion/reboot and shared server integration through the adapted existing Pakar /update handlers. Native W5500 uses the same lwIP listener/updater and authorization policy; no parallel Ethernet-only firmware or server.
- Omitted/replaced code: `Global BusyOTA and InitAsyncOTA declarations / ESP8266 updater ownership`. Disposition: **replaced**. Reason: Pakar owns admission, upload lease, staged firmware and deferred reboot explicitly in its integrated main/MaintenanceRoutes. Reintroducing donor globals would create a second mutable owner and bypass paired programming/backup/peer fences. ESP32-S3 uses native OTA partitions, not ESP8266 raw linker/FS bounds.
- Test-code evidence: `tests/async_http_test.cpp:43 [UTF8-LF sha256=aa31ba27f15b74ba5cd1f61133bc261776c09637b22a4f7d6365b9208867fc97; anchor=Reproduce installed SDK3.12.1]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Scoped advantage: One native lwIP API/update owner across Wi-Fi and optional W5500, with bounded dispatch and truthful readiness.
- Remaining: Actual W5500 local POST OTA, auth/abort/concurrent-client behavior and separate reboot/running-image verification remain open. Host fixtures and SDK build do not establish live OTA or interrupted-power safety.

### PU850-ESP: `LocalLib/AsyncWebServer.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c2941d73d383ce9e5059d8b01b90063d0c41e58d52302541d0dcc3f1e3d719dd`.

- Target: `firmware/esp8266/src/AsyncHttpService.h:123 [UTF8-LF sha256=4adbe46485571c92c0061e008ef3044d71a580f7f62d3cb1a1b3c19117eee40b; anchor=AsyncURIMatcher::exact(path)]`.
- Target: `firmware/esp8266/src/LoopMessages.h:79 [UTF8-LF sha256=0539837793778de69839ea205fce43bda14f1734866dee5c158389f48e3837ab; anchor=Message *items_=nullptr;]`.
- Target: `firmware/esp8266/esp8266.ino:3934 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=loopMessages.begin(); // PSRAM-backed S3 queue]`.
- Target: `firmware/esp8266/src/MaintenanceRoutes.h:159 [UTF8-LF sha256=9f486926615fdec3d6e7c6ad32e384816e66811674aec2a9f6b34697233e83cf; anchor=internal_heap_largest_block_bytes]`.
- Target: `tools/update-firmware.mjs:154 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export function checkWebSocket(]`.
- Target: `firmware/esp8266/esp8266.ino:2211 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=void onWebSocketEvent(]`.
- Target: `firmware/esp32s3/src/EthernetPort.h:63 [UTF8-LF sha256=9504a3f71b4c0998277764d6c5f39f66e7b8c3b49cfb294fb5e5869138e1a930; anchor=SPIClass spi_{HSPI};]`.
- Retained: Complete original implementation read through EOF. Retains shared asynchronous server setup, /ws attachment, connected-client limits and per-client message dispatch. The actual AsyncTCP listener uses native lwIP interfaces, so Wi-Fi and W5500 reach identical routes. Existing Pakar bounded loop-owned dispatch and access ownership replace unsafe donor callback behavior. Reread for the exact-image S3 panic in WebAccess::bindSocket: reuse donor onWebSocketEvent's client->remoteIP(), passing its copied address into admission. Installed SDK _newClient transfers request->clientRelease() before CONNECT, leaving request headers valid but its TCP client null. The regression fixture explicitly releases that request client and checks retained peer address/version; no auth, origin or release-policy bypass.
- Retained: Updater WebSocket preflight now adapts actual desktop main.rs::ws_worker read-only status acquisition: exactly one masked {type:status} is sent only after validating101/Accept/Connection:Upgrade and the actual pinned TCP peer. Only exact-board/boot recognized status-rgb/update-progress events may be skipped; neither renews the bounded full-status deadline nor satisfies readiness. The complete status must still pass boardPeer identity/capability checks. Separate measured connect/upgrade and full-status stages allow bounded cloud contention without removing the preflight or replaying any action.
- Retained: The existing bounded callback-to-loop WebSocket queue now allocates its eight S3 complete-message slots once in explicit PSRAM before network callbacks. Allocation has no internal-DRAM fallback; ready() and ws_queue_ready expose actual queue availability, and missing storage rejects ingress rather than executing borrowed/partial data. ESP8266 retains four inline cooperative slots. Locks, message bounds/copy isolation, epochs and backpressure remain unchanged. This is a Pakar/S3 SDK memory adaptation, not a PSRAM feature attributed to the ESP8266 donor.
- Retained: Internal8-bit heap free/largest/minimum metrics are reported separately from total heap and free PSRAM, so a large external-memory value cannot conceal Wi-Fi/OTA internal-memory exhaustion. BLE buffers use a separate same-policy lifetime owner; static GPIO/RMT wire buffers, driver timing, task stacks, SDK host pools and production build flags are not moved or reconfigured by this queue change.
- Retained: Actual PU AsyncWebServer source and installed SDK3.12.1 WebResponses/WebRequest were reread for observed partial HTTP bodies. Project-owned immutable SnapshotResponse now accounts only actual add returns, retains zero/partial tails and failed sends, and waits for all admitted bytes to be acknowledged before completion. JSON owns one immutable payload; PROGMEM UI uses512-byte staging, not a whole heap copy. Existing mailbox/weak owner/actor-only attachment and headers remain. Real adapter fixtures reproduce the upstream final-fill truncation and cover partial final body, header, zero window, failed send, empty/HEAD,224KiB flash UI and disconnect. This SDK adaptation does not claim OTA ingress, WebSocket upgrade or separate firmware-export response fixes.
- Omitted/replaced code: `data[len] = 0 and direct onWsReceivedCommand callback on borrowed receive memory`. Disposition: **replaced**. Reason: There is no writable extra byte guarantee on the receive buffer. Pakar copies at most1300 bytes into bounded loop-owned messages, validates lengths/sequences and revokes socket admission before close; HTTP uses weak request mailboxes so lost connections cannot actuate dangling requests.
- Omitted/replaced code: `Ten-client limit and permissive handling of message/transport fragments`. Disposition: **replaced**. Reason: Pakar limits eight simultaneous sockets for finite S3/ESP8266 queue/RAM admission. It handles sequential transport chunks within one final frame and rejects oversized, fragmented-message or invalid-sequence frames explicitly instead of executing partial or unterminated commands.
- Omitted/replaced code: `Donor PU command globals and serial business dispatch`. Disposition: **replaced**. Reason: Pakar dispatches the shared board/protocol/cell/firmware contracts under current access and programming ownership. PU business logic and its UART path do not match this AVR/S3 board.
- Omitted/replaced code: `Implicit expectation that the first WebSocket text frame is an unsolicited full status`. Disposition: **replaced**. Reason: The integrated board/native worker supports explicit read-only status requests and can publish independent compact RGB/update events first. The updater now requests one status without credentials/control writes, validates every optional event identity before ignoring it, and refuses completion from partial/foreign/malformed frames. Both handshake and full-status waits remain finite and separately measured; retries of firmware or control messages are not added.
- Test-code evidence: `tests/led_scheduler_test.cpp:32 [UTF8-LF sha256=e3967d3e77a7073dc002bf476f1b628e022ac6c32cab87387da9dfb529d29a07; anchor=S3 queue must not retain internal bulk storage]`.
- Test-code evidence: `tests/ble_storage_stubs/esp_heap_caps.h:18 [UTF8-LF sha256=f6ff5f7843125cd7e780df39fd9d31006af9ef23070641ec580288235750365c; anchor=assert(capabilities==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))]`.
- Test-code evidence: `tests/update_tool_test.mjs:199 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=Exactly one read-only request, no replay]`.
- Test-code evidence: `tests/update_tool_test.mjs:210 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=Partial events never satisfy readiness or trigger request replay]`.
- Test-code evidence: `tests/async_http_test.cpp:43 [UTF8-LF sha256=aa31ba27f15b74ba5cd1f61133bc261776c09637b22a4f7d6365b9208867fc97; anchor=Reproduce installed SDK3.12.1]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Scoped advantage: One native lwIP API/update owner across Wi-Fi and optional W5500, with bounded dispatch and truthful readiness.
- Remaining: Physical W5500 HTTP/WebSocket, malformed frames, client-limit, reconnect/revocation and concurrent LED/OTA soak remain open. Native ETH driver ownership/recovery is a Pakar/SDK addition, not a feature attributed to the PU donor.
- Remaining: Measure actual S3 internal8-bit free/largest/minimum headroom after BLE and PSRAM queue startup, under HTTP/WebSocket/TLS/OTA/matrix contention and allocation failure. Source/host allocator fixtures do not establish internal-heap recovery, radio coexistence, OTA completion or optical timing.

### PU850-ESP: `LocalLib/AsyncWebServer.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `db79b8de84d8f874df348b0d68b1d2056f8945ad742f172be99ddb9b1d94e218`.

- Target: `firmware/esp8266/src/AsyncHttpService.h:107 [UTF8-LF sha256=4adbe46485571c92c0061e008ef3044d71a580f7f62d3cb1a1b3c19117eee40b; anchor=explicit AsyncHttpService(uint16_t port) : native_(port) {}]`.
- Target: `firmware/esp8266/esp8266.ino:175 [UTF8-LF sha256=a45fee5a8ebbc7cea6d7c84532f5e7217b6117427928796c47989fb14958f985; anchor=AsyncWebSocket webSocket("/ws");]`.
- Retained: Complete original header read through EOF. Shared HTTP80 and /ws WebSocket composition are retained in the actual Pakar server; the native Ethernet netif uses that same existing listener, API routes and authentication rather than new WIZnet sockets.
- Omitted/replaced code: `Public donor global server/ws symbols and InitWebServer declaration`. Disposition: **replaced**. Reason: The integrated project already owns one AsyncHttpService facade and one AsyncWebSocket. Hardware-affecting callbacks defer into the loop owner using weak request lifetimes; duplicate globals/init would introduce divergent lifecycles, auth and connection owners.
- Test-code evidence: `tests/async_http_test.cpp:43 [UTF8-LF sha256=aa31ba27f15b74ba5cd1f61133bc261776c09637b22a4f7d6365b9208867fc97; anchor=Reproduce installed SDK3.12.1]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Scoped advantage: One native lwIP API/update owner across Wi-Fi and optional W5500, with bounded dispatch and truthful readiness.
- Remaining: Real Ethernet HTTP/WebSocket serving and multi-client reconnect/access revocation remain unverified. The HTTP acceptance probe explicitly does not claim WebSocket or multicast acceptance.

### PU850-ESP: `LocalLib/Authentication.cpp`

Verdict: **scoped-improvement**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `7009bb58fb5bc686476c6a533284f2e62564442e35c4621922c19f145ce4c491`.

- Target: `firmware/esp8266/src/WebAccess.h:31 [UTF8-LF sha256=7c87b0f4e3f4cc849da343e7da39fa946ee68716a7181b0a2ee1241cb4579cab; anchor=class WebAccess]`.
- Target: `firmware/esp8266/src/AccessConfig.cpp:66 [UTF8-LF sha256=6ec60a32ab5e42943ea53844a5812acc86baf7232f3cf1090c3c8b591a81d2f7; anchor=bool AccessConfig::configure(]`.
- Target: `desktop/src/ui/connection_controls.rs:209 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=if authentication == Some(true)]`.
- Target: `firmware/esp8266/src/WebUi.h:1338 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function presentDeviceAccess(info)]`.
- Retained: Complete exact donor source read and live SHA-256 matched the inventoried revision. Retained middleware authorization, public login, credential validation and cookie/session admission concepts in the actual shared WebAccess implementation. Added saved explicit authentication opt-in, default-off Web/native behavior, verified atomic storage migration, bounded session/WS ownership, rate limiting, origin and peer/boot fencing.
- Omitted/replaced code: `complexAuth hardcoded user/password; doLogin password/token response and diagnostic dump; /pass PU access-level commands`. Disposition: **replaced**. Reason: Factory authentication is not an opt-in. Password/token dumps disclose secrets, and the PU access-level business commands do not describe this LED board. Use explicit saved access settings and non-disclosing session routes instead.
- Omitted/replaced code: `Duplicated run implementation, placeholder AuthenticationHandler/getAuthToken/session access-level skeleton and role=staff sample`. Disposition: **replaced**. Reason: The donor skeleton has unresolved identifiers and no complete role-based authorization contract. Imported the applicable route/session behavior into compiling middleware rather than enabling an incomplete authorization example. No implemented Pakar role support is claimed.
- Test-code evidence: `tests/access_config_test.cpp:8 [UTF8-LF sha256=94a3a2860c4edb316a3d24595311455b3b1b6544fbd4143f11c919549ad1b55c; anchor=assert(!config.enabled())]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:204 [UTF8-LF sha256=224df11ea7e8a2198e172337822b8d0843398e751458e754c69f0accf807a019; anchor=// Auth-off does not mean loss]`.
- Test-code evidence: `tests/web_access_test.mjs:94 [UTF8-LF sha256=83c06839b0cfb57d8660ad9e66600b7ad8b9a799bf70fdf42755997e3f78cd02; anchor=authEnabled=false;signedIn=false;]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:337 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=for authentication in [None, Some(false), Some(true)]]`.
- Scoped advantage: Executable auth-off/opt-in persistence and conditional Web/native credential UI, without weakening corruption recovery, origin or peer fencing.

### PU850-ESP: `LocalLib/Authentication.h`

Verdict: **scoped-improvement**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d63933cf51fb09ef2ce09052fd616b8d62ca422d36c713d7705b9fb8717f287e`.

- Target: `firmware/esp8266/src/WebAccess.h:31 [UTF8-LF sha256=7c87b0f4e3f4cc849da343e7da39fa946ee68716a7181b0a2ee1241cb4579cab; anchor=class WebAccess]`.
- Target: `firmware/esp8266/src/AccessConfig.cpp:66 [UTF8-LF sha256=6ec60a32ab5e42943ea53844a5812acc86baf7232f3cf1090c3c8b591a81d2f7; anchor=bool AccessConfig::configure(]`.
- Target: `desktop/src/ui/connection_controls.rs:209 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=if authentication == Some(true)]`.
- Target: `firmware/esp8266/src/WebUi.h:1338 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function presentDeviceAccess(info)]`.
- Retained: Complete exact donor source read and live SHA-256 matched the inventoried revision. Retained middleware authorization, public login, credential validation and cookie/session admission concepts in the actual shared WebAccess implementation. Added saved explicit authentication opt-in, default-off Web/native behavior, verified atomic storage migration, bounded session/WS ownership, rate limiting, origin and peer/boot fencing.
- Omitted/replaced code: `complexAuth hardcoded user/password; doLogin password/token response and diagnostic dump; /pass PU access-level commands`. Disposition: **replaced**. Reason: Factory authentication is not an opt-in. Password/token dumps disclose secrets, and the PU access-level business commands do not describe this LED board. Use explicit saved access settings and non-disclosing session routes instead.
- Omitted/replaced code: `Duplicated run implementation, placeholder AuthenticationHandler/getAuthToken/session access-level skeleton and role=staff sample`. Disposition: **replaced**. Reason: The donor skeleton has unresolved identifiers and no complete role-based authorization contract. Imported the applicable route/session behavior into compiling middleware rather than enabling an incomplete authorization example. No implemented Pakar role support is claimed.
- Test-code evidence: `tests/access_config_test.cpp:8 [UTF8-LF sha256=94a3a2860c4edb316a3d24595311455b3b1b6544fbd4143f11c919549ad1b55c; anchor=assert(!config.enabled())]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:204 [UTF8-LF sha256=224df11ea7e8a2198e172337822b8d0843398e751458e754c69f0accf807a019; anchor=// Auth-off does not mean loss]`.
- Test-code evidence: `tests/web_access_test.mjs:94 [UTF8-LF sha256=83c06839b0cfb57d8660ad9e66600b7ad8b9a799bf70fdf42755997e3f78cd02; anchor=authEnabled=false;signedIn=false;]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:337 [UTF8-LF sha256=58eaae178af71e86725a71df606b6ed6fb142c4fe852d3d5b8dce8b980ed3440; anchor=for authentication in [None, Some(false), Some(true)]]`.
- Scoped advantage: Executable auth-off/opt-in persistence and conditional Web/native credential UI, without weakening corruption recovery, origin or peer fencing.

### PU850-ESP: `LocalLib/Commands.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `5181d7cc83efe2416a3a198ff88aad2aeffc8871ee2ef7a2a1ffdad51d31b88e`.

- Target: `firmware/esp8266/src/ShellService.h:72 [UTF8-LF sha256=897ac8e49f95e1e2ebc8b59dbd1a76148d1fcaaecf609cf33d99d067fc849a0c; anchor=bool shellSet(]`.
- Target: `firmware/esp8266/src/ShellTuiBinding.h:21 [UTF8-LF sha256=6ba1c1b6da225954c10988a2269eee7dc5f422d6f59ac3c6453bc3d51e7f7138; anchor=shellGet(property)]`.
- Target: `firmware/esp8266/src/ShellSettings.h:19 [UTF8-LF sha256=edc36bb702f4304ee1eac6e2e848cf4f62b9d075bb9d6e90349029c768ae19e4; anchor=bool configure(]`.
- Target: `docs/PU850-PARITY.md:98 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Fresh runtime and shell section review, 2026-10-19]`.
- Retained: DeviceParameter name/type/read-only/get/set design is adapted to ShellProperty descriptors for hostname/label, network mode/DHCP/static addressing, station SSID/write-only password, MQTT/WebSocket endpoints, telemetry interval, shell settings and LED mode/brightness/effect.
- Retained: Read-only observations expose version, boot identity, uptime, heap, AVR presence, display ACK state, firmware phase and actual LED backend/RMT/DMA capability; no shell-specific LED engine or second configuration store is introduced.
- Retained: Useful echo/status/connect behavior maps to echo/status and queued network reconnect; typed JSON builds route device/cell/bulk/pixel/audio/display/firmware actions through existing validators and maintenance owners instead of PU callback addresses.
- Retained: Durable configuration owners apply validated updates rather than copying the donor manual Settings_Write/Settings_Read lifecycle or direct ESP8266 SDK network structures.
- Omitted/replaced code: `Station_Password/AP_Password getters returning plaintext credentials`. Disposition: **replaced**. Reason: Secret descriptors are write-only and display <write-only>/[hidden]; classified secret commands and URL credentials are excluded from history. Copying plaintext getters would disclose credentials over Telnet and framed USB.
- Omitted/replaced code: `busy/ready SendESPStatus_ToPU and Request_Beep PU protocol callbacks`. Disposition: **replaced**. Reason: Generic flow/busy/ready/audio intent is retained through existing AVR flow/maintenance ownership and finite typed cues, not excluded as business. Only legacy PU wire addresses and unowned byte-to-buzzer callbacks are replaced; physical timing/audio acceptance stays open.
- Omitted/replaced code: `Direct wifi_station/softap config mutation, AssignResult into the global command buffer`. Disposition: **replaced**. Reason: Shared typed JSON configuration owners validate and persist updates without truncating into the current command buffer or mutating ESP8266-only SDK structures. The generic descriptor/read-only mechanism itself is retained.
- Omitted/replaced code: `AP_SSID/AP_Password descriptors and legacy save/restore aliases`. Disposition: **pending**. Reason: AP credential editing and reviewed restore semantics remain useful gaps. Actual Firmware_Hash/Build_Date descriptor/getter code now serves read-only firmware.hash/build.date with focused production-prefix fixtures; this is SDK sketch MD5/compiler time, not full-image integrity or installation time.
- Omitted/replaced code: `Commented AP_IP/AP_Gateway/AP_Subnet parameter entries`. Disposition: **pending**. Reason: The donor leaves these entries commented out. Pakar station static-address fields are implemented, but independent configurable AP address/gateway/subnet via the shell is not claimed complete.
- Test-code evidence: `tests/shell_metadata_test.mjs:41 [UTF8-LF sha256=ac8c554e7adf8c858d7ca6e7ae4ca35cf9de785a96120869305b0547b328bc01; anchor=Actual read-only firmware hash/build date getters]`.
- Test-code evidence: `tests/shell_wire_test.cpp:29 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=4294967296]`.
- Test-code evidence: `tests/shell_tui_test.cpp:31 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=Readonly never emits Set.]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:71 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=!editor.remember(true)]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.
- Remaining: Review remaining AP/restore descriptor gaps separately; hash/build getters now imported, while generic get/set source coverage does not certify every prior PU property.

### PU850-ESP: `LocalLib/Commands.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `48c0deb76d6105090fe1d58a2449427681a88775b1e63ddbb1b28ecff6341961`.

- Target: `firmware/esp8266/src/ShellService.h:72 [UTF8-LF sha256=897ac8e49f95e1e2ebc8b59dbd1a76148d1fcaaecf609cf33d99d067fc849a0c; anchor=bool shellSet(]`.
- Target: `firmware/esp8266/src/ShellTuiBinding.h:21 [UTF8-LF sha256=6ba1c1b6da225954c10988a2269eee7dc5f422d6f59ac3c6453bc3d51e7f7138; anchor=shellGet(property)]`.
- Target: `firmware/esp8266/src/ShellSettings.h:19 [UTF8-LF sha256=edc36bb702f4304ee1eac6e2e848cf4f62b9d075bb9d6e90349029c768ae19e4; anchor=bool configure(]`.
- Target: `docs/PU850-PARITY.md:98 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Fresh runtime and shell section review, 2026-10-19]`.
- Retained: DeviceParameter name/type/read-only/get/set design is adapted to ShellProperty descriptors for hostname/label, network mode/DHCP/static addressing, station SSID/write-only password, MQTT/WebSocket endpoints, telemetry interval, shell settings and LED mode/brightness/effect.
- Retained: Read-only observations expose version, boot identity, uptime, heap, AVR presence, display ACK state, firmware phase and actual LED backend/RMT/DMA capability; no shell-specific LED engine or second configuration store is introduced.
- Retained: Useful echo/status/connect behavior maps to echo/status and queued network reconnect; typed JSON builds route device/cell/bulk/pixel/audio/display/firmware actions through existing validators and maintenance owners instead of PU callback addresses.
- Retained: Durable configuration owners apply validated updates rather than copying the donor manual Settings_Write/Settings_Read lifecycle or direct ESP8266 SDK network structures.
- Omitted/replaced code: `Station_Password/AP_Password getters returning plaintext credentials`. Disposition: **replaced**. Reason: Secret descriptors are write-only and display <write-only>/[hidden]; classified secret commands and URL credentials are excluded from history. Copying plaintext getters would disclose credentials over Telnet and framed USB.
- Omitted/replaced code: `busy/ready SendESPStatus_ToPU and Request_Beep PU protocol callbacks`. Disposition: **replaced**. Reason: Generic flow/busy/ready/audio intent is retained through existing AVR flow/maintenance ownership and finite typed cues, not excluded as business. Only legacy PU wire addresses and unowned byte-to-buzzer callbacks are replaced; physical timing/audio acceptance stays open.
- Omitted/replaced code: `Direct wifi_station/softap config mutation, AssignResult into the global command buffer`. Disposition: **replaced**. Reason: Shared typed JSON configuration owners validate and persist updates without truncating into the current command buffer or mutating ESP8266-only SDK structures. The generic descriptor/read-only mechanism itself is retained.
- Omitted/replaced code: `AP_SSID/AP_Password descriptors and legacy save/restore aliases`. Disposition: **pending**. Reason: AP credential editing and reviewed restore semantics remain useful gaps. Actual Firmware_Hash/Build_Date descriptor/getter code now serves read-only firmware.hash/build.date with focused production-prefix fixtures; this is SDK sketch MD5/compiler time, not full-image integrity or installation time.
- Omitted/replaced code: `Commented AP_IP/AP_Gateway/AP_Subnet parameter entries`. Disposition: **pending**. Reason: The donor leaves these entries commented out. Pakar station static-address fields are implemented, but independent configurable AP address/gateway/subnet via the shell is not claimed complete.
- Test-code evidence: `tests/shell_metadata_test.mjs:41 [UTF8-LF sha256=ac8c554e7adf8c858d7ca6e7ae4ca35cf9de785a96120869305b0547b328bc01; anchor=Actual read-only firmware hash/build date getters]`.
- Test-code evidence: `tests/shell_wire_test.cpp:29 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=4294967296]`.
- Test-code evidence: `tests/shell_tui_test.cpp:31 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=Readonly never emits Set.]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:71 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=!editor.remember(true)]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.
- Remaining: Review remaining AP/restore descriptor gaps separately; hash/build getters now imported, while generic get/set source coverage does not certify every prior PU property.

### PU850-ESP: `LocalLib/CrashHandler.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `be001f01f3467950ec27b578a16c391d4d680e1bb562e3d8c06fb422d28cc5c9`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: active crash_ets_uart_putc1; complete commented custom_crash_callback timestamp/reset/register offsets/counter/cursor/stack heuristic
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `commented crash persistence/stack-capture sketch`. Disposition: **pending**. Reason: No active donor persistence implementation exists. Design bounded secret-safe platform capture/rotation and fault proof; keep S3 SDK panic/reset evidence rather than importing ESP8266 suppression.
- Remaining: No active donor persistence implementation exists. Design bounded secret-safe platform capture/rotation and fault proof; keep S3 SDK panic/reset evidence rather than importing ESP8266 suppression.

### PU850-ESP: `LocalLib/CrashHandler.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d958445204e65686693ab766a3b090095585ed538c374139b7baf5f61c91785a`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: pragma/include; NO_CUT_HERE; crash printer declaration; commented custom callback declaration
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic durable crash capture contract`. Disposition: **pending**. Reason: Do not transplant ESP8266 callback structs/cont registers into S3; safe storage/rotation and fault acceptance remain open.
- Remaining: Do not transplant ESP8266 callback structs/cont registers into S3; safe storage/rotation and fault acceptance remain open.

### PU850-ESP: `LocalLib/FirmwareTransfer.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `1dada0f284ba4c81a6185f1aeb34dbd299752a7b48b0ba56336d1dfdc9e2ac3c`.

- Target: `firmware/esp8266/src/FirmwareTransfer.cpp:173 [UTF8-LF sha256=98dff31984a81dcfb769fb036664e711c4312b257baefbdcbbac92168799d4de; anchor=void RegisterFirmwareTransferRoutes(]`.
- Target: `firmware/esp8266/src/FirmwareReadWindow.h:11 [UTF8-LF sha256=d67a961994aefd8eb3c9df1b0e5cd7e04529097e707b41988f9e4e5c00e07a88; anchor=class ReadWindow]`.
- Target: `firmware/esp8266/src/FirmwareReadResponse.h:13 [UTF8-LF sha256=1671071b0a3fa1d036abec6289ba4f924f9ab52fc968b9f6b9c29c6f7d72cbfd; anchor=class ReadResponse final]`.
- Target: `firmware/esp8266/src/FirmwareRange.h:26 [UTF8-LF sha256=03bf18b46233e539a84563f1eb63330d2eaf8e57b9bb73b6cfd92026bda6bef8; anchor=inline bool parseRange(]`.
- Target: `firmware/esp8266/src/FirmwarePlatform.h:143 [UTF8-LF sha256=ad3e975a14f0f7b7d78446bf94217fbb2fc2770e984ad4979d7bcce142b75e8d; anchor=inline bool readRunning(]`.
- Target: `tools/check-ethernet-live.mjs:61 [UTF8-LF sha256=33e72a4a0a29bbbaba321485b5c98bb1936e2c1e2f58fed1e66d22e2a83905c1; anchor=export async function checkEthernet(]`.
- Target: `desktop/src/bridge_transfer.rs:54 [UTF8-LF sha256=fbb13202d86296f1766d0e20bcd9e4a520a8b13d4988890e5a5273a20a5469fe; anchor=pub(super) fn open]`.
- Retained: Complete actual donor read through EOF and current adapted production target reviewed. Imported GET/HEAD firmware download, bounded single byte ranges/open/suffix ranges, 416 responses, ETag/304, Content-MD5/Digest/identity headers and chunked flash reads; extracted parser exercises the production implementation.
- Retained: ESP8266 aligned flash implementation is retained. ESP32-S3 adaptation downloads only the running application partition using bounded esp_partition_read, uses mbedtls Base64 and platform model, and shares the same endpoint/HTTP engine instead of a second flasher firmware.
- Retained: Reconsulted complete donor for W5500 integration. Existing native ETH/lwIP shares the very same firmware HEAD/Range route; the hostname-first live probe checks actual TCP interface identity and exported image framing instead of assuming carrier means correct serving.
- Retained: Reconsulted full FirmwareTransfer implementation/header for native network repeater. Preserve opaque binary GET/HEAD/Range/ETag and actual multipart upload status instead of JSON reparsing. Bound export/body sizes, selected route/boot/trace ownership and no-replay outcomes; USB never silently opens Wi-Fi.
- Retained: Export recovery adapts the actual installed ESPAsyncWebServer3.12.1 response boundary: transient guarded flash-read timeout returns RESPONSE_TRY_AGAIN at the same offset, bounded by five failures or two seconds; persistent/fatal errors invalidate the response source so the SDK closes a failed transport instead of treating zero as successful EOF. Loop-owned admission retains a cross-actor-safe scoped reader lease through response destruction and accepts an immutable boot-verified identity snapshot rather than rehashing the full image per GET. This concrete source defect is repaired; the earlier observed live truncation's precise cause remains unmeasured.
- Omitted/replaced code: `Optional build-time FW_OTA_BEARER_TOKEN/secureEquals and WWW-Authenticate bearer realm`. Disposition: **replaced**. Reason: Shared Pakar WebAccess callback now owns authorization and defaults to deny without an authorization provider. Credentials/session policy must not diverge between firmware download and other device endpoints. X-Login-Path links the actual login flow without provoking unexplained browser Basic authentication.
- Omitted/replaced code: `FW_ENABLE_FULL_FLASH_DOWNLOAD full raw flash including settings`. Disposition: **replaced**. Reason: Network full-flash export remains disabled because it can expose stored Wi-Fi/cloud/access secrets. Application-partition download and separately authorized backup paths are retained. User-authorized physical full flash backup is separate, not this network route.
- Omitted/replaced code: `ProductConfig PU model/product identity and compile date/time header`. Disposition: **replaced**. Reason: Uses Pakar product/model and consistent BuildInfo version without dates/timestamp naming; no PU850 business identity is retained.
- Omitted/replaced code: `readFlashRange failure returning0 from an in-flight asynchronous fixed-length response`. Disposition: **replaced**. Reason: Zero is SDK end-of-file, not a transient flash error. A bounded retry/read-window plus source-invalid transport failure preserves advertised length/integrity semantics without advancing offsets, silently truncating, retaining dangling requests or retrying any mutation. Raw app-image authorization/range/ETag/HEAD behavior is unchanged; full-flash secrets remain disabled.
- Test-code evidence: `tests/firmware_read_response_test.cpp:41 [UTF8-LF sha256=fb0006a9575039fffc66a1e34e2f68f40844357a6d17fbe8655d637e7c72fad9; anchor=real SDK sets RESPONSE_FAILED + closes, never EOF]`.
- Test-code evidence: `tests/firmware_range_test.cpp:7 [UTF8-LF sha256=96a79254872cd829c6e23a240225063503d0286e9fe1a477dd48aac5e05a6ed5; anchor=assert(parseRange("bytes=0-9"]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:25 [UTF8-LF sha256=538f7c02dbd9184c0e500ea65bb14f7be924ef98d8f5a88b0e26c58938be534a; anchor=assert.match(transfer, /esp_partition_read]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Test-code evidence: `desktop/src/bridge_transfer.rs:275 [UTF8-LF sha256=fbb13202d86296f1766d0e20bcd9e4a520a8b13d4988890e5a5273a20a5469fe; anchor=fn bridge_transfer_real_binary_range_and_multipart_are_not_json_reparsed]`.
- Remaining: Real HTTP GET/HEAD/Range/auth/ETag checks against the integrated S3 and ESP8266 firmware remain separate from source/host-parser proof; verify exported application bytes against the running partition and protect credentials. Physical OTA/AVR flashing/power-loss/recovery acceptance remains open.

### PU850-ESP: `LocalLib/FirmwareTransfer.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `23121e9cb09ad44e41d0f4d9a4f3dec68ef09c8b79010b57cd0d2e4d14df3929`.

- Target: `firmware/esp8266/src/FirmwareTransfer.h:15 [UTF8-LF sha256=80bf2195482e2eddde0074f298d712008d7b0dbd38c3863b987c2c76f0be26c4; anchor=void RegisterFirmwareTransferRoutes(AsyncHttpService* server,]`.
- Target: `tools/check-ethernet-live.mjs:61 [UTF8-LF sha256=33e72a4a0a29bbbaba321485b5c98bb1936e2c1e2f58fed1e66d22e2a83905c1; anchor=export async function checkEthernet(]`.
- Retained: Complete donor header read through EOF. Public authorization check and route registration remain integrated with the shared asynchronous HTTP server; no separate ISP-only image or parallel updater service.
- Retained: Reconsulted complete donor for W5500 integration. Existing native ETH/lwIP shares the very same firmware HEAD/Range route; the hostname-first live probe checks actual TCP interface identity and exported image framing instead of assuming carrier means correct serving.
- Omitted/replaced code: `Single-argument RegisterFirmwareTransferRoutes declaration and optional token-only comments`. Disposition: **replaced**. Reason: The explicit std::function authorization provider adapts the same route to Pakar session/access ownership, defaults to deny without a provider and avoids duplicated credentials. Real device authorization acceptance is still required.
- Test-code evidence: `firmware/esp8266/src/FirmwareTransfer.cpp:157 [UTF8-LF sha256=98dff31984a81dcfb769fb036664e711c4312b257baefbdcbbac92168799d4de; anchor=return transferAuthorization && transferAuthorization(request);]`.
- Test-code evidence: `tests/ethernet_live_tool_test.mjs:17 [UTF8-LF sha256=6a0c0fcb764500ba41a918169306e417cbb94548a872dcccdbc424237bbcf9d9; anchor=for(const scenario of []`.
- Remaining: Real HTTP GET/HEAD/Range/auth/ETag checks against the integrated S3 and ESP8266 firmware remain separate from source/host-parser proof; verify exported application bytes against the running partition and protect credentials. Physical OTA/AVR flashing/power-loss/recovery acceptance remains open.

### PU850-ESP: `LocalLib/FS.cpp`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `7ee3832b2ef679d21013f7a06da0e885c7d53c954f5a9fde88dde1ee65e76dd7`.

- Target: `firmware/esp8266/src/SystemPlatform.h:21 [UTF8-LF sha256=679c04ccfc3310e68c4116f52fc2a450f6343795246444102ab98768b704990d; anchor=inline bool mountFilesystem()]`.
- Target: `firmware/esp8266/src/FilesystemTelemetry.h:31 [UTF8-LF sha256=6e0cdf19a541d7b60eb682729f5cc4436cde5d8ce3dfd2322077708dee3f0d9a; anchor=class Observation]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: entire commented initFS mount and Serial result logger
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `inactive mount/logger example`. Disposition: **replaced**. Reason: Actual SystemPlatform disables automatic format; FilesystemTelemetry reports mounted/unknown/error without protocol UART injection. Physical persistence/full-storage/power-loss acceptance stays open.

### PU850-ESP: `LocalLib/MNDP.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `33a7389ec2e1b5c3d75bf88f06d8397f2bf4ba008298ecd4610ddfaccc6e4eb3`.

- Target: `firmware/esp8266/src/Mndp.h:9 [UTF8-LF sha256=5d579c4dcc95c4fb298212798d03d127ce6f131819ac2b348036367c0ed5428a; anchor=class Packet]`.
- Target: `firmware/esp8266/src/MndpService.h:17 [UTF8-LF sha256=6d90c7bba686d0fcb9d144de689e60feb5967eaed5fede4a8a19a7842e9303f4; anchor=class MndpService]`.
- Target: `mobile/lib/discovery.dart:348 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=DiscoveredDevice? parseMndp]`.
- Target: `firmware/esp32s3/src/DiscoveryInterfacePolicy.h:12 [UTF8-LF sha256=94ead0d3e60e0aebe1a4bce7c18203cce57cfebacfbc2caff0fe3f6bb6b85814; anchor=struct DiscoveryInterfaces]`.
- Retained: Fully read header/implementation. Actual port 5678/group 224.0.0.88 and big-endian TLV type/length layout adapted into bounded packets; MAC, hostname, firmware version, platform, board, uptime, stable board ID and IPv4 metadata retained.
- Retained: Integrated station lifecycle starts/stops MNDP alongside existing discovery, announces at 30 seconds and accepts only a rate-limited four-zero-byte query. Android scans/decodes defensive bounded TLVs without sending credentials. Packet overflow prevents any partial advertisement.
- Retained: S3 enumeration now emits actual ready Ethernet/STA/AP MAC, IP, mask-derived reply selection and interface kind, with bounded membership recovery, while preserving donor TLV/rate/packet bounds. Ethernet AP/local discovery is not advertised as Internet proof.
- Omitted/replaced code: `Unbounded addTlv into 255-byte buffer; global UDP/ASA0002E header inclusion`. Disposition: **replaced**. Reason: Each copy checks the complete packet capacity and fails closed; module-owned socket state avoids donor globals/application header dependency. The header uses reserved/version bytes plus a bounded big-endian sequence, not a host-endian counter.
- Omitted/replaced code: `E_SerialNumber/firmwareVersion fallback and interface WiFi.SSID`. Disposition: **replaced**. Reason: Use the actual stable Pakar board ID, descriptive build identity and station interface instead of unknown PU serial/business fields or disclosing private SSIDs. This does not synthesize an AVR serial or expose Wi-Fi credentials.
- Omitted/replaced code: `IPv6 and unpack TLV definitions`. Disposition: **pending**. Reason: Donor send path does not implement these TLVs. Actual IPv6 readiness/metadata and compatibility need an explicit adapter and tests; do not emit invented addresses.
- Test-code evidence: `tests/mndp_test.cpp:11 [UTF8-LF sha256=871599d4f8ba3c23c123c769c5e85be75b3ce5d5c6ac4890ca4ed52eb8b8dbfa; anchor=never emit a partial advertisement]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:201 [UTF8-LF sha256=1e650a1f6897c67ce9e87de864ff28ed98e0a818ca95ba6b413897b5cd99a905; anchor=Actual loopback bridge requires bearer]`.
- Test-code evidence: `tests/network_path_test.cpp:8 [UTF8-LF sha256=e1e1b8c23498d6b67bc8334f4c8d4c73bf88d83f7373930d11f90d8318d8ec69; anchor=static void discoveryPolicy()]`.
- Remaining: Live router/Winbox response/announcement compatibility, interface/UUID continuity across rename/reconnect and integrated S3 adapter; compiled/fixture packets are not physical discovery acceptance.
- Remaining: Actual Ethernet MNDP multicast/query compatibility and overlapping-subnet ingress remain pending; host policies are not real LAN or Internet evidence.

### PU850-ESP: `LocalLib/MNDP.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d61e3e316c4a21f5c8f8fd7f6879008e68a1af4adc32b3459a9857b26d0bb03f`.

- Target: `firmware/esp8266/src/Mndp.h:9 [UTF8-LF sha256=5d579c4dcc95c4fb298212798d03d127ce6f131819ac2b348036367c0ed5428a; anchor=class Packet]`.
- Target: `firmware/esp8266/src/MndpService.h:17 [UTF8-LF sha256=6d90c7bba686d0fcb9d144de689e60feb5967eaed5fede4a8a19a7842e9303f4; anchor=class MndpService]`.
- Target: `mobile/lib/discovery.dart:348 [UTF8-LF sha256=fae7cf4ff72c6d72850e972e4e39832d838b96093266e9379cf4b3377c7b4a33; anchor=DiscoveredDevice? parseMndp]`.
- Retained: Fully read header/implementation. Actual port 5678/group 224.0.0.88 and big-endian TLV type/length layout adapted into bounded packets; MAC, hostname, firmware version, platform, board, uptime, stable board ID and IPv4 metadata retained.
- Retained: Integrated station lifecycle starts/stops MNDP alongside existing discovery, announces at 30 seconds and accepts only a rate-limited four-zero-byte query. Android scans/decodes defensive bounded TLVs without sending credentials. Packet overflow prevents any partial advertisement.
- Omitted/replaced code: `Unbounded addTlv into 255-byte buffer; global UDP/ASA0002E header inclusion`. Disposition: **replaced**. Reason: Each copy checks the complete packet capacity and fails closed; module-owned socket state avoids donor globals/application header dependency. The header uses reserved/version bytes plus a bounded big-endian sequence, not a host-endian counter.
- Omitted/replaced code: `E_SerialNumber/firmwareVersion fallback and interface WiFi.SSID`. Disposition: **replaced**. Reason: Use the actual stable Pakar board ID, descriptive build identity and station interface instead of unknown PU serial/business fields or disclosing private SSIDs. This does not synthesize an AVR serial or expose Wi-Fi credentials.
- Omitted/replaced code: `IPv6 and unpack TLV definitions`. Disposition: **pending**. Reason: Donor send path does not implement these TLVs. Actual IPv6 readiness/metadata and compatibility need an explicit adapter and tests; do not emit invented addresses.
- Test-code evidence: `tests/mndp_test.cpp:11 [UTF8-LF sha256=871599d4f8ba3c23c123c769c5e85be75b3ce5d5c6ac4890ca4ed52eb8b8dbfa; anchor=never emit a partial advertisement]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:201 [UTF8-LF sha256=1e650a1f6897c67ce9e87de864ff28ed98e0a818ca95ba6b413897b5cd99a905; anchor=Actual loopback bridge requires bearer]`.
- Remaining: Live router/Winbox response/announcement compatibility, interface/UUID continuity across rename/reconnect and integrated S3 adapter; compiled/fixture packets are not physical discovery acceptance.

### PU850-ESP: `LocalLib/NetDiscovery.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `168dbd75a7cd712143a26bda28e25ad058c85dff02d229f219c6b95551859fe8`.

- Target: `firmware/esp32s3/src/NbnsWire.h:26 [UTF8-LF sha256=5ecc7462855057585a28b732216798637158e5b830863b546490859ab8110699; anchor=inline size_t answer(]`.
- Target: `firmware/esp32s3/src/NbnsService.h:14 [UTF8-LF sha256=01d4a3c989bb3adaed24b13fb9e7923b99756bcb117e030428a8efab46c01d03; anchor=class NbnsService]`.
- Target: `firmware/esp32s3/src/Discovery.h:41 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=bool netbiosReady() const]`.
- Target: `firmware/esp32s3/src/Lldp.h:46 [UTF8-LF sha256=1c485dc2ee5fcaceefc2ac815a022a7fab4e5aa226d36256734ffb293c199060; anchor=inline bool parse(]`.
- Target: `firmware/esp32s3/src/EthernetLldp.cpp:136 [UTF8-LF sha256=753dd01083136f14e44d862fa883c916bbf6c971a64b8762dc1cc14c207c7e2c; anchor=void serviceEthernetLldp(]`.
- Target: `tools/check-router-upnp.ps1:60 [UTF8-LF sha256=91700b6cbeb7717b9d72b07247f2044f7a09cb8450753b3e49429046c61d5a39; anchor=function Invoke-DiscoveryProbe(]`.
- Retained: Completely read the actual donor source/header through EOF. NetDiscovery.cpp contains experimental ND_ACK/ANNOUNCE handlers and an infinite one-second-delay loop with last-octet identity; the header declarations/configuration are commented out. These are not a reusable NetBIOS or LLDP implementation. Retain the useful LAN discovery intent using actual product identity and bounded per-interface services, not a claimed byte-for-byte port.
- Retained: New S3 NBNS adapts the installed Arduino3.3.12/ESP8266 first-level name encoding plus RFC1002 bounded NB/NBSTAT messages into loop-owned nonblocking exact-netif UDP137 listeners; no task-owned AsyncUDP buffer, silent hostname truncation, SMB advertisement or wrong-subnet reply. LLDP is a separate pinned-IDF native Ethernet link diagnostic, not donor ND protocol parity.
- Omitted/replaced code: `Experimental ND_ACK/ANNOUNCE application protocol, last-octet ACK identity, infinite delay loop and commented header configuration`. Disposition: **pending**. Reason: Do not stall the integrated LED/API/UART loop or substitute an unauthenticated last-octet peer identity. Review any real ND peer consumer before a bounded product-identified compatibility adapter; useful custom ND semantics are not silently classified as business logic.
- Omitted/replaced code: `NBNS WINS registration/conflict arbitration, SMB sessions and LLMNR`. Disposition: **pending**. Reason: This slice provides local NB name/node-status replies only. It does not promise WINS registration or implement a file-sharing/session server; conflict arbitration and LLMNR need their own interface-bound runtime acceptance.
- Omitted/replaced code: `Automatic IGD port forwarding and router login`. Disposition: **pending**. Reason: Observed router advertises no IGD service yet; private label-based router credentials and isolated temporary mapping approval are pending. Do not expose auth-disabled board HTTP or Telnet to WAN, reset a router or guess credentials.
- Test-code evidence: `tests/nbns_test.cpp:66 [UTF8-LF sha256=4e4a84d2f731e50ae717805b32cb3acd5be00bdfa918894e9680779c23e28d48; anchor=Actual NBNS codec/service]`.
- Test-code evidence: `tests/ethernet_lldp_test.cpp:17 [UTF8-LF sha256=6d4d5656df1d2defdcafa0527b8cdd058f6c2fb8df1fba3baa66461f1d27fff8; anchor=void pure()]`.
- Test-code evidence: `tests/router_upnp_test.ps1:18 [UTF8-LF sha256=3fadc7e454cb0bbac934eb0363073044eb7e61ee861833eb9d1fcf5c1b7b3a4f; anchor=foreach($action]`.
- Remaining: Actual Windows NBNS name/node-status and all-netif overlap/rename tests require deployment. Actual W5500 wiring/link/LLDP-switch visibility, Ethernet-only API/WebSocket/OTA/cloud and unplug/replug acceptance remain open; host doubles and an SDK build are not physical proof.
- Remaining: Source provenance for the LLDP shim is the installed Arduino3.3.12/IDF5.5.5 native glue and W5500 driver: standard input callback preserved exactly, TCPIP-thread TX, explicit opt-in promiscuous RX because hardware MAC filtering rejects LLDP multicast. No arbitrary raw-frame API, layer2 bridge or router capability is advertised.

### PU850-ESP: `LocalLib/NetDiscovery.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `405ee3b6854ad478d58811b1bb27ab69ceae490e31673aa4daf2512017a29b66`.

- Target: `firmware/esp32s3/src/NbnsWire.h:26 [UTF8-LF sha256=5ecc7462855057585a28b732216798637158e5b830863b546490859ab8110699; anchor=inline size_t answer(]`.
- Target: `firmware/esp32s3/src/NbnsService.h:14 [UTF8-LF sha256=01d4a3c989bb3adaed24b13fb9e7923b99756bcb117e030428a8efab46c01d03; anchor=class NbnsService]`.
- Target: `firmware/esp32s3/src/Discovery.h:41 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=bool netbiosReady() const]`.
- Target: `firmware/esp32s3/src/Lldp.h:46 [UTF8-LF sha256=1c485dc2ee5fcaceefc2ac815a022a7fab4e5aa226d36256734ffb293c199060; anchor=inline bool parse(]`.
- Target: `firmware/esp32s3/src/EthernetLldp.cpp:136 [UTF8-LF sha256=753dd01083136f14e44d862fa883c916bbf6c971a64b8762dc1cc14c207c7e2c; anchor=void serviceEthernetLldp(]`.
- Target: `tools/check-router-upnp.ps1:60 [UTF8-LF sha256=91700b6cbeb7717b9d72b07247f2044f7a09cb8450753b3e49429046c61d5a39; anchor=function Invoke-DiscoveryProbe(]`.
- Retained: Completely read the actual donor source/header through EOF. NetDiscovery.cpp contains experimental ND_ACK/ANNOUNCE handlers and an infinite one-second-delay loop with last-octet identity; the header declarations/configuration are commented out. These are not a reusable NetBIOS or LLDP implementation. Retain the useful LAN discovery intent using actual product identity and bounded per-interface services, not a claimed byte-for-byte port.
- Retained: New S3 NBNS adapts the installed Arduino3.3.12/ESP8266 first-level name encoding plus RFC1002 bounded NB/NBSTAT messages into loop-owned nonblocking exact-netif UDP137 listeners; no task-owned AsyncUDP buffer, silent hostname truncation, SMB advertisement or wrong-subnet reply. LLDP is a separate pinned-IDF native Ethernet link diagnostic, not donor ND protocol parity.
- Omitted/replaced code: `Experimental ND_ACK/ANNOUNCE application protocol, last-octet ACK identity, infinite delay loop and commented header configuration`. Disposition: **pending**. Reason: Do not stall the integrated LED/API/UART loop or substitute an unauthenticated last-octet peer identity. Review any real ND peer consumer before a bounded product-identified compatibility adapter; useful custom ND semantics are not silently classified as business logic.
- Omitted/replaced code: `NBNS WINS registration/conflict arbitration, SMB sessions and LLMNR`. Disposition: **pending**. Reason: This slice provides local NB name/node-status replies only. It does not promise WINS registration or implement a file-sharing/session server; conflict arbitration and LLMNR need their own interface-bound runtime acceptance.
- Omitted/replaced code: `Automatic IGD port forwarding and router login`. Disposition: **pending**. Reason: Observed router advertises no IGD service yet; private label-based router credentials and isolated temporary mapping approval are pending. Do not expose auth-disabled board HTTP or Telnet to WAN, reset a router or guess credentials.
- Test-code evidence: `tests/nbns_test.cpp:66 [UTF8-LF sha256=4e4a84d2f731e50ae717805b32cb3acd5be00bdfa918894e9680779c23e28d48; anchor=Actual NBNS codec/service]`.
- Test-code evidence: `tests/ethernet_lldp_test.cpp:17 [UTF8-LF sha256=6d4d5656df1d2defdcafa0527b8cdd058f6c2fb8df1fba3baa66461f1d27fff8; anchor=void pure()]`.
- Test-code evidence: `tests/router_upnp_test.ps1:18 [UTF8-LF sha256=3fadc7e454cb0bbac934eb0363073044eb7e61ee861833eb9d1fcf5c1b7b3a4f; anchor=foreach($action]`.
- Remaining: Actual Windows NBNS name/node-status and all-netif overlap/rename tests require deployment. Actual W5500 wiring/link/LLDP-switch visibility, Ethernet-only API/WebSocket/OTA/cloud and unplug/replug acceptance remain open; host doubles and an SDK build are not physical proof.
- Remaining: Source provenance for the LLDP shim is the installed Arduino3.3.12/IDF5.5.5 native glue and W5500 driver: standard input callback preserved exactly, TCPIP-thread TX, explicit opt-in promiscuous RX because hardware MAC filtering rejects LLDP multicast. No arbitrary raw-frame API, layer2 bridge or router capability is advertised.

### PU850-ESP: `LocalLib/Public.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `9cda4aa1a1d02d3a7f8042173e049d326788952b7a05438a57b91d6e7d381303`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: UnPack32 four MSB-first shifts; eExpandDateTime/eCompressDateTime; CompressDate/ShamsiU16toU8/GregorianU16toU8
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic compressed Gregorian/Shamsi date/time and year offsets`. Disposition: **pending**. Reason: Preserve useful calendar capability with validated real consumers/ranges/months/tests, not shared mutable ePart/year globals or PU payload compatibility.
- Remaining: Preserve useful calendar capability with validated real consumers/ranges/months/tests, not shared mutable ePart/year globals or PU payload compatibility.

### PU850-ESP: `LocalLib/Public.h`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a5e504e4b33c8b1e26d41a7e5a8196fc1f7e7ae7a41be6209dc5617e484a83af`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: stdbool include and bool-to-boolean/true/false replacement macros
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `C++ boolean token replacement`. Disposition: **replaced**. Reason: The supported toolchains natively implement bool/true/false; global macro substitution can corrupt SDK/system headers.

### PU850-ESP: `LocalLib/RemoteCommand.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `9ca2a3d6be55cc5d0271d0e036e28fbc6a25ed71c276f53128262a03d30e9e52`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: all UDP state/ports; initUdpDebug/UdpService/udpSend/handleUdpReceive/executeCommand; all ServiceEvent cases
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic bounded UDP diagnostic/network-event lifecycle`. Disposition: **pending**. Reason: File is disabled in donor sketch, input truncates at254 bytes, no auth/identity, and UdpService tests a function pointer. Define capability/ownership/fixtures before adding any listener or SDK event adapter.
- Remaining: File is disabled in donor sketch, input truncates at254 bytes, no auth/identity, and UdpService tests a function pointer. Define capability/ownership/fixtures before adding any listener or SDK event adapter.

### PU850-ESP: `LocalLib/Sessions.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `6186f9861e8ee3dcb093f260e25311a1e3952b5c6c2a7fa764de05b8a45e7f55`.

- Target: `libraries/BoardCore/src/AccessPolicy.h:39 [UTF8-LF sha256=3524ee6acd036d7ef30e266d55fd71045628abe2c57243383162dea8e90c55d0; anchor=class Sessions]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: initSessions; addSession free-slot and unused-oldest scan; destroySession; validateSession; empty SessionCommand
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `SessionCommand authenticated protocol-origin revoke semantics`. Disposition: **pending**. Reason: Existing bounded 8-slot AccessPolicy imports the pool with expiry, unsigned elapsed time, strict hex and constant-work compare. Donor SessionCommand is empty; review actual UART-origin authority before implementing a new command.
- Test-code evidence: `tests/access_policy_test.cpp:13 [UTF8-LF sha256=96b2ca0669e125a08354779919684c192e71e7a2a6e760202eda653f5824c737; anchor=Sessions sessions;]`.
- Remaining: Existing bounded 8-slot AccessPolicy imports the pool with expiry, unsigned elapsed time, strict hex and constant-work compare. Donor SessionCommand is empty; review actual UART-origin authority before implementing a new command.

### PU850-ESP: `LocalLib/Sessions.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `1ab8ea8f32ce56d64b976eb1c6922fa5476a3fd0498526bbf453a4254726862c`.

- Target: `libraries/BoardCore/src/AccessPolicy.h:39 [UTF8-LF sha256=3524ee6acd036d7ef30e266d55fd71045628abe2c57243383162dea8e90c55d0; anchor=class Sessions]`.
- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: MAX_SESSIONS/TOKEN_LENGTH; raw token/accessLevel/lastActive/lastIP/isLoggedIn struct/global array; SessionCommand declaration
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `SessionCommand lifecycle declaration`. Disposition: **pending**. Reason: Existing encapsulated AccessPolicy/WebAccess owns roles/address/expiry/mutex admission; exact authenticated UART session lifecycle is still pending.
- Test-code evidence: `tests/access_policy_test.cpp:13 [UTF8-LF sha256=96b2ca0669e125a08354779919684c192e71e7a2a6e760202eda653f5824c737; anchor=Sessions sessions;]`.
- Remaining: Existing encapsulated AccessPolicy/WebAccess owns roles/address/expiry/mutex admission; exact authenticated UART session lifecycle is still pending.

### PU850-ESP: `LocalLib/Shell.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0113acbff973285f5af186a4857d94cc2df3c99813e0db4ba876f861e7c1083b`.

- Target: `firmware/esp8266/src/TelnetShell.h:95 [UTF8-LF sha256=20e75d8dd8f39bea93c0df3da5aff98635b857eefb55003cfebeabd4e5c92281; anchor=class LineEditor]`.
- Target: `firmware/esp8266/src/ShellProperties.h:15 [UTF8-LF sha256=7f5e741f7ff29d308da7d201a59b37f116a5b846ec7bcb157032477ca7f34816; anchor=static const ShellProperty shellProperties[]]`.
- Target: `firmware/esp8266/src/ShellWire.h:9 [UTF8-LF sha256=af3f23d908310dd349b0c90d1fc4385a212e6439b10c72f8dc2f37dace4b5216; anchor=class SerialChannel]`.
- Target: `firmware/esp8266/src/ShellTuiBinding.h:3 [UTF8-LF sha256=6ba1c1b6da225954c10988a2269eee7dc5f422d6f59ac3c6453bc3d51e7f7138; anchor=struct ShellTuiSnapshot]`.
- Target: `tools/device-shell.ps1:8 [UTF8-LF sha256=4e197e3502eb6ef40d2a3df7bdde97ed3c05140c97e06e3d07426232732a9c55; anchor=AutomaticCommandRetry=$false]`.
- Retained: PU ShellSession transport/editor separation, insertion/shift/backspace/delete, CSI/SS3 arrows/home/end, Ctrl-C/Ctrl-L and split CR-LF/CR-NUL behavior are adapted in instance-owned TelnetParser/LineEditor.
- Retained: Generic parameter lookup/read-only enforcement and dispatch now map to ShellProperty descriptors, typed shellSet and the existing Pakar command/configuration owners; the same descriptors feed the device TUI.
- Retained: Session-owned prompt-safe watch summaries use redrawEvent without losing the draft/cursor. Utilities clear/cls, echo, hex and upper/noupper are present; bounded eight-entry history adds recall without retaining classified secrets.
- Retained: Raw serial ownership is replaced by an additive boot-bound COBS shell channel: explicit token, contiguous nonwrapping input sequence, duplicate-byte ACK without redispatch, and immutable output windows consumed only by matching ACK.
- Omitted/replaced code: `Raw ShellTransport_Serial/SerialShellSession and mutable global command/include Commands.cpp adapter`. Disposition: **replaced**. Reason: A separately boot-fenced COBS shell channel preserves USB recovery/protocol framing rather than mixing raw ASCII into the existing decoder. Instance-owned dispatch removes the donor global command/output pointer; host script never automatically replays uncertain input.
- Omitted/replaced code: `NumberFromString permissive comma/minus parsing and unchecked signed multiply/add`. Disposition: **replaced**. Reason: Strict unsignedNumber validates full decimal input and overflow before conversion; command-specific signed display handling is explicit. Fixtures reject signs, commas, trailing junk and UINT32 overflow rather than silently normalizing a dangerous argument.
- Omitted/replaced code: `ShellBroadcastLine unrestricted event broadcast`. Disposition: **replaced**. Reason: Opt-in per-session watch publishes changed bounded summaries with prompt erase/redraw and backpressure. This is not an ordered complete peripheral event stream; individual event delivery remains outside this shell slice.
- Omitted/replaced code: `upper/noupper applies to all donor input echo and prompt redraw`. Disposition: **pending**. Reason: Current session upper option changes echo command output only; the original wider input-display behavior is not claimed equivalent.
- Omitted/replaced code: `Donor printable-ASCII editor and richer Unicode terminals`. Disposition: **pending**. Reason: Arduino line editor/TUI deliberately accepts printable ASCII and sanitizes external controls. Persian/UTF-8 editing, bidi layout and font support are not supplied by this shell adaptation.
- Test-code evidence: `tests/shell_service_test.mjs:18 [UTF8-LF sha256=def5ff0a5e8a20eee060c6ba40df782f2f52a89d18f45713c0c3982cbfc763a8; anchor=Actual shell fixture extraction markers changed]`.
- Test-code evidence: `tests/shell_service_test.cpp:132 [UTF8-LF sha256=f235dde1902ae6949f8a4460a03c55b99c8886adf6befc113af9f9ea6112294b; anchor=Accept crosses millis rollover.]`.
- Test-code evidence: `tests/shell_properties_test.cpp:4 [UTF8-LF sha256=3eebb3865c918b7432e06691bd5f63915588b84d3b80ab824819e62e1c7fffe8; anchor=int main()]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:88 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=editor.redrawEvent(]`.
- Test-code evidence: `tests/shell_wire_test.cpp:11 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=C::Duplicate]`.
- Test-code evidence: `tests/shell_tui_test.cpp:22 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=Unchanged Enter is not a write.]`.
- Test-code evidence: `tools/test-shell.ps1:5 [UTF8-LF sha256=35e69b9b0de3b37121afb19c96b5d903e0323a1a004c654ac3e2024c13668a50; anchor=-Concern device-shell -Gate]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### PU850-ESP: `LocalLib/Shell.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `9f8e76509ad917d8e204a82d9495e376607a159547d4eee3de6760e6bd5d7da6`.

- Target: `firmware/esp8266/src/TelnetShell.h:95 [UTF8-LF sha256=20e75d8dd8f39bea93c0df3da5aff98635b857eefb55003cfebeabd4e5c92281; anchor=class LineEditor]`.
- Target: `firmware/esp8266/src/ShellProperties.h:15 [UTF8-LF sha256=7f5e741f7ff29d308da7d201a59b37f116a5b846ec7bcb157032477ca7f34816; anchor=static const ShellProperty shellProperties[]]`.
- Target: `firmware/esp8266/src/ShellWire.h:9 [UTF8-LF sha256=af3f23d908310dd349b0c90d1fc4385a212e6439b10c72f8dc2f37dace4b5216; anchor=class SerialChannel]`.
- Target: `firmware/esp8266/src/ShellTuiBinding.h:3 [UTF8-LF sha256=6ba1c1b6da225954c10988a2269eee7dc5f422d6f59ac3c6453bc3d51e7f7138; anchor=struct ShellTuiSnapshot]`.
- Target: `tools/device-shell.ps1:8 [UTF8-LF sha256=4e197e3502eb6ef40d2a3df7bdde97ed3c05140c97e06e3d07426232732a9c55; anchor=AutomaticCommandRetry=$false]`.
- Retained: PU ShellSession transport/editor separation, insertion/shift/backspace/delete, CSI/SS3 arrows/home/end, Ctrl-C/Ctrl-L and split CR-LF/CR-NUL behavior are adapted in instance-owned TelnetParser/LineEditor.
- Retained: Generic parameter lookup/read-only enforcement and dispatch now map to ShellProperty descriptors, typed shellSet and the existing Pakar command/configuration owners; the same descriptors feed the device TUI.
- Retained: Session-owned prompt-safe watch summaries use redrawEvent without losing the draft/cursor. Utilities clear/cls, echo, hex and upper/noupper are present; bounded eight-entry history adds recall without retaining classified secrets.
- Retained: Raw serial ownership is replaced by an additive boot-bound COBS shell channel: explicit token, contiguous nonwrapping input sequence, duplicate-byte ACK without redispatch, and immutable output windows consumed only by matching ACK.
- Omitted/replaced code: `Raw ShellTransport_Serial/SerialShellSession and mutable global command/include Commands.cpp adapter`. Disposition: **replaced**. Reason: A separately boot-fenced COBS shell channel preserves USB recovery/protocol framing rather than mixing raw ASCII into the existing decoder. Instance-owned dispatch removes the donor global command/output pointer; host script never automatically replays uncertain input.
- Omitted/replaced code: `NumberFromString permissive comma/minus parsing and unchecked signed multiply/add`. Disposition: **replaced**. Reason: Strict unsignedNumber validates full decimal input and overflow before conversion; command-specific signed display handling is explicit. Fixtures reject signs, commas, trailing junk and UINT32 overflow rather than silently normalizing a dangerous argument.
- Omitted/replaced code: `ShellBroadcastLine unrestricted event broadcast`. Disposition: **replaced**. Reason: Opt-in per-session watch publishes changed bounded summaries with prompt erase/redraw and backpressure. This is not an ordered complete peripheral event stream; individual event delivery remains outside this shell slice.
- Omitted/replaced code: `upper/noupper applies to all donor input echo and prompt redraw`. Disposition: **pending**. Reason: Current session upper option changes echo command output only; the original wider input-display behavior is not claimed equivalent.
- Omitted/replaced code: `Donor printable-ASCII editor and richer Unicode terminals`. Disposition: **pending**. Reason: Arduino line editor/TUI deliberately accepts printable ASCII and sanitizes external controls. Persian/UTF-8 editing, bidi layout and font support are not supplied by this shell adaptation.
- Test-code evidence: `tests/shell_service_test.mjs:18 [UTF8-LF sha256=def5ff0a5e8a20eee060c6ba40df782f2f52a89d18f45713c0c3982cbfc763a8; anchor=Actual shell fixture extraction markers changed]`.
- Test-code evidence: `tests/shell_service_test.cpp:132 [UTF8-LF sha256=f235dde1902ae6949f8a4460a03c55b99c8886adf6befc113af9f9ea6112294b; anchor=Accept crosses millis rollover.]`.
- Test-code evidence: `tests/shell_properties_test.cpp:4 [UTF8-LF sha256=3eebb3865c918b7432e06691bd5f63915588b84d3b80ab824819e62e1c7fffe8; anchor=int main()]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:88 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=editor.redrawEvent(]`.
- Test-code evidence: `tests/shell_wire_test.cpp:11 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=C::Duplicate]`.
- Test-code evidence: `tests/shell_tui_test.cpp:22 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=Unchanged Enter is not a write.]`.
- Test-code evidence: `tools/test-shell.ps1:5 [UTF8-LF sha256=35e69b9b0de3b37121afb19c96b5d903e0323a1a004c654ac3e2024c13668a50; anchor=-Concern device-shell -Gate]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### PU850-ESP: `LocalLib/SSDP.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `2f9c9f8f0b613d958e8bb31920174a7d8fa9f523b3477b88d9430a71d385bf48`.

- Target: `firmware/esp32s3/src/Discovery.h:11 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=class Discovery {]`.
- Target: `firmware/esp32s3/src/Discovery.h:44 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=String xmlDescription() const {]`.
- Retained: Complete donor wrapper was reread by the evidence owner, in addition to network agent's full read. Actual initSSDP metadata/schema/periodic-service ownership maps to Discovery::begin/service/xmlDescription and the imported SsdpDevice::schema XML escaper. Hostname/display name/application/version are Pakar-owned; no PU850 EEPROM business identity is retained.
- Omitted/replaced code: `ASA0002E/E_MainVersion/E_SerialNumber globals, include-SSDPDevice.cpp pattern, server-global registration and duplicate hand-built description`. Disposition: **replaced**. Reason: Canonical main registers the description route; instance-owned Discovery uses consistent Pakar metadata and the imported escaped schema. No duplicate compiled .cpp/global service or PU serial/version state is retained.
- Omitted/replaced code: `Optional UPnP serviceList/SOAP/GENA implementation`. Disposition: **not-applicable**. Reason: The donor intentionally does not advertise an unimplemented service. Pakar likewise leaves optional service discovery disabled; no invented control protocol is exposed. Add a real contract before enabling it.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/SSDP.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `f9f7b078b6085e731f0ed42c5dd983d1ab0a3a48c2912797406fb3e0b0df3ea5`.

- Target: `firmware/esp32s3/src/Discovery.h:57 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=SsdpDevice ssdp_;]`.
- Retained: Complete tiny donor header was reread by the evidence owner. Its current role is only the historical SSDP-to-singleton alias; actual device declarations map separately to the imported SsdpDevice.h.
- Omitted/replaced code: `#define SSDP SSDPDevice singleton shorthand`. Disposition: **replaced**. Reason: Explicit Discovery-owned instance avoids macro alias collisions and makes restart/teardown ownership clear; no useful implementation is silently dropped with this alias.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/SSDPDevice.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `82f8c2a3c9779392202dc9b864328040cf47b9303674f142e5348df2b17e34b4`.

- Target: `firmware/esp32s3/src/SsdpDevice.cpp:267 [UTF8-LF sha256=c05031ff074885374295d471878742ef2f785c6591786246e5023454118f78b9; anchor=void SsdpDevice::post(]`.
- Target: `firmware/esp32s3/src/SsdpDevice.cpp:568 [UTF8-LF sha256=c05031ff074885374295d471878742ef2f785c6591786246e5023454118f78b9; anchor=void SsdpDevice::schema(Print &client) const]`.
- Target: `firmware/esp32s3/src/SsdpUdp.h:70 [UTF8-LF sha256=e25782c43552d9a2fb570b2dd18446c99907a9b58d0d1ec6f5f190b9fee84f98; anchor=bool beginPacketMulticast(]`.
- Target: `firmware/esp32s3/src/DiscoveryInterfaces.h:17 [UTF8-LF sha256=f6c87195a581b098486e533d44192b3fa728145977888caf46676f780c9a50b6; anchor=inline DiscoveryInterfaces activeDiscoveryInterfaces()]`.
- Target: `firmware/esp32s3/src/EthernetPort.cpp:41 [UTF8-LF sha256=b2ef56e69ae00a5d74c6d1fb90b7cd1dbdcaf2757f440b05f52abc28938c8a0a; anchor=bool EthernetPort::begin(]`.
- Retained: Network agent fully read the actual complete PU850 donor and copied/adapted its source, not merely an SDK call. The evidence owner reviewed the complete donor-to-target diff. Retains bounded/de-duplicated delayed M-SEARCH queue, root/UUID/device responses, alive/update/byebye, boot/config identity, topology change handling and XML escaping/schema.
- Retained: Only platform boundary and ownership are changed: PakarS3 namespace/instance-owned class, ESP32 chip identity and interface-bound SsdpUdp replace global ESP8266 WiFiUDP/IGMP. Both active AP and station memberships are supported rather than carrying ESP8266's small-IGMP-pool one-interface restriction.
- Retained: Extended the actual SDK socket adapter from STA/AP to ready Ethernet/STA/AP, with independent W5500 SPI3 ownership, actual subnet masks, bounded secondary membership retry and topology cleanup. Native mDNS uses SDK netifs; no second WIZnet-only API/socket stack. Defaults leave W5500 uninitialized until the explicit build feature is selected.
- Omitted/replaced code: `Global SSDPDeviceClass singleton, ESP.getChipId and manual igmp_leavegroup/ESP8266 one-interface IGMP-pool policy`. Disposition: **replaced**. Reason: Instance-owned S3 Discovery explicitly owns teardown and stable identity; socket-scoped membership/IF/TTL replaces incompatible ESP8266 UDP internals. The adapter is bounded/nonblocking and no arbitrary shared global SPI/network engine is substituted. Actual AP/STA behavior remains unverified.
- Test-code evidence: `tests/network_path_test.cpp:20 [UTF8-LF sha256=e1e1b8c23498d6b67bc8334f4c8d4c73bf88d83f7373930d11f90d8318d8ec69; anchor=int main()]`.
- Test-code evidence: `tests/ethernet_discovery_test.mjs:41 [UTF8-LF sha256=9ed62ccfc6cb24433d80ba11e748e6f23ed8e09f5b31ebb8ee5ed90e8907f5d0; anchor=Ethernet discovery/build source integration checks passed]`.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.
- Remaining: Verify W5500 physical multicast/source routing and all-netif mDNS on actual hardware. Custom UDP lacks ingress-netif metadata: equal overlapping subnets use documented longest-prefix/tie policy, not proven exact ingress.

### PU850-ESP: `LocalLib/SSDPDevice.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `3d94b63400ab64c0e556b1e73c722b40f7adade996aaf40ce73f88d1d0ab7cf2`.

- Target: `firmware/esp32s3/src/SsdpDevice.h:98 [UTF8-LF sha256=09da08b4765186ff4549e6552ecda1791ba38369de57eb8af038e4771db85c60; anchor=class SsdpDevice {]`.
- Retained: Network agent fully read the actual donor; evidence owner reviewed the complete header diff. Retains packet templates, bounded queue and metadata fields, public discovery/description setters, cache interval and protocol structures. Adds truthful running() state from the real socket/interface.
- Omitted/replaced code: `ESP8266WiFi/WiFiUDP/IGMP includes, ProductConfig and extern singleton declaration`. Disposition: **replaced**. Reason: ESP32 WiFi, explicit socket adapter, Pakar DiscoveryConfig and namespace-owned instance replace platform-specific includes/global ownership. SERVER announces ESP32-S3 rather than falsely identifying as ESP8266. Useful public protocol behavior is retained; physical discovery remains open.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/Telnet.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a78d88c149d19d8989c6db3dce0731189c2bf46c45fd64b73e9871a80f6c9bdd`.

- Target: `firmware/esp8266/src/TelnetShell.h:45 [UTF8-LF sha256=20e75d8dd8f39bea93c0df3da5aff98635b857eefb55003cfebeabd4e5c92281; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/src/ShellService.h:270 [UTF8-LF sha256=897ac8e49f95e1e2ebc8b59dbd1a76148d1fcaaecf609cf33d99d067fc849a0c; anchor=void serviceDeviceShells()]`.
- Target: `firmware/esp8266/src/ShellSettings.h:9 [UTF8-LF sha256=edc36bb702f4304ee1eac6e2e848cf4f62b9d075bb9d6e90349029c768ae19e4; anchor=class ShellSettings]`.
- Target: `firmware/esp8266/src/ShellBuild.h:5 [UTF8-LF sha256=a2a8a9ee528e9cb48a64ad11268ff473cfbaecc8351ab01c1d9c0d55ed8128a7; anchor=PAKAR_TELNET_SESSIONS]`.
- Retained: Actual five-state IAC/option/subnegotiation parser, escaped IAC, server ECHO/SGA greeting and unsupported-option rejection are retained. Option state suppresses greeting-ACK negotiation loops.
- Retained: Native WiFiServer/WiFiClient sessions use independent parser/editor/TUI/auth state, configurable bounded session count, 64-byte input budget, bounded output and nonblocking S3 socket writes; slow or overfull sessions retire rather than blocking LED/UART work.
- Retained: ShellSettings persists enable, TCP port and color with checksum, pending write/readback/rename and unchanged-write suppression. Configuration schedules listener retirement/rebind after its response; published discovery port comes from the actual bound listener.
- Retained: Authentication readiness/security revision fences both Telnet and framed USB; asynchronous close requests are deferred to the main loop so callbacks do not delete a live loop-owned editor/socket.
- Omitted/replaced code: `ESPTelnet dependency and ESP8266WEBSERVER_H synchronous-header suppression`. Disposition: **replaced**. Reason: Native WiFiServer/WiFiClient avoids the conflicting synchronous web header while retaining the actual Telnet protocol/parser behavior, not merely newline input.
- Omitted/replaced code: `Single global TelnetShellSession and legacy product greeting callbacks`. Disposition: **replaced**. Reason: Bounded independent sessions use Pakar application/hostname identity and the existing access owner. Telnet is still plaintext and is not claimed encrypted; disabling it or compile-time gating remains available.
- Omitted/replaced code: `TelnetNetworkAvailable stop/restart and mDNS remove on every AP/station loss`. Disposition: **pending**. Reason: Source now supports durable enable/port rebind and truthful actual-listener discovery. Exact donor network-loss transition parity and multi-interface socket behavior still need explicit review/live acceptance rather than being inferred from settings tests.
- Test-code evidence: `tests/shell_service_test.mjs:18 [UTF8-LF sha256=def5ff0a5e8a20eee060c6ba40df782f2f52a89d18f45713c0c3982cbfc763a8; anchor=Actual shell fixture extraction markers changed]`.
- Test-code evidence: `tests/shell_service_test.cpp:132 [UTF8-LF sha256=f235dde1902ae6949f8a4460a03c55b99c8886adf6befc113af9f9ea6112294b; anchor=Accept crosses millis rollover.]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:18 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=No negotiation loops on greeting acknowledgements.]`.
- Test-code evidence: `tests/shell_wire_test.cpp:34 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=fs.failRename=true]`.
- Test-code evidence: `tests/shell_wire_test.cpp:36 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=!corrupt.enabled()]`.
- Test-code evidence: `tools/test-shell.ps1:5 [UTF8-LF sha256=35e69b9b0de3b37121afb19c96b5d903e0323a1a004c654ac3e2024c13668a50; anchor=-Concern device-shell -Gate]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### PU850-ESP: `LocalLib/Telnet.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `e85f5f1a4f2767323d9886313ddb2aabfd3acad7d781c2770bc8d2b2d2c6cb15`.

- Target: `firmware/esp8266/src/TelnetShell.h:45 [UTF8-LF sha256=20e75d8dd8f39bea93c0df3da5aff98635b857eefb55003cfebeabd4e5c92281; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/src/ShellService.h:270 [UTF8-LF sha256=897ac8e49f95e1e2ebc8b59dbd1a76148d1fcaaecf609cf33d99d067fc849a0c; anchor=void serviceDeviceShells()]`.
- Target: `firmware/esp8266/src/ShellSettings.h:9 [UTF8-LF sha256=edc36bb702f4304ee1eac6e2e848cf4f62b9d075bb9d6e90349029c768ae19e4; anchor=class ShellSettings]`.
- Target: `firmware/esp8266/src/ShellBuild.h:5 [UTF8-LF sha256=a2a8a9ee528e9cb48a64ad11268ff473cfbaecc8351ab01c1d9c0d55ed8128a7; anchor=PAKAR_TELNET_SESSIONS]`.
- Retained: Actual five-state IAC/option/subnegotiation parser, escaped IAC, server ECHO/SGA greeting and unsupported-option rejection are retained. Option state suppresses greeting-ACK negotiation loops.
- Retained: Native WiFiServer/WiFiClient sessions use independent parser/editor/TUI/auth state, configurable bounded session count, 64-byte input budget, bounded output and nonblocking S3 socket writes; slow or overfull sessions retire rather than blocking LED/UART work.
- Retained: ShellSettings persists enable, TCP port and color with checksum, pending write/readback/rename and unchanged-write suppression. Configuration schedules listener retirement/rebind after its response; published discovery port comes from the actual bound listener.
- Retained: Authentication readiness/security revision fences both Telnet and framed USB; asynchronous close requests are deferred to the main loop so callbacks do not delete a live loop-owned editor/socket.
- Omitted/replaced code: `ESPTelnet dependency and ESP8266WEBSERVER_H synchronous-header suppression`. Disposition: **replaced**. Reason: Native WiFiServer/WiFiClient avoids the conflicting synchronous web header while retaining the actual Telnet protocol/parser behavior, not merely newline input.
- Omitted/replaced code: `Single global TelnetShellSession and legacy product greeting callbacks`. Disposition: **replaced**. Reason: Bounded independent sessions use Pakar application/hostname identity and the existing access owner. Telnet is still plaintext and is not claimed encrypted; disabling it or compile-time gating remains available.
- Omitted/replaced code: `TelnetNetworkAvailable stop/restart and mDNS remove on every AP/station loss`. Disposition: **pending**. Reason: Source now supports durable enable/port rebind and truthful actual-listener discovery. Exact donor network-loss transition parity and multi-interface socket behavior still need explicit review/live acceptance rather than being inferred from settings tests.
- Test-code evidence: `tests/shell_service_test.mjs:18 [UTF8-LF sha256=def5ff0a5e8a20eee060c6ba40df782f2f52a89d18f45713c0c3982cbfc763a8; anchor=Actual shell fixture extraction markers changed]`.
- Test-code evidence: `tests/shell_service_test.cpp:132 [UTF8-LF sha256=f235dde1902ae6949f8a4460a03c55b99c8886adf6befc113af9f9ea6112294b; anchor=Accept crosses millis rollover.]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:18 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=No negotiation loops on greeting acknowledgements.]`.
- Test-code evidence: `tests/shell_wire_test.cpp:34 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=fs.failRename=true]`.
- Test-code evidence: `tests/shell_wire_test.cpp:36 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=!corrupt.enabled()]`.
- Test-code evidence: `tools/test-shell.ps1:5 [UTF8-LF sha256=35e69b9b0de3b37121afb19c96b5d903e0323a1a004c654ac3e2024c13668a50; anchor=-Concern device-shell -Gate]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### PU850-ESP: `LocalLib/UART.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `8c8201cad39550cffb8567b279447a093061bba7b9a188dbe2f47dbd22ef9a45`.

- Target: `libraries/BoardCore/src/SerialLink.h:12 [UTF8-LF sha256=99373bc4c090e34324cfd2ae8c94d6666c227ad7e157a47fbd85125aac6c13a0; anchor=class SerialLink {]`.
- Target: `libraries/BoardCore/src/UartFlow.h:26 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=inline uint16_t receiveBudget(]`.
- Target: `libraries/BoardCore/src/Protocol.h:191 [UTF8-LF sha256=a525972fb3364a20efb8cfa7d611d19aed3b2850dd6069b78efc446ff9daaf36; anchor=inline bool decode(]`.
- Target: `docs/PU850-PARITY.md:141 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C++ transfer and UART, 2026-10-19]`.
- Retained: Every transport and endpoint function read through EOF; current bounded COBS/CRC/sequence/explicit ownership remains, with exact generic pending and business-only omissions.
- Retained: Complete section/function decisions and concrete unsafe legacy defects recorded in the C++ transfer and UART section of docs/PU850-PARITY.md.
- Omitted/replaced code: `SOH/EOT framing/RevisionMatch/bulk ring/string endpoints/baud/drain/response match`. Disposition: **replaced**. Reason: Bounded COBS/CRC/sequence/current capability owner replaces unframed legacy ABI. Exact modulo-zero, sizeof(pointer), U16 wrapping, ten-byte string overflow, AP wrong-interface and baud fallthrough defects are recorded in PU850-PARITY.
- Omitted/replaced code: `Generic panel/title/buttons/icons/language/clock/voltage/topology/file bulk/backpressure/cancel/session/diagnostic/logging`. Disposition: **pending**. Reason: Not business exclusions. Current low-level transport and HTTP backup do not prove these high-level paired capabilities; unwired power is Unknown, raw credential traces prohibited, file receive and diagnostic placeholders not claimed implemented.
- Omitted/replaced code: `Weight/hidden-weight/tare/receipt/report payloads only`. Disposition: **not-applicable**. Reason: Exact PU weighing/report business operations have no PakarController consumer; this does not exclude generic settings, file, UI or diagnostic code.
- Remaining: Not business exclusions. Current low-level transport and HTTP backup do not prove these high-level paired capabilities; unwired power is Unknown, raw credential traces prohibited, file receive and diagnostic placeholders not claimed implemented.

### PU850-ESP: `LocalLib/UART.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `13d3ec4617363d32ba58595a9e24137c45501cbda4f923b5e08e561e066e50fc`.

- Target: `libraries/BoardCore/src/SerialLink.h:12 [UTF8-LF sha256=99373bc4c090e34324cfd2ae8c94d6666c227ad7e157a47fbd85125aac6c13a0; anchor=class SerialLink {]`.
- Target: `libraries/BoardCore/src/UartFlow.h:26 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=inline uint16_t receiveBudget(]`.
- Target: `libraries/BoardCore/src/Protocol.h:191 [UTF8-LF sha256=a525972fb3364a20efb8cfa7d611d19aed3b2850dd6069b78efc446ff9daaf36; anchor=inline bool decode(]`.
- Target: `docs/PU850-PARITY.md:141 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C++ transfer and UART, 2026-10-19]`.
- Retained: Every transport and endpoint function read through EOF; current bounded COBS/CRC/sequence/explicit ownership remains, with exact generic pending and business-only omissions.
- Retained: Complete section/function decisions and concrete unsafe legacy defects recorded in the C++ transfer and UART section of docs/PU850-PARITY.md.
- Omitted/replaced code: `SOH/EOT framing/RevisionMatch/bulk ring/string endpoints/baud/drain/response match`. Disposition: **replaced**. Reason: Bounded COBS/CRC/sequence/current capability owner replaces unframed legacy ABI. Exact modulo-zero, sizeof(pointer), U16 wrapping, ten-byte string overflow, AP wrong-interface and baud fallthrough defects are recorded in PU850-PARITY.
- Omitted/replaced code: `Generic panel/title/buttons/icons/language/clock/voltage/topology/file bulk/backpressure/cancel/session/diagnostic/logging`. Disposition: **pending**. Reason: Not business exclusions. Current low-level transport and HTTP backup do not prove these high-level paired capabilities; unwired power is Unknown, raw credential traces prohibited, file receive and diagnostic placeholders not claimed implemented.
- Omitted/replaced code: `Weight/hidden-weight/tare/receipt/report payloads only`. Disposition: **not-applicable**. Reason: Exact PU weighing/report business operations have no PakarController consumer; this does not exclude generic settings, file, UI or diagnostic code.
- Remaining: Not business exclusions. Current low-level transport and HTTP backup do not prove these high-level paired capabilities; unwired power is Unknown, raw credential traces prohibited, file receive and diagnostic placeholders not claimed implemented.

### PU850-ESP: `PRODUCT-PROFILES.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `b396143fd17590b35455f91fc17fc15bfb52862d3419198df10da62b86340aaa`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: identity/overrides; secret-safe flags; all unversioned transfer routes; range/ETag/hash-pair/downstream/redaction contracts
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Full cross-language uploader/download/resume/gzip/manifest compatibility`. Disposition: **pending**. Reason: Read and adapt every actual client/test; strict current auth/identity/integrity and supported-release policy remain authoritative. PU weighing/report identity is specifically out of scope.
- Remaining: Read and adapt every actual client/test; strict current auth/identity/integrity and supported-release policy remain authoritative. PU weighing/report identity is specifically out of scope.

### PU850-ESP: `ProductConfig.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `61ad4d13aa9416ac0dc2008722f2c092eceed521fd980407a23beff2258e684a`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: both profile IDs; every product/manufacturer/HTTP/WS/MNDP override; UPnP schema/service/cache/TTL/server; firmware/full-flash/bearer/redaction flags
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Unmapped generic compile-default and override knobs`. Disposition: **pending**. Reason: Reconcile actual discovery/transfer consumers individually. Preserve centralized Pakar identity, no absent SOAP/GENA advertisement or bearer bypass.
- Remaining: Reconcile actual discovery/transfer consumers individually. Preserve centralized Pakar identity, no absent SOAP/GENA advertisement or bearer bypass.

### PU850-ESP: `pu850-esp.ino`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `75d5492a67cc754e7e6617354016e549856aa015f93016460eed024534f4f3b1`.

- Target: `firmware/esp8266/src/NetworkConfig.cpp:230 [UTF8-LF sha256=c717c24898e0b02270eacbc764ebe2e02a4f3b91868b6fa717d16460f0d132cf; anchor=bool NetworkConfigStore::configure(]`.
- Target: `firmware/esp8266/src/NetworkPath.h:56 [UTF8-LF sha256=cab7eb5a6e7985dfb8e5120b19991b9d434bcb4f083d5509678636b5adbda662; anchor=inline bool service()]`.
- Target: `firmware/esp8266/src/EspFrontPanel.h:82 [UTF8-LF sha256=373453bd3730a23b62740ebcbd672d9de15bffac40068bb2e4868b4344ef8e04; anchor=Queries report the actual current connection/settings]`.
- Target: `firmware/esp32s3/src/Discovery.h:16 [UTF8-LF sha256=bb332589075e0cab611474f2faddf0024ba23734ce9ff27989c79984cb963769; anchor=mdnsReady_ = startDiscoveryMdns(]`.
- Target: `firmware/esp32s3/src/DiscoveryMdns.h:15 [UTF8-LF sha256=b7fc46c269164de2c618a3a95e76adff4e08660dcfd9df7d1fdf7038addbeb9a; anchor=bool startDiscoveryMdns(]`.
- Retained: Actual complete 65234-byte donor sketch read through EOF, not keyword-screened. WiFi_Setup, Settings_Read/Settings_Write, setNetFlags, hostname change and ConfirmSettings were compared to the integrated Pakar network/configuration owner. Persisted AP-only/STA-only/AP+STA, stable configured hostname without chip suffixes, station credentials, DHCP/static address/gateway/subnet/DNS and reconnect behavior map to NetworkConfigStore and NetworkPath; no second network service or PU business state is introduced.
- Retained: Actual donor startup/service ownership is adapted to the S3 checked mDNS/DNS-SD registration helper and existing SSDP/MNDP interface owners. Every required service/TXT registration is checked, partial initialization is retired before bounded retry, and failed registration is not reported ready. AP SSID is explicitly bounded to32 bytes while full DNS hostname remains unchanged; a failed STA configuration preserves genuinely started AP recovery.
- Retained: ESP settings are exposed through the same persisted network owner used by API/Web/native/mobile. The paired AVR front-panel adapter offers actual fresh mode/DHCP/connectivity/hostname preview and queues reconnect/scan; queries never replay stale snapshots or evict the previous mutation receipt. Scan admission is not misrepresented as completed SSID results.
- Retained: Donor storage writes are adapted to checksum/exact private-layout validation, identical-write suppression, pending-file readback and rename. Invalid enabled-empty SSIDs fail closed to AP recovery; static profiles are retained while DHCP is selected. Accepted default-route and resolver changes are distinct, avoiding cloud-session teardown on DNS-only refreshes.
- Retained: S3 latency policy adapts the actual donor WiFi_Setup wake/reconnect ownership to Arduino-ESP32 setSleep before and after interface startup: externally powered default explicitly disables modem sleep, with opt-in PAKAR_S3_WIFI_MODEM_SLEEP=1. Actual esp_wifi_get_ps readback is exposed separately from the requested policy; AP recovery is preserved even if setting latency policy fails. ESP8266 behavior and private persisted network layout are unchanged.
- Omitted/replaced code: `ASA0002E PU-specific EEPROM/business settings and device/product globals`. Disposition: **replaced**. Reason: Pakar has a different AVR/S3 protocol, matrix/game capabilities and private flash records. The reusable network identity/settings behavior is retained through current typed owners; blindly copying PU EEPROM addresses, serial/model state or business commands would corrupt identity/settings rather than add a supported feature.
- Omitted/replaced code: `Live configurable Telnet enable/port rebind and matching discovery lifecycle acceptance`. Disposition: **pending**. Reason: The device-shell slice now exposes durable enabled/port/color via typed API/cloud commands and shell properties, closes/rebinds sessions on the loop owner, and advertises only the actually bound port. Focused registration/settings fixtures do not establish real restart, network-loss and multi-client lifecycle acceptance across AP/STA/Ethernet or parity in every client settings UI.
- Omitted/replaced code: `S3 NBNS live acceptance, WINS/LLMNR compatibility and complete physical network editors`. Disposition: **pending**. Reason: The link-discovery slice implements bounded interface-bound S3 NB/NBSTAT replies; physical Windows/W5500 acceptance, WINS arbitration and LLMNR remain separate unfinished work. AVR front-panel mode/DHCP/reconnect/scan still does not implement full SSID/password/static-address text editors or completed scan-result navigation; those useful features remain open.
- Omitted/replaced code: `All generic sketch routes, cloud/time diagnostics, storage/control surfaces and complete donor-wide parity outside this network/front-panel slice`. Disposition: **pending**. Reason: Dedicated AsyncWebServer/AsyncOTA/FirmwareTransfer/Authentication/SSDP/MNDP comparisons preserve their own exact mappings and remaining acceptance. A complete sketch read is not an assertion that every route, shell action, network mode or generic tool was imported, deployed or physically accepted; each remaining behavior still requires its owning row and live evidence.
- Test-code evidence: `tests/network_config_test.cpp:95 [UTF8-LF sha256=f01572ae3609de976bd4fdf23a0ece575c8a3caf00636a3582afa4f130ca8979; anchor=AP remains genuinely available on STA apply failure]`.
- Test-code evidence: `tests/network_path_test.cpp:69 [UTF8-LF sha256=e1e1b8c23498d6b67bc8334f4c8d4c73bf88d83f7373930d11f90d8318d8ec69; anchor=state().routeChanges==fallbackRoutes]`.
- Test-code evidence: `tests/esp_front_panel_test.cpp:94 [UTF8-LF sha256=58220f795fcb6d16277308285f37f7cede5af7f431ceafd675ad8bfe6f3c5013; anchor=f=request(E::Scan);f.sequence=12;service.handle(f,110]`.
- Test-code evidence: `tests/discovery_mdns_test.cpp:46 [UTF8-LF sha256=bfeb2db3ba40c7a00d30258154b008a02c3110894793d12b1382b7825cb1d88e; anchor=for(unsigned at=1;at<=14;++at)]`.
- Remaining: Issue #2: verify persisted AP/STA/both across reboots, actual authorized DRS Tether scan/association, hostname/DHCP/static behavior and recovery without exposing credentials. Source fixtures do not prove RF, association or user visibility.
- Remaining: Verify real AP/STA/Ethernet mDNS/DNS-SD/SSDP/MNDP discovery/rename and HTTP/WebSocket service under concurrent matrix/cloud traffic; resolve intermittent HTTP response loss before production acceptance. Complete the useful Telnet lifecycle, NetBIOS/LLMNR and physical text/scan editors rather than certifying whole-sketch parity.

### PU850-ESP: `sketch.yaml`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `54df8b54e56a589f85b981f15996813e447fc14245ea8048c5f87cfd1f3fdf51`.

- Target: `tools/firmware-dependencies.json:2 [UTF8-LF sha256=4a7dc6ef881b1100856419608f5eceaf5c748a7a890c1834fa48250985efe3c1; anchor="platforms"]`.
- Target: `tools/build-firmware.ps1:130 [UTF8-LF sha256=24584210e1f306e53c78f1db8b52ed0d7765d0950eb67b4034ff41f284bb39e7; anchor=esp32:esp32:esp32s3:CDCOnBoot]`.
- Retained: Read complete donor board profile/platform/library locks. Retain exact ESP8266 core/ArduinoJson/ESP Async TCP pins where applicable, preserve existing reviewed Pakar dependency pins, and keep actual S3/AVR FQBN/flags in the canonical wrapper.
- Omitted/replaced code: `PU-specific board/profile and separate dependency/config write paths`. Disposition: **replaced**. Reason: Reuse applicable exact versions/official registries in one reviewed pin owner. Preserve operator config and Pakar hardware flags; no silent downgrade, duplicate Arduino_JSON API, unsupported library additions or automatic SDK initialization.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run live registry check and review any candidate version changes with full build/hardware acceptance; pins were not upgraded by this slice.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Temp/Always.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `08fc271ee313bced76acf687e782aacd8eb03517e440910ff591c62aaedd0adf`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: all scalar/pointer/flash/BOOL aliases/sentinels; High/Low/IsHigh/IsLow/byte/word/Pack/ReverseOrder/Null macros
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic byte packing reconciliation`. Disposition: **pending**. Reason: Use existing stdint/typed COBS owners instead of width-dependent aliases and repeated-evaluation/token-fragment macros; full UART function review still required.
- Remaining: Use existing stdint/typed COBS owners instead of width-dependent aliases and repeated-evaluation/token-fragment macros; full UART function review still required.

### PU850-ESP: `Temp/ChartIndex.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `f7b7cf3dd40a64c4de41420a0bdd7ce8aa4685ed895b30e7f7b838cb009069c7`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: all access roles/chart classes/menu/form/list/page/reserved indices
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic Wi-Fi/settings/diagnostics/terminal/password/clock screen behavior`. Disposition: **pending**. Reason: Reconcile actual UI/session/runtime owners; only exact weighing/report/printer/calibration charts are PU business. Do not copy donor role credential hints.
- Remaining: Reconcile actual UI/session/runtime owners; only exact weighing/report/printer/calibration charts are PU business. Do not copy donor role credential hints.

### PU850-ESP: `Temp/CommonDefine.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `e78774e8b164205479d73ec5bee1651160140d3fc3e5f0e204e6ff127fb958ca`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: all generic readiness/result/calendar/toggle/comparison/format constants; ADC/weigh/calibration/DLC commands/limits/brands/status/factors
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic readiness/result/calendar/format semantics`. Disposition: **pending**. Reason: Map real typed consumers in UART/clock review; only exact load-cell/calibration/weighing command groups are PU business exclusions.
- Remaining: Map real typed consumers in UART/clock review; only exact load-cell/calibration/weighing command groups are PU business exclusions.

### PU850-ESP: `Temp/ESP_Master_Common.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `33789905b19368d1f35d133fc38e16e8530e8841500ddfab5a0e5dcc2de421cf`.

- Target: `docs/PU850-PARITY.md:307 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: foundation slice, 2026-10-19]`.
- Retained: Every section/function read through EOF: revision/icons/network/internet/control tables; every prefix/numeric/string/request/order/bulk/file/LCD/diagnostic/programming suffix; groups/HTTP/results/baud/ResultPack
- Retained: Function-by-function disposition and exact remaining behaviors are recorded in docs/PU850-PARITY.md foundation slice.
- Omitted/replaced code: `Generic network/clock/message/file/LCD/session/diagnostic/programming semantics`. Disposition: **pending**. Reason: Complete UART function review and import useful current authenticated behavior; a second legacy parser/version selector is replaced by living COBS/capability/ACK. Only weight/tare/receipt/report payloads are business exclusions.
- Remaining: Complete UART function review and import useful current authenticated behavior; a second legacy parser/version selector is replaced by living COBS/capability/ACK. Only weight/tare/receipt/report payloads are business exclusions.

### PU850-ESP: `Tool/build-all.ps1`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `441ecba0c01223a1b91679cb531fccb37bb339b1938f196524489afdc53fa5b2`.

- Target: `tools/script-runtime.ps1:12 [UTF8-LF sha256=6fbe86a1ba2d5347abd1b70c4ab3dfee91f4dd7a58415324d9d291232ca07fcc; anchor=function Invoke-PakarNative]`.
- Target: `tools/pakar.mjs:10 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export const commands]`.
- Target: `tools/build-all.ps1:6 [UTF8-LF sha256=8175e248b0a79227467dc334dac932454df4b59d48588673f8b9446b086f31a3; anchor=$taskSelected=[Collections.Generic.HashSet[string]]]`.
- Retained: Import actual case-insensitive HashSet selection, Write-Step, Assert-Executable, Invoke-Native, scoped working directory and elapsed result. Typed hashtable splatting selects Firmware/Desktop/Mobile, defaults to plans and stops on failure.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C#/build.ps1`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `f3e6a8ed2eae52c8ab7f1f178b85c3a1fe4ad2a85bce2a27a67a0b1c07a24d6c`.

- Target: `tools/build-desktop.ps1:81 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Invoke-PakarNative -WorkingDirectory]`.
- Target: `tools/firmware-report.mjs:22 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function collectArtifacts]`.
- Retained: Adapt actual native-command failure preservation and build/test/publish stage dispatch into script-runtime.ps1 and build-all.ps1. Existing Rust/Flutter owners remain responsible for Pakar client outputs, with an explicit Release/preview choice.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C#/README.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0f40d6726756d86f30b82dac4fbe8fe11a5dc85ecba2cf8d68f5cb6771da0229`.

- Target: `docs/SCRIPT-TOOLING.md:94 [UTF8-LF sha256=9920569e7027308d50babfb2fd7144e1b9580d699053e7279197eebac31bb909; anchor=Actual PU850 source import]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware]`.
- Retained: Read full .NET documented upload/download/watch/backup/timeouts and platform outputs. Map one-shot preflight/identity/postboot behavior to the existing updater; document remaining download/resume/watch and package gaps rather than substituting a second client.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C#/Uploader/ConsoleUi.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `727fcb1631802567cabff2c5212bc649496bfcbf39e344015653a40629705c9b`.

- Target: `tools/script-console.mjs:14 [UTF8-LF sha256=2243be58e4dce21a7592865121e94ebdb64f21286f3333358c8259bb2bdedafb; anchor=export function createLogger(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: All fields/constructors and heading/info/success/warning/error/detail/upload/download/transfer/poll/clear/verbose/write/sanitize/byte/color methods
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `TransferProgress independent streams/redirected10percent/bar; PollProgress spinner`. Disposition: **pending**. Reason: Redirected progress buckets/bar/spinner remain useful pending. Actual C# independent stdout/stderr color policy is now imported into current Node logger with both redirection directions and NO_COLOR/TERM=dumb/JSON fixtures; stronger control/OSC sanitation retained.
- Test-code evidence: `tests/script_tooling_test.mjs:21 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=const independent=createLogger(]`.
- Remaining: Redirected progress buckets/bar/spinner remain useful pending. Actual C# independent stdout/stderr color policy is now imported into current Node logger with both redirection directions and NO_COLOR/TERM=dumb/JSON fixtures; stronger control/OSC sanitation retained.

### PU850-ESP: `Tool/C#/Uploader/DownloadApp.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `14c0831141b761e303110955ba0141bc683b0d80c18890b842dce6a65f77d920`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: RunAsync/DownloadOnceAsync/Finish/ReportFailure every retry/hash/commit/error branch
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `DownloadOnceAsync progress/error categories and remaining transport/platform acceptance`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Explicit verified backup-before-update and resumable export now imported. Remaining stable watcher/coalescing/graceful stop/skip-identical/progress, gzip consumer/platform/filesystem/transport and unported negative scenarios need applicable implementation/acceptance; no source fixture closes live/physical/restore gates.

### PU850-ESP: `Tool/C#/Uploader/DownloadClient.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `aa03832b78921b5bd2d89aaf6cce4cadb26a72f940dbc3da3ea7cefcef7443fe`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Constructor/GetMetadataAsync/DownloadAsync/ExtractExpectedMd5/EnsureHashesAgree/ValidateDownloadResponse/AddAsciiMd5Header/ValidateExpectedHash/IsMd5/ComputeMd5Async/CreateClient/CreateTimeout; records/exceptions
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `GetMetadataAsync Accept-Ranges advertisement check and real transport/storage acceptance`. Disposition: **pending**. Reason: Current backup validates hashes/lengths/same peer and never truncates unknown non-resume partials. Actual advertised range support and real storage/transport/proxy/redirect acceptance remain pending; no second HTTP runtime.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Remaining: Current backup validates hashes/lengths/same peer and never truncates unknown non-resume partials. Actual advertised range support and real storage/transport/proxy/redirect acceptance remain pending; no second HTTP runtime.

### PU850-ESP: `Tool/C#/Uploader/EndpointResolver.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `dbb00d1703e46147b950b67837667de78c9c2b4b45899ed5bfd9187663a1ae2a`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: All resolver/cache/local-DNS/ResolveAsync branches plus ResolvedEndpoint Rebase/UriAt/UpdateInfoUri/InfoUri
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `ResolveAsync IPv6/non-local host and legacy platform behavior`. Disposition: **pending**. Reason: IPv6/non-.local resolver/legacy platform acceptance remains pending; current same-origin hostname/IPv4/TCP/board/boot admission refuses route changes/replay.
- Remaining: IPv6/non-.local resolver/legacy platform acceptance remains pending; current same-origin hostname/IPv4/TCP/board/boot admission refuses route changes/replay.

### PU850-ESP: `Tool/C#/Uploader/FirmwareManifest.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `ea82a43da0bb2f5f98687b4cbd1a9b343faa636ccd8b536cd31e687dbcdd5b91`.

- Target: `tools/firmware-report.mjs:34 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=const gz=portableGzip(bytes),manifest=createFirmwareManifest(]`.
- Target: `tools/firmware-manifest.mjs:14 [UTF8-LF sha256=fd8a77a38ee5f1b6ca8fc3df3f77cb2790bf4fc747fc22dd85315b5da113ac7c; anchor=export function parseFirmwareManifest(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: All LoadAsync/SelectFor/ComputeMd5Async/ComputeFirmwareHashesAsync/PortableFileName/generated regex methods, entries/hash/limit records and exceptions
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Streamed FileStream/GZipStream wrappers; complete paired distribution/ESP8266 gzip upload/interruption acceptance`. Disposition: **pending**. Reason: Actual parse/portable matching/strict two records/both hashes/bounded gzip code ported into real explicit ESP8266 artifact compression. Full pair distribution/ESP8266 gzip upload/interrupted packaging acceptance remains pending.
- Test-code evidence: `tests/firmware_manifest_test.mjs:10 [UTF8-LF sha256=13344971c71fd41e84781673467003a181999106dccec9d4f4a5863eabd7546b; anchor=const content=hash(raw).toUpperCase()]`.
- Remaining: Actual parse/portable matching/strict two records/both hashes/bounded gzip code ported into real explicit ESP8266 artifact compression. Full pair distribution/ESP8266 gzip upload/interrupted packaging acceptance remains pending.

### PU850-ESP: `Tool/C#/Uploader/InfoParser.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d09ab4bcd65490aa41ffa8ae078f8052f41cce5d368480264538b14e62117e78`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Parse, all hash-selector overloads, IsAsciiHex and FlattenJson with every value-kind/fallback branch
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Parse/FlattenJson display-only scalar inspection and legacy text parsing`. Disposition: **pending**. Reason: Display-only scalar flattening/legacy text inspection remain useful pending; critical identity/hash uses exact current typed JSON instead of ambiguous nested/fuzzy leaf/fallback parsing.
- Remaining: Display-only scalar flattening/legacy text inspection remain useful pending; critical identity/hash uses exact current typed JSON instead of ambiguous nested/fuzzy leaf/fallback parsing.

### PU850-ESP: `Tool/C#/Uploader/Program.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c1f4bc597c546c6003edc4075e77681c9dac51ab500e9e71bdefdb50c7e6cc74`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Main every help/selftest/upload/download/watch/cancel/exception/finally branch plus FirstEnvironment
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Main watcher graceful-stop/hard-cancel lifecycle and actual process acceptance`. Disposition: **pending**. Reason: Current dispatcher/signal owner/JSON and stderr handling remain. Graceful watcher stop/process cancellation acceptance is pending; no encoding preference write, bearer aliases or active watcher copied.
- Remaining: Current dispatcher/signal owner/JSON and stderr handling remain. Graceful watcher stop/process cancellation acceptance is pending; no encoding preference write, bearer aliases or active watcher copied.

### PU850-ESP: `Tool/C#/Uploader/SelfTests.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `2429a0837fab902db2d834fc513ee41dd21418722067c67fc46cb478c459677b`.

- Target: `tools/firmware-report.mjs:34 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=const gz=portableGzip(bytes),manifest=createFirmwareManifest(]`.
- Target: `tests/firmware_manifest_test.mjs:10 [UTF8-LF sha256=13344971c71fd41e84781673467003a181999106dccec9d4f4a5863eabd7546b; anchor=const content=hash(raw).toUpperCase()]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: All20top-level tests; every helper/assertion; both FakeFirmwareServer/FakeDownloadServer constructors/routes/request/response/read/write/dispose/properties and TestHttpRequest
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `TestWatchStateMachine/TestWatchStopController/TestPreStoppedWatchRunnerAsync plus remaining real TCP redirect/oversize/streaming fixtures`. Disposition: **pending**. Reason: Actual TestManifestAsync cases ported to shipped source plus bounded/corrupt output and real report consumer. Remaining watcher/graceful-stop/real-TCP oversized/redirect scenarios need applicable ports; donor tests not executed as target proof.
- Test-code evidence: `tests/firmware_manifest_test.mjs:10 [UTF8-LF sha256=13344971c71fd41e84781673467003a181999106dccec9d4f4a5863eabd7546b; anchor=const content=hash(raw).toUpperCase()]`.
- Remaining: Actual TestManifestAsync cases ported to shipped source plus bounded/corrupt output and real report consumer. Remaining watcher/graceful-stop/real-TCP oversized/redirect scenarios need applicable ports; donor tests not executed as target proof.

### PU850-ESP: `Tool/C#/Uploader/UploadClient.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `55ba96cd5363807bfddd4bcc81191c93738183acdb503ae929b1350b7b79f9f9`.

- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Constructor/UploadAsync/GetFirmwareInfoAsync/IsAcceptedResponse/CreateClient/ApplyHeaders/Truncate/RequestInfoAsync/ReadLimitedStringAsync/AppendReplaySafety/Quote; progress/serialization content and all exception classes
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `ProgressFileContent streaming and CommitTrackingMultipartFormDataContent body-start tracking`. Disposition: **pending**. Reason: Current exact raw-S3/new-boot/slot/full SHA/JSON admission remains stricter, no write replay. Applicable bounded streaming/multipart progress and request-body-start tracking remain pending; no arbitrary protected header overrides.
- Remaining: Current exact raw-S3/new-boot/slot/full SHA/JSON admission remains stricter, no write replay. Applicable bounded streaming/multipart progress and request-body-start tracking remain pending; no arbitrary protected header overrides.

### PU850-ESP: `Tool/C#/Uploader/Uploader.csproj`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `270b70fc066996277eeedcd5bb7a4f9eb2fc8278027a1711c4d615a921ea68ac`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Every SDK/output/target-framework/using/nullability/identity/version/warning/debug/determinism field
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Second .NET8 executable/runtime/assembly/debug build owner`. Disposition: **replaced**. Reason: One existing Node/PowerShell owner serves actual port; duplicate .NET8 product build/runtime/debug payload is unnecessary, not claimed binary-distribution acceptance.

### PU850-ESP: `Tool/C#/Uploader/Uploader.sln`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `34506a7f51ff0c6bd62509b9322a8f6178971ba2e67b3953829ae9835f600977`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Complete format/VS/project/GUID/Debug/Release/AnyCPU and solution properties
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Donor solution/project GUID and Debug/Release/AnyCPU build organization`. Disposition: **replaced**. Reason: Current guarded scripts/editor process tasks own scoped operations; donor solution/GUID organization has no additional runtime implementation to copy.

### PU850-ESP: `Tool/C#/Uploader/UploaderApp.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `eaf663a9f81dc3372a378ab6332177c7b17d22170020b572a7c865575eada68f`.

- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: RunAsync all preflight/manifest/hash/backup/skip/upload/retry/verify branches; VerifyAfterRebootAsync/WriteUsefulInfo/BackupInstalledFirmwareAsync; SynchronousProgress.Report and UploaderRuntime
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `SkipIdentical policy and WriteUsefulInfo/progress consumer`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Explicit verified backup-before-update and resumable export now imported. Remaining stable watcher/coalescing/graceful stop/skip-identical/progress, gzip consumer/platform/filesystem/transport and unported negative scenarios need applicable implementation/acceptance; no source fixture closes live/physical/restore gates.

### PU850-ESP: `Tool/C#/Uploader/UploaderOptions.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `dd61d42256dfa14782a6495794508fe240c7c57135316c4fa6332fbf0168234e`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: All enums/records/options/defaults/parse branches, URL/header/time/value validators, help/derived URL and ParseResult factories
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Parse applicable connect/upload/download/reboot/poll/debounce/help/verbose/backup/watch options`. Disposition: **pending**. Reason: Applicable timeout/help/verbose/backup/watch parity remains pending. Current explicit-run/strict URL/auth/identity/integrity owners reject no-verify/bearer aliases/arbitrary critical headers.
- Remaining: Applicable timeout/help/verbose/backup/watch parity remains pending. Current explicit-run/strict URL/auth/identity/integrity owners reject no-verify/bearer aliases/arbitrary critical headers.

### PU850-ESP: `Tool/C#/Uploader/WatchUploader.cs`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `53c27d11d347092c0addd510ac2de5e47ecd2e7ad7fa1dd26dbd4e1a7265ad54`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Every WatchUploader, WatchStopController, BuildWatchMonitor, WatchStateMachine, observation/stable/stamp/identity member and branch
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `WatchStopController/BuildWatchMonitor/WatchStateMachine complete optional watcher lifecycle`. Disposition: **pending**. Reason: Generic event-plus-poll/generation/debounce/coalesce/graceful-stop/operation ownership/status/disposal remains useful pending real optional watcher. Existing hash+file-stability backup guard retained; metadata-only polling must not replace it.
- Remaining: Generic event-plus-poll/generation/debounce/coalesce/graceful-stop/operation ownership/status/disposal remains useful pending real optional watcher. Existing hash+file-stability backup guard retained; metadata-only polling must not replace it.

### PU850-ESP: `Tool/C++/build.ps1`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `7379ae908f01676ed3db145f11579bb2bf07b79a7b5da2bdfe3cc8baa172e297`.

- Target: `tools/build-desktop.ps1:81 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Invoke-PakarNative -WorkingDirectory]`.
- Target: `tools/firmware-report.mjs:22 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function collectArtifacts]`.
- Retained: Retain executable preflight, bounded native failure handling and explicit selected component dispatch in shared PowerShell helpers. Windows package/capture utilities remain explicitly platform-specific, not advertised as Unix binaries.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C++/build.sh`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0af1f4b0ba2a7014d7f0938f69885b7d7a46763513408058cffeab4772d5827d`.

- Target: `tools/build-desktop.ps1:81 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Invoke-PakarNative -WorkingDirectory]`.
- Target: `tools/firmware-report.mjs:22 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function collectArtifacts]`.
- Retained: Retain Bash preflight/delegation to an explicit component owner. pakar.sh shares dispatch with Windows and CI; a MinGW cross-build of WinHTTP is not represented as a Linux-native Pakar application.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C++/cmake/mingw-w64-x64.cmake`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `8f376d6737245386a856b2c3b568a4281ec1cff5848db24d7eb4b471dbf2b1df`.

- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: System/processor/C/C++/RC compilers and all four root search modes
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `x64 target compilers/root-path-only dependency isolation and native distribution`. Disposition: **pending**. Reason: Existing explicit host/runtime owners replace duplicate cross-build; x64 native packaging/target-only dependency search acceptance remains pending if required.
- Remaining: Existing explicit host/runtime owners replace duplicate cross-build; x64 native packaging/target-only dependency search acceptance remains pending if required.

### PU850-ESP: `Tool/C++/cmake/mingw-w64-x86.cmake`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `5850c058156b64c0db96561a907c869fdcf53d939abf62c326dda8a34d1de5b5`.

- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: System/processor/C/C++/RC compilers and all four root search modes
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `x86 target compilers/root-path-only dependency isolation and native distribution`. Disposition: **pending**. Reason: Existing explicit host/runtime owners replace duplicate cross-build; x86 native packaging/target-only dependency search acceptance remains pending if required.
- Remaining: Existing explicit host/runtime owners replace duplicate cross-build; x86 native packaging/target-only dependency search acceptance remains pending if required.

### PU850-ESP: `Tool/C++/CMakeLists.txt`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0fb5ee784250ed0eb356db4ba273c9dcf6eddffc9bd8466af87f5a0d7b83d2cb`.

- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Every platform guard/executable/standards/defines/library/MSVC/MinGW option; CTest/selftest/Python/integration/timeout/install stanza
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Windows WinHTTP/BCrypt/Winsock/Win7 native executable and distribution build contract`. Disposition: **pending**. Reason: Windows WinHTTP/BCrypt/Winsock/Win7/native binary distribution is not silently equated with Node. Existing guarded runtime/CI serves actual port; exact legacy-OS/native-distribution acceptance remains pending, no extra compiler/cache.
- Remaining: Windows WinHTTP/BCrypt/Winsock/Win7/native binary distribution is not silently equated with Node. Existing guarded runtime/CI serves actual port; exact legacy-OS/native-distribution acceptance remains pending, no extra compiler/cache.

### PU850-ESP: `Tool/C++/puff.c`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `5b9d75aeb5baf3575415bc6ade3f2a02e50b6b971b3f8b4fda2b03543bc6e52f`.

- Target: `tools/firmware-manifest.mjs:14 [UTF8-LF sha256=fd8a77a38ee5f1b6ca8fc3df3f77cb2790bf4fc747fc22dd85315b5da113ac7c; anchor=export function parseFirmwareManifest(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Full history/format notes/includes/limits/state; bits/stored/both SLOW and fast decode/huffman/construct/codes/fixed/dynamic/puff/invalid-distance option/error contracts
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `bits/stored/decode/construct/codes/fixed/dynamic/puff raw-deflate scan/error corpus contract`. Disposition: **pending**. Reason: Node zlib supplies bounded gzip decoder with corrupt/empty/output-bound fixture, not another longjmp/static virgin-table decoder. Full Huffman/raw-deflate scan/corpus parity remains pending; no unsafe-distance option copied.
- Test-code evidence: `tests/firmware_manifest_test.mjs:10 [UTF8-LF sha256=13344971c71fd41e84781673467003a181999106dccec9d4f4a5863eabd7546b; anchor=const content=hash(raw).toUpperCase()]`.
- Remaining: Node zlib supplies bounded gzip decoder with corrupt/empty/output-bound fixture, not another longjmp/static virgin-table decoder. Full Huffman/raw-deflate scan/corpus parity remains pending; no unsafe-distance option copied.

### PU850-ESP: `Tool/C++/puff.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `969b7be2a930db0cdcb19b0e5b29ae6741f5a8f663b6dba6d647e12ec60cfa8e`.

- Target: `tools/firmware-manifest.mjs:14 [UTF8-LF sha256=fd8a77a38ee5f1b6ca8fc3df3f77cb2790bf4fc747fc22dd85315b5da113ac7c; anchor=export function parseFirmwareManifest(]`.
- Target: `docs/PU850-PARITY.md:196 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C# manifest and deflate slice, 2026-10-19]`.
- Retained: Entire file/sections read through EOF: Entire Mark Adler license/version/NIL/prototype/input-output contract
- Retained: Exact function dispositions are in the C# manifest/deflate slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `NIL/raw-deflate scan/prototype compatibility`. Disposition: **pending**. Reason: No puff code redistributed; original license retained in read-only donor, not relabeled MIT. Existing Node zlib is bounded decoder; raw-deflate scan/corpus parity needs real consumer/proof.
- Test-code evidence: `tests/firmware_manifest_test.mjs:10 [UTF8-LF sha256=13344971c71fd41e84781673467003a181999106dccec9d4f4a5863eabd7546b; anchor=const content=hash(raw).toUpperCase()]`.
- Remaining: No puff code redistributed; original license retained in read-only donor, not relabeled MIT. Existing Node zlib is bounded decoder; raw-deflate scan/corpus parity needs real consumer/proof.

### PU850-ESP: `Tool/C++/README.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `92c5355d3cbb855acbb60352b9250926e94856905a7845ecaf541489db5bc2d7`.

- Target: `docs/SCRIPT-TOOLING.md:94 [UTF8-LF sha256=9920569e7027308d50babfb2fd7144e1b9580d699053e7279197eebac31bb909; anchor=Actual PU850 source import]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware]`.
- Retained: Read full WinHTTP/puff/gzip/manifest, cached DNS, no-replay, atomic resume, auth and exit contracts. Retain host identity/no-replay/integrity concepts; clearly distinguish Windows cross-builds from actual Linux-native outputs.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C++/tests/integration_test.py`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `176dabcb53d4b1db79c4493b08958e1d1b0546151f6046603e589bd534308bb9`.

- Target: `tests/script_tooling_test.mjs:73 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=const cliPlan=]`.
- Target: `tests/update_tool_test.mjs:193 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=await uploadImage(origin,peer,image,identity)]`.
- Retained: Adapt loopback/subprocess fixture structure and fail-before-contact/integrity assertions without requiring a WinHTTP executable or Python test server. Existing updater fixtures exercise actual multipart bytes and fragmented WS; new CLI subprocess fixtures exercise offline/invalid argument behavior.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Test-code evidence: `tests/update_tool_test.mjs:246 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=Automatic updater plan/preflight]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/C++/upload.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `b5f810636d9acbbe8df4079acf10f00cf7aa71d0130023bb558205d146e69fa2`.

- Target: `tools/download-firmware.mjs:22 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export function parseDigestMD5(]`.
- Target: `tools/check-ethernet-live.mjs:48 [UTF8-LF sha256=33e72a4a0a29bbbaba321485b5c98bb1936e2c1e2f58fed1e66d22e2a83905c1; anchor=distinctHeaders:response.headersDistinct]`.
- Target: `docs/PU850-PARITY.md:141 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: C++ transfer and UART, 2026-10-19]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Actual single_header/parse_header_md5/collect_download_hashes implementation adapted for critical duplicate rejection and canonical full Digest-MD5; range Content-MD5 remains section integrity.
- Retained: Complete section/function decisions and concrete unsafe legacy defects recorded in the C++ transfer and UART section of docs/PU850-PARITY.md.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `WinHTTP/BCrypt/UTF8/console/metadata selection and async close lifecycle`. Disposition: **replaced**. Reason: Existing Node request/crypto/logger and exact typed identity owner retain finite deadlines, hostname-preserving pinned TLS and stronger sanitation. Donor final HANDLE_CLOSING wait is unbounded; do not copy it.
- Omitted/replaced code: `verified_download/replace/truncate/delete/retry/no-verify branches`. Disposition: **replaced**. Reason: Mandatory integrity, explicit resume, exclusive stable partial descriptor and no-replace atomic publication retain failures and existing backups; no ambiguous retry or no-hash success.
- Omitted/replaced code: `Multipart streaming/progress/watch/coalescing/IPv6/native console and complete gzip corpus`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/download_firmware_test.mjs:33 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=assert.equal(observed.distinctHeaders.etag.length,2);]`.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Useful generic behavior remains pending integration and actual filesystem/transport/platform acceptance; no donor/native build or live operation performed.

### PU850-ESP: `Tool/Go/build.ps1`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `7a7d58167051cdfa2bab660a4128d0b29400c17a653bdf751226f86607da246a`.

- Target: `tools/build-desktop.ps1:81 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Invoke-PakarNative -WorkingDirectory]`.
- Target: `tools/firmware-report.mjs:22 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function collectArtifacts]`.
- Retained: Adapt actual selected-target orchestration, tool preflight and raw native exit preservation without installing a duplicate Go application stack. Existing Rust checks/tests/Release and Flutter preview remain explicit.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/Go/build.sh`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `746e6667fba804bce07d90acf6bf370e0f7b62100c83d806cf057ef77f600ff7`.

- Target: `tools/build-desktop.ps1:81 [UTF8-LF sha256=8ebd02ebcd5556bd70f97216faed6ea3c03ec8e92950514118e1b9901132e037; anchor=Invoke-PakarNative -WorkingDirectory]`.
- Target: `tools/firmware-report.mjs:22 [UTF8-LF sha256=fbe1f3f238379cf514626d1a46232ce0c22086417b79aa14c6e82939edeb5698; anchor=export function collectArtifacts]`.
- Retained: Retain shell launcher preflight and one delegated owner with explicit testing/build selection. Pakar Bash and Windows now reach identical guarded component wrappers rather than separate drifting build recipes.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Omitted/replaced code: `Forced cleans/no-cache recompiles, PU profile/name, timestamp/personal build headers and audible failure side effects`. Disposition: **replaced**. Reason: Use existing Pakar board/link flags and generated-file checks, stable warm output/cache paths and quiet explicit actions. Do not wear SSDs, alter identity or start devices as an implicit build action.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: Run configured OS-matrix command plans and affected real SDK builds on their matching hosts.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/Go/console_other.go`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c6d5b649d1ff9ceb7469a7dad0f1020e6fb0e085fed9b94dda438660f5c451b6`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: enableVirtualTerminal/isCharacterDevice call
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Go terminal runtime copy`. Disposition: **replaced**. Reason: Existing Node TTY/NO_COLOR/TERM ownership serves script terminal detection without a duplicate Go runtime or terminal mutation.

### PU850-ESP: `Tool/Go/console_windows.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `fb558b089a87dd62ba560db249402d424428c45f50c1286c656de4b8f8a9b440`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: VT flag/kernel32 procedures; complete enableVirtualTerminal GetConsoleMode/SetConsoleMode flow
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Legacy Windows console VT acceptance`. Disposition: **pending**. Reason: Current script logger and native CLI own terminal setup; prove actual older Windows VT behavior before closing this generic gap.
- Remaining: Current script logger and native CLI own terminal setup; prove actual older Windows VT behavior before closing this generic gap.

### PU850-ESP: `Tool/Go/download_test.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c33bfe1e8ebfabbf03db06a96ced7e08045599cbc1bda7592b04cdd5a3942fc7`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Complete EOF read: every test function/helper and all HTTP branches
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `Raw skip/fallback-specific and remaining applicable fixtures`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Actual byte verification, exact resume and mismatch/atomic preservation scenarios ported into shipped Node fixture plus multi-range/peer/release/cancel/time/stale partial/no-overwrite cases. Remaining uploader/gzip behaviors await real implementation; donor tests are not claimed executed.

### PU850-ESP: `Tool/Go/download.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `06cd1dd3150f7b0bd7f9b57f90a1ae3731767374d43527de296bdf91be3b1185`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Actual Range/length/hash/resume/partial/commit flow adapted into tools/download-firmware.mjs and invoked by guarded wrapper/pakar download.
- Retained: Complete EOF read: all constants/regex/types/functions
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Other generic client transfer modes/real network acceptance`. Disposition: **pending**. Reason: Actual metadata/hash/range/resume/part/verification flow ported to shipped backup tool. Current exact metadata, Basic auth, release/board/boot/TCP fencing replace fuzzy aliases, arbitrary second origin and bearer override; live authenticated LAN/Windows/storage acceptance remains open.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Remaining: Actual metadata/hash/range/resume/part/verification flow ported to shipped backup tool. Current exact metadata, Basic auth, release/board/boot/TCP fencing replace fuzzy aliases, arbitrary second origin and bearer override; live authenticated LAN/Windows/storage acceptance remains open.

### PU850-ESP: `Tool/Go/go.mod`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0a7dadd87990be9f31ff22f3c056e96f8210540cd2bf1c637c84bbc756f63ed8`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: module path and go1.22; no third-party dependencies
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Additional Go application module/runtime`. Disposition: **replaced**. Reason: Use the existing Node/PowerShell client dispatcher for actual ported code; no donor build or new toolchain/dependency/cache.

### PU850-ESP: `Tool/Go/main_test.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `c3efa93383cc376123f500e279bc16686d611229141157162e750b44813cf79e`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: all nine tests/all HTTP branches
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Applicable gzip/manifest/env-priority/filename tests`. Disposition: **pending**. Reason: Existing script/update fixtures cover current help/plan/multipart/new-boot/secret/exit behavior; port remaining scenarios with actual generic implementation. Bearer aliases/fuzzy scalar suffixes are replaced by exact current admission; no donor test execution claim.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Remaining: Existing script/update fixtures cover current help/plan/multipart/new-boot/secret/exit behavior; port remaining scenarios with actual generic implementation. Bearer aliases/fuzzy scalar suffixes are replaced by exact current admission; no donor test execution claim.

### PU850-ESP: `Tool/Go/main.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d44049897ca5c86fb3748c42f1871a9f066ae2f665951393b2502250249f7229`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Complete EOF read: all imports/constants/types/options and every main/CLI/endpoint/HTTP/upload/manifest/hash/gzip/multipart/reboot/info/console/progress function
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `Applicable generic gzip/paired manifests/watch/progress refinements`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Explicit verified backup-before-update and resumable export now imported. Remaining stable watcher/coalescing/graceful stop/skip-identical/progress, gzip consumer/platform/filesystem/transport and unported negative scenarios need applicable implementation/acceptance; no source fixture closes live/physical/restore gates.

### PU850-ESP: `Tool/Go/README.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d9142cd0128d3b963259c9eaaf5afc179c120d232f9d80d6e401591ecac6bb40`.

- Target: `docs/SCRIPT-TOOLING.md:94 [UTF8-LF sha256=9920569e7027308d50babfb2fd7144e1b9580d699053e7279197eebac31bb909; anchor=Actual PU850 source import]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware]`.
- Retained: Read full standard-library client, atomic verified download, backup/resume/watch, auth and multi-target build contract. Retain bounded verification and hostname caching intent in the existing updater; no claim that read-only export loops are a resumable CLI backup command.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Omitted/replaced code: `Build and distribute parallel .NET/WinHTTP/Go ASA clients`. Disposition: **replaced**. Reason: Pakar already owns Rust and Flutter applications and the standard-library Node updater. Avoid three duplicate client stacks/SDK caches; reusable command/preflight/transfer contracts are mapped separately.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: Verify full native distributable/platform parity; Windows cross-build output is not Linux-native execution.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/Go/replace_other.go`

Verdict: **equivalent-behavior**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `b9c6d47bb4072251c72002984676b420794a4e05c3c056a19afb4b4b3b2557b5`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: atomicReplace/os.Rename
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Replace-existing rename`. Disposition: **replaced**. Reason: Actual download commit uses atomic no-overwrite link+unlink on its verified private partial; existing destination and failed partial are preserved. No delete fallback.

### PU850-ESP: `Tool/Go/replace_windows.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `892dbd97703bb64d5c2ff3b6916a808350085eb034b5a7583ae5416e04a698bf`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: MoveFileExW flags/procedure/UTF16 conversion/atomicReplace
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `WRITE_THROUGH durability and unsupported filesystem acceptance`. Disposition: **pending**. Reason: Current atomic no-overwrite commit avoids replacing another backup. Hard-link unsupported filesystems fail retaining partial; power-loss/write-through durability is not certified.
- Remaining: Current atomic no-overwrite commit avoids replacing another backup. Hard-link unsupported filesystems fail retaining partial; power-loss/write-through durability is not certified.

### PU850-ESP: `Tool/Go/watch_test.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `ed25ba3b659d148914bc58ef87cf5fa5d63cb68966ce26b3d40108965888fd91`.

- Target: `tools/pakar.mjs:59 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export async function main(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: all nine tests plus writeWatchBuild/rewriteWatchBuild and each file/sentinel branch
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Remaining generic stable-watch scenarios`. Disposition: **pending**. Reason: Actual changed-content guard is exercised in backup fixture. Stable-window/no replay/newest-during-upload/deletion/sentinels/content-vs-metadata/mismatch/dedupe/missing-watch tests must follow actual optional watcher implementation.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Remaining: Actual changed-content guard is exercised in backup fixture. Stable-window/no replay/newest-during-upload/deletion/sentinels/content-vs-metadata/mismatch/dedupe/missing-watch tests must follow actual optional watcher implementation.

### PU850-ESP: `Tool/Go/watch.go`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `46468b3f00791504df0b62d58adfbf6f416b840875febecf7a46868ae7051d40`.

- Target: `tools/download-firmware.mjs:52 [UTF8-LF sha256=5017cbbe3e774b0ac049512fe09eed446758a498ba9ffbbe8baa5028b292e0b7; anchor=export async function downloadFirmware(]`.
- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Actual digestUnchangedFile size/time/mode stability checks adapted around the held backup file hash and final commit.
- Retained: Complete EOF read: all constants/errors/types/state branches/functions/methods
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Optional generic stable-build watcher/coalescing/manifest capability`. Disposition: **pending**. Reason: Actual digestUnchangedFile size/time/mode stability is imported around backup verification/commit. Optional stable-window/newest-coalesce/once-only deployment and raw/compressed marker contracts remain useful pending explicit ownership; no watcher started or automatic device mutation enabled.
- Test-code evidence: `tests/download_firmware_test.mjs:36 [UTF8-LF sha256=e402cda97ed5b12de77b109d530f4cb2458a7a7237b935297833052cafb67a23; anchor=for(const scenario of []`.
- Remaining: Actual digestUnchangedFile size/time/mode stability is imported around backup verification/commit. Optional stable-window/newest-coalesce/once-only deployment and raw/compressed marker contracts remain useful pending explicit ownership; no watcher started or automatic device mutation enabled.

### PU850-ESP: `Tool/NODE-UPLOADER.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `e90a5cc82b0f88bb9178739c02885dd4aaf49167b77cfbf8de17d8aed87c26e8`.

- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware]`.
- Target: `tools/deploy-artifact.mjs:15 [UTF8-LF sha256=0710b78d5359b0d6ece4dd48f9129acebe1809c370ed4bb6b6767fd4917bf55d; anchor=export function verifiedArtifact]`.
- Retained: Retain stable help/exit, quiet output, explicit validation and safe transfer intent. Document the Pakar updater's current contract and guarded entrypoints; do not claim documented donor watch/resume/backup behavior is already implemented.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/package-lock.json`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `8d20a728050e85d69d3291c04beb0002056384782a795dc954b4fe8fe3355237`.

- Target: `tools/script-console.mjs:14 [UTF8-LF sha256=2243be58e4dce21a7592865121e94ebdb64f21286f3333358c8259bb2bdedafb; anchor=export function createLogger]`.
- Target: `tools/pakar.mjs:65 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=Node.js20.19]`.
- Retained: Read exact chalk/chokidar/readdirp locked graph. Shared console implements required logger/color/terminal sanitation with Node standard libraries; no new node_modules or lockfile is needed for currently implemented one-shot commands.
- Omitted/replaced code: `New chalk/chokidar/readdirp npm installation for one-shot tools`. Disposition: **replaced**. Reason: Currently required console/argv behavior uses Node standard libraries and no file watcher is advertised. Preserve zero additional npm tree; advanced watcher parity remains explicit.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Complete guarded content-stable watch behavior before claiming uploader dependency/feature parity.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/package.json`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `60754d1116254eeac6b10e626c2e2b4506e60ad3ecd0296ce59645d63858e540`.

- Target: `tools/script-console.mjs:5 [UTF8-LF sha256=2243be58e4dce21a7592865121e94ebdb64f21286f3333358c8259bb2bdedafb; anchor=export const EXIT]`.
- Target: `.github/workflows/build.yml:86 [UTF8-LF sha256=b352090f2790ff875580ed2b985667accb7a6dcc1e9a675c8c551f430438e2f1; anchor=node tests/script_tooling_test.mjs]`.
- Retained: Retain explicit Node20.19 minimum and direct Node entrypoints, tests and help without an additional npm install. Shared logger/CLI use Node standard libraries; unavailable watch mode is recorded as a gap.
- Omitted/replaced code: `New chalk/chokidar/readdirp npm installation for one-shot tools`. Disposition: **replaced**. Reason: Currently required console/argv behavior uses Node standard libraries and no file watcher is advertised. Preserve zero additional npm tree; advanced watcher parity remains explicit.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Complete guarded content-stable watch behavior before claiming uploader dependency/feature parity.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/README.md`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `02d192de780df4199c722c8b314aabdb68a67f1a4cf8790f83f687f5e3ae7985`.

- Target: `tools/pakar.mjs:10 [UTF8-LF sha256=0875bb298a832a275d62e3595c2e2691c38eef442ac5246a85fb4445c3715bd3; anchor=export const commands]`.
- Target: `tools/script-runtime.ps1:8 [UTF8-LF sha256=6fbe86a1ba2d5347abd1b70c4ab3dfee91f4dd7a58415324d9d291232ca07fcc; anchor=function Assert-PakarExecutable]`.
- Retained: Map actual uploader/build/CLI usage to the shared Pakar command reference and tools. Clearly separate raw S3/JSON verification from ASA gzip/ok!, plans from execution and current one-shot support from pending advanced transfer modes.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Content-stable auto-upload watch, resumable atomic CLI download and verified pre-update CLI backup`. Disposition: **pending**. Reason: One-shot updater and readback are not complete watch/resume/backup parity. Import these only with peer identity, stable snapshot and no ambiguous-write replay guards; keep useful donor cases open.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Implement and exercise watch/resume/export/pre-update-backup contracts and remaining donor negative cases.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/upload.js`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `3d3d067426480bb0a2137ffb7f8fa74d7378362c237f573b923f6dd82298d9d0`.

- Target: `tools/script-console.mjs:7 [UTF8-LF sha256=2243be58e4dce21a7592865121e94ebdb64f21286f3333358c8259bb2bdedafb; anchor=export function sanitizeText]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Import actual logger, terminal sanitizer, stable exit categories, quiet/color rules and argv/preflight concepts into script-console.mjs/pakar.mjs. Correct OSC-before-ESC regex ordering and redirected stderr coloring. Existing updater retains hostname/TCP identity pinning, no uncertain-write replay and full postboot verification.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Stable watcher/coalescing/graceful-stop/progress and remaining negative cases`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Explicit verified backup-before-update and resumable export now imported. Remaining stable watcher/coalescing/graceful stop/skip-identical/progress, gzip consumer/platform/filesystem/transport and unported negative scenarios need applicable implementation/acceptance; no source fixture closes live/physical/restore gates.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/upload.test.js`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `02842871c7f5c1285b57c1cc2392d655df848ef993c138f6a6c11b9779bf0778`.

- Target: `tests/script_tooling_test.mjs:73 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=const cliPlan=]`.
- Target: `tests/update_tool_test.mjs:129 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=const server=createServer]`.
- Target: `tools/update-firmware.mjs:215 [UTF8-LF sha256=489bab793017cb7bfab436ab3a0f49a8d3eb3b8a58172a77dad30b7a7a8e0d90; anchor=export async function updateFirmware(]`.
- Target: `tools/peer-identity.mjs:54 [UTF8-LF sha256=869e9ea5db1887dde58b170c66d4c2b379aa6fb7d3c123adc5c023e1b8db1ce7; anchor=export function boardPeer(]`.
- Target: `docs/PU850-PARITY.md:3 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Actual backup-before-update orchestration, 2026-10-19]`.
- Retained: Adapt actual subprocess execution and no-device-contact assertions into script_tooling_test.mjs for offline raw-image planning and invalid mode rejection. Existing real loopback multipart/WS updater fixtures retain exact byte/length/hash/admission and postboot checks, not ASA response parsing.
- Retained: Actual Node performUpload and C# BackupInstalledFirmwareAsync backup-before-write sequencing now serves existing updater with explicit --backup/--resume-backup, first-read expected-peer fencing, metadata match and fresh auth/job/boot/latch admission before local upload or remote stage/apply.
- Omitted/replaced code: `ASA gzip/two-record manifest, legacy plain-info fallback and exact ok! reply; verification bypass`. Disposition: **replaced**. Reason: Current Pakar S3 uses raw application validation and structured JSON/update endpoints with board/boot fences. Mandatory auth-off/new-boot/full-SHA checks must not be weakened to preserve an incompatible donor wire protocol.
- Omitted/replaced code: `Stable watcher/coalescing/graceful-stop/progress and remaining negative cases`. Disposition: **pending**. Reason: Actual verified pre-update backup and ranged/resumable export now implemented through existing owners with same-peer/release/auth/job/boot/latch and no-overwrite checks. Watch/coalescing/graceful stop/skip-identical/progress, remaining gzip consumer/platform/filesystem/transport and exact unported negative scenarios remain pending as applicable. No donor tests, SDK builds, live update/restore or physical proof claimed.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Test-code evidence: `tests/update_tool_test.mjs:246 [UTF8-LF sha256=8c3e3df8db7d7fe46fdca6e1b03958f812f22521e7efb795ee5c54074273b66d; anchor=Automatic updater plan/preflight]`.
- Test-code evidence: `tests/update_backup_test.mjs:53 [UTF8-LF sha256=9fad7b056864a821193c642fbc8739ca60f6b490fc358853906564fd242b75cc; anchor=Backup must precede even remote staging]`.
- Remaining: Explicit verified backup-before-update and resumable export now imported. Remaining stable watcher/coalescing/graceful stop/skip-identical/progress, gzip consumer/platform/filesystem/transport and unported negative scenarios need applicable implementation/acceptance; no source fixture closes live/physical/restore gates.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### PU850-ESP: `Tool/Websocket Client.html`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `cb1e505e9fd5df220e976b3a42e5baedeeef8d8c6e8e0f17fd4c6a6b58f26206`.

- Target: `docs/PU850-PARITY.md:244 [UTF8-LF sha256=fa0f798b253216dae2da86de732cdeeeb522fc16557b5b30634cf47a203f589e; anchor=## Exhaustive continuation: editor and Go client slice, 2026-10-19]`.
- Retained: Complete EOF read: complete HTML/CSS and initWebSocket/onOpen/onClose/onMessage/onLoad/initButton/toggle
- Retained: Exact function-by-function adaptations, safety defects and remaining contracts are recorded in the editor/Go slice of docs/PU850-PARITY.md.
- Omitted/replaced code: `Standalone generic endpoint/message inspector`. Disposition: **pending**. Reason: Current Web socket/reconnect/safe text ownership replaces hardcoded PU hostname, innerHTML injection, unbounded dots/fixed retry/unguarded sends. Standalone inspector remains useful pending real auth/identity design.
- Remaining: Current Web socket/reconnect/safe text ownership replaces hardcoded PU hostname, innerHTML injection, unbounded dots/fixed retry/unguarded sends. Standalone inspector remains useful pending real auth/identity design.

### PU850-ESP: `upgrade.sh`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `2e1870cb90db3459eb4abb2e991605409621021fb33b5e61cb38efc82e30d75b`.

- Target: `tools/firmware-dependencies.mjs:62 [UTF8-LF sha256=903bd1fdd6e50218cab22db4fc21fb64aee44da2b025faf329fcee4703506ab1; anchor=export async function checkDependencies]`.
- Target: `tools/firmware-dependencies.json:2 [UTF8-LF sha256=4a7dc6ef881b1100856419608f5eceaf5c748a7a890c1834fa48250985efe3c1; anchor="platforms"]`.
- Target: `upgrade.sh:4 [UTF8-LF sha256=2d02c4404e822f167989e965061d705f87896ec3069efb39d8bce1ad644f96d7; anchor=dependencies "$@"]`.
- Target: `upgrade.cmd:3 [UTF8-LF sha256=8e8614d6dc26192f66ef8e842a3dd53a3a5b66272a53d7d3bff7c275ea454dcb; anchor=dependencies]`.
- Retained: Adapt exact-name registry indexing, check/apply operation and one cached index into firmware-dependencies.mjs. Improve lexical version selection to numeric stable comparison, retain pinned-ahead versions, detect concurrent edits and install only explicitly locked versions.
- Omitted/replaced code: `PU-specific board/profile and separate dependency/config write paths`. Disposition: **replaced**. Reason: Reuse applicable exact versions/official registries in one reviewed pin owner. Preserve operator config and Pakar hardware flags; no silent downgrade, duplicate Arduino_JSON API, unsupported library additions or automatic SDK initialization.
- Test-code evidence: `tests/script_tooling_test.mjs:107 [UTF8-LF sha256=53ce71d8a1965076c732bcd32c94c9cf0d70f08e0a5488d5a715d3d794a33498; anchor=Script tooling:]`.
- Test-code evidence: `tests/script_tooling_test.ps1:26 [UTF8-LF sha256=d3c679d8caa22177e7a5590564cd1141b84e1b194b3c3ee0bb825c26a4d46305; anchor=PowerShell utilities parse]`.
- Remaining: Run live registry check and review any candidate version changes with full build/hardware acceptance; pins were not upgraded by this slice.
- Remaining: No physical deployment, full client distribution or donor-wide completion is inferred from source or fixtures.

### RayanLamp: `internal/i18n/digits.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `e81f35286d38b645ed3846d3806021089663d9b74f95def6394f5ba921793542`.

- Target: `desktop/src/ui/digits.rs:3 [UTF8-LF sha256=5a93b2fe85c492c9cd393e96dce442f82b7f4936c08c3f81ec92e723a8df9929; anchor=pub fn latin]`.
- Target: `desktop/src/ui/cell_search.rs:47 [UTF8-LF sha256=735f49c2159d9eeb61f08a24e41bace6242115a722ad58abcd9463ec54a3413e; anchor=super::digits::latin(query)]`.
- Target: `assets/web/rayanlamp.js:4 [UTF8-LF sha256=56c1f07d95c20925773552af89e6a768b2abf15796fb4911568854b10c6180ac; anchor=function latinDigits]`.
- Target: `mobile/lib/cell_search.dart:5 [UTF8-LF sha256=470ed85cf8b5ed034f63bc4bc84b415fd8d943803880e142e62396826f4481fe; anchor=String latinDigits]`.
- Target: `mobile/lib/device_settings_page.dart:582 [UTF8-LF sha256=143c7e774de6f90a48a1bcdbdf37c3241a9f86106b04768befa1b9c6f3d02774; anchor=int.tryParse(latinDigits(value))]`.
- Retained: Actual Persian/Arabic-Indic-to-ASCII maps adapted into Rust, JavaScript and Dart and invoked by native/Web/mobile cell search and actual mobile port/telemetry fields. Canonical IDs/ranges stay ASCII; peer names, secrets and protocol identifiers are not globally transformed.
- Omitted/replaced code: `ShapePersianDigits visual decimal shaping`. Disposition: **pending**. Reason: Search needs ASCII normalization now. Apply visual shaping only to audited application-owned numeric labels, not arbitrary peer text or wire IDs. Display consumers remain pending.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:28 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=latinDigits('۰۱۲۳۴۵۶۷۸۹]`.
- Test-code evidence: `mobile/test/cell_search_test.dart:15 [UTF8-LF sha256=1fce7766710c10169453820ae4a6d39ba8625149ea0181989fccc6689b109bce; anchor=RayanLamp numeric normalization]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:211 [UTF8-LF sha256=e40bce43e135cc5f4508b37cca7e2cd4351852609101774636e55e2bf12c1177; anchor=expect(api.cloud['mqtt_port'], 8883)]`.
- Remaining: Finish owned numeric display shaping and execute guarded native digit/search tests.

### RayanLamp: `internal/terminal/policy.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `c250474261e4ce8d6f63826719a408b0ae57a2533ec547c9a151c9939b5b5518`.

- Target: `desktop/src/terminal_ui.rs:108 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn color_allowed()]`.
- Retained: Actual NO_COLOR/TERM=dumb/CLICOLOR=0/FORCE_COLOR=0 opt-out precedence and redirected-input/output rejection are retained; crossterm owns native VT/raw transitions, checked on entry.
- Omitted/replaced code: `Emoji/style detection preferences and broader platform policy tests`. Disposition: **pending**. Reason: Current TUI uses conservative text only and no donor emoji UI. Per-platform style controls/detection tests remain useful pending work, not accepted exclusions.
- Test-code evidence: `desktop/src/terminal_ui.rs:968 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn donor_text_safety_preserves_unicode_not_peer_terminal_commands()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/form.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `3a3b4f13d0c839664edc2b15bb4418254463ff4d36016cb798a799016ef2890a`.

- Target: `firmware/esp8266/src/ShellTui.h:81 [UTF8-LF sha256=619d9de1094638ee9dc8878aa749bdf14cfa1610f434fb6905a1d596e43e1307; anchor=if(editing_)return edit(ch,snapshot,action);]`.
- Target: `firmware/esp8266/src/ShellTuiBinding.h:3 [UTF8-LF sha256=6ba1c1b6da225954c10988a2269eee7dc5f422d6f59ac3c6453bc3d51e7f7138; anchor=struct ShellTuiSnapshot]`.
- Retained: formIsFiltering focused-field key ownership is adapted to a pure ShellTui editing state: printable input/Tab cannot invoke workspace shortcuts while a draft is active; Esc/Ctrl-C cancel without applying.
- Retained: Readonly fields cannot emit Set, unchanged Enter is not a write, CR-LF submits once, and a draft is cancelled if its key/editable/secret authority changes. Observed refreshes do not repaint or replace an active draft.
- Retained: One shared ShellProperty snapshot binds TUI fields to the same shell/configuration validators, rather than introducing a separate TUI business or persistence layer.
- Omitted/replaced code: `Huh form types, Go interface assertions and Bubble Tea runtime`. Disposition: **replaced**. Reason: The microcontroller uses an allocation-free C++ editing state and explicit Action handoff. No Go/Huh/Bubble Tea runtime is ported or required, and no full Huh select-filter UI is claimed.
- Omitted/replaced code: `Unicode/Persian input and desktop form widgets`. Disposition: **pending**. Reason: Current Arduino form accepts bounded printable ASCII only; terminal character shaping, Persian editing and richer host widgets require separate implementation and acceptance.
- Test-code evidence: `tests/shell_tui_test.cpp:33 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=private\trefresh]`.
- Test-code evidence: `tests/shell_tui_test.cpp:27 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=Async observation cannot repaint draft.]`.
- Test-code evidence: `tests/shell_tui_test.cpp:62 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=rows[0].editable=false]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### RayanLamp: `internal/tui/keys.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `9db93f624f1fcc5b850993de2c6cd2ca1304bf91034849f246dd54fb70a02720`.

- Target: `desktop/src/terminal_ui.rs:301 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn key(&mut self, key: KeyEvent)]`.
- Retained: Contextual Tab/ShiftTab, arrows, Help/quit and Find draft Enter/Escape route into shared Pakar selection and explicit capability-gated actions; repeat presses cannot queue writes.
- Omitted/replaced code: `Theme/mouse/reconnect/help catalog, localized binding vocabulary and donor lamp actions`. Disposition: **pending**. Reason: Lamp-specific actuation is replaced by ACKed table actions. Useful theme, mouse, reconnect and complete localized help remain pending rather than hidden; g refresh and r red are deliberate table bindings.
- Test-code evidence: `desktop/src/terminal_ui.rs:1018 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn shared_find_selection_and_arrow_navigation_never_write()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/run_test.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `2d645e6b27b7a4d17afc027e8393471d6b19b7beec6bbf359cee1c8588ee832a`.

- Target: `desktop/src/terminal_ui.rs:118 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn enter()]`.
- Retained: The actual donor fail-before-control-sequences guard is retained; built executable piped-child fixture proves zero stdout/control escapes and zero HTTP requests.
- Omitted/replaced code: `Independent stdin-only/stdout-only terminal fixtures and real interactive raw-mode acceptance`. Disposition: **pending**. Reason: The built test redirects both descriptors; separate mixed-descriptor and actual console navigation/cleanup acceptance remain pending. Do not equate guard tests with full terminal acceptance.
- Test-code evidence: `desktop/src/terminal_ui.rs:980 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn offline_and_unacknowledged_peers_have_no_table_actions()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/run.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `462fdbc977b5261f971120fd071b7dc6b023e6d6e66fd37e2dabf03af12c0a16`.

- Target: `desktop/src/terminal_ui.rs:804 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=pub fn run(endpoint: Endpoint)]`.
- Retained: Terminal/stdin/stdout ownership, early redirected guard, independent request worker, bounded exit cleanup and terminal-neutral command text are adapted to crossterm and the existing Rust HTTP client.
- Omitted/replaced code: `Bubble Tea program/context/screen options, UI theme/ASCII preferences and transport-manager ownership`. Disposition: **pending**. Reason: Go toolkit-specific ownership is replaced by crossterm RAII and the existing Rust transport; theme controls and transport discovery/USB choice still require actual Pakar implementation.
- Test-code evidence: `desktop/src/terminal_ui.rs:980 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn offline_and_unacknowledged_peers_have_no_table_actions()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/runtime.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1d6f08a67f0076c36a7951d79e7d35c9d8044090dbf60c9105bf4307ed052028`.

- Target: `desktop/src/terminal_ui.rs:810 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=let worker = thread::spawn(move ||]`.
- Retained: Independent model/network scheduling and deterministic keyboard/receipt tests adapt to a bounded Rust worker/channel. Shared incident log coalesces repeats but retains distinct causes and revokes lost peer controls.
- Omitted/replaced code: `Donor spinner/expiring-notice/theme scheduling and complete runtime injection surface`. Disposition: **pending**. Reason: The new renderer shows observed busy state without fake device progress. Full spinner/theme scheduling and all runtime adapters remain pending; current model plus HTTP doubles do not certify hardware.
- Test-code evidence: `desktop/src/terminal_ui.rs:1050 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn loss_and_peer_change_revoke_controls_without_false_recovery()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/terminal.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `3b56b9ee2bfa2e40b02e464c7cc2cd471d6dbb2148f06091bd56dfdf2926e1b3`.

- Target: `firmware/esp8266/src/ShellWire.h:31 [UTF8-LF sha256=af3f23d908310dd349b0c90d1fc4385a212e6439b10c72f8dc2f37dace4b5216; anchor=class OutputWindow]`.
- Target: `firmware/esp8266/src/TelnetShell.h:17 [UTF8-LF sha256=20e75d8dd8f39bea93c0df3da5aff98635b857eefb55003cfebeabd4e5c92281; anchor=inline size_t terminalText]`.
- Target: `firmware/esp8266/src/ShellTui.h:10 [UTF8-LF sha256=619d9de1094638ee9dc8878aa749bdf14cfa1610f434fb6905a1d596e43e1307; anchor=class ShellTui]`.
- Target: `firmware/esp8266/src/ShellService.h:252 [UTF8-LF sha256=897ac8e49f95e1e2ebc8b59dbd1a76148d1fcaaecf609cf33d99d067fc849a0c; anchor=void closeDeviceShells()]`.
- Target: `tools/device-shell.ps1:25 [UTF8-LF sha256=4e197e3502eb6ef40d2a3df7bdde97ed3c05140c97e06e3d07426232732a9c55; anchor=Exactly once; uncertain input is NEVER replayed.]`.
- Retained: Session-fenced immutable input/output ownership is adapted into SerialChannel/OutputWindow and the host terminal tool: explicit Open/token, no uncertain input replay, nonwrapping sequences, output ACK before consumption and explicit close.
- Retained: Terminal safety is adapted into bounded terminalText and TUI rendering: external CSI/OSC/DCS/control payloads cannot inject terminal actions, and only the project renderer emits ANSI; output/drafts/transcripts are bounded.
- Retained: Independent session state, draft preservation, explicit backpressure and batched TUI draws retain the donor terminal's ownership/refresh principles without moving networking or actuator work into callbacks.
- Omitted/replaced code: `Go terminalOverlay, Bubble Tea viewport, 512KiB scrollback and host long-poll goroutines`. Disposition: **replaced**. Reason: An MCU-owned bounded ANSI TUI and PowerShell framed terminal replace this runtime, not a wholesale Go port. The device does not retain a 512KiB transcript or spawn host long-poll workers.
- Omitted/replaced code: `sanitizeDeviceTerminal printable UTF-8/SGR subset, bidi-aware text and scrollback viewport parity`. Disposition: **pending**. Reason: Arduino sanitization intentionally substitutes non-ASCII and removes external SGR rather than reproducing the donor Unicode viewport. Persian editing, resize/mouse/scrollback and full terminal-emulation parity are not implemented by this slice.
- Omitted/replaced code: `Transparent raw serial and automatic uncertain write retry`. Disposition: **replaced**. Reason: An additive boot/token-fenced COBS channel preserves ordinary USB recovery. Host terminal sends input once and does not reopen/replay actions automatically after uncertainty.
- Test-code evidence: `tests/shell_wire_test.cpp:21 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=Lost read reply replays same bytes]`.
- Test-code evidence: `tests/shell_wire_test.cpp:26 [UTF8-LF sha256=5b90c5c6eb25b7bd34d33203b7db7da0f6257ff00976b88c70c6b662ca5b48f9; anchor=output.sequence()==UINT16_MAX]`.
- Test-code evidence: `tests/shell_tui_test.cpp:10 [UTF8-LF sha256=6fd3966ebe7ed5567a018116a7d3ef73110a2149409dec9145da4e95f9107929; anchor=never-render-this]`.
- Test-code evidence: `tests/telnet_shell_test.cpp:98 [UTF8-LF sha256=c0a3b2937c012535482557a093176894af3c6a600291839b0c38565afe8b45e1; anchor=Inspect the caller-owned backing bytes]`.
- Scoped advantage: Source comparison and hash-bound fixture references only; deployment, terminal interoperability and hardware acceptance remain pending.
- Remaining: Compile and deploy the integrated shell, then prove simultaneous bounded Telnet sessions, AP/station/Ethernet listener and discovery changes, and framed USB recovery alongside normal protocol commands.
- Remaining: Exercise authentication changes, disconnect/reconnect, slow-reader backpressure and maintenance exclusion on the actual board; an immediate OK/queued command is not proof of AVR execution or physical LED/display/audio success.

### RayanLamp: `internal/tui/text.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `ac575e39f688d9c796f4b013fdb768b0b3ecb5bbda04908950f14fc7217e327e`.

- Target: `desktop/src/terminal_ui.rs:25 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=pub fn safe_line(value: &str)]`.
- Retained: Actual sanitizeTerminalLine unsafe-control/bidi filtering and cell-aware truncation adapt to Unicode Rust strings and unicode-width. Removes OSC/CSI/DCS sequences and bounds work while retaining Persian text.
- Omitted/replaced code: `Donor renderer-specific text entry points and full terminal shaping acceptance`. Disposition: **pending**. Reason: Rust TUI renderer calls one sanitizer for every emitted line rather than copying Go view APIs. Actual Persian shaping/terminal-font behavior is not inferred from string-preservation tests.
- Test-code evidence: `desktop/src/terminal_ui.rs:968 [UTF8-LF sha256=ef66e4c5f115d6b128525c8c06d9b8b23e6ece6c962399673800d6b765af04b3; anchor=fn donor_text_safety_preserves_unicode_not_peer_terminal_commands()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `LICENSE`

Verdict: **equivalent-file**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `a7931f292caa44328db79c5b19515c6629a5b31042a4cf375f28c341d84fd28d`.

- Target: `desktop/licenses/RayanLamp-MIT.txt:3 [UTF8-LF sha256=a7931f292caa44328db79c5b19515c6629a5b31042a4cf375f28c341d84fd28d; anchor=Copyright (c) 2026 YektaYar]`.
- Retained: Complete YektaYar MIT copyright and notice retained for actual donor code.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:1 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=Adapted from RayanLamp haptics.test.ts]`.
- Remaining: Include notice with eventual binary distributions, not just source.

### RayanLamp: `webui/public/theme-init.js`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `0e39142a82287beefd43b59e032ff8e55f0a054b52be83a2cb3efd26ae5bb2b8`.

- Target: `assets/themes/browser-appearance.js:69 [UTF8-LF sha256=a3cfd720a376fc399dd8abfef91a172180b583b828516f4d1198e4be7bd34dae; anchor=function applyPrepaintLocale()]`.
- Target: `tools/generate-web-appearance.mjs:34 [UTF8-LF sha256=2318e70ce657ac29e2e4c1da18381cf7493179c23ca4368edf6b8828a73e5ff6; anchor=replaceBlock('// BEGIN BROWSER CONTROLLER']`.
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=<script data-pakar-presentation>]`.
- Retained: Actual safe validated storage read, requested/OS theme resolution, root color-scheme and chrome metadata, locale/direction and resolved-locale pre-paint behavior are adapted into the single head bootstrap. It runs without dialog/body nodes, requests, preference writes or application/peer title mutation; generator checks canonical equality.
- Omitted/replaced code: `Separate same-origin /theme-init.js, duplicated theme resolver, lamp colors and rayanlamp.web.locale auto/language-list some(fa) selection`. Disposition: **replaced**. Reason: Inline canonical bootstrap shares the existing Pealayer helpers and palette once, before CSS, saving an ESP endpoint and extra request. Existing Pakar locale keys use system/en/fa, primary browser language and explicit Auto/LTR/RTL override, consistent with current runtime resolver. Supporting a secondary Persian language must not unexpectedly override an English primary choice. Existing dark/Studio defaults are retained rather than silently changing user appearance.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:55 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=Pre-paint survives blocked storage]`.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:41 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp pre-paint bootstrap runs before CSS/body]`.
- Remaining: Post-bootstrap generic dynamic copy, browser language-change lifecycle and complete native/Web/mobile localization remain pending in the existing localization concern; this import certifies pre-paint behavior, not those separate source files.

### RayanLamp: `webui/src/api.test.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `6dfacf9fb7bff4896ac699547c46bf100c744b9017bec554ef42b9ea4d9c9f5b`.

- Target: `assets/web/peer-http.js:5 [UTF8-LF sha256=04dd5da95667de82bc556e005db5fc72b7d31ebafbda7b605590e8b80b120aa6; anchor=class PeerAPIError]`.
- Retained: Full test source read through EOF. Actual same-origin request and exact mutation-header/body scenarios adapted to the real Web peer client with server-advertised board/boot pins; cookie auth is retained separately, pins are not substituted for CSRF/auth.
- Retained: Typed status/error, no credential/body logging, no 401 write replay and versionless paths are exercised in existing embedded sign-in/console and new peer tests.
- Omitted/replaced code: `establishSession forced renewal coalescing and X-RayanLamp-CSRF/If-Match session/config assertions`. Disposition: **pending**. Reason: ESP cookie/origin authentication is different from the donor Go session endpoint. Explicit no-replay replaces 401 retries; generic coalescing/revision conflict tests remain useful pending the matching server/client contracts.
- Omitted/replaced code: `lamp toggle/on/off living routes`. Disposition: **not-applicable**. Reason: Lamp commands are donor business; table/controller capability modes are retained instead.
- Omitted/replaced code: `diagnostics, UI bootstrap/config, audio content-addressed import/playback, hotkey capture lease and integration registration assertions`. Disposition: **pending**. Reason: All are useful generic host contracts beyond this bounded device HTTP safety slice; need actual typed host implementations and contract fixtures, not fake ESP endpoints.
- Test-code evidence: `tests/web_peer_http_test.mjs:70 [UTF8-LF sha256=c9c8062ace7ecba708eb2619eac096bc2cca8ee1f7cf4777794c1f824a46e9ab; anchor=exact pin headers]`.
- Test-code evidence: `tests/web_access_test.mjs:7 [UTF8-LF sha256=83c06839b0cfb57d8660ad9e66600b7ad8b9a799bf70fdf42755997e3f78cd02; anchor=Execute the actual embedded Web application]`.
- Remaining: Port authenticated coordinator/bootstrap/config/audio/hotkey/registration tests as their corresponding host contracts are imported. Do not claim the complete donor test suite from this request-header/error slice.

### RayanLamp: `webui/src/api.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1b201ac38c3ca4f45dea639d198204f294c83f7e07fb511be99b3d54154fc741`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=255036e79060b6d348944e5f1d1edc90ac1c79f20dc1a095a4384b38d85d15f0; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:1341 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=async function refreshAuthenticated(options={})]`.
- Target: `assets/web/peer-http.js:51 [UTF8-LF sha256=04dd5da95667de82bc556e005db5fc72b7d31ebafbda7b605590e8b80b120aa6; anchor=function createPeerHttp(]`.
- Target: `assets/web/basic-cells.js:3 [UTF8-LF sha256=f4746908dd22a62379c5d8b9a5491f1225272f359e956d3f135a2594dca2f9bd; anchor=function createBasicCells(]`.
- Target: `assets/web/basic.html:15 [UTF8-LF sha256=f7b0c5c1231549923f8ba3440f5a342d4b24225480ed26782dcacc91ae54eeec; anchor={{PEER_HTTP}}]`.
- Retained: Fully read actual api.ts. APIError status/code, Headers Accept, same-origin/no-store fetch, AbortController 12000 ms deadline and finally cleanup now adapted to all embedded Web HTTP requests in the shared mechanically embedded peer-http.js, not reconnect GET only.
- Retained: Pakar cookie authentication and expired-session login are retained; no uncertain mutating request/session replay, credentials or raw endpoint/body diagnostics copied.
- Omitted/replaced code: `establishSession/requestSession, CSRF header and request retrySession on 401`. Disposition: **replaced**. Reason: Go coordinator /app/session is absent on ESP. Reconnect checks the existing authenticated GET; 401 cancels recovery and requires sign-in. Blind mutating retry is incompatible with no-uncertain-write-replay. Generic session coalescing/authorization remains pending host-contract adaptation.
- Omitted/replaced code: `lamp on/off/toggle routes`. Disposition: **not-applicable**. Reason: Lamp business commands do not apply to discovered table/other board modes. Existing capability-ACK controller actions remain authoritative.
- Omitted/replaced code: `bootstrap/getConfig/patchConfig If-Match, diagnostics and diagnosticsRefreshTimeout`. Disposition: **pending**. Reason: Typed bootstrap, If-Match revision/config concurrency and long-running diagnostic contracts remain pending; all Web HTTP requests now have cancellation/deadlines, but this is not atomic revision-controlled server persistence.
- Omitted/replaced code: `RayanLampClient interface, APIError/isEnvelope/unwrapResponse/readJSON and realtime event decoding`. Disposition: **pending**. Reason: APIError and advertised JSON parse validation are adapted. Full RayanLampClient typed interface, isEnvelope/unwrapResponse result schema and realtime decoding still require contracts.ts/direct-api/client-factory imports; Pakar keeps real flat JSON and plain-text receipts, not invented Go envelopes.
- Omitted/replaced code: `hotkey capture begin/renew/release/probe, integration registration and window/show/hide`. Disposition: **pending**. Reason: Generic host/desktop operations need verified native capability, lease ownership and full surfaces. No nonexistent ESP endpoints are exposed.
- Omitted/replaced code: `notify/playAudio/importAudio/validateAudioRequest/safeAudioFilenameHeader`. Disposition: **pending**. Reason: Generic host/browser audio/notification/import support remains useful; AVR melody/buzzer control is not equivalent WAV playback or content-addressed host import. Adapt discovered host/device ownership and port exact tests.
- Test-code evidence: `tests/realtime_retry_test.mjs:55 [UTF8-LF sha256=2e0229615c0709866448b3d79eb46806a383edcb3946833d80f403c31d25af1f; anchor=Actual api.ts AbortController deadline/finally cleanup]`.
- Test-code evidence: `tests/web_peer_http_test.mjs:51 [UTF8-LF sha256=c9c8062ace7ecba708eb2619eac096bc2cca8ee1f7cf4777794c1f824a46e9ab; anchor=Deadline, timer release and external cancellation port api.ts]`.
- Test-code evidence: `tests/basic_cells_test.mjs:55 [UTF8-LF sha256=97dbf0bafc0e5cd2f73e7c30a4978e0fe4993ea274206bbaed5d1dfa1e275a2f; anchor=No optimistic paint after POST/ACK]`.
- Remaining: api.test.ts was fully read; all its authenticated host/config/audio/hotkey/registration scenarios remain pending matching implementations, not a completed import certificate.
- Remaining: Complete typed structured response schemas, revision concurrency, session coalescing and native/mobile adoption; actual LAN and physical acceptance remain open.

### RayanLamp: `webui/src/browser-console.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `024ed161c6c71655ef67cdbed8cf743fd33864118604737a5f44f533bf9f48f5`.

- Target: `tests/browser_console_test.mjs:1 [UTF8-LF sha256=a8277aae5b51218dae4eaf625d2e05a6cd9817e25057c46976a2ae6e67f61916; anchor=Actual RayanLamp browser-console.test.ts scenarios]`.
- Retained: All actual donor startup, five-level dispatch, allowlist/redaction, keyed dedupe, Error-code-only and control/Bidi scenarios ported against shipped code. Added bounds/cycles/prototype, invalid sink/date, current palette, real HTTP secret/no-replay/quiet-poll, stale WebSocket ownership and canonical embedding checks.
- Omitted/replaced code: `Vitest describe/it.each/vi sink mocks`. Disposition: **replaced**. Reason: Built-in Node assertions/VM sink and controlled time run the same scenarios plus real production adapter code without adding dependencies or build caches.
- Omitted/replaced code: `Lamp state names and fixed brand color expectations`. Disposition: **replaced**. Reason: Tests assert adapted device vocabulary and actual shared Pealayer colors rather than claiming the lamp identity/colors apply to Pakar.
- Test-code evidence: `tests/browser_console_test.mjs:79 [UTF8-LF sha256=a8277aae5b51218dae4eaf625d2e05a6cd9817e25057c46976a2ae6e67f61916; anchor=Real HTTP adapter]`.
- Remaining: VM execution is not pixels, deployment, LAN or physical board acceptance.

### RayanLamp: `webui/src/browser-console.ts`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `5584e97f745eadd5fa52dd1cbec91813dd4f0638da0ca4cf7ca986658f730165`.

- Target: `assets/web/browser-console.js:77 [UTF8-LF sha256=7789b2a4baae93afdd45a0a803f8567722ef0b9ea776c1051fc8482be93e0b60; anchor=class StyledBrowserConsole]`.
- Target: `firmware/esp8266/src/WebUi.h:1357 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function connectWs()]`.
- Retained: Actual five-level styled sink/startup, operational event vocabulary, allowlisted metadata, recursive sanitizer, safe Error-code projection and keyed quiet-window deduplication imported into canonical browser module. Real HTTP and WebSocket lifecycle consumers execute it; routine successful polling, telemetry and binary frames stay quiet.
- Retained: Actual URL/Bearer/JWT/query, single-line/control-character protection, depth/array/object bounds and cycle handling retained; no interception, secret/raw-response trace or persistent writes.
- Omitted/replaced code: `Lamp-only lamp.state/lamp.command vocabulary and RAYAN LAMP startup brand`. Disposition: **replaced**. Reason: Pakar uses device.event/device.command and its controller identity, not another device's lamp protocol. Generic operational semantics are retained.
- Omitted/replaced code: `Hardcoded lamp palettes/gradient and monospace badge typography`. Disposition: **replaced**. Reason: Current shared Pealayer palette and controller typography are resolved on each emission; no second fixed color engine or unwanted tint.
- Omitted/replaced code: `Unbounded recent Map, boundary-only sensitiveKey, caller-controlled percent format, raw DOM id/function name and throwing host sink`. Disposition: **replaced**. Reason: 256-key/60-second bounds, camelCase redaction, percent escaping, minimal DOM/function summaries and fail-closed sink protect long-lived firmware clients and secret-bearing host data.
- Omitted/replaced code: `Vitest/TypeScript runtime packaging`. Disposition: **replaced**. Reason: ESP's existing classic-script embedding and Node VM tests require no React/Vitest dependency, donor build or duplicate firmware copy.
- Test-code evidence: `tests/browser_console_test.mjs:35 [UTF8-LF sha256=a8277aae5b51218dae4eaf625d2e05a6cd9817e25057c46976a2ae6e67f61916; anchor=Actual allowlist + recursive redaction]`.
- Test-code evidence: `tests/browser_console_test.mjs:121 [UTF8-LF sha256=a8277aae5b51218dae4eaf625d2e05a6cd9817e25057c46976a2ae6e67f61916; anchor=Actual WebSocket ownership]`.
- Scoped advantage: Adds bounded dedupe lifetime, camelCase/prototype/format protection, newest-socket ownership and diagnostic failure containment. Source-specific continuing incidents coalesce interleaved GET/WS failures until recovery while different error codes and every explicit write outcome remain visible; recovery reports occurrence counts. Executable production-module/API/socket fixtures cover these adaptations.
- Remaining: Actual LAN/physical firmware runtime remains separate. Other donor app-level settings/audio/diagnostics consumers are independently inventoried and pending, not certified by this utility import.

### RayanLamp: `webui/src/components/icon.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `60ee5f05d39b0cd5e93bf5abb6f2ac702d6964bf6d1878f13a0c8a3bc7f32b02`.

- Target: `assets/web/toasts.js:15 [UTF8-LF sha256=c416d19144ed962376079bf4aeed116ccd5aaed6c4ddc4bf63a846796f2e8d2e; anchor=const geometry=]`.
- Retained: Fully read actual icon source. Exact check/warning/info/close path/circle geometry plus size/viewBox/currentColor/1.75 stroke/round joins/aria-hidden contract imported into four real toast consumers; close uses 16px and status 20px.
- Retained: Application branding remains shared Pealayer artwork, not replaced with RayanLamp icon.
- Omitted/replaced code: `lamp lightbulb artwork`. Disposition: **not-applicable**. Reason: Lamp-specific artwork does not identify this controller or its peer-advertised board applications; keep the existing shared application identity.
- Omitted/replaced code: `settings/diagnostics/about/power/refresh/reconnect/window/moon/sun/system/menu/chevron/bell/sound/upload/keyboard/terminal/network SVG consumers`. Disposition: **pending**. Reason: Reusable generic symbols need actual source-consumer mapping with the existing Phosphor system. Do not certify them from four notification paths or silently substitute application branding.
- Omitted/replaced code: `IconName reusable API, class and labeled role=img/aria-label variant`. Disposition: **pending**. Reason: Current four icons are decorative within independently labeled status rows/buttons. A full searchable/labeled generic Web icon API and actual accessibility consumers remain useful pending work; TSX framework is not imported as a second renderer.
- Test-code evidence: `tests/toasts_test.mjs:26 [UTF8-LF sha256=df864b7b15f9f61e3f291efaf2377e02dc85d4bf35496ae05a32c447d6e96e23; anchor=aria-hidden]`.
- Remaining: Map remaining generic symbols/labeled API to real native/Web/mobile consumers, preserve Phosphor/shared-brand identity and verify accessible visual parity.

### RayanLamp: `webui/src/components/toasts.tsx`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `81a5d6f6d1d77bc6466a6ee98770d4f248a7b195078ed029f1c39bd8f1724b36`.

- Target: `assets/web/toasts.js:5 [UTF8-LF sha256=c416d19144ed962376079bf4aeed116ccd5aaed6c4ddc4bf63a846796f2e8d2e; anchor=function createToastController]`.
- Target: `firmware/esp8266/src/WebUi.h:1348 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function report(e,tone)]`.
- Retained: Actual ToastRegion polite/nonatomic live region, per-ID status rows, tone icon selection, title/message and localized dismiss button adapted to shipped DOM queue. All real Web reports and saved-identity notices use this queue outside authenticated workspace.
- Retained: Status SVG geometry and keyed store lifetimes imported from actual dependencies; bounded timers, hover/focus holds, safe text nodes, keyboard dismissal and teardown added without storage or network writes.
- Omitted/replaced code: `Preact signal-driven JSX/ToastRegion and Icon framework dependency`. Disposition: **replaced**. Reason: ESP embeds classic scripts and existing DOM controls. Actual row, live-region, keyed queue and SVG semantics are adapted without a second UI framework; store/Icon get separate full-read comparisons with remaining generic contracts.
- Test-code evidence: `tests/toasts_test.mjs:22 [UTF8-LF sha256=df864b7b15f9f61e3f291efaf2377e02dc85d4bf35496ae05a32c447d6e96e23; anchor=older expiry must not erase newer notification]`.
- Scoped advantage: Four owned/cancelled timers, bounded duration/text, independent hover/focus holds, focus-preserving keyboard dismissal and idempotent teardown prevent newer-message expiry and lifetime leaks; executable shipped-module DOM tests cover these contracts. This is a scoped queue improvement, not screenshot or whole-app superiority.
- Remaining: Validate actual browser responsive/open-modal placement and screenshots; complete shared native/Web/mobile delivery and Windows OS notification surfaces. DOM tests are not pixels, deployment or all-feature parity.

### RayanLamp: `webui/src/connection-preferences.test.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `26c99032222edf27a1b59476d107abc7aadbaf497b7e47887e4aa52460e0c331`.

- Target: `tests/rayanlamp_reuse_test.mjs:34 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=Actual donor local-event + cross-tab delivery]`.
- Retained: Actual immediate preference delivery, persistence and malformed-value rejection adapted to locale channel; adds blocked API, no echo/write and listener teardown cases.
- Omitted/replaced code: `Static-direct/embedded-relay defaults and unsafe stored lamp URL fixtures`. Disposition: **pending**. Reason: Their actual transport is not imported; running those fixtures against a nonexistent Pakar relay adapter would manufacture coverage.
- Omitted/replaced code: `Vitest environment/mocks`. Disposition: **replaced**. Reason: Built-in Node VM and deterministic host doubles replace the donor test dependency.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:61 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=No identical persistence writes]`.
- Remaining: Restore URL/default fixtures when actual connection adapter is implemented.

### RayanLamp: `webui/src/connection-preferences.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `f2ab880c8676f63d1bed9ea548a4680625a5b707f5f8aab12d8e8fda93d9c531`.

- Target: `assets/web/rayanlamp.js:79 [UTF8-LF sha256=56c1f07d95c20925773552af89e6a768b2abf15796fb4911568854b10c6180ac; anchor=function subscribeLocalePreferences]`.
- Target: `assets/web/localization.js:69 [UTF8-LF sha256=75db2917045580ed8dfd63eb2ecd42e54511b33e337f9855e50def8742259e6e; anchor=subscribeLocalePreferences(value=>]`.
- Retained: Actual validated local-event/BroadcastChannel announcement, subscription and teardown adapted to client language/direction. Adds storage fallback, blocked-channel containment, no identical writes and no receiver rebroadcast. Board drafts/secrets remain separate.
- Omitted/replaced code: `ConnectionMode auto/relay/direct, static export defaults, normalizeLampBaseURL and lampURL preference`. Disposition: **pending**. Reason: Lamp transport/endpoints differ from Pakar authenticated board/USB contracts. The event mechanism is imported; real endpoint/relay selection requires full direct-protocol/security review, not a cosmetic mode switch.
- Omitted/replaced code: `Unconditional save/broadcast persistence`. Disposition: **replaced**. Reason: Pakar low-write policy skips identical values and receivers never rebroadcast.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:59 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=Receiver never echoes]`.
- Remaining: Port applicable endpoint security/connection selection with full transport review; verify real tabs and untouched device drafts.

### RayanLamp: `webui/src/haptics.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `5aa8461039519fb310b6cb8d22eb9c26ae9ea63bfd41a6fe586434ea0b8e6525`.

- Target: `tests/rayanlamp_reuse_test.mjs:1 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=Adapted from RayanLamp haptics.test.ts]`.
- Retained: Actual fixed patterns, message/audio normalization, absent API, hidden/reduced-motion, host exceptions and throwing condition tests adapted to Node assertions; adds mutable-host and unknown-cue protections.
- Omitted/replaced code: `Vitest describe/it/vi mocks`. Disposition: **replaced**. Reason: Existing Node assertions and VM doubles execute donor scenarios without new dependencies or a donor Web build cache.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:26 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=fixedHapticPatterns]`.
- Remaining: Android vibration remains separate platform acceptance.

### RayanLamp: `webui/src/haptics.ts`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `26b14208309628ea7eca9a05eb0f7e0d8206f2b59cd0c6a226e2389d7fde89a1`.

- Target: `assets/web/rayanlamp.js:15 [UTF8-LF sha256=56c1f07d95c20925773552af89e6a768b2abf15796fb4911568854b10c6180ac; anchor=class WebHaptics]`.
- Target: `firmware/esp8266/src/WebUi.h:248 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=id="uiHaptics"]`.
- Retained: Actual pulse constants, bounded audio cues, optional vibration API, fail-closed visibility/reduced-motion conditions, copied pattern arrays and host-error containment adapted into embedded JavaScript. Opt-in Touch feedback defaults off, disabled when unsupported; only explicit command HTTP results trigger it, never polling/telemetry/frames.
- Omitted/replaced code: `TypeScript interfaces/private/type annotations`. Disposition: **replaced**. Reason: ESP serves dependency-free JavaScript. Actual runtime bodies are retained; explicit own-property cue validation replaces a compile-time-only guarantee.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:24 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=h.trigger('arbitrary raw frame')]`.
- Scoped advantage: Unknown JavaScript cues now fail closed at runtime.
- Remaining: Verify supported Android vibration and actual command lifecycle. HTTP acceptance is not actuator readback.

### RayanLamp: `webui/src/i18n.test.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1c72c37780aeefe39c4d571636616e10c4bbc0fb0b57df8332e9b62c144d599b`.

- Target: `tests/rayanlamp_locale_test.mjs:1 [UTF8-LF sha256=e53b09f80c07b61af0bc9dd41e50d6ff113572a42e99359616aba92725b9a30c; anchor=Actual RayanLamp i18n.test.tsx contracts]`.
- Retained: Actual normalization, Persian-vs-Arabic detection, translation/placeholder/visual-number and automatic languagechange tests adapted to actual shipped code. Added explicit language/direction stability, low-write persistence, new-owner teardown, identity/draft preservation, reversible dynamic labels and detached-node pruning.
- Omitted/replaced code: `Vitest/Preact, lamp title/storage identity and secondary-language expectations`. Disposition: **replaced**. Reason: Node/VM executes the dependency-free embedded module. Pakar deliberately retains primary language, explicit direction and acknowledged board identity; tests document each difference.
- Omitted/replaced code: `Full actual native/mobile/browser pixel and cross-interface acceptance`. Disposition: **pending**. Reason: Actual DOM-bound BDI and calendar tests now execute the shipped helper and verify real summary/NTP call sites. VM DOM contracts are not rendered pixels or the full platform/interface matrix.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:67 [UTF8-LF sha256=e53b09f80c07b61af0bc9dd41e50d6ff113572a42e99359616aba92725b9a30c; anchor=Restart and idempotent teardown]`.
- Remaining: Inspect current protocol/date rendering in the browser; complete remaining consumers and native/mobile/browser matrix. VM tests are not pixels or physical board proof.

### RayanLamp: `webui/src/i18n.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `9a4843ade326c1ee323c3f60d6b0d6ac41cab9d2dcd50affed276fdd8096ff0a`.

- Target: `assets/web/localization.js:65 [UTF8-LF sha256=75db2917045580ed8dfd63eb2ecd42e54511b33e337f9855e50def8742259e6e; anchor=function startUiLocaleController()]`.
- Target: `firmware/esp8266/src/WebUi.h:965 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function renderSelectionSummary()]`.
- Retained: Actual normalization/detection, t placeholder fallback, visual digits, Intl.NumberFormat and languagechange lifecycle adapted into the real embedded localization module. Selection counts and batch readback messages are real consumers. Restart ownership, idempotent cleanup, missing/inherited placeholder handling, blocked storage and no OS-triggered preference writes are added.
- Retained: Dynamic network/cloud labels and panel menus retranslate reversibly without replacing board identity, hostnames, mapping IDs, protocol strings or unsaved secrets.
- Retained: Actual BidiLtr bdi/dir=ltr isolation and formatDateTime Persian/Gregorian calendar rules now serve the real hostname/URL summary and observed NTP time. TextContent-only rendering never parses peer HTML or rewrites protocol values.
- Omitted/replaced code: `Preact signals, lamp storage key/auto enum, any secondary Persian language and unconditional render-time persistence`. Disposition: **replaced**. Reason: ESP uses existing dependency-free state, system enum and presentation channel. Primary browser language and independent explicit direction preserve Pakar pre-paint policy. Persistence belongs only to validated preference changes; no OS/render writes or second toolchain.
- Omitted/replaced code: `Rayan Lamp document title/meta branding and lamp/coordinator/COM/URI-specific literal captions`. Disposition: **not-applicable**. Reason: These describe another application's lamp identity/endpoints. Pakar title follows independently acknowledged board application metadata; translating peer names or claiming unrelated lamp actions would be incorrect. Generic mechanisms remain independently pending.
- Omitted/replaced code: `Additional generic dictionary/settings/protocol consumers, protocol monospace styling and native/mobile locale synchronization`. Disposition: **pending**. Reason: Web hostname/URL summary now renders real BDI and NTP uses the actual calendar formatter. Remaining input/hash/status consumers, monospace styling and native/mobile/browser pixel acceptance are not implied by those two consumers or VM tests.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:55 [UTF8-LF sha256=e53b09f80c07b61af0bc9dd41e50d6ff113572a42e99359616aba92725b9a30c; anchor=Automatic locale reacts to OS changes]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:127 [UTF8-LF sha256=c77c09c64eeceed32560cea3b1b0a0ebe361fefd936a6383fe7590c5ac983aa4; anchor=Selection must not mutate hardware]`.
- Remaining: Finish remaining dynamic confirmations/errors/status and protocol inputs; validate current BDI/NTP pixels and native/mobile locale synchronization. No full localization or hardware acceptance claim.

### RayanLamp: `webui/src/store.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `d0322ac4161e8e65354f0fcf6d02ac3b5409dc9ffe5bad1f0ca892ba6e5dc32c`.

- Target: `assets/web/toasts.js:62 [UTF8-LF sha256=c416d19144ed962376079bf4aeed116ccd5aaed6c4ddc4bf63a846796f2e8d2e; anchor=function showToast(]`.
- Retained: Fully read store; actual ToastMessage fields, monotonic serial IDs, last-three-plus-new four-item queue, 4500 ms per-ID expiry and dismissToast filter adapted into canonical toasts.js.
- Retained: Added four-timer eviction, bounded text/duration, independent hover/focus holds, queue snapshots, no markup interpretation and idempotent teardown for long-lived firmware browser sessions.
- Omitted/replaced code: `Lamp stateLabel, lamp.state event and lamp-only bootstrap/state interpretation`. Disposition: **not-applicable**. Reason: No lamp get/on/off controller or lamp state contract exists on the Table/AVR board. Generic availability, diagnostics and transport ownership are separate pending work, not excluded with lamp vocabulary.
- Omitted/replaced code: `signal/computed/batch framework ownership`. Disposition: **replaced**. Reason: Actual notification queue/filter is adapted to the current DOM engine without introducing Preact to ESP. Full generic signal/configuration ownership is not claimed.
- Omitted/replaced code: `applyBootstrap/applySnapshot/beginControllerConnection/markControllerAvailable/markControllerUnavailable/derivePhase, config/revision and client ownership`. Disposition: **pending**. Reason: Need actual contracts.ts/api.ts transport comparison and mapped real Pakar capabilities, diagnostic state and epochs. Only the notification subcontract is integrated, not the entire store.
- Omitted/replaced code: `mergeRecord/updateConfig/configureSettingsPersistence/flushConfigPatch/acceptRemoteConfig/pendingSettingsPatch and theme/locale/motion persistence`. Disposition: **pending**. Reason: Useful generic draft/conflict/ownership behavior requires full applicable device/client lifecycle integration. Donor optimistic failed-patch retention/retry must not automatically replay uncertain Pakar writes; current explicit ACK/readback/no-replay contracts remain authoritative.
- Test-code evidence: `tests/toasts_test.mjs:41 [UTF8-LF sha256=df864b7b15f9f61e3f291efaf2377e02dc85d4bf35496ae05a32c447d6e96e23; anchor=evicted timers must be cancelled]`.
- Remaining: Compare actual API/contracts dependencies and adapt applicable bootstrap/revision/draft/client lifecycle with positive ACK/readback, secret handling and no uncertain replay. Finish real runtime/synchronized-client acceptance.

### RayanLamp: `webui/src/sync.test.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `6c427de4b58ce89fbea38203e8dfb575a375be943b3a7072cb96399ff4c8b6ff`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=255036e79060b6d348944e5f1d1edc90ac1c79f20dc1a095a4384b38d85d15f0; anchor=function createRealtimeRetry]`.
- Retained: Fully read actual tests. Prepare-before-reconnect/session-loss and stopped lifecycle concepts adapted to actual Pakar authenticated status GET and shipped Web connectWs/refreshAuthenticated/api execution.
- Retained: Added deterministic injected timers/RNG, jitter/cap, deadline/AbortController, constructor failure, failed reads, one owner and stale timer/promise cancellation regressions.
- Omitted/replaced code: `server event sequence epochs and duplicate ordinary events`. Disposition: **pending**. Reason: No authoritative sequence contract has been adapted yet. Port hello/reset/duplicate tests with actual Pakar event epoch ownership; do not assign synthetic sequence values just to reuse assertions.
- Omitted/replaced code: `per-target sync names, event-disabled config synchronization, lease owner, initial-sync/HTTP/follower audio policies`. Disposition: **pending**. Reason: Useful generic multi-tab/config/audio tests await their full implementation and validated dependencies. Existing independent clients and semantic haptics are not evidence of leadership parity.
- Omitted/replaced code: `marks coordinator unavailable/graceful close and renews browser session using Go API`. Disposition: **pending**. Reason: Pakar 401 status GET fails closed to login, without creating a Go session or automatically replaying mutation. Readback-before-open recovery is adapted; generic coordinator shutdown/typed availability tests remain pending.
- Omitted/replaced code: `Vitest/Preact store fixtures and TestBroadcastChannel/TestRealtimeSocket classes`. Disposition: **replaced**. Reason: Built-in Node VM and purpose-built HTTP/socket/timer fixtures execute shipped Pakar code, without adding a second framework or donor build. Channel fixtures remain pending leadership implementation.
- Test-code evidence: `tests/realtime_retry_test.mjs:73 [UTF8-LF sha256=2e0229615c0709866448b3d79eb46806a383edcb3946833d80f403c31d25af1f; anchor=Execute actual Web connectWs + refreshAuthenticated + api]`.
- Remaining: Port every useful remaining donor sequence/lease/config/audio/shutdown assertion with its actual consumer; no whole-test-file parity claim.
- Remaining: Real LAN, multi-client and native/mobile lifecycle verification remains required.

### RayanLamp: `webui/src/sync.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `bd849cbef1ee67ac97cca9e0c6e00ed69acf725d805ad986ae58f11513546d73`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=255036e79060b6d348944e5f1d1edc90ac1c79f20dc1a095a4384b38d85d15f0; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:1357 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=function connectWs()]`.
- Retained: Actual scheduleReconnect 500 ms exponential base, 10-second cap and 0.8..1.2 jitter, prepareReconnect-before-open, reset-on-open and stop cleanup adapted to actual Pakar WebSocket/status GET consumers.
- Retained: Added one pending owner, bounded exponent, late timer/promise generation checks, login/pause/pagehide cancellation and constructor-failure recovery; no device command is replayed.
- Omitted/replaced code: `start/electLeader/acquireLeadership/releaseLeadership and BroadcastChannel/Web Locks fallback lease election/heartbeats/watchdogs/connectionSyncNames`. Disposition: **pending**. Reason: Generic per-origin leadership is useful but not imported. Pakar supports independent authenticated streaming clients; a shared socket requires validated command/stream ownership and follower readback. Compare full client/contracts/store dependencies and port actual multi-tab/takeover tests before enabling.
- Omitted/replaced code: `handleServerEvent/onChannelMessage/acceptSnapshot and serverSequenceDecision`. Disposition: **pending**. Reason: Generic typed snapshot/config/diagnostic projection, revisions and epoch resets remain pending actual Pakar advertised contracts. Existing source-bound contextual readiness/no-replay ownership is retained; do not invent server event sequences or treat arbitrary channel objects as authoritative.
- Omitted/replaced code: `handleDirectStateCue initial on/off transition`. Disposition: **not-applicable**. Reason: Lamp state transitions are not table/first-wins business. Adapt only future discovered board events, not hardcode lamp state.
- Omitted/replaced code: `audioCueCount/playDirectCue, generic message/haptic/diagnostic fanout and setConnectionState/relayDisconnectReason`. Disposition: **pending**. Reason: Existing semantic haptics/toast/trace helpers do not implement full leader-only audible notification or graceful coordinator state. Generic audio/count/shutdown semantics remain useful pending exact firmware/host event and capability mapping.
- Test-code evidence: `tests/realtime_retry_test.mjs:22 [UTF8-LF sha256=2e0229615c0709866448b3d79eb46806a383edcb3946833d80f403c31d25af1f; anchor=Donor prepareReconnect is completed before any replacement socket]`.
- Remaining: Read/adapt contracts.ts, direct-api/client-factory and complete store consumers; restore generic typed multi-tab/revision/diagnostic/audio contracts with factual capabilities.
- Remaining: Verify real reconnect/session loss across LAN and native/mobile interfaces; shipped-module VM tests are not physical/network acceptance.

### RayanLamp: `webui/src/theme.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `827e41f9229227582fb78819344dceda12a9b1d581a4b73266e4780b3de2a5b8`.

- Target: `tests/rayanlamp_theme_test.mjs:1 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=Actual RayanLamp theme.test.ts scenarios]`.
- Retained: All three actual donor scenarios adapted: live System OS changes update browser chrome and preserve selection; explicit override is stable; classic pre-paint bootstrap precedes application initialization. Added old-stop/new-owner, no duplicated listeners, idempotent cleanup, cross-tab clear/malformed input, all palettes, blocked storage and no-device-call assertions.
- Omitted/replaced code: `Vitest/Preact imports, lamp-only metadata constants and index.html external classic /theme-init.js before src/main.tsx assertion`. Disposition: **replaced**. Reason: Node built-in test/VM executes the actual embedded bootstrap and controller without installing a second donor toolchain. Expected chrome values come from shared Pakar palettes. Firmware serves one HTML document; classic inline script precedes CSS/body with no extra endpoint or render-blocking HTTP fetch. Source equality and script order are asserted.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:41 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp pre-paint bootstrap runs before CSS/body]`.
- Remaining: VM/DOM doubles are not actual browser pixels or live firmware verification.

### RayanLamp: `webui/src/theme.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `b4a927e80577f163257b62a28b6df2148d10ee506f2cdeeb51e6b1666b9ce54e`.

- Target: `assets/themes/browser-appearance.js:40 [UTF8-LF sha256=a3cfd720a376fc399dd8abfef91a172180b583b828516f4d1198e4be7bd34dae; anchor=function startBrowserAppearanceController()]`.
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=0727cd7f407642d24bc72841b3613123601cde834c881f7c2dfae89d3656bde6; anchor=<script data-pakar-presentation>]`.
- Retained: Actual startWebThemeController detach/restart ownership, idempotent stop, captured listener cleanup, System-only OS reconciliation, safe stored selection and browser color-scheme/theme-color metadata adapted into startBrowserAppearanceController/applyBrowserAppearance. Used by actual pre-paint embedded Web UI, with storage-event reset handling and no hardware writes.
- Retained: Pealayer appearance helpers and four shared palettes remain the only color engine; requested theme and resolved theme are distinct root metadata. Existing form persistence skips identical writes and retains session selection on storage failure.
- Omitted/replaced code: `Preact signal state, RayanLamp storage key/default System, root.dataset.theme as requested mode, fixed lamp theme colors and applyWebTheme persistence entrypoint`. Disposition: **replaced**. Reason: Dependency-free ESP HTML uses existing browserAppearance state and pakar-appearance object with explicit Pakar dark/Studio defaults. Existing resolved root.dataset.theme is preserved for CSS; themePreference and resolvedTheme retain both states. Shared palette canvas drives browser chrome, not lamp colors. Existing form is the persistence owner; rendering and OS events never rewrite preferences.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:49 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp controller restart and idempotent teardown]`.
- Test-code evidence: `tests/web_appearance_test.mjs:93 [UTF8-LF sha256=a9dcc74b96fcd6cede05ecd4766f87cd6361e4c18f63c054d52e4de2ee01a444; anchor=unavailable storage still applies]`.
- Remaining: Real browser/device acceptance and cross-client native/mobile appearance transport remain independent requirements; this comparison covers the donor Web theme controller only.

### RayanLamp: `webui/src/toast-layout.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `34e3226492ade7bb892a13f8e5e9f97d02c3f79aa4b30df80755c5e2e583e6b6`.

- Target: `tests/toasts_test.mjs:8 [UTF8-LF sha256=df864b7b15f9f61e3f291efaf2377e02dc85d4bf35496ae05a32c447d6e96e23; anchor=function declarationsFor]`.
- Target: `assets/web/toasts.css:5 [UTF8-LF sha256=f117a82f068e3051caa053f9360617b37014f863adcf26c24bbd88358185d9b2; anchor=.app-toast{display:flex;align-items:center;]`.
- Retained: Actual declarationsFor CSS extraction and both status/copy center-axis assertions ported against shipped canonical toast CSS. Added executable ID queue/focus/hover/safe DOM and one-pass embedding regressions.
- Omitted/replaced code: `Vitest source file access against entire src/styles/app.css`. Disposition: **replaced**. Reason: Built-in Node assertions read actual assets/web/toasts.css. Relevant donor style/viewport/accessibility slices were adapted; entire app.css remains pending and is not certified by this test import.
- Remaining: Inspect real browser/native/mobile layout at narrow/wide widths; source declaration assertions and DOM doubles do not establish screenshot or physical acceptance.

## Unreviewed files are not approved omissions

Every pending path has its own CSV row. Its omission reason explicitly says there is no approved reason to leave it out. Prior keyword screening or an unbound historical adaptation note is not accepted as behavior proof. This includes all still-pending PuzzleBoard/40LED variants and ControllerBoardMini legacy files, plus unreviewed Pealayer/PCController/PU850 files, assets, dependencies, tests and scripts. Earlier focused variant notes remain useful routing evidence, not complete comparative certification.

File-by-file checklist: https://github.com/atomicdeploy/PakarController/issues/3. Master production acceptance: https://github.com/atomicdeploy/PakarController/issues/2. Run `pwsh -File tools/generate-file-comparison.ps1 -RequireComplete` to enforce the comparison gate. It must fail while any pending comparison or known source parity gap remains. Physical, deployment, visual and all-surface acceptance are separate gates even after this one passes.
