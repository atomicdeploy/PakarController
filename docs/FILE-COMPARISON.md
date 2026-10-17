# PakarController donor comparison

Whole-repository superiority is **not established**. This report exposes retained code, scoped improvements, omissions and gaps; it does not turn missing reviews into exclusions.

Routing: 1634 paths have no specialist capability match. They remain visible through `consult-reuse.ps1 -Concern donor-audit` and issue #3; lack of a keyword match is NOT an exclusion. Every CSV row includes capability routing and owning issue links.

Inventory: 4673 files; 132 complete source-file comparisons; 4541 pending comparisons; 96 compared files with known parity gaps. Header/implementation pairs can describe the same gap. Missing sparse files remain pending.

Every inventoried path, source revision/hash, target/proof file and line, omitted code, disposition and concrete reason is in `file-comparison.csv`. Hash-bound manual decisions are in `file-comparison-evidence.json`. Donor SHA-256 values bind original bytes; target/proof SHA-256 values bind UTF-8 without BOM with LF newlines. Anchors are recomputed into exact line numbers. Test-code presence is not test execution; execution receipts are in `FILE-COMPARISON-RECEIPTS.md`.

## Coverage

| Donor | Inventoried | Compared | Pending | Files with gaps |
|---|---:|---:|---:|---:|
| ControllerBoardMini | 19 | 2 | 17 | 0 |
| ControllerBoardMini/Puzzles | 18 | 12 | 6 | 7 |
| PCController | 1517 | 32 | 1485 | 25 |
| Pealayer | 639 | 46 | 593 | 34 |
| PU850-ESP | 109 | 13 | 96 | 13 |
| PuzzleBoard | 22 | 0 | 22 | 0 |
| RayanLamp | 2349 | 27 | 2322 | 17 |

## Exact reviewed-file mappings and omission decisions

### ControllerBoardMini: `Timer/LocalLib/Tasks.cpp`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `3f5ef23f7a8052d8086b2803343e3e22bb9751924b7760077950f4b0bd2cc196`.

- Target: `libraries/BoardCore/src/Tasks.h:11 [UTF8-LF sha256=1726bd6aed88da9964bfce4bbc27becb58e10f85acc4b5b04d1b20741b6e7e74; anchor=class Tasks]`.
- Target: `libraries/BoardCore/src/Tasks.cpp:7 [UTF8-LF sha256=55aac682e670f2aa62832818c60d45d508c85729aca242ff306439e9f4a1fb8b; anchor=void Tasks::update(]`.
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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

- Target: `firmware/avr/avr.ino:451 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:435 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=void boardKeyGesture(]`.
- Retained: Active-low physical plus virtual shift input state and candidate/stable transition tracking; AVR reports releases as dirty state; press handling is mode-specific.
- Retained: Actual PCController per-key callback/gesture classifier now feeds all nine inputs; no game action waits for click classification.
- Omitted/replaced code: `DEBOUNCE_MS = 50`. Disposition: **pending**. Reason: Actual PCController classifier now uses macro-configurable PAKAR_KEY_DEBOUNCE_MS (default 20ms) with tested Down/Up, click and hold timing. Physical switch-bounce measurements remain pending; faster is not certified electrically.
- Omitted/replaced code: `Key virtual readCurrentState/onStateChanged, setCallback, bool/getCurrentState interface`. Disposition: **replaced**. Reason: Imported PCController per-key classifier, setCallback and setEventCallback are retained. updateSample separates the input driver from gesture classification and supports all nine shift/ADC keys; subclass-specific raw reads are replaced by the actual board sample adapter.
- Omitted/replaced code: `Raw Puzzles lower-four input order`. Disposition: **pending**. Reason: Current PCController-based register reader reverses those bits. Physical key identity must decide the mapping; source reuse alone cannot.
- Remaining: Add input mapping/debounce configuration and bounce/press/release tests; preserve reusable callback capability or record a reviewed alternative contract.

### ControllerBoardMini/Puzzles: `LocalLib/Keys.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `caa21fe97e3df262f0821bd582b60f2895a14ec193d7d6fec13321afe4c5a46d`.

- Target: `firmware/avr/avr.ino:451 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:435 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=void boardKeyGesture(]`.
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
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=405a1d11908e89e9abee584354f49e32dd0bd65e07209f75f8b297eeef21eae1; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:21 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:28 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9f2aea4eadd0a4112ba62d4bdac906a9749026aa4e69404c2ff3169667f204df`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=405a1d11908e89e9abee584354f49e32dd0bd65e07209f75f8b297eeef21eae1; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:21 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:28 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `0f7fec7879abd26dbf2d56df28441727ea76fcb9e093f009d39f20cae77832bb`.

- Target: `firmware/esp8266/src/LedGrid.cpp:20 [UTF8-LF sha256=b8324474326127758aa38c30a5a68e30cdd30315abab314b064d955e5ea8b55c; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:62 [UTF8-LF sha256=f4b1fd7cc0e7d367f6ce5cf88ec527bba69819422d584169d785388096c45b0d; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: Final effects remain in the shared ESP engine: ESP8266 D2 or migrated ESP32-S3 GPIO4, with 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `395096029df7c507782f40c5b32c8b1dffbf2db392cb26cd2b02c824d6a36949`.

- Target: `firmware/esp8266/src/LedGrid.cpp:20 [UTF8-LF sha256=b8324474326127758aa38c30a5a68e30cdd30315abab314b064d955e5ea8b55c; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:62 [UTF8-LF sha256=f4b1fd7cc0e7d367f6ce5cf88ec527bba69819422d584169d785388096c45b0d; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: Final effects remain in the shared ESP engine: ESP8266 D2 or migrated ESP32-S3 GPIO4, with 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### PCController: `LocalLib/SevenSegments.cpp`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `34e0e8a481bc5febed33dc5610d783bb093c8be23fdbc4de796d066c1d389769`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:100 [UTF8-LF sha256=776c190eb16a8aa72ed538a4728d35bbf7e51b4b9daddbbb81e62bfce252e74b; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:267 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:492 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/SevenSegments.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `e58e1783404f494ea7b1852780b6485cbfda872781b9159b9ee29426edae9240`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:100 [UTF8-LF sha256=776c190eb16a8aa72ed538a4728d35bbf7e51b4b9daddbbb81e62bfce252e74b; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Omitted/replaced code: `New donor ATmega328P-only fast-port guard and AVR validation removal`. Disposition: **replaced**. Reason: Pakar AVR target remains explicitly ATmega328P, with its correct PC4/PC5 ports and retained sentinel/brightness validation on all branches. Removing checks only to mirror donor performance changes would weaken bounds without measured need. Other AVR targets are not advertised; future ports must bind their own pins and guard.
- Test-code evidence: `tests/pc_contract_test.cpp:267 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:492 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=display.serviceBrightness(]`.
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
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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
- Target: `firmware/avr/avr.ino:166 [UTF8-LF sha256=ef1cca7fe1006e5c5350aa3e8117a723e5197a6e70338a41e493067ded908bf3; anchor=cancelDeferredReset(); // A previously ACKed reset]`.
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
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=405a1d11908e89e9abee584354f49e32dd0bd65e07209f75f8b297eeef21eae1; anchor=startsAt = stepEndsAt_]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:21 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:28 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.setMuted(false)]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; unmute restarts the active tone.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.

### PCController: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `edb2f40cfdfb37d1c08867c121c617beb8f9f53ad2583855ae2218a07eb9fd89`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=3884bc416745c54a4cc2aed23abc60c1b284b7c67e8586d380e805811325cc1d; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=405a1d11908e89e9abee584354f49e32dd0bd65e07209f75f8b297eeef21eae1; anchor=startsAt = stepEndsAt_]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:21 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:28 [UTF8-LF sha256=64793f18a88d57e9102b7a20f04986bc6f5e250c763d524449e5e94f48191a55; anchor=player.setMuted(false)]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; unmute restarts the active tone.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.

### PCController: `Project/AutomationStore.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `09abbdbfe7fb317e7a690bb5783fe5eaca3809bb24fb0cbc8b6baf7bcc06dc5c`.

- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.cpp:223 [UTF8-LF sha256=22e9cdf2018d0247ef1c6192b50c8fb6f991ccbaecfdcbf07a00a76fe1756223; anchor=bool AutomationStore::commit(]`.
- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.cpp:310 [UTF8-LF sha256=22e9cdf2018d0247ef1c6192b50c8fb6f991ccbaecfdcbf07a00a76fe1756223; anchor=uint8_t AutomationExecutor::dispatch(]`.
- Retained: Complete actual donor implementation/header and dependency EEPROM ownership layout were read, together with complete current target and source adaptation diffs. Retains dual-bank committed-generation selection, per-record/header CRC, stable twelve-byte UART record encoding, mutation validation, ascending-slot executor, event/window budgets and recursion protection.
- Retained: Adapted actual CRC calls to shared WireCodec, supports all nine physical keys, leaves blank EEPROM volatile until an explicit edit and verifies committed bank readback before replacing active state. Architecture guard preserves this AVR EEPROM owner and native fixtures while excluding ESP8266/ESP32 transport proxies; no ESP EEPROM emulation or duplicated persistent owner.
- Omitted/replaced code: `Immediate clear/write when both EEPROM banks are invalid; publishing active generation before verifying committed bank`. Disposition: **replaced**. Reason: Pakar retains a volatile empty state until an explicit user mutation and verifies the actual inactive-bank commit before promoting it. This preserves existing EEPROM and allows failure reporting instead of falsely accepting a torn write.
- Omitted/replaced code: `Four-key eventValue bound and donor UART CRC namespace`. Disposition: **replaced**. Reason: This board has nine confirmed inputs; valid key IDs0..8 replace donor0..3. Shared WireCodec provides the same CRC algorithm without importing a second UART protocol engine.
- Omitted/replaced code: `Compiling AVR EEPROM owner into ESP transport targets`. Disposition: **replaced**. Reason: Both ESP architectures proxy PCController operations to the AVR. The observed S3 SDK EEPROM.update error exposed an overly narrow ESP8266-only guard; the guard now excludes both families and their architecture macros. AVR/host fixture code remains intact.
- Test-code evidence: `tests/pc_contract_test.cpp:127 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=void macroAndAutomations() {]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:43 [UTF8-LF sha256=9fd89470c5dae0b5877a1c823797c4c64fcbb580d5a958bd7ad84939326b44d2; anchor=assert.match(automationStore,]`.
- Remaining: Run exact-source AVR firmware and integrated ESP32-S3/ESP8266 builds, physical automation/EEPROM readback and interruption tests on the actual board. Existing host fixtures/source syntax checks are not physical EEPROM, power-loss, actuator or integrated SDK acceptance.

### PCController: `Project/AutomationStore.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `adc48b05b237bac5093e748e820d06515efe991e4fef2a696dbdbecd262f658a`.

- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.h:63 [UTF8-LF sha256=881d09b14a807e8907209e6e14aa87d630313051180f01975b88d0c21b64d98f; anchor=class AutomationStore {]`.
- Target: `libraries/BoardCore/src/pccontroller/AutomationStore.h:141 [UTF8-LF sha256=881d09b14a807e8907209e6e14aa87d630313051180f01975b88d0c21b64d98f; anchor=class AutomationExecutor {]`.
- Retained: Complete actual donor/current header read. Retains event/action enums and allowed flags, packed twelve-byte UART record static assertion, compact stored record/header layouts, dual-bank API, executor budgets and explicit action callback. Adds initialized_ to represent volatile blank storage truthfully.
- Omitted/replaced code: `Unrelated ProjectConfig include and donor RF/reset/status-profile EEPROM allocation`. Disposition: **replaced**. Reason: Pakar uses its existing explicit disjoint settings/automation/Prog layout, not the donor's other-board persistent image. The new architecture exclusion applies to the implementation, not the portable record contract; optional hardware actions remain gated by actual runtime capabilities.
- Test-code evidence: `tests/pc_contract_test.cpp:145 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=AutomationRecord rec{]`.
- Remaining: Run exact-source AVR firmware and integrated ESP32-S3/ESP8266 builds, physical automation/EEPROM readback and interruption tests on the actual board. Existing host fixtures/source syntax checks are not physical EEPROM, power-loss, actuator or integrated SDK acceptance.

### PCController: `Project/SettingsStore.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `7309fd36f0680722a4260e8f1251ae5a0fc67e99fc60f2426bb1294f789e87d4`.

- Target: `firmware/esp8266/esp8266.ino:752 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=const LedPreferences value = whiteBootPreferences(grid.brightness())]`.
- Target: `firmware/esp8266/src/LedPreferences.h:28 [UTF8-LF sha256=bb0a86a9b0506953270c5bf58f0fb9a49d5e6990b67f3adbd1b22d7e3a71d6cc; anchor=inline bool applyLedBootPreferences(]`.
- Retained: Fully read actual current SettingsStore.cpp/header through EOF and its EEPROM ownership layout. This mapping is only the saveNow changed-byte write-avoidance mechanism, adapted to a complete six-byte LittleFS record comparison before opening with truncation; it does not claim the complete donor settings owner was moved into ESP.
- Retained: The existing private LED record/checksum/layout remains six bytes. saveLedPreferences stores runtime-none with the chosen brightness, so transient effects cannot become boot animations and identical bytes cause no flash write. Valid old none/rainbow/fire records retain brightness but reset all80 cells/480 pixels to white; invalid magic/checksum/brightness/pattern records are rejected without changing the current grid.
- Omitted/replaced code: `Donor AVR EEPROM address/layout,14-page motion-alias normalization, relay/PWM/motion/sensor restore and temperature/name records`. Disposition: **replaced**. Reason: LED preferences live in ESP LittleFS, not the donor AVR record. Copying the address/layout or actuator settings would collide with the separate Pakar AVR owner and imply absent hardware. Applicable board settings have an independent historical manifest; this narrow mapping neither replaces it nor certifies those full current-source contracts.
- Omitted/replaced code: `Dirty1500ms coalescing service, complete settings normalization and field-level persistence lifecycle`. Disposition: **pending**. Reason: Identical-record skipping addresses repeated unchanged flash writes only. Changed brightness still uses the existing explicit save path, not the donor full coalescing/state machine; generic settings-owner parity needs separate consumer and interrupted-save tests.
- Omitted/replaced code: `LittleFS changed-record transactional pending/rename, write/close/readback failure propagation and exact unchanged-write count fixture`. Disposition: **pending**. Reason: The six-byte path still opens changed records with write/truncate and does not report a verified durable commit. Pure grid/record tests and static callsite checks do not prove filesystem power-loss recovery or actual flash-write counts. Do not claim crash-safe LED persistence from the checksum or EEPROM donor algorithm.
- Test-code evidence: `tests/led_grid_test.cpp:12 [UTF8-LF sha256=136526419cbf0b2985119d24117082970c0cececc9d57c4b232319434a34da07; anchor=Boot policy accepts existing files without restoring a saved animation]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:48 [UTF8-LF sha256=9fd89470c5dae0b5877a1c823797c4c64fcbb580d5a958bd7ad84939326b44d2; anchor=whiteBootPreferences]`.
- Scoped advantage: White is the deliberate boot presentation; runtime animation commands remain available without becoming a persistence side effect.
- Remaining: Verify final guarded S3/ESP8266 SDK builds and actual white boot plus retained brightness after power cycle. Add bounded filesystem failure/write-count fixtures and transactional persistence before claiming durable-save parity; no reboot or optical success inferred from host doubles.

### PCController: `Project/StatusLedController.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `b68da7e21af4396027c0f7ed7267d4c778292da47c19f1b9172bb4547ed1f53d`.

- Target: `firmware/esp32s3/src/StatusRgb.h:108 [UTF8-LF sha256=fc589df726f9c3892194ecc3eb7c679a41881ae2471c6a76e995d1e006c8be50; anchor=bool service(uint32_t now, Writer writer)]`.
- Retained: Full header/implementation read through EOF. Actual absolute elapsed/period evaluation, idempotent state ownership and already-scaled rendered RGB reporting adapted into the S3's board-owned semantic engine. Added bounded 300ms eased state transitions, explicit pin safety and driver-accepted output revision; no independent client-side animation reconstruction.
- Omitted/replaced code: `PwmController/EEPROM/StatusProfileAddress dependencies and sixteen-bit phase clock`. Disposition: **replaced**. Reason: Those target AVR RGB PWM wiring and EEPROM allocation, not the identified S3 addressable pixel. Existing S3 persisted configuration owns enable/brightness, writer acceptance owns reported output, and the 32-bit elapsed timer advances without per-loop phase drift.
- Omitted/replaced code: `profile/setProfile/defaultProfile, applyRequested/cancelEffect, playCue priority and finite Flash/Cycle/Transition descriptors`. Disposition: **pending**. Reason: This source slice implements lifecycle semantic light, not every manually controllable donor RGB behavior. Preserve pending custom profiles/cues/descriptors and their exact owner tests rather than replacing them with optimistic UI controls.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:39 [UTF8-LF sha256=9afb15932b6efd3c82928e727894e9658e47ced87829ba09bd7bc802826ebca1; anchor=No duplicate state events]`.
- Remaining: Implement useful generic omitted RGB profile/manual/cue contracts; measure physical GPIO/RGB order and latency under simultaneous matrix/network/AVR work. Native writer doubles are not optical acceptance.

### PCController: `Project/StatusLedController.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `afbc70e3e8674c80d779168c37dc7430ab058e35baf2705b4b45cc11dd4e22ca`.

- Target: `firmware/esp32s3/src/StatusRgb.h:12 [UTF8-LF sha256=fc589df726f9c3892194ecc3eb7c679a41881ae2471c6a76e995d1e006c8be50; anchor=class StatusRgb]`.
- Retained: Full header/implementation read through EOF. Adapted nonblocking lifecycle state, absolute elapsed/period animation, repeat-state phase preservation, brightness-scaled rendered-byte reporting and explicit board-owned configuration into PakarS3::StatusRgb.
- Omitted/replaced code: `AVR PwmController channels13..15/Power signal driver, EEPROM profile records and uint16 clock`. Disposition: **replaced**. Reason: The S3 owns a separate explicitly confirmed addressable pixel, not PCController PWM channels or the AVR EEPROM map. Its current network configuration owner persists enable/brightness and uint32 elapsed arithmetic avoids the AVR clock restriction; no electrical pin probing is performed.
- Omitted/replaced code: `19-condition editable profile catalog, temporary cue composition/priority, retained manual descriptors and Flash/Cycle/Transition effects`. Disposition: **pending**. Reason: Current semantic states use smooth transitions and breathe, not the full generic donor manual/profile/cue contract. Custom descriptors, priorities, persistence and native/Web/mobile/front-panel editors remain useful work; fixed semantic profiles do not certify full donor parity.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:42 [UTF8-LF sha256=9afb15932b6efd3c82928e727894e9658e47ced87829ba09bd7bc802826ebca1; anchor=New mode cross-fades from actual frame]`.
- Remaining: Finish generic manual/profile/cue capabilities with persisted ACK/readback across all clients; verify physical onboard output and safety/error/update state priorities under real peer loss.

### PCController: `Project/StatusLedMath.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `997e15f12ca1d51689567f3a15b2f3b716261173878797cd5e68866289724740`.

- Target: `firmware/esp32s3/src/StatusRgb.h:161 [UTF8-LF sha256=fc589df726f9c3892194ecc3eb7c679a41881ae2471c6a76e995d1e006c8be50; anchor=static uint8_t scale(]`.
- Retained: Full original source read through EOF. Actual unsigned byte-exact scale(value, level) and interpolate(from, to, phase) arithmetic copied into the S3 semantic onboard-pixel engine, independent of the GPIO4 matrix.
- Omitted/replaced code: `phaseDeadline distributing 64 phases in a modulo-16-bit AVR clock`. Disposition: **replaced**. Reason: S3 evaluates the present phase directly from uint32 elapsed milliseconds rather than scheduling 64 deadline steps. Late service does not slow the animation; native tests cover real production arithmetic and millis wrap, not optical output.
- Test-code evidence: `tests/s3_status_rgb_test.cpp:35 [UTF8-LF sha256=9afb15932b6efd3c82928e727894e9658e47ced87829ba09bd7bc802826ebca1; anchor=Already-scaled output bytes]`.
- Remaining: Verify exact optical RGB order, configured onboard GPIO and animation timing on the identified S3 under simultaneous Wi-Fi/matrix load; source and writer acceptance are not physical light proof.

### PCController: `Project/UartProtocol.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `bd2d1b8586622e7cc5a2712402b8832519745c7403b0326097dfd4dda07efb2f`.

- Target: `libraries/BoardCore/src/SerialLink.h:26 [UTF8-LF sha256=63f84c4c294b7c3a84a5170048a453097ee0ff6452e637c1e0b686a120507bbe; anchor=UartFlow::receiveBudget(serial_.available())]`.
- Target: `libraries/BoardCore/src/UartFlow.h:25 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=constexpr uint16_t serviceBudget = 64]`.
- Retained: Actual entry-occupancy receive snapshot, drop-until-delimiter recovery, bounded framing and synchronous response behavior adapted to shared SerialLink. Added64-byte cap for ESP32's larger RX ring and explicit negative-read handling; CRC/COBS code unchanged.
- Retained: Hardware UART remains on AVR; buffered unsolicited sends require enough TX ring space for a complete maximum frame. Failed game-state sends keep stateDirty armed. Busy-wire/polling behavior and default retained.
- Omitted/replaced code: `Zero-copy Frame view, in-place RX decode, raw_ TX/macro scratch and writeCobs streaming encoder`. Disposition: **pending**. Reason: Useful AVR stack/SRAM optimization remains pending. Pakar's current Frame is a bounded owning 48-byte payload and synchronous nested responses cannot overwrite it; retain tested ownership until optimizing the actual shared codec/consumers and stack budget. No full memory-efficiency parity claim.
- Omitted/replaced code: `framingErrors(), crcErrors(), responseErrors() separate counters`. Disposition: **pending**. Reason: Existing shared combined errors and S3 hardwareErrors are retained. Separate counter taxonomy and peer/client telemetry need full validated layout adaptation; this bounded flow change does not invent or renumber remote fields.
- Omitted/replaced code: `Event high-bit timestamp, BuzzerChanged and timed Ack/ErrorResponse suffixes`. Disposition: **pending**. Reason: Donor raw operation/schema values differ from the established Pakar envelope and explicit PCController adapter. Blindly appending suffixes would change ACK matching and nested contract semantics. Read full ProtocolRuntime/ControllerEvents and clients, then integrate timestamped projection through the explicit adapter.
- Omitted/replaced code: `advisory unknown reserved envelope byte acceptance`. Disposition: **replaced**. Reason: Pakar deliberately requires its fixed zero reserved byte and valid CRC, per living API fail-closed integrity policy. No numbered generation negotiation or compatibility alias is introduced. Existing protocol_test preserves CRC-correct nonzero rejection; optional opcodes still reach semantic dispatch.
- Test-code evidence: `tests/serial_link_test.cpp:44 [UTF8-LF sha256=0bd90ca96053cfdd5369b155f848b7823f250cddb951d2d1ac7c2d79e12ec2b0; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: Buffered AVR profile is distinct and explicitly rejects ESP8266 targeting; no blind write replay.
- Remaining: Paired S3 application integration, reconnect/readback ownership and physical stress; build flag is not hardware acceptance.
- Remaining: Complete zero-copy/counter/timestamp projection pending above; actual donor protocol registry is not imported wholesale.

### PCController: `Project/UartProtocol.h`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `287a95a65a9b9655e64a15ece051bc792a06564b648a1b53eed4f373c564e458`.

- Target: `libraries/BoardCore/src/SerialLink.h:26 [UTF8-LF sha256=63f84c4c294b7c3a84a5170048a453097ee0ff6452e637c1e0b686a120507bbe; anchor=UartFlow::receiveBudget(serial_.available())]`.
- Target: `libraries/BoardCore/src/UartFlow.h:25 [UTF8-LF sha256=44f8638f725c9cf83a6f53639144f107aace1556dd254d6dcebcaed4853c6469; anchor=constexpr uint16_t serviceBudget = 64]`.
- Retained: Fully read actual UART API and synchronous payload lifetime contract. Shared SerialLink keeps AVR HardwareSerial ownership, separate RX/TX frame storage and handler/opaque context; Stream constructor preserves the S3 explicit pins/buffers.
- Omitted/replaced code: `Zero-copy Frame view, in-place RX decode, raw_ TX/macro scratch and writeCobs streaming encoder`. Disposition: **pending**. Reason: Useful AVR stack/SRAM optimization remains pending. Pakar's current Frame is a bounded owning 48-byte payload and synchronous nested responses cannot overwrite it; retain tested ownership until optimizing the actual shared codec/consumers and stack budget. No full memory-efficiency parity claim.
- Omitted/replaced code: `framingErrors(), crcErrors(), responseErrors() separate counters`. Disposition: **pending**. Reason: Existing shared combined errors and S3 hardwareErrors are retained. Separate counter taxonomy and peer/client telemetry need full validated layout adaptation; this bounded flow change does not invent or renumber remote fields.
- Omitted/replaced code: `Event high-bit timestamp, BuzzerChanged and timed Ack/ErrorResponse suffixes`. Disposition: **pending**. Reason: Donor raw operation/schema values differ from the established Pakar envelope and explicit PCController adapter. Blindly appending suffixes would change ACK matching and nested contract semantics. Read full ProtocolRuntime/ControllerEvents and clients, then integrate timestamped projection through the explicit adapter.
- Omitted/replaced code: `advisory unknown reserved envelope byte acceptance`. Disposition: **replaced**. Reason: Pakar deliberately requires its fixed zero reserved byte and valid CRC, per living API fail-closed integrity policy. No numbered generation negotiation or compatibility alias is introduced. Existing protocol_test preserves CRC-correct nonzero rejection; optional opcodes still reach semantic dispatch.
- Test-code evidence: `tests/serial_link_test.cpp:44 [UTF8-LF sha256=0bd90ca96053cfdd5369b155f848b7823f250cddb951d2d1ac7c2d79e12ec2b0; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: Bounded large-ring RX, negative availability/read rejection and paired buffered build mode without busy GPIO.
- Remaining: Optimize zero-copy/storage only with full shared consumer and AVR stack/RAM proof; no claim of donor-wide protocol parity.
- Remaining: Finish separate counters and timestamped event contracts, and physical UART/LED/OTA contention acceptance.

### PCController: `Tools/Controller/docs/Go-Embedding-API.md`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4d8ff85c9c44092100d4f349f20eb5493d1516d064dd03d0f6b8d1010e7b6795`.

- Target: `docs/API-POLICY.md:1 [UTF8-LF sha256=8597dc02ab8d2d379b964f79a05f512768f8a0dbff33b0b8d095ec4510da67d9; anchor=# One living API contract]`.
- Target: `tools/check-living-contract.mjs:7 [UTF8-LF sha256=238b20f9f3915ea76759d303e57ad25ce9dfed9a9dc0da14fbbf89de0eae25d4; anchor=export function violations]`.
- Target: `desktop/src/capabilities.rs:32 [UTF8-LF sha256=cc3e3b3ffbfb3c776639b64326ddd90125285a389688dcc56ec147b3cecf80ed; anchor=pub fn known(]`.
- Retained: Actual living unversioned contract policy, additive optional data, stable feature IDs and strict critical identity/auth/routing/replay/deadline/integrity rejection applied across firmware and all clients.
- Omitted/replaced code: `host.New/Start/Stop, Host.Call/Events and foreign-function host lifecycle ownership`. Disposition: **pending**. Reason: Pakar is ESP/AVR plus Rust/Flutter, not the Go embedded host. Importing the entire Go lifecycle without a shared-runtime adapter would create a second UART owner. Generic typed host lifecycle, notifications and library integration remain useful and pending; only the exact API policy is completed here.
- Test-code evidence: `tests/living_contract_test.mjs:26 [UTF8-LF sha256=55c421193701d61071019dccee676a0f8e7388518992dbb30d0ac8c3ddcbb676; anchor=Living-contract guard negative fixtures]`.
- Test-code evidence: `tests/web_context_test.mjs:50 [UTF8-LF sha256=6ffe93034ab3cd0e92608fb8b4137621373c84e958c599904ddb4fea0b125080; anchor=Additive metadata does not select another generation]`.
- Remaining: Compare and integrate applicable canonical host lifecycle and typed RPC/event ownership through an explicit adapter; not certified from this policy-only change.

### PCController: `Tools/Controller/internal/discovery/discovery.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4ca50ce622f998f1922a828350ad71aea5fa5dee7506069ca3df2e857536198e`.

- Target: `mobile/lib/discovery.dart:16 [UTF8-LF sha256=c02419074d5e3c47d2a967f3ed818e3a6196ab50c94d412d6dbae18f54050914; anchor=class DeviceDiscovery]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/DiscoveryChannel.kt:15 [UTF8-LF sha256=b6e39b4cce47a1eb35051ed1f007046e08d4cf476354db243ca89b948ae5f0f1; anchor=class DiscoveryChannel]`.
- Target: `mobile/lib/devices_page.dart:7 [UTF8-LF sha256=077c722018b5849d5d972f228b326151da15cc4238c3cd54052219df4d8d59db; anchor=class DevicesPage]`.
- Retained: Fully read source through EOF. Adapted actual mDNS/DNS-SD single transport intent, metadata-only discovery, bounded search lifetime/cleanup, M-SEARCH headers, validated origins and explicit authentication separation into Android discovery.
- Retained: Android NsdManager replaces Go zeroconf; serialized resolves, scan generation ownership, multicast-lock acquisition/release and lifecycle cleanup are real native code. Local/remote saved device types describe transport, never fabricate capabilities. Actual peer ACK remains authoritative.
- Retained: Canonical discovery.go783lines fully reread through EOF. Android NSD/SSDP/MNDP mappings remain metadata-only partial adaptations; advertisements do not grant authenticated board ownership.
- Omitted/replaced code: `Advertiser/UpdateText/SSDP alive/byebye, public enrichment/mergeInstances and TXT metadata normalization`. Disposition: **pending**. Reason: Android bridge is deliberately authenticated loopback-only. Publishing a LAN-reachable phone URL would be false; an opt-in LAN/TLS listener and advertisement lifecycle need a separate implementation. Advertised names alone cannot merge authenticated board identity. PublicInfo enrichment dependencies are not yet fully reviewed.
- Omitted/replaced code: `WSDiscovery, broadcast and NetBIOS adapter consumers`. Disposition: **pending**. Reason: The current phone implements requested DNS-SD/SSDP/MNDP browsing, not every PCController discovery wire adapter. Android-specific permission/network ownership and complete adapter tests remain useful work.
- Omitted/replaced code: `PCController product service type and Go zeroconf goroutines`. Disposition: **replaced**. Reason: Android's platform NSD resolves Pakar/HTTP/PCController metadata without importing a Go runtime or copying numbered product contract names. Advertisements never assert generic PCController RPC compatibility.
- Omitted/replaced code: `ActiveProtocols/Failures degraded startup; instance.id SSDP UUID;64-record255-byte secret-filtered TXT refresh; eight-slot enrichment and multi-product browsing`. Disposition: **pending**. Reason: Phone still has no donor LAN advertiser/UpdateText lifecycle, public enrichment/identity merge or complete WSDiscovery/broadcast/NetBIOS consumers. Native USB-only mode must invoke no discovery/public HTTP enrichment. Never merge verified identity from names or claim merely configured responders started.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:183 [UTF8-LF sha256=e233594eaa2763444ec62cd59dd06245f08caac290938022e3475ddad9de1acc; anchor=Actual loopback bridge requires bearer]`.
- Remaining: Real Android multicast/router/AP-isolation, NSD failed/partial resolves and identity verification; optional authenticated LAN/TLS app advertiser; all outstanding donor discovery adapters.

### PCController: `Tools/Controller/internal/hostbridge/buzzer_mirror_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `d92d0826cd9665520087a3b5704ed874ee5c605695d571975b3c15b7a54aa84a`.

- Target: `mobile/lib/mirroring.dart:72 [UTF8-LF sha256=996e6ca2d96e8f4b6999439fb775d59612e0ed575f514b38ffbef0b0ab10ce4d; anchor=class ToneTimeline]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/ToneMirrorChannel.kt:22 [UTF8-LF sha256=48934806ea64dc02be68fbdf73978d65337fdb36dd94482749c14422aaf8dd20; anchor=class ToneMirrorChannel]`.
- Target: `firmware/avr/PcControllerRuntime.cpp:581 [UTF8-LF sha256=43be8fbede226483a9a71c6459a97cbb41a331a6b802c004f23ac981058990e8; anchor=put32(audio + 5, s.audioStartedMicros);]`.
- Retained: Fully read actual current donor through EOF. Adapted uint32 MCU timestamp wrap, 50ms playout delay, 250ms skew bound, absolute note deadlines, source-generation cancellation, independent opt-in and distinct failure transitions into real Android mirroring.
- Retained: Donor cadence/stop/reconnect tests adapted to a silent injectable Flutter output. Actual PCM Android renderer uses bounded 5ms buffers, exact requested Hz, click-reducing envelope, finite deadline, focus/lifecycle cancellation and an independent six-second native receive lease.
- Retained: Canonical buzzer_mirror_test.go539lines fully reread through EOF; existing Flutter fixture mappings remain bounded historical adaptations, not execution of refreshed Go tests.
- Omitted/replaced code: `Go multi-source pending/replacement sorting, Linux KDMKTONE/external beep drivers and bridge ingress`. Disposition: **replaced**. Reason: Android mirrors only the explicitly selected peer. Independent session tokens and selected board/boot/AVR-session ownership replace multi-source scheduling; Android AudioTrack PCM replaces incompatible PC-speaker drivers. No background service or remote note ingestion is silently enabled.
- Omitted/replaced code: `Native receiver physical waveform/latency and complete all-host audio-domain parity`. Disposition: **pending**. Reason: No connected ADB phone or physical board acceptance yet. Native/Web do not yet consume the new timestamped audio stream; ESP32-S3 integration remains separate pending work. Phone synthesis matches reported pitch, not piezo timbre or measured Timer1 quantization.
- Omitted/replaced code: `Fresh10ms/1ms recovery; scheduler handoff; stale-note rejection; no backend reprobe; same-source playback/stop/reconnect purge fixtures`. Disposition: **pending**. Reason: Current Flutter fixtures do not implement all refreshed short-note recovery/source-wide scheduler assertions. These useful regression imports remain pending with silent outputs. No donor suite or physical audio was run by this review.
- Test-code evidence: `mobile/test/mirroring_transport_test.dart:14 [UTF8-LF sha256=5b86dae832e659a5f2a1cb5d3587ae4d3e1a354dfae33c62b14dfed3ccc1f8cf; anchor=Actual authenticated WebSocket]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:81 [UTF8-LF sha256=43df9cb38f7b52892a66db911dfd2a6c0317c68e580247536cdf8a6cd61c1eaf; anchor=Actual donor cadence]`.
- Test-code evidence: `tests/pc_contract_test.cpp:291 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=Appended actual musical start]`.
- Remaining: Install APK on authorized phone, deploy integrated ESP/AVR together after backups; measure welcome cadence, background/focus/long-tone/disconnect cutoff, all actual front-panel keys and segment masks.
- Remaining: Complete S3 shared firmware integration, multi-source/background receiver if requested and network bridge note forwarding without echo/replay.

### PCController: `Tools/Controller/internal/hostbridge/buzzer_mirror.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `dcb9bf3c1a31cb6cc06f4f5ed52def32b7388fd4d33587e233476a3a76e1f032`.

- Target: `mobile/lib/mirroring.dart:72 [UTF8-LF sha256=996e6ca2d96e8f4b6999439fb775d59612e0ed575f514b38ffbef0b0ab10ce4d; anchor=class ToneTimeline]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/ToneMirrorChannel.kt:22 [UTF8-LF sha256=48934806ea64dc02be68fbdf73978d65337fdb36dd94482749c14422aaf8dd20; anchor=class ToneMirrorChannel]`.
- Target: `firmware/avr/PcControllerRuntime.cpp:581 [UTF8-LF sha256=43be8fbede226483a9a71c6459a97cbb41a331a6b802c004f23ac981058990e8; anchor=put32(audio + 5, s.audioStartedMicros);]`.
- Retained: Fully read actual current donor through EOF. Adapted uint32 MCU timestamp wrap, 50ms playout delay, 250ms skew bound, absolute note deadlines, source-generation cancellation, independent opt-in and distinct failure transitions into real Android mirroring.
- Retained: Donor cadence/stop/reconnect tests adapted to a silent injectable Flutter output. Actual PCM Android renderer uses bounded 5ms buffers, exact requested Hz, click-reducing envelope, finite deadline, focus/lifecycle cancellation and an independent six-second native receive lease.
- Retained: Canonical buzzer_mirror.go435lines fully reread through EOF. Existing Android timeline/peer-owner mappings remain partial retained adaptations, not parity with the refreshed scheduler/backend lifecycle.
- Omitted/replaced code: `Go multi-source pending/replacement sorting, Linux KDMKTONE/external beep drivers and bridge ingress`. Disposition: **replaced**. Reason: Android mirrors only the explicitly selected peer. Independent session tokens and selected board/boot/AVR-session ownership replace multi-source scheduling; Android AudioTrack PCM replaces incompatible PC-speaker drivers. No background service or remote note ingestion is silently enabled.
- Omitted/replaced code: `Native receiver physical waveform/latency and complete all-host audio-domain parity`. Disposition: **pending**. Reason: No connected ADB phone or physical board acceptance yet. Native/Web do not yet consume the new timestamped audio stream; ESP32-S3 integration remains separate pending work. Phone synthesis matches reported pitch, not piezo timbre or measured Timer1 quantization.
- Omitted/replaced code: `Fresh short-note/scheduler/timer rebuffering; source-wide queued-plan rebasing; original receipt-time and ready backend reuse without KDMKTONE reprobe`. Disposition: **pending**. Reason: Current mobile ToneTimeline spends age from its cushion and can discard delayed short notes; no rebaseForStart equivalent exists. Import bounded fresh-note recovery with silent fixtures without rehabilitating stale observations. Native backend reuse is useful when implemented; unrelated settings must not probe live speakers.
- Test-code evidence: `mobile/test/mirroring_transport_test.dart:14 [UTF8-LF sha256=5b86dae832e659a5f2a1cb5d3587ae4d3e1a354dfae33c62b14dfed3ccc1f8cf; anchor=Actual authenticated WebSocket]`.
- Test-code evidence: `mobile/test/mirroring_test.dart:81 [UTF8-LF sha256=43df9cb38f7b52892a66db911dfd2a6c0317c68e580247536cdf8a6cd61c1eaf; anchor=Actual donor cadence]`.
- Test-code evidence: `tests/pc_contract_test.cpp:291 [UTF8-LF sha256=a551e4b70ad1fe91670490dba2f583cc9aa4e03d03284976b8df8603a0c4c063; anchor=Appended actual musical start]`.
- Remaining: Install APK on authorized phone, deploy integrated ESP/AVR together after backups; measure welcome cadence, background/focus/long-tone/disconnect cutoff, all actual front-panel keys and segment masks.
- Remaining: Complete S3 shared firmware integration, multi-source/background receiver if requested and network bridge note forwarding without echo/replay.

### PCController: `Tools/Controller/internal/ipcjson/rest_routes_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `729b980122ce5e3b3024544c9229d52ce03647bf195d62958fa388304d44fbe8`.

- Target: `libraries/BoardCore/src/ApiPolicy.h:8 [UTF8-LF sha256=ee7dcf1c76d2d1703ae2d8d11ab0feff19650f08e8cfddfa03faac17ba6a163c; anchor=inline bool numberedRoute]`.
- Target: `firmware/esp8266/src/WebAccess.h:15 [UTF8-LF sha256=663dacf63a1457835946b5d9fef484585fd2d6a8ce08beda647e885fe7c8cf10; anchor=ApiPolicy::numberedRoute]`.
- Retained: Actual TestVersionedCoreAPIRoutesAreRejected test cases adapted into the C++ helper exercised by ESP async middleware; generalized to all numbered API roots and pre-auth rejection.
- Omitted/replaced code: `TestCanonicalRESTRouteInventory, TestRESTAppActionCannotInjectCoordinatorNavigationMetadata, typed app launch/action outcome lifecycle`. Disposition: **pending**. Reason: These exercise the full PCController Go host coordinator, not the ESP handlers. Applicable generic typed action/outcome/auth ownership and full real multiplexer route inventory still need explicit Pakar contracts and tests. The narrower source/wire tests here do not claim end-to-end async server acceptance.
- Test-code evidence: `tests/protocol_test.cpp:30 [UTF8-LF sha256=69424ba85e58733026f42c92802a0239702c1719b283d3d22a8a05eb97eb92c6; anchor=Actual donor TestVersionedCoreAPIRoutesAreRejected cases]`.
- Test-code evidence: `tests/living_contract_test.mjs:26 [UTF8-LF sha256=55c421193701d61071019dccee676a0f8e7388518992dbb30d0ac8c3ddcbb676; anchor=pre-auth 404 wiring]`.
- Remaining: Port applicable real top-level route inventory, typed app outcomes and coordinator-owned metadata spoof tests after mapping the actual Pakar host coordinator.

### PCController: `Tools/Controller/internal/link/request_admission_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `0148ea4aac3cf230050045756a4308157adf786a2b6ca2d4fd99bcf665a622c1`.

- Target: `desktop/src/connection.rs:25 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=pub struct Gate]`.
- Target: `desktop/src/main.rs:1045 [UTF8-LF sha256=65e92fb29dd0b55895c725be9f78fa8a868366d30fd7aa0b0cf9284fe8bac0a0; anchor=fn serial_worker(]`.
- Retained: Fully read all489lines through EOF. Adapted queued-intent revocation/no-late-write and ownership-before/after admission assertions as Rust selected-method epoch tests. The production Gate keeps revoked owners until their resource leases drop and never admits a competing route or a second USB handle.
- Retained: New main fixture invokes the actual HTTP worker against a live loopback listener while USB is selected, asserting zero accepted connections and zero failure events. This is an unexecuted source fixture reference until the guarded owner runs tests, not live USB or electrical proof.
- Omitted/replaced code: `Priority/background strip scheduling,750ms ACK bound, active-opcode wait diagnostics and delayed/concurrent exact ACK matching`. Disposition: **pending**. Reason: A mutually exclusive route lease does not implement the donor's one-command-on-wire response scheduler or exact opcode/sequence ACK admission. Preserve these useful serial contracts for the hardware-matched host transport rather than declaring all session tests imported.
- Omitted/replaced code: `PulseDTR cancellation/restoration/write exclusion and live physical port acquisition/late-close tests`. Disposition: **pending**. Reason: This pass intentionally adds no automatic reset and keeps AVR/ISP/audio deferred. Inactive DTR on Rust open is source configuration, not measured electrical behavior; OS acquisition/cancellation/retryable-close ownership still needs dedicated fake-handle and real-host acceptance.
- Test-code evidence: `desktop/src/connection.rs:95 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=fn handoff_waits_for_all_actual_old_resources_and_rejects_stale_epoch()]`.
- Test-code evidence: `desktop/src/connection.rs:83 [UTF8-LF sha256=38778812b8bf03975198808c672421f808a1486c402452473af3e63a83b0eda6; anchor=fn usb_offline_never_admits_network_and_disconnect_revokes_everything()]`.
- Test-code evidence: `desktop/src/main.rs:2643 [UTF8-LF sha256=65e92fb29dd0b55895c725be9f78fa8a868366d30fd7aa0b0cf9284fe8bac0a0; anchor=fn usb_selected_http_worker_never_contacts_even_a_live_network_endpoint()]`.
- Scoped advantage: User-required exclusive Wi-Fi-or-USB lifecycle replaces Both rather than silently falling back; physical resources retire before the new method opens. Source evidence only until guarded tests and runtime observations.
- Remaining: Run guarded native tests and verify selected USB causes no HTTP/DNS/SSDP/telemetry or second physical connection; validate bounded route retirement, dirty-draft protection and retired outcomes in actual runtime. Do not infer whole donor parity or electrical acceptance from Gate tests.

### PCController: `Tools/Controller/internal/link/session_test.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `ff6c7b21f72bfb944d232c87210f8ffa139dfe016e8b5756f2582604489fffc4`.

- Target: `tests/protocol_test.cpp:13 [UTF8-LF sha256=69424ba85e58733026f42c92802a0239702c1719b283d3d22a8a05eb97eb92c6; anchor=Adapt donor matching-identity/request assertions]`.
- Target: `desktop/src/terminal_ui.rs:1303 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn server_reboot_between_preflight_and_post_stops_without_replay()]`.
- Target: `desktop/src/peer_http.rs:294 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_settings_pin_every_nested_request_and_verify_same_boot()]`.
- Retained: Actual fakePort matching/mismatched request and identity scenarios read fully and adapted to C++ peer validation and native same-host reboot/pinned-header/409 no-replay HTTP fixtures.
- Retained: Old receipts stay in Events, not new peer's confirmed output.
- Retained: Full session_test.go reread through EOF and live hash reverified. Admission revocation, peer mismatch and no-replay assertions adapted to actual native GUI HTTP loopback fixtures, not claims of importing Go fakePort acquisition/reset/ACK routing.
- Retained: Reread complete donor at unchanged live hash; mismatch/admission assertions now also execute real WebAccess middleware and AsyncHttpService with SDK/transport/auth doubles. Retired queued probe never reaches the simulated hardware call or replays. This does not validate Basic-auth parsing, physical ISP or downstream AVR epoch.
- Omitted/replaced code: `serialOpenWithPending/late acquisition/configuration cancellation/close retry, preclose ordering and terminal error ownership; corresponding fakePort tests`. Disposition: **pending**. Reason: Useful serial lifecycle/quarantine has no equivalent in HTTP-only TUI. Adapt with actual Rust USB owner before discovery/reconnect; never close another client's port.
- Omitted/replaced code: `priority requestAdmission, sequence/opcode-matched ACK/Error/events, reserved macro sequence, boot HELLO retry and recoverable decoder overflow tests`. Disposition: **pending**. Reason: HTTP fencing is not downstream AVR ACK/decoder ownership. These production behaviors and exact tests remain pending with USB/UART host integration.
- Omitted/replaced code: `compact HELLO schemas/build stamp and DTR reset/inactive control-line tests`. Disposition: **pending**. Reason: Build stamp is not board/boot identity; numbered public generation negotiation conflicts with living API. Adapt additive identity and explicit one-shot reset policy with serial owner, without schema selectors/reset-on-connect.
- Test-code evidence: `desktop/src/terminal_ui.rs:1372 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn shared_cli_mutation_inherits_pinned_epoch_headers()]`.
- Test-code evidence: `tests/peer_fence_test.mjs:20 [UTF8-LF sha256=a96ca49c6fdbc0c425f6199d6c4371a76536ca9db785e9d093047953d31a101c; anchor=Upload callbacks precede middleware]`.
- Test-code evidence: `desktop/src/peer_http.rs:323 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_preflight_reboot_or_capability_loss_never_dispatches_mutation()]`.
- Test-code evidence: `desktop/src/peer_http.rs:356 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_readback_from_new_boot_is_not_applied()]`.
- Test-code evidence: `desktop/src/peer_http.rs:415 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_unfenced_mutation_fails_before_network_and_preserves_correlation()]`.
- Test-code evidence: `desktop/src/peer_http.rs:340 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_server_boot_conflict_is_not_replayed()]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:69 [UTF8-LF sha256=5eb03895e22d12a41c817a1d198a828494a299a4ef95922a9bfa9e553a1b461e; anchor=Real async queue + real fence recheck]`.
- Remaining: GUI HTTP and Web pin adoption implemented as bounded slices; native WebSocket/USB/mobile/standalone CLI/cloud fencing, remembered pairing and AVR-only boot identity remain pending; ESP32-S3 application not integrated.
- Remaining: Port remaining useful serial lifecycle/priority/ACK/reset tests; fixtures/source/compile are not physical acceptance.

### PCController: `Tools/Controller/internal/link/session.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `1311fdcde94b2ef8ed06db63ff874a255c135dc1969709e6f43ff4b5c5c8235b`.

- Target: `desktop/src/terminal_ui.rs:804 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn command_request(]`.
- Target: `mobile/lib/api.dart:325 [UTF8-LF sha256=fcdc03cb654eabb48c94f921fb2f64e998b3d18837ff09cb03288d1c67aaf864; anchor=bool _acceptIdentity(]`.
- Target: `mobile/lib/main.dart:102 [UTF8-LF sha256=6ea527d0625585fd493101975fb55a6156d9f30a4d53430cdd494b48021cee61; anchor=api.onPeerChanged = ()]`.
- Target: `mobile/lib/bridge.dart:316 [UTF8-LF sha256=4fd40d999691968be1d32826a925a4bb221a5bd102c2a237ed947f6bf37b2be9; anchor=watchdog = Timer.periodic]`.
- Target: `libraries/BoardCore/src/PeerFence.h:31 [UTF8-LF sha256=9f1ab63252bd881d2fc786e230a18db331e66708a1bd3ccd0f096d2d2b5d49d5; anchor=inline bool accepts(]`.
- Target: `firmware/esp8266/src/WebAccess.h:47 [UTF8-LF sha256=663dacf63a1457835946b5d9fef484585fd2d6a8ce08beda647e885fe7c8cf10; anchor=bool peerAllowed(]`.
- Target: `firmware/esp8266/esp8266.ino:1070 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=bool authenticate()]`.
- Target: `assets/web/peer-http.js:32 [UTF8-LF sha256=a56aca0f2a159a0bdf22f1db0bde8541aa745693282167c28d3a0d08cb6c67d3; anchor=function assertOwner(peer)]`.
- Target: `desktop/src/peer_http.rs:73 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=pub fn admit(]`.
- Target: `desktop/src/main.rs:505 [UTF8-LF sha256=65e92fb29dd0b55895c725be9f78fa8a868366d30fd7aa0b0cf9284fe8bac0a0; anchor=fn scoped_http_request(]`.
- Retained: Actual Session.Request closing/context checks before and after admission adapted to TUI atomic stop checks before/after fresh preflight; no post-dispatch replay.
- Retained: Full source read. Fail-closed identity checks strengthened with HTTP server-side board/boot fences; donor HELLO/build stamp is NOT a boot epoch.
- Retained: Before/after admission ownership checks also adapted to Web's shared pinned HTTP preflight and post-response generation validation. Existing session.go full review/live 1bf4d52 hash remains unchanged; this is not import of Go serial acquisition or request-priority scheduling.
- Retained: Full session.go reread through EOF and live source hash reverified. Actual acquireWrite/requestAdmission before/after admission closing checks adapted to native HTTP queued generation, deadline and post-preflight ownership checks. Shared native/TUI strict Pin validation removes the duplicate validator; pinned clients refuse redirects.
- Retained: Reread complete donor at unchanged live hash for firmware admission/dispatch ownership. Supplied HTTP headers are checked for all methods including hardware-affecting GET probe; actual loop dispatch rechecks the same request before SPI/reset. Header-free compatibility is deliberately unfenced, not ownership proof.
- Retained: Fully reread current session.go and session_test.go for Android networking. Adapted actual SetBeforeClose/notifyBeforeClose synchronous retirement into mobile HTTP/WebSocket board/boot acceptance, root state revocation and discarded prior firmware owners. Learned critical identity cannot silently disappear into legacy readiness. A one-second bridge watchdog is independent from the chosen telemetry interval, and retired MQTT callbacks cannot publish through a replacement client.
- Retained: Canonical b0111ae0 session.go916lines fully reread through EOF; old1bf4d52 review remains historical. Existing bounded HTTP/mobile ownership targets do not imply current whole serial-session parity. Frozen native transport targets were separately reviewed: selected-method epoch Gate/Stamp/Lease, before-write lease checks, actual USB handle drop before handoff, HTTP/WS lease retention, no idle HTTP connection pool, no Both fallback and stale-event retirement.
- Omitted/replaced code: `serialOpenWithPending/late acquisition/configuration cancellation/close retry, preclose ordering and terminal error ownership; corresponding fakePort tests`. Disposition: **pending**. Reason: Useful serial lifecycle/quarantine has no equivalent in HTTP-only TUI. Adapt with actual Rust USB owner before discovery/reconnect; never close another client's port.
- Omitted/replaced code: `priority requestAdmission, sequence/opcode-matched ACK/Error/events, reserved macro sequence, boot HELLO retry and recoverable decoder overflow tests`. Disposition: **pending**. Reason: HTTP fencing is not downstream AVR ACK/decoder ownership. These production behaviors and exact tests remain pending with USB/UART host integration.
- Omitted/replaced code: `compact HELLO schemas/build stamp and DTR reset/inactive control-line tests`. Disposition: **pending**. Reason: Build stamp is not board/boot identity; numbered public generation negotiation conflicts with living API. Adapt additive identity and explicit one-shot reset policy with serial owner, without schema selectors/reset-on-connect.
- Omitted/replaced code: `WithRequestWriteGuard final transport admission;750ms strip ACK/background priority and active-opcode slot diagnostics`. Disposition: **pending**. Reason: Selected-method lease checks now precede normal Rust serial write calls, with old physical-owner lease retained until resource drop. Full donor request-priority scheduling, reserved macro sequence and single-inflight opcode/sequence ACK ownership are not imported by that gate; OS-inflight bytes remain non-retractable. Rust USB/PnP close/open cancellation, timing and actual no-network handoff require runtime acceptance.
- Test-code evidence: `desktop/src/terminal_ui.rs:1066 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn same_hostname_reboot_revokes_drafts_and_old_receipts()]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:36 [UTF8-LF sha256=e233594eaa2763444ec62cd59dd06245f08caac290938022e3475ddad9de1acc; anchor=Same-host board/boot replacement retires firmware owners synchronously]`.
- Test-code evidence: `tests/peer_fence_test.mjs:20 [UTF8-LF sha256=a96ca49c6fdbc0c425f6199d6c4371a76536ca9db785e9d093047953d31a101c; anchor=Upload callbacks precede middleware]`.
- Test-code evidence: `tests/web_peer_http_test.mjs:22 [UTF8-LF sha256=e6c5d2fec06ef43602252c62e341f0126c457b11c9fe5bb8a08636301ce37f0c; anchor=Same-host reboot between draft creation and preflight]`.
- Test-code evidence: `desktop/src/peer_http.rs:373 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_revocation_during_preflight_stops_before_write()]`.
- Test-code evidence: `desktop/src/peer_http.rs:393 [UTF8-LF sha256=ab52253344193953b470f963358afca041f8e0871a86d74778c91044bd5bc055; anchor=fn gui_revocation_after_dispatch_reports_uncertainty_not_success()]`.
- Test-code evidence: `tests/web_access_fence_test.cpp:69 [UTF8-LF sha256=5eb03895e22d12a41c817a1d198a828494a299a4ef95922a9bfa9e553a1b461e; anchor=Real async queue + real fence recheck]`.
- Remaining: GUI HTTP and Web pin adoption implemented as bounded slices; native WebSocket/USB/mobile/standalone CLI/cloud fencing, remembered pairing and AVR-only boot identity remain pending; ESP32-S3 application not integrated.
- Remaining: Port remaining useful serial lifecycle/priority/ACK/reset tests; fixtures/source/compile are not physical acceptance.

### PCController: `Tools/Controller/internal/tui/table.go`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `ecd399e2bf7718f3eb7b62e49f9dd884c0fb179e3d907bd15b238c01d31501e3`.

- Target: `desktop/src/terminal_ui.rs:463 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn window(total: usize, visible: usize, cursor: usize)]`.
- Retained: Actual tableWindow cursor-centred viewport arithmetic is adapted and exhaustively tested across total/visible/cursor cases. Reuses native logical selection/navigation and Unicode column clipping.
- Omitted/replaced code: `Donor gocui DataTable groups/control-tone rendering, visibleLine mouse mapping and richer table columns`. Disposition: **pending**. Reason: Only the viewport math is currently integrated; gocui APIs require crossterm adaptation, while generic groups/tones/mouse and richer settings/mapping editors remain pending. No full donor table parity is claimed.
- Test-code evidence: `desktop/src/terminal_ui.rs:1120 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn actual_donor_table_window_keeps_cursor_in_resized_viewport()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### PCController: `Tools/Controller/web/src/card-layout-controls.tsx`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `de67f2ac9dd8037e354b98227616ed8c207379dbc8205692587bc07a6392be83`.

- Target: `assets/web/device-settings.js:108 [UTF8-LF sha256=0174e9db3639482585cf498595dd38e412ba7d71b05bae8edffea15d2ee650f3; anchor=function panelToggle(]`.
- Target: `docs/INTERFACE-CONTRACT.md:50 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## Known useful parity gaps, not accepted exclusions]`.
- Retained: Actual CardLayoutEditor hidden-card show/reset and CardLayoutFrame hidden/collapsed actions adapted to the embedded workspace Panels dropdown and panel headers. Persisted preferences skip identical writes; lost capability overrides visibility.
- Omitted/replaced code: `editing/onToggleEditing and pointer-capture onReorderStart/Move/End/onLostPointerCapture; order/CSSProperties; keyboardReorderOffset dependency`. Disposition: **pending**. Reason: Show/hide/collapse/reset does not restore panel-order editing. Read actual pointer-reorder.ts completely; its stable-target and RTL keyboard helper need compatible per-container order, focus, persistence and stationary-pointer tests. Existing cell-order editing is a separate contract.
- Omitted/replaced code: `CardLayoutCopy localization and Lucide Check/ChevronDown/Eye/EyeOff/GripVertical/RotateCcw toolbar glyphs`. Disposition: **pending**. Reason: Standalone embedded controls preserve labels and actions but not the entire localized vector-icon toolbar. Bind the established controller icon catalog and localization rather than loading the React/Lucide runtime or claiming glyph parity.
- Omitted/replaced code: `React generic component props and ReactPointerEvent/CSSProperties wrappers`. Disposition: **replaced**. Reason: The ESP serves a standalone embedded document, not the donor React runtime. Existing live DOM cards retain their handlers when settings cards are reparented; no duplicate React state is introduced.
- Test-code evidence: `tests/interface_settings_test.mjs:58 [UTF8-LF sha256=7a3717629e56db13e988faf28730a6f4b358e748fbc61709b6aaf43a13f96f0e; anchor=Capability loss must override stored panel visibility]`.
- Remaining: Restore edit-mode panel ordering, actual icon/localization contract and pointer/keyboard tests; source/HTTP doubles and simulated browser proof are not physical acceptance.

### PCController: `Tools/Controller/web/src/peripheral-availability.test.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `a5d7366279057e0868478846c6bbe92eda4209a6bdbcbc16e1c149e57dfe035e`.

- Target: `tests/web_context_test.mjs:22 [UTF8-LF sha256=6ffe93034ab3cd0e92608fb8b4137621373c84e958c599904ddb4fea0b125080; anchor=const manifest=]`.
- Target: `mobile/test/peer_context_test.dart:12 [UTF8-LF sha256=8fd25b7d5f1b6392f077f72f1790ff80af2b7f54371cd35180286a34ddc8dbc5; anchor=No peer ACK cannot be inferred]`.
- Target: `mobile/test/widget_test.dart:111 [UTF8-LF sha256=fc194efef9dddcf5260af2d824d79aa3343bc6deb656544d269a78d83820f3d7; anchor=Unacknowledged board has no Table]`.
- Target: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Target: `desktop/src/capabilities.rs:223 [UTF8-LF sha256=cc3e3b3ffbfb3c776639b64326ddd90125285a389688dcc56ec147b3cecf80ed; anchor=fn serial_manifest_is_individual_and_table_modes_are_profile_scoped]`.
- Retained: Actual disconnected, missing status and capability-free cases adapted into shipped-Web tests. Independent peripheral ACKs, expiry, cross-kind and malformed geometry/schema cases added. C++ tests execute actual HELLO/display-ACK policy.
- Retained: Equivalent Flutter fixture cases execute actual capability parsing, unsupported schema/geometry, display measured ACK, per-feature cards, identity change and phone/tablet navigation; API fixtures reject pending reads/queued writes after owner or credential changes.
- Omitted/replaced code: `Vitest/liveSnapshot/emptySnapshot fixture ownership`. Disposition: **replaced**. Reason: Node VM executes embedded Web code, C++ executes firmware policy, and Rust has USB adapter tests. React/Vitest would target a nonexistent React owner in this embedded surface.
- Omitted/replaced code: `Valid/invalid INA219, temperature, PWM, LCD, Bluetooth, relay/RF/statusLED fixture cases`. Disposition: **pending**. Reason: Restore exact bounded-invalid-value cases with the genuine PCController adapter; those peripherals are currently absent, not replaced by synthetic zero fixtures.
- Test-code evidence: `tests/web_context_test.mjs:48 [UTF8-LF sha256=6ffe93034ab3cd0e92608fb8b4137621373c84e958c599904ddb4fea0b125080; anchor=Unsupported geometry is not forced]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Rust tests remain cache-budget blocked; generic adapter sensor/profile tests and physical/native acceptance remain #10.

### PCController: `Tools/Controller/web/src/peripheral-availability.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `fcf2e397564c843711fd64db2bd2d820552180ac4f5abdf0d44ccbbb86be7a84`.

- Target: `desktop/src/capabilities.rs:43 [UTF8-LF sha256=cc3e3b3ffbfb3c776639b64326ddd90125285a389688dcc56ec147b3cecf80ed; anchor=pub fn has(]`.
- Target: `mobile/lib/peer_context.dart:35 [UTF8-LF sha256=b97790f7b9ffbe47e8af864b0eed36c7f621baf38d2a84821d2370265fa3bf7d; anchor=static PeerContext fromStatus]`.
- Target: `mobile/lib/main.dart:290 [UTF8-LF sha256=6ea527d0625585fd493101975fb55a6156d9f30a4d53430cdd494b48021cee61; anchor=Future<bool> _command]`.
- Target: `libraries/BoardCore/src/DeviceContext.h:26 [UTF8-LF sha256=ce571a112f7d06dc25cdf7f35a383ba4f23c3ca4337ac1bb9968ef17dec72689; anchor=inline uint16_t features(]`.
- Target: `firmware/esp8266/src/WebUi.h:733 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function peerAvailability(]`.
- Retained: Actual connected AND have_status AND advertised-capability conjunction adapted to selected-peer fresh manifests. No feature inference from hostname, pixel count, catalog, defaults or online alone.
- Retained: TM1637 additionally requires fresh measured ACK; Text and Buzzer have independent HELLO support. Native/Web hide unrelated controls and gate domain commands.
- Retained: Flutter now adapts the same ready AND advertised schema-1 conjunction, table geometry and individual ACK policy; selected owner revisions reject old replies and retained dialogs. Unknown/stale peers have only client connection/overview navigation.
- Omitted/replaced code: `Snapshot/status/hello fields and compact status flag fallback`. Disposition: **replaced**. Reason: Pakar JSON and append-only USB share a schema-1 normalized manifest. AVR HELLO and display-status ACK are aggregated on ESP; copying PCController bit values would alias different protocols.
- Omitted/replaced code: `bounded, ina219, pwm, temperatureLED, temperatureBTAudio, lcd, bluetoothAudio, relays, rf, statusLED and invalid measurement fields`. Disposition: **pending**. Reason: Those drivers are absent on the current pair. A real PCController adapter must preserve their detection, bounds and invalid-value distinctions later. No fabricated controls are displayed.
- Test-code evidence: `tests/web_context_test.mjs:24 [UTF8-LF sha256=6ffe93034ab3cd0e92608fb8b4137621373c84e958c599904ddb4fea0b125080; anchor=No inferred board kind or controls]`.
- Test-code evidence: `mobile/test/peer_context_test.dart:28 [UTF8-LF sha256=8fd25b7d5f1b6392f077f72f1790ff80af2b7f54371cd35180286a34ddc8dbc5; anchor=Independent ACKs and measured display gate]`.
- Test-code evidence: `mobile/test/widget_test.dart:260 [UTF8-LF sha256=fc194efef9dddcf5260af2d824d79aa3343bc6deb656544d269a78d83820f3d7; anchor=Peer ACK loss removes table and independent cards]`.
- Test-code evidence: `mobile/test/api_test.dart:11 [UTF8-LF sha256=fe3ebf61af9206d9e160668662115f9f9ce1918f87668ec02ff2cbbd0637bd9f; anchor=Changed endpoint discards an in-flight status]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Native execution and generic PCController profile/discovery adapter remain #10/#6 work; source/C++/Web proof is not native or physical acceptance.

### PCController: `Tools/Controller/web/src/status-frame-batcher.test.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `61d9972be27e3ab35db5012600e8a7b5c8507815edeb55b80764357d4658d222`.

- Target: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Retained: Full source read through EOF. Actual fake-clock callback Map/cancelled-list and deliberately delivered cancelled callback scenarios ported to the actual Web streaming source.
- Retained: Generation replacement and idempotent disposal scenarios strengthened to assert stale callback cannot disarm the newer handle; new pacing, buffering, peer loss, partial-send and final-page/back-cache cases exercise real source.
- Omitted/replaced code: `100 status updates, 72-sample history, ordered batch/error-then-success and metric snapshot assertions`. Disposition: **pending**. Reason: These test a still-useful telemetry reducer not implemented in the latest-only LED scheduler. No whole-suite parity is claimed; add them alongside generic status/history imports.
- Test-code evidence: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Test-code evidence: `tests/web_access_test.mjs:88 [UTF8-LF sha256=2c4d232af4e2ed42d74ae96e282bc8a6c1ae38180cf5bc146bbb8834cb1804f7; anchor=Reboot revokes the actual running browser stream]`.
- Scoped advantage: Real two-chunk packet and embedded module byte equality plus partial-send no-replay coverage; not hardware acceptance.
- Remaining: Adapt donor status/history/error scenarios when those contracts are implemented; actual board throughput/reboot/buffering stress remains a separate physical gate.

### PCController: `Tools/Controller/web/src/status-frame-batcher.ts`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `f600267bf09d925eaf9e5cc3d752574a0a3fbc46350782c614440300c08799e4`.

- Target: `assets/web/frame-stream.js:9 [UTF8-LF sha256=3d0ab90616b173570e5618be76aded339754caa92961868a1d9b43a02e3c6d90; anchor=function createFrameStream]`.
- Target: `firmware/esp8266/src/WebUi.h:1118 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function readStreamPolicy]`.
- Retained: Full source read through EOF. Actual injected clock, generation admission, one scheduled callback, cancellation and disposal structure adapted to the mechanically embedded latest-frame Web stream.
- Retained: Stale callbacks check generation before clearing the timer handle, strengthening the donor against disarming a replacement owner. Observed board/boot and exact socket ownership replace build/name-only identity for this bounded slice.
- Omitted/replaced code: `pendingStatus history queue and applyStatusFrameBatch ordered metrics/error reducer`. Disposition: **pending**. Reason: All status arrivals are useful for telemetry/history, but stale LED actuations must not be replayed. This latest-only send scheduler does not implement the donor telemetry reducer; retain it as separate useful work.
- Omitted/replaced code: `sampleFrom, metricSamplesAfterSnapshot, controllerSnapshotIdentity and peripheralAvailability imports`. Disposition: **pending**. Reason: Metric history and authoritative snapshot resets remain useful and need typed peer contracts. I2C supply/current sensor consumers must stay gated on boards without those sensors; stronger boot fencing alone is not complete metric parity.
- Omitted/replaced code: `TypeScript/React clock module ownership`. Disposition: **replaced**. Reason: Standalone embedded Web consumes one generated JavaScript source and timer clock; requestAnimationFrame would add a display refresh cap to LED streaming. No second React toolchain is necessary for this firmware page.
- Test-code evidence: `tests/web_streaming_test.mjs:10 [UTF8-LF sha256=2f6706f00f4a4a44d1da308ca866195624b43a9de13474f802c6687668e7c5c9; anchor=class FakeFrameClock]`.
- Test-code evidence: `tests/web_access_test.mjs:88 [UTF8-LF sha256=2c4d232af4e2ed42d74ae96e282bc8a6c1ae38180cf5bc146bbb8834cb1804f7; anchor=Reboot revokes the actual running browser stream]`.
- Scoped advantage: Explicit 60/default, 0/uncapped policy, bounded complete-frame buffering, no catch-up bursts, partial-send uncertainty and generation-before-handle checks; browser evidence only.
- Remaining: Port generic telemetry history/status reducer and sensor availability contracts with their own authoritative peer state; validate physical throughput, queued WebSocket epoch fencing, multi-client arbitration and all-client streaming parity.

### PCController: `Tools/VirtualBoard/tests/firmware_controls_test.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `4facf265f8593b83f7610ae99ce58050013226cf1aa5a4d6e60bdbd6e5cc399e`.

- Target: `tests/tm1637_bus_test.cpp:15 [UTF8-LF sha256=0bd8312952f6b58447d12126c698e835290add115b91a753a6b6d6efcbc7d031; anchor=void testDisplayBrightnessFade()]`.
- Target: `tests/display_stubs/Arduino.h:17 [UTF8-LF sha256=b6c81c35395db97127f88f449e51f3cef884592bafe85406f381a73a37f20c5b; anchor=namespace display_mock]`.
- Target: `tests/display_stubs/avr/io.h:5 [UTF8-LF sha256=a5dc33f85c2dde21843165ae0e64bd64bec0ed44c3fdc002c805e29b5e310e63; anchor=struct DisplayRegister]`.
- Target: `firmware/avr/SevenSegments.cpp:77 [UTF8-LF sha256=776c190eb16a8aa72ed538a4728d35bbf7e51b4b9daddbbb81e62bfce252e74b; anchor=pinMode(BoardPins::Tm1637Data, INPUT_PULLUP)]`.
- Retained: Actual require helper, testDisplayBrightnessFade and testDisplayAckDetectionAndRecovery adapted to the separate Pakar keepalive service and A4/A5 hardware ownership; decode actual data/address/control bytes rather than inferring frame recovery from cached values.
- Retained: Canonical firmware_controls_test.cpp639lines fully reread through EOF; rechecked unchanged Pakar require/fade/ACK-recovery subset and A4/A5 fixtures. No all-controls parity or physical acceptance is inferred.
- Omitted/replaced code: `Side A/B directional relay and motion-door policy matrix, temperature role swapping, retired MOVE alias and DS18B20 absent-pullup tests`. Disposition: **not-applicable**. Reason: This fitted board has no directional motors/door policy or DS18B20. Do not enable these or copy donor relay pin mapping; future generic adapters remain feature-flagged with their own acceptance.
- Omitted/replaced code: `Full key double-click/fast-repeat/16-bit rollover, transition smoothing, context-sensitive leaf navigation/settings normalization and tone timer/queue cases`. Disposition: **pending**. Reason: Pakar has existing key/tone/front-panel suites, but these complete donor assertions have not all been transplanted or compared behavior-by-behavior. A display-only import is not proof of those generic contracts.
- Omitted/replaced code: `testDisplayNumericBoundaries/testEveryEasedBytePair; calibrated expandByte12 and EMA/deadband assertions`. Disposition: **pending**. Reason: Pakar display-bus fixtures do not contain new donor numeric overflow-to-dashes or exhaustive65536 transition-pair assertions. Generic display/transition tests remain pending. Temperature-ROM EEPROM threshold/CRC tests remain inapplicable while no DS18B20 is fitted; do not transplant its EEPROM layout.
- Test-code evidence: `tools/test-display.ps1:7 [UTF8-LF sha256=e3c3a230cb93757edfaac1e2ea208695a9cfdfb6b4cd613c657c632b8fb25e51; anchor=foreach ($taskVariant]`.
- Test-code evidence: `tools/test-protocol.sh:84 [UTF8-LF sha256=20b221af60c6bbab505ad50491fa18a12a8e7c26f2a35b2b673bbeb10cbe2213; anchor=for display_flags]`.
- Scoped advantage: Exercise both actual AVR register and Arduino fallback branches; assert ninth-slot input/pull-up, preserved other port bits, exact replay, each NACK byte, display-off restoration and 32-bit rollover. Physical/stuck-low line diagnosis remains unverified.
- Remaining: Import/compare the remaining generic donor key/transition/menu/tone assertions against actual hardware-matched consumers. Physically validate display disconnect/reconnect and supply/timing; register/GPIO doubles are not electrical proof.

### PCController: `Tools/VirtualBoard/tests/uart_protocol_test.cpp`

Verdict: **parity-gap**. Revision: `b0111ae0c80dc34149ea1044de7dd5488920301b`. Original-byte SHA-256: `56c0474a5f0a7475340ec22e718b262b61a449820eb958403cd47bc36b1497c7`.

- Target: `tests/serial_link_test.cpp:1 [UTF8-LF sha256=0bd90ca96053cfdd5369b155f848b7823f250cddb951d2d1ac7c2d79e12ec2b0; anchor=Adapt actual PCController Tools/VirtualBoard/tests/uart_protocol_test.cpp]`.
- Retained: Fully read all seven actual donor scenarios. Adapted representative/consecutive-zero/max48 payload, optional opcode dispatch, nested ACK stability, split RX with synchronous TX, oversized noise/yield/delimiter recovery and malformed/CRC rejection to actual shared SerialLink.
- Retained: Added2048-byte ring/64-byte pass, negative occupancy/read, failed-write/no-automatic-replay, explicit S3 Stream setup preservation, ESP32 counters and all three flow-profile readiness assertions.
- Omitted/replaced code: `testBuzzerPushCarriesMCUTimestamp and separate framing/CRC counter assertions`. Disposition: **pending**. Reason: Timestamped raw donor events and separate counter layout are not yet adapted to Pakar clients/contracts. Port those exact assertions with their production projection instead of deleting useful behavior or declaring parity.
- Omitted/replaced code: `testUnknownReservedEnvelopeByteDoesNotBlockSemanticFrames`. Disposition: **replaced**. Reason: Pakar fixed reserved-zero integrity rejection is intentionally retained and covered in existing protocol_test.cpp. Optional opcode semantic-dispatch test is adapted independently; no product generation or alias introduced.
- Omitted/replaced code: `framePayloadScratch macro mutation test fixture`. Disposition: **replaced**. Reason: SerialLink exposes no mutable TX scratch API. The test exercises split RX plus an actual synchronous ACK between halves and nested response stability, preserving the ownership behavior without fabricating a missing macro scratch interface.
- Test-code evidence: `tests/serial_link_test.cpp:44 [UTF8-LF sha256=0bd90ca96053cfdd5369b155f848b7823f250cddb951d2d1ac7c2d79e12ec2b0; anchor=Actual donor split-RX/synchronous-TX isolation]`.
- Scoped advantage: One stable tiny test executable reused for all modes; no donor build or debug cache.
- Remaining: Port remaining useful counter/timestamp tests when their actual production path is implemented.
- Remaining: Loopback C++ tests do not prove S3 driver compile, physical link, overflow stress or zero loss.

### Pealayer: `.github/dependabot.yml`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fa4791635251ad7d5be26ce70d71a8e59aa7b814a4f09f2b5b749d8a49908225`.

- Target: `.github/dependabot.yml:1 [UTF8-LF sha256=136887ba77b56544d0bcd367a31d2daaa7637ecdb8b997d63df6cb552d6f08e8; anchor=version: 2]`.
- Retained: Actual donor weekly grouped github-actions and Cargo updates, with Cargo path adapted to the existing desktop workspace.
- Omitted/replaced code: `npm updates at /web_ui`. Disposition: **not-applicable**. Reason: Pakar has no /web_ui/package.json or Vite runtime. Embedded Web/tests use Node built-ins; Flutter's actual /mobile Pub dependency contract is covered instead.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b; anchor=console.log('Repository governance]`.
- Scoped advantage: Adds grouped Pub updates for the actual Android client and label routing while retaining weekly grouped Actions/Cargo updates.

### Pealayer: `.github/workflows/ci-cd.yml`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `bba9a295ee7b59e318f76e0b6f6a80f0aec631e249f7d8c798f6c92c0bb7fd8c`.

- Target: `.github/workflows/build.yml:2 [UTF8-LF sha256=4e284a5e8b4f01ae8bfd62731b533cc02c3e24bf10c108b49f9120ce07b92bf6; anchor=name: "🚀 Build · Test · Release"]`.
- Target: `tools/build-desktop.ps1:39 [UTF8-LF sha256=4de42c00c2dca7bb1ea1d234dd1529fd5a06b110192fd8cee3e03d6cd451dc20; anchor=--test-threads=1]`.
- Target: `tools/ci-checks.mjs:209 [UTF8-LF sha256=449c07e0b26f93a640444a7ab1729ea1a0e2d6c2ae5c0beae49b1a2961cb1c60; anchor=export function runChecks]`.
- Target: `tools/prepare-release.mjs:16 [UTF8-LF sha256=c5dcb698c397b30ca0ed076437a695838366aa0a82ed3c580df3cf6380d03306; anchor=export function firmwareManifest]`.
- Retained: Actual donor branch/PR/tag/dispatch/merge-group triggers, concurrency policy, pinned actions without checkout credentials, health-before-build jobs, timeouts, Rust cache reuse, locked tests/builds, packaging, curated notes, manifest/checksums/provenance and aggregate results adapted to current Windows/ESP/AVR/Android targets.
- Retained: Release content is gated on all builds, created as an immutable draft prerelease, with two correctly matched board pairs and raw HTTPS update manifests. Untrusted PRs do not execute publishing code with a write token.
- Retained: Actual donor serial native test execution, isolated test configuration and RUST_BACKTRACE=1 restored. Runtime harness flags are separated from compile jobs; immediate panic output remains available even if a later test aborts before its summary.
- Omitted/replaced code: `LIBMPV_WINDOWS_RELEASES_API_URL, MPV_DLL_PATH, Windows GNU libmpv linking, Linux/macOS mpv dependencies and standalone pealayer-downloader`. Disposition: **not-applicable**. Reason: Pakar deliberately excludes the media runtime and standalone media utility. Copying this setup would introduce forbidden mpv and unrelated package dependencies.
- Omitted/replaced code: `macOS Intel/Apple silicon and Linux native platform matrix/packages`. Disposition: **pending**. Reason: Current accepted native target is Windows; firmware and Android use Linux runners. Full donor reread includes the new scripts/run-ci-rust-tests.sh macOS sampling helper and 15-minute native test step. Linux/macOS native matrix and diagnostic sampling remain useful pending work; they require actual Pakar platform coverage, not donor's unrelated acceptance.
- Omitted/replaced code: `UPX checksum-locked packing and packed native package smoke/resource checks`. Disposition: **pending**. Reason: Existing Windows wrappers preserve donor resource/donor/cache gates and CLI runtime tests, but Pakar has no accepted compressed native package probe. Do not call a source or CLI test full packed GUI runtime acceptance.
- Omitted/replaced code: `branch-tag and archive suffix creation, softprops update-existing release`. Disposition: **replaced**. Reason: User requests stable names without random/time suffixes. Version releases keep stable package names; release enumeration must succeed and an existing release is not overwritten.
- Test-code evidence: `tests/build_storage_test.ps1:17 [UTF8-LF sha256=4bc7dfaa3c70014662b6cc63f9c302d5be53b988083acb62dcadcf462ae01d6d; anchor=Actual donor serial test contract]`.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b; anchor=console.log('Repository governance]`.
- Remaining: The historical 8849ab8 menu failure was subsequently fixed; 5d02b07 Build 38141677102 and CodeQL 38141677110 pass. This is not production/deployment proof. Validate the unexecuted draft publisher, useful additional native targets, donor macOS diagnostics and optional compressed package/runtime verification.

### Pealayer: `.github/workflows/codeql.yml`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fc3c298cf966d77f50b575614b4c26f24f1606459f07749a46880cd482f1cb53`.

- Target: `.github/workflows/codeql.yml:2 [UTF8-LF sha256=71540360da38622faaa237eec4f1276d5b3f67853754d8a221149e8c0f2726ab; anchor=name: "🛡️ Security · CodeQL"]`.
- Retained: Actual donor Actions and JavaScript matrix, no-build security-extended queries, pinned checkout/CodeQL actions, no persisted checkout credentials, PR/main/schedule/dispatch/merge-group triggers, cancellation, timeouts and fail-closed aggregate summary.
- Omitted/replaced code: `Pealayer React/Vite names, packages:read permission and branded prose`. Disposition: **replaced**. Reason: Pakar analyzes embedded Web/scripts and Actions, not Pealayer React. Package read permission is unnecessary for these none-build scans; Rust/C++ coverage is explicitly not invented.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b; anchor=console.log('Repository governance]`.

### Pealayer: `assets/themes/palettes.json`

Verdict: **equivalent-file**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501`.

- Target: `assets/themes/palettes.json:2 [UTF8-LF sha256=11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501; anchor="studio"]`.
- Retained: Byte-identical Studio/Native light/dark JSON catalog, all fourteen semantic roles.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/ui/palette.rs:71 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn imported_palettes_have_every_required_role()]`.
- Test-code evidence: `tests/web_appearance_test.mjs:55 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=embedded catalog exactly matches]`.
- Remaining: Embedded Web now consumes all four actual palettes and fourteen roles. Mobile catalog use and native/Web shared preference synchronization remain pending; byte-identical asset retention is not all-surface parity.

### Pealayer: `build.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `68fb1bdd9e7c7523ab303e37001d3a5252dcb03abe85e489432fe913be7b2e0f`.

- Target: `desktop/build.rs:11 [UTF8-LF sha256=02a9422df8169a9f9795df83c0a990d07a2f74bf2dff314828a3c402f816f3bd; anchor=brand_image::native_icon_bytes(brand_image::icon_bytes())]`.
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
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b; anchor=console.log('Repository governance]`.

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
- Target: `desktop/src/main.rs:1888 [UTF8-LF sha256=65e92fb29dd0b55895c725be9f78fa8a868366d30fd7aa0b0cf9284fe8bac0a0; anchor=event_log.failure("Windows tray tooltip", &error)]`.
- Retained: Full donor plan read through EOF as source requirements, not trusted commands. Generic existing tray lifecycle/menu/disposal retained; actual windows.rs NIM_MODIFY/TIP sections read and adapted to the owned GUID rather than duplicate registration. Tooltip Shell acceptance is separated from object presence; failed writes retain pending latest state with bounded retry/incident recovery and observed restart epoch invalidation.
- Omitted/replaced code: `Playback percent/error/paused inputs, media thumbnail toolbar Previous/Play/Next/Mute and video-only iconic clipping`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free and owns controller operations, not playback/chapter/video. No synthetic media percentages or nonexistent actions are imported; actual pending-operation activity remains.
- Omitted/replaced code: `Full root taskbar initialization/retry, thumbnail toolbar lifecycle, visible tray/menu/notification clicks and all Windows shell suite acceptance`. Disposition: **pending**. Reason: This fixes only GUID-bound tooltip synchronization on an already owned tray object. Mock clocks, NOTIFYICONDATA assertions and observed restart messages cannot certify complete root shell readiness or user-visible action delivery.
- Omitted/replaced code: `Direct donor NIM_ADD/uID1 and popup window ownership`. Disposition: **replaced**. Reason: The pinned tray library already owns HWND, registered GUID, menus, icon updates, restart recreation and disposal. A second NIM_ADD/global icon could conflict with its identity. Adapt direct NIM_MODIFY to the actual registered GUID; no guessed numeric internal ID or library source patch.
- Test-code evidence: `desktop/src/platform/windows_tray.rs:236 [UTF8-LF sha256=5f847993b6177d97f3b5fa6930f6b1acfd6e6e26dfb4fb51c58730f0d8a4ed2d; anchor=fn failed_tooltip_retries_latest_value_without_false_acceptance_or_flood]`.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:147 [UTF8-LF sha256=0b39032fd9b88d51514b9cb39f0def7377016a6f073247404c1b1ccb67dcf550; anchor=fn tooltip_uses_the_registered_guid_and_bounded_utf16]`.
- Test-code evidence: `tests/windows_tray_test.mjs:18 [UTF8-LF sha256=a478534b0a00bc19b19b0e0eb3260a669615608914a485761db1006315ffc13a; anchor=Actual GUID-bound tooltip]`.
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
- Target: `desktop/src/ui/shell.rs:591 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=value["tray_shell"]]`.
- Retained: Full donor document read through EOF. Apply its concrete distinction between working IPC/service and actual shell/GUI readiness: expose tray Shell acceptance separately, retain failures and recover only after accepted synchronization, verify open drafts before graceful replacement and keep actual runtime proof separate.
- Omitted/replaced code: `Timeline navigation/follow/playhead/ruler/toolbar-order repairs and host-specific libmpv/Cafe deployment`. Disposition: **not-applicable**. Reason: Explicitly excluded timeline/mpv/media-seat domain and unrelated host identities. Generic no-nested-egui-lock principle, owned preferences and graceful replacement remain relevant.
- Omitted/replaced code: `Full shared shell-action JSONL launch/forward/GUI/helper evidence and Jump List helper-readiness acceptance`. Disposition: **pending**. Reason: Pakar already has typed IPC and native dialogs but does not contain this complete per-action stage receipt or full actual Explorer dispatch matrix. New tooltip acceptance diagnostics do not substitute for it.
- Omitted/replaced code: `Upload progress broken-pipe panic regression and disconnected-writer acceptance`. Disposition: **pending**. Reason: Actual Pakar CLI best-effort output has a separate implementation; this document read does not compare donor CLI writer sources/tests or certify all upload-output paths.
- Test-code evidence: `tests/windows_tray_test.mjs:18 [UTF8-LF sha256=a478534b0a00bc19b19b0e0eb3260a669615608914a485761db1006315ffc13a; anchor=Actual GUID-bound tooltip]`.
- Remaining: Import/test applicable shell-action stage diagnostics and output writer contracts from actual donor implementation, then verify user-visible native actions. Do not mark complete from this focused tooltip recovery.

### Pealayer: `README.md`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `4e78a6c486bfc7d78adc6d34552bddfd67c89997edb8d2bae0b46e70c705dcb6`.

- Target: `README.md:7 [UTF8-LF sha256=14b6d49428f60f3d48b8b56f16e735f70ad791a99612f97a9c9ff13ba3f297d2; anchor=Connected boards. One contextual control workspace.]`.
- Target: `tools/generate-readme-artwork.mjs:1 [UTF8-LF sha256=2162d1ad3760618260eb9d659dfebb6651ad1e5f5b4eb2c37c12cde1ef7ff8d2; anchor=README uses the same bytes]`.
- Retained: Pealayer's actual centered logo/title/tagline, badge row, visual preview, section navigation, architecture, build, docs, contribution and license structure adapted to Pakar.
- Omitted/replaced code: `Media engine, NLE cue/timeline walkthroughs, playback APIs/shortcuts and libmpv installation sections`. Disposition: **not-applicable**. Reason: These describe Pealayer media business/runtime rather than Pakar board control. README links actual controller APIs, wiring and release/acceptance instead; no mpv is linked.
- Omitted/replaced code: `Donor platform and latest-release badges, cinema-grade completion language and media screenshots`. Disposition: **replaced**. Reason: Unsupported OS/release claims and unrelated images would misrepresent this repo. Use the shared application logo and real embedded Web screenshot labeled simulated, explicit PR/source and physical acceptance boundaries.
- Test-code evidence: `tests/repository_governance_test.mjs:77 [UTF8-LF sha256=5bc629302abaeb749345d1eebc7b2ed1144251e65c7a87b7bbbd1d6bbe05125b; anchor=console.log('Repository governance]`.
- Scoped advantage: Canonical cross-interface icon bytes and explicit proof/acceptance boundaries prevent a polished landing page from claiming an unaccepted release.

### Pealayer: `scripts/capture-visible-window-windows.ps1`

Verdict: **scoped-improvement**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `94bc5aed91407818905412d50dde72ea669f3c799ef56a043dd0a81befdce47f`.

- Target: `tools/capture-window-windows.ps1:75 [UTF8-LF sha256=c9f48604a224599ea0252638558e80f4025beb4b53834112af9e48af03ae73d0; anchor=public static extern bool PrintWindow]`.
- Retained: Actual donor PrintWindow P/Invoke, minimized/window-bounds validation, DPI awareness, System.Drawing PNG lifecycle and read-only behavior.
- Omitted/replaced code: `Get-Process -Name foreground filter`. Disposition: **replaced**. Reason: Explicit expected PID and window-owner revalidation remove ambiguity between production and fixtures; screen fallback checks foreground ownership without changing focus.
- Omitted/replaced code: `RenderOccludedWindow branching and unconditional Bitmap.Save`. Disposition: **replaced**. Reason: Capability-gated egui framebuffer is tried first, then PrintWindow/BitBlt and verified foreground pixels. Blank images and overwrites are rejected.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=9280c4441d44e2a070b81d3e22a24188621db226de01e93c125c205be4e9744a; anchor=for(const forbidden]`.
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

- Target: `tools/windows-build-host.ps1:16 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Get-PakarCargoTargetDirectory]`.
- Target: `tools/windows-build-host.ps1:44 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Test-PakarFileContentEqual]`.
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

- Target: `tools/package-probe-windows.ps1:1 [UTF8-LF sha256=f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f; anchor=function Invoke-PackageProbe]`.
- Retained: Byte-identical actual donor process-handle probe: no-shell hidden process, async redirected stdout, 30-second wait, explicit exit code, output return and Dispose. Optional staging probe calls this helper.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/reuse_preflight_test.ps1:87 [UTF8-LF sha256=842c7d0149c2717f7378116063712f4b719941152d89bd43cdc31e60b148437d; anchor=Actual imported short-lived process stdout]`.
- Remaining: Donor timeout throws without terminating its child. Full GUI/runtime package smoke remains separate from CLI identity.

### Pealayer: `scripts/package-windows.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `5651709cfb58c2f1e0d7052b3ca604df154563ce552498a066c6e67b8f9baedf`.

- Target: `tools/windows-build-host.ps1:60 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Assert-PakarPackageSourceStable]`.
- Target: `tools/stage-desktop.ps1:36 [UTF8-LF sha256=bb1995575486751f88833de244be46b1d6c92727514c2acf81e9e516bccd9b2d; anchor=Invoke-PackageProbe]`.
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

- Target: `tools/build-desktop.ps1:26 [UTF8-LF sha256=4de42c00c2dca7bb1ea1d234dd1529fd5a06b110192fd8cee3e03d6cd451dc20; anchor=Get-PakarCargoTargetDirectory]`.
- Retained: Shared resolver call and locked native build flow mapped to Pakar wrapper. Existing canonical cache is reused; explicit Check/Test/Release and storage/reuse gates avoid unnecessary links.
- Omitted/replaced code: `RustBin machine/user/toolchain executable fallback selection and rustc -vV host detection`. Disposition: **pending**. Reason: Pakar currently uses installed Cargo from PATH. Alternate host/toolchain executable fallback and recorded host triple are reusable infrastructure and remain unported.
- Omitted/replaced code: `libmpv host resolution/environment/profile and DLL copy`. Disposition: **not-applicable**. Reason: These operations configure the excluded mpv renderer. Pakar uses its own mpv-free linked dependencies.
- Omitted/replaced code: `BuildOnly versus launch forwarding with ApplicationArguments`. Disposition: **pending**. Reason: Pakar wrapper builds only. Generic built-executable validation and configurable run forwarding should become a Pakar runner; manual launching is not equivalent.
- Omitted/replaced code: `Set-Location to repository root`. Disposition: **replaced**. Reason: Absolute manifest and resolved target paths preserve caller cwd without introducing a second cache.
- Test-code evidence: `tests/build_storage_test.ps1:15 [UTF8-LF sha256=4bc7dfaa3c70014662b6cc63f9c302d5be53b988083acb62dcadcf462ae01d6d; anchor=One canonical target]`.
- Remaining: Import generic Windows runner/toolchain fallback and host-triple provenance without mpv.

### Pealayer: `scripts/storage-hygiene.ps1`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `0d8fdeb3e15fb52b917784d4cb3875c68bc12c6c6ed78cd16f2ff8ee3561d89c`.

- Target: `tools/storage-hygiene.ps1:11 [UTF8-LF sha256=0d1af57a83775e2670a5ac222973970dfc3bfc2c2e1bde1a1753523b444bdb5c; anchor=function Get-DirectorySummary]`.
- Target: `tools/build-mobile.ps1:58 [UTF8-LF sha256=cd2502238f503647a3ff252b0c79ab5d988064416c1d968c34c7cc858ee39781; anchor=$taskCache=Get-DirectorySummary]`.
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
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=9280c4441d44e2a070b81d3e22a24188621db226de01e93c125c205be4e9744a; anchor=for(const forbidden]`.
- Remaining: Complete generic locale/theme/icon/width capture matrix and manifest receipts; verify native interaction scenarios.

### Pealayer: `src/application_shortcuts.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `1f173bef8acc2904eac5b0a91407aa4f7629ce65c88ea3a8ac78f8c13321d203`.

- Target: `desktop/src/ui/shell.rs:623 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=egui::Key::F11]`.
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
- Target: `mobile/lib/main.dart:422 [UTF8-LF sha256=6ea527d0625585fd493101975fb55a6156d9f30a4d53430cdd494b48021cee61; anchor=peer.application?.icon ?? bundledApplicationArtwork]`.
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

- Target: `desktop/src/cli_output.rs:19 [UTF8-LF sha256=a0224e7006967fa7bc9915fe58d1595f4349cfa2e5763d3cc0263a513db898fc; anchor=pub fn parse_cli_output(]`.
- Retained: Actual format_help_message and PrintHelp/PrintVersion flag branches adapted with native short/long help/version assertions; terminate before config, network, GUI ownership and windows. Share device command help with existing TUI instead of duplicate stale strings.
- Omitted/replaced code: `Media target/chapter/playlist/seek/rate/player associations`. Disposition: **not-applicable**. Reason: User explicitly requires an mpv-free controller; media/timeline/cue business must not replace board control.
- Omitted/replaced code: `Embedded build identity, scoped peer/instance routing, unified remote request fallback, automatic verified host deployment/update status, headless Web-only backend and complete shortcut/command parser tests`. Disposition: **pending**. Reason: These generic contracts are useful but not completed by static help output. Current Pakar typed GUI IPC and board command routes do not prove full donor CLI parity.
- Test-code evidence: `desktop/src/cli_output.rs:31 [UTF8-LF sha256=a0224e7006967fa7bc9915fe58d1595f4349cfa2e5763d3cc0263a513db898fc; anchor=fn actual_donor_short_and_long_help_version_flags()]`.
- Scoped advantage: Top-level flags do not consume matching tokens inside device command payloads; board commands retain their own parsing. CLI endpoint/user default to saved GUI preferences rather than a conflicting hostname.
- Remaining: Complete generic CLI parser/validation, command/IPC parity, embedded build identity and verified app-update contracts; media-only switches are explicitly excluded. Validate actual packaged help/version exit without creating windows and preserve Preferences/rollback when replacing the app.

### Pealayer: `src/messaging.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `44ba995614f2acc3230f6adc13c00500e6844db81aa36d5e60e7eeb9e90d6f9f`.

- Target: `desktop/src/messaging.rs:81 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=pub fn publish(]`.
- Retained: Earlier MessageHub remains byte-identical to historical source hash 471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573: request validation, bounded queue, expiry/upsert/dismiss/revision/source identity and both original tests. The current 226-line donor was completely reread; new detailed diagnostics remain pending, not falsely described as current file equivalence.
- Omitted/replaced code: `Current Toast.details optional serialized payload, publish_with_details, public publish_detailed and bounded-diagnostics preserving-last-good-toast regression`. Disposition: **pending**. Reason: Pakar has no detailed-diagnostics payload yet. Import the 8192-character pre-mutation validation and preserving-last-good-toast regression together with disclosure UI, retaining existing distinct-event identity/upsert behavior.
- Test-code evidence: `desktop/src/messaging.rs:171 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=fn upsert_dismiss_and_expire_are_shared()]`.
- Remaining: Import current bounded detailed diagnostics and native disclosure; historical hub equivalence does not establish current Web/TUI/IPC notification-surface parity.

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

### Pealayer: `src/ui/board_info.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ab1c99e674e902e37626963e2dd5cf7d993965334411d5e33dafd1ea2bf79e51`.

- Target: `desktop/src/capabilities.rs:7 [UTF8-LF sha256=cc3e3b3ffbfb3c776639b64326ddd90125285a389688dcc56ec147b3cecf80ed; anchor=pub struct Manifest]`.
- Target: `desktop/src/ui/shell.rs:458 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=fn synchronize_context(]`.
- Target: `desktop/src/ui/device_settings.rs:21 [UTF8-LF sha256=1c4c1ed4aa4fa1f30320fc9d7a3b7ef622e8894217503890881c9e98319a4240; anchor=pub(crate) fn available(]`.
- Target: `docs/DEVICE-CONTEXT.md:71 [UTF8-LF sha256=ad4669cbe23b48b4f6ee22634c21c015cd5b9767b8006d7316ad04d4ee99648b; anchor=## Exact reuse and remaining useful code]`.
- Retained: Reread all 1280 lines through EOF. Prior authoritative overview/capability/front-panel adaptation remains; shared dropdown dependency is now imported, but full typed board settings and ACK consumers remain pending.
- Retained: Numeric display controls are independent of Text; peer title follows acknowledged name and lost capabilities cancel Table work.
- Omitted/replaced code: `TABS/draw persistence; overview typed board/profile/protocol/build/EEPROM identity; optional_row/yes_no`. Disposition: **pending**. Reason: Existing settings/dialog owners retained; complete typed PCController identity/profile/readback and persistence/localization tests require a real adapter. Current manifest is not that universal adapter.
- Omitted/replaced code: `settings/settings_grid/setting_control/checkbox_setting/slider_setting/enum_combo/bit_checkbox/relay_mask_editor`. Disposition: **pending**. Reason: Full EEPROM draft/write/readback, menu/reset/reboot and relay/PWM/RF/LCD/macro editors remain useful pending contracts. Current driver absence hides them but does not certify generic helpers imported.
- Omitted/replaced code: `capability_badges/capability_column_count/capability_pill/state_badge/unavailable and responsive tests`. Disposition: **pending**. Reason: Availability semantics adapted; entire donor badge geometry, localized empty states and responsive/keyboard consumer tests remain unverified.
- Omitted/replaced code: `FRONT_PANEL_KEYS/key_icon fixed K1-K4 previous/next/decrease/increase`. Disposition: **not-applicable**. Reason: Our board has nine shift-register/ADC keys; four hardcoded identities would be false. Matching future adapters can advertise them.
- Omitted/replaced code: `front_panel_key_button and acknowledged menu/key/layout readback`. Disposition: **pending**. Reason: Nine-key state exists, but full remotely navigable front-panel/menu/key commands need per-board descriptors and donor ACK tests. Incompatible labels do not justify dropping reusable command helpers.
- Omitted/replaced code: `seven_segment_preview, section/row drawing`. Disposition: **replaced**. Reason: Existing Pakar seven_segment/dialog primitives own preview and geometry. A duplicate renderer would fork styling; fresh peer gates wrap the current owners. Native paint acceptance remains pending.
- Omitted/replaced code: `DropdownUiExt / dropdown::ComboBox board enum selector integration`. Disposition: **pending**. Reason: The tracked shared dropdown module is now fully read and imported, with native consumers and donor tests. This does not implement every generic consumer contract in this file: render-rate policy, workspace/language/direction actions and complete board EEPROM enum ownership remain independently pending. Native execution and visual acceptance are reported separately.
- Test-code evidence: `tests/web_context_test.mjs:62 [UTF8-LF sha256=6ffe93034ab3cd0e92608fb8b4137621373c84e958c599904ddb4fea0b125080; anchor=Native wiring is source-checked only]`.
- Test-code evidence: `desktop/src/capabilities.rs:186 [UTF8-LF sha256=cc3e3b3ffbfb3c776639b64326ddd90125285a389688dcc56ec147b3cecf80ed; anchor=fn no_profile_or_individual_ack_means_no_controls]`.
- Remaining: Typed PCController adapter, donor tests and native runtime proof remain #10/#3/#6. No whole-file equivalence or application superiority claimed.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.

### Pealayer: `src/ui/color_picker.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `fe2bf96c968d83874903f5d06085775f5787cf24467fb0c7be3fcf53077c1aa4`.

- Target: `desktop/src/ui/color_picker.rs:25 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=pub fn remember_color(]`.
- Target: `desktop/src/ui/color_picker.rs:240 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=pub fn accent_picker(]`.
- Target: `desktop/src/ui/preferences.rs:126 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=super::color_picker::accent_picker]`.
- Target: `desktop/src/ui/i18n.rs:113 [UTF8-LF sha256=8d839842bc94f3e8baabb054f4b96f1a83be236b4f758f37a1b306a1d39bd10b; anchor="Recent colors"]`.
- Retained: Reread all 550 current donor lines through EOF. Actual session-only12-color MRU deduplication, recent/shared swatches, integrated HEX-field swatch, full-width Custom choice, independently anchored palette/validated temporary HEX draft and prior donor tests remain adapted. The newer integrated Custom expansion/density behavior is pending; current production accent consumer is not a complete Preferences contract.
- Omitted/replaced code: `palette_contents baseline/draft ownership and integrated color_dropdown one-popup Custom expansion, dynamic screen-height clamp/growth, explicit arrow-key opening, control_density geometry/chevron`. Disposition: **pending**. Reason: Fully reread all 550 current donor lines through EOF. Pakar still uses its previously imported independently anchored Custom palette/validated HEX draft and fixed row geometry. The newer single-popup expansion and dynamic density mechanics require the real shared preference/density dependency and adapted focus/geometry tests before replacing those working owners.
- Omitted/replaced code: `Pealayer asset path, session ID and product-specific shared-colors heading`. Disposition: **replaced**. Reason: Uses the actual already imported shared palette asset, Pakar context ID and localized generic headings without disk persistence for recent colors.
- Omitted/replaced code: `Full System accent mode and shared native/Web/mobile Preferences contract`. Disposition: **pending**. Reason: This source module is imported, but the current persisted RGB-string consumer is not Pealayer dynamic OS-accent mode or cross-surface preferences ownership. The selected Preferences Accent branch was inspected; that entire dependency file is not certified by this review.
- Test-code evidence: `desktop/src/ui/color_picker.rs:373 [UTF8-LF sha256=e07f4284ff64968179df0888451169c5e915b5b231624755ab50750e7fccd8b8; anchor=fn accent_custom_row_opens_separate_palette_without_mutating_on_open()]`.
- Test-code evidence: `tests/native_dropdown_test.mjs:1 [UTF8-LF sha256=fb892411e1339a82d7de1be2df090f86ea46b71750fd15a482bf796afad468da; anchor=Wiring/provenance checks]`.
- Remaining: Import the full applicable shared Preferences contract and System accent ownership across surfaces; execute native behavior tests and capture the deployed palette. Source guards are not runtime or visual evidence.

### Pealayer: `src/ui/dialog.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `8a396d67862578f8727e43a952e82120b20c9c582ebcc5366ab0968ba8f704d8`.

- Target: `desktop/src/ui/cell_table.rs:363 [UTF8-LF sha256=e1e1bb468a30f3d1974fe45d1ff9ddf0d4d9cf1d83b96e13b270477305cc7e4f; anchor=fn manager_selector(]`.
- Target: `firmware/esp8266/src/WebUi.h:179 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=/* Fixed manager slots:]`.
- Retained: Read all 952 donor lines through EOF. navigation_control's fixed allocation, clipped centered text and inside-stroke painting are adapted to manager_selector. Compact cell/row/state slots use the existing shared neutral animated cell_surface; state-dependent Button Frames cannot resize their text/content geometry. Selected widget metadata is retained explicitly.
- Retained: Current complete 1236-line donor and complete mapped Pakar dialog.rs were reread by the status/front-panel agent; historical bounded geometry, inside-stroke navigation, equal action sizing, RTL adjustments, numeric modifier/clipboard/wheel and scrolling mappings remain intact. This is source comparison, not native test execution.
- Omitted/replaced code: `navigation_control's 80px minimum, left-aligned icon/title/metadata slots and selected bg_fill tint`. Disposition: **replaced**. Reason: A compact cell needs a centered number in 32x28; row/action slots are 96/104x28. The user requires neutral shared selection, not a tinted navigation side rail. Fixed text positions and inside strokes are preserved; existing side-rail helper remains unchanged.
- Omitted/replaced code: `singleline_text_edit/workspace_overlay_layer/preferred_host/centered_default_rect/bounded_geometry/opaque frame/action buttons/action rows/navigation detail/section/compact_row/setting_row/scroll_column/fit_scroll and their donor tests`. Disposition: **pending**. Reason: Existing imported dialog helpers are present, but this scoped geometry fix does not certify each call site or host/resize/action/localization test. Useful generic code is not excluded; full consumer and test reconciliation remains issue #3 work.
- Omitted/replaced code: `numeric_stepper/numeric_adjustment/adjust_numeric/numeric_slider_wheel/parse_numeric_paste/receive_numeric_paste/numeric_context_menu and seven pointer/paste/wheel tests`. Disposition: **pending**. Reason: Helpers already exist in the imported module; bounded input geometry, paste ownership, wheel tail suppression and context behavior need their actual donor runtime tests restored and controller settings coverage. Hover selector acceptance is not numeric-control parity.
- Omitted/replaced code: `Current stable_toggle, fixed-slot close_button, confirmation_modal AlertDialog focus restoration/top-modal Escape/non-destructive backdrop, inline_caption_edit and density-aware singleline_text_edit`. Disposition: **pending**. Reason: These useful generic helpers are absent from Pakar and must be imported with geometry/accessibility/focus regression tests while preserving Pakar's RTL input alignment. Existing Preferences and device dialogs remain custom implementations, not equivalent implementations of the new helpers.
- Test-code evidence: `desktop/src/ui/cell_table.rs:1171 [UTF8-LF sha256=e1e1bb468a30f3d1974fe45d1ff9ddf0d4d9cf1d83b96e13b270477305cc7e4f; anchor=fn manager_hit_slots_text_and_wrap_never_follow_hover_or_stroke_width()]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:11 [UTF8-LF sha256=38dae57d1e982f90edb831eb1381fb6323d7199c908f843b3295631d53444dbd; anchor=Manager must not allocate state-dependent selectable Frames]`.
- Remaining: Execute the added actual native painter test at narrow/wide widths with animation on/off, then capture/deploy through guarded workflows. Native Test is currently blocked by the canonical >8 GiB cache; Web checks are source/DOM-double evidence, not browser pixels or physical deployment.
- Remaining: Complete the named generic dialog call-site/test reconciliation in issue #3; no whole-file behavioral equivalence or whole-project superiority claim.

### Pealayer: `src/ui/dropdown.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `134f1613df05c9db44f8ccd178981a8eca4fb6c254ced6466229afca66b40ee9`.

- Target: `desktop/src/ui/dropdown.rs:368 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=pub trait DropdownUiExt]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4; anchor=pub fn draw(]`.
- Target: `desktop/src/ui/preferences.rs:124 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=super::dropdown::ComboBox::from_id_salt("theme")]`.
- Target: `desktop/src/ui/device_settings.rs:36 [UTF8-LF sha256=1c4c1ed4aa4fa1f30320fc9d7a3b7ef622e8894217503890881c9e98319a4240; anchor=super::dropdown::ComboBox]`.
- Target: `desktop/src/ui/shell.rs:1171 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=super::dropdown::ComboBox]`.
- Retained: Reread all 646 current donor lines through EOF against the retained target. Actual focus owner, enabled/selected entries, search focus, Arrow/Home/End/Enter navigation, disabled skipping, inside focus stroke, nested menu framing and closed-popup cleanup remain imported. The prior three native tests remain adapted to fixed32px Pakar rows; newer dynamic density/chevron and inline-expansion behavior is explicitly pending.
- Retained: Current exclusive-transport slice reread all646 donor lines again. Unified connection draw uses actual menu_ui/DropdownUiExt enabled/selected/focus registration, one lifecycle action, lazy selected-USB OS enumeration and closed-popup cleanup. Old dual-popup proof anchors were replaced with real new contextual/lifecycle tests, not kept as fictitious surviving tests; runtime execution remains separate.
- Omitted/replaced code: `control_density::height/paint_chevron in trigger and choice geometry, temporary interact_size restoration, choice_with_close reusable expansion rows`. Disposition: **pending**. Reason: Fully reread all 646 current donor lines through EOF. Pakar retains fixed32px rows and the previous explicit keyboard/search-focus owner, but lacks the new dynamic density and inline expansion API. Current scoped advantage remains source-ID/texture handling, not full newer donor parity.
- Omitted/replaced code: `Pealayer active-dropdown ID`. Disposition: **replaced**. Reason: Product-specific context key renamed to Pakar. No generic helper or donor assertion removed.
- Test-code evidence: `desktop/src/ui/dropdown.rs:440 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_arrows_seed_focus_skip_disabled_commit_enter_and_cancel_escape()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:519 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_search_keeps_text_focus_until_arrow_enters_choices()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:585 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn dropdown_color_rows_are_full_width_equal_height_and_hover_stable()]`.
- Test-code evidence: `desktop/src/ui/dropdown.rs:415 [UTF8-LF sha256=f7b1532924e3e3ae87d72d91983e7e49f48fed2efb9b01aede48d1a9734f1fa3; anchor=fn from_label_keeps_locked_egui_trigger_identity()]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:254 [UTF8-LF sha256=573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4; anchor=fn one_contextual_control_lazy_usb_scan_and_busy_guard()]`.
- Test-code evidence: `tests/native_dropdown_test.mjs:1 [UTF8-LF sha256=fb892411e1339a82d7de1be2df090f86ea46b71750fd15a482bf796afad468da; anchor=Wiring/provenance checks]`.
- Scoped advantage: Adds the locked egui from_label trigger identity for existing labeled settings and actual connection-control keyboard ownership with busy-port rejection. Clears texture ownership before native assertions to avoid a second panic masking a failure. Native execution and production visual acceptance must be reported separately from source wiring.
- Remaining: Integrate current donor control_density height/chevron dependency and configurable density through real persisted native/Web/mobile preferences, then port current dynamic-height geometry assertions; preserve existing focus/ID/selection behavior.

### Pealayer: `src/ui/i18n.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `811a48221c47c903c1d70ca904ead90a7a6b7f5ee0ba9a5f26a7e60dd30a75c3`.

- Target: `desktop/src/ui/i18n.rs:6 [UTF8-LF sha256=8d839842bc94f3e8baabb054f4b96f1a83be236b4f758f37a1b306a1d39bd10b; anchor=pub(crate) fn configure_ui_fonts]`.
- Target: `desktop/src/ui/dialog.rs:18 [UTF8-LF sha256=b3786964dd0c375f0f73325a4046b3fc0e3967ace4e7e0c4f66b6f52d66eda47; anchor=pub fn singleline_text_edit]`.
- Target: `desktop/src/ui/preferences.rs:128 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=AppLanguage::Persian]`.
- Target: `docs/PERSIAN-PARITY.md:48 [UTF8-LF sha256=d1f873fb589b406316d77e9ca2fcaf652fe8773b1534063504fc1ab516b506da; anchor=## Exact source-to-target mapping]`.
- Retained: Actual donor platform/UI face loading, Vazirmatn and Phosphor registration, application dictionary, identity-preserving visual_text, script detection, input alignment and horizontal/vertical direction helpers retained and wired into real native controls.
- Retained: Monospace Persian fallback and preference-driven priority refresh added without discarding later widget font registrations. Actual donor contextual glyph test adapted to both families; saved Unicode and peer identifiers never manually reversed.
- Omitted/replaced code: `configure_preferred_fonts/install_ui_fonts UiFontFamily cache, normal/monospace font-family preferences, named pealayer-status-strong and pealayer-status-mono-strong emphasis faces, newer generic hardware/effects/status dictionary additions`. Disposition: **pending**. Reason: Fully reread all 1665 current donor lines through EOF. Pakar retains its existing registry-preserving language font installation and both-family Persian fallback; it does not yet expose these font-family preferences or named status emphasis faces. Generic hardware/action/status translations still need actual corresponding Pakar consumers and localization tests; only media/timeline-specific copy remains excluded.
- Omitted/replaced code: `Media playback, cue/timeline reveal and keyframe-specific dictionary keys`. Disposition: **not-applicable**. Reason: They describe explicitly excluded media business; no corresponding controller surface exists. No generic UI/tooling capability is excluded on that basis.
- Omitted/replaced code: `Manage folders, Manage folder, New folder, Expand folders, Collapse folders, Move to folder, Delete folder, Confirm delete, Channels will be moved to Ungrouped, pending folder change and attached-board folder configuration copy`. Disposition: **pending**. Reason: Generic grouping behavior is useful but not yet integrated with actual board cell/group contracts. No fake folder actions or completion claim; retain on issue #4 for real grouping and localized acceptance.
- Omitted/replaced code: `Separate ttf_parser coverage assertions and exhaustive native surface translations`. Disposition: **pending**. Reason: Avoid introducing a duplicate font parser dependency: actual renderer glyph/contextual and font-retention tests pass in 8849ab8 Windows CI after headless output repair. The run completes 114 passed/1 unrelated menu-label failure; no abort. Additional Unicode, production rendering, device-settings and status/transport copy remain unaccepted.
- Test-code evidence: `desktop/src/ui/i18n.rs:1438 [UTF8-LF sha256=8d839842bc94f3e8baabb054f4b96f1a83be236b4f758f37a1b306a1d39bd10b; anchor=fn contextual_persian_in_labels_and_monospace_inputs]`.
- Test-code evidence: `desktop/src/ui/i18n.rs:1400 [UTF8-LF sha256=8d839842bc94f3e8baabb054f4b96f1a83be236b4f758f37a1b306a1d39bd10b; anchor=fn changing_language_preserves_widget_fonts]`.
- Test-code evidence: `tests/localization_test.mjs:40 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Peer text identical to a dictionary key]`.
- Remaining: Inspect real deployed Persian inputs/menus/dialogs and finish application-copy/generic grouping parity. Actual 8849ab8 font-retention/shaping tests pass, but full native release remains open with one menu-label assertion; local cache budget remains enforced.

### Pealayer: `src/ui/icons.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `cd56a5849a0be621d8472b440c9cbac8b91bcaf603d228cb6148bba1eb554fc9`.

- Target: `desktop/src/ui/icons.rs:573 [UTF8-LF sha256=d9e95867777af0771f64322fef26958558c4c976cd3bdec5377a41754550a8da; anchor=pub fn searchable_workspace_icon_picker(]`.
- Target: `desktop/src/ui/shell.rs:1999 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=icons::searchable_workspace_icon_picker]`.
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
- Target: `desktop/src/ui/shell.rs:650 [UTF8-LF sha256=46697fee16f151e0f1e6b836a9318dc9ca061247cf1c9511adffffa79ab9a4b3; anchor=menu::top_menu_button(ui, crate::ui::i18n::text("File")]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4; anchor=pub fn draw(]`.
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

- Target: `desktop/src/ui/preferences.rs:63 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=fn sync_connection_owner(]`.
- Target: `desktop/src/ui/preferences.rs:82 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=fn connection_summary(]`.
- Retained: Fully reread all2308 actual donor lines through EOF, including draft lifecycle, helper ownership, control rendering and tests. Pakar retains saved/config/previewed baselines, live preview, dirty close confirmation and rail/body/footer layout; this is a narrow adaptation, not complete Preferences equivalence.
- Retained: Adapted actual PreferencesDraft::sync_external_appearance triple-baseline merge to sync_connection_owner. External selected method/enabled/endpoint/user/port update all three baselines while preserving unrelated dirty edits. Preferences now shows only the selected method's summary and delegates clean drafts to the single canonical connection dialog; unrelated Save cannot restore stale connection owner fields.
- Omitted/replaced code: `NativePreferencesController helper process/HWND ownership, independent exit wake/watch, owned chrome and peer-active embedded authority`. Disposition: **pending**. Reason: Pakar uses its existing deferred egui viewport rather than the donor helper process. A matching appearance or native-dialog option does not establish owner-window, helper-exit cancellation or focus/chrome equivalence; import only after actual process/interop dependencies and host lifecycle tests are compared.
- Omitted/replaced code: `External ConfigFileWatcher90ms debounce, shared server config authority and complete dirty external-file conflict merge`. Disposition: **pending**. Reason: The actual triple-baseline algorithm is adapted only to the live connection owner fields. Other persisted/external fields and watcher/readback lifecycle are not covered by this change and must not overwrite unrelated drafts.
- Omitted/replaced code: `preferences_contract typed sections/groups, content-derived responsive rows, inline checkbox and ChoiceSlider/MultiSelect/numeric-step controls, density/font/motion parity`. Disposition: **pending**. Reason: Existing Pakar controls implement only part of the donor editor. The full typed-contract dependency and dynamic layout/control tests were not imported in this slice; no all-controls or visual parity claim.
- Omitted/replaced code: `Config-file open/copy/edit/folder menus, complete reload/import/export ownership, timed footer feedback and full save/preview failure lifecycle matrix`. Disposition: **pending**. Reason: Basic import/export and dirty Save/Discard behavior already exist, but do not implement the donor's complete picker/config authority, transient status and helper-exit matrix. Generic file-picker/control mechanisms remain useful, not excluded media business.
- Omitted/replaced code: `Playback/track/subtitle/history/media-association options and mpv audio refresh`. Disposition: **not-applicable**. Reason: The requested controller is explicitly mpv-free and has no media playback domain. Generic preference lifecycle, navigation, picker and rendering mechanisms remain independently pending rather than excluded with those options.
- Test-code evidence: `desktop/src/ui/preferences.rs:354 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=fn external_connection_owner_updates_all_baselines_without_losing_unrelated_drafts()]`.
- Test-code evidence: `desktop/src/ui/preferences.rs:373 [UTF8-LF sha256=c5356e250994432a76e9bc3533e482d3c9903dd5db73864c1479f5c7d1b81739; anchor=fn connection_summary_is_contextual_and_dirty_preferences_cannot_open_another_editor()]`.
- Scoped advantage: One connection owner instead of conflicting Preferences editors, while preserving unrelated drafts and the donor's triple-baseline merge mechanism.
- Remaining: Compare/import the pending generic helper/controller/config-watcher and typed-control dependencies, then verify native visual/navigation/focus and dirty concurrent-owner behavior in the installed app. Guarded Rust tests are not live USB or human acceptance.

### Pealayer: `src/ui/status_bar.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `e407025a1ee959230634f8350de20d58432313d6dd0f734ada735b8fc9fec4db`.

- Target: `desktop/src/ui/status_bar.rs:66 [UTF8-LF sha256=bca2fdc67bba3a46b37cc6fd165857c42055888edc979d53d694672e9bbd582d; anchor=fn hide_menu(]`.
- Target: `desktop/src/ui/connection_controls.rs:84 [UTF8-LF sha256=573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4; anchor=pub fn draw(]`.
- Target: `docs/CONNECTION-CONTROLS.md:1 [UTF8-LF sha256=002ee046e9c206a4cc241ad2b9033971b4933959d8e14f43958848403879f21d; anchor=# Front-panel transport dropdowns]`.
- Target: `firmware/esp8266/src/WebUi.h:748 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function statusRgbOutput]`.
- Retained: Current complete source read through EOF independently by the status/front-panel and network/UI agents; selected target changes reviewed again during evidence integration. Actual per-item hide context menus, persistent visibility choices, bar-level restore menu and small truncating message labels remain adapted. Transport controls remain usable offline without claiming hardware readiness.
- Retained: Actual status_led_display_color direct emitted RGB semantics, 20x22 Widget::StatusRgb slot, circle radius6, hover metadata/copy-color/hide menu and no-double-dimming test adapted into native status bar. Web uses the same authoritative observed channels, individual availability/output and fresh-peer guards; no locally reconstructed status animation.
- Omitted/replaced code: `current_status_message update-manager priority and OSD lifetime; fullscreen exclusion; workspace label; i18n/RTL layout`. Disposition: **pending**. Reason: Pakar retains a status message and item menu, but does not yet share authoritative update/OSD lifetime, workspace text or localized/RTL/fullscreen policy. Restore actual dependencies and test rather than claim full-file parity.
- Omitted/replaced code: `draw_hardware_status board_ready/coordinator/active_transport distinction and telemetry voltage/current/temperature/warnings`. Disposition: **pending**. Reason: Pakar peer freshness/ACK gates and actual S3 physical RGB mirror now exist, but generic coordinator, sensor and warning inventories still need actual adapters before display. Native dropdown live indicators and local pixel-writer acceptance are not independent physical measurement.
- Omitted/replaced code: `prepared_timeline, media-rate/fps mode, hardware timeline revision/feedback; estop_active label`. Disposition: **not-applicable**. Reason: Prepared media/timeline playback is expressly excluded from this mpv-free controller. No E-stop contract is acknowledged by the current pair; reusable latch safety remains tracked in menu comparison, not fabricated as present.
- Omitted/replaced code: `Pealayer hardcoded connection dot RGB`. Disposition: **replaced**. Reason: Pakar transport controls use its existing shared palette green/current text roles; menu state stays readable in both themes without tinting unrelated hardware controls.
- Omitted/replaced code: `UI render-rate toggle/context policy, latest dropdown-backed FPS menu, new widget ordering/dividers/fonts/value ranges/chart history/update operation ownership`. Disposition: **pending**. Reason: The tracked shared dropdown module is now fully read and imported, with native consumers and donor tests. This does not implement every generic consumer contract in this file: render-rate policy, workspace/language/direction actions and complete board EEPROM enum ownership remain independently pending. Native execution and visual acceptance are reported separately.
- Test-code evidence: `desktop/src/ui/status_bar.rs:202 [UTF8-LF sha256=bca2fdc67bba3a46b37cc6fd165857c42055888edc979d53d694672e9bbd582d; anchor=fn rgb_requires_observed_output_and_does_not_double_dim]`.
- Test-code evidence: `tests/interface_settings_test.mjs:45 [UTF8-LF sha256=7a3717629e56db13e988faf28730a6f4b358e748fbc61709b6aaf43a13f96f0e; anchor=Actual emitted RGB is not brightness-scaled again]`.
- Test-code evidence: `tests/connection_controls_test.mjs:1 [UTF8-LF sha256=574c605348414b7f65ce48f3e92821ab1abdf41f05ccb4be1d6f7b5526d6a38b; anchor=Source wiring guard only.]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:359 [UTF8-LF sha256=573fe0f11df41826f7ff797cec716abf76a2482339dac7f5e9ff592fffbad6d4; anchor=fn lifecycle_is_one_explicit_action_and_disconnect_retains_port()]`.
- Remaining: Run native popup/pointer tests, deploy and capture actual controls; restore useful generic status lifetime/readiness/localization contracts in issues #3/#10.
- Remaining: Shared dropdown dependency now has a complete-read comparison; preserve remaining generic consumer contracts and verify native runtime without claiming whole-file parity.

### Pealayer: `src/ui/toasts.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `30d4ff8c9a30b783f9513a3508787e9ca233881bb3a9bda2e1b9f9cd1bc9c989`.

- Target: `desktop/src/ui/toasts.rs:4 [UTF8-LF sha256=8772d049fab0fba597bd7f4ba4403c7d7bd122f6027442e25f32da11d8f058f6; anchor=pub fn draw(ctx:]`.
- Retained: Actual donor root-only overlay: four newest messages, severity icons and semantic palette roles, bounded width, scrollable text, title, dismiss and deadline-driven repaint.
- Omitted/replaced code: `interop::get_live_config() accessor`. Disposition: **replaced**. Reason: Pakar owns the same live palette through config::live(). The OS-native notification route is separate; adding an explicit in-app enable preference does not remove the donor overlay when enabled.
- Omitted/replaced code: `Current optional Toast.details collapsible disclosure and localized details_label`. Disposition: **pending**. Reason: The complete current donor was reread against Pakar's actual renderer. Pakar still renders title/message only. Import bounded detailed diagnostics together with MessageHub validation and regression tests; earlier overlay behavior is retained but is not current whole-file parity.
- Remaining: This native overlay mapping is not shared Web/TUI message-delivery parity.

### Pealayer: `tests/i18n_surface_audit_test.rs`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `504768bc9cf2fda32a8dab8b0f50d573c74e45001734f45c7ff3120f36333f63`.

- Target: `tests/localization_test.mjs:47 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Adapt Pealayer tests/i18n_surface_audit_test.rs]`.
- Retained: Actual donor known-raw-caption prohibition and required localized-boundary audit pattern adapted to real shell menu captions, not renamed timeline/media files. Added peer text exclusion and reversible placeholder/language tests.
- Omitted/replaced code: `Literal media cue/keyframe/player forbidden-string fixtures`. Disposition: **not-applicable**. Reason: Pakar has no those media surfaces; substituting their filenames would manufacture coverage. Controller-specific boundaries and font assets are tested instead.
- Omitted/replaced code: `Complete native/Web raw-caption audit coverage`. Disposition: **pending**. Reason: The current test protects selected imported boundaries; it is not exhaustive. Remaining native settings, transport/status and dynamic Web copy are explicitly pending, and native Cargo tests are blocked by cache budget.
- Test-code evidence: `tests/localization_test.mjs:50 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=for(const key of]`.
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

- Target: `tests/web_appearance_test.mjs:1 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=First four tests port the actual Pealayer]`.
- Retained: All four actual donor contract tests ported: Neutral helper default, preference merge without unrelated-field loss/resolution leakage, host/offline theme/accent and repeated native event/explicit-theme handling. Additional shipped-code tests cover all palette roles, cancel/apply, contrast, identical-write skip, media events, malformed storage, cross-tab updates and session-only storage failure.
- Omitted/replaced code: `volume:37 unrelated media setting fixture`. Disposition: **replaced**. Reason: The unrelated value is endpoint:http://ledboard.local instead. Its preservation and original-object immutability assertions are retained; no actual test contract is removed.
- Omitted/replaced code: `Direct TypeScript test import`. Disposition: **replaced**. Reason: Tests import the actual adapted JavaScript functions; browser tests additionally execute the shipped embedded source, avoiding a separate mock appearance engine.
- Test-code evidence: `tests/web_appearance_test.mjs:30 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=live host theme events leave System preference unchanged]`.
- Remaining: Test execution proves module and simulated browser contracts, not connected native event transport or physical hardware.

### Pealayer: `web_ui/src/appearance.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `b521c20a1f10e6f818978bcb09948c689b74caaf72df18bdec56f2f566f8db58`.

- Target: `assets/themes/appearance.js:4 [UTF8-LF sha256=62a7ebba994ad44f4e2d647bb97684aac9b563104f3f707b43573289e514ecdd; anchor=export function mergeAppearance]`.
- Target: `tools/generate-web-appearance.mjs:31 [UTF8-LF sha256=7fb8bbd16edbff1798dd77e16ee2abbf32ac34d0adf57e63baa4673abdf9d6a2; anchor=replaceBlock('// BEGIN SHARED APPEARANCE']`.
- Target: `firmware/esp8266/src/WebUi.h:9 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=// BEGIN SHARED APPEARANCE]`.
- Retained: Fully reread current source through EOF. The five previously adapted helpers remain in the target: mergeAppearance, resolvedAccent, resolvedAppearanceTheme, accentForeground and paletteName. Existing theme/palette/accent host resolution and luminance semantics remain; newer animation/density/font preference fields and font CSS helpers are explicitly pending, not claimed imported.
- Retained: Generated into the shipped WebUi.h with the shared palette asset; --check rejects stale embedding. Browser preferences dialog validates theme/palette/accent, protects drafts, handles storage failure and cross-tab/System events without device writes or repetitive identical storage writes.
- Omitted/replaced code: `ui_animation_mode, ui_control_density, ui_font_family, monospace_font_family merge fields; ControlDensity/controlDensityHeight 24/28/32/36 geometry; uiFontCss and monoFontCss`. Disposition: **pending**. Reason: New current donor behavior was fully read during mobile-network build preflight. Existing embedded target does not consume these client preferences and does not bundle PealayerUbuntu/PealayerHack; copying their font names would imply unavailable assets. Add actual shared settings/asset support and cross-client tests before claiming parity.
- Omitted/replaced code: `AppearanceState TypeScript interface and generic T return annotation`. Disposition: **replaced**. Reason: ESP serves standalone JavaScript, not a TypeScript/React build. Browser-facing enum/color validation rejects untrusted stored values. New fields/helpers are separately tracked pending rather than silently dropped.
- Omitted/replaced code: `Connected host AppearanceState broadcast/native-Web shared preference path`. Disposition: **pending**. Reason: Pure functions accept host state and tests preserve donor semantics, but standalone browser controls are browser-local today. No actual native appearance event/readback transport is integrated in this pass; System fallback is labeled browser-local.
- Test-code evidence: `tests/web_appearance_test.mjs:22 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=System theme and accent preserve donor host resolution]`.
- Test-code evidence: `tests/web_appearance_test.mjs:64 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=dialog draft/cancel never changes]`.
- Remaining: Implement native/Web/mobile shared preference transport and live authoritative host events, while keeping device parameters separate from client appearance; verify multi-client broadcast/readback.

### Pealayer: `web_ui/src/components/HeaderBar.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `ad53ee284d969054f69b65e8447a313498b5928eae0aed2564e95f8c71e70d04`.

- Target: `tools/generate-branding.mjs:3 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor=Pakar embeds it for the standalone ESP surface]`.
- Target: `firmware/esp8266/src/WebUi.h:228 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=<!-- BEGIN SHARED BRAND HEADER -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `firmware/esp8266/src/WebUi.h:783 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function renderApplicationIdentity]`.
- Target: `assets/web/device-settings.js:65 [UTF8-LF sha256=0174e9db3639482585cf498595dd38e412ba7d71b05bae8edffea15d2ee650f3; anchor=function settingsCanClose()]`.
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
- Test-code evidence: `tests/interface_settings_test.mjs:32 [UTF8-LF sha256=7a3717629e56db13e988faf28730a6f4b358e748fbc61709b6aaf43a13f96f0e; anchor=Failed saves preserve the draft]`.
- Remaining: Complete generic omissions and missing editors named in INTERFACE-CONTRACT.md; native/mobile/deployed acceptance remains open.

### Pealayer: `web_ui/src/components/PreferencesTab.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `30a63cc1db61a46346c2d4c30ce011dc74081a8a6da2e29853e56d39811a0de1`.

- Target: `mobile/lib/device_settings_page.dart:16 [UTF8-LF sha256=ce24d6aa8c8d05095692920a60d990269c741f7756036761f28a5283ed0d1ab3; anchor=class DeviceSettingsPage]`.
- Target: `mobile/lib/device_settings.dart:197 [UTF8-LF sha256=35309b6a6d18d9fb8a5ef687509edee9c9f3aa25d63ad3026081bc96b17d5f1b; anchor=class DeviceSettingsSession]`.
- Retained: Actual contract-driven sections/selected-section navigation, grouped labeled typed controls, contextual options, retained draft on section changes, descriptions, load/save status and reload adapted to Flutter DeviceSettingsPage using the existing shared Pakar catalog. Tablet uses a searchable navigation rail; phone uses wrapping section chips. Actual Phosphor repertoire supplies verified icon codepoints.
- Retained: Actual valueAtPath/updatePaths structured settings ownership adapted to separate public drafts and secret actions. DeviceSettingsSession uses the fully read canonical Pakar native/Web validation, observe/conflict and positive acknowledgment/exact public readback workflow rather than generic optimistic UI success. In-memory secrets never enter saved client preferences; one POST body per explicit operation, no mutation replay.
- Omitted/replaced code: `updatePaths optimistic autosave to /api/config and playback.preference_set`. Disposition: **replaced**. Reason: These are Pealayer state/RPC contracts, not Pakar persisted board endpoints. Password replacement and network restart require explicit confirmation, positive accepted/updated ACK and exact public readback. No optimistic physical save success or automatic write replay.
- Omitted/replaced code: `Ant Design components and ServerFilePicker transport`. Disposition: **replaced**. Reason: Android Flutter cannot import TSX/Ant Design components directly. Actual section/group/control logic is adapted to Material controls with the shared Studio palette, Vazirmatn and verified Phosphor glyphs. Mobile firmware now uses the fully read ServerFilePicker selection/ownership contract with native Android SAF; exact mapping and remaining generic consumers have their own comparison row.
- Omitted/replaced code: `AccentColorPicker, color swatches, appearance live preview, NumericValueControl steps/defaults, multi_select rich options, replacement_list add/delete, Collapsible groups, ServerFilePicker and refreshAudioOutputs two-stage option refresh`. Disposition: **pending**. Reason: Device Wi-Fi/cloud/identity/access editors are implemented, not the entire application preference framework. Generic appearance, collection editors, defaults/step controls, reusable file picking and dynamic options remain useful and require actual client/device capability contracts and tests before importing their controls.
- Omitted/replaced code: `tr(locale, ...) complete control/description localization`. Disposition: **pending**. Reason: Vazirmatn is loaded and the actual RayanLamp digit map normalizes numeric fields, but the new settings copy is English. Full mobile locale/direction must be audited and rendered before claiming Persian preference parity.
- Omitted/replaced code: `timelineWheelPreferences merge, playback audio-output RPC and media-extension preference consumers`. Disposition: **not-applicable**. Reason: Timeline/cue/media playback business was explicitly excluded from Pakar. Their generic dynamic-option and file-picker mechanisms remain pending above; they are not excluded as media business.
- Omitted/replaced code: `Current ColorDropdown custom/accent popup, discrete choice_slider indexed preview/commit and NumericValueControl persisted per-field adjustment steps`. Disposition: **pending**. Reason: Current 563-line donor was completely reread. These generic typed controls are not in the mobile device settings implementation; shared appearance controls, structured batch changes and keyboard/locale acceptance must be imported with their dependencies instead of claiming the earlier section/group adaptation covers them.
- Test-code evidence: `mobile/test/device_settings_test.dart:129 [UTF8-LF sha256=365cb9c9e9986b9f5ad42bf99fdd910be723b690113ebb126253c5c337b79c63; anchor=Settings write $behavior: one POST]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:248 [UTF8-LF sha256=da7de6a91d19dee9231d9b16e80da25864f4bcc84146d1b4425564f98d6d234b; anchor=Acknowledged $route handoff after old view closes]`.
- Test-code evidence: `mobile/test/capture_test.dart:144 [UTF8-LF sha256=f4042a5e06690a29010ced124f1ec5a540ad060a23fecf480aaee03a89b61cab; anchor=Capture must never issue a device mutation]`.
- Remaining: Mobile firmware/update/backup and Android picker workflows are implemented in the separate ServerFilePicker row; physical acceptance remains. Complete connection/discovery controls, unsaved Android-back/quit protection, client appearance/localization, generic typed preference controls and applicable donor dependencies. Verify physical phone/tablet configuration readback and hostname/credential handoffs; current API lacks transactional compare-and-set and encrypted HTTP transport.

### Pealayer: `web_ui/src/components/ServerFilePicker.tsx`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `df5b59007e0b726c815bed0eafcce87e4f5a6e4a59e8c79a1414f2cc7be89316`.

- Target: `mobile/lib/documents.dart:17 [UTF8-LF sha256=9cd92317a5407799c79882fb1aef15dca128adf6291887a3a12ce6ee1fabc5a3; anchor=class AndroidDocuments]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:39 [UTF8-LF sha256=a2b240972900683f5f2622bbe69f47569c0a0bfabee8f66311116f8ae0c6a61e; anchor=Intent.ACTION_OPEN_DOCUMENT]`.
- Target: `mobile/lib/firmware_page.dart:10 [UTF8-LF sha256=40b64996112f85e7acfb3f5c7db19c473cbfc0a018c72387f1447a4e386be626; anchor=class FirmwarePage]`.
- Target: `mobile/lib/firmware.dart:114 [UTF8-LF sha256=a9e1ecce00ae674541490e64733accdfc882dcec8ff78d1c594d0ff89d5b9c86; anchor=class FirmwareController]`.
- Target: `mobile/lib/api.dart:120 [UTF8-LF sha256=fcdc03cb654eabb48c94f921fb2f64e998b3d18837ff09cb03288d1c67aaf864; anchor=Future<Uint8List> maintenanceBytes]`.
- Retained: Fully read actual typed single-file selection, disabled/unselected/loading state, extension filtering, cancellation, stale-result protection and explicit acceptance. Adapted into Flutter AndroidDocuments + native SAF; firmware imports are validated and separately confirmed before any upload. Parent navigation is locked during maintenance confirmation/picking and writes; selected bytes never automatically program either board.
- Retained: Canonical Pakar desktop maintenance and actual firmware routes were read, not fabricated mobile endpoints. FirmwareController pins remote job/boot/hash/targets and verifies a fresh identity before apply/cancel; single-dispatch bounded HTTP refuses redirects/replay. FirmwarePage gates each ISP/OTA domain on individual live feature ACK, distinguishes stage/apply/completion, and exposes real full flash/EEPROM exports.
- Omitted/replaced code: `React/Ant Design modal/table row handlers, server /api/fs/browse, Directory.path/parent_path and AbortController network browse`. Disposition: **replaced**. Reason: Android SAF owns user-granted document navigation, search, single selection and cancellation. ESP has no arbitrary host filesystem browse contract; inventing that endpoint or accepting Windows paths would be incorrect. The native picker reads bounded bytes on its executor; captured board ownership is checked after picker/HTTP awaits. Only a one-use user-created destination token can be exported, with no broad storage permissions.
- Omitted/replaced code: `initialFile parent path, remembered folder, custom icon table/size labels and full localized picker controls`. Disposition: **pending**. Reason: SAF system navigation is available, but mobile app-owned retained-folder hints and a complete localized review/preview workflow still need actual provider/phone tests. Donor custom server file-browser behavior remains useful for native/Web preference consumers; this Android adaptation is not whole-app picker parity.
- Omitted/replaced code: `Current FileLocationSelect location/drive/network-root selector and disabled-while-loading navigation`. Disposition: **pending**. Reason: Current donor was completely reread. The new FileLocationSelect dependency is not yet fully reviewed/imported for native/Web consumers. Android SAF provides provider-owned navigation but does not establish parity with the donor server's filesystem location contract.
- Test-code evidence: `mobile/test/documents_test.dart:37 [UTF8-LF sha256=2d9de6807621a79e043b1e1240afb639e26ea30dc0e0084f42f232e5e4025c8c; anchor=Changed owner while choosing export]`.
- Test-code evidence: `mobile/test/firmware_test.dart:231 [UTF8-LF sha256=092a0a2a8003779b88300cbfe6d63872ffda8205962d6b1661ec4fe34c2e3ce1; anchor=Lost mutating GET probe]`.
- Test-code evidence: `mobile/test/firmware_page_test.dart:103 [UTF8-LF sha256=18f91ffeed357ee85d49cb75bd22edd5518665a28fcad7d76d1cbb81a5985ea6; anchor=Cancelled file selection]`.
- Test-code evidence: `mobile/test/capture_test.dart:149 [UTF8-LF sha256=f4042a5e06690a29010ced124f1ec5a540ad060a23fecf480aaee03a89b61cab; anchor=Firmware capture must never issue a device mutation]`.
- Remaining: Verify Android provider cancellation/rotation/process death and real import/export; actual board update/readback/power-loss acceptance and production APK signing remain open. Generic donor file-browser consumers and full locale/direction still require review. Shared manual AVR staging has no advertised hash/CAS; pinned remote job is preferred and manual limitation is explicit.

### Pealayer: `web_ui/src/i18n.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `331d2545379e36ea717fcf2876508315437bc5abe49923170f50a4f1bb1b47fc`.

- Target: `assets/web/localization.js:19 [UTF8-LF sha256=9761b88471261b763819a3ad76b332d4aa5243bf2d81a9c37b293e1be2b5ac1b; anchor=function uiTr(text,values)]`.
- Target: `tools/generate-font-assets.mjs:41 [UTF8-LF sha256=729da9321a12ee07e1ed0433f8a401b8c20d2d0281dc32deba1072f46f69540e; anchor=const dictionary={}]`.
- Target: `firmware/esp8266/src/WebUi.h:614 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=// BEGIN SHARED LOCALIZATION]`.
- Retained: Fully reread all 630 current donor lines through EOF. Actual fa-dictionary fallback semantics remain reused for application copy, rather than translating arbitrary runtime data. Applicable native dictionary subset generates into the canonical Web module and actual firmware HTML; the new generic hardware/status/font/control vocabulary is useful pending consumer work, not an automatic dictionary import.
- Retained: Added system/explicit language and Auto/LTR/RTL preferences, owned-template text/placeholder/accessibility snapshots, per-browser low-write persistence, shared font route and immutable peer-name boundary.
- Omitted/replaced code: `Media-library, player, playback, cue/timeline and libmpv-specific Web dictionary/surfaces`. Disposition: **not-applicable**. Reason: Those surfaces belong to excluded media business and would add unrelated controls, not controller capabilities.
- Omitted/replaced code: `Remaining generic Web copy and native/Web/mobile preference synchronization`. Disposition: **pending**. Reason: The actual controller subset is integrated, not the entire donor Web feature set. Dynamic errors/confirmations/status/menu attributes and narrow/mobile runtime acceptance remain; browser locale is not silently written to another client or firmware.
- Test-code evidence: `tests/localization_test.mjs:43 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=No identical storage writes]`.
- Test-code evidence: `tests/localization_test.mjs:41 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=assert.equal(attrs.placeholder]`.
- Remaining: Finish all dynamic application-copy surfaces and controller-safe shared preference contracts; execute live-device and mobile-width/Android locale tests.

### Pealayer: `web_ui/src/preferenceFiles.ts`

Verdict: **equivalent-behavior**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `4580d1396b36f5a907cad6cf2f33a38486c3a7e6948a3bb222974e1b8a2a4af4`.

- Target: `mobile/lib/documents.dart:4 [UTF8-LF sha256=9cd92317a5407799c79882fb1aef15dca128adf6291887a3a12ce6ee1fabc5a3; anchor=bool preferenceFileAllowed]`.
- Retained: Actual case-insensitive dot-extension suffix rule translated directly to Dart preferenceFileAllowed and used by the real Android firmware picker. Additional bounds/name and image-format checks do not treat filename as authenticity.
- Omitted/replaced code: `preferenceFileParent path regex`. Disposition: **replaced**. Reason: SAF content URIs are provider-managed identities, not slash-delimited filesystem paths. Android owns the directory picker; deriving a parent by string slicing would select the wrong resource. Host/server parent navigation remains represented in the ServerFilePicker pending row, not silently excluded.
- Test-code evidence: `mobile/test/documents_test.dart:7 [UTF8-LF sha256=2d9de6807621a79e043b1e1240afb639e26ea30dc0e0084f42f232e5e4025c8c; anchor=Actual donor extension rule is case-insensitive]`.
- Remaining: Physical Android document-provider verification is separate from exact helper equivalence.

### Pealayer: `web_ui/src/webPlatform.ts`

Verdict: **parity-gap**. Revision: `5a6961ce6a477a4726b7baf8a84d12fde1cdf6ad`. Original-byte SHA-256: `d6ee8217c05e96be2e20f9a40ab4c46249d57f58871c661a31c33e2310a92f0e`.

- Target: `tools/generate-branding.mjs:17 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor='FAVICON':]`.
- Target: `firmware/esp8266/src/WebUi.h:140 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=<!-- BEGIN SHARED BRAND FAVICON -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Same resolved artwork binds favicon/touch/header before login; ESP embeds data instead of requesting a protected host filesystem icon.
- Retained: Current 383-line donor was completely reread. Existing favicon/header/touch artwork mapping remains valid; this is not browser-platform parity.
- Omitted/replaced code: `storedBoolean/supportsStandaloneMode/useWebPlatform capabilities/events; install/share/toggleFullscreen/enableNotifications/applyUpdate; settings setters/signalInteraction`. Disposition: **pending**. Reason: Generic browser capabilities/lifecycle/preferences need controller-specific integration and tests; static branding does not certify them.
- Omitted/replaced code: `registerPealayerServiceWorker/update/controllerchange, wake-lock cancellation/visibility and offline badge`. Disposition: **pending**. Reason: PWA manifest/high-resolution artwork/cache ownership and streaming-specific wake-lock state remain open.
- Omitted/replaced code: `mediaTitle/current_video state icon resolution, MediaSession video metadata/play/pause/seek/chapter handlers`. Disposition: **not-applicable**. Reason: Video title/track/session business is excluded; generic browser capabilities are still pending.
- Omitted/replaced code: `Current app_icon_revision-qualified touch/manifest URL refresh, dynamic application-title metadata and explicit media-action callback retirement/position clearing`. Disposition: **pending**. Reason: Generic artwork revision/title refresh must be adapted to board-owned application identity and actual PWA resources. Retained stale media callbacks and position cleanup illustrate useful lifecycle patterns, not permission to expose playback actions on a controller. Generic event ownership must be tested in Pakar's real contexts.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### PU850-ESP: `LocalLib/AsyncOTA.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `3d681d678b58e5e615448acddc52fd623914c8ec4a76f80f5bbb75cd34c3aaec`.

- Target: `firmware/esp32s3/src/OtaStage.h:9 [UTF8-LF sha256=38f573b619915ff3c09062d4198aef6d2cc4b44a25fb72ec187edb33a8f22f9b; anchor=class S3OtaStage]`.
- Target: `firmware/esp8266/src/RemoteFirmware.cpp:290 [UTF8-LF sha256=6430fa249685ddce5f5267b801697f80dfc77b9b3eb49320787d94d695d7198b; anchor=bool RemoteFirmware::validateEspStage()]`.
- Target: `firmware/esp8266/src/MaintenanceRoutes.h:169 [UTF8-LF sha256=0a3543ef149f69346c545b510ebc941490a0c6e68eba463258f91f4bf8defe04; anchor=if(plan.targets==RemotePolicy::Esp && !programmingLatched) return true;]`.
- Target: `firmware/esp8266/src/ArtifactDownload.h:37 [UTF8-LF sha256=140a5db58e6fbdb7aad0797eb69d32f9794067db512b547dfcca2b4f57dec32d; anchor=remoteFirmware.readStagedEsp]`.
- Retained: Actual full AsyncOTA.cpp/header and FirmwareTransfer.cpp/header read through EOF. Retains the donor's authenticated chunked application-image OTA and explicit finalization/reboot workflow in the existing integrated /update handler; this is not an additional programmer firmware.
- Retained: S3-specific remote download stages directly into the inactive OTA partition because the identified image exceeds LittleFS capacity. Bounded1024-byte writes, sequential erase, exact transfer length/SHA, SDK image validation, explicit journaled boot selection, persisted-stage revalidation and actual running-image hash checks replace ESP8266-only FS staging. Quiet ESP-only preparation/finish does not send AVR commands; integrated builds retain paired Prog/backup/version gates.
- Retained: The publisher now accepts a raw hashed S3 application and buffered profile with optional AVR component. Ready S3 stages retain bounded download/Range support and block competing local OTA. Firmware-download response bounds are captured once, removing repeated whole-image verification from the S3 TCP chunk callback.
- Omitted/replaced code: `ESP8266 raw FS linker symbols, filesystem OTA, global updater cancellation and watchdog disable/PU busy-status callbacks`. Disposition: **replaced**. Reason: S3 has named OTA partitions and esp_ota handles; global cancellation could affect another owner. Filesystem OTA would erase runtime network/auth/recovery data and remains disabled. PU-specific UART callbacks do not match the AVR protocol, and the quiet profile must remain silent.
- Omitted/replaced code: `Robust physical LAN OTA, Internet fetch/apply, interrupted-power recovery, boot rollback and concurrency/soak matrix`. Disposition: **pending**. Reason: Host fixtures use SDK/HTTP/flash doubles. Resolved actual HTTP400 rejection from a typed MD5 multipart field being interpreted as a file, without weakening duplicate-file ownership. Corrected Wi-Fi upload returned200; actual new boot reports candidate size/MD5, white480-pixel command buffer and persisted station/AP settings. Complete1850096-byte running-image SHA readback later matched the candidate in19.24seconds, but earlier streams failed and a later boot reports panic reset4. Happy-path delivery is not robust recovery/optical/soak or board Internet apply acceptance. Server HTTPS artifacts alone are not board TLS/download/apply proof; boot selection safety is not automatic rollback.
- Omitted/replaced code: `All-client progress/outcome persistence, bounded DNS/TLS opening and complete staged export concurrency`. Disposition: **pending**. Reason: Existing native IPC acceptance did not expose a durable dispatch outcome; HTTP synchronous connect/DNS still has SDK-bound blocking. Range/lease integration has static/SDK checks but still needs live slow-client contention and credential/owner isolation testing.
- Test-code evidence: `tests/remote_s3_test.cpp:90 [UTF8-LF sha256=7e653b709febe81e26a1b80531074d12c897643028b0ca160919585765703498; anchor=Actual S3 RemoteFirmware]`.
- Test-code evidence: `tests/remote_tools_test.mjs:11 [UTF8-LF sha256=4a38cbf8a24751720b35f940e894a64bcd1689b1402972feeb69fa079603e760; anchor=const solo=manifest]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:24 [UTF8-LF sha256=9fd89470c5dae0b5877a1c823797c4c64fcbb580d5a958bd7ad84939326b44d2; anchor=const flashRead =]`.
- Scoped advantage: Explicit inactive-slot staging with independent apply, instead of an image that cannot fit its filesystem or an upload that chooses boot before separate consent.
- Remaining: Diagnose the observed S3 panic with actual console/backtrace; finish AP recovery, stable external-power/optical and OTA concurrency/abort acceptance beyond the verified full-image happy path. Restore the native app connection. Resolve NTP/clock recovery and the uncertain Internet stage request by actual job/boot inspection before any new mutation; then verify pinned HTTPS stage/export/explicit apply and rejection/cancel/reconnect. Keep AVR and production issue2 acceptance open.

### PU850-ESP: `LocalLib/FirmwareTransfer.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `1dada0f284ba4c81a6185f1aeb34dbd299752a7b48b0ba56336d1dfdc9e2ac3c`.

- Target: `firmware/esp8266/src/FirmwareTransfer.cpp:165 [UTF8-LF sha256=b71f673730547b39e4198bc0bd1366a5853f0f3aea988ba19addbcc1426c9f00; anchor=void RegisterFirmwareTransferRoutes(]`.
- Target: `firmware/esp8266/src/FirmwareRange.h:26 [UTF8-LF sha256=03bf18b46233e539a84563f1eb63330d2eaf8e57b9bb73b6cfd92026bda6bef8; anchor=inline bool parseRange(]`.
- Target: `firmware/esp8266/src/FirmwarePlatform.h:108 [UTF8-LF sha256=944d37c11ad396f61054afd539bb498f4cdbbb6240a04f7323e16c53f6a65dc7; anchor=inline bool readRunning(]`.
- Retained: Complete actual donor read through EOF and current adapted production target reviewed. Imported GET/HEAD firmware download, bounded single byte ranges/open/suffix ranges, 416 responses, ETag/304, Content-MD5/Digest/identity headers and chunked flash reads; extracted parser exercises the production implementation.
- Retained: ESP8266 aligned flash implementation is retained. ESP32-S3 adaptation downloads only the running application partition using bounded esp_partition_read, uses mbedtls Base64 and platform model, and shares the same endpoint/HTTP engine instead of a second flasher firmware.
- Omitted/replaced code: `Optional build-time FW_OTA_BEARER_TOKEN/secureEquals and WWW-Authenticate bearer realm`. Disposition: **replaced**. Reason: Shared Pakar WebAccess callback now owns authorization and defaults to deny without an authorization provider. Credentials/session policy must not diverge between firmware download and other device endpoints. X-Login-Path links the actual login flow without provoking unexplained browser Basic authentication.
- Omitted/replaced code: `FW_ENABLE_FULL_FLASH_DOWNLOAD full raw flash including settings`. Disposition: **replaced**. Reason: Network full-flash export remains disabled because it can expose stored Wi-Fi/cloud/access secrets. Application-partition download and separately authorized backup paths are retained. User-authorized physical full flash backup is separate, not this network route.
- Omitted/replaced code: `ProductConfig PU model/product identity and compile date/time header`. Disposition: **replaced**. Reason: Uses Pakar product/model and consistent BuildInfo version without dates/timestamp naming; no PU850 business identity is retained.
- Test-code evidence: `tests/firmware_range_test.cpp:7 [UTF8-LF sha256=96a79254872cd829c6e23a240225063503d0286e9fe1a477dd48aac5e05a6ed5; anchor=assert(parseRange("bytes=0-9"]`.
- Test-code evidence: `tests/s3_platform_wiring_test.mjs:23 [UTF8-LF sha256=9fd89470c5dae0b5877a1c823797c4c64fcbb580d5a958bd7ad84939326b44d2; anchor=assert.match(transfer, /FirmwarePlatform::readRunning]`.
- Remaining: Real HTTP GET/HEAD/Range/auth/ETag checks against the integrated S3 and ESP8266 firmware remain separate from source/host-parser proof; verify exported application bytes against the running partition and protect credentials. Physical OTA/AVR flashing/power-loss/recovery acceptance remains open.

### PU850-ESP: `LocalLib/FirmwareTransfer.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `23121e9cb09ad44e41d0f4d9a4f3dec68ef09c8b79010b57cd0d2e4d14df3929`.

- Target: `firmware/esp8266/src/FirmwareTransfer.h:6 [UTF8-LF sha256=e0e828cc9cde406ed9009c46a2e803b965f36eac58e160c5aaa233deed390496; anchor=void RegisterFirmwareTransferRoutes(AsyncWebServer* server,]`.
- Retained: Complete donor header read through EOF. Public authorization check and route registration remain integrated with the shared asynchronous HTTP server; no separate ISP-only image or parallel updater service.
- Omitted/replaced code: `Single-argument RegisterFirmwareTransferRoutes declaration and optional token-only comments`. Disposition: **replaced**. Reason: The explicit std::function authorization provider adapts the same route to Pakar session/access ownership, defaults to deny without a provider and avoids duplicated credentials. Real device authorization acceptance is still required.
- Test-code evidence: `firmware/esp8266/src/FirmwareTransfer.cpp:149 [UTF8-LF sha256=b71f673730547b39e4198bc0bd1366a5853f0f3aea988ba19addbcc1426c9f00; anchor=return transferAuthorization && transferAuthorization(request);]`.
- Remaining: Real HTTP GET/HEAD/Range/auth/ETag checks against the integrated S3 and ESP8266 firmware remain separate from source/host-parser proof; verify exported application bytes against the running partition and protect credentials. Physical OTA/AVR flashing/power-loss/recovery acceptance remains open.

### PU850-ESP: `LocalLib/MNDP.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `33a7389ec2e1b5c3d75bf88f06d8397f2bf4ba008298ecd4610ddfaccc6e4eb3`.

- Target: `firmware/esp8266/src/Mndp.h:9 [UTF8-LF sha256=5d579c4dcc95c4fb298212798d03d127ce6f131819ac2b348036367c0ed5428a; anchor=class Packet]`.
- Target: `firmware/esp8266/src/MndpService.h:16 [UTF8-LF sha256=12e73866ec7c6b554c57532c1482ce369b072ede745ae2402f66f970027770ae; anchor=class MndpService]`.
- Target: `mobile/lib/discovery.dart:133 [UTF8-LF sha256=c02419074d5e3c47d2a967f3ed818e3a6196ab50c94d412d6dbae18f54050914; anchor=DiscoveredDevice? parseMndp]`.
- Retained: Fully read header/implementation. Actual port 5678/group 224.0.0.88 and big-endian TLV type/length layout adapted into bounded packets; MAC, hostname, firmware version, platform, board, uptime, stable board ID and IPv4 metadata retained.
- Retained: Integrated station lifecycle starts/stops MNDP alongside existing discovery, announces at 30 seconds and accepts only a rate-limited four-zero-byte query. Android scans/decodes defensive bounded TLVs without sending credentials. Packet overflow prevents any partial advertisement.
- Omitted/replaced code: `Unbounded addTlv into 255-byte buffer; global UDP/ASA0002E header inclusion`. Disposition: **replaced**. Reason: Each copy checks the complete packet capacity and fails closed; module-owned socket state avoids donor globals/application header dependency. The header uses reserved/version bytes plus a bounded big-endian sequence, not a host-endian counter.
- Omitted/replaced code: `E_SerialNumber/firmwareVersion fallback and interface WiFi.SSID`. Disposition: **replaced**. Reason: Use the actual stable Pakar board ID, descriptive build identity and station interface instead of unknown PU serial/business fields or disclosing private SSIDs. This does not synthesize an AVR serial or expose Wi-Fi credentials.
- Omitted/replaced code: `IPv6 and unpack TLV definitions`. Disposition: **pending**. Reason: Donor send path does not implement these TLVs. Actual IPv6 readiness/metadata and compatibility need an explicit adapter and tests; do not emit invented addresses.
- Test-code evidence: `tests/mndp_test.cpp:11 [UTF8-LF sha256=871599d4f8ba3c23c123c769c5e85be75b3ce5d5c6ac4890ca4ed52eb8b8dbfa; anchor=never emit a partial advertisement]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:183 [UTF8-LF sha256=e233594eaa2763444ec62cd59dd06245f08caac290938022e3475ddad9de1acc; anchor=Actual loopback bridge requires bearer]`.
- Remaining: Live router/Winbox response/announcement compatibility, interface/UUID continuity across rename/reconnect and integrated S3 adapter; compiled/fixture packets are not physical discovery acceptance.

### PU850-ESP: `LocalLib/MNDP.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `d61e3e316c4a21f5c8f8fd7f6879008e68a1af4adc32b3459a9857b26d0bb03f`.

- Target: `firmware/esp8266/src/Mndp.h:9 [UTF8-LF sha256=5d579c4dcc95c4fb298212798d03d127ce6f131819ac2b348036367c0ed5428a; anchor=class Packet]`.
- Target: `firmware/esp8266/src/MndpService.h:16 [UTF8-LF sha256=12e73866ec7c6b554c57532c1482ce369b072ede745ae2402f66f970027770ae; anchor=class MndpService]`.
- Target: `mobile/lib/discovery.dart:133 [UTF8-LF sha256=c02419074d5e3c47d2a967f3ed818e3a6196ab50c94d412d6dbae18f54050914; anchor=DiscoveredDevice? parseMndp]`.
- Retained: Fully read header/implementation. Actual port 5678/group 224.0.0.88 and big-endian TLV type/length layout adapted into bounded packets; MAC, hostname, firmware version, platform, board, uptime, stable board ID and IPv4 metadata retained.
- Retained: Integrated station lifecycle starts/stops MNDP alongside existing discovery, announces at 30 seconds and accepts only a rate-limited four-zero-byte query. Android scans/decodes defensive bounded TLVs without sending credentials. Packet overflow prevents any partial advertisement.
- Omitted/replaced code: `Unbounded addTlv into 255-byte buffer; global UDP/ASA0002E header inclusion`. Disposition: **replaced**. Reason: Each copy checks the complete packet capacity and fails closed; module-owned socket state avoids donor globals/application header dependency. The header uses reserved/version bytes plus a bounded big-endian sequence, not a host-endian counter.
- Omitted/replaced code: `E_SerialNumber/firmwareVersion fallback and interface WiFi.SSID`. Disposition: **replaced**. Reason: Use the actual stable Pakar board ID, descriptive build identity and station interface instead of unknown PU serial/business fields or disclosing private SSIDs. This does not synthesize an AVR serial or expose Wi-Fi credentials.
- Omitted/replaced code: `IPv6 and unpack TLV definitions`. Disposition: **pending**. Reason: Donor send path does not implement these TLVs. Actual IPv6 readiness/metadata and compatibility need an explicit adapter and tests; do not emit invented addresses.
- Test-code evidence: `tests/mndp_test.cpp:11 [UTF8-LF sha256=871599d4f8ba3c23c123c769c5e85be75b3ce5d5c6ac4890ca4ed52eb8b8dbfa; anchor=never emit a partial advertisement]`.
- Test-code evidence: `mobile/test/bridge_discovery_test.dart:183 [UTF8-LF sha256=e233594eaa2763444ec62cd59dd06245f08caac290938022e3475ddad9de1acc; anchor=Actual loopback bridge requires bearer]`.
- Remaining: Live router/Winbox response/announcement compatibility, interface/UUID continuity across rename/reconnect and integrated S3 adapter; compiled/fixture packets are not physical discovery acceptance.

### PU850-ESP: `LocalLib/Shell.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0113acbff973285f5af186a4857d94cc2df3c99813e0db4ba876f861e7c1083b`.

- Target: `firmware/esp8266/src/TelnetShell.h:63 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class LineEditor]`.
- Target: `firmware/esp8266/esp8266.ino:2725 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=void executeTelnetCommand(String line)]`.
- Retained: Actual insertion/shift/backspace/delete, CSI/SS3 arrows/home/end, Ctrl-C/Ctrl-L and split CR-LF/CR-NUL handling retained.
- Retained: Password input is not echoed and is scrubbed; overflow discards the entire line instead of executing a truncated command; editor state is independent.
- Omitted/replaced code: `ShellTransport_Serial, SerialShellSession and serial-shell service`. Disposition: **pending**. Reason: USB currently owns COBS-framed PC commands. Mixing raw shell bytes would corrupt that transport; provide an explicit framed shell channel or negotiated mode before claiming equivalent serial-shell support.
- Omitted/replaced code: `DeviceParameters get/set registry, readonly behavior and commandMap callbacks`. Disposition: **pending**. Reason: Pakar's fixed game/identity/display commands are not equivalent to the useful generic parameter registry. Audit/adapt the generic registry and supply Pakar parameter descriptors; only actual PU application callbacks may be excluded after individual review.
- Omitted/replaced code: `ShellBroadcastLine, event-line prompt erasure/redraw and active-session output routing`. Disposition: **pending**. Reason: Pakar redraws during editing but does not implement the donor's asynchronous event-broadcast/session routing contract; a WebSocket event feed is not a Telnet prompt-safe replacement.
- Omitted/replaced code: `clear/cls, echo, hex, upper/noupper and reusable formatting helpers`. Disposition: **pending**. Reason: The current command set omits these donor shell utilities. No concrete product or hardware reason justifies blanket deletion; implement applicable utilities or provide reviewed alternatives.
- Omitted/replaced code: `NumberFromString unchecked multiply/add and permissive embedded minus/comma parsing`. Disposition: **pending**. Reason: Do not copy signed-overflow-prone numeric parsing unchanged. The current String::toInt call sites still require a bounded strict parser plus overflow tests; avoiding this donor function alone is not a proven superior replacement.
- Omitted/replaced code: `Mutable global command pointer and include Commands.cpp adapter`. Disposition: **replaced**. Reason: Instance-owned line/editor state and explicit command dispatch avoid the legacy global pointer/include ownership pattern. Useful command behavior itself remains pending rather than excluded with the adapter.
- Test-code evidence: `tests/telnet_shell_test.cpp:45 [UTF8-LF sha256=f7dd91333d3aced80fdab41b146b98ecd2e9dd3896a504f8a70cfb33912f00e9; anchor=LineEditor::TooLong]`.
- Remaining: Port generic registry and useful utilities; implement prompt-safe asynchronous events; keep serial framing explicit.
- Remaining: Harden command JSON construction/numeric arguments and report queued versus AVR-acknowledged outcomes; current immediate OK is not proof of AVR execution.

### PU850-ESP: `LocalLib/Shell.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `9f8e76509ad917d8e204a82d9495e376607a159547d4eee3de6760e6bd5d7da6`.

- Target: `firmware/esp8266/src/TelnetShell.h:63 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class LineEditor]`.
- Target: `firmware/esp8266/esp8266.ino:2725 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=void executeTelnetCommand(String line)]`.
- Retained: Actual insertion/shift/backspace/delete, CSI/SS3 arrows/home/end, Ctrl-C/Ctrl-L and split CR-LF/CR-NUL handling retained.
- Retained: Password input is not echoed and is scrubbed; overflow discards the entire line instead of executing a truncated command; editor state is independent.
- Omitted/replaced code: `ShellTransport_Serial, SerialShellSession and serial-shell service`. Disposition: **pending**. Reason: USB currently owns COBS-framed PC commands. Mixing raw shell bytes would corrupt that transport; provide an explicit framed shell channel or negotiated mode before claiming equivalent serial-shell support.
- Omitted/replaced code: `DeviceParameters get/set registry, readonly behavior and commandMap callbacks`. Disposition: **pending**. Reason: Pakar's fixed game/identity/display commands are not equivalent to the useful generic parameter registry. Audit/adapt the generic registry and supply Pakar parameter descriptors; only actual PU application callbacks may be excluded after individual review.
- Omitted/replaced code: `ShellBroadcastLine, event-line prompt erasure/redraw and active-session output routing`. Disposition: **pending**. Reason: Pakar redraws during editing but does not implement the donor's asynchronous event-broadcast/session routing contract; a WebSocket event feed is not a Telnet prompt-safe replacement.
- Omitted/replaced code: `clear/cls, echo, hex, upper/noupper and reusable formatting helpers`. Disposition: **pending**. Reason: The current command set omits these donor shell utilities. No concrete product or hardware reason justifies blanket deletion; implement applicable utilities or provide reviewed alternatives.
- Omitted/replaced code: `NumberFromString unchecked multiply/add and permissive embedded minus/comma parsing`. Disposition: **pending**. Reason: Do not copy signed-overflow-prone numeric parsing unchanged. The current String::toInt call sites still require a bounded strict parser plus overflow tests; avoiding this donor function alone is not a proven superior replacement.
- Omitted/replaced code: `Mutable global command pointer and include Commands.cpp adapter`. Disposition: **replaced**. Reason: Instance-owned line/editor state and explicit command dispatch avoid the legacy global pointer/include ownership pattern. Useful command behavior itself remains pending rather than excluded with the adapter.
- Test-code evidence: `tests/telnet_shell_test.cpp:45 [UTF8-LF sha256=f7dd91333d3aced80fdab41b146b98ecd2e9dd3896a504f8a70cfb33912f00e9; anchor=LineEditor::TooLong]`.
- Remaining: Port generic registry and useful utilities; implement prompt-safe asynchronous events; keep serial framing explicit.
- Remaining: Harden command JSON construction/numeric arguments and report queued versus AVR-acknowledged outcomes; current immediate OK is not proof of AVR execution.

### PU850-ESP: `LocalLib/SSDP.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `2f9c9f8f0b613d958e8bb31920174a7d8fa9f523b3477b88d9430a71d385bf48`.

- Target: `firmware/esp32s3/src/Discovery.h:8 [UTF8-LF sha256=ceb14ad85094384dbbe3225ca467f52dc43e187b44dcc78a1a7892a03a95dec7; anchor=class Discovery {]`.
- Target: `firmware/esp32s3/src/Discovery.h:49 [UTF8-LF sha256=ceb14ad85094384dbbe3225ca467f52dc43e187b44dcc78a1a7892a03a95dec7; anchor=String xmlDescription() const {]`.
- Retained: Complete donor wrapper was reread by the evidence owner, in addition to network agent's full read. Actual initSSDP metadata/schema/periodic-service ownership maps to Discovery::begin/service/xmlDescription and the imported SsdpDevice::schema XML escaper. Hostname/display name/application/version are Pakar-owned; no PU850 EEPROM business identity is retained.
- Omitted/replaced code: `ASA0002E/E_MainVersion/E_SerialNumber globals, include-SSDPDevice.cpp pattern, server-global registration and duplicate hand-built description`. Disposition: **replaced**. Reason: Canonical main registers the description route; instance-owned Discovery uses consistent Pakar metadata and the imported escaped schema. No duplicate compiled .cpp/global service or PU serial/version state is retained.
- Omitted/replaced code: `Optional UPnP serviceList/SOAP/GENA implementation`. Disposition: **not-applicable**. Reason: The donor intentionally does not advertise an unimplemented service. Pakar likewise leaves optional service discovery disabled; no invented control protocol is exposed. Add a real contract before enabling it.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/SSDP.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `f9f7b078b6085e731f0ed42c5dd983d1ab0a3a48c2912797406fb3e0b0df3ea5`.

- Target: `firmware/esp32s3/src/Discovery.h:62 [UTF8-LF sha256=ceb14ad85094384dbbe3225ca467f52dc43e187b44dcc78a1a7892a03a95dec7; anchor=SsdpDevice ssdp_;]`.
- Retained: Complete tiny donor header was reread by the evidence owner. Its current role is only the historical SSDP-to-singleton alias; actual device declarations map separately to the imported SsdpDevice.h.
- Omitted/replaced code: `#define SSDP SSDPDevice singleton shorthand`. Disposition: **replaced**. Reason: Explicit Discovery-owned instance avoids macro alias collisions and makes restart/teardown ownership clear; no useful implementation is silently dropped with this alias.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/SSDPDevice.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `82f8c2a3c9779392202dc9b864328040cf47b9303674f142e5348df2b17e34b4`.

- Target: `firmware/esp32s3/src/SsdpDevice.cpp:260 [UTF8-LF sha256=2ebc872f04fec0e5c9bdc6965c7c67df18425d08283b7fab95a768d59f81f819; anchor=void SsdpDevice::post(]`.
- Target: `firmware/esp32s3/src/SsdpDevice.cpp:584 [UTF8-LF sha256=2ebc872f04fec0e5c9bdc6965c7c67df18425d08283b7fab95a768d59f81f819; anchor=void SsdpDevice::schema(Print &client) const]`.
- Target: `firmware/esp32s3/src/SsdpUdp.h:70 [UTF8-LF sha256=e25782c43552d9a2fb570b2dd18446c99907a9b58d0d1ec6f5f190b9fee84f98; anchor=bool beginPacketMulticast(]`.
- Retained: Network agent fully read the actual complete PU850 donor and copied/adapted its source, not merely an SDK call. The evidence owner reviewed the complete donor-to-target diff. Retains bounded/de-duplicated delayed M-SEARCH queue, root/UUID/device responses, alive/update/byebye, boot/config identity, topology change handling and XML escaping/schema.
- Retained: Only platform boundary and ownership are changed: PakarS3 namespace/instance-owned class, ESP32 chip identity and interface-bound SsdpUdp replace global ESP8266 WiFiUDP/IGMP. Both active AP and station memberships are supported rather than carrying ESP8266's small-IGMP-pool one-interface restriction.
- Omitted/replaced code: `Global SSDPDeviceClass singleton, ESP.getChipId and manual igmp_leavegroup/ESP8266 one-interface IGMP-pool policy`. Disposition: **replaced**. Reason: Instance-owned S3 Discovery explicitly owns teardown and stable identity; socket-scoped membership/IF/TTL replaces incompatible ESP8266 UDP internals. The adapter is bounded/nonblocking and no arbitrary shared global SPI/network engine is substituted. Actual AP/STA behavior remains unverified.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/SSDPDevice.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `3d94b63400ab64c0e556b1e73c722b40f7adade996aaf40ce73f88d1d0ab7cf2`.

- Target: `firmware/esp32s3/src/SsdpDevice.h:98 [UTF8-LF sha256=ba308c286db11cfa5cbabf540f784fda419805bc35e287825e264ca59a46feec; anchor=class SsdpDevice {]`.
- Retained: Network agent fully read the actual donor; evidence owner reviewed the complete header diff. Retains packet templates, bounded queue and metadata fields, public discovery/description setters, cache interval and protocol structures. Adds truthful running() state from the real socket/interface.
- Omitted/replaced code: `ESP8266WiFi/WiFiUDP/IGMP includes, ProductConfig and extern singleton declaration`. Disposition: **replaced**. Reason: ESP32 WiFi, explicit socket adapter, Pakar DiscoveryConfig and namespace-owned instance replace platform-specific includes/global ownership. SERVER announces ESP32-S3 rather than falsely identifying as ESP8266. Useful public protocol behavior is retained; physical discovery remains open.
- Remaining: Validate actual S3 AP/STA multicast membership, M-SEARCH responses, alive/update/byebye/rename lifecycle, interface-correct LOCATION, escaped description and router/phone interoperability. Source anchors and compilation do not prove on-LAN discovery.

### PU850-ESP: `LocalLib/Telnet.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a78d88c149d19d8989c6db3dce0731189c2bf46c45fd64b73e9871a80f6c9bdd`.

- Target: `firmware/esp8266/src/TelnetShell.h:13 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/esp8266.ino:2842 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=void serviceTelnet() {]`.
- Retained: Actual five-state IAC/option/subnegotiation parser, escaped IAC, ECHO/SGA negotiation and greeting retained in standalone WiFiClient-based implementation.
- Retained: Pakar tracks option state to suppress greeting-ack negotiation loops; unsupported DO/WILL options are explicitly rejected.
- Omitted/replaced code: `ESPTelnet dependency and ESP8266WEBSERVER_H synchronous-header suppression`. Disposition: **replaced**. Reason: Native WiFiServer/WiFiClient removes the dependency which pulled a conflicting synchronous web-server header into an async server product; the actual protocol parser was retained rather than replaced by plain newline input.
- Omitted/replaced code: `Telnet_Port and listener restart/stop on port or AP/STA availability changes; matching mDNS add/remove`. Disposition: **pending**. Reason: Pakar currently hardcodes port 23 and starts its listener at setup. Donor network lifecycle/port configurability is useful generic behavior and is not fully mapped or runtime tested.
- Omitted/replaced code: `Product-branded ANSI greeting and legacy ShellSession callback hookup`. Disposition: **replaced**. Reason: Pakar's authenticated greeting and instance-owned parser/editor serve the new product. Plain Telnet still transmits credentials without encryption; this must be shown/configurable as a trusted-LAN-only surface.
- Test-code evidence: `tests/telnet_shell_test.cpp:17 [UTF8-LF sha256=f7dd91333d3aced80fdab41b146b98ecd2e9dd3896a504f8a70cfb33912f00e9; anchor=assert(output.empty());]`.
- Remaining: Expose configurable enable/port and network-aware listener/discovery lifecycle; verify reconnect, disabled port, AP and station loss on hardware.
- Remaining: Prove actual Telnet login, command/ACK outcome and simultaneous API/WebSocket clients; source parser tests are not LAN acceptance.

### PU850-ESP: `LocalLib/Telnet.h`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `e85f5f1a4f2767323d9886313ddb2aabfd3acad7d781c2770bc8d2b2d2c6cb15`.

- Target: `firmware/esp8266/src/TelnetShell.h:13 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/esp8266.ino:2842 [UTF8-LF sha256=a65fedddb7e7741739ebdcd04078f7847f844b74e0dc92c5204ceb7bb454688e; anchor=void serviceTelnet() {]`.
- Retained: Actual five-state IAC/option/subnegotiation parser, escaped IAC, ECHO/SGA negotiation and greeting retained in standalone WiFiClient-based implementation.
- Retained: Pakar tracks option state to suppress greeting-ack negotiation loops; unsupported DO/WILL options are explicitly rejected.
- Omitted/replaced code: `ESPTelnet dependency and ESP8266WEBSERVER_H synchronous-header suppression`. Disposition: **replaced**. Reason: Native WiFiServer/WiFiClient removes the dependency which pulled a conflicting synchronous web-server header into an async server product; the actual protocol parser was retained rather than replaced by plain newline input.
- Omitted/replaced code: `Telnet_Port and listener restart/stop on port or AP/STA availability changes; matching mDNS add/remove`. Disposition: **pending**. Reason: Pakar currently hardcodes port 23 and starts its listener at setup. Donor network lifecycle/port configurability is useful generic behavior and is not fully mapped or runtime tested.
- Omitted/replaced code: `Product-branded ANSI greeting and legacy ShellSession callback hookup`. Disposition: **replaced**. Reason: Pakar's authenticated greeting and instance-owned parser/editor serve the new product. Plain Telnet still transmits credentials without encryption; this must be shown/configurable as a trusted-LAN-only surface.
- Test-code evidence: `tests/telnet_shell_test.cpp:17 [UTF8-LF sha256=f7dd91333d3aced80fdab41b146b98ecd2e9dd3896a504f8a70cfb33912f00e9; anchor=assert(output.empty());]`.
- Remaining: Expose configurable enable/port and network-aware listener/discovery lifecycle; verify reconnect, disabled port, AP and station loss on hardware.
- Remaining: Prove actual Telnet login, command/ACK outcome and simultaneous API/WebSocket clients; source parser tests are not LAN acceptance.

### RayanLamp: `internal/i18n/digits.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `e81f35286d38b645ed3846d3806021089663d9b74f95def6394f5ba921793542`.

- Target: `desktop/src/ui/digits.rs:3 [UTF8-LF sha256=5a93b2fe85c492c9cd393e96dce442f82b7f4936c08c3f81ec92e723a8df9929; anchor=pub fn latin]`.
- Target: `desktop/src/ui/cell_search.rs:47 [UTF8-LF sha256=735f49c2159d9eeb61f08a24e41bace6242115a722ad58abcd9463ec54a3413e; anchor=super::digits::latin(query)]`.
- Target: `assets/web/rayanlamp.js:4 [UTF8-LF sha256=56c1f07d95c20925773552af89e6a768b2abf15796fb4911568854b10c6180ac; anchor=function latinDigits]`.
- Target: `mobile/lib/cell_search.dart:5 [UTF8-LF sha256=470ed85cf8b5ed034f63bc4bc84b415fd8d943803880e142e62396826f4481fe; anchor=String latinDigits]`.
- Target: `mobile/lib/device_settings_page.dart:457 [UTF8-LF sha256=ce24d6aa8c8d05095692920a60d990269c741f7756036761f28a5283ed0d1ab3; anchor=int.tryParse(latinDigits(value))]`.
- Retained: Actual Persian/Arabic-Indic-to-ASCII maps adapted into Rust, JavaScript and Dart and invoked by native/Web/mobile cell search and actual mobile port/telemetry fields. Canonical IDs/ranges stay ASCII; peer names, secrets and protocol identifiers are not globally transformed.
- Omitted/replaced code: `ShapePersianDigits visual decimal shaping`. Disposition: **pending**. Reason: Search needs ASCII normalization now. Apply visual shaping only to audited application-owned numeric labels, not arbitrary peer text or wire IDs. Display consumers remain pending.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:28 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=latinDigits('۰۱۲۳۴۵۶۷۸۹]`.
- Test-code evidence: `mobile/test/cell_search_test.dart:15 [UTF8-LF sha256=1fce7766710c10169453820ae4a6d39ba8625149ea0181989fccc6689b109bce; anchor=RayanLamp numeric normalization]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:208 [UTF8-LF sha256=da7de6a91d19dee9231d9b16e80da25864f4bcc84146d1b4425564f98d6d234b; anchor=expect(api.cloud['mqtt_port'], 8883)]`.
- Remaining: Finish owned numeric display shaping and execute guarded native digit/search tests.

### RayanLamp: `internal/terminal/policy.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `c250474261e4ce8d6f63826719a408b0ae57a2533ec547c9a151c9939b5b5518`.

- Target: `desktop/src/terminal_ui.rs:108 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn color_allowed()]`.
- Retained: Actual NO_COLOR/TERM=dumb/CLICOLOR=0/FORCE_COLOR=0 opt-out precedence and redirected-input/output rejection are retained; crossterm owns native VT/raw transitions, checked on entry.
- Omitted/replaced code: `Emoji/style detection preferences and broader platform policy tests`. Disposition: **pending**. Reason: Current TUI uses conservative text only and no donor emoji UI. Per-platform style controls/detection tests remain useful pending work, not accepted exclusions.
- Test-code evidence: `desktop/src/terminal_ui.rs:987 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn donor_text_safety_preserves_unicode_not_peer_terminal_commands()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/keys.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `9db93f624f1fcc5b850993de2c6cd2ca1304bf91034849f246dd54fb70a02720`.

- Target: `desktop/src/terminal_ui.rs:297 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn key(&mut self, key: KeyEvent)]`.
- Retained: Contextual Tab/ShiftTab, arrows, Help/quit and Find draft Enter/Escape route into shared Pakar selection and explicit capability-gated actions; repeat presses cannot queue writes.
- Omitted/replaced code: `Theme/mouse/reconnect/help catalog, localized binding vocabulary and donor lamp actions`. Disposition: **pending**. Reason: Lamp-specific actuation is replaced by ACKed table actions. Useful theme, mouse, reconnect and complete localized help remain pending rather than hidden; g refresh and r red are deliberate table bindings.
- Test-code evidence: `desktop/src/terminal_ui.rs:1012 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn shared_find_selection_and_arrow_navigation_never_write()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/run_test.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `2d645e6b27b7a4d17afc027e8393471d6b19b7beec6bbf359cee1c8588ee832a`.

- Target: `desktop/src/terminal_ui.rs:118 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn enter()]`.
- Retained: The actual donor fail-before-control-sequences guard is retained; built executable piped-child fixture proves zero stdout/control escapes and zero HTTP requests.
- Omitted/replaced code: `Independent stdin-only/stdout-only terminal fixtures and real interactive raw-mode acceptance`. Disposition: **pending**. Reason: The built test redirects both descriptors; separate mixed-descriptor and actual console navigation/cleanup acceptance remain pending. Do not equate guard tests with full terminal acceptance.
- Test-code evidence: `desktop/src/terminal_ui.rs:999 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn offline_and_unacknowledged_peers_have_no_table_actions()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/run.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `462fdbc977b5261f971120fd071b7dc6b023e6d6e66fd37e2dabf03af12c0a16`.

- Target: `desktop/src/terminal_ui.rs:829 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=pub fn run(endpoint: Endpoint)]`.
- Retained: Terminal/stdin/stdout ownership, early redirected guard, independent request worker, bounded exit cleanup and terminal-neutral command text are adapted to crossterm and the existing Rust HTTP client.
- Omitted/replaced code: `Bubble Tea program/context/screen options, UI theme/ASCII preferences and transport-manager ownership`. Disposition: **pending**. Reason: Go toolkit-specific ownership is replaced by crossterm RAII and the existing Rust transport; theme controls and transport discovery/USB choice still require actual Pakar implementation.
- Test-code evidence: `desktop/src/terminal_ui.rs:999 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn offline_and_unacknowledged_peers_have_no_table_actions()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/runtime.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1d6f08a67f0076c36a7951d79e7d35c9d8044090dbf60c9105bf4307ed052028`.

- Target: `desktop/src/terminal_ui.rs:835 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=let worker = thread::spawn(move ||]`.
- Retained: Independent model/network scheduling and deterministic keyboard/receipt tests adapt to a bounded Rust worker/channel. Shared incident log coalesces repeats but retains distinct causes and revokes lost peer controls.
- Omitted/replaced code: `Donor spinner/expiring-notice/theme scheduling and complete runtime injection surface`. Disposition: **pending**. Reason: The new renderer shows observed busy state without fake device progress. Full spinner/theme scheduling and all runtime adapters remain pending; current model plus HTTP doubles do not certify hardware.
- Test-code evidence: `desktop/src/terminal_ui.rs:1039 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn loss_and_peer_change_revoke_controls_without_false_recovery()]`.
- Test-code evidence: `tests/terminal_ui_test.mjs:23 [UTF8-LF sha256=3916ae657e18de899b278509cd0f8fad53886579c9af53deebc6e47cb5ff9b26; anchor=TUI must reject redirected surfaces before contacting a device]`.
- Scoped advantage: Bounded checkpoint only; no whole-repository superiority or physical acceptance claim.
- Remaining: Complete useful omitted behavior and actual terminal/board acceptance; issue #2 remains open.

### RayanLamp: `internal/tui/text.go`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `ac575e39f688d9c796f4b013fdb768b0b3ecb5bbda04908950f14fc7217e327e`.

- Target: `desktop/src/terminal_ui.rs:25 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=pub fn safe_line(value: &str)]`.
- Retained: Actual sanitizeTerminalLine unsafe-control/bidi filtering and cell-aware truncation adapt to Unicode Rust strings and unicode-width. Removes OSC/CSI/DCS sequences and bounds work while retaining Persian text.
- Omitted/replaced code: `Donor renderer-specific text entry points and full terminal shaping acceptance`. Disposition: **pending**. Reason: Rust TUI renderer calls one sanitizer for every emitted line rather than copying Go view APIs. Actual Persian shaping/terminal-font behavior is not inferred from string-preservation tests.
- Test-code evidence: `desktop/src/terminal_ui.rs:987 [UTF8-LF sha256=8750a3b3f5ed8ceb2434c68217aaf0eb9c4f6fd9003dbfa2692142b3c37ce225; anchor=fn donor_text_safety_preserves_unicode_not_peer_terminal_commands()]`.
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
- Target: `tools/generate-web-appearance.mjs:33 [UTF8-LF sha256=7fb8bbd16edbff1798dd77e16ee2abbf32ac34d0adf57e63baa4673abdf9d6a2; anchor=replaceBlock('// BEGIN BROWSER CONTROLLER']`.
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=<script data-pakar-presentation>]`.
- Retained: Actual safe validated storage read, requested/OS theme resolution, root color-scheme and chrome metadata, locale/direction and resolved-locale pre-paint behavior are adapted into the single head bootstrap. It runs without dialog/body nodes, requests, preference writes or application/peer title mutation; generator checks canonical equality.
- Omitted/replaced code: `Separate same-origin /theme-init.js, duplicated theme resolver, lamp colors and rayanlamp.web.locale auto/language-list some(fa) selection`. Disposition: **replaced**. Reason: Inline canonical bootstrap shares the existing Pealayer helpers and palette once, before CSS, saving an ESP endpoint and extra request. Existing Pakar locale keys use system/en/fa, primary browser language and explicit Auto/LTR/RTL override, consistent with current runtime resolver. Supporting a secondary Persian language must not unexpectedly override an English primary choice. Existing dark/Studio defaults are retained rather than silently changing user appearance.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:55 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=Pre-paint survives blocked storage]`.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:41 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp pre-paint bootstrap runs before CSS/body]`.
- Remaining: Post-bootstrap generic dynamic copy, browser language-change lifecycle and complete native/Web/mobile localization remain pending in the existing localization concern; this import certifies pre-paint behavior, not those separate source files.

### RayanLamp: `webui/src/api.test.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `6dfacf9fb7bff4896ac699547c46bf100c744b9017bec554ef42b9ea4d9c9f5b`.

- Target: `assets/web/peer-http.js:5 [UTF8-LF sha256=a56aca0f2a159a0bdf22f1db0bde8541aa745693282167c28d3a0d08cb6c67d3; anchor=class PeerAPIError]`.
- Retained: Full test source read through EOF. Actual same-origin request and exact mutation-header/body scenarios adapted to the real Web peer client with server-advertised board/boot pins; cookie auth is retained separately, pins are not substituted for CSRF/auth.
- Retained: Typed status/error, no credential/body logging, no 401 write replay and versionless paths are exercised in existing embedded sign-in/console and new peer tests.
- Omitted/replaced code: `establishSession forced renewal coalescing and X-RayanLamp-CSRF/If-Match session/config assertions`. Disposition: **pending**. Reason: ESP cookie/origin authentication is different from the donor Go session endpoint. Explicit no-replay replaces 401 retries; generic coalescing/revision conflict tests remain useful pending the matching server/client contracts.
- Omitted/replaced code: `lamp toggle/on/off living routes`. Disposition: **not-applicable**. Reason: Lamp commands are donor business; table/controller capability modes are retained instead.
- Omitted/replaced code: `diagnostics, UI bootstrap/config, audio content-addressed import/playback, hotkey capture lease and integration registration assertions`. Disposition: **pending**. Reason: All are useful generic host contracts beyond this bounded device HTTP safety slice; need actual typed host implementations and contract fixtures, not fake ESP endpoints.
- Test-code evidence: `tests/web_peer_http_test.mjs:63 [UTF8-LF sha256=e6c5d2fec06ef43602252c62e341f0126c457b11c9fe5bb8a08636301ce37f0c; anchor=exact pin headers]`.
- Test-code evidence: `tests/web_access_test.mjs:7 [UTF8-LF sha256=2c4d232af4e2ed42d74ae96e282bc8a6c1ae38180cf5bc146bbb8834cb1804f7; anchor=Execute the actual embedded Web application]`.
- Remaining: Port authenticated coordinator/bootstrap/config/audio/hotkey/registration tests as their corresponding host contracts are imported. Do not claim the complete donor test suite from this request-header/error slice.

### RayanLamp: `webui/src/api.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1b201ac38c3ca4f45dea639d198204f294c83f7e07fb511be99b3d54154fc741`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=255036e79060b6d348944e5f1d1edc90ac1c79f20dc1a095a4384b38d85d15f0; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:993 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=async function refreshAuthenticated(options={})]`.
- Target: `assets/web/peer-http.js:10 [UTF8-LF sha256=a56aca0f2a159a0bdf22f1db0bde8541aa745693282167c28d3a0d08cb6c67d3; anchor=function createPeerHttp(]`.
- Retained: Fully read actual api.ts. APIError status/code, Headers Accept, same-origin/no-store fetch, AbortController 12000 ms deadline and finally cleanup now adapted to all embedded Web HTTP requests in the shared mechanically embedded peer-http.js, not reconnect GET only.
- Retained: Pakar cookie authentication and expired-session login are retained; no uncertain mutating request/session replay, credentials or raw endpoint/body diagnostics copied.
- Omitted/replaced code: `establishSession/requestSession, CSRF header and request retrySession on 401`. Disposition: **replaced**. Reason: Go coordinator /app/session is absent on ESP. Reconnect checks the existing authenticated GET; 401 cancels recovery and requires sign-in. Blind mutating retry is incompatible with no-uncertain-write-replay. Generic session coalescing/authorization remains pending host-contract adaptation.
- Omitted/replaced code: `lamp on/off/toggle routes`. Disposition: **not-applicable**. Reason: Lamp business commands do not apply to discovered table/other board modes. Existing capability-ACK controller actions remain authoritative.
- Omitted/replaced code: `bootstrap/getConfig/patchConfig If-Match, diagnostics and diagnosticsRefreshTimeout`. Disposition: **pending**. Reason: Typed bootstrap, If-Match revision/config concurrency and long-running diagnostic contracts remain pending; all Web HTTP requests now have cancellation/deadlines, but this is not atomic revision-controlled server persistence.
- Omitted/replaced code: `RayanLampClient interface, APIError/isEnvelope/unwrapResponse/readJSON and realtime event decoding`. Disposition: **pending**. Reason: APIError and advertised JSON parse validation are adapted. Full RayanLampClient typed interface, isEnvelope/unwrapResponse result schema and realtime decoding still require contracts.ts/direct-api/client-factory imports; Pakar keeps real flat JSON and plain-text receipts, not invented Go envelopes.
- Omitted/replaced code: `hotkey capture begin/renew/release/probe, integration registration and window/show/hide`. Disposition: **pending**. Reason: Generic host/desktop operations need verified native capability, lease ownership and full surfaces. No nonexistent ESP endpoints are exposed.
- Omitted/replaced code: `notify/playAudio/importAudio/validateAudioRequest/safeAudioFilenameHeader`. Disposition: **pending**. Reason: Generic host/browser audio/notification/import support remains useful; AVR melody/buzzer control is not equivalent WAV playback or content-addressed host import. Adapt discovered host/device ownership and port exact tests.
- Test-code evidence: `tests/realtime_retry_test.mjs:54 [UTF8-LF sha256=93051463cde69c096dfa66dbba285cde9c41c956dbf7db4ceec20010daefe60d; anchor=Actual api.ts AbortController deadline/finally cleanup]`.
- Test-code evidence: `tests/web_peer_http_test.mjs:49 [UTF8-LF sha256=e6c5d2fec06ef43602252c62e341f0126c457b11c9fe5bb8a08636301ce37f0c; anchor=Deadline, timer release and external cancellation port api.ts]`.
- Remaining: api.test.ts was fully read; all its authenticated host/config/audio/hotkey/registration scenarios remain pending matching implementations, not a completed import certificate.
- Remaining: Complete typed structured response schemas, revision concurrency, session coalescing and native/mobile adoption; actual LAN and physical acceptance remain open.

### RayanLamp: `webui/src/browser-console.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `024ed161c6c71655ef67cdbed8cf743fd33864118604737a5f44f533bf9f48f5`.

- Target: `tests/browser_console_test.mjs:1 [UTF8-LF sha256=be3b1acb621e2fee5ff3cab473d882fe917e78c4b16e904ff4cdd6dc73af293c; anchor=Actual RayanLamp browser-console.test.ts scenarios]`.
- Retained: All actual donor startup, five-level dispatch, allowlist/redaction, keyed dedupe, Error-code-only and control/Bidi scenarios ported against shipped code. Added bounds/cycles/prototype, invalid sink/date, current palette, real HTTP secret/no-replay/quiet-poll, stale WebSocket ownership and canonical embedding checks.
- Omitted/replaced code: `Vitest describe/it.each/vi sink mocks`. Disposition: **replaced**. Reason: Built-in Node assertions/VM sink and controlled time run the same scenarios plus real production adapter code without adding dependencies or build caches.
- Omitted/replaced code: `Lamp state names and fixed brand color expectations`. Disposition: **replaced**. Reason: Tests assert adapted device vocabulary and actual shared Pealayer colors rather than claiming the lamp identity/colors apply to Pakar.
- Test-code evidence: `tests/browser_console_test.mjs:75 [UTF8-LF sha256=be3b1acb621e2fee5ff3cab473d882fe917e78c4b16e904ff4cdd6dc73af293c; anchor=Real HTTP adapter]`.
- Remaining: VM execution is not pixels, deployment, LAN or physical board acceptance.

### RayanLamp: `webui/src/browser-console.ts`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `5584e97f745eadd5fa52dd1cbec91813dd4f0638da0ca4cf7ca986658f730165`.

- Target: `assets/web/browser-console.js:77 [UTF8-LF sha256=7789b2a4baae93afdd45a0a803f8567722ef0b9ea776c1051fc8482be93e0b60; anchor=class StyledBrowserConsole]`.
- Target: `firmware/esp8266/src/WebUi.h:1005 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function connectWs()]`.
- Retained: Actual five-level styled sink/startup, operational event vocabulary, allowlisted metadata, recursive sanitizer, safe Error-code projection and keyed quiet-window deduplication imported into canonical browser module. Real HTTP and WebSocket lifecycle consumers execute it; routine successful polling, telemetry and binary frames stay quiet.
- Retained: Actual URL/Bearer/JWT/query, single-line/control-character protection, depth/array/object bounds and cycle handling retained; no interception, secret/raw-response trace or persistent writes.
- Omitted/replaced code: `Lamp-only lamp.state/lamp.command vocabulary and RAYAN LAMP startup brand`. Disposition: **replaced**. Reason: Pakar uses device.event/device.command and its controller identity, not another device's lamp protocol. Generic operational semantics are retained.
- Omitted/replaced code: `Hardcoded lamp palettes/gradient and monospace badge typography`. Disposition: **replaced**. Reason: Current shared Pealayer palette and controller typography are resolved on each emission; no second fixed color engine or unwanted tint.
- Omitted/replaced code: `Unbounded recent Map, boundary-only sensitiveKey, caller-controlled percent format, raw DOM id/function name and throwing host sink`. Disposition: **replaced**. Reason: 256-key/60-second bounds, camelCase redaction, percent escaping, minimal DOM/function summaries and fail-closed sink protect long-lived firmware clients and secret-bearing host data.
- Omitted/replaced code: `Vitest/TypeScript runtime packaging`. Disposition: **replaced**. Reason: ESP's existing classic-script embedding and Node VM tests require no React/Vitest dependency, donor build or duplicate firmware copy.
- Test-code evidence: `tests/browser_console_test.mjs:31 [UTF8-LF sha256=be3b1acb621e2fee5ff3cab473d882fe917e78c4b16e904ff4cdd6dc73af293c; anchor=Actual allowlist + recursive redaction]`.
- Test-code evidence: `tests/browser_console_test.mjs:117 [UTF8-LF sha256=be3b1acb621e2fee5ff3cab473d882fe917e78c4b16e904ff4cdd6dc73af293c; anchor=Actual WebSocket ownership]`.
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
- Target: `firmware/esp8266/src/WebUi.h:1000 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function report(e,tone)]`.
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
- Target: `assets/web/localization.js:47 [UTF8-LF sha256=9761b88471261b763819a3ad76b332d4aa5243bf2d81a9c37b293e1be2b5ac1b; anchor=subscribeLocalePreferences(value=>]`.
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
- Target: `firmware/esp8266/src/WebUi.h:226 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=id="uiHaptics"]`.
- Retained: Actual pulse constants, bounded audio cues, optional vibration API, fail-closed visibility/reduced-motion conditions, copied pattern arrays and host-error containment adapted into embedded JavaScript. Opt-in Touch feedback defaults off, disabled when unsupported; only explicit command HTTP results trigger it, never polling/telemetry/frames.
- Omitted/replaced code: `TypeScript interfaces/private/type annotations`. Disposition: **replaced**. Reason: ESP serves dependency-free JavaScript. Actual runtime bodies are retained; explicit own-property cue validation replaces a compile-time-only guarantee.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:24 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=h.trigger('arbitrary raw frame')]`.
- Scoped advantage: Unknown JavaScript cues now fail closed at runtime.
- Remaining: Verify supported Android vibration and actual command lifecycle. HTTP acceptance is not actuator readback.

### RayanLamp: `webui/src/i18n.test.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1c72c37780aeefe39c4d571636616e10c4bbc0fb0b57df8332e9b62c144d599b`.

- Target: `tests/rayanlamp_locale_test.mjs:1 [UTF8-LF sha256=8e1cabaca26644801b145877f6b6d682e5153a7270d90087f1104e6f71bf6a2a; anchor=Actual RayanLamp i18n.test.tsx contracts]`.
- Retained: Actual normalization, Persian-vs-Arabic detection, translation/placeholder/visual-number and automatic languagechange tests adapted to actual shipped code. Added explicit language/direction stability, low-write persistence, new-owner teardown, identity/draft preservation, reversible dynamic labels and detached-node pruning.
- Omitted/replaced code: `Vitest/Preact, lamp title/storage identity and secondary-language expectations`. Disposition: **replaced**. Reason: Node/VM executes the dependency-free embedded module. Pakar deliberately retains primary language, explicit direction and acknowledged board identity; tests document each difference.
- Omitted/replaced code: `Full actual native/mobile/browser pixel and cross-interface acceptance`. Disposition: **pending**. Reason: Actual DOM-bound BDI and calendar tests now execute the shipped helper and verify real summary/NTP call sites. VM DOM contracts are not rendered pixels or the full platform/interface matrix.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:67 [UTF8-LF sha256=8e1cabaca26644801b145877f6b6d682e5153a7270d90087f1104e6f71bf6a2a; anchor=Restart and idempotent teardown]`.
- Remaining: Inspect current protocol/date rendering in the browser; complete remaining consumers and native/mobile/browser matrix. VM tests are not pixels or physical board proof.

### RayanLamp: `webui/src/i18n.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `9a4843ade326c1ee323c3f60d6b0d6ac41cab9d2dcd50affed276fdd8096ff0a`.

- Target: `assets/web/localization.js:43 [UTF8-LF sha256=9761b88471261b763819a3ad76b332d4aa5243bf2d81a9c37b293e1be2b5ac1b; anchor=function startUiLocaleController()]`.
- Target: `firmware/esp8266/src/WebUi.h:799 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function renderSelectionSummary()]`.
- Retained: Actual normalization/detection, t placeholder fallback, visual digits, Intl.NumberFormat and languagechange lifecycle adapted into the real embedded localization module. Selection counts and batch readback messages are real consumers. Restart ownership, idempotent cleanup, missing/inherited placeholder handling, blocked storage and no OS-triggered preference writes are added.
- Retained: Dynamic network/cloud labels and panel menus retranslate reversibly without replacing board identity, hostnames, mapping IDs, protocol strings or unsaved secrets.
- Retained: Actual BidiLtr bdi/dir=ltr isolation and formatDateTime Persian/Gregorian calendar rules now serve the real hostname/URL summary and observed NTP time. TextContent-only rendering never parses peer HTML or rewrites protocol values.
- Omitted/replaced code: `Preact signals, lamp storage key/auto enum, any secondary Persian language and unconditional render-time persistence`. Disposition: **replaced**. Reason: ESP uses existing dependency-free state, system enum and presentation channel. Primary browser language and independent explicit direction preserve Pakar pre-paint policy. Persistence belongs only to validated preference changes; no OS/render writes or second toolchain.
- Omitted/replaced code: `Rayan Lamp document title/meta branding and lamp/coordinator/COM/URI-specific literal captions`. Disposition: **not-applicable**. Reason: These describe another application's lamp identity/endpoints. Pakar title follows independently acknowledged board application metadata; translating peer names or claiming unrelated lamp actions would be incorrect. Generic mechanisms remain independently pending.
- Omitted/replaced code: `Additional generic dictionary/settings/protocol consumers, protocol monospace styling and native/mobile locale synchronization`. Disposition: **pending**. Reason: Web hostname/URL summary now renders real BDI and NTP uses the actual calendar formatter. Remaining input/hash/status consumers, monospace styling and native/mobile/browser pixel acceptance are not implied by those two consumers or VM tests.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:55 [UTF8-LF sha256=8e1cabaca26644801b145877f6b6d682e5153a7270d90087f1104e6f71bf6a2a; anchor=Automatic locale reacts to OS changes]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:118 [UTF8-LF sha256=38dae57d1e982f90edb831eb1381fb6323d7199c908f843b3295631d53444dbd; anchor=Selection must not mutate hardware]`.
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
- Test-code evidence: `tests/realtime_retry_test.mjs:72 [UTF8-LF sha256=93051463cde69c096dfa66dbba285cde9c41c956dbf7db4ceec20010daefe60d; anchor=Execute actual Web connectWs + refreshAuthenticated + api]`.
- Remaining: Port every useful remaining donor sequence/lease/config/audio/shutdown assertion with its actual consumer; no whole-test-file parity claim.
- Remaining: Real LAN, multi-client and native/mobile lifecycle verification remains required.

### RayanLamp: `webui/src/sync.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `bd849cbef1ee67ac97cca9e0c6e00ed69acf725d805ad986ae58f11513546d73`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=255036e79060b6d348944e5f1d1edc90ac1c79f20dc1a095a4384b38d85d15f0; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:1005 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=function connectWs()]`.
- Retained: Actual scheduleReconnect 500 ms exponential base, 10-second cap and 0.8..1.2 jitter, prepareReconnect-before-open, reset-on-open and stop cleanup adapted to actual Pakar WebSocket/status GET consumers.
- Retained: Added one pending owner, bounded exponent, late timer/promise generation checks, login/pause/pagehide cancellation and constructor-failure recovery; no device command is replayed.
- Omitted/replaced code: `start/electLeader/acquireLeadership/releaseLeadership and BroadcastChannel/Web Locks fallback lease election/heartbeats/watchdogs/connectionSyncNames`. Disposition: **pending**. Reason: Generic per-origin leadership is useful but not imported. Pakar supports independent authenticated streaming clients; a shared socket requires validated command/stream ownership and follower readback. Compare full client/contracts/store dependencies and port actual multi-tab/takeover tests before enabling.
- Omitted/replaced code: `handleServerEvent/onChannelMessage/acceptSnapshot and serverSequenceDecision`. Disposition: **pending**. Reason: Generic typed snapshot/config/diagnostic projection, revisions and epoch resets remain pending actual Pakar advertised contracts. Existing source-bound contextual readiness/no-replay ownership is retained; do not invent server event sequences or treat arbitrary channel objects as authoritative.
- Omitted/replaced code: `handleDirectStateCue initial on/off transition`. Disposition: **not-applicable**. Reason: Lamp state transitions are not table/first-wins business. Adapt only future discovered board events, not hardcode lamp state.
- Omitted/replaced code: `audioCueCount/playDirectCue, generic message/haptic/diagnostic fanout and setConnectionState/relayDisconnectReason`. Disposition: **pending**. Reason: Existing semantic haptics/toast/trace helpers do not implement full leader-only audible notification or graceful coordinator state. Generic audio/count/shutdown semantics remain useful pending exact firmware/host event and capability mapping.
- Test-code evidence: `tests/realtime_retry_test.mjs:21 [UTF8-LF sha256=93051463cde69c096dfa66dbba285cde9c41c956dbf7db4ceec20010daefe60d; anchor=Donor prepareReconnect is completed before any replacement socket]`.
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
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=6a68077dd5ade973b060269b6dfde17dbc10576091f25173c23107cfb6a55c44; anchor=<script data-pakar-presentation>]`.
- Retained: Actual startWebThemeController detach/restart ownership, idempotent stop, captured listener cleanup, System-only OS reconciliation, safe stored selection and browser color-scheme/theme-color metadata adapted into startBrowserAppearanceController/applyBrowserAppearance. Used by actual pre-paint embedded Web UI, with storage-event reset handling and no hardware writes.
- Retained: Pealayer appearance helpers and four shared palettes remain the only color engine; requested theme and resolved theme are distinct root metadata. Existing form persistence skips identical writes and retains session selection on storage failure.
- Omitted/replaced code: `Preact signal state, RayanLamp storage key/default System, root.dataset.theme as requested mode, fixed lamp theme colors and applyWebTheme persistence entrypoint`. Disposition: **replaced**. Reason: Dependency-free ESP HTML uses existing browserAppearance state and pakar-appearance object with explicit Pakar dark/Studio defaults. Existing resolved root.dataset.theme is preserved for CSS; themePreference and resolvedTheme retain both states. Shared palette canvas drives browser chrome, not lamp colors. Existing form is the persistence owner; rendering and OS events never rewrite preferences.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:49 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp controller restart and idempotent teardown]`.
- Test-code evidence: `tests/web_appearance_test.mjs:91 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=unavailable storage still applies]`.
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
