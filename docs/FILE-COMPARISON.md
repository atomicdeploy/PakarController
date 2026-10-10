# PakarController donor comparison

Whole-repository superiority is **not established**. This report exposes retained code, scoped improvements, omissions and gaps; it does not turn missing reviews into exclusions.

Routing: 1317 paths have no specialist capability match. They remain visible through `consult-reuse.ps1 -Concern donor-audit` and issue #3; lack of a keyword match is NOT an exclusion. Every CSV row includes capability routing and owning issue links.

Inventory: 4237 files; 67 complete source-file comparisons; 4170 pending comparisons; 42 compared files with known parity gaps. Header/implementation pairs can describe the same gap. Missing sparse files remain pending.

Every inventoried path, source revision/hash, target/proof file and line, omitted code, disposition and concrete reason is in `file-comparison.csv`. Hash-bound manual decisions are in `file-comparison-evidence.json`. Donor SHA-256 values bind original bytes; target/proof SHA-256 values bind UTF-8 without BOM with LF newlines. Anchors are recomputed into exact line numbers. Test-code presence is not test execution; execution receipts are in `FILE-COMPARISON-RECEIPTS.md`.

## Coverage

| Donor | Inventoried | Compared | Pending | Files with gaps |
|---|---:|---:|---:|---:|
| ControllerBoardMini | 19 | 0 | 19 | 0 |
| ControllerBoardMini/Puzzles | 18 | 12 | 6 | 9 |
| PCController | 1211 | 9 | 1202 | 3 |
| Pealayer | 509 | 36 | 473 | 23 |
| PU850-ESP | 109 | 4 | 105 | 4 |
| PuzzleBoard | 22 | 0 | 22 | 0 |
| RayanLamp | 2349 | 6 | 2343 | 3 |

## Exact reviewed-file mappings and omission decisions

### ControllerBoardMini/Puzzles: `LocalLib/Keys.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9ceb7e764ef9d4d5feb62b2c7a22eed59d614b7b56432fae87ce87b4aa434578`.

- Target: `firmware/avr/avr.ino:409 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:394 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void boardKeyGesture(]`.
- Retained: Active-low physical plus virtual shift input state and candidate/stable transition tracking; AVR reports releases as dirty state; press handling is mode-specific.
- Retained: Actual PCController per-key callback/gesture classifier now feeds all nine inputs; no game action waits for click classification.
- Omitted/replaced code: `DEBOUNCE_MS = 50`. Disposition: **pending**. Reason: Actual PCController classifier now uses macro-configurable PAKAR_KEY_DEBOUNCE_MS (default 20ms) with tested Down/Up, click and hold timing. Physical switch-bounce measurements remain pending; faster is not certified electrically.
- Omitted/replaced code: `Key virtual readCurrentState/onStateChanged, setCallback, bool/getCurrentState interface`. Disposition: **replaced**. Reason: Imported PCController per-key classifier, setCallback and setEventCallback are retained. updateSample separates the input driver from gesture classification and supports all nine shift/ADC keys; subclass-specific raw reads are replaced by the actual board sample adapter.
- Omitted/replaced code: `Raw Puzzles lower-four input order`. Disposition: **pending**. Reason: Current PCController-based register reader reverses those bits. Physical key identity must decide the mapping; source reuse alone cannot.
- Remaining: Add input mapping/debounce configuration and bounce/press/release tests; preserve reusable callback capability or record a reviewed alternative contract.

### ControllerBoardMini/Puzzles: `LocalLib/Keys.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `caa21fe97e3df262f0821bd582b60f2895a14ec193d7d6fec13321afe4c5a46d`.

- Target: `firmware/avr/avr.ino:409 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:394 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void boardKeyGesture(]`.
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

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `3f5ef23f7a8052d8086b2803343e3e22bb9751924b7760077950f4b0bd2cc196`.

- Target: `firmware/avr/avr.ino:409 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void serviceInputs(]`.
- Retained: Only cooperative target-specific timers exist in current AVR services; the generic donor task scheduler is not implemented.
- Omitted/replaced code: `Tasks::addTask/cancelTask/update and ten callback slots`. Disposition: **pending**. Reason: Front-panel deadlines do not replace arbitrary scheduled callbacks or cancellation. It is useful infrastructure and needs an explicit replacement, not a business-logic exclusion.
- Omitted/replaced code: `startsAt==0 free-slot sentinel and currentTime > startsAt comparison`. Disposition: **pending**. Reason: Do not copy the rollover/zero-deadline ambiguity unchanged. A replacement must keep the generic scheduling/cancellation API using explicit occupancy and wrap-safe deadlines; such a replacement is not present yet.
- Remaining: Implement and test a bounded cooperative scheduler: cancellation, full queue, zero delay, callback reentrancy and millis rollover.

### ControllerBoardMini/Puzzles: `LocalLib/Tasks.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `5d838a7fb53c799160d55a65b22546ab2e749eb30f8412ed5d08e9ce44300fed`.

- Target: `firmware/avr/avr.ino:409 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=void serviceInputs(]`.
- Retained: Only cooperative target-specific timers exist in current AVR services; the generic donor task scheduler is not implemented.
- Omitted/replaced code: `Tasks::addTask/cancelTask/update and ten callback slots`. Disposition: **pending**. Reason: Front-panel deadlines do not replace arbitrary scheduled callbacks or cancellation. It is useful infrastructure and needs an explicit replacement, not a business-logic exclusion.
- Omitted/replaced code: `startsAt==0 free-slot sentinel and currentTime > startsAt comparison`. Disposition: **pending**. Reason: Do not copy the rollover/zero-deadline ambiguity unchanged. A replacement must keep the generic scheduling/cancellation API using explicit occupancy and wrap-safe deadlines; such a replacement is not present yet.
- Remaining: Implement and test a bounded cooperative scheduler: cancellation, full queue, zero delay, callback reentrancy and millis rollover.

### ControllerBoardMini/Puzzles: `LocalLib/TonePlayer.cpp`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `97954eecd111d4c1bd092488ee395b7ec8eb4630ed507c9177af9fba6d434fc5`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=03ede515e478fd936c0414a9332b50cb9dd07c540a22a11738317e18220da1f3; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=ca7d0f91005fed47fa5410e2109f7a667a86fc2d56652ea8f08a1243f02a224f; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:20 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:26 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9f2aea4eadd0a4112ba62d4bdac906a9749026aa4e69404c2ff3169667f204df`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=03ede515e478fd936c0414a9332b50cb9dd07c540a22a11738317e18220da1f3; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=ca7d0f91005fed47fa5410e2109f7a667a86fc2d56652ea8f08a1243f02a224f; anchor=startsAt = stepEndsAt_]`.
- Retained: Ten queued tones/pauses and PB1/D9 buzzer ownership; note constants retained in pitches.h.
- Omitted/replaced code: `waitForTonesEnd blocking loop`. Disposition: **replaced**. Reason: Nonblocking update() plus isBusy() preserves completion observation while allowing UART, input and front-panel services to run; a busy wait would starve those required services.
- Omitted/replaced code: `Global addTone/resetTone/playToneSequence state/functions and millis()+duration deadline`. Disposition: **replaced**. Reason: Object-owned enqueue/stop/update retains queue lifecycle and avoids namespace/global ownership collisions; deadline anchoring and signed rollover comparisons replace accumulating loop delay.
- Omitted/replaced code: `Unbounded frequency and zero-duration acceptance`. Disposition: **replaced**. Reason: Requests outside the explicit hardware/wire range are rejected rather than accepted without reliable output.
- Test-code evidence: `tests/tone_player_test.cpp:20 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:26 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.setMuted(false)]`.
- Scoped advantage: Hardware compare toggling replaces the older donor software approach; nonblocking deadline anchoring passes late-call/rollover tests.
- Remaining: Register-double tests are not audible or physical clock proof.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `0f7fec7879abd26dbf2d56df28441727ea76fcb9e093f009d39f20cae77832bb`.

- Target: `firmware/esp8266/src/LedGrid.cpp:16 [UTF8-LF sha256=18feb27f5eaa57208a60d92f809d395ddff34427bb7d2fcfc531b6c32623298c; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:59 [UTF8-LF sha256=fcbc8d0c6f3edfcb9be73964081a82407d489ff348c02ce9f70378f5676a4774; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: User assigned final WS2811 effects to ESP D2 and 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `395096029df7c507782f40c5b32c8b1dffbf2db392cb26cd2b02c824d6a36949`.

- Target: `firmware/esp8266/src/LedGrid.cpp:16 [UTF8-LF sha256=18feb27f5eaa57208a60d92f809d395ddff34427bb7d2fcfc531b6c32623298c; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:59 [UTF8-LF sha256=fcbc8d0c6f3edfcb9be73964081a82407d489ff348c02ce9f70378f5676a4774; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: User assigned final WS2811 effects to ESP D2 and 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### PCController: `LocalLib/SevenSegments.cpp`

Verdict: **scoped-improvement**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `54c366525142aca0103a8d58f7ce8d9cbc34c1b5884b20a1ad5ae50ddc6bb4a8`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:98 [UTF8-LF sha256=28547987e3fe6bf85e13763fa93ef8bb234f8d8f6204b43658058f070f6d9228; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:266 [UTF8-LF sha256=562145677bbcee14307aeeaed2a720f6138cc9ca156a1f5423b6696d97a7aff8; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:450 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/SevenSegments.h`

Verdict: **scoped-improvement**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `e58e1783404f494ea7b1852780b6485cbfda872781b9159b9ee29426edae9240`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:98 [UTF8-LF sha256=28547987e3fe6bf85e13763fa93ef8bb234f8d8f6204b43658058f070f6d9228; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:266 [UTF8-LF sha256=562145677bbcee14307aeeaed2a720f6138cc9ca156a1f5423b6696d97a7aff8; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:450 [UTF8-LF sha256=5e568156133dec9eaffbdec4d5196fb6113693d2715387b78d5f9b3a471e23c3; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/ShiftRegisters.cpp`

Verdict: **equivalent-file**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133`.

- Target: `firmware/avr/ShiftRegisters.cpp:1 [UTF8-LF sha256=43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133; anchor=#include "ShiftRegisters.h"]`.
- Retained: Byte-identical donor file, including eight active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- No production-code omission claimed for this reviewed file.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/ShiftRegisters.h`

Verdict: **equivalent-file**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff`.

- Target: `firmware/avr/ShiftRegisters.h:32 [UTF8-LF sha256=397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff; anchor=class ShiftRegisters]`.
- Retained: Byte-identical donor file, including eight active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- No production-code omission claimed for this reviewed file.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/TonePlayer.cpp`

Verdict: **scoped-improvement**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `e4715e126bcaa342fc3004f2d0fbc300921710d8e1ea7e6b44fc8fe7870ef343`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=03ede515e478fd936c0414a9332b50cb9dd07c540a22a11738317e18220da1f3; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=ca7d0f91005fed47fa5410e2109f7a667a86fc2d56652ea8f08a1243f02a224f; anchor=startsAt = stepEndsAt_]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:20 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:26 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.setMuted(false)]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; unmute restarts the active tone.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.

### PCController: `LocalLib/TonePlayer.h`

Verdict: **scoped-improvement**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `edb2f40cfdfb37d1c08867c121c617beb8f9f53ad2583855ae2218a07eb9fd89`.

- Target: `firmware/avr/TonePlayer.h:19 [UTF8-LF sha256=03ede515e478fd936c0414a9332b50cb9dd07c540a22a11738317e18220da1f3; anchor=bool enqueue]`.
- Target: `firmware/avr/TonePlayer.cpp:69 [UTF8-LF sha256=ca7d0f91005fed47fa5410e2109f7a667a86fc2d56652ea8f08a1243f02a224f; anchor=startsAt = stepEndsAt_]`.
- Retained: Timer1 OC1A PB1/D9 hardware output without audio ISR; ten-step nonblocking ring queue; silent pauses; begin/stop/mute and active telemetry.
- Retained: PCController already uses Timer1 hardware toggling; that is reuse, not a Pakar invention.
- Omitted/replaced code: `now + step.durationMs; nondelegating default constructor; stale idle active-frequency metadata`. Disposition: **replaced**. Reason: Pakar delegates construction, anchors the next step to the previous deadline, skips fully expired steps, and zeros idle metadata. Register-double tests exercise late calls, wraparound and idle state.
- Omitted/replaced code: `Permissive enqueue of unsupported frequency/zero-duration requests`. Disposition: **replaced**. Reason: Rejecting zero durations and nonzero frequencies outside 20..20000 Hz matches the bounded wire contract and avoids silently scheduling an unplayable request.
- Test-code evidence: `tests/tone_player_test.cpp:20 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.update(start+270)]`.
- Test-code evidence: `tests/tone_player_test.cpp:26 [UTF8-LF sha256=69aba7426639bc88a60e8f6a51cef0ecfbed73c3d4a16ab453d4e1f2d50ae794; anchor=player.setMuted(false)]`.
- Scoped advantage: Tested late-loop and wraparound timing without cumulative melody-length drift; initialized default construction; validation at enqueue; unmute restarts the active tone.
- Remaining: Physical timing, mute/unmute acoustics and supply/clock validity remain issue #2 gates.

### PCController: `Tools/Controller/web/src/card-layout-controls.tsx`

Verdict: **parity-gap**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `de67f2ac9dd8037e354b98227616ed8c207379dbc8205692587bc07a6392be83`.

- Target: `assets/web/device-settings.js:94 [UTF8-LF sha256=ffbe4354eba7347ecd0a80618c9c92b72578e2a67b3e7c5097ce41333f75a2d8; anchor=function panelToggle(]`.
- Target: `docs/INTERFACE-CONTRACT.md:50 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## Known useful parity gaps, not accepted exclusions]`.
- Retained: Actual CardLayoutEditor hidden-card show/reset and CardLayoutFrame hidden/collapsed actions adapted to the embedded workspace Panels dropdown and panel headers. Persisted preferences skip identical writes; lost capability overrides visibility.
- Omitted/replaced code: `editing/onToggleEditing and pointer-capture onReorderStart/Move/End/onLostPointerCapture; order/CSSProperties; keyboardReorderOffset dependency`. Disposition: **pending**. Reason: Show/hide/collapse/reset does not restore panel-order editing. Read actual pointer-reorder.ts completely; its stable-target and RTL keyboard helper need compatible per-container order, focus, persistence and stationary-pointer tests. Existing cell-order editing is a separate contract.
- Omitted/replaced code: `CardLayoutCopy localization and Lucide Check/ChevronDown/Eye/EyeOff/GripVertical/RotateCcw toolbar glyphs`. Disposition: **pending**. Reason: Standalone embedded controls preserve labels and actions but not the entire localized vector-icon toolbar. Bind the established controller icon catalog and localization rather than loading the React/Lucide runtime or claiming glyph parity.
- Omitted/replaced code: `React generic component props and ReactPointerEvent/CSSProperties wrappers`. Disposition: **replaced**. Reason: The ESP serves a standalone embedded document, not the donor React runtime. Existing live DOM cards retain their handlers when settings cards are reparented; no duplicate React state is introduced.
- Test-code evidence: `tests/interface_settings_test.mjs:43 [UTF8-LF sha256=e2588bf230a64a1260bba5acd7ad3d2f26c8f16361e07185a1059cd210ef122e; anchor=Capability loss must override stored panel visibility]`.
- Remaining: Restore edit-mode panel ordering, actual icon/localization contract and pointer/keyboard tests; source/HTTP doubles and simulated browser proof are not physical acceptance.

### PCController: `Tools/Controller/web/src/peripheral-availability.test.ts`

Verdict: **parity-gap**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `a5d7366279057e0868478846c6bbe92eda4209a6bdbcbc16e1c149e57dfe035e`.

- Target: `tests/web_context_test.mjs:22 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=const manifest=]`.
- Target: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Target: `desktop/src/capabilities.rs:202 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=fn serial_manifest_is_individual_and_table_modes_are_profile_scoped]`.
- Retained: Actual disconnected, missing status and capability-free cases adapted into shipped-Web tests. Independent peripheral ACKs, expiry, cross-kind and malformed geometry/schema cases added. C++ tests execute actual HELLO/display-ACK policy.
- Omitted/replaced code: `Vitest/liveSnapshot/emptySnapshot fixture ownership`. Disposition: **replaced**. Reason: Node VM executes embedded Web code, C++ executes firmware policy, and Rust has USB adapter tests. React/Vitest would target a nonexistent React owner in this embedded surface.
- Omitted/replaced code: `Valid/invalid INA219, temperature, PWM, LCD, Bluetooth, relay/RF/statusLED fixture cases`. Disposition: **pending**. Reason: Restore exact bounded-invalid-value cases with the genuine PCController adapter; those peripherals are currently absent, not replaced by synthetic zero fixtures.
- Test-code evidence: `tests/web_context_test.mjs:48 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=Unsupported geometry is not forced]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Rust tests remain cache-budget blocked; generic adapter sensor/profile tests and physical/native acceptance remain #10.

### PCController: `Tools/Controller/web/src/peripheral-availability.ts`

Verdict: **parity-gap**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `fcf2e397564c843711fd64db2bd2d820552180ac4f5abdf0d44ccbbb86be7a84`.

- Target: `desktop/src/capabilities.rs:40 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=pub fn has(]`.
- Target: `libraries/BoardCore/src/DeviceContext.h:26 [UTF8-LF sha256=ce571a112f7d06dc25cdf7f35a383ba4f23c3ca4337ac1bb9968ef17dec72689; anchor=inline uint16_t features(]`.
- Target: `firmware/esp8266/src/WebUi.h:319 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=function peerAvailability(]`.
- Retained: Actual connected AND have_status AND advertised-capability conjunction adapted to selected-peer fresh manifests. No feature inference from hostname, pixel count, catalog, defaults or online alone.
- Retained: TM1637 additionally requires fresh measured ACK; Text and Buzzer have independent HELLO support. Native/Web hide unrelated controls and gate domain commands.
- Omitted/replaced code: `Snapshot/status/hello fields and compact status flag fallback`. Disposition: **replaced**. Reason: Pakar JSON and append-only USB share a schema-1 normalized manifest. AVR HELLO and display-status ACK are aggregated on ESP; copying PCController bit values would alias different protocols.
- Omitted/replaced code: `bounded, ina219, pwm, temperatureLED, temperatureBTAudio, lcd, bluetoothAudio, relays, rf, statusLED and invalid measurement fields`. Disposition: **pending**. Reason: Those drivers are absent on the current pair. A real PCController adapter must preserve their detection, bounds and invalid-value distinctions later. No fabricated controls are displayed.
- Test-code evidence: `tests/web_context_test.mjs:24 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=No inferred board kind or controls]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Native execution and generic PCController profile/discovery adapter remain #10/#6 work; source/C++/Web proof is not native or physical acceptance.

### Pealayer: `.github/dependabot.yml`

Verdict: **scoped-improvement**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `fa4791635251ad7d5be26ce70d71a8e59aa7b814a4f09f2b5b749d8a49908225`.

- Target: `.github/dependabot.yml:1 [UTF8-LF sha256=136887ba77b56544d0bcd367a31d2daaa7637ecdb8b997d63df6cb552d6f08e8; anchor=version: 2]`.
- Retained: Actual donor weekly grouped github-actions and Cargo updates, with Cargo path adapted to the existing desktop workspace.
- Omitted/replaced code: `npm updates at /web_ui`. Disposition: **not-applicable**. Reason: Pakar has no /web_ui/package.json or Vite runtime. Embedded Web/tests use Node built-ins; Flutter's actual /mobile Pub dependency contract is covered instead.
- Test-code evidence: `tests/repository_governance_test.mjs:52 [UTF8-LF sha256=c758bc9f48a363b61e7798be987673ecb06c2460a7bbdf19717e471a2a2334c1; anchor=console.log('Repository governance]`.
- Scoped advantage: Adds grouped Pub updates for the actual Android client and label routing while retaining weekly grouped Actions/Cargo updates.

### Pealayer: `.github/workflows/ci-cd.yml`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `4efb85988474d177a90ea1e31320e64bab1b027c42c3c5ba842fed9781b4eb5b`.

- Target: `.github/workflows/build.yml:2 [UTF8-LF sha256=8cd07cef477916c2cf052cc30d01c03a3860734155acd94cefdca366f5f1a39c; anchor=name: "🚀 Build · Test · Release"]`.
- Target: `tools/ci-checks.mjs:177 [UTF8-LF sha256=9e6028a3ad338cc6bb631a216204cfd3b5480c149188ed099152fc48768e49e6; anchor=export function runChecks]`.
- Target: `tools/prepare-release.mjs:16 [UTF8-LF sha256=96c3a9f5fbc5dfd055a1b139b09271d92d19beaf9cc1cda3c83a88f266ee177e; anchor=export function firmwareManifest]`.
- Retained: Actual donor branch/PR/tag/dispatch/merge-group triggers, concurrency policy, pinned actions without checkout credentials, health-before-build jobs, timeouts, Rust cache reuse, locked tests/builds, packaging, curated notes, manifest/checksums/provenance and aggregate results adapted to current Windows/ESP/AVR/Android targets.
- Retained: Release content is gated on all builds, created as an immutable draft prerelease, with two correctly matched board pairs and raw HTTPS update manifests. Untrusted PRs do not execute publishing code with a write token.
- Omitted/replaced code: `LIBMPV_WINDOWS_RELEASES_API_URL, MPV_DLL_PATH, Windows GNU libmpv linking, Linux/macOS mpv dependencies and standalone pealayer-downloader`. Disposition: **not-applicable**. Reason: Pakar deliberately excludes the media runtime and standalone media utility. Copying this setup would introduce forbidden mpv and unrelated package dependencies.
- Omitted/replaced code: `macOS Intel/Apple silicon and Linux native platform matrix/packages`. Disposition: **pending**. Reason: Current accepted native target is Windows; firmware and Android use Linux runners. Additional native targets need actual platform build/runtime tests rather than claiming donor's unrelated acceptance.
- Omitted/replaced code: `UPX checksum-locked packing and packed native package smoke/resource checks`. Disposition: **pending**. Reason: Existing Windows wrappers preserve donor resource/donor/cache gates and CLI runtime tests, but Pakar has no accepted compressed native package probe. Do not call a source or CLI test full packed GUI runtime acceptance.
- Omitted/replaced code: `branch-tag and archive suffix creation, softprops update-existing release`. Disposition: **replaced**. Reason: User requests stable names without random/time suffixes. Version releases keep stable package names; release enumeration must succeed and an existing release is not overwritten.
- Test-code evidence: `tests/repository_governance_test.mjs:52 [UTF8-LF sha256=c758bc9f48a363b61e7798be987673ecb06c2460a7bbdf19717e471a2a2334c1; anchor=console.log('Repository governance]`.
- Remaining: Restore useful additional native targets and optional accepted compressed package/runtime verification; first execution of the new CI/publisher and physical acceptance remain separate.

### Pealayer: `.github/workflows/codeql.yml`

Verdict: **equivalent-behavior**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `fc3c298cf966d77f50b575614b4c26f24f1606459f07749a46880cd482f1cb53`.

- Target: `.github/workflows/codeql.yml:2 [UTF8-LF sha256=f5d8d998dba7c035ed67184dd3d4a45db41cf54828683648096eb0ed096fe653; anchor=name: "🛡️ Security · CodeQL"]`.
- Retained: Actual donor Actions and JavaScript matrix, no-build security-extended queries, pinned checkout/CodeQL actions, no persisted checkout credentials, PR/main/schedule/dispatch/merge-group triggers, cancellation, timeouts and fail-closed aggregate summary.
- Omitted/replaced code: `Pealayer React/Vite names, packages:read permission and branded prose`. Disposition: **replaced**. Reason: Pakar analyzes embedded Web/scripts and Actions, not Pealayer React. Package read permission is unnecessary for these none-build scans; Rust/C++ coverage is explicitly not invented.
- Test-code evidence: `tests/repository_governance_test.mjs:52 [UTF8-LF sha256=c758bc9f48a363b61e7798be987673ecb06c2460a7bbdf19717e471a2a2334c1; anchor=console.log('Repository governance]`.

### Pealayer: `assets/themes/palettes.json`

Verdict: **equivalent-file**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501`.

- Target: `assets/themes/palettes.json:2 [UTF8-LF sha256=11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501; anchor="studio"]`.
- Retained: Byte-identical Studio/Native light/dark JSON catalog, all fourteen semantic roles.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/ui/palette.rs:71 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn imported_palettes_have_every_required_role()]`.
- Test-code evidence: `tests/web_appearance_test.mjs:54 [UTF8-LF sha256=e634efa751b1fad2d351897adceccead42bda6978c2e43e71de6d63f936ef79c; anchor=embedded catalog exactly matches]`.
- Remaining: Embedded Web now consumes all four actual palettes and fourteen roles. Mobile catalog use and native/Web shared preference synchronization remain pending; byte-identical asset retention is not all-surface parity.

### Pealayer: `build.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `68fb1bdd9e7c7523ab303e37001d3a5252dcb03abe85e489432fe913be7b2e0f`.

- Target: `desktop/build.rs:11 [UTF8-LF sha256=02a9422df8169a9f9795df83c0a990d07a2f74bf2dff314828a3c402f816f3bd; anchor=brand_image::native_icon_bytes(brand_image::icon_bytes())]`.
- Target: `docs/BRANDING-PARITY.md:40 [UTF8-LF sha256=b9b3c92027c004ae46735034876e30bca688e9ec66d8391bfef98d25be7439e7; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Application PE/window/tray master derivatives are separated from actual semantic quick-action resource generation; master/helper changes trigger rebuild.
- Omitted/replaced code: `emit_build_metadata; APPLICATION_BRAND JSON/APP_* precedence/filename validation; version/InternalName/OriginalFilename/copyright/provenance fields`. Disposition: **pending**. Reason: Fixed current identity exists; validated external branding and full generic metadata/provenance remain useful imports.
- Omitted/replaced code: `macOS brew/mpv probing and rpath flags`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free; media linker dependencies violate that scope.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=91f8491b1cd941c2af78f621ab69658397d372959fb3c25a9dce913f62405bc9; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `docs/releases/next.md`

Verdict: **equivalent-behavior**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `71f81fa503d0494a85dd700e16cabdb23e1a6dcd6abbb0abc1af47000584b3f7`.

- Target: `docs/releases/next.md:1 [UTF8-LF sha256=cecaadeb61a7daaec709130503f49a0bb98cde4408af69bc6abe5837b7756fcd; anchor=# PakarController development packages]`.
- Retained: Actual donor curated preview notes explicitly distinguish a development milestone from stable-release, physical and platform acceptance.
- Omitted/replaced code: `Pealayer media/audio/timeline milestone inventory and archived media screenshot evolution links`. Disposition: **replaced**. Reason: These are donor-specific release facts, not transferable Pakar features. Record actual controller/interface packages and link Pakar issue #2 instead.
- Test-code evidence: `tests/repository_governance_test.mjs:52 [UTF8-LF sha256=c758bc9f48a363b61e7798be987673ecb06c2460a7bbdf19717e471a2a2334c1; anchor=console.log('Repository governance]`.

### Pealayer: `docs/verification/LOCAL-WORKSPACE-HYGIENE.md`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `acfcfccde3647841e3c55430fcc36677501659e8ff8fb1761c25391a0c6bad1d`.

- Target: `docs/REUSE-PREFLIGHT.md:42 [UTF8-LF sha256=13521024e5b18f314abcc71af4e9c722262a7cb04c17f03155b1e88d0cf0972e; anchor=Mandatory sequence on every task]`.
- Retained: Actual canonical-cache/stable-staging/preservation/blocked-operation and separate runtime-acceptance guidance read and adopted in mandatory continuation policy and tooling.
- Retained: Reread new shared-cache rejection/hash-copy/storage-inventory guidance at d3f11b1; actual read-only directory summary and equality helper now imported. Ownership and blocked-operation rules still apply.
- Omitted/replaced code: `Historical Pealayer paths, exact relocation/cleanup/deployment/media-clock events`. Disposition: **not-applicable**. Reason: These are donor historical facts, not Pakar installations. Copying them would misrepresent ownership or completed cleanup.
- Omitted/replaced code: `Same-volume rename/hardlink-aware relocation and reusable host profile/peer updater workflow`. Disposition: **pending**. Reason: These are useful generic mechanisms but no Pakar cache relocation or complete updater deployment is implemented by this policy. Current blocked cleanup stays blocked; no rerouting through another shell.
- Omitted/replaced code: `Mandatory donor lookup was previously absent from executable build/stage/capture paths`. Disposition: **replaced**. Reason: Capability lookup with source/target/test hash gates now runs before those actions; other task surfaces are required by AGENTS.md, not falsely claimed universally sandbox-enforced.
- Omitted/replaced code: `New source-checkout cache prohibition and explicit guarded target prune`. Disposition: **pending**. Reason: Canonical cache relocation requires an owner-preserving migration without duplication. Existing cleanup was blocked and cannot be rerouted through this helper. No reclaim or reduced physical SSD wear is claimed.
- Remaining: Complete applicable generic host/relocation/updater tooling after ownership checks; extend executable gates when additional capability mappings are fully reviewed.

### Pealayer: `README.md`

Verdict: **scoped-improvement**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `4e78a6c486bfc7d78adc6d34552bddfd67c89997edb8d2bae0b46e70c705dcb6`.

- Target: `README.md:7 [UTF8-LF sha256=badf979fd797d25a90ca6538ba49339d27a3af0cf6cb929ae588f9ad1ea13c4e; anchor=Connected boards. One contextual control workspace.]`.
- Target: `tools/generate-readme-artwork.mjs:1 [UTF8-LF sha256=a167da336c6133ff59afcbbf22bb947b8a0e66e658773ca051f7d5ff8e4b97d4; anchor=README uses the same bytes]`.
- Retained: Pealayer's actual centered logo/title/tagline, badge row, visual preview, section navigation, architecture, build, docs, contribution and license structure adapted to Pakar.
- Omitted/replaced code: `Media engine, NLE cue/timeline walkthroughs, playback APIs/shortcuts and libmpv installation sections`. Disposition: **not-applicable**. Reason: These describe Pealayer media business/runtime rather than Pakar board control. README links actual controller APIs, wiring and release/acceptance instead; no mpv is linked.
- Omitted/replaced code: `Donor platform and latest-release badges, cinema-grade completion language and media screenshots`. Disposition: **replaced**. Reason: Unsupported OS/release claims and unrelated images would misrepresent this repo. Use the shared application logo and real embedded Web screenshot labeled simulated, explicit PR/source and physical acceptance boundaries.
- Test-code evidence: `tests/repository_governance_test.mjs:52 [UTF8-LF sha256=c758bc9f48a363b61e7798be987673ecb06c2460a7bbdf19717e471a2a2334c1; anchor=console.log('Repository governance]`.
- Scoped advantage: Canonical cross-interface icon bytes and explicit proof/acceptance boundaries prevent a polished landing page from claiming an unaccepted release.

### Pealayer: `scripts/capture-visible-window-windows.ps1`

Verdict: **scoped-improvement**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `2876c19687b1a209e777f3d505e4b24b6c4b1403a0f1de99edcedf0a624d9527`.

- Target: `tools/capture-window-windows.ps1:62 [UTF8-LF sha256=e6892b3c4da4f6a5bd90d6275f868b041516d8bebecb03de23dccaad94e60135; anchor=public static extern bool PrintWindow]`.
- Retained: Actual donor PrintWindow P/Invoke, minimized/window-bounds validation, DPI awareness, System.Drawing PNG lifecycle and read-only behavior.
- Omitted/replaced code: `Get-Process -Name foreground filter`. Disposition: **replaced**. Reason: Explicit expected PID and window-owner revalidation remove ambiguity between production and fixtures; screen fallback checks foreground ownership without changing focus.
- Omitted/replaced code: `RenderOccludedWindow branching and unconditional Bitmap.Save`. Disposition: **replaced**. Reason: Capability-gated egui framebuffer is tried first, then PrintWindow/BitBlt and verified foreground pixels. Blank images and overwrites are rejected.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=b1378631676b871090abb8c83457b4c5c2a1c5dfbfaed7a662045cae2919bb9f; anchor=for(const forbidden]`.
- Scoped advantage: PID-bound, no-overwrite capture with an app-owned framebuffer and automatic read-only Win32 fallback; runtime evidence is separate from source guards.

### Pealayer: `scripts/check-crate-boundary.mjs`

Verdict: **scoped-improvement**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `da9b469ac596e6eb1999e4d499742a51ebf289335285549569116c1124963e38`.

- Target: `tools/check-crate-boundary.mjs:10 [UTF8-LF sha256=4a8fbcecaf424460c051de61d5e44658532ec64527460ce21d4d86ec95c0550e; anchor=export function checkCrateBoundary]`.
- Retained: Actual donor module extraction/intersection, thin executable and MSVC duplicate resource linkage guards imported/adapted. Native build wrapper and CI execute this read-only guard before compiling.
- Omitted/replaced code: `pealayer::startup::run and main.rs/lib.rs module intersection assertions`. Disposition: **replaced**. Reason: Pakar has one binary today. The adapted tool detects optional lib.rs and then enforces zero executable modules, module ownership and delegation to Pakar's library namespace. Today's single binary additionally rejects duplicate declarations without requiring a nonexistent Pealayer startup symbol.
- Omitted/replaced code: `MSVC winres library/bin duplicate resource.lib guard`. Disposition: **replaced**. Reason: Both original resource.lib patterns are retained and run for today's binary as well as a future shared-library layout. No need to defer these guards until the architecture changes.
- Test-code evidence: `tests/crate_boundary_test.mjs:4 [UTF8-LF sha256=ebf1cbc5410a3f67a7b704d17415801e68daeba961253f8b60a183fdb5fec17f; anchor=const single =]`.
- Scoped advantage: Executes useful donor guards immediately on the current single-binary layout, rejects duplicate declarations, and automatically activates library ownership/thin-entry checks when a library appears. No compiler or generated fixture writes.

### Pealayer: `scripts/libmpv-windows.ps1`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `ab6e48f3e0a55a2d86ad1dbb19c01a23c751891cb5a7ed61875a21b1e52645e5`.

- Target: `tools/windows-build-host.ps1:16 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Get-PakarCargoTargetDirectory]`.
- Target: `tools/windows-build-host.ps1:44 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Test-PakarFileContentEqual]`.
- Retained: Actual Get-PealayerWindowsHostPaths, Get-PealayerCargoTargetDirectory and Get-PealayerFileIdentity adapted to Pakar names/locations. Environment expansion and relative overrides retained; native build and staging now call these helpers.
- Retained: New actual Test-PealayerFileContentEqual imported as Test-PakarFileContentEqual and called by staging: missing files fail, size mismatch skips hashing, equal lengths require SHA-256 equality.
- Omitted/replaced code: `Resolve-PealayerLibmpv, Copy-PealayerLibmpvRuntime and Set-PealayerLibmpvBuildEnvironment`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free; importing DLL aliases, GNU/MSVC mpv imports, LIBMPV_DIR and native linker flags would restore an excluded media dependency.
- Omitted/replaced code: `Save-PealayerWindowsHostProfile generic host provenance and generated Cargo-config ownership protection`. Disposition: **pending**. Reason: Saved host/compiler/cache provenance and refusal to overwrite user-managed configuration remain useful. Pakar currently resolves its established cache/explicit override but does not persist this generic host profile.
- Omitted/replaced code: `Program-root shared target default across separate source worktrees`. Disposition: **pending**. Reason: Pakar keeps its existing single canonical desktop/target cache without moving or duplicating it. A future managed-worktree build needs a shared Pakar-owned cache; copying Pealayer's owner path would corrupt ownership.
- Omitted/replaced code: `New Get-PealayerCargoTargetDirectory rejection of all source-checkout cache paths`. Disposition: **pending**. Reason: Pakar's existing sole desktop/target is inside the checkout. Applying this rejection now would strand the warm cache or force a duplicate cold build. An owned same-volume relocation and wrapper/config migration are required first; the oversized-cache guard remains active and blocked cleanup is not bypassed.
- Omitted/replaced code: `Copy-PealayerFileLowWrite delete-destination-first copy and PreferHardLink`. Disposition: **replaced**. Reason: Pakar retains its checksum-verified pending-copy then rename. The prior candidate is preserved until verification; a mutable executable is not hardlinked to compiler output. mpv immutable-runtime aliases are explicitly excluded, not a reason to omit generic content comparison.
- Test-code evidence: `tests/reuse_preflight_test.ps1:73 [UTF8-LF sha256=30a46f7b0625b26115363c9241a4a56020159d8b2e29485459c3a2b034a92ed8; anchor=Actual donor relative cache resolution]`.
- Test-code evidence: `tests/storage_hygiene_test.ps1:15 [UTF8-LF sha256=f8a5d8bfe1728d2643820229ce2c5310148e1e93dc2a727a714fe82c6e5a0f82; anchor=Actual imported equal-content helper]`.
- Remaining: Adapt generic saved host provenance/config ownership checks without mpv, and keep one owned cache if worktrees are later authorized.

### Pealayer: `scripts/package-probe-windows.ps1`

Verdict: **equivalent-file**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f`.

- Target: `tools/package-probe-windows.ps1:1 [UTF8-LF sha256=f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f; anchor=function Invoke-PackageProbe]`.
- Retained: Byte-identical actual donor process-handle probe: no-shell hidden process, async redirected stdout, 30-second wait, explicit exit code, output return and Dispose. Optional staging probe calls this helper.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/reuse_preflight_test.ps1:77 [UTF8-LF sha256=30a46f7b0625b26115363c9241a4a56020159d8b2e29485459c3a2b034a92ed8; anchor=Actual imported short-lived process stdout]`.
- Remaining: Donor timeout throws without terminating its child. Full GUI/runtime package smoke remains separate from CLI identity.

### Pealayer: `scripts/package-windows.ps1`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `3e6f8a82456329558c7ec24e596f353c75d39680d8807d6eb0261bd419e5025a`.

- Target: `tools/windows-build-host.ps1:60 [UTF8-LF sha256=5115289daf89e09489faf340623fa4a1dbaacd97abe4abaf42384f1757def08e; anchor=function Assert-PakarPackageSourceStable]`.
- Target: `tools/stage-desktop.ps1:36 [UTF8-LF sha256=bb1995575486751f88833de244be46b1d6c92727514c2acf81e9e516bccd9b2d; anchor=Invoke-PackageProbe]`.
- Retained: Actual Assert-PackageSourceStable adapted without Web exclusions; optional ExpectedCommit checks run before/after build/stage. Imported bounded Process probe and file-identity helper are used by staging; identical hashes skip payload copies.
- Omitted/replaced code: `libmpv runtime/import-library packaging and pealayer-downloader media companion`. Disposition: **not-applicable**. Reason: The user explicitly requested an mpv-free controller, not the donor media player/downloader. Board remote firmware contracts remain required separately.
- Omitted/replaced code: `Restore-PackageWebDistribution git restore and recursive generated-Web cleanup`. Disposition: **replaced**. Reason: Pakar staging does not generate or mutate a tracked Web distribution. Blind donor restoration could overwrite user work and is not copied.
- Omitted/replaced code: `Branding, Persian font/Web bundle packaging, Win32 resource and quick-action icon verification, UPX validation`. Disposition: **pending**. Reason: These are useful generic package capabilities. Existing app source contains icons/font but current staging does not validate/package all donor facilities.
- Omitted/replaced code: `Embedded --build-info/--smoke-test checks, GITHUB_SHA invalidation and host-manifest artifact/provenance/validation schema`. Disposition: **pending**. Reason: Current app lacks equivalent embedded source identity and full runtime smoke. Optional Git source-stability and CLI-help checks are narrower and cannot certify the old executable's source commit.
- Omitted/replaced code: `New Copy-PealayerFileLowWrite calls for executable/downloader/font and runtime aliases`. Disposition: **replaced**. Reason: Staging now calls the actual imported content-equality helper, skips identical candidates, and keeps verified pending-copy/rename rather than deleting the prior candidate first. No excluded mpv/downloader payload is created. Useful branding/font/Web packaging remains separately pending.
- Omitted/replaced code: `Timestamp package metadata and native optional Run`. Disposition: **pending**. Reason: User excludes dated/random project/package names, not necessarily diagnostic time fields. Preserve useful manifest provenance and bounded explicit launching in a future adapted package pipeline.
- Remaining: Import full mpv-free package manifest, embedded identity, resource/icon/font/Web validation and appropriate generic runner; do not call staging a full package release.

### Pealayer: `scripts/run-windows.ps1`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `013e5c67e227ed851f8a3d26b27654e38c8816edca2965381bbb1b48d10a750d`.

- Target: `tools/build-desktop.ps1:20 [UTF8-LF sha256=48fc7a68747bc747a4b9b0a4ae675097329e20a99ab12f4581b170d8cd2f74d3; anchor=Get-PakarCargoTargetDirectory]`.
- Retained: Shared resolver call and locked native build flow mapped to Pakar wrapper. Existing canonical cache is reused; explicit Check/Test/Release and storage/reuse gates avoid unnecessary links.
- Omitted/replaced code: `RustBin machine/user/toolchain executable fallback selection and rustc -vV host detection`. Disposition: **pending**. Reason: Pakar currently uses installed Cargo from PATH. Alternate host/toolchain executable fallback and recorded host triple are reusable infrastructure and remain unported.
- Omitted/replaced code: `libmpv host resolution/environment/profile and DLL copy`. Disposition: **not-applicable**. Reason: These operations configure the excluded mpv renderer. Pakar uses its own mpv-free linked dependencies.
- Omitted/replaced code: `BuildOnly versus launch forwarding with ApplicationArguments`. Disposition: **pending**. Reason: Pakar wrapper builds only. Generic built-executable validation and configurable run forwarding should become a Pakar runner; manual launching is not equivalent.
- Omitted/replaced code: `Set-Location to repository root`. Disposition: **replaced**. Reason: Absolute manifest and resolved target paths preserve caller cwd without introducing a second cache.
- Test-code evidence: `tests/build_storage_test.ps1:15 [UTF8-LF sha256=ac9af1dddae9cf93afdf7dad889c73e1b4f11e4efcce254d0b621b5218dd47aa; anchor=One canonical target]`.
- Remaining: Import generic Windows runner/toolchain fallback and host-triple provenance without mpv.

### Pealayer: `scripts/storage-hygiene.ps1`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `52f93bf96867b286d7d1c22f36c08603558e10dc6e9c14433e0a9ea9bb5f4f42`.

- Target: `tools/storage-hygiene.ps1:10 [UTF8-LF sha256=bcb4b2b588ccf22a169e050602ac4a5a66d39ab446dd20c212744dbe27378d9a; anchor=function Get-DirectorySummary]`.
- Retained: Actual Get-DirectorySummary imported into tools/storage-hygiene.ps1 with Pakar canonical cache, board cache, mobile build and stable staging paths. Read-only by default and warns at the existing 8 GiB budget; logical sizes explicitly distinguished from disk allocation or wear.
- Omitted/replaced code: `PruneRepositoryTarget exact-child/git-ignore/allowed-child/running-process checks and recursive deletion`. Disposition: **pending**. Reason: Blocked earlier cleanup cannot be rerouted through a new helper. The current cache is still canonical, not an obsolete donor target. Preserve evidence/active owners and resolve the original cleanup/migration boundary first; no destructive entrypoint imported in this pass.
- Omitted/replaced code: `Pealayer target/shared-cache/staging paths and 48 GiB warning`. Disposition: **replaced**. Reason: Pakar uses its actual existing sole cache, Arduino and Flutter products and stable outputs staging, with the established 8 GiB warning. Importing another project's directories would misstate ownership.
- Omitted/replaced code: `Silently ignored recursive read errors and empty Measure-Object Sum`. Disposition: **replaced**. Reason: Unmeasured paths must fail visibly rather than understate storage. Empty directories use zero bytes instead of a missing StrictMode Sum property.
- Test-code evidence: `tests/storage_hygiene_test.ps1:10 [UTF8-LF sha256=f8a5d8bfe1728d2643820229ce2c5310148e1e93dc2a727a714fe82c6e5a0f82; anchor=Actual imported recursive summary]`.
- Test-code evidence: `tests/storage_hygiene_test.ps1:21 [UTF8-LF sha256=f8a5d8bfe1728d2643820229ce2c5310148e1e93dc2a727a714fe82c6e5a0f82; anchor=Read-only import must never become]`.
- Remaining: Resolve owned canonical-cache migration/blocked cleanup without deleting evidence or another process's artifacts; then implement and test any separately authorized cleanup contract.

### Pealayer: `scripts/update-screenshots-windows.ps1`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `48507f6e9395c8c312c5557db729d05ed8d6a83e980d57a9b01e77ea525db287`.

- Target: `tools/capture-window-windows.ps1:62 [UTF8-LF sha256=e6892b3c4da4f6a5bd90d6275f868b041516d8bebecb03de23dccaad94e60135; anchor=public static extern bool PrintWindow]`.
- Retained: Actual PrintWindow(PW_RENDERFULLCONTENT), near-uniform-black sampler, GetWindowDC/BitBlt/screen fallback and PID/dimension validation imported. Pakar fixture supplies isolated profile/selected surfaces; app-owned framebuffer capture does not depend on WGC.
- Omitted/replaced code: `SetForegroundWindow, BringWindowToTop, SetWindowPos, ShowWindow, SetCursorPos, mouse_event, SendMouseWheel/Click/RightClick`. Disposition: **replaced**. Reason: Capture remains read-only and must not steal focus, resize or inject desktop input. Startup CLI sets the application's own surface; native gestures belong to the supported computer-use APIs.
- Omitted/replaced code: `DwmGetWindowAttribute extended bounds and logical/client coordinate transformations`. Disposition: **replaced**. Reason: Per-monitor-v2 physical GetWindowRect is kept consistent with full window-DC capture, avoiding a mismatched crop origin. Internal framebuffer needs no desktop coordinates.
- Omitted/replaced code: `Locale/Theme arrays, PreferencesTab/Icon, AppName/Branding, WorkspaceDockLayout and per-surface matrix manifest`. Disposition: **pending**. Reason: Config-driven Pakar CLI surfaces/isolated profiles exist, but the full multi-locale/theme/icon/width matrix and hashed per-capture manifest runner remain useful unported harness work.
- Omitted/replaced code: `temporary GUID profile cleanup, process CloseMainWindow/Kill and generated_at_utc`. Disposition: **replaced**. Reason: Stable named evidence/profiles are retained for debugging rather than deleted. Only verified fixture processes are stopped; production replacement uses graceful typed IPC and avoids timestamp/suffix deliverables.
- Omitted/replaced code: `native resize/scroll/client-coordinate scenario parameters`. Disposition: **pending**. Reason: Model/painter tests and selected-surface captures do not replace the full interactive narrow/wide scenario matrix. Native live gesture acceptance remains separate.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=b1378631676b871090abb8c83457b4c5c2a1c5dfbfaed7a662045cae2919bb9f; anchor=for(const forbidden]`.
- Remaining: Complete generic locale/theme/icon/width capture matrix and manifest receipts; verify native interaction scenarios.

### Pealayer: `src/application_shortcuts.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `1f173bef8acc2904eac5b0a91407aa4f7629ce65c88ea3a8ac78f8c13321d203`.

- Target: `desktop/src/ui/shell.rs:389 [UTF8-LF sha256=61deb06fb3209fe62743ee11fc824bb301a7fb52356fc17a8a86d361160de47e; anchor=egui::Key::F11]`.
- Retained: Default Preferences Ctrl+, and device configuration Ctrl+Shift+, navigation exist; F11 fullscreen is restored with the shared typed dispatcher. These are not equivalent to the donor persisted rebinding model.
- Omitted/replaced code: `ApplicationShortcuts persisted fields, parse_shortcut, entries, validate, action_for_event and their non-media tests`. Disposition: **pending**. Reason: The foundation hardcoded its accelerators and omitted the reusable binding model. No accepted reason justifies leaving rebinding out. The actual donor depends on KeyChord and hardware_shortcuts match/key-code helpers, which must be ported and tested with controller-specific typed actions rather than guessed labels.
- Omitted/replaced code: `take_frame_step_shortcut and frame-step tests`. Disposition: **not-applicable**. Reason: These consume Ctrl+Left/Right for media frame stepping and timeline transport. Those business domains were explicitly removed; Pakar must not hijack ordinary text word navigation for nonexistent video frames.
- Omitted/replaced code: `containing_media_folder and local-media path tests`. Disposition: **not-applicable**. Reason: This resolves media files/streams, not board identities or firmware files. Firmware selection already uses explicit native file dialogs; introducing a nonexistent media folder action would be misleading.
- Omitted/replaced code: `media_information and media_folder accelerators`. Disposition: **pending**. Reason: The media operations cannot be retained as-is, but their reusable information/folder-navigation affordances need explicit hardware-monitor and application-folder action decisions. They are not silently treated as completed controller mappings.
- Remaining: Restore validated persistent rebinding, exact-modifier matching, repeat/release rejection and text-edit protection; separately reconcile global/hardware shortcut ownership.

### Pealayer: `src/branding.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `be44ca91dfb08ce78fb377ccfcc4676676c05e6b36ed319c497365788ab264fe`.

- Target: `desktop/src/branding.rs:63 [UTF8-LF sha256=28849fc17b8082114c0bc3d9c38baf0aef64b929c7c721aa19bdc048b25905e7; anchor=pub fn icon_data_from_bytes]`.
- Target: `desktop/src/brand_image.rs:25 [UTF8-LF sha256=ab71607dcde16d7af14b0a292982f902186dacf6b5b78b3f4c983e37464428e9; anchor=pub fn native_icon_bytes]`.
- Target: `docs/BRANDING-PARITY.md:40 [UTF8-LF sha256=b9b3c92027c004ae46735034876e30bca688e9ec66d8391bfef98d25be7439e7; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `desktop/src/branding.rs:8 [UTF8-LF sha256=28849fc17b8082114c0bc3d9c38baf0aef64b929c7c721aa19bdc048b25905e7; anchor=pub fn sync_peer]`.
- Retained: Actual icon_data_from_bytes imported unchanged; one embedded PNG supplies native/Web artwork without disk caches.
- Retained: Actual native_icon_bytes seven-frame ICO encoding/master preservation/Lanczos3 resampling and shell derivative test adapted to shared supplied bytes and PNG-only decoder.
- Retained: Actual sync_native_window_icon source-signature cache, multi-viewport icon publication and tray refresh lifecycle adapted to fresh selected-peer Application metadata; no per-frame decode or disk cache.
- Omitted/replaced code: `resolved_icon_path/state_env/non_empty_path, builtin presets, icon_bytes precedence/fallback and override/preset tests`. Disposition: **pending**. Reason: Useful runtime preferences need a bounded shared board/client transport; local filesystem media-state paths cannot be copied blindly to ESP.
- Omitted/replaced code: `sync_native_window_icon, icon_source_signature/revision metadata/multi-viewport/tray sync and tests`. Disposition: **replaced**. Reason: sync_peer now adapts the cache/multi-viewport lifecycle to peer Application metadata, shared header/About and tray artwork/Show label. Native runtime acceptance is still blocked by the cache guard; full icon preferences/tests remain open.
- Omitted/replaced code: `PlaybackIconState/from_player/playback_state_is_unambiguous`. Disposition: **not-applicable**. Reason: Video state business expressly removed; hardware state icons require a separate validated state contract.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=91f8491b1cd941c2af78f621ab69658397d372959fb3c25a9dce913f62405bc9; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=99621b44a961771053066590ce43759c24bdd2c0993de0d7c36ae615a2c5a2ad; anchor=Actual Web application identity]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `src/messaging.rs`

Verdict: **equivalent-file**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573`.

- Target: `desktop/src/messaging.rs:81 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=pub fn publish(]`.
- Retained: Byte-identical MessageHub, validation, bounded queue, expiry/upsert/dismiss/revision/source identity, and both donor tests.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/messaging.rs:171 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=fn upsert_dismiss_and_expire_are_shared()]`.
- Remaining: This file equivalence does not establish Web/TUI/IPC notification-surface parity.

### Pealayer: `src/platform/windows_quick_actions.rs`

Verdict: **equivalent-behavior**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `35f6de708a0e402f6958a4d8feeead5037b42174286050ddc98c494d4173e4cb`.

- Target: `desktop/src/platform/windows_quick_actions.rs:15 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=pub const WINDOWS_QUICK_ACTIONS]`.
- Retained: Actual typed action/resource catalog, equality traits, explicit negative resource-index selection, Preferences, Exit and generic Toggle fullscreen. Controller actions share the launch/IPC dispatcher and resource compiler.
- Omitted/replaced code: `Play / Pause, Previous chapter, Next chapter, Mute / Unmute media quick actions`. Disposition: **not-applicable**. Reason: Pakar intentionally has no mpv playback, chapters or media volume. These would call removed media business methods. Connect, device settings, refresh, workspaces and About are real controller navigation actions instead; generic fullscreen is retained, not misclassified as media-only.
- Test-code evidence: `desktop/src/platform/windows_quick_actions.rs:69 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=fn controller_shell_actions_have_unique_native_resource_ids]`.
- Remaining: Visible Windows Jump List clicks remain a manual acceptance gate; typed launch forwarding and resource uniqueness are tested.

### Pealayer: `src/platform/windows_shell_icons.rs`

Verdict: **equivalent-file**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a`.

- Target: `desktop/src/platform/windows_shell_icons.rs:45 [UTF8-LF sha256=e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a; anchor=pub fn shell_icon_ico]`.
- Retained: Byte-identical actual Phosphor rasterizer, centered glyph coverage, all seven ICO resolutions, bottom-up BGRA, DWORD-aligned AND masks and explicit directory offsets. Pakar build.rs keeps it for semantic quick-action assets; application PE/window/tray artwork now uses the shared master and actual Pealayer branding conversion.
- No production-code omission claimed for this reviewed file.
- Remaining: User-configurable icon packs and runtime state-icon transitions are a separate unresolved integration; retaining the generator does not implement that editor.

### Pealayer: `src/ui/board_info.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `c252864d80912a03bca718b0a13d721fda2e8f7378c033f44619590a82380fd2`.

- Target: `desktop/src/capabilities.rs:7 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=pub struct Manifest]`.
- Target: `desktop/src/ui/shell.rs:275 [UTF8-LF sha256=61deb06fb3209fe62743ee11fc824bb301a7fb52356fc17a8a86d361160de47e; anchor=fn synchronize_context(]`.
- Target: `desktop/src/ui/device_settings.rs:20 [UTF8-LF sha256=109f5c93bfd2b6c35ad8b160bc2adc6a785c4bd098a61074a61d1988931451b5; anchor=pub(crate) fn available(]`.
- Target: `docs/DEVICE-CONTEXT.md:71 [UTF8-LF sha256=13429955cd7e98ba80de7a2394e46fd3e5c973b6ca0e559b5e2aee639126d277; anchor=## Exact reuse and remaining useful code]`.
- Retained: Reread all 1280 current lines through EOF. Prior authoritative overview/capability/front-panel adaptation remains; new dropdown dependency is explicitly pending.
- Retained: Numeric display controls are independent of Text; peer title follows acknowledged name and lost capabilities cancel Table work.
- Omitted/replaced code: `TABS/draw persistence; overview typed board/profile/protocol/build/EEPROM identity; optional_row/yes_no`. Disposition: **pending**. Reason: Existing settings/dialog owners retained; complete typed PCController identity/profile/readback and persistence/localization tests require a real adapter. Current manifest is not that universal adapter.
- Omitted/replaced code: `settings/settings_grid/setting_control/checkbox_setting/slider_setting/enum_combo/bit_checkbox/relay_mask_editor`. Disposition: **pending**. Reason: Full EEPROM draft/write/readback, menu/reset/reboot and relay/PWM/RF/LCD/macro editors remain useful pending contracts. Current driver absence hides them but does not certify generic helpers imported.
- Omitted/replaced code: `capability_badges/capability_column_count/capability_pill/state_badge/unavailable and responsive tests`. Disposition: **pending**. Reason: Availability semantics adapted; entire donor badge geometry, localized empty states and responsive/keyboard consumer tests remain unverified.
- Omitted/replaced code: `FRONT_PANEL_KEYS/key_icon fixed K1-K4 previous/next/decrease/increase`. Disposition: **not-applicable**. Reason: Our board has nine shift-register/ADC keys; four hardcoded identities would be false. Matching future adapters can advertise them.
- Omitted/replaced code: `front_panel_key_button and acknowledged menu/key/layout readback`. Disposition: **pending**. Reason: Nine-key state exists, but full remotely navigable front-panel/menu/key commands need per-board descriptors and donor ACK tests. Incompatible labels do not justify dropping reusable command helpers.
- Omitted/replaced code: `seven_segment_preview, section/row drawing`. Disposition: **replaced**. Reason: Existing Pakar seven_segment/dialog primitives own preview and geometry. A duplicate renderer would fork styling; fresh peer gates wrap the current owners. Native paint acceptance remains pending.
- Omitted/replaced code: `DropdownUiExt / dropdown::ComboBox board enum selector integration`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `tests/web_context_test.mjs:57 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=Native wiring is source-checked only]`.
- Test-code evidence: `desktop/src/capabilities.rs:179 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=fn no_profile_or_individual_ack_means_no_controls]`.
- Remaining: Typed PCController adapter, donor tests and native runtime proof remain #10/#3/#6. No whole-file equivalence or application superiority claimed.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/dialog.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `4ffdd81afbaa2f918cb572f11d1ec978b279ca0e263f9bfad83e99a59bc27781`.

- Target: `desktop/src/ui/cell_table.rs:349 [UTF8-LF sha256=031e1161d656673af02349c80e3c615a7a9ccd280258d428cccd69439b78bb1b; anchor=fn manager_selector(]`.
- Target: `firmware/esp8266/src/WebUi.h:46 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=/* Fixed manager slots:]`.
- Retained: Read all 952 donor lines through EOF. navigation_control's fixed allocation, clipped centered text and inside-stroke painting are adapted to manager_selector. Compact cell/row/state slots use the existing shared neutral animated cell_surface; state-dependent Button Frames cannot resize their text/content geometry. Selected widget metadata is retained explicitly.
- Omitted/replaced code: `navigation_control's 80px minimum, left-aligned icon/title/metadata slots and selected bg_fill tint`. Disposition: **replaced**. Reason: A compact cell needs a centered number in 32x28; row/action slots are 96/104x28. The user requires neutral shared selection, not a tinted navigation side rail. Fixed text positions and inside strokes are preserved; existing side-rail helper remains unchanged.
- Omitted/replaced code: `singleline_text_edit/workspace_overlay_layer/preferred_host/centered_default_rect/bounded_geometry/opaque frame/action buttons/action rows/navigation detail/section/compact_row/setting_row/scroll_column/fit_scroll and their donor tests`. Disposition: **pending**. Reason: Existing imported dialog helpers are present, but this scoped geometry fix does not certify each call site or host/resize/action/localization test. Useful generic code is not excluded; full consumer and test reconciliation remains issue #3 work.
- Omitted/replaced code: `numeric_stepper/numeric_adjustment/adjust_numeric/numeric_slider_wheel/parse_numeric_paste/receive_numeric_paste/numeric_context_menu and seven pointer/paste/wheel tests`. Disposition: **pending**. Reason: Helpers already exist in the imported module; bounded input geometry, paste ownership, wheel tail suppression and context behavior need their actual donor runtime tests restored and controller settings coverage. Hover selector acceptance is not numeric-control parity.
- Test-code evidence: `desktop/src/ui/cell_table.rs:1123 [UTF8-LF sha256=031e1161d656673af02349c80e3c615a7a9ccd280258d428cccd69439b78bb1b; anchor=fn manager_hit_slots_text_and_wrap_never_follow_hover_or_stroke_width()]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:11 [UTF8-LF sha256=87f848f35d195bdf8bd6782cc90c0c5e786a734f49ea9415c779d3f49a1f2971; anchor=Manager must not allocate state-dependent selectable Frames]`.
- Remaining: Execute the added actual native painter test at narrow/wide widths with animation on/off, then capture/deploy through guarded workflows. Native Test is currently blocked by the canonical >8 GiB cache; Web checks are source/DOM-double evidence, not browser pixels or physical deployment.
- Remaining: Complete the named generic dialog call-site/test reconciliation in issue #3; no whole-file behavioral equivalence or whole-project superiority claim.

### Pealayer: `src/ui/i18n.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `811f1e08e7126932772c49da8aa9669ce8f47d4e2208c1ef6cde435dde20bdf4`.

- Target: `desktop/src/ui/i18n.rs:6 [UTF8-LF sha256=9587bca5bd62d4e8827a8a4ccaabc81e5dd69289ce552e6f3929fd386e8c6ff8; anchor=pub(crate) fn configure_ui_fonts]`.
- Target: `desktop/src/ui/dialog.rs:18 [UTF8-LF sha256=b3786964dd0c375f0f73325a4046b3fc0e3967ace4e7e0c4f66b6f52d66eda47; anchor=pub fn singleline_text_edit]`.
- Target: `desktop/src/ui/preferences.rs:96 [UTF8-LF sha256=a0b8c1a2ba2857733245c7e76c0f2d1be3ff857a3691df95440913e5639908e6; anchor=AppLanguage::Persian]`.
- Target: `docs/PERSIAN-PARITY.md:21 [UTF8-LF sha256=9e5e153c3872e2347365af8d60762ed971fb41f77210d1a6d6ff2306400e3864; anchor=## Exact source-to-target mapping]`.
- Retained: Actual donor platform/UI face loading, Vazirmatn and Phosphor registration, application dictionary, identity-preserving visual_text, script detection, input alignment and horizontal/vertical direction helpers retained and wired into real native controls.
- Retained: Monospace Persian fallback and preference-driven priority refresh added without discarding later widget font registrations. Actual donor contextual glyph test adapted to both families; saved Unicode and peer identifiers never manually reversed.
- Omitted/replaced code: `Media playback, cue/timeline reveal and keyframe-specific dictionary keys`. Disposition: **not-applicable**. Reason: They describe explicitly excluded media business; no corresponding controller surface exists. No generic UI/tooling capability is excluded on that basis.
- Omitted/replaced code: `Manage folders, Manage folder, New folder, Expand folders, Collapse folders, Move to folder, Delete folder, Confirm delete, Channels will be moved to Ungrouped, pending folder change and attached-board folder configuration copy`. Disposition: **pending**. Reason: Generic grouping behavior is useful but not yet integrated with actual board cell/group contracts. No fake folder actions or completion claim; retain on issue #4 for real grouping and localized acceptance.
- Omitted/replaced code: `Separate ttf_parser coverage assertions and exhaustive native surface translations`. Disposition: **pending**. Reason: Avoid introducing a duplicate font parser dependency: actual renderer glyph/contextual tests are adapted instead, but unexecuted native tests and additional Unicode/surface acceptance remain gaps. Full device-settings bodies and status/transport copy still need audit.
- Test-code evidence: `desktop/src/ui/i18n.rs:1434 [UTF8-LF sha256=9587bca5bd62d4e8827a8a4ccaabc81e5dd69289ce552e6f3929fd386e8c6ff8; anchor=fn contextual_persian_in_labels_and_monospace_inputs]`.
- Test-code evidence: `desktop/src/ui/i18n.rs:1396 [UTF8-LF sha256=9587bca5bd62d4e8827a8a4ccaabc81e5dd69289ce552e6f3929fd386e8c6ff8; anchor=fn changing_language_preserves_widget_fonts]`.
- Test-code evidence: `tests/localization_test.mjs:40 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Peer text identical to a dictionary key]`.
- Remaining: Run native glyph/font-retention/locale tests after approved obsolete-debug cleanup; inspect actual Persian native inputs/menus/dialogs and replacement app. Complete all application-copy surfaces and generic folder/group behavior.

### Pealayer: `src/ui/icons.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `fba85edd503798868acc5527600b2ee52326339816c5b73e9926d1d7b0929384`.

- Target: `desktop/src/ui/icons.rs:567 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=pub fn searchable_workspace_icon_picker(]`.
- Target: `desktop/src/ui/shell.rs:1787 [UTF8-LF sha256=61deb06fb3209fe62743ee11fc824bb301a7fb52356fc17a8a86d361160de47e; anchor=icons::searchable_workspace_icon_picker]`.
- Retained: Production Phosphor vocabulary, stable names/kind fallback, searchable list/grid picker, UTF-8-safe match highlighting, bounded popups, disclosure/submenu helpers.
- Retained: Actual donor regression tests restored: eleven tests, including interactive egui-frame search/select/reopen, both themes and popup width.
- Omitted/replaced code: `every_native_icon_editor_uses_the_shared_searchable_picker donor-only editor path list`. Disposition: **replaced**. Reason: That assertion references Pealayer media/4D editor files not present in this product. Replaced only its call-site assertion with Pakar's actual workspace editor; the other ten tests were imported. This does not excuse missing generic editor features elsewhere.
- Omitted/replaced code: `DropdownUiExt, dropdown_choice, register_choice and dropdown::ComboBox in searchable icon pickers`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `desktop/src/ui/icons.rs:825 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=fn pakar_workspace_editor_uses_the_actual_shared_searchable_picker()]`.
- Test-code evidence: `desktop/src/ui/icons.rs:954 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=fn icon_picker_search_click_filter_select_and_reopen_work_in_both_forms()]`.
- Remaining: Imported compact/control picker helpers are not all used by current panels; UI-wide application/action parity needs separate file reviews.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/menu.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `18e255a5ccc65dfa0588eac10283c7c1f5fe33ea63bb3e07bd599e30a0cf19c3`.

- Target: `desktop/src/ui/menu.rs:9 [UTF8-LF sha256=199cebacdf180d88cb724d458ee096ffd29555ec1a16df9d47c28799a5c46cff; anchor=pub fn top_menu_button<R>(]`.
- Target: `desktop/src/ui/shell.rs:445 [UTF8-LF sha256=61deb06fb3209fe62743ee11fc824bb301a7fb52356fc17a8a86d361160de47e; anchor=menu::top_menu_button(ui, crate::ui::i18n::text("File")]`.
- Target: `desktop/src/ui/connection_controls.rs:38 [UTF8-LF sha256=3216d5dde3a9bb6a8386cec0986140fdc2da5e1130c0cab17172d728b214fb76; anchor=pub fn websocket(]`.
- Target: `docs/NATIVE-MENU-COMPARISON.md:32 [UTF8-LF sha256=a86271d68a6553075026ad4e1ffef59c44f87074401b7c2936d9cd539d032fb9; anchor=## Exact donor menu mapping and remaining decisions]`.
- Retained: Actual top_menu_button body imported with only application state ID/public visibility adaptation; all four production menu headings use active-menu sibling hover switching.
- Retained: Actual Button::shortcut_text idiom replaces space-padded primary labels in all six shortcut rows; separate right-aligned muted atoms are supplied by the same egui 0.36.2 dependency. Existing dispatch/Phosphor submenu retained; user-requested height policy unchanged.
- Retained: Actual connection selector/persist-on-change and custom OS hardware-path fallback adapted to independent WebSocket and USB popup controls. Lazy factual enumeration, reconnect/disconnect and route actions added; complete donor lifecycle remains pending.
- Omitted/replaced code: `TrackMenuState, track_menu_state, format_track_label; video/recent/close/media info/folder/timeline and Audio/Subtitles/Chapters blocks; track_menu_is_contextual_to_media_and_discovered_tracks and test_format_track_label_variations`. Disposition: **not-applicable**. Reason: These require media tracks, chapters, video/timeline state or a media undo stack expressly excluded from Pakar. Their disabled Undo/Redo placeholders are not functional controller actions. Generic config-folder/device information actions remain pending separately.
- Omitted/replaced code: `draw persisted application_shortcuts and Edit configuration action`. Disposition: **pending**. Reason: Current fixed accelerators match dispatch but are not the donor's validated persisted rebinding model. Restore parser/dependencies/non-media tests and controller action ownership; shortcut color/alignment does not certify rebinding.
- Omitted/replaced code: `draw ordered_workspace_profiles/active selection/reset save acknowledgment`. Disposition: **pending**. Reason: Manager exists but inline ordered active-profile selection/state and immediate reset persistence are not restored. Requires profile identity/persistence acceptance rather than labels alone.
- Omitted/replaced code: `draw Help Language/Direction/Keyboard Shortcuts menus`. Disposition: **pending**. Reason: Existing English/Persian preference lacks complete System language and Auto/LTR/RTL/configurable bindings contracts. Nonfunctional entries must not be shown as completed features.
- Omitted/replaced code: `estop_button, draw_estop_release_dialog and draw shared-latch readiness/hide cluster`. Disposition: **pending**. Reason: Generic vector control, authoritative readiness, release confirmation/cancel/skip preference and synchronized remote release remain useful safety imports. Remove only media pause; adapt actual hardware latch contracts and tests.
- Omitted/replaced code: `draw connection requested/connecting/connected status/manual discovery and remote connection/browser/download actions`. Disposition: **pending**. Reason: Pakar has connection/typed actions and firmware tools, but full donor lifecycle/readiness/authenticated endpoint adaptation is not compared or accepted. Media remote browser cannot be blindly substituted for firmware management.
- Omitted/replaced code: `DropdownUiExt selected workspace/language/direction entries and new unified dropdown lifecycle`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `desktop/src/ui/menu.rs:43 [UTF8-LF sha256=199cebacdf180d88cb724d458ee096ffd29555ec1a16df9d47c28799a5c46cff; anchor=fn shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes()]`.
- Test-code evidence: `desktop/src/ui/menu.rs:124 [UTF8-LF sha256=199cebacdf180d88cb724d458ee096ffd29555ec1a16df9d47c28799a5c46cff; anchor=fn sibling_hover_switches_only_an_already_open_menu_and_clears_closed_state()]`.
- Test-code evidence: `tests/native_menu_test.mjs:12 [UTF8-LF sha256=80ac61041de322c2c0e2c0fad379a74e6e89d02724f1b80f2d9fa6200c7ef937; anchor=actual donor helper must match after application ID/visibility/whitespace adaptation]`.
- Remaining: Restore remaining useful generic menu contracts above against actual dependencies; see exact function-level mapping in NATIVE-MENU-COMPARISON.md. Native CI paint/pointer tests and running-app visual acceptance are separate from source guards.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/palette.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `c169db6a61b79ebb2ce00f0ca442305eefeea7b34740720a2cdf6424d6fef85d`.

- Target: `desktop/src/ui/palette.rs:36 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=pub fn apply(]`.
- Retained: Native semantic visual-role mapping and actual palette JSON; appearance geometry is not overwritten.
- Omitted/replaced code: `WebAppearanceState and donor native/Web shared-preferences assertions`. Disposition: **pending**. Reason: Pakar lacks the shared native/Web preferences contract. These are useful generic requirements, not media business logic; omitting them cannot be justified as superior.
- Omitted/replaced code: `Donor default Native palette`. Disposition: **replaced**. Reason: Pakar chooses Studio as its product default. Both palettes remain available; this is a preference decision, not a measured improvement.
- Test-code evidence: `desktop/src/ui/palette.rs:101 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn donor_surface_text_roles_and_geometry_are_preserved_in_both_themes()]`.
- Remaining: Embedded Web consumes the same JSON catalog through a stale-generation check. Complete mobile catalog use and live native/Web preference broadcast/readback; pure donor host-resolution tests do not prove connected-host synchronization.

### Pealayer: `src/ui/status_bar.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `7004bd7e936875b4d5328f10f482aa492827117251fce69e8ebb34561ff90663`.

- Target: `desktop/src/ui/status_bar.rs:44 [UTF8-LF sha256=ac3ad2971427a8b0b16e50eb1763d87080871df2a6ba88ce238e2eef5bbf7b26; anchor=fn hide_menu(]`.
- Target: `desktop/src/ui/connection_controls.rs:59 [UTF8-LF sha256=3216d5dde3a9bb6a8386cec0986140fdc2da5e1130c0cab17172d728b214fb76; anchor=pub fn usb(]`.
- Target: `docs/CONNECTION-CONTROLS.md:1 [UTF8-LF sha256=002ee046e9c206a4cc241ad2b9033971b4933959d8e14f43958848403879f21d; anchor=# Front-panel transport dropdowns]`.
- Retained: Actual per-item hide context menus, persistent visibility choices, bar-level restore menu and small truncating message labels adapted in Pakar status_bar.rs. Transport controls remain usable offline without claiming hardware readiness.
- Omitted/replaced code: `current_status_message update-manager priority and OSD lifetime; fullscreen exclusion; workspace label; i18n/RTL layout`. Disposition: **pending**. Reason: Pakar retains a status message and item menu, but does not yet share authoritative update/OSD lifetime, workspace text or localized/RTL/fullscreen policy. Restore actual dependencies and test rather than claim full-file parity.
- Omitted/replaced code: `draw_hardware_status board_ready/coordinator/active_transport distinction, physical RGB glyph and telemetry voltage/current/temperature/warnings`. Disposition: **pending**. Reason: Pakar current peer freshness/ACK gates exist but generic coordinator, RGB sensor and warning inventories need actual adapters before those fields can be displayed. Native dropdown live indicators are transport state, not hardware ACK.
- Omitted/replaced code: `prepared_timeline, media-rate/fps mode, hardware timeline revision/feedback; estop_active label`. Disposition: **not-applicable**. Reason: Prepared media/timeline playback is expressly excluded from this mpv-free controller. No E-stop contract is acknowledged by the current pair; reusable latch safety remains tracked in menu comparison, not fabricated as present.
- Omitted/replaced code: `Pealayer hardcoded connection dot RGB`. Disposition: **replaced**. Reason: Pakar transport controls use its existing shared palette green/current text roles; menu state stays readable in both themes without tinting unrelated hardware controls.
- Omitted/replaced code: `UI render-rate toggle/context policy and latest dropdown-backed FPS menu; authoritative physical RGB without double dimming`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `tests/connection_controls_test.mjs:1 [UTF8-LF sha256=4824489d60f705fccda52d3166ca487a1ea4d1721d0a0814706747fb7033aaf2; anchor=Source wiring guard only.]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:112 [UTF8-LF sha256=3216d5dde3a9bb6a8386cec0986140fdc2da5e1130c0cab17172d728b214fb76; anchor=fn offline_dropdowns_open_independently_and_usb_enumerates_only_when_open()]`.
- Remaining: Run native popup/pointer tests, deploy and capture actual controls; restore useful generic status lifetime/readiness/localization contracts in issues #3/#10.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/toasts.rs`

Verdict: **equivalent-behavior**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `a6501d2c0f859e186cac659a37d256f26f8896e98644e9cfc8ac880da5460e54`.

- Target: `desktop/src/ui/toasts.rs:4 [UTF8-LF sha256=8772d049fab0fba597bd7f4ba4403c7d7bd122f6027442e25f32da11d8f058f6; anchor=pub fn draw(ctx:]`.
- Retained: Actual donor root-only overlay: four newest messages, severity icons and semantic palette roles, bounded width, scrollable text, title, dismiss and deadline-driven repaint.
- Omitted/replaced code: `interop::get_live_config() accessor`. Disposition: **replaced**. Reason: Pakar owns the same live palette through config::live(). The OS-native notification route is separate; adding an explicit in-app enable preference does not remove the donor overlay when enabled.
- Remaining: This native overlay mapping is not shared Web/TUI message-delivery parity.

### Pealayer: `tests/i18n_surface_audit_test.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `504768bc9cf2fda32a8dab8b0f50d573c74e45001734f45c7ff3120f36333f63`.

- Target: `tests/localization_test.mjs:47 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Adapt Pealayer tests/i18n_surface_audit_test.rs]`.
- Retained: Actual donor known-raw-caption prohibition and required localized-boundary audit pattern adapted to real shell menu captions, not renamed timeline/media files. Added peer text exclusion and reversible placeholder/language tests.
- Omitted/replaced code: `Literal media cue/keyframe/player forbidden-string fixtures`. Disposition: **not-applicable**. Reason: Pakar has no those media surfaces; substituting their filenames would manufacture coverage. Controller-specific boundaries and font assets are tested instead.
- Omitted/replaced code: `Complete native/Web raw-caption audit coverage`. Disposition: **pending**. Reason: The current test protects selected imported boundaries; it is not exhaustive. Remaining native settings, transport/status and dynamic Web copy are explicitly pending, and native Cargo tests are blocked by cache budget.
- Test-code evidence: `tests/localization_test.mjs:50 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=for(const key of]`.
- Remaining: Expand ownership audit as each real controller surface is translated; execute actual native renderer tests and acceptance instead of treating source guards as runtime proof.

### Pealayer: `tests/windows_shell_suite_test.rs`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `d8696d8b591ca8ae4c77c78a07d19f6aca103e29906f267f19ac8ba1aa5d6e01`.

- Target: `desktop/src/platform/windows_notifications.rs:115 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=assert!(Hook::new(42).is_err())]`.
- Retained: Actual Preferences action/resource catalog is validated; invalid HWND registration rejects rather than claiming success; owner exclusion is tested. Live notification acceptance and shown-callback evidence are recorded independently.
- Omitted/replaced code: `Playback-duration error/paused percentages and Play/Pause thumbnail/tray labels`. Disposition: **not-applicable**. Reason: The tests model removed video playback, not device transfer progress. Pakar uses indeterminate taskbar activity only for a real pending settings/maintenance request; inventing a media percentage would publish false progress.
- Omitted/replaced code: `PealayerApp::default and media playback_time/duration/is_paused assignments`. Disposition: **replaced**. Reason: Pakar is an executable-only mpv-free controller; creating the donor media engine is inappropriate. Actual controller IPC/live smoke and direct invalid-HWND tests exercise its own lifetime instead.
- Omitted/replaced code: `ensure_shell_initialized retry and repeated shell-lifecycle acceptance`. Disposition: **pending**. Reason: Tray creation failures are visible and close-to-tray cannot hide an app without a tray, but complete root-taskbar initialization/retry and Explorer restart acceptance are not covered by the current tests.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:110 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=fn utf16_limits_preserve_complete_surrogates_and_termination]`.
- Remaining: Add root-taskbar lifecycle/reinitialization and hidden-window/tray/notification-click acceptance without fabricating successful HWND installation.

### Pealayer: `web_ui/scripts/appearance.test.mjs`

Verdict: **equivalent-behavior**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `35f269e9463e49dbc41572683ca374be8674a93e0e97c84240838d34dc950ddc`.

- Target: `tests/web_appearance_test.mjs:1 [UTF8-LF sha256=e634efa751b1fad2d351897adceccead42bda6978c2e43e71de6d63f936ef79c; anchor=First four tests port the actual Pealayer]`.
- Retained: All four actual donor contract tests ported: Neutral helper default, preference merge without unrelated-field loss/resolution leakage, host/offline theme/accent and repeated native event/explicit-theme handling. Additional shipped-code tests cover all palette roles, cancel/apply, contrast, identical-write skip, media events, malformed storage, cross-tab updates and session-only storage failure.
- Omitted/replaced code: `volume:37 unrelated media setting fixture`. Disposition: **replaced**. Reason: The unrelated value is endpoint:http://ledboard.local instead. Its preservation and original-object immutability assertions are retained; no actual test contract is removed.
- Omitted/replaced code: `Direct TypeScript test import`. Disposition: **replaced**. Reason: Tests import the actual adapted JavaScript functions; browser tests additionally execute the shipped embedded source, avoiding a separate mock appearance engine.
- Test-code evidence: `tests/web_appearance_test.mjs:30 [UTF8-LF sha256=e634efa751b1fad2d351897adceccead42bda6978c2e43e71de6d63f936ef79c; anchor=live host theme events leave System preference unchanged]`.
- Remaining: Test execution proves module and simulated browser contracts, not connected native event transport or physical hardware.

### Pealayer: `web_ui/src/appearance.ts`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `aca0c9aba5fe6389995f92306e20f62debf38fdb4ed3b3061cf9b9f17a56c430`.

- Target: `assets/themes/appearance.js:4 [UTF8-LF sha256=62a7ebba994ad44f4e2d647bb97684aac9b563104f3f707b43573289e514ecdd; anchor=export function mergeAppearance]`.
- Target: `tools/generate-web-appearance.mjs:27 [UTF8-LF sha256=16789897f672dad4c853e7d25e4f27a3b54663b67b6b875c904e93704faea339; anchor=replaceBlock('// BEGIN SHARED APPEARANCE']`.
- Target: `firmware/esp8266/src/WebUi.h:255 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=// BEGIN SHARED APPEARANCE]`.
- Retained: All five actual donor functions retained: mergeAppearance, resolvedAccent, resolvedAppearanceTheme, accentForeground and paletteName. TypeScript annotations removed for standalone embedded JavaScript; field names, host/offline resolution, exact accent constants, custom validation and luminance threshold remain unchanged.
- Retained: Generated into the shipped WebUi.h with the shared palette asset; --check rejects stale embedding. Browser preferences dialog validates theme/palette/accent, protects drafts, handles storage failure and cross-tab/System events without device writes or repetitive identical storage writes.
- Omitted/replaced code: `AppearanceState TypeScript interface and generic T return annotation`. Disposition: **replaced**. Reason: ESP serves standalone JavaScript, not a TypeScript/React build. Actual function bodies are retained; browser-facing enum/color validation rejects untrusted stored values. No preference or resolution field is dropped.
- Omitted/replaced code: `Connected host AppearanceState broadcast/native-Web shared preference path`. Disposition: **pending**. Reason: Pure functions accept host state and tests preserve donor semantics, but standalone browser controls are browser-local today. No actual native appearance event/readback transport is integrated in this pass; System fallback is labeled browser-local.
- Test-code evidence: `tests/web_appearance_test.mjs:22 [UTF8-LF sha256=e634efa751b1fad2d351897adceccead42bda6978c2e43e71de6d63f936ef79c; anchor=System theme and accent preserve donor host resolution]`.
- Test-code evidence: `tests/web_appearance_test.mjs:63 [UTF8-LF sha256=e634efa751b1fad2d351897adceccead42bda6978c2e43e71de6d63f936ef79c; anchor=dialog draft/cancel never changes]`.
- Remaining: Implement native/Web/mobile shared preference transport and live authoritative host events, while keeping device parameters separate from client appearance; verify multi-client broadcast/readback.

### Pealayer: `web_ui/src/components/HeaderBar.tsx`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `df84386e53d729eabddf9476026a9f96afe130dd3429612f4c73509d1757d16b`.

- Target: `tools/generate-branding.mjs:3 [UTF8-LF sha256=a515e79397a85a6211ac0251f7bb5d6e90eda237979e4d40740622167fe0e2db; anchor=Pakar embeds it for the standalone ESP surface]`.
- Target: `firmware/esp8266/src/WebUi.h:69 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=<!-- BEGIN SHARED BRAND HEADER -->]`.
- Target: `docs/BRANDING-PARITY.md:40 [UTF8-LF sha256=b9b3c92027c004ae46735034876e30bca688e9ec66d8391bfef98d25be7439e7; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `firmware/esp8266/src/WebUi.h:362 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=function renderApplicationIdentity]`.
- Target: `assets/web/device-settings.js:53 [UTF8-LF sha256=ffbe4354eba7347ecd0a80618c9c92b72578e2a67b3e7c5097ce41333f75a2d8; anchor=function settingsCanClose()]`.
- Target: `docs/INTERFACE-CONTRACT.md:8 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## One catalog, distinct transport and platform adapters]`.
- Retained: Actual resolved-artwork img/alt binding adapted to standalone header/login, replacing diamond and font fallback.
- Retained: Firmware name/title/artwork binding replaces fixed header branding; stale/disconnected peer resets title/header/favicon/touch artwork and validates bounded inline PNG.
- Retained: Actual retained Modal/draft lifecycle adapted to grouped device settings; separate same-instance WebSocket actions preserve HTTP refresh and reject obsolete socket callbacks. Native Draft/SecretEdit/Write validation/readback semantics are reused.
- Omitted/replaced code: `Header collapse; arbitrary remote connectionTarget/draftTarget endpoint modal; localization/RTL`. Disposition: **pending**. Reason: The new grouped device-settings modal and same-instance WebSocket dropdown are not arbitrary remote endpoint selection or localized navigation. Standalone-device origin/auth and preferences adapters still need porting and tests.
- Omitted/replaced code: `capability-aware Dropdown install/update/share/fullscreen/notifications/haptics/audio/wake-lock cluster`. Disposition: **pending**. Reason: Requires actual WebPlatform integration and ownership/permission tests; not excluded media business.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=91f8491b1cd941c2af78f621ab69658397d372959fb3c25a9dce913f62405bc9; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=99621b44a961771053066590ce43759c24bdd2c0993de0d7c36ae615a2c5a2ad; anchor=Actual Web application identity]`.
- Test-code evidence: `tests/interface_settings_test.mjs:31 [UTF8-LF sha256=e2588bf230a64a1260bba5acd7ad3d2f26c8f16361e07185a1059cd210ef122e; anchor=Failed saves preserve the draft]`.
- Remaining: Complete generic omissions and missing editors named in INTERFACE-CONTRACT.md; native/mobile/deployed acceptance remains open.

### Pealayer: `web_ui/src/i18n.ts`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `331b0ef10c89f4e695101cb332e62607e15ce37bd0543104453060c0e45a66e6`.

- Target: `assets/web/localization.js:15 [UTF8-LF sha256=d11d246009a684e5de75e7c006623f33df1383e646d74a8d903a006cd89725e2; anchor=function uiTr(text)]`.
- Target: `tools/generate-font-assets.mjs:27 [UTF8-LF sha256=58b1c4831be837ea38319d599d014add98f670f9006fd3d91599377e8e432f91; anchor=const dictionary={}]`.
- Target: `firmware/esp8266/src/WebUi.h:197 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=// BEGIN SHARED LOCALIZATION]`.
- Retained: Actual donor fa-dictionary fallback semantics reused for application copy, rather than translating arbitrary runtime data. Applicable native dictionary subset generates into the canonical Web module and actual firmware HTML.
- Retained: Added system/explicit language and Auto/LTR/RTL preferences, owned-template text/placeholder/accessibility snapshots, per-browser low-write persistence, shared font route and immutable peer-name boundary.
- Omitted/replaced code: `Media-library, player, playback, cue/timeline and libmpv-specific Web dictionary/surfaces`. Disposition: **not-applicable**. Reason: Those surfaces belong to excluded media business and would add unrelated controls, not controller capabilities.
- Omitted/replaced code: `Remaining generic Web copy and native/Web/mobile preference synchronization`. Disposition: **pending**. Reason: The actual controller subset is integrated, not the entire donor Web feature set. Dynamic errors/confirmations/status/menu attributes and narrow/mobile runtime acceptance remain; browser locale is not silently written to another client or firmware.
- Test-code evidence: `tests/localization_test.mjs:43 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=No identical storage writes]`.
- Test-code evidence: `tests/localization_test.mjs:41 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=assert.equal(attrs.placeholder]`.
- Remaining: Finish all dynamic application-copy surfaces and controller-safe shared preference contracts; execute live-device and mobile-width/Android locale tests.

### Pealayer: `web_ui/src/webPlatform.ts`

Verdict: **parity-gap**. Revision: `9f605ade0c8056d56816f1dffcdf04c3e0a27fbc`. Original-byte SHA-256: `23e8fd2a8f17d296f14b81d6618cbd166129e116e36515444239edac15f89554`.

- Target: `tools/generate-branding.mjs:17 [UTF8-LF sha256=a515e79397a85a6211ac0251f7bb5d6e90eda237979e4d40740622167fe0e2db; anchor='FAVICON':]`.
- Target: `firmware/esp8266/src/WebUi.h:7 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=<!-- BEGIN SHARED BRAND FAVICON -->]`.
- Target: `docs/BRANDING-PARITY.md:40 [UTF8-LF sha256=b9b3c92027c004ae46735034876e30bca688e9ec66d8391bfef98d25be7439e7; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Same resolved artwork binds favicon/touch/header before login; ESP embeds data instead of requesting a protected host filesystem icon.
- Omitted/replaced code: `storedBoolean/supportsStandaloneMode/useWebPlatform capabilities/events; install/share/toggleFullscreen/enableNotifications/applyUpdate; settings setters/signalInteraction`. Disposition: **pending**. Reason: Generic browser capabilities/lifecycle/preferences need controller-specific integration and tests; static branding does not certify them.
- Omitted/replaced code: `registerPealayerServiceWorker/update/controllerchange, wake-lock cancellation/visibility and offline badge`. Disposition: **pending**. Reason: PWA manifest/high-resolution artwork/cache ownership and streaming-specific wake-lock state remain open.
- Omitted/replaced code: `mediaTitle/current_video state icon resolution, MediaSession video metadata/play/pause/seek/chapter handlers`. Disposition: **not-applicable**. Reason: Video title/track/session business is excluded; generic browser capabilities are still pending.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=91f8491b1cd941c2af78f621ab69658397d372959fb3c25a9dce913f62405bc9; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### PU850-ESP: `LocalLib/Shell.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0113acbff973285f5af186a4857d94cc2df3c99813e0db4ba876f861e7c1083b`.

- Target: `firmware/esp8266/src/TelnetShell.h:63 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class LineEditor]`.
- Target: `firmware/esp8266/esp8266.ino:2384 [UTF8-LF sha256=70e5ee73c384801511ce9b75ac25ffbde60c1ff1e9be45f4dafd225d1e10df5d; anchor=void executeTelnetCommand(String line)]`.
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
- Target: `firmware/esp8266/esp8266.ino:2384 [UTF8-LF sha256=70e5ee73c384801511ce9b75ac25ffbde60c1ff1e9be45f4dafd225d1e10df5d; anchor=void executeTelnetCommand(String line)]`.
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

### PU850-ESP: `LocalLib/Telnet.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `a78d88c149d19d8989c6db3dce0731189c2bf46c45fd64b73e9871a80f6c9bdd`.

- Target: `firmware/esp8266/src/TelnetShell.h:13 [UTF8-LF sha256=dac714f88775ffada5fa6d706dee049b344e6d701530de9b57d53400444542ec; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/esp8266.ino:2501 [UTF8-LF sha256=70e5ee73c384801511ce9b75ac25ffbde60c1ff1e9be45f4dafd225d1e10df5d; anchor=void serviceTelnet() {]`.
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
- Target: `firmware/esp8266/esp8266.ino:2501 [UTF8-LF sha256=70e5ee73c384801511ce9b75ac25ffbde60c1ff1e9be45f4dafd225d1e10df5d; anchor=void serviceTelnet() {]`.
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
- Target: `assets/web/rayanlamp.js:4 [UTF8-LF sha256=73144dfaf4a3e02fff90e25d3d53bb48029bb2dd802bf5051bf4f2bc67124491; anchor=function latinDigits]`.
- Retained: Actual Persian/Arabic-Indic-to-ASCII maps adapted into Rust and JavaScript and invoked by native/Web cell search. Canonical IDs/ranges stay ASCII; peer names and protocol identifiers are not globally transformed.
- Omitted/replaced code: `ShapePersianDigits visual decimal shaping`. Disposition: **pending**. Reason: Search needs ASCII normalization now. Apply visual shaping only to audited application-owned numeric labels, not arbitrary peer text or wire IDs. Display consumers remain pending.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:28 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=latinDigits('۰۱۲۳۴۵۶۷۸۹]`.
- Remaining: Finish owned numeric display shaping and execute guarded native digit/search tests.

### RayanLamp: `LICENSE`

Verdict: **equivalent-file**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `a7931f292caa44328db79c5b19515c6629a5b31042a4cf375f28c341d84fd28d`.

- Target: `desktop/licenses/RayanLamp-MIT.txt:3 [UTF8-LF sha256=a7931f292caa44328db79c5b19515c6629a5b31042a4cf375f28c341d84fd28d; anchor=Copyright (c) 2026 YektaYar]`.
- Retained: Complete YektaYar MIT copyright and notice retained for actual donor code.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:1 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=Adapted from RayanLamp haptics.test.ts]`.
- Remaining: Include notice with eventual binary distributions, not just source.

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

- Target: `assets/web/rayanlamp.js:79 [UTF8-LF sha256=73144dfaf4a3e02fff90e25d3d53bb48029bb2dd802bf5051bf4f2bc67124491; anchor=function subscribeLocalePreferences]`.
- Target: `assets/web/localization.js:33 [UTF8-LF sha256=d11d246009a684e5de75e7c006623f33df1383e646d74a8d903a006cd89725e2; anchor=subscribeLocalePreferences(value=>]`.
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

- Target: `assets/web/rayanlamp.js:15 [UTF8-LF sha256=73144dfaf4a3e02fff90e25d3d53bb48029bb2dd802bf5051bf4f2bc67124491; anchor=class WebHaptics]`.
- Target: `firmware/esp8266/src/WebUi.h:67 [UTF8-LF sha256=0f6590811cc85cf72272921bc77c9c68afb4dfb8b822387dd63ebf0ae5996622; anchor=id="uiHaptics"]`.
- Retained: Actual pulse constants, bounded audio cues, optional vibration API, fail-closed visibility/reduced-motion conditions, copied pattern arrays and host-error containment adapted into embedded JavaScript. Opt-in Touch feedback defaults off, disabled when unsupported; only explicit command HTTP results trigger it, never polling/telemetry/frames.
- Omitted/replaced code: `TypeScript interfaces/private/type annotations`. Disposition: **replaced**. Reason: ESP serves dependency-free JavaScript. Actual runtime bodies are retained; explicit own-property cue validation replaces a compile-time-only guarantee.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:24 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=h.trigger('arbitrary raw frame')]`.
- Scoped advantage: Unknown JavaScript cues now fail closed at runtime.
- Remaining: Verify supported Android vibration and actual command lifecycle. HTTP acceptance is not actuator readback.

## Unreviewed files are not approved omissions

Every pending path has its own CSV row. Its omission reason explicitly says there is no approved reason to leave it out. Prior keyword screening or an unbound historical adaptation note is not accepted as behavior proof. This includes all still-pending PuzzleBoard/40LED variants and ControllerBoardMini legacy files, plus unreviewed Pealayer/PCController/PU850 files, assets, dependencies, tests and scripts. Earlier focused variant notes remain useful routing evidence, not complete comparative certification.

File-by-file checklist: https://github.com/atomicdeploy/PakarController/issues/3. Master production acceptance: https://github.com/atomicdeploy/PakarController/issues/2. Run `pwsh -File tools/generate-file-comparison.ps1 -RequireComplete` to enforce the comparison gate. It must fail while any pending comparison or known source parity gap remains. Physical, deployment, visual and all-surface acceptance are separate gates even after this one passes.
