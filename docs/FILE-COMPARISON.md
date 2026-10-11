# PakarController donor comparison

Whole-repository superiority is **not established**. This report exposes retained code, scoped improvements, omissions and gaps; it does not turn missing reviews into exclusions.

Routing: 1320 paths have no specialist capability match. They remain visible through `consult-reuse.ps1 -Concern donor-audit` and issue #3; lack of a keyword match is NOT an exclusion. Every CSV row includes capability routing and owning issue links.

Inventory: 4242 files; 84 complete source-file comparisons; 4158 pending comparisons; 51 compared files with known parity gaps. Header/implementation pairs can describe the same gap. Missing sparse files remain pending.

Every inventoried path, source revision/hash, target/proof file and line, omitted code, disposition and concrete reason is in `file-comparison.csv`. Hash-bound manual decisions are in `file-comparison-evidence.json`. Donor SHA-256 values bind original bytes; target/proof SHA-256 values bind UTF-8 without BOM with LF newlines. Anchors are recomputed into exact line numbers. Test-code presence is not test execution; execution receipts are in `FILE-COMPARISON-RECEIPTS.md`.

## Coverage

| Donor | Inventoried | Compared | Pending | Files with gaps |
|---|---:|---:|---:|---:|
| ControllerBoardMini | 19 | 0 | 19 | 0 |
| ControllerBoardMini/Puzzles | 18 | 12 | 6 | 9 |
| PCController | 1211 | 9 | 1202 | 3 |
| Pealayer | 514 | 39 | 475 | 25 |
| PU850-ESP | 109 | 4 | 105 | 4 |
| PuzzleBoard | 22 | 0 | 22 | 0 |
| RayanLamp | 2349 | 20 | 2329 | 10 |

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

- Target: `assets/web/device-settings.js:94 [UTF8-LF sha256=f3a38568cdd14a091d2a120d352adc40c25311c310c9f748c9e300c57eaee613; anchor=function panelToggle(]`.
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
- Target: `mobile/test/peer_context_test.dart:12 [UTF8-LF sha256=b29ce5cc725192d9ed743ebfb6cf2ced530794d1ae7fb9e26e061c39526e837e; anchor=No peer ACK cannot be inferred]`.
- Target: `mobile/test/widget_test.dart:112 [UTF8-LF sha256=1701769d2026ac4bbdedca44ed111b195b9ae5d7d02ba29fd7e8b2cc8d68d3e1; anchor=Unknown schema has no Table]`.
- Target: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Target: `desktop/src/capabilities.rs:202 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=fn serial_manifest_is_individual_and_table_modes_are_profile_scoped]`.
- Retained: Actual disconnected, missing status and capability-free cases adapted into shipped-Web tests. Independent peripheral ACKs, expiry, cross-kind and malformed geometry/schema cases added. C++ tests execute actual HELLO/display-ACK policy.
- Retained: Equivalent Flutter fixture cases execute actual capability parsing, unsupported schema/geometry, display measured ACK, per-feature cards, identity change and phone/tablet navigation; API fixtures reject pending reads/queued writes after owner or credential changes.
- Omitted/replaced code: `Vitest/liveSnapshot/emptySnapshot fixture ownership`. Disposition: **replaced**. Reason: Node VM executes embedded Web code, C++ executes firmware policy, and Rust has USB adapter tests. React/Vitest would target a nonexistent React owner in this embedded surface.
- Omitted/replaced code: `Valid/invalid INA219, temperature, PWM, LCD, Bluetooth, relay/RF/statusLED fixture cases`. Disposition: **pending**. Reason: Restore exact bounded-invalid-value cases with the genuine PCController adapter; those peripherals are currently absent, not replaced by synthetic zero fixtures.
- Test-code evidence: `tests/web_context_test.mjs:48 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=Unsupported geometry is not forced]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Rust tests remain cache-budget blocked; generic adapter sensor/profile tests and physical/native acceptance remain #10.

### PCController: `Tools/Controller/web/src/peripheral-availability.ts`

Verdict: **parity-gap**. Revision: `bd9fbf92162182b9d05e99b357943f72353a43ea`. Original-byte SHA-256: `fcf2e397564c843711fd64db2bd2d820552180ac4f5abdf0d44ccbbb86be7a84`.

- Target: `desktop/src/capabilities.rs:40 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=pub fn has(]`.
- Target: `mobile/lib/peer_context.dart:35 [UTF8-LF sha256=d15536d7b1c4315b737004e9e90cc6757f972353431dbad09151255d7c41cf77; anchor=static PeerContext fromStatus]`.
- Target: `mobile/lib/main.dart:274 [UTF8-LF sha256=771361f85f85fade6d4fdd7e9532752249d4724482782bfd7b7ef1913a58b5b8; anchor=Future<bool> _command]`.
- Target: `libraries/BoardCore/src/DeviceContext.h:26 [UTF8-LF sha256=ce571a112f7d06dc25cdf7f35a383ba4f23c3ca4337ac1bb9968ef17dec72689; anchor=inline uint16_t features(]`.
- Target: `firmware/esp8266/src/WebUi.h:704 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function peerAvailability(]`.
- Retained: Actual connected AND have_status AND advertised-capability conjunction adapted to selected-peer fresh manifests. No feature inference from hostname, pixel count, catalog, defaults or online alone.
- Retained: TM1637 additionally requires fresh measured ACK; Text and Buzzer have independent HELLO support. Native/Web hide unrelated controls and gate domain commands.
- Retained: Flutter now adapts the same ready AND advertised schema-1 conjunction, table geometry and individual ACK policy; selected owner revisions reject old replies and retained dialogs. Unknown/stale peers have only client connection/overview navigation.
- Omitted/replaced code: `Snapshot/status/hello fields and compact status flag fallback`. Disposition: **replaced**. Reason: Pakar JSON and append-only USB share a schema-1 normalized manifest. AVR HELLO and display-status ACK are aggregated on ESP; copying PCController bit values would alias different protocols.
- Omitted/replaced code: `bounded, ina219, pwm, temperatureLED, temperatureBTAudio, lcd, bluetoothAudio, relays, rf, statusLED and invalid measurement fields`. Disposition: **pending**. Reason: Those drivers are absent on the current pair. A real PCController adapter must preserve their detection, bounds and invalid-value distinctions later. No fabricated controls are displayed.
- Test-code evidence: `tests/web_context_test.mjs:24 [UTF8-LF sha256=fefae5fe0fd4aaf5d0961fb24695b979ca80e6efd07d3d87ea6b6e0e3ea6ed13; anchor=No inferred board kind or controls]`.
- Test-code evidence: `mobile/test/peer_context_test.dart:28 [UTF8-LF sha256=b29ce5cc725192d9ed743ebfb6cf2ced530794d1ae7fb9e26e061c39526e837e; anchor=Independent ACKs and measured display gate]`.
- Test-code evidence: `mobile/test/widget_test.dart:261 [UTF8-LF sha256=1701769d2026ac4bbdedca44ed111b195b9ae5d7d02ba29fd7e8b2cc8d68d3e1; anchor=Peer ACK loss removes table and independent cards]`.
- Test-code evidence: `mobile/test/api_test.dart:11 [UTF8-LF sha256=75829eb3b8dd744b2d87025db824b291bdc86e074baff8c28a597d425ed4a823; anchor=Changed endpoint discards an in-flight status]`.
- Test-code evidence: `tests/device_context_test.cpp:4 [UTF8-LF sha256=aea06975d4e027b0804c0a78c0385aefe1b7766418cd7835f22e80ea361bc650; anchor=int main(]`.
- Remaining: Native execution and generic PCController profile/discovery adapter remain #10/#6 work; source/C++/Web proof is not native or physical acceptance.

### Pealayer: `.github/dependabot.yml`

Verdict: **scoped-improvement**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `fa4791635251ad7d5be26ce70d71a8e59aa7b814a4f09f2b5b749d8a49908225`.

- Target: `.github/dependabot.yml:1 [UTF8-LF sha256=136887ba77b56544d0bcd367a31d2daaa7637ecdb8b997d63df6cb552d6f08e8; anchor=version: 2]`.
- Retained: Actual donor weekly grouped github-actions and Cargo updates, with Cargo path adapted to the existing desktop workspace.
- Omitted/replaced code: `npm updates at /web_ui`. Disposition: **not-applicable**. Reason: Pakar has no /web_ui/package.json or Vite runtime. Embedded Web/tests use Node built-ins; Flutter's actual /mobile Pub dependency contract is covered instead.
- Test-code evidence: `tests/repository_governance_test.mjs:67 [UTF8-LF sha256=da232ac7efd7a41028424bffd0f08865fc8049b0a4f971faa74703da43b2d966; anchor=console.log('Repository governance]`.
- Scoped advantage: Adds grouped Pub updates for the actual Android client and label routing while retaining weekly grouped Actions/Cargo updates.

### Pealayer: `.github/workflows/ci-cd.yml`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `449d49033eb2ab3ced173566858e0badfa700414fe82747131c4d18a8eda0b0c`.

- Target: `.github/workflows/build.yml:2 [UTF8-LF sha256=c2bc9c995b4c3717a6e66916f31cef5b8010730e146b4e4dfe8ee437a62c80a2; anchor=name: "🚀 Build · Test · Release"]`.
- Target: `tools/build-desktop.ps1:34 [UTF8-LF sha256=14cd626601e9c3aad7b8d9913abe53078a9185e9bad10052c3cf0ff2e1c41fe6; anchor=--test-threads=1]`.
- Target: `tools/ci-checks.mjs:186 [UTF8-LF sha256=9144f55833c8bc5bd8f2552129bd874247bee75cc5355abbaef68ec8740302fb; anchor=export function runChecks]`.
- Target: `tools/prepare-release.mjs:16 [UTF8-LF sha256=96c3a9f5fbc5dfd055a1b139b09271d92d19beaf9cc1cda3c83a88f266ee177e; anchor=export function firmwareManifest]`.
- Retained: Actual donor branch/PR/tag/dispatch/merge-group triggers, concurrency policy, pinned actions without checkout credentials, health-before-build jobs, timeouts, Rust cache reuse, locked tests/builds, packaging, curated notes, manifest/checksums/provenance and aggregate results adapted to current Windows/ESP/AVR/Android targets.
- Retained: Release content is gated on all builds, created as an immutable draft prerelease, with two correctly matched board pairs and raw HTTPS update manifests. Untrusted PRs do not execute publishing code with a write token.
- Retained: Actual donor serial native test execution, isolated test configuration and RUST_BACKTRACE=1 restored. Runtime harness flags are separated from compile jobs; immediate panic output remains available even if a later test aborts before its summary.
- Omitted/replaced code: `LIBMPV_WINDOWS_RELEASES_API_URL, MPV_DLL_PATH, Windows GNU libmpv linking, Linux/macOS mpv dependencies and standalone pealayer-downloader`. Disposition: **not-applicable**. Reason: Pakar deliberately excludes the media runtime and standalone media utility. Copying this setup would introduce forbidden mpv and unrelated package dependencies.
- Omitted/replaced code: `macOS Intel/Apple silicon and Linux native platform matrix/packages`. Disposition: **pending**. Reason: Current accepted native target is Windows; firmware and Android use Linux runners. Additional native targets need actual platform build/runtime tests rather than claiming donor's unrelated acceptance.
- Omitted/replaced code: `UPX checksum-locked packing and packed native package smoke/resource checks`. Disposition: **pending**. Reason: Existing Windows wrappers preserve donor resource/donor/cache gates and CLI runtime tests, but Pakar has no accepted compressed native package probe. Do not call a source or CLI test full packed GUI runtime acceptance.
- Omitted/replaced code: `branch-tag and archive suffix creation, softprops update-existing release`. Disposition: **replaced**. Reason: User requests stable names without random/time suffixes. Version releases keep stable package names; release enumeration must succeed and an existing release is not overwritten.
- Test-code evidence: `tests/build_storage_test.ps1:17 [UTF8-LF sha256=7fb730f4913809a4252404c97e5d73c21d9c9efed6ec794df48715b38c152590; anchor=Actual donor serial test contract]`.
- Test-code evidence: `tests/repository_governance_test.mjs:67 [UTF8-LF sha256=da232ac7efd7a41028424bffd0f08865fc8049b0a4f971faa74703da43b2d966; anchor=console.log('Repository governance]`.
- Remaining: Complete Windows menu-label primary-color assertion/package and validate the unexecuted draft publisher. 8849ab8 native execution completes 114 passed/1 failed without abort; workflow/source, both firmware variants, Android preview and CodeQL pass, not all-green or deployed acceptance. Restore useful additional native targets and optional compressed package/runtime verification.

### Pealayer: `.github/workflows/codeql.yml`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `fc3c298cf966d77f50b575614b4c26f24f1606459f07749a46880cd482f1cb53`.

- Target: `.github/workflows/codeql.yml:2 [UTF8-LF sha256=cb50de1c3e72fc9eeeb4931e82a519331857ca4f19e928f9964a649f0769718e; anchor=name: "🛡️ Security · CodeQL"]`.
- Retained: Actual donor Actions and JavaScript matrix, no-build security-extended queries, pinned checkout/CodeQL actions, no persisted checkout credentials, PR/main/schedule/dispatch/merge-group triggers, cancellation, timeouts and fail-closed aggregate summary.
- Omitted/replaced code: `Pealayer React/Vite names, packages:read permission and branded prose`. Disposition: **replaced**. Reason: Pakar analyzes embedded Web/scripts and Actions, not Pealayer React. Package read permission is unnecessary for these none-build scans; Rust/C++ coverage is explicitly not invented.
- Test-code evidence: `tests/repository_governance_test.mjs:67 [UTF8-LF sha256=da232ac7efd7a41028424bffd0f08865fc8049b0a4f971faa74703da43b2d966; anchor=console.log('Repository governance]`.

### Pealayer: `assets/themes/palettes.json`

Verdict: **equivalent-file**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501`.

- Target: `assets/themes/palettes.json:2 [UTF8-LF sha256=11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501; anchor="studio"]`.
- Retained: Byte-identical Studio/Native light/dark JSON catalog, all fourteen semantic roles.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/ui/palette.rs:71 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn imported_palettes_have_every_required_role()]`.
- Test-code evidence: `tests/web_appearance_test.mjs:55 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=embedded catalog exactly matches]`.
- Remaining: Embedded Web now consumes all four actual palettes and fourteen roles. Mobile catalog use and native/Web shared preference synchronization remain pending; byte-identical asset retention is not all-surface parity.

### Pealayer: `build.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `68fb1bdd9e7c7523ab303e37001d3a5252dcb03abe85e489432fe913be7b2e0f`.

- Target: `desktop/build.rs:11 [UTF8-LF sha256=02a9422df8169a9f9795df83c0a990d07a2f74bf2dff314828a3c402f816f3bd; anchor=brand_image::native_icon_bytes(brand_image::icon_bytes())]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Application PE/window/tray master derivatives are separated from actual semantic quick-action resource generation; master/helper changes trigger rebuild.
- Omitted/replaced code: `emit_build_metadata; APPLICATION_BRAND JSON/APP_* precedence/filename validation; version/InternalName/OriginalFilename/copyright/provenance fields`. Disposition: **pending**. Reason: Fixed current identity exists; validated external branding and full generic metadata/provenance remain useful imports.
- Omitted/replaced code: `macOS brew/mpv probing and rpath flags`. Disposition: **not-applicable**. Reason: Pakar is explicitly mpv-free; media linker dependencies violate that scope.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `docs/releases/next.md`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `71f81fa503d0494a85dd700e16cabdb23e1a6dcd6abbb0abc1af47000584b3f7`.

- Target: `docs/releases/next.md:1 [UTF8-LF sha256=cecaadeb61a7daaec709130503f49a0bb98cde4408af69bc6abe5837b7756fcd; anchor=# PakarController development packages]`.
- Retained: Actual donor curated preview notes explicitly distinguish a development milestone from stable-release, physical and platform acceptance.
- Omitted/replaced code: `Pealayer media/audio/timeline milestone inventory and archived media screenshot evolution links`. Disposition: **replaced**. Reason: These are donor-specific release facts, not transferable Pakar features. Record actual controller/interface packages and link Pakar issue #2 instead.
- Test-code evidence: `tests/repository_governance_test.mjs:67 [UTF8-LF sha256=da232ac7efd7a41028424bffd0f08865fc8049b0a4f971faa74703da43b2d966; anchor=console.log('Repository governance]`.

### Pealayer: `docs/verification/LOCAL-WORKSPACE-HYGIENE.md`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `c79e88b50b9586fd937620456cb329350c60f9b825ec68c39d07c4544e559af9`.

- Target: `docs/REUSE-PREFLIGHT.md:42 [UTF8-LF sha256=13521024e5b18f314abcc71af4e9c722262a7cb04c17f03155b1e88d0cf0972e; anchor=Mandatory sequence on every task]`.
- Retained: Actual canonical-cache/stable-staging/preservation/blocked-operation and separate runtime-acceptance guidance read and adopted in mandatory continuation policy and tooling.
- Retained: Reread new shared-cache rejection/hash-copy/storage-inventory guidance at d3f11b1; actual read-only directory summary and equality helper now imported. Ownership and blocked-operation rules still apply.
- Omitted/replaced code: `Historical Pealayer paths, exact relocation/cleanup/deployment/media-clock events`. Disposition: **not-applicable**. Reason: These are donor historical facts, not Pakar installations. Copying them would misrepresent ownership or completed cleanup.
- Omitted/replaced code: `Same-volume rename/hardlink-aware relocation and reusable host profile/peer updater workflow`. Disposition: **pending**. Reason: These are useful generic mechanisms but no Pakar cache relocation or complete updater deployment is implemented by this policy. Current blocked cleanup stays blocked; no rerouting through another shell.
- Omitted/replaced code: `Mandatory donor lookup was previously absent from executable build/stage/capture paths`. Disposition: **replaced**. Reason: Capability lookup with source/target/test hash gates now runs before those actions; other task surfaces are required by AGENTS.md, not falsely claimed universally sandbox-enforced.
- Omitted/replaced code: `New source-checkout cache prohibition and explicit guarded target prune`. Disposition: **pending**. Reason: Canonical cache relocation requires an owner-preserving migration without duplication. Existing cleanup was blocked and cannot be rerouted through this helper. No reclaim or reduced physical SSD wear is claimed.
- Remaining: Complete applicable generic host/relocation/updater tooling after ownership checks; extend executable gates when additional capability mappings are fully reviewed.

### Pealayer: `README.md`

Verdict: **scoped-improvement**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `4e78a6c486bfc7d78adc6d34552bddfd67c89997edb8d2bae0b46e70c705dcb6`.

- Target: `README.md:7 [UTF8-LF sha256=badf979fd797d25a90ca6538ba49339d27a3af0cf6cb929ae588f9ad1ea13c4e; anchor=Connected boards. One contextual control workspace.]`.
- Target: `tools/generate-readme-artwork.mjs:1 [UTF8-LF sha256=2162d1ad3760618260eb9d659dfebb6651ad1e5f5b4eb2c37c12cde1ef7ff8d2; anchor=README uses the same bytes]`.
- Retained: Pealayer's actual centered logo/title/tagline, badge row, visual preview, section navigation, architecture, build, docs, contribution and license structure adapted to Pakar.
- Omitted/replaced code: `Media engine, NLE cue/timeline walkthroughs, playback APIs/shortcuts and libmpv installation sections`. Disposition: **not-applicable**. Reason: These describe Pealayer media business/runtime rather than Pakar board control. README links actual controller APIs, wiring and release/acceptance instead; no mpv is linked.
- Omitted/replaced code: `Donor platform and latest-release badges, cinema-grade completion language and media screenshots`. Disposition: **replaced**. Reason: Unsupported OS/release claims and unrelated images would misrepresent this repo. Use the shared application logo and real embedded Web screenshot labeled simulated, explicit PR/source and physical acceptance boundaries.
- Test-code evidence: `tests/repository_governance_test.mjs:67 [UTF8-LF sha256=da232ac7efd7a41028424bffd0f08865fc8049b0a4f971faa74703da43b2d966; anchor=console.log('Repository governance]`.
- Scoped advantage: Canonical cross-interface icon bytes and explicit proof/acceptance boundaries prevent a polished landing page from claiming an unaccepted release.

### Pealayer: `scripts/capture-visible-window-windows.ps1`

Verdict: **scoped-improvement**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `2876c19687b1a209e777f3d505e4b24b6c4b1403a0f1de99edcedf0a624d9527`.

- Target: `tools/capture-window-windows.ps1:62 [UTF8-LF sha256=e6892b3c4da4f6a5bd90d6275f868b041516d8bebecb03de23dccaad94e60135; anchor=public static extern bool PrintWindow]`.
- Retained: Actual donor PrintWindow P/Invoke, minimized/window-bounds validation, DPI awareness, System.Drawing PNG lifecycle and read-only behavior.
- Omitted/replaced code: `Get-Process -Name foreground filter`. Disposition: **replaced**. Reason: Explicit expected PID and window-owner revalidation remove ambiguity between production and fixtures; screen fallback checks foreground ownership without changing focus.
- Omitted/replaced code: `RenderOccludedWindow branching and unconditional Bitmap.Save`. Disposition: **replaced**. Reason: Capability-gated egui framebuffer is tried first, then PrintWindow/BitBlt and verified foreground pixels. Blank images and overwrites are rejected.
- Test-code evidence: `tests/screenshot_harness_test.mjs:9 [UTF8-LF sha256=b1378631676b871090abb8c83457b4c5c2a1c5dfbfaed7a662045cae2919bb9f; anchor=for(const forbidden]`.
- Scoped advantage: PID-bound, no-overwrite capture with an app-owned framebuffer and automatic read-only Win32 fallback; runtime evidence is separate from source guards.

### Pealayer: `scripts/check-crate-boundary.mjs`

Verdict: **scoped-improvement**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `da9b469ac596e6eb1999e4d499742a51ebf289335285549569116c1124963e38`.

- Target: `tools/check-crate-boundary.mjs:10 [UTF8-LF sha256=4a8fbcecaf424460c051de61d5e44658532ec64527460ce21d4d86ec95c0550e; anchor=export function checkCrateBoundary]`.
- Retained: Actual donor module extraction/intersection, thin executable and MSVC duplicate resource linkage guards imported/adapted. Native build wrapper and CI execute this read-only guard before compiling.
- Omitted/replaced code: `pealayer::startup::run and main.rs/lib.rs module intersection assertions`. Disposition: **replaced**. Reason: Pakar has one binary today. The adapted tool detects optional lib.rs and then enforces zero executable modules, module ownership and delegation to Pakar's library namespace. Today's single binary additionally rejects duplicate declarations without requiring a nonexistent Pealayer startup symbol.
- Omitted/replaced code: `MSVC winres library/bin duplicate resource.lib guard`. Disposition: **replaced**. Reason: Both original resource.lib patterns are retained and run for today's binary as well as a future shared-library layout. No need to defer these guards until the architecture changes.
- Test-code evidence: `tests/crate_boundary_test.mjs:4 [UTF8-LF sha256=ebf1cbc5410a3f67a7b704d17415801e68daeba961253f8b60a183fdb5fec17f; anchor=const single =]`.
- Scoped advantage: Executes useful donor guards immediately on the current single-binary layout, rejects duplicate declarations, and automatically activates library ownership/thin-entry checks when a library appears. No compiler or generated fixture writes.

### Pealayer: `scripts/libmpv-windows.ps1`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `ab6e48f3e0a55a2d86ad1dbb19c01a23c751891cb5a7ed61875a21b1e52645e5`.

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

Verdict: **equivalent-file**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f`.

- Target: `tools/package-probe-windows.ps1:1 [UTF8-LF sha256=f4516884990ad76c020fbc6534d902c4949abae0b48da8ea40c008afa44dfd4f; anchor=function Invoke-PackageProbe]`.
- Retained: Byte-identical actual donor process-handle probe: no-shell hidden process, async redirected stdout, 30-second wait, explicit exit code, output return and Dispose. Optional staging probe calls this helper.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `tests/reuse_preflight_test.ps1:87 [UTF8-LF sha256=842c7d0149c2717f7378116063712f4b719941152d89bd43cdc31e60b148437d; anchor=Actual imported short-lived process stdout]`.
- Remaining: Donor timeout throws without terminating its child. Full GUI/runtime package smoke remains separate from CLI identity.

### Pealayer: `scripts/package-windows.ps1`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `3e6f8a82456329558c7ec24e596f353c75d39680d8807d6eb0261bd419e5025a`.

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

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `013e5c67e227ed851f8a3d26b27654e38c8816edca2965381bbb1b48d10a750d`.

- Target: `tools/build-desktop.ps1:21 [UTF8-LF sha256=14cd626601e9c3aad7b8d9913abe53078a9185e9bad10052c3cf0ff2e1c41fe6; anchor=Get-PakarCargoTargetDirectory]`.
- Retained: Shared resolver call and locked native build flow mapped to Pakar wrapper. Existing canonical cache is reused; explicit Check/Test/Release and storage/reuse gates avoid unnecessary links.
- Omitted/replaced code: `RustBin machine/user/toolchain executable fallback selection and rustc -vV host detection`. Disposition: **pending**. Reason: Pakar currently uses installed Cargo from PATH. Alternate host/toolchain executable fallback and recorded host triple are reusable infrastructure and remain unported.
- Omitted/replaced code: `libmpv host resolution/environment/profile and DLL copy`. Disposition: **not-applicable**. Reason: These operations configure the excluded mpv renderer. Pakar uses its own mpv-free linked dependencies.
- Omitted/replaced code: `BuildOnly versus launch forwarding with ApplicationArguments`. Disposition: **pending**. Reason: Pakar wrapper builds only. Generic built-executable validation and configurable run forwarding should become a Pakar runner; manual launching is not equivalent.
- Omitted/replaced code: `Set-Location to repository root`. Disposition: **replaced**. Reason: Absolute manifest and resolved target paths preserve caller cwd without introducing a second cache.
- Test-code evidence: `tests/build_storage_test.ps1:15 [UTF8-LF sha256=7fb730f4913809a4252404c97e5d73c21d9c9efed6ec794df48715b38c152590; anchor=One canonical target]`.
- Remaining: Import generic Windows runner/toolchain fallback and host-triple provenance without mpv.

### Pealayer: `scripts/storage-hygiene.ps1`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `0d8fdeb3e15fb52b917784d4cb3875c68bc12c6c6ed78cd16f2ff8ee3561d89c`.

- Target: `tools/storage-hygiene.ps1:11 [UTF8-LF sha256=0d1af57a83775e2670a5ac222973970dfc3bfc2c2e1bde1a1753523b444bdb5c; anchor=function Get-DirectorySummary]`.
- Target: `tools/build-mobile.ps1:45 [UTF8-LF sha256=449eb0b4949d19cb10e4c9ef9e22c34cc5d266df770879c499a8b82aa19e03cc; anchor=$taskCache=Get-DirectorySummary]`.
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

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `48507f6e9395c8c312c5557db729d05ed8d6a83e980d57a9b01e77ea525db287`.

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

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `1f173bef8acc2904eac5b0a91407aa4f7629ce65c88ea3a8ac78f8c13321d203`.

- Target: `desktop/src/ui/shell.rs:393 [UTF8-LF sha256=412b52fc56b9958c091a0e445544ecee6a36abe63f68c915bba7f4659966e75e; anchor=egui::Key::F11]`.
- Retained: Default Preferences Ctrl+, and device configuration Ctrl+Shift+, navigation exist; F11 fullscreen is restored with the shared typed dispatcher. These are not equivalent to the donor persisted rebinding model.
- Omitted/replaced code: `ApplicationShortcuts persisted fields, parse_shortcut, entries, validate, action_for_event and their non-media tests`. Disposition: **pending**. Reason: The foundation hardcoded its accelerators and omitted the reusable binding model. No accepted reason justifies leaving rebinding out. The actual donor depends on KeyChord and hardware_shortcuts match/key-code helpers, which must be ported and tested with controller-specific typed actions rather than guessed labels.
- Omitted/replaced code: `take_frame_step_shortcut and frame-step tests`. Disposition: **not-applicable**. Reason: These consume Ctrl+Left/Right for media frame stepping and timeline transport. Those business domains were explicitly removed; Pakar must not hijack ordinary text word navigation for nonexistent video frames.
- Omitted/replaced code: `containing_media_folder and local-media path tests`. Disposition: **not-applicable**. Reason: This resolves media files/streams, not board identities or firmware files. Firmware selection already uses explicit native file dialogs; introducing a nonexistent media folder action would be misleading.
- Omitted/replaced code: `media_information and media_folder accelerators`. Disposition: **pending**. Reason: The media operations cannot be retained as-is, but their reusable information/folder-navigation affordances need explicit hardware-monitor and application-folder action decisions. They are not silently treated as completed controller mappings.
- Remaining: Restore validated persistent rebinding, exact-modifier matching, repeat/release rejection and text-edit protection; separately reconcile global/hardware shortcut ownership.

### Pealayer: `src/branding.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `be44ca91dfb08ce78fb377ccfcc4676676c05e6b36ed319c497365788ab264fe`.

- Target: `desktop/src/branding.rs:63 [UTF8-LF sha256=a16e879c4b7fba1154584e51822dba5bc2bb48ba9946270fb9b25d129e48bd18; anchor=pub fn icon_data_from_bytes]`.
- Target: `desktop/src/brand_image.rs:25 [UTF8-LF sha256=ab71607dcde16d7af14b0a292982f902186dacf6b5b78b3f4c983e37464428e9; anchor=pub fn native_icon_bytes]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `desktop/src/branding.rs:8 [UTF8-LF sha256=a16e879c4b7fba1154584e51822dba5bc2bb48ba9946270fb9b25d129e48bd18; anchor=pub fn sync_peer]`.
- Target: `mobile/lib/main.dart:380 [UTF8-LF sha256=771361f85f85fade6d4fdd7e9532752249d4724482782bfd7b7ef1913a58b5b8; anchor=peer.application?.icon ?? bundledApplicationArtwork]`.
- Target: `mobile/lib/application_artwork.dart:2 [UTF8-LF sha256=a1ee8fc15b0aa9111a5c416fcf74b3d05093b22e47b76d7744837f175f7b5ef4; anchor=const applicationArtworkBase64]`.
- Retained: Actual icon_data_from_bytes imported unchanged; one embedded PNG supplies native/Web artwork without disk caches.
- Retained: Actual native_icon_bytes seven-frame ICO encoding/master preservation/Lanczos3 resampling and shell derivative test adapted to shared supplied bytes and PNG-only decoder.
- Retained: Actual sync_native_window_icon source-signature cache, multi-viewport icon publication and tray refresh lifecycle adapted to fresh selected-peer Application metadata; no per-frame decode or disk cache.
- Retained: Flutter now shares the exact embedded native/Web PNG fallback instead of a different Phosphor glyph, with bounded peer-application title and inline PNG overrides and default-artwork decode fallback. Android launcher equivalent and runtime icon preferences remain pending.
- Omitted/replaced code: `resolved_icon_path/state_env/non_empty_path, builtin presets, icon_bytes precedence/fallback and override/preset tests`. Disposition: **pending**. Reason: Useful runtime preferences need a bounded shared board/client transport; local filesystem media-state paths cannot be copied blindly to ESP.
- Omitted/replaced code: `sync_native_window_icon, icon_source_signature/revision metadata/multi-viewport/tray sync and tests`. Disposition: **replaced**. Reason: sync_peer now adapts the cache/multi-viewport lifecycle to peer Application metadata, shared header/About and tray artwork/Show label. Native runtime acceptance is still blocked by the cache guard; full icon preferences/tests remain open.
- Omitted/replaced code: `PlaybackIconState/from_player/playback_state_is_unambiguous`. Disposition: **not-applicable**. Reason: Video state business expressly removed; hardware state icons require a separate validated state contract.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=99621b44a961771053066590ce43759c24bdd2c0993de0d7c36ae615a2c5a2ad; anchor=Actual Web application identity]`.
- Remaining: Complete named generic omissions with preference/endpoint/runtime tests; source checks are not deployed acceptance. See BRANDING-PARITY.md.

### Pealayer: `src/messaging.rs`

Verdict: **equivalent-file**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573`.

- Target: `desktop/src/messaging.rs:81 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=pub fn publish(]`.
- Retained: Byte-identical MessageHub, validation, bounded queue, expiry/upsert/dismiss/revision/source identity, and both donor tests.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/messaging.rs:171 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=fn upsert_dismiss_and_expire_are_shared()]`.
- Remaining: This file equivalence does not establish Web/TUI/IPC notification-surface parity.

### Pealayer: `src/platform/windows_quick_actions.rs`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `35f6de708a0e402f6958a4d8feeead5037b42174286050ddc98c494d4173e4cb`.

- Target: `desktop/src/platform/windows_quick_actions.rs:15 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=pub const WINDOWS_QUICK_ACTIONS]`.
- Retained: Actual typed action/resource catalog, equality traits, explicit negative resource-index selection, Preferences, Exit and generic Toggle fullscreen. Controller actions share the launch/IPC dispatcher and resource compiler.
- Omitted/replaced code: `Play / Pause, Previous chapter, Next chapter, Mute / Unmute media quick actions`. Disposition: **not-applicable**. Reason: Pakar intentionally has no mpv playback, chapters or media volume. These would call removed media business methods. Connect, device settings, refresh, workspaces and About are real controller navigation actions instead; generic fullscreen is retained, not misclassified as media-only.
- Test-code evidence: `desktop/src/platform/windows_quick_actions.rs:69 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=fn controller_shell_actions_have_unique_native_resource_ids]`.
- Remaining: Visible Windows Jump List clicks remain a manual acceptance gate; typed launch forwarding and resource uniqueness are tested.

### Pealayer: `src/platform/windows_shell_icons.rs`

Verdict: **equivalent-file**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a`.

- Target: `desktop/src/platform/windows_shell_icons.rs:45 [UTF8-LF sha256=e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a; anchor=pub fn shell_icon_ico]`.
- Retained: Byte-identical actual Phosphor rasterizer, centered glyph coverage, all seven ICO resolutions, bottom-up BGRA, DWORD-aligned AND masks and explicit directory offsets. Pakar build.rs keeps it for semantic quick-action assets; application PE/window/tray artwork now uses the shared master and actual Pealayer branding conversion.
- No production-code omission claimed for this reviewed file.
- Remaining: User-configurable icon packs and runtime state-icon transitions are a separate unresolved integration; retaining the generator does not implement that editor.

### Pealayer: `src/ui/board_info.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `c252864d80912a03bca718b0a13d721fda2e8f7378c033f44619590a82380fd2`.

- Target: `desktop/src/capabilities.rs:7 [UTF8-LF sha256=8686e5df23a2a048c353bdc8a632de646674741586263217c1e40abb068298ec; anchor=pub struct Manifest]`.
- Target: `desktop/src/ui/shell.rs:275 [UTF8-LF sha256=412b52fc56b9958c091a0e445544ecee6a36abe63f68c915bba7f4659966e75e; anchor=fn synchronize_context(]`.
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

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `4ffdd81afbaa2f918cb572f11d1ec978b279ca0e263f9bfad83e99a59bc27781`.

- Target: `desktop/src/ui/cell_table.rs:363 [UTF8-LF sha256=88d23430ba97a525706fae20a108ffec6a8ab2d8485be6eaad2459bd5de45c3a; anchor=fn manager_selector(]`.
- Target: `firmware/esp8266/src/WebUi.h:179 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=/* Fixed manager slots:]`.
- Retained: Read all 952 donor lines through EOF. navigation_control's fixed allocation, clipped centered text and inside-stroke painting are adapted to manager_selector. Compact cell/row/state slots use the existing shared neutral animated cell_surface; state-dependent Button Frames cannot resize their text/content geometry. Selected widget metadata is retained explicitly.
- Omitted/replaced code: `navigation_control's 80px minimum, left-aligned icon/title/metadata slots and selected bg_fill tint`. Disposition: **replaced**. Reason: A compact cell needs a centered number in 32x28; row/action slots are 96/104x28. The user requires neutral shared selection, not a tinted navigation side rail. Fixed text positions and inside strokes are preserved; existing side-rail helper remains unchanged.
- Omitted/replaced code: `singleline_text_edit/workspace_overlay_layer/preferred_host/centered_default_rect/bounded_geometry/opaque frame/action buttons/action rows/navigation detail/section/compact_row/setting_row/scroll_column/fit_scroll and their donor tests`. Disposition: **pending**. Reason: Existing imported dialog helpers are present, but this scoped geometry fix does not certify each call site or host/resize/action/localization test. Useful generic code is not excluded; full consumer and test reconciliation remains issue #3 work.
- Omitted/replaced code: `numeric_stepper/numeric_adjustment/adjust_numeric/numeric_slider_wheel/parse_numeric_paste/receive_numeric_paste/numeric_context_menu and seven pointer/paste/wheel tests`. Disposition: **pending**. Reason: Helpers already exist in the imported module; bounded input geometry, paste ownership, wheel tail suppression and context behavior need their actual donor runtime tests restored and controller settings coverage. Hover selector acceptance is not numeric-control parity.
- Test-code evidence: `desktop/src/ui/cell_table.rs:1171 [UTF8-LF sha256=88d23430ba97a525706fae20a108ffec6a8ab2d8485be6eaad2459bd5de45c3a; anchor=fn manager_hit_slots_text_and_wrap_never_follow_hover_or_stroke_width()]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:11 [UTF8-LF sha256=420b8dfb17544ca9aa53204045d2e2b9fff993c98a225fc5eddb4a99adff5feb; anchor=Manager must not allocate state-dependent selectable Frames]`.
- Remaining: Execute the added actual native painter test at narrow/wide widths with animation on/off, then capture/deploy through guarded workflows. Native Test is currently blocked by the canonical >8 GiB cache; Web checks are source/DOM-double evidence, not browser pixels or physical deployment.
- Remaining: Complete the named generic dialog call-site/test reconciliation in issue #3; no whole-file behavioral equivalence or whole-project superiority claim.

### Pealayer: `src/ui/i18n.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `811f1e08e7126932772c49da8aa9669ce8f47d4e2208c1ef6cde435dde20bdf4`.

- Target: `desktop/src/ui/i18n.rs:6 [UTF8-LF sha256=f44d0302f729f7e724b2d02c1ce1f94cf5915a1e7f111b911a68a63b8bbbac24; anchor=pub(crate) fn configure_ui_fonts]`.
- Target: `desktop/src/ui/dialog.rs:18 [UTF8-LF sha256=b3786964dd0c375f0f73325a4046b3fc0e3967ace4e7e0c4f66b6f52d66eda47; anchor=pub fn singleline_text_edit]`.
- Target: `desktop/src/ui/preferences.rs:102 [UTF8-LF sha256=545ba8ac309c796b1bd1dfd70eb7ca085f422e03b92fa4a5628906e723d49d1f; anchor=AppLanguage::Persian]`.
- Target: `docs/PERSIAN-PARITY.md:48 [UTF8-LF sha256=d1f873fb589b406316d77e9ca2fcaf652fe8773b1534063504fc1ab516b506da; anchor=## Exact source-to-target mapping]`.
- Retained: Actual donor platform/UI face loading, Vazirmatn and Phosphor registration, application dictionary, identity-preserving visual_text, script detection, input alignment and horizontal/vertical direction helpers retained and wired into real native controls.
- Retained: Monospace Persian fallback and preference-driven priority refresh added without discarding later widget font registrations. Actual donor contextual glyph test adapted to both families; saved Unicode and peer identifiers never manually reversed.
- Omitted/replaced code: `Media playback, cue/timeline reveal and keyframe-specific dictionary keys`. Disposition: **not-applicable**. Reason: They describe explicitly excluded media business; no corresponding controller surface exists. No generic UI/tooling capability is excluded on that basis.
- Omitted/replaced code: `Manage folders, Manage folder, New folder, Expand folders, Collapse folders, Move to folder, Delete folder, Confirm delete, Channels will be moved to Ungrouped, pending folder change and attached-board folder configuration copy`. Disposition: **pending**. Reason: Generic grouping behavior is useful but not yet integrated with actual board cell/group contracts. No fake folder actions or completion claim; retain on issue #4 for real grouping and localized acceptance.
- Omitted/replaced code: `Separate ttf_parser coverage assertions and exhaustive native surface translations`. Disposition: **pending**. Reason: Avoid introducing a duplicate font parser dependency: actual renderer glyph/contextual and font-retention tests pass in 8849ab8 Windows CI after headless output repair. The run completes 114 passed/1 unrelated menu-label failure; no abort. Additional Unicode, production rendering, device-settings and status/transport copy remain unaccepted.
- Test-code evidence: `desktop/src/ui/i18n.rs:1434 [UTF8-LF sha256=f44d0302f729f7e724b2d02c1ce1f94cf5915a1e7f111b911a68a63b8bbbac24; anchor=fn contextual_persian_in_labels_and_monospace_inputs]`.
- Test-code evidence: `desktop/src/ui/i18n.rs:1396 [UTF8-LF sha256=f44d0302f729f7e724b2d02c1ce1f94cf5915a1e7f111b911a68a63b8bbbac24; anchor=fn changing_language_preserves_widget_fonts]`.
- Test-code evidence: `tests/localization_test.mjs:40 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Peer text identical to a dictionary key]`.
- Remaining: Inspect real deployed Persian inputs/menus/dialogs and finish application-copy/generic grouping parity. Actual 8849ab8 font-retention/shaping tests pass, but full native release remains open with one menu-label assertion; local cache budget remains enforced.

### Pealayer: `src/ui/icons.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `fba85edd503798868acc5527600b2ee52326339816c5b73e9926d1d7b0929384`.

- Target: `desktop/src/ui/icons.rs:567 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=pub fn searchable_workspace_icon_picker(]`.
- Target: `desktop/src/ui/shell.rs:1791 [UTF8-LF sha256=412b52fc56b9958c091a0e445544ecee6a36abe63f68c915bba7f4659966e75e; anchor=icons::searchable_workspace_icon_picker]`.
- Retained: Production Phosphor vocabulary, stable names/kind fallback, searchable list/grid picker, UTF-8-safe match highlighting, bounded popups, disclosure/submenu helpers.
- Retained: Actual donor regression tests restored: eleven tests, including interactive egui-frame search/select/reopen, both themes and popup width.
- Omitted/replaced code: `every_native_icon_editor_uses_the_shared_searchable_picker donor-only editor path list`. Disposition: **replaced**. Reason: That assertion references Pealayer media/4D editor files not present in this product. Replaced only its call-site assertion with Pakar's actual workspace editor; the other ten tests were imported. This does not excuse missing generic editor features elsewhere.
- Omitted/replaced code: `DropdownUiExt, dropdown_choice, register_choice and dropdown::ComboBox in searchable icon pickers`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `desktop/src/ui/icons.rs:825 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=fn pakar_workspace_editor_uses_the_actual_shared_searchable_picker()]`.
- Test-code evidence: `desktop/src/ui/icons.rs:954 [UTF8-LF sha256=eb09707f6334c27aa27569a3a27dc6078137be0dcc81b059ebe7df26dcc21188; anchor=fn icon_picker_search_click_filter_select_and_reopen_work_in_both_forms()]`.
- Remaining: Imported compact/control picker helpers are not all used by current panels; UI-wide application/action parity needs separate file reviews.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/menu.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `18e255a5ccc65dfa0588eac10283c7c1f5fe33ea63bb3e07bd599e30a0cf19c3`.

- Target: `desktop/src/ui/menu.rs:9 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=pub fn top_menu_button<R>(]`.
- Target: `desktop/src/ui/shell.rs:449 [UTF8-LF sha256=412b52fc56b9958c091a0e445544ecee6a36abe63f68c915bba7f4659966e75e; anchor=menu::top_menu_button(ui, crate::ui::i18n::text("File")]`.
- Target: `desktop/src/ui/connection_controls.rs:38 [UTF8-LF sha256=7ae6fa52982d23b58928cab8fd5d2019bc777b2a44be815718f9e8b3a92c21f6; anchor=pub fn websocket(]`.
- Target: `docs/NATIVE-MENU-COMPARISON.md:32 [UTF8-LF sha256=7319a38240f9c23a43f5a18c2f20d05b0c9d5ff273cf0e8f28b2ba98aa1aabd8; anchor=## Exact donor menu mapping and remaining decisions]`.
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
- Test-code evidence: `desktop/src/ui/menu.rs:69 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=fn shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes()]`.
- Test-code evidence: `desktop/src/ui/menu.rs:192 [UTF8-LF sha256=485783deefe484f7d413e30f3020dc3c2355aa564ca038c39741f6149271420e; anchor=fn sibling_hover_switches_only_an_already_open_menu_and_clears_closed_state()]`.
- Test-code evidence: `tests/native_menu_test.mjs:12 [UTF8-LF sha256=80ac61041de322c2c0e2c0fad379a74e6e89d02724f1b80f2d9fa6200c7ef937; anchor=actual donor helper must match after application ID/visibility/whitespace adaptation]`.
- Test-code evidence: `tests/native_render_lifecycle_test.mjs:27 [UTF8-LF sha256=c817865734fb041b787c6756d14ce5a8c20f47a45150871e85d8a954c15d23ce; anchor=const compact=source=>source.replace]`.
- Remaining: Restore remaining useful generic menu contracts above against actual dependencies; see exact function-level mapping in NATIVE-MENU-COMPARISON.md. Native CI paint/pointer tests and running-app visual acceptance are separate from source guards.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/palette.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `c169db6a61b79ebb2ce00f0ca442305eefeea7b34740720a2cdf6424d6fef85d`.

- Target: `desktop/src/ui/palette.rs:36 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=pub fn apply(]`.
- Retained: Native semantic visual-role mapping and actual palette JSON; appearance geometry is not overwritten.
- Omitted/replaced code: `WebAppearanceState and donor native/Web shared-preferences assertions`. Disposition: **pending**. Reason: Pakar lacks the shared native/Web preferences contract. These are useful generic requirements, not media business logic; omitting them cannot be justified as superior.
- Omitted/replaced code: `Donor default Native palette`. Disposition: **replaced**. Reason: Pakar chooses Studio as its product default. Both palettes remain available; this is a preference decision, not a measured improvement.
- Test-code evidence: `desktop/src/ui/palette.rs:101 [UTF8-LF sha256=418896294fbe25922736b41865a18dbb2f6bc96cb540c7401a435a6e0cfd38f3; anchor=fn donor_surface_text_roles_and_geometry_are_preserved_in_both_themes()]`.
- Remaining: Embedded Web consumes the same JSON catalog through a stale-generation check. Complete mobile catalog use and live native/Web preference broadcast/readback; pure donor host-resolution tests do not prove connected-host synchronization.

### Pealayer: `src/ui/status_bar.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `7004bd7e936875b4d5328f10f482aa492827117251fce69e8ebb34561ff90663`.

- Target: `desktop/src/ui/status_bar.rs:44 [UTF8-LF sha256=ac3ad2971427a8b0b16e50eb1763d87080871df2a6ba88ce238e2eef5bbf7b26; anchor=fn hide_menu(]`.
- Target: `desktop/src/ui/connection_controls.rs:59 [UTF8-LF sha256=7ae6fa52982d23b58928cab8fd5d2019bc777b2a44be815718f9e8b3a92c21f6; anchor=pub fn usb(]`.
- Target: `docs/CONNECTION-CONTROLS.md:1 [UTF8-LF sha256=002ee046e9c206a4cc241ad2b9033971b4933959d8e14f43958848403879f21d; anchor=# Front-panel transport dropdowns]`.
- Retained: Actual per-item hide context menus, persistent visibility choices, bar-level restore menu and small truncating message labels adapted in Pakar status_bar.rs. Transport controls remain usable offline without claiming hardware readiness.
- Omitted/replaced code: `current_status_message update-manager priority and OSD lifetime; fullscreen exclusion; workspace label; i18n/RTL layout`. Disposition: **pending**. Reason: Pakar retains a status message and item menu, but does not yet share authoritative update/OSD lifetime, workspace text or localized/RTL/fullscreen policy. Restore actual dependencies and test rather than claim full-file parity.
- Omitted/replaced code: `draw_hardware_status board_ready/coordinator/active_transport distinction, physical RGB glyph and telemetry voltage/current/temperature/warnings`. Disposition: **pending**. Reason: Pakar current peer freshness/ACK gates exist but generic coordinator, RGB sensor and warning inventories need actual adapters before those fields can be displayed. Native dropdown live indicators are transport state, not hardware ACK.
- Omitted/replaced code: `prepared_timeline, media-rate/fps mode, hardware timeline revision/feedback; estop_active label`. Disposition: **not-applicable**. Reason: Prepared media/timeline playback is expressly excluded from this mpv-free controller. No E-stop contract is acknowledged by the current pair; reusable latch safety remains tracked in menu comparison, not fabricated as present.
- Omitted/replaced code: `Pealayer hardcoded connection dot RGB`. Disposition: **replaced**. Reason: Pakar transport controls use its existing shared palette green/current text roles; menu state stays readable in both themes without tinting unrelated hardware controls.
- Omitted/replaced code: `UI render-rate toggle/context policy and latest dropdown-backed FPS menu; authoritative physical RGB without double dimming`. Disposition: **pending**. Reason: Live donor tracked bytes were reread completely. They now depend on another task's uncommitted src/ui/dropdown.rs, outside the tracked inventory. Pakar retains prior imported mechanisms, but does not claim that new dependency, consumer migration or runtime tests. Preserve donor work and import after exact tracked provenance and native acceptance.
- Test-code evidence: `tests/connection_controls_test.mjs:1 [UTF8-LF sha256=4824489d60f705fccda52d3166ca487a1ea4d1721d0a0814706747fb7033aaf2; anchor=Source wiring guard only.]`.
- Test-code evidence: `desktop/src/ui/connection_controls.rs:112 [UTF8-LF sha256=7ae6fa52982d23b58928cab8fd5d2019bc777b2a44be815718f9e8b3a92c21f6; anchor=fn offline_dropdowns_open_independently_and_usb_enumerates_only_when_open()]`.
- Remaining: Run native popup/pointer tests, deploy and capture actual controls; restore useful generic status lifetime/readiness/localization contracts in issues #3/#10.
- Remaining: Latest donor checkout is dirty; reviewed original-byte hash binds observed tracked source only. Audit/import the new dropdown dependency and consumers without overwriting its owner.

### Pealayer: `src/ui/toasts.rs`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `a6501d2c0f859e186cac659a37d256f26f8896e98644e9cfc8ac880da5460e54`.

- Target: `desktop/src/ui/toasts.rs:4 [UTF8-LF sha256=8772d049fab0fba597bd7f4ba4403c7d7bd122f6027442e25f32da11d8f058f6; anchor=pub fn draw(ctx:]`.
- Retained: Actual donor root-only overlay: four newest messages, severity icons and semantic palette roles, bounded width, scrollable text, title, dismiss and deadline-driven repaint.
- Omitted/replaced code: `interop::get_live_config() accessor`. Disposition: **replaced**. Reason: Pakar owns the same live palette through config::live(). The OS-native notification route is separate; adding an explicit in-app enable preference does not remove the donor overlay when enabled.
- Remaining: This native overlay mapping is not shared Web/TUI message-delivery parity.

### Pealayer: `tests/i18n_surface_audit_test.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `504768bc9cf2fda32a8dab8b0f50d573c74e45001734f45c7ff3120f36333f63`.

- Target: `tests/localization_test.mjs:47 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=Adapt Pealayer tests/i18n_surface_audit_test.rs]`.
- Retained: Actual donor known-raw-caption prohibition and required localized-boundary audit pattern adapted to real shell menu captions, not renamed timeline/media files. Added peer text exclusion and reversible placeholder/language tests.
- Omitted/replaced code: `Literal media cue/keyframe/player forbidden-string fixtures`. Disposition: **not-applicable**. Reason: Pakar has no those media surfaces; substituting their filenames would manufacture coverage. Controller-specific boundaries and font assets are tested instead.
- Omitted/replaced code: `Complete native/Web raw-caption audit coverage`. Disposition: **pending**. Reason: The current test protects selected imported boundaries; it is not exhaustive. Remaining native settings, transport/status and dynamic Web copy are explicitly pending, and native Cargo tests are blocked by cache budget.
- Test-code evidence: `tests/localization_test.mjs:50 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=for(const key of]`.
- Remaining: Expand ownership audit as each real controller surface is translated; execute actual native renderer tests and acceptance instead of treating source guards as runtime proof.

### Pealayer: `tests/windows_shell_suite_test.rs`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `d8696d8b591ca8ae4c77c78a07d19f6aca103e29906f267f19ac8ba1aa5d6e01`.

- Target: `desktop/src/platform/windows_notifications.rs:115 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=assert!(Hook::new(42).is_err())]`.
- Retained: Actual Preferences action/resource catalog is validated; invalid HWND registration rejects rather than claiming success; owner exclusion is tested. Live notification acceptance and shown-callback evidence are recorded independently.
- Omitted/replaced code: `Playback-duration error/paused percentages and Play/Pause thumbnail/tray labels`. Disposition: **not-applicable**. Reason: The tests model removed video playback, not device transfer progress. Pakar uses indeterminate taskbar activity only for a real pending settings/maintenance request; inventing a media percentage would publish false progress.
- Omitted/replaced code: `PealayerApp::default and media playback_time/duration/is_paused assignments`. Disposition: **replaced**. Reason: Pakar is an executable-only mpv-free controller; creating the donor media engine is inappropriate. Actual controller IPC/live smoke and direct invalid-HWND tests exercise its own lifetime instead.
- Omitted/replaced code: `ensure_shell_initialized retry and repeated shell-lifecycle acceptance`. Disposition: **pending**. Reason: Tray creation failures are visible and close-to-tray cannot hide an app without a tray, but complete root-taskbar initialization/retry and Explorer restart acceptance are not covered by the current tests.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:110 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=fn utf16_limits_preserve_complete_surrogates_and_termination]`.
- Remaining: Add root-taskbar lifecycle/reinitialization and hidden-window/tray/notification-click acceptance without fabricating successful HWND installation.

### Pealayer: `web_ui/scripts/appearance.test.mjs`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `35f269e9463e49dbc41572683ca374be8674a93e0e97c84240838d34dc950ddc`.

- Target: `tests/web_appearance_test.mjs:1 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=First four tests port the actual Pealayer]`.
- Retained: All four actual donor contract tests ported: Neutral helper default, preference merge without unrelated-field loss/resolution leakage, host/offline theme/accent and repeated native event/explicit-theme handling. Additional shipped-code tests cover all palette roles, cancel/apply, contrast, identical-write skip, media events, malformed storage, cross-tab updates and session-only storage failure.
- Omitted/replaced code: `volume:37 unrelated media setting fixture`. Disposition: **replaced**. Reason: The unrelated value is endpoint:http://ledboard.local instead. Its preservation and original-object immutability assertions are retained; no actual test contract is removed.
- Omitted/replaced code: `Direct TypeScript test import`. Disposition: **replaced**. Reason: Tests import the actual adapted JavaScript functions; browser tests additionally execute the shipped embedded source, avoiding a separate mock appearance engine.
- Test-code evidence: `tests/web_appearance_test.mjs:30 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=live host theme events leave System preference unchanged]`.
- Remaining: Test execution proves module and simulated browser contracts, not connected native event transport or physical hardware.

### Pealayer: `web_ui/src/appearance.ts`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `aca0c9aba5fe6389995f92306e20f62debf38fdb4ed3b3061cf9b9f17a56c430`.

- Target: `assets/themes/appearance.js:4 [UTF8-LF sha256=62a7ebba994ad44f4e2d647bb97684aac9b563104f3f707b43573289e514ecdd; anchor=export function mergeAppearance]`.
- Target: `tools/generate-web-appearance.mjs:29 [UTF8-LF sha256=bcf9cdb01e96c932d0ac2895930d52e774846d5791f40f11fd7254e52cecb18e; anchor=replaceBlock('// BEGIN SHARED APPEARANCE']`.
- Target: `firmware/esp8266/src/WebUi.h:9 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=// BEGIN SHARED APPEARANCE]`.
- Retained: All five actual donor functions retained: mergeAppearance, resolvedAccent, resolvedAppearanceTheme, accentForeground and paletteName. TypeScript annotations removed for standalone embedded JavaScript; field names, host/offline resolution, exact accent constants, custom validation and luminance threshold remain unchanged.
- Retained: Generated into the shipped WebUi.h with the shared palette asset; --check rejects stale embedding. Browser preferences dialog validates theme/palette/accent, protects drafts, handles storage failure and cross-tab/System events without device writes or repetitive identical storage writes.
- Omitted/replaced code: `AppearanceState TypeScript interface and generic T return annotation`. Disposition: **replaced**. Reason: ESP serves standalone JavaScript, not a TypeScript/React build. Actual function bodies are retained; browser-facing enum/color validation rejects untrusted stored values. No preference or resolution field is dropped.
- Omitted/replaced code: `Connected host AppearanceState broadcast/native-Web shared preference path`. Disposition: **pending**. Reason: Pure functions accept host state and tests preserve donor semantics, but standalone browser controls are browser-local today. No actual native appearance event/readback transport is integrated in this pass; System fallback is labeled browser-local.
- Test-code evidence: `tests/web_appearance_test.mjs:22 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=System theme and accent preserve donor host resolution]`.
- Test-code evidence: `tests/web_appearance_test.mjs:64 [UTF8-LF sha256=610045a01978ac3522788f612581185f9ef889edb579e237f3fc0c3fcdfb3a2e; anchor=dialog draft/cancel never changes]`.
- Remaining: Implement native/Web/mobile shared preference transport and live authoritative host events, while keeping device parameters separate from client appearance; verify multi-client broadcast/readback.

### Pealayer: `web_ui/src/components/HeaderBar.tsx`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `df84386e53d729eabddf9476026a9f96afe130dd3429612f4c73509d1757d16b`.

- Target: `tools/generate-branding.mjs:3 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor=Pakar embeds it for the standalone ESP surface]`.
- Target: `firmware/esp8266/src/WebUi.h:226 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=<!-- BEGIN SHARED BRAND HEADER -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Target: `firmware/esp8266/src/WebUi.h:747 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function renderApplicationIdentity]`.
- Target: `assets/web/device-settings.js:53 [UTF8-LF sha256=f3a38568cdd14a091d2a120d352adc40c25311c310c9f748c9e300c57eaee613; anchor=function settingsCanClose()]`.
- Target: `docs/INTERFACE-CONTRACT.md:8 [UTF8-LF sha256=bd8de673cab2e9d0c9d8948bedb0637dce7e8da10911f9a481bea0171f52ce47; anchor=## One catalog, distinct transport and platform adapters]`.
- Retained: Actual resolved-artwork img/alt binding adapted to standalone header/login, replacing diamond and font fallback.
- Retained: Firmware name/title/artwork binding replaces fixed header branding; stale/disconnected peer resets title/header/favicon/touch artwork and validates bounded inline PNG.
- Retained: Actual retained Modal/draft lifecycle adapted to grouped device settings; separate same-instance WebSocket actions preserve HTTP refresh and reject obsolete socket callbacks. Native Draft/SecretEdit/Write validation/readback semantics are reused.
- Omitted/replaced code: `Header collapse; arbitrary remote connectionTarget/draftTarget endpoint modal; localization/RTL`. Disposition: **pending**. Reason: The new grouped device-settings modal and same-instance WebSocket dropdown are not arbitrary remote endpoint selection or localized navigation. Standalone-device origin/auth and preferences adapters still need porting and tests.
- Omitted/replaced code: `capability-aware Dropdown install/update/share/fullscreen/notifications/haptics/audio/wake-lock cluster`. Disposition: **pending**. Reason: Requires actual WebPlatform integration and ownership/permission tests; not excluded media business.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
- Test-code evidence: `tests/web_identity_test.mjs:40 [UTF8-LF sha256=99621b44a961771053066590ce43759c24bdd2c0993de0d7c36ae615a2c5a2ad; anchor=Actual Web application identity]`.
- Test-code evidence: `tests/interface_settings_test.mjs:31 [UTF8-LF sha256=e2588bf230a64a1260bba5acd7ad3d2f26c8f16361e07185a1059cd210ef122e; anchor=Failed saves preserve the draft]`.
- Remaining: Complete generic omissions and missing editors named in INTERFACE-CONTRACT.md; native/mobile/deployed acceptance remains open.

### Pealayer: `web_ui/src/components/PreferencesTab.tsx`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `bada7d4b1cdd332fc51bcbc00ac59544c860f7012eca7d425e477ef932170c27`.

- Target: `mobile/lib/device_settings_page.dart:16 [UTF8-LF sha256=00d2553f6435444bfba52cbeab8d82d2dbd6a1a21109e9cb31f50785c2517d01; anchor=class DeviceSettingsPage]`.
- Target: `mobile/lib/device_settings.dart:156 [UTF8-LF sha256=44ff1181b9a8a7e47e14e91a1af650d7ccb3041da971a67178e34b2fe608be0d; anchor=class DeviceSettingsSession]`.
- Retained: Actual contract-driven sections/selected-section navigation, grouped labeled typed controls, contextual options, retained draft on section changes, descriptions, load/save status and reload adapted to Flutter DeviceSettingsPage using the existing shared Pakar catalog. Tablet uses a searchable navigation rail; phone uses wrapping section chips. Actual Phosphor repertoire supplies verified icon codepoints.
- Retained: Actual valueAtPath/updatePaths structured settings ownership adapted to separate public drafts and secret actions. DeviceSettingsSession uses the fully read canonical Pakar native/Web validation, observe/conflict and positive acknowledgment/exact public readback workflow rather than generic optimistic UI success. In-memory secrets never enter saved client preferences; one POST body per explicit operation, no mutation replay.
- Omitted/replaced code: `updatePaths optimistic autosave to /api/config and playback.preference_set`. Disposition: **replaced**. Reason: These are Pealayer state/RPC contracts, not Pakar persisted board endpoints. Password replacement and network restart require explicit confirmation, positive accepted/updated ACK and exact public readback. No optimistic physical save success or automatic write replay.
- Omitted/replaced code: `Ant Design components and ServerFilePicker transport`. Disposition: **replaced**. Reason: Android Flutter cannot import TSX/Ant Design components directly. Actual section/group/control logic is adapted to Material controls with the shared Studio palette, Vazirmatn and verified Phosphor glyphs. Mobile firmware now uses the fully read ServerFilePicker selection/ownership contract with native Android SAF; exact mapping and remaining generic consumers have their own comparison row.
- Omitted/replaced code: `AccentColorPicker, color swatches, appearance live preview, NumericValueControl steps/defaults, multi_select rich options, replacement_list add/delete, Collapsible groups, ServerFilePicker and refreshAudioOutputs two-stage option refresh`. Disposition: **pending**. Reason: Device Wi-Fi/cloud/identity/access editors are implemented, not the entire application preference framework. Generic appearance, collection editors, defaults/step controls, reusable file picking and dynamic options remain useful and require actual client/device capability contracts and tests before importing their controls.
- Omitted/replaced code: `tr(locale, ...) complete control/description localization`. Disposition: **pending**. Reason: Vazirmatn is loaded and the actual RayanLamp digit map normalizes numeric fields, but the new settings copy is English. Full mobile locale/direction must be audited and rendered before claiming Persian preference parity.
- Omitted/replaced code: `timelineWheelPreferences merge, playback audio-output RPC and media-extension preference consumers`. Disposition: **not-applicable**. Reason: Timeline/cue/media playback business was explicitly excluded from Pakar. Their generic dynamic-option and file-picker mechanisms remain pending above; they are not excluded as media business.
- Test-code evidence: `mobile/test/device_settings_test.dart:90 [UTF8-LF sha256=69d953899f8e2bf1c617811933bbc03393590b5a794882e8e4ea3b6fcf7f08bb; anchor=Settings write $behavior: one POST]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:249 [UTF8-LF sha256=31f995cacf52ea88eb50fd2aabb912c040c75c74f2bc403c55d551491568f3d5; anchor=Acknowledged $route handoff after old view closes]`.
- Test-code evidence: `mobile/test/capture_test.dart:121 [UTF8-LF sha256=e1a4512a8697c0ffaf37d5018cbd19d56e7430b9ee944e1664ee112c5963b28b; anchor=Capture must never issue a device mutation]`.
- Remaining: Mobile firmware/update/backup and Android picker workflows are implemented in the separate ServerFilePicker row; physical acceptance remains. Complete connection/discovery controls, unsaved Android-back/quit protection, client appearance/localization, generic typed preference controls and applicable donor dependencies. Verify physical phone/tablet configuration readback and hostname/credential handoffs; current API lacks transactional compare-and-set and encrypted HTTP transport.

### Pealayer: `web_ui/src/components/ServerFilePicker.tsx`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `9fef787ded9a20c0efa58a3dd8ed69e9e9fb1fc0cbeac7304490862389882071`.

- Target: `mobile/lib/documents.dart:17 [UTF8-LF sha256=9cd92317a5407799c79882fb1aef15dca128adf6291887a3a12ce6ee1fabc5a3; anchor=class AndroidDocuments]`.
- Target: `mobile/android/app/src/main/kotlin/ir/digitalogic/pakar/pakarcontroller/MainActivity.kt:35 [UTF8-LF sha256=013f73929d124fa0b4125ea2bf60e24544a08b1f5bb02f51d2e8d75f217562f0; anchor=Intent.ACTION_OPEN_DOCUMENT]`.
- Target: `mobile/lib/firmware_page.dart:10 [UTF8-LF sha256=be11fe1304b15a04da43e9c7d268f40afedc12bc406a1dd2a7d631a7da72384b; anchor=class FirmwarePage]`.
- Target: `mobile/lib/firmware.dart:116 [UTF8-LF sha256=5ca918eba5a07245bf118baed97f01eea32b6cabcfb4c5b5e54fb5b1443dd265; anchor=class FirmwareController]`.
- Target: `mobile/lib/api.dart:106 [UTF8-LF sha256=d8838b217264525d31a799b08190f5abdc93271e8b18aeddc35ae8a3efdb759a; anchor=Future<Uint8List> maintenanceBytes]`.
- Retained: Fully read actual typed single-file selection, disabled/unselected/loading state, extension filtering, cancellation, stale-result protection and explicit acceptance. Adapted into Flutter AndroidDocuments + native SAF; firmware imports are validated and separately confirmed before any upload. Parent navigation is locked during maintenance confirmation/picking and writes; selected bytes never automatically program either board.
- Retained: Canonical Pakar desktop maintenance and actual firmware routes were read, not fabricated mobile endpoints. FirmwareController pins remote job/boot/hash/targets and verifies a fresh identity before apply/cancel; single-dispatch bounded HTTP refuses redirects/replay. FirmwarePage gates each ISP/OTA domain on individual live feature ACK, distinguishes stage/apply/completion, and exposes real full flash/EEPROM exports.
- Omitted/replaced code: `React/Ant Design modal/table row handlers, server /api/fs/browse, Directory.path/parent_path and AbortController network browse`. Disposition: **replaced**. Reason: Android SAF owns user-granted document navigation, search, single selection and cancellation. ESP has no arbitrary host filesystem browse contract; inventing that endpoint or accepting Windows paths would be incorrect. The native picker reads bounded bytes on its executor; captured board ownership is checked after picker/HTTP awaits. Only a one-use user-created destination token can be exported, with no broad storage permissions.
- Omitted/replaced code: `initialFile parent path, remembered folder, custom icon table/size labels and full localized picker controls`. Disposition: **pending**. Reason: SAF system navigation is available, but mobile app-owned retained-folder hints and a complete localized review/preview workflow still need actual provider/phone tests. Donor custom server file-browser behavior remains useful for native/Web preference consumers; this Android adaptation is not whole-app picker parity.
- Test-code evidence: `mobile/test/documents_test.dart:37 [UTF8-LF sha256=2d9de6807621a79e043b1e1240afb639e26ea30dc0e0084f42f232e5e4025c8c; anchor=Changed owner while choosing export]`.
- Test-code evidence: `mobile/test/firmware_test.dart:234 [UTF8-LF sha256=1e6fe3ec8a66cabd7a94ef7babf4b7d5f9592b59ed1c20b6af891ddc3d9c83d0; anchor=Lost mutating GET probe]`.
- Test-code evidence: `mobile/test/firmware_page_test.dart:103 [UTF8-LF sha256=c1d686260c263f25ceb5a4a861494298d4c6fe4f6524f7e7a1d73fbe40e59b3f; anchor=Cancelled file selection]`.
- Test-code evidence: `mobile/test/capture_test.dart:126 [UTF8-LF sha256=e1a4512a8697c0ffaf37d5018cbd19d56e7430b9ee944e1664ee112c5963b28b; anchor=Firmware capture must never issue a device mutation]`.
- Remaining: Verify Android provider cancellation/rotation/process death and real import/export; actual board update/readback/power-loss acceptance and production APK signing remain open. Generic donor file-browser consumers and full locale/direction still require review. Shared manual AVR staging has no advertised hash/CAS; pinned remote job is preferred and manual limitation is explicit.

### Pealayer: `web_ui/src/i18n.ts`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `331b0ef10c89f4e695101cb332e62607e15ce37bd0543104453060c0e45a66e6`.

- Target: `assets/web/localization.js:19 [UTF8-LF sha256=67b413df8ceaeaff6cb2b3724ac457a97ca7206c47887b15a46ae555a19d5eab; anchor=function uiTr(text,values)]`.
- Target: `tools/generate-font-assets.mjs:41 [UTF8-LF sha256=729da9321a12ee07e1ed0433f8a401b8c20d2d0281dc32deba1072f46f69540e; anchor=const dictionary={}]`.
- Target: `firmware/esp8266/src/WebUi.h:597 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=// BEGIN SHARED LOCALIZATION]`.
- Retained: Actual donor fa-dictionary fallback semantics reused for application copy, rather than translating arbitrary runtime data. Applicable native dictionary subset generates into the canonical Web module and actual firmware HTML.
- Retained: Added system/explicit language and Auto/LTR/RTL preferences, owned-template text/placeholder/accessibility snapshots, per-browser low-write persistence, shared font route and immutable peer-name boundary.
- Omitted/replaced code: `Media-library, player, playback, cue/timeline and libmpv-specific Web dictionary/surfaces`. Disposition: **not-applicable**. Reason: Those surfaces belong to excluded media business and would add unrelated controls, not controller capabilities.
- Omitted/replaced code: `Remaining generic Web copy and native/Web/mobile preference synchronization`. Disposition: **pending**. Reason: The actual controller subset is integrated, not the entire donor Web feature set. Dynamic errors/confirmations/status/menu attributes and narrow/mobile runtime acceptance remain; browser locale is not silently written to another client or firmware.
- Test-code evidence: `tests/localization_test.mjs:43 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=No identical storage writes]`.
- Test-code evidence: `tests/localization_test.mjs:41 [UTF8-LF sha256=c410cf90ffb005bd7a62735ca70711407f643bca9c81411e00e04e98fbcabcab; anchor=assert.equal(attrs.placeholder]`.
- Remaining: Finish all dynamic application-copy surfaces and controller-safe shared preference contracts; execute live-device and mobile-width/Android locale tests.

### Pealayer: `web_ui/src/preferenceFiles.ts`

Verdict: **equivalent-behavior**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `4580d1396b36f5a907cad6cf2f33a38486c3a7e6948a3bb222974e1b8a2a4af4`.

- Target: `mobile/lib/documents.dart:4 [UTF8-LF sha256=9cd92317a5407799c79882fb1aef15dca128adf6291887a3a12ce6ee1fabc5a3; anchor=bool preferenceFileAllowed]`.
- Retained: Actual case-insensitive dot-extension suffix rule translated directly to Dart preferenceFileAllowed and used by the real Android firmware picker. Additional bounds/name and image-format checks do not treat filename as authenticity.
- Omitted/replaced code: `preferenceFileParent path regex`. Disposition: **replaced**. Reason: SAF content URIs are provider-managed identities, not slash-delimited filesystem paths. Android owns the directory picker; deriving a parent by string slicing would select the wrong resource. Host/server parent navigation remains represented in the ServerFilePicker pending row, not silently excluded.
- Test-code evidence: `mobile/test/documents_test.dart:7 [UTF8-LF sha256=2d9de6807621a79e043b1e1240afb639e26ea30dc0e0084f42f232e5e4025c8c; anchor=Actual donor extension rule is case-insensitive]`.
- Remaining: Physical Android document-provider verification is separate from exact helper equivalence.

### Pealayer: `web_ui/src/webPlatform.ts`

Verdict: **parity-gap**. Revision: `33448d7a9aa6652b556554f8d686dff0cac90c3c`. Original-byte SHA-256: `23e8fd2a8f17d296f14b81d6618cbd166129e116e36515444239edac15f89554`.

- Target: `tools/generate-branding.mjs:17 [UTF8-LF sha256=b59b6a8fd73dad45d4015492d44460e9d4d3b78137aa567bebf13c3798f38ffc; anchor='FAVICON':]`.
- Target: `firmware/esp8266/src/WebUi.h:140 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=<!-- BEGIN SHARED BRAND FAVICON -->]`.
- Target: `docs/BRANDING-PARITY.md:60 [UTF8-LF sha256=0bfa8122dc9d6034551304f2fed472ddebcff170e14a23f63011177aab75d2d3; anchor=## Explicit unfinished useful code and reasons]`.
- Retained: Same resolved artwork binds favicon/touch/header before login; ESP embeds data instead of requesting a protected host filesystem icon.
- Omitted/replaced code: `storedBoolean/supportsStandaloneMode/useWebPlatform capabilities/events; install/share/toggleFullscreen/enableNotifications/applyUpdate; settings setters/signalInteraction`. Disposition: **pending**. Reason: Generic browser capabilities/lifecycle/preferences need controller-specific integration and tests; static branding does not certify them.
- Omitted/replaced code: `registerPealayerServiceWorker/update/controllerchange, wake-lock cancellation/visibility and offline badge`. Disposition: **pending**. Reason: PWA manifest/high-resolution artwork/cache ownership and streaming-specific wake-lock state remain open.
- Omitted/replaced code: `mediaTitle/current_video state icon resolution, MediaSession video metadata/play/pause/seek/chapter handlers`. Disposition: **not-applicable**. Reason: Video title/track/session business is excluded; generic browser capabilities are still pending.
- Test-code evidence: `tests/branding_test.mjs:12 [UTF8-LF sha256=c956b89b2363c8770bc345f7132822c383a8329b76375556caf71da48d39fe24; anchor=actual Pealayer icon_data_from_bytes import]`.
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
- Target: `mobile/lib/cell_search.dart:5 [UTF8-LF sha256=470ed85cf8b5ed034f63bc4bc84b415fd8d943803880e142e62396826f4481fe; anchor=String latinDigits]`.
- Target: `mobile/lib/device_settings_page.dart:459 [UTF8-LF sha256=00d2553f6435444bfba52cbeab8d82d2dbd6a1a21109e9cb31f50785c2517d01; anchor=int.tryParse(latinDigits(value))]`.
- Retained: Actual Persian/Arabic-Indic-to-ASCII maps adapted into Rust, JavaScript and Dart and invoked by native/Web/mobile cell search and actual mobile port/telemetry fields. Canonical IDs/ranges stay ASCII; peer names, secrets and protocol identifiers are not globally transformed.
- Omitted/replaced code: `ShapePersianDigits visual decimal shaping`. Disposition: **pending**. Reason: Search needs ASCII normalization now. Apply visual shaping only to audited application-owned numeric labels, not arbitrary peer text or wire IDs. Display consumers remain pending.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:28 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=latinDigits('۰۱۲۳۴۵۶۷۸۹]`.
- Test-code evidence: `mobile/test/cell_search_test.dart:15 [UTF8-LF sha256=1fce7766710c10169453820ae4a6d39ba8625149ea0181989fccc6689b109bce; anchor=RayanLamp numeric normalization]`.
- Test-code evidence: `mobile/test/device_settings_page_test.dart:209 [UTF8-LF sha256=31f995cacf52ea88eb50fd2aabb912c040c75c74f2bc403c55d551491568f3d5; anchor=expect(api.cloud['mqtt_port'], 8883)]`.
- Remaining: Finish owned numeric display shaping and execute guarded native digit/search tests.

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
- Target: `tools/generate-web-appearance.mjs:31 [UTF8-LF sha256=bcf9cdb01e96c932d0ac2895930d52e774846d5791f40f11fd7254e52cecb18e; anchor=replaceBlock('// BEGIN BROWSER CONTROLLER']`.
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=<script data-pakar-presentation>]`.
- Retained: Actual safe validated storage read, requested/OS theme resolution, root color-scheme and chrome metadata, locale/direction and resolved-locale pre-paint behavior are adapted into the single head bootstrap. It runs without dialog/body nodes, requests, preference writes or application/peer title mutation; generator checks canonical equality.
- Omitted/replaced code: `Separate same-origin /theme-init.js, duplicated theme resolver, lamp colors and rayanlamp.web.locale auto/language-list some(fa) selection`. Disposition: **replaced**. Reason: Inline canonical bootstrap shares the existing Pealayer helpers and palette once, before CSS, saving an ESP endpoint and extra request. Existing Pakar locale keys use system/en/fa, primary browser language and explicit Auto/LTR/RTL override, consistent with current runtime resolver. Supporting a secondary Persian language must not unexpectedly override an English primary choice. Existing dark/Studio defaults are retained rather than silently changing user appearance.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:55 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=Pre-paint survives blocked storage]`.
- Test-code evidence: `tests/rayanlamp_theme_test.mjs:41 [UTF8-LF sha256=f1ca3a14805c3b186d52918164fe968a6da07319dba15299a10deee658480783; anchor=RayanLamp pre-paint bootstrap runs before CSS/body]`.
- Remaining: Post-bootstrap generic dynamic copy, browser language-change lifecycle and complete native/Web/mobile localization remain pending in the existing localization concern; this import certifies pre-paint behavior, not those separate source files.

### RayanLamp: `webui/src/api.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1b201ac38c3ca4f45dea639d198204f294c83f7e07fb511be99b3d54154fc741`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=b3ff00c8b1996d5b00675faf6a64fe8313d2f85525330f7cb2596ec16554eaee; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:859 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=async function refreshAuthenticated(options={})]`.
- Retained: Fully read actual client API. request's AbortController, default 12000 ms timeout and finally timer release adapted to reconnect preparation only; signal passes through actual refreshAuthenticated/api status GET.
- Retained: Pakar cookie authentication and expired-session login are retained; no uncertain mutating request/session replay, credentials or raw endpoint/body diagnostics copied.
- Omitted/replaced code: `establishSession/requestSession, CSRF header and request retrySession on 401`. Disposition: **replaced**. Reason: Go coordinator /app/session is absent on ESP. Reconnect checks the existing authenticated GET; 401 cancels recovery and requires sign-in. Blind mutating retry is incompatible with no-uncertain-write-replay. Generic session coalescing/authorization remains pending host-contract adaptation.
- Omitted/replaced code: `lamp on/off/toggle routes`. Disposition: **not-applicable**. Reason: Lamp business commands do not apply to discovered table/other board modes. Existing capability-ACK controller actions remain authoritative.
- Omitted/replaced code: `bootstrap/getConfig/patchConfig If-Match, diagnostics and diagnosticsRefreshTimeout`. Disposition: **pending**. Reason: Useful typed bootstrap/revision/config/diagnostic contracts still require actual native/Web/mobile transport and server concurrency/readback mapping; selected GET timeout is not full request lifecycle.
- Omitted/replaced code: `RayanLampClient interface, APIError/isEnvelope/unwrapResponse/readJSON and realtime event decoding`. Disposition: **pending**. Reason: Pakar currently uses flat firmware responses and contextual status projection. Typed structured errors/content checks and client abstraction remain useful pending actual contracts.ts/direct-api/client-factory review, not an accepted superior omission.
- Omitted/replaced code: `hotkey capture begin/renew/release/probe, integration registration and window/show/hide`. Disposition: **pending**. Reason: Generic host/desktop operations need verified native capability, lease ownership and full surfaces. No nonexistent ESP endpoints are exposed.
- Omitted/replaced code: `notify/playAudio/importAudio/validateAudioRequest/safeAudioFilenameHeader`. Disposition: **pending**. Reason: Generic host/browser audio/notification/import support remains useful; AVR melody/buzzer control is not equivalent WAV playback or content-addressed host import. Adapt discovered host/device ownership and port exact tests.
- Test-code evidence: `tests/realtime_retry_test.mjs:54 [UTF8-LF sha256=ee9a3c1cc01dc42d5b1a181e0e37695f34fafe05685844ca6de7f37c925c4a9d; anchor=Actual api.ts AbortController deadline/finally cleanup]`.
- Remaining: api.test.ts was fully read; all its authenticated host/config/audio/hotkey/registration scenarios remain pending matching implementations, not a completed import certificate.
- Remaining: Complete generic request timeout/cancellation/error/session/concurrency on actual interfaces without replaying uncertain writes; LAN/native/mobile verification remains open.

### RayanLamp: `webui/src/browser-console.test.ts`

Verdict: **equivalent-behavior**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `024ed161c6c71655ef67cdbed8cf743fd33864118604737a5f44f533bf9f48f5`.

- Target: `tests/browser_console_test.mjs:1 [UTF8-LF sha256=7e820c8640413b2b05b82c8be6bc45a7374feae067270ecf39719a2f856f0120; anchor=Actual RayanLamp browser-console.test.ts scenarios]`.
- Retained: All actual donor startup, five-level dispatch, allowlist/redaction, keyed dedupe, Error-code-only and control/Bidi scenarios ported against shipped code. Added bounds/cycles/prototype, invalid sink/date, current palette, real HTTP secret/no-replay/quiet-poll, stale WebSocket ownership and canonical embedding checks.
- Omitted/replaced code: `Vitest describe/it.each/vi sink mocks`. Disposition: **replaced**. Reason: Built-in Node assertions/VM sink and controlled time run the same scenarios plus real production adapter code without adding dependencies or build caches.
- Omitted/replaced code: `Lamp state names and fixed brand color expectations`. Disposition: **replaced**. Reason: Tests assert adapted device vocabulary and actual shared Pealayer colors rather than claiming the lamp identity/colors apply to Pakar.
- Test-code evidence: `tests/browser_console_test.mjs:75 [UTF8-LF sha256=7e820c8640413b2b05b82c8be6bc45a7374feae067270ecf39719a2f856f0120; anchor=Real HTTP adapter]`.
- Remaining: VM execution is not pixels, deployment, LAN or physical board acceptance.

### RayanLamp: `webui/src/browser-console.ts`

Verdict: **scoped-improvement**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `5584e97f745eadd5fa52dd1cbec91813dd4f0638da0ca4cf7ca986658f730165`.

- Target: `assets/web/browser-console.js:77 [UTF8-LF sha256=6c0c640433ebfcfff648fed3ce3f384da857d92123f134b6ff122843d83cfa64; anchor=class StyledBrowserConsole]`.
- Target: `firmware/esp8266/src/WebUi.h:871 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function connectWs()]`.
- Retained: Actual five-level styled sink/startup, operational event vocabulary, allowlisted metadata, recursive sanitizer, safe Error-code projection and keyed quiet-window deduplication imported into canonical browser module. Real HTTP and WebSocket lifecycle consumers execute it; routine successful polling, telemetry and binary frames stay quiet.
- Retained: Actual URL/Bearer/JWT/query, single-line/control-character protection, depth/array/object bounds and cycle handling retained; no interception, secret/raw-response trace or persistent writes.
- Omitted/replaced code: `Lamp-only lamp.state/lamp.command vocabulary and RAYAN LAMP startup brand`. Disposition: **replaced**. Reason: Pakar uses device.event/device.command and its controller identity, not another device's lamp protocol. Generic operational semantics are retained.
- Omitted/replaced code: `Hardcoded lamp palettes/gradient and monospace badge typography`. Disposition: **replaced**. Reason: Current shared Pealayer palette and controller typography are resolved on each emission; no second fixed color engine or unwanted tint.
- Omitted/replaced code: `Unbounded recent Map, boundary-only sensitiveKey, caller-controlled percent format, raw DOM id/function name and throwing host sink`. Disposition: **replaced**. Reason: 256-key/60-second bounds, camelCase redaction, percent escaping, minimal DOM/function summaries and fail-closed sink protect long-lived firmware clients and secret-bearing host data.
- Omitted/replaced code: `Vitest/TypeScript runtime packaging`. Disposition: **replaced**. Reason: ESP's existing classic-script embedding and Node VM tests require no React/Vitest dependency, donor build or duplicate firmware copy.
- Test-code evidence: `tests/browser_console_test.mjs:31 [UTF8-LF sha256=7e820c8640413b2b05b82c8be6bc45a7374feae067270ecf39719a2f856f0120; anchor=Actual allowlist + recursive redaction]`.
- Test-code evidence: `tests/browser_console_test.mjs:94 [UTF8-LF sha256=7e820c8640413b2b05b82c8be6bc45a7374feae067270ecf39719a2f856f0120; anchor=Actual WebSocket ownership]`.
- Scoped advantage: Adds bounded dedupe lifetime, camelCase/prototype/format protection, newest-socket ownership and diagnostic failure containment, with executable negative fixtures.
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
- Target: `firmware/esp8266/src/WebUi.h:866 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function report(e,tone)]`.
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

- Target: `assets/web/rayanlamp.js:79 [UTF8-LF sha256=73144dfaf4a3e02fff90e25d3d53bb48029bb2dd802bf5051bf4f2bc67124491; anchor=function subscribeLocalePreferences]`.
- Target: `assets/web/localization.js:36 [UTF8-LF sha256=67b413df8ceaeaff6cb2b3724ac457a97ca7206c47887b15a46ae555a19d5eab; anchor=subscribeLocalePreferences(value=>]`.
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
- Target: `firmware/esp8266/src/WebUi.h:224 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=id="uiHaptics"]`.
- Retained: Actual pulse constants, bounded audio cues, optional vibration API, fail-closed visibility/reduced-motion conditions, copied pattern arrays and host-error containment adapted into embedded JavaScript. Opt-in Touch feedback defaults off, disabled when unsupported; only explicit command HTTP results trigger it, never polling/telemetry/frames.
- Omitted/replaced code: `TypeScript interfaces/private/type annotations`. Disposition: **replaced**. Reason: ESP serves dependency-free JavaScript. Actual runtime bodies are retained; explicit own-property cue validation replaces a compile-time-only guarantee.
- Test-code evidence: `tests/rayanlamp_reuse_test.mjs:24 [UTF8-LF sha256=8b10200e63b30f5b73c36986971c52438fa0f21858c6da74110d868149a30a5d; anchor=h.trigger('arbitrary raw frame')]`.
- Scoped advantage: Unknown JavaScript cues now fail closed at runtime.
- Remaining: Verify supported Android vibration and actual command lifecycle. HTTP acceptance is not actuator readback.

### RayanLamp: `webui/src/i18n.test.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `1c72c37780aeefe39c4d571636616e10c4bbc0fb0b57df8332e9b62c144d599b`.

- Target: `tests/rayanlamp_locale_test.mjs:1 [UTF8-LF sha256=526020d00cc63918018159a77ce2d06c46f1a2425f83bb99d6ca1e9acc193e47; anchor=Actual RayanLamp i18n.test.tsx contracts]`.
- Retained: Actual normalization, Persian-vs-Arabic detection, translation/placeholder/visual-number and automatic languagechange tests adapted to actual shipped code. Added explicit language/direction stability, low-write persistence, new-owner teardown, identity/draft preservation, reversible dynamic labels and detached-node pruning.
- Omitted/replaced code: `Vitest/Preact, lamp title/storage identity and secondary-language expectations`. Disposition: **replaced**. Reason: Node/VM executes the dependency-free embedded module. Pakar deliberately retains primary language, explicit direction and acknowledged board identity; tests document each difference.
- Omitted/replaced code: `Rendered BidiLtr component test and full actual native/mobile/browser acceptance`. Disposition: **pending**. Reason: Protocol fixture asserts unchanged text/direction but is not a new BDI renderer. Grid LTR is source-checked only. Actual protocol isolation/date/platform consumers still need implementation and runtime inspection.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:66 [UTF8-LF sha256=526020d00cc63918018159a77ce2d06c46f1a2425f83bb99d6ca1e9acc193e47; anchor=Restart and idempotent teardown]`.
- Remaining: Implement/render protocol isolation and date consumers; complete native/mobile/browser matrix. VM tests are not pixels or physical board proof.

### RayanLamp: `webui/src/i18n.tsx`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `9a4843ade326c1ee323c3f60d6b0d6ac41cab9d2dcd50affed276fdd8096ff0a`.

- Target: `assets/web/localization.js:32 [UTF8-LF sha256=67b413df8ceaeaff6cb2b3724ac457a97ca7206c47887b15a46ae555a19d5eab; anchor=function startUiLocaleController()]`.
- Target: `firmware/esp8266/src/WebUi.h:763 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function renderSelectionSummary()]`.
- Retained: Actual normalization/detection, t placeholder fallback, visual digits, Intl.NumberFormat and languagechange lifecycle adapted into the real embedded localization module. Selection counts and batch readback messages are real consumers. Restart ownership, idempotent cleanup, missing/inherited placeholder handling, blocked storage and no OS-triggered preference writes are added.
- Retained: Dynamic network/cloud labels and panel menus retranslate reversibly without replacing board identity, hostnames, mapping IDs, protocol strings or unsaved secrets.
- Omitted/replaced code: `Preact signals, lamp storage key/auto enum, any secondary Persian language and unconditional render-time persistence`. Disposition: **replaced**. Reason: ESP uses existing dependency-free state, system enum and presentation channel. Primary browser language and independent explicit direction preserve Pakar pre-paint policy. Persistence belongs only to validated preference changes; no OS/render writes or second toolchain.
- Omitted/replaced code: `Rayan Lamp document title/meta branding and lamp/coordinator/COM/URI-specific literal captions`. Disposition: **not-applicable**. Reason: These describe another application's lamp identity/endpoints. Pakar title follows independently acknowledged board application metadata; translating peer names or claiming unrelated lamp actions would be incorrect. Generic mechanisms remain independently pending.
- Omitted/replaced code: `Persian-calendar formatDateTime, rendered reusable BidiLtr, additional generic dictionary/settings consumers and native/mobile locale synchronization`. Disposition: **pending**. Reason: Need real controller date/protocol/settings consumers and runtime acceptance, not unused copied functions or fixture-only UI. Current grid/seven-segment LTR and unchanged wire text are not full BidiLtr parity. Visual digit helper exists but further app call sites remain pending.
- Test-code evidence: `tests/rayanlamp_locale_test.mjs:54 [UTF8-LF sha256=526020d00cc63918018159a77ce2d06c46f1a2425f83bb99d6ca1e9acc193e47; anchor=Automatic locale reacts to OS changes]`.
- Test-code evidence: `tests/web_cell_table_test.mjs:117 [UTF8-LF sha256=420b8dfb17544ca9aa53204045d2e2b9fff993c98a225fc5eddb4a99adff5feb; anchor=Selection must not mutate hardware]`.
- Remaining: Finish dynamic confirmations/errors/status and generic protocol/date consumers. Port locale lifecycle into actual Flutter controls and validate native/mobile/browser synchronization. No full localization or hardware acceptance claim.

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

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=b3ff00c8b1996d5b00675faf6a64fe8313d2f85525330f7cb2596ec16554eaee; anchor=function createRealtimeRetry]`.
- Retained: Fully read actual tests. Prepare-before-reconnect/session-loss and stopped lifecycle concepts adapted to actual Pakar authenticated status GET and shipped Web connectWs/refreshAuthenticated/api execution.
- Retained: Added deterministic injected timers/RNG, jitter/cap, deadline/AbortController, constructor failure, failed reads, one owner and stale timer/promise cancellation regressions.
- Omitted/replaced code: `server event sequence epochs and duplicate ordinary events`. Disposition: **pending**. Reason: No authoritative sequence contract has been adapted yet. Port hello/reset/duplicate tests with actual Pakar event epoch ownership; do not assign synthetic sequence values just to reuse assertions.
- Omitted/replaced code: `per-target sync names, event-disabled config synchronization, lease owner, initial-sync/HTTP/follower audio policies`. Disposition: **pending**. Reason: Useful generic multi-tab/config/audio tests await their full implementation and validated dependencies. Existing independent clients and semantic haptics are not evidence of leadership parity.
- Omitted/replaced code: `marks coordinator unavailable/graceful close and renews browser session using Go API`. Disposition: **pending**. Reason: Pakar 401 status GET fails closed to login, without creating a Go session or automatically replaying mutation. Readback-before-open recovery is adapted; generic coordinator shutdown/typed availability tests remain pending.
- Omitted/replaced code: `Vitest/Preact store fixtures and TestBroadcastChannel/TestRealtimeSocket classes`. Disposition: **replaced**. Reason: Built-in Node VM and purpose-built HTTP/socket/timer fixtures execute shipped Pakar code, without adding a second framework or donor build. Channel fixtures remain pending leadership implementation.
- Test-code evidence: `tests/realtime_retry_test.mjs:72 [UTF8-LF sha256=ee9a3c1cc01dc42d5b1a181e0e37695f34fafe05685844ca6de7f37c925c4a9d; anchor=Execute actual Web connectWs + refreshAuthenticated + api]`.
- Remaining: Port every useful remaining donor sequence/lease/config/audio/shutdown assertion with its actual consumer; no whole-test-file parity claim.
- Remaining: Real LAN, multi-client and native/mobile lifecycle verification remains required.

### RayanLamp: `webui/src/sync.ts`

Verdict: **parity-gap**. Revision: `filesystem-snapshot-no-git`. Original-byte SHA-256: `bd849cbef1ee67ac97cca9e0c6e00ed69acf725d805ad986ae58f11513546d73`.

- Target: `assets/web/realtime-retry.js:6 [UTF8-LF sha256=b3ff00c8b1996d5b00675faf6a64fe8313d2f85525330f7cb2596ec16554eaee; anchor=function createRealtimeRetry]`.
- Target: `firmware/esp8266/src/WebUi.h:871 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=function connectWs()]`.
- Retained: Actual scheduleReconnect 500 ms exponential base, 10-second cap and 0.8..1.2 jitter, prepareReconnect-before-open, reset-on-open and stop cleanup adapted to actual Pakar WebSocket/status GET consumers.
- Retained: Added one pending owner, bounded exponent, late timer/promise generation checks, login/pause/pagehide cancellation and constructor-failure recovery; no device command is replayed.
- Omitted/replaced code: `start/electLeader/acquireLeadership/releaseLeadership and BroadcastChannel/Web Locks fallback lease election/heartbeats/watchdogs/connectionSyncNames`. Disposition: **pending**. Reason: Generic per-origin leadership is useful but not imported. Pakar supports independent authenticated streaming clients; a shared socket requires validated command/stream ownership and follower readback. Compare full client/contracts/store dependencies and port actual multi-tab/takeover tests before enabling.
- Omitted/replaced code: `handleServerEvent/onChannelMessage/acceptSnapshot and serverSequenceDecision`. Disposition: **pending**. Reason: Generic typed snapshot/config/diagnostic projection, revisions and epoch resets remain pending actual Pakar advertised contracts. Existing source-bound contextual readiness/no-replay ownership is retained; do not invent server event sequences or treat arbitrary channel objects as authoritative.
- Omitted/replaced code: `handleDirectStateCue initial on/off transition`. Disposition: **not-applicable**. Reason: Lamp state transitions are not table/first-wins business. Adapt only future discovered board events, not hardcode lamp state.
- Omitted/replaced code: `audioCueCount/playDirectCue, generic message/haptic/diagnostic fanout and setConnectionState/relayDisconnectReason`. Disposition: **pending**. Reason: Existing semantic haptics/toast/trace helpers do not implement full leader-only audible notification or graceful coordinator state. Generic audio/count/shutdown semantics remain useful pending exact firmware/host event and capability mapping.
- Test-code evidence: `tests/realtime_retry_test.mjs:21 [UTF8-LF sha256=ee9a3c1cc01dc42d5b1a181e0e37695f34fafe05685844ca6de7f37c925c4a9d; anchor=Donor prepareReconnect is completed before any replacement socket]`.
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
- Target: `firmware/esp8266/src/WebUi.h:8 [UTF8-LF sha256=9b77c8d5c35380bf12dbe32cf9c852e365ba19089c09be0dbf7c26d4d90d4fde; anchor=<script data-pakar-presentation>]`.
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
