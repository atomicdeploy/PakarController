# PakarController donor comparison

Whole-repository superiority is **not established**. This report exposes retained code, scoped improvements, omissions and gaps; it does not turn missing reviews into exclusions.

Inventory: 1868 files; 31 complete source-file comparisons; 1837 pending comparisons; 16 compared files with known parity gaps. Header/implementation pairs can describe the same gap. Missing sparse files remain pending.

Every inventoried path, source revision/hash, target/proof file and line, omitted code, disposition and concrete reason is in `file-comparison.csv`. Hash-bound manual decisions are in `file-comparison-evidence.json`. Donor SHA-256 values bind original bytes; target/proof SHA-256 values bind UTF-8 without BOM with LF newlines. Anchors are recomputed into exact line numbers. Test-code presence is not test execution; execution receipts are in `FILE-COMPARISON-RECEIPTS.md`.

## Coverage

| Donor | Inventoried | Compared | Pending | Files with gaps |
|---|---:|---:|---:|---:|
| ControllerBoardMini | 19 | 0 | 19 | 0 |
| ControllerBoardMini/Puzzles | 18 | 12 | 6 | 9 |
| PCController | 1207 | 6 | 1201 | 0 |
| Pealayer | 493 | 9 | 484 | 3 |
| PU850-ESP | 109 | 4 | 105 | 4 |
| PuzzleBoard | 22 | 0 | 22 | 0 |

## Exact reviewed-file mappings and omission decisions

### ControllerBoardMini/Puzzles: `LocalLib/Keys.cpp`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `9ceb7e764ef9d4d5feb62b2c7a22eed59d614b7b56432fae87ce87b4aa434578`.

- Target: `firmware/avr/avr.ino:387 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:372 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void boardKeyGesture(]`.
- Retained: Active-low physical plus virtual shift input state and candidate/stable transition tracking; AVR reports releases as dirty state; press handling is mode-specific.
- Retained: Actual PCController per-key callback/gesture classifier now feeds all nine inputs; no game action waits for click classification.
- Omitted/replaced code: `DEBOUNCE_MS = 50`. Disposition: **pending**. Reason: Actual PCController classifier now uses macro-configurable PAKAR_KEY_DEBOUNCE_MS (default 20ms) with tested Down/Up, click and hold timing. Physical switch-bounce measurements remain pending; faster is not certified electrically.
- Omitted/replaced code: `Key virtual readCurrentState/onStateChanged, setCallback, bool/getCurrentState interface`. Disposition: **replaced**. Reason: Imported PCController per-key classifier, setCallback and setEventCallback are retained. updateSample separates the input driver from gesture classification and supports all nine shift/ADC keys; subclass-specific raw reads are replaced by the actual board sample adapter.
- Omitted/replaced code: `Raw Puzzles lower-four input order`. Disposition: **pending**. Reason: Current PCController-based register reader reverses those bits. Physical key identity must decide the mapping; source reuse alone cannot.
- Remaining: Add input mapping/debounce configuration and bounce/press/release tests; preserve reusable callback capability or record a reviewed alternative contract.

### ControllerBoardMini/Puzzles: `LocalLib/Keys.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `caa21fe97e3df262f0821bd582b60f2895a14ec193d7d6fec13321afe4c5a46d`.

- Target: `firmware/avr/avr.ino:387 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void serviceInputs(]`.
- Target: `firmware/avr/avr.ino:372 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void boardKeyGesture(]`.
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

- Target: `firmware/avr/avr.ino:387 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void serviceInputs(]`.
- Retained: Only cooperative target-specific timers exist in current AVR services; the generic donor task scheduler is not implemented.
- Omitted/replaced code: `Tasks::addTask/cancelTask/update and ten callback slots`. Disposition: **pending**. Reason: Front-panel deadlines do not replace arbitrary scheduled callbacks or cancellation. It is useful infrastructure and needs an explicit replacement, not a business-logic exclusion.
- Omitted/replaced code: `startsAt==0 free-slot sentinel and currentTime > startsAt comparison`. Disposition: **pending**. Reason: Do not copy the rollover/zero-deadline ambiguity unchanged. A replacement must keep the generic scheduling/cancellation API using explicit occupancy and wrap-safe deadlines; such a replacement is not present yet.
- Remaining: Implement and test a bounded cooperative scheduler: cancellation, full queue, zero delay, callback reentrancy and millis rollover.

### ControllerBoardMini/Puzzles: `LocalLib/Tasks.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `5d838a7fb53c799160d55a65b22546ab2e749eb30f8412ed5d08e9ce44300fed`.

- Target: `firmware/avr/avr.ino:387 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=void serviceInputs(]`.
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

- Target: `firmware/esp8266/src/LedGrid.cpp:16 [UTF8-LF sha256=1abd228ccecf1c949f90f5ce90ecdbdece35929239b1f3b12f08633414da5c4f; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:56 [UTF8-LF sha256=279bb2156a6e1987acf9f84462efc88f4f633964ac9a37749dbe7c19c7557113; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: User assigned final WS2811 effects to ESP D2 and 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### ControllerBoardMini/Puzzles: `LocalLib/WS2811.h`

Verdict: **parity-gap**. Revision: `filesystem-snapshot`. Original-byte SHA-256: `395096029df7c507782f40c5b32c8b1dffbf2db392cb26cd2b02c824d6a36949`.

- Target: `firmware/esp8266/src/LedGrid.cpp:16 [UTF8-LF sha256=1abd228ccecf1c949f90f5ce90ecdbdece35929239b1f3b12f08633414da5c4f; anchor=FastLED.addLeds<WS2811]`.
- Target: `firmware/esp8266/src/LedGrid.h:56 [UTF8-LF sha256=279bb2156a6e1987acf9f84462efc88f4f633964ac9a37749dbe7c19c7557113; anchor=uint8_t brightness_ = 255]`.
- Retained: Actual FastLED WS2811 setup and TypicalLEDStrip correction concept; full 255 brightness; ESP owns the final strip engine.
- Omitted/replaced code: `AVR D6 / 11-pixel final effect buffer`. Disposition: **replaced**. Reason: User assigned final WS2811 effects to ESP D2 and 80 groups of six LEDs. A duplicate 480-pixel AVR buffer would consume 1440 bytes before protocol/game state on a 2 KiB AVR; streaming it over UART would also consume bandwidth. D6 remains a diagnostic pin, not a second full engine.
- Omitted/replaced code: `COLOR_ORDER BRG`. Disposition: **pending**. Reason: Current ESP code hardcodes RGB. Different chip/wiring orders are not inherently better; independent R/G/B physical tests and a configurable order are required before declaring channel parity.
- Omitted/replaced code: `Black initialization and second post-show loop clearing leds[]`. Disposition: **replaced**. Reason: Pakar initializes logical cells white to match the explicitly requested table mode, instead of the donor eleven-LED black scene. No claim of generic performance superiority is made for removing this loop.
- Remaining: Make chipset/channel order configurable and verify R/G/B plus all 480 pixels on hardware; expose current configuration across the control surfaces.

### PCController: `LocalLib/SevenSegments.cpp`

Verdict: **scoped-improvement**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `54c366525142aca0103a8d58f7ce8d9cbc34c1b5884b20a1ad5ae50ddc6bb4a8`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:98 [UTF8-LF sha256=28547987e3fe6bf85e13763fa93ef8bb234f8d8f6204b43658058f070f6d9228; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:234 [UTF8-LF sha256=325baf7036fa3a9cfcf65f8d4d415ee2ac42155a89fcdb1282c461b3e9102930; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:428 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/SevenSegments.h`

Verdict: **scoped-improvement**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `e58e1783404f494ea7b1852780b6485cbfda872781b9159b9ee29426edae9240`.

- Target: `firmware/avr/SevenSegments.h:26 [UTF8-LF sha256=a2719b4f3d9c21e98200420a91cb0326b1869dbcfb90136a1184e379f7b7ee3b; anchor=void serviceBrightness(]`.
- Target: `firmware/avr/SevenSegments.cpp:98 [UTF8-LF sha256=28547987e3fe6bf85e13763fa93ef8bb234f8d8f6204b43658058f070f6d9228; anchor=void writeBusByte(]`.
- Target: `firmware/avr/BoardPins.h:31 [UTF8-LF sha256=cd69a72a316a0ad001689b9ad19fd147bf335482a4d9e931124d8eff9af2892f; anchor=Tm1637Data = I2cSda]`.
- Retained: Bounded four-digit RAM/flash text, raw segments, integer/fixed-point output, caching and 0..7 brightness/display-off behavior.
- Retained: Actual donor ACK sampling, one-second cached-frame/brightness keepalive, observed presence and active 70ms brightness service on PC4/PC5. Independent probing was removed.
- Omitted/replaced code: `PB3/PB5 TM1637 bus pins`. Disposition: **replaced**. Reason: User-confirmed PuzzleBoard owns A4/SDA as DIO and A5/SCL as CLK. D11/D13 must remain free for ESP-driven ISP; copying PCController's pins would conflict with programming.
- Test-code evidence: `tests/pc_contract_test.cpp:234 [UTF8-LF sha256=325baf7036fa3a9cfcf65f8d4d415ee2ac42155a89fcdb1282c461b3e9102930; anchor=void keysAndDisplay()]`.
- Test-code evidence: `firmware/avr/avr.ino:428 [UTF8-LF sha256=0ebdc01cc3c3b0ce8168f825561337f3c317313b1cd90f3a2784fb276b9d068a; anchor=display.serviceBrightness(]`.
- Scoped advantage: Retains donor recovery on the correct board pins, accumulates intermediate NACKs instead of letting the final control ACK hide failure; host disconnect/reconnect and true-off tests pass.
- Remaining: Physical TM1637 unplug/replug and power-cycle acceptance remains unverified; host GPIO doubles are not a physical board.

### PCController: `LocalLib/ShiftRegisters.cpp`

Verdict: **equivalent-file**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133`.

- Target: `firmware/avr/ShiftRegisters.cpp:1 [UTF8-LF sha256=43f944f0767f3cb59d5224e1241ca0aff2793c40c7ededdb95bd7553a0268133; anchor=#include "ShiftRegisters.h"]`.
- Retained: Byte-identical donor file, including eight active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- No production-code omission claimed for this reviewed file.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/ShiftRegisters.h`

Verdict: **equivalent-file**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff`.

- Target: `firmware/avr/ShiftRegisters.h:32 [UTF8-LF sha256=397dc4abff3e865e922cfd94dc66d554345368a3f95f899c75b49c3dc1215bff; anchor=class ShiftRegisters]`.
- Retained: Byte-identical donor file, including eight active-low inputs/outputs, shared-clock transfer, safe output-enable/reset startup and virtual input injection.
- Retained: The implementation retains PCController's lower-four input-bit reversal; this is not physical proof that the PuzzleBoard wiring uses the same order.
- No production-code omission claimed for this reviewed file.
- Remaining: Verify actual key/output mapping on PuzzleBoard; unchanged file depends on Pakar's different BoardPins.

### PCController: `LocalLib/TonePlayer.cpp`

Verdict: **scoped-improvement**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `e4715e126bcaa342fc3004f2d0fbc300921710d8e1ea7e6b44fc8fe7870ef343`.

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

Verdict: **scoped-improvement**. Revision: `20c6115bb462fe5111b66cd7c21f7328a1f78892`. Original-byte SHA-256: `edb2f40cfdfb37d1c08867c121c617beb8f9f53ad2583855ae2218a07eb9fd89`.

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

### Pealayer: `assets/themes/palettes.json`

Verdict: **equivalent-file**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501`.

- Target: `assets/themes/palettes.json:2 [UTF8-LF sha256=11990ef4b31ba6eb1275c6e76b9156e0f05bacb2f59a8fa0372b778c6caf1501; anchor="studio"]`.
- Retained: Byte-identical Studio/Native light/dark JSON catalog, all fourteen semantic roles.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/ui/palette.rs:71 [UTF8-LF sha256=27e4d4ab36060699693c2bdd951c46666b1831fc9d0a1771b6980b431076cd3e; anchor=fn imported_palettes_have_every_required_role()]`.
- Remaining: Embedded Web UI still hardcodes its palette; catalog retention is not all-surface integration.

### Pealayer: `src/application_shortcuts.rs`

Verdict: **parity-gap**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `1f173bef8acc2904eac5b0a91407aa4f7629ce65c88ea3a8ac78f8c13321d203`.

- Target: `desktop/src/ui/shell.rs:284 [UTF8-LF sha256=9f9b8afe14bfe43373bbbf02c19d236340a26d769c674de346324efbeeabb9fd; anchor=egui::Key::F11]`.
- Retained: Default Preferences Ctrl+, and device configuration Ctrl+Shift+, navigation exist; F11 fullscreen is restored with the shared typed dispatcher. These are not equivalent to the donor persisted rebinding model.
- Omitted/replaced code: `ApplicationShortcuts persisted fields, parse_shortcut, entries, validate, action_for_event and their non-media tests`. Disposition: **pending**. Reason: The foundation hardcoded its accelerators and omitted the reusable binding model. No accepted reason justifies leaving rebinding out. The actual donor depends on KeyChord and hardware_shortcuts match/key-code helpers, which must be ported and tested with controller-specific typed actions rather than guessed labels.
- Omitted/replaced code: `take_frame_step_shortcut and frame-step tests`. Disposition: **not-applicable**. Reason: These consume Ctrl+Left/Right for media frame stepping and timeline transport. Those business domains were explicitly removed; Pakar must not hijack ordinary text word navigation for nonexistent video frames.
- Omitted/replaced code: `containing_media_folder and local-media path tests`. Disposition: **not-applicable**. Reason: This resolves media files/streams, not board identities or firmware files. Firmware selection already uses explicit native file dialogs; introducing a nonexistent media folder action would be misleading.
- Omitted/replaced code: `media_information and media_folder accelerators`. Disposition: **pending**. Reason: The media operations cannot be retained as-is, but their reusable information/folder-navigation affordances need explicit hardware-monitor and application-folder action decisions. They are not silently treated as completed controller mappings.
- Remaining: Restore validated persistent rebinding, exact-modifier matching, repeat/release rejection and text-edit protection; separately reconcile global/hardware shortcut ownership.

### Pealayer: `src/messaging.rs`

Verdict: **equivalent-file**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573`.

- Target: `desktop/src/messaging.rs:81 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=pub fn publish(]`.
- Retained: Byte-identical MessageHub, validation, bounded queue, expiry/upsert/dismiss/revision/source identity, and both donor tests.
- No production-code omission claimed for this reviewed file.
- Test-code evidence: `desktop/src/messaging.rs:171 [UTF8-LF sha256=471236bd7e8b1733fef743c81a9dff407816cd221a4d290d0cfa5a9fc753c573; anchor=fn upsert_dismiss_and_expire_are_shared()]`.
- Remaining: This file equivalence does not establish Web/TUI/IPC notification-surface parity.

### Pealayer: `src/platform/windows_quick_actions.rs`

Verdict: **equivalent-behavior**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `35f6de708a0e402f6958a4d8feeead5037b42174286050ddc98c494d4173e4cb`.

- Target: `desktop/src/platform/windows_quick_actions.rs:15 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=pub const WINDOWS_QUICK_ACTIONS]`.
- Retained: Actual typed action/resource catalog, equality traits, explicit negative resource-index selection, Preferences, Exit and generic Toggle fullscreen. Controller actions share the launch/IPC dispatcher and resource compiler.
- Omitted/replaced code: `Play / Pause, Previous chapter, Next chapter, Mute / Unmute media quick actions`. Disposition: **not-applicable**. Reason: Pakar intentionally has no mpv playback, chapters or media volume. These would call removed media business methods. Connect, device settings, refresh, workspaces and About are real controller navigation actions instead; generic fullscreen is retained, not misclassified as media-only.
- Test-code evidence: `desktop/src/platform/windows_quick_actions.rs:69 [UTF8-LF sha256=cefe74617d723f5dd9ef987613cbcfd593e196f7a0bbfd704d65b0759034f2cd; anchor=fn controller_shell_actions_have_unique_native_resource_ids]`.
- Remaining: Visible Windows Jump List clicks remain a manual acceptance gate; typed launch forwarding and resource uniqueness are tested.

### Pealayer: `src/platform/windows_shell_icons.rs`

Verdict: **equivalent-file**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a`.

- Target: `desktop/src/platform/windows_shell_icons.rs:45 [UTF8-LF sha256=e1d8793f8f1697f2030bc407f51e48cab4063964c051f2e21d1f60ae3349732a; anchor=pub fn shell_icon_ico]`.
- Retained: Byte-identical actual Phosphor rasterizer, centered glyph coverage, all seven ICO resolutions, bottom-up BGRA, DWORD-aligned AND masks and explicit directory offsets. Pakar build.rs uses it for PE, tray and action assets.
- No production-code omission claimed for this reviewed file.
- Remaining: User-configurable icon packs and runtime state-icon transitions are a separate unresolved integration; retaining the generator does not implement that editor.

### Pealayer: `src/ui/icons.rs`

Verdict: **equivalent-behavior**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `d3a71c69516de9059735c6baee76791be785e65b7f435e0bd965f4db7f521d30`.

- Target: `desktop/src/ui/icons.rs:567 [UTF8-LF sha256=b3d882b8832f0c74c579ddfba1a992723b4ed06a8521e1a0aa1d2d45128e6a11; anchor=pub fn searchable_workspace_icon_picker(]`.
- Target: `desktop/src/ui/shell.rs:1637 [UTF8-LF sha256=9f9b8afe14bfe43373bbbf02c19d236340a26d769c674de346324efbeeabb9fd; anchor=icons::searchable_workspace_icon_picker]`.
- Retained: Production Phosphor vocabulary, stable names/kind fallback, searchable list/grid picker, UTF-8-safe match highlighting, bounded popups, disclosure/submenu helpers.
- Retained: Actual donor regression tests restored: eleven tests, including interactive egui-frame search/select/reopen, both themes and popup width.
- Omitted/replaced code: `every_native_icon_editor_uses_the_shared_searchable_picker donor-only editor path list`. Disposition: **replaced**. Reason: That assertion references Pealayer media/4D editor files not present in this product. Replaced only its call-site assertion with Pakar's actual workspace editor; the other ten tests were imported. This does not excuse missing generic editor features elsewhere.
- Test-code evidence: `desktop/src/ui/icons.rs:825 [UTF8-LF sha256=b3d882b8832f0c74c579ddfba1a992723b4ed06a8521e1a0aa1d2d45128e6a11; anchor=fn pakar_workspace_editor_uses_the_actual_shared_searchable_picker()]`.
- Test-code evidence: `desktop/src/ui/icons.rs:954 [UTF8-LF sha256=b3d882b8832f0c74c579ddfba1a992723b4ed06a8521e1a0aa1d2d45128e6a11; anchor=fn icon_picker_search_click_filter_select_and_reopen_work_in_both_forms()]`.
- Remaining: Imported compact/control picker helpers are not all used by current panels; UI-wide application/action parity needs separate file reviews.

### Pealayer: `src/ui/palette.rs`

Verdict: **parity-gap**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `c169db6a61b79ebb2ce00f0ca442305eefeea7b34740720a2cdf6424d6fef85d`.

- Target: `desktop/src/ui/palette.rs:36 [UTF8-LF sha256=27e4d4ab36060699693c2bdd951c46666b1831fc9d0a1771b6980b431076cd3e; anchor=pub fn apply(]`.
- Retained: Native semantic visual-role mapping and actual palette JSON; appearance geometry is not overwritten.
- Omitted/replaced code: `WebAppearanceState and donor native/Web shared-preferences assertions`. Disposition: **pending**. Reason: Pakar lacks the shared native/Web preferences contract. These are useful generic requirements, not media business logic; omitting them cannot be justified as superior.
- Omitted/replaced code: `Donor default Native palette`. Disposition: **replaced**. Reason: Pakar chooses Studio as its product default. Both palettes remain available; this is a preference decision, not a measured improvement.
- Test-code evidence: `desktop/src/ui/palette.rs:101 [UTF8-LF sha256=27e4d4ab36060699693c2bdd951c46666b1831fc9d0a1771b6980b431076cd3e; anchor=fn donor_surface_text_roles_and_geometry_are_preserved_in_both_themes()]`.
- Remaining: Make embedded Web and mobile consume the same role catalog/appearance contract; port donor shared-preference tests after adapting types.

### Pealayer: `src/ui/toasts.rs`

Verdict: **equivalent-behavior**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `a6501d2c0f859e186cac659a37d256f26f8896e98644e9cfc8ac880da5460e54`.

- Target: `desktop/src/ui/toasts.rs:4 [UTF8-LF sha256=8772d049fab0fba597bd7f4ba4403c7d7bd122f6027442e25f32da11d8f058f6; anchor=pub fn draw(ctx:]`.
- Retained: Actual donor root-only overlay: four newest messages, severity icons and semantic palette roles, bounded width, scrollable text, title, dismiss and deadline-driven repaint.
- Omitted/replaced code: `interop::get_live_config() accessor`. Disposition: **replaced**. Reason: Pakar owns the same live palette through config::live(). The OS-native notification route is separate; adding an explicit in-app enable preference does not remove the donor overlay when enabled.
- Remaining: This native overlay mapping is not shared Web/TUI message-delivery parity.

### Pealayer: `tests/windows_shell_suite_test.rs`

Verdict: **parity-gap**. Revision: `78563ff7501e13cb8e40a60dd181f69f127c550d`. Original-byte SHA-256: `d8696d8b591ca8ae4c77c78a07d19f6aca103e29906f267f19ac8ba1aa5d6e01`.

- Target: `desktop/src/platform/windows_notifications.rs:115 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=assert!(Hook::new(42).is_err())]`.
- Retained: Actual Preferences action/resource catalog is validated; invalid HWND registration rejects rather than claiming success; owner exclusion is tested. Live notification acceptance and shown-callback evidence are recorded independently.
- Omitted/replaced code: `Playback-duration error/paused percentages and Play/Pause thumbnail/tray labels`. Disposition: **not-applicable**. Reason: The tests model removed video playback, not device transfer progress. Pakar uses indeterminate taskbar activity only for a real pending settings/maintenance request; inventing a media percentage would publish false progress.
- Omitted/replaced code: `PealayerApp::default and media playback_time/duration/is_paused assignments`. Disposition: **replaced**. Reason: Pakar is an executable-only mpv-free controller; creating the donor media engine is inappropriate. Actual controller IPC/live smoke and direct invalid-HWND tests exercise its own lifetime instead.
- Omitted/replaced code: `ensure_shell_initialized retry and repeated shell-lifecycle acceptance`. Disposition: **pending**. Reason: Tray creation failures are visible and close-to-tray cannot hide an app without a tray, but complete root-taskbar initialization/retry and Explorer restart acceptance are not covered by the current tests.
- Test-code evidence: `desktop/src/platform/windows_notifications.rs:110 [UTF8-LF sha256=4603df421e6361701f4d092e0f966abdd39a0fb91f968a098fd5552b67c6c00a; anchor=fn utf16_limits_preserve_complete_surrogates_and_termination]`.
- Remaining: Add root-taskbar lifecycle/reinitialization and hidden-window/tray/notification-click acceptance without fabricating successful HWND installation.

### PU850-ESP: `LocalLib/Shell.cpp`

Verdict: **parity-gap**. Revision: `431104072952f694a35b3a83c19f13fbb098ca5b`. Original-byte SHA-256: `0113acbff973285f5af186a4857d94cc2df3c99813e0db4ba876f861e7c1083b`.

- Target: `firmware/esp8266/src/TelnetShell.h:63 [UTF8-LF sha256=bdd9e09124f9b5c60582beba58dc92bc4ab2b818a84fd3c98c8f5165971f4cf1; anchor=class LineEditor]`.
- Target: `firmware/esp8266/esp8266.ino:2188 [UTF8-LF sha256=3230b426ffc05aefe354e2e23434d5d94e0426f099ce2be1e809112bd828ca54; anchor=void executeTelnetCommand(String line)]`.
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

- Target: `firmware/esp8266/src/TelnetShell.h:63 [UTF8-LF sha256=bdd9e09124f9b5c60582beba58dc92bc4ab2b818a84fd3c98c8f5165971f4cf1; anchor=class LineEditor]`.
- Target: `firmware/esp8266/esp8266.ino:2188 [UTF8-LF sha256=3230b426ffc05aefe354e2e23434d5d94e0426f099ce2be1e809112bd828ca54; anchor=void executeTelnetCommand(String line)]`.
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

- Target: `firmware/esp8266/src/TelnetShell.h:13 [UTF8-LF sha256=bdd9e09124f9b5c60582beba58dc92bc4ab2b818a84fd3c98c8f5165971f4cf1; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/esp8266.ino:2294 [UTF8-LF sha256=3230b426ffc05aefe354e2e23434d5d94e0426f099ce2be1e809112bd828ca54; anchor=void serviceTelnet() {]`.
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

- Target: `firmware/esp8266/src/TelnetShell.h:13 [UTF8-LF sha256=bdd9e09124f9b5c60582beba58dc92bc4ab2b818a84fd3c98c8f5165971f4cf1; anchor=class TelnetParser]`.
- Target: `firmware/esp8266/esp8266.ino:2294 [UTF8-LF sha256=3230b426ffc05aefe354e2e23434d5d94e0426f099ce2be1e809112bd828ca54; anchor=void serviceTelnet() {]`.
- Retained: Actual five-state IAC/option/subnegotiation parser, escaped IAC, ECHO/SGA negotiation and greeting retained in standalone WiFiClient-based implementation.
- Retained: Pakar tracks option state to suppress greeting-ack negotiation loops; unsupported DO/WILL options are explicitly rejected.
- Omitted/replaced code: `ESPTelnet dependency and ESP8266WEBSERVER_H synchronous-header suppression`. Disposition: **replaced**. Reason: Native WiFiServer/WiFiClient removes the dependency which pulled a conflicting synchronous web-server header into an async server product; the actual protocol parser was retained rather than replaced by plain newline input.
- Omitted/replaced code: `Telnet_Port and listener restart/stop on port or AP/STA availability changes; matching mDNS add/remove`. Disposition: **pending**. Reason: Pakar currently hardcodes port 23 and starts its listener at setup. Donor network lifecycle/port configurability is useful generic behavior and is not fully mapped or runtime tested.
- Omitted/replaced code: `Product-branded ANSI greeting and legacy ShellSession callback hookup`. Disposition: **replaced**. Reason: Pakar's authenticated greeting and instance-owned parser/editor serve the new product. Plain Telnet still transmits credentials without encryption; this must be shown/configurable as a trusted-LAN-only surface.
- Test-code evidence: `tests/telnet_shell_test.cpp:17 [UTF8-LF sha256=f7dd91333d3aced80fdab41b146b98ecd2e9dd3896a504f8a70cfb33912f00e9; anchor=assert(output.empty());]`.
- Remaining: Expose configurable enable/port and network-aware listener/discovery lifecycle; verify reconnect, disabled port, AP and station loss on hardware.
- Remaining: Prove actual Telnet login, command/ACK outcome and simultaneous API/WebSocket clients; source parser tests are not LAN acceptance.

## Unreviewed files are not approved omissions

Every pending path has its own CSV row. Its omission reason explicitly says there is no approved reason to leave it out. Prior keyword screening or an unbound historical adaptation note is not accepted as behavior proof. This includes all still-pending PuzzleBoard/40LED variants and ControllerBoardMini legacy files, plus unreviewed Pealayer/PCController/PU850 files, assets, dependencies, tests and scripts. Earlier focused variant notes remain useful routing evidence, not complete comparative certification.

File-by-file checklist: https://github.com/atomicdeploy/PakarController/issues/3. Master production acceptance: https://github.com/atomicdeploy/PakarController/issues/2. Run `pwsh -File tools/generate-file-comparison.ps1 -RequireComplete` to enforce the comparison gate. It must fail while any pending comparison or known source parity gap remains. Physical, deployment, visual and all-surface acceptance are separate gates even after this one passes.
