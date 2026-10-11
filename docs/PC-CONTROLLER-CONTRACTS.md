# PakarController PCController contracts

Source version **0.2.6**. Source/build delivery, not physical-board or production-release certification. Donor inspected: PCController commit **20c6115bb462fe5111b66cd7c21f7328a1f78892**. Exact source/target hashes and exclusions: pccontroller/SOURCE-MAPPING.json. Whole-project gaps remain visible in FILE-COMPARISON.md; this does not close unrelated Pealayer, PuzzleBoard or PU850 gaps.

## Current hardware and ownership

AVR owns eight shift-register inputs/outputs, ADC6 ninth button, Timer1 buzzer and TM1637 **A4/PC4 DIO, A5/PC5 CLK**. ESP owns D2/GPIO4 WS2811 and the existing 480-pixel engine. AVR hardware serial stays 38400 baud; ESP D4/GPIO2 receives AVR TX, D0/GPIO16 transmits to AVR RX. Busy: Node D3/GPIO0 to AVR D10/PB2 with AVR INPUT_PULLUP. ISP and durable Prog record (EEPROM 1008..1012) unchanged. No sensor, PWM, relay, RF, LCD, status-RGB or I2C hardware is inferred from a shift-register LED.

## Wire and safety

Actual donor generated opcode header and JSON registry imported. Pakar legacy 0x11 means audio cue, whereas donor 0x11 means PWM_SET. Separate dialects are mandatory:

* Outer 0x56 request: donor opcode then payload.
* Outer 0xB0 response: donor response opcode then payload. ACK/ERROR include actual request opcode/error.
* Full 48-byte requests use outer 0x57: start [0, opcode, 48, first 45 payload bytes], final [1, remaining 3 bytes], same sequence, less than 200ms apart. No mutation before matched final. Preserves scheduled 40-character DISPLAY_TEXT and macro payload bounds. Current responses fit the 47-byte inner-response bound.
* Inner HELLO: [0x50,0,0,48,enabled_features_LE16,requested_features_LE16]. Bytes 1/2 are fixed reserved zero, not contract generations. Hardware profile **0x50** is not the donor sensor board. Stock PCController hardware decoding needs a profile adapter.
* AVR HELLO preserves original nine-byte prefix and appends [0x50,features_LE16]. ESP does not assume support from version alone.
* Prog rejects mutations including settings, macros and automations; queries remain available. Application PROGRAM_STATE is not ISP Prog. Reset ACK precedes safe shutdown/watchdog reset; no flash write.
* ESP per-client receipts bind boot identity, request ID, sequence and expected response. Eight slots; 750ms sent timeout, 2s queued timeout, 60s retention unless explicitly consumed, 30s sequence quarantine. Queue acceptance is not AVR acceptance. Uncertain mutations never retry. Validated terminal receipts can be consumed without releasing sequence quarantine; queued/awaiting and USB receipts cannot be deleted.
* MACRO_STEP selectors 2 (query) and 5 (download recording) require MACRO_STATUS. Other selectors require ACK. Expected shape derives from the stored request, not the incoming payload; read-only queries reject ACK and ERROR requires a nonzero defined error code.
* Five-second PC session loss cancels active macros, safe-stops active/running outputs and clears stale events. Local physical menu ownership survives. PC clients must query within the lease.

## Every registered request

| Requests | Implementation or explicit exclusion |
|---|---|
| 01 HELLO, 02 GET_STATUS | Profile/features and actual 25-byte snapshot; no fabricated temperatures/voltage/current. |
| 03 SET_STREAM, 04 GET_SETTINGS, 05 SET_SETTINGS | Validated board parameters, dual CRC banks, commit-last/readback, no implicit boot provisioning. Changed-only stream defaults off. |
| 10 BUZZER | Actual Timer1 driver, 20..20000Hz, positive duration; all existing melody cues retained. |
| 16 ADDRESSABLE_LED | ESP donor configure/stage/commit/pixel/fill adapter, real shared brightness, existing engine. Donor window 1..100; full 480 streaming remains PKAR. Direct AVR route rejects because AVR is not strip owner. |
| 30 MENU_ACTION, 37 MENU_SET_PAGE, 3D REMOTE_KEY_GESTURE | Real local/injected lifecycle and six hardware-appropriate pages. Down/repeated-hold acts immediately. Key monitor rejects edits. |
| 35 RESET | Safe shutdown, correlated ACK, deferred watchdog reset. Hint 0/1 is not programming. |
| 38 DISPLAY_TEXT | TM1637 text, raw segments, capture/release/metadata, one-shot/loop/interval, full 40-character scrolling, true-off, donor marquee lifecycle. Default non-repeating. LCD targets 1/2 return HardwareUnavailable. |
| 39 MACRO_START, 3A MACRO_CANCEL, 3B MACRO_STEP | Imported donor 128-byte ring/300-byte workspace, append/start/query, recording/read/replay/clear, timing, status/results, guarded dispatch for AVR-owned hardware. Recorded masks use actual shift command, not fictitious relays. ESP-owned LED steps in AVR-stored macros return HardwareUnavailable; correlated cross-owner forwarding is still open, not simulated success. |
| 3C FRONT_PANEL_GET | Donor schema-2 47-byte offsets; observed segments/brightness/presence, inputs/page/mode/capture metadata. LCD rows blank and absence flags clear. |
| 3E MENU_LIST, 40 MENU_LAYOUT_GET, 41 MENU_LAYOUT_SET | Stable six IDs, pagination, validated visible mask/permutation, persistence. CELL, OUT, KEYS, SND, DISP, HOST. |
| 45 PROGRAM_STATE | Application idle/running independent from durable flashing latch. |
| 46 AUTOMATION_LIST, 47 AUTOMATION_PUT, 48 AUTOMATION_REMOVE, 49 AUTOMATION_CLEAR | Imported donor 11-record dual-bank store/executor, exact 12-byte records, pagination/generation/CRC, budget/recursion guard. Boot/Host/all nine Key events; SafeStop/Buzzer/HostMacroRequest. Other hardware event/actions reject. |
| 06 TEMPERATURE_LIST | No sensor module: PAKAR_PC_SENSORS off. |
| 11 PWM_SET, 12 PWM_ALL_OFF, 15 PWM_GET | No matched PWM hardware; Timer1 reserved for buzzer: PAKAR_PC_PWM off. |
| 14 STATUS_RGB, 17 STATUS_EFFECT, 18 STATUS_PROFILE_GET, 19 STATUS_PROFILE_SET | No dedicated donor RGB hardware; ESP table is not substituted: PAKAR_PC_STATUS_RGB off. |
| 20..25 RF transmit/learn/start/cancel/clear/list/remove, 3F RF replace | No RF module; pin ownership conflicts: PAKAR_PC_RF off. |
| 31 RELAY_SET, 32 RELAY_SIDE, 33 RELAY_ALL_OFF, 34 RELAY_TEST | No donor relay sides/limits/motion hardware: PAKAR_PC_RELAYS off. Shift LEDs keep actual mask command, not relay safety semantics. |
| 36 I2C_TRANSFER | No I2C devices; A4/A5 occupied by TM1637: PAKAR_PC_I2C off. |
| 42 HOST_MENU_DIRECTORY, 43 HOST_MENU_CONTENT, 44 HOST_MENU_STATE_GET | Registry-only: donor ProtocolRuntime has no handlers. Unsupported, never fake ACK. Future host-directory/menu adapter remains work, not a dropped working donor handler. |
| Reserved 13, 26, 4A; unknown | Unsupported; never aliases a Pakar opcode. |

SEGMENT_CHANGED, BUZZER_CHANGED, keys, macro/automation events derive from actual state. Actual donor ControllerEvents encoders are imported; key [type,id,gesture,source,sourceId] and buzzer [frequency_LE16,duration_LE16,muted] remain exactly five bytes. No extra UART timestamps. Latent hardware event encoders remain available for future adapters without synthesizing absent producers. Busy-wire emits at most one unsolicited frame per service iteration and retains blocked events/results. Polling GET_STATUS grants one extra bounded changed-only mirror/event frame after its own response. Polling builds never touch busy GPIO.

## Settings profile

GET_SETTINGS: 21 bytes plus persisted boolean. SET_SETTINGS: first 21 only.

| Offset | Meaning |
|---|---|
| 0 | schema 0x50 |
| 1 | bit0 silent; bit1 boot welcome; bit2 save last menu page |
| 2 | TM1637 brightness 0..7 |
| 3..4 | event stream LE16; 0 or 100..65535ms |
| 5..6 | ADC6 threshold LE16, 1..1023 |
| 7 | default page 0..5; visible required |
| 8 | nonzero six-bit visible mask |
| 9..11 | packed six-page permutation, low nibble first |
| 12 | board-name length 0..8 |
| 13..20 | printable ASCII board name; zero unused tail |
| 21 | persisted boolean |

AVR board name is not ESP DNS identity. Existing device rename command controls ledboard.local. Settings banks 32..127; automations 128..411; Prog disjoint. Deferred edits coalesce for 1500ms. Failed immediate saves preserve earlier pending edits and valid bank.

## CLI/TUI/API

Hostname default http://ledboard.local. PAKAR_ENDPOINT, PAKAR_USER, PAKAR_PASSWORD configure client without hardcoded credentials.

    PakarController.exe --command avr contracts
    PakarController.exe --command avr settings
    PakarController.exe --command avr set display-brightness 7
    PakarController.exe --command avr set welcome on
    PakarController.exe --command avr set button9-threshold 512
    PakarController.exe --command avr set board-name AVR
    PakarController.exe --command avr status
    PakarController.exe --command avr panel
    PakarController.exe --command avr command MENU_LIST 00
    PakarController.exe --command avr command BUZZER b801fa00

TUI: same commands without --command. Typed set: GET current, validate, SET once, require ACK, GET persisted matching readback. Generic commands accept actual donor names or registered numeric/0x values; not reserved IDs. The client consumes each validated success/error receipt so repeated settings edits do not exhaust the eight slots. Cleanup failure does not undo a real ACK or retry the write; the receipt instead expires normally. Unknown outcomes and mismatched reboot identities are left available for inspection.

Authenticated HTTP:

* GET /api/avr/contracts: current support/features/profile/boot identity.
* POST /api/avr/contracts, form opcode=16&payload_hex=b801fa00: 202 queued receipt.
* GET /api/avr/contracts/result?id=ID: queued/awaiting/responded/outcome_unknown.
* DELETE /api/avr/contracts/result?id=ID&opcode=DECIMAL&boot_id=BOOT: explicitly consume your terminal receipt after verifying its identity and response. Authenticated, exact boot/request/opcode match required; 409 for pending/mismatched/missing receipts. An explicit caller may consume an outcome_unknown receipt after inspecting it; this never retries or cancels its possible device mutation.

Authenticated inbound WebSocket and configured MQTT/outbound WebSocket share bounded queueing:

    {"command":"pc-contract","opcode":16,"payload_hex":"b801fa00"}

Results/events publish without falsely completing an AVR request. USB uses existing COBS/CRC outer frame and same tunnel/chunk dialect. Dedicated native/Web/mobile editors for every new AVR contract are **not** claimed complete. Live broker/network/hardware/multi-client acceptance remains pending.

## Feature flags

Defaults: PAKAR_USE_BUSY_WIRE=1, PAKAR_PC_MENU=1, PAKAR_PC_MACROS=1, PAKAR_PC_AUTOMATIONS=1, PAKAR_PC_ESP_LED=1. Disable individually through compiler.cpp.extra_flags. PAKAR_KEY_DEBOUNCE_MS defaults donor 20ms, accepts 1..1000.

Future guards default zero: PAKAR_PC_I2C, PAKAR_PC_SENSORS, PAKAR_PC_PWM, PAKAR_PC_RF, PAKAR_PC_RELAYS, PAKAR_PC_STATUS_RGB, PAKAR_PC_AVR_LED. **Fail-closed integration guards, not working drivers:** setting one to 1 rejects compilation until an adapter, conflict-free pin profile, memory budget and hardware acceptance are supplied. A macro cannot enable absent hardware.

AVR LED engine intentionally off: duplicates effects and consumes RAM/UART; ESP owns current strip contract. Future enablement must preserve ISP/UART/buzzer Timer1/TM1637 and advertise only installed capabilities. The donor host-directory IDs above also require an implementation, not merely an enabled menu.

## Verification boundary

Production-code host tests: dispatch/error isolation, feature-off profiles, Prog safety, keys, macros/record/replay, automation budget/ninth-key, settings/automation torn writes, TM1637 ACK/reconnect/cache/off, display capture/40 characters, chunk matching/expiry/rollover, per-client receipts, polling mirrors, no retries. CLI tests: authenticated loopback fixture, not board.

ASUS SSH localhost:1022 refuses connection. No physical firmware/EEPROM write in this delivery. Do not disconnect USB based on source tests. Flashing/electrical checks, Wi-Fi/cloud acceptance and two-machine rollout remain unchecked.
