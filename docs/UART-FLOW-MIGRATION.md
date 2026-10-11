# Paired UART migration checkpoint

This is source/native-fixture/AVR-compile evidence, not an S3 firmware release,
physical link acceptance or permission to remove the old busy wire yet.

## Build modes

| Mode | AVR flags | Unsolicited transmission |
|---|---|---|
| busy-wire (unchanged default) | PAKAR_USE_BUSY_WIRE=1, PAKAR_UART_BUFFERED=0 | Existing LOW-ready wire; INPUT_PULLUP retained |
| polling | PAKAR_USE_BUSY_WIRE=0, PAKAR_UART_BUFFERED=0 | Status replies only; no busy GPIO |
| buffered (S3 preparation) | PAKAR_USE_BUSY_WIRE=0, PAKAR_UART_BUFFERED=1 | Whole-frame TX space required; no busy GPIO |

Use `./tools/build-firmware.ps1 -Target avr -LinkMode buffered`, or
`./build.sh buffered avr`. The wrapper rejects buffered ESP8266/both targeting:
the current SoftwareSerial application must not be paired accidentally with an
unsolicited AVR sender. Boolean/conflicting flags fail at compilation. Each
mode reuses its stable existing board/profile cache; no forced clean.

AVR still uses hardware Serial at38400, TM1637 A4/DIO+A5/CLK, buzzer D9 and existing
peripheral pins. A dirty game-state remains armed when its send fails. Successful
write is not peer ACK, LED commit or physical state. Existing HELLO/GET_STATUS
convergence remains necessary; no device mutation is automatically replayed.

## Actual donor use and safety adaptation

Canonical PCController was clean at c26cc561d2bf353a948312f89f59d90dbda8e38d;
its1211-file inventory was refreshed. All twelve prior reviewed hashes match.
Completely read Project/UartProtocol.h, Project/UartProtocol.cpp and
Tools/VirtualBoard/tests/uart_protocol_test.cpp; also read their ProtocolCodec.h
and ProtocolContract.h dependencies. Three new complete-read comparisons with
explicit gaps were added to the same ledger, not a new donor-completion claim.

Shared SerialLink adapts the actual entry-occupancy receive snapshot and parser
recovery behavior. It now also caps one pass at64 bytes for larger S3 RX rings,
leaving time for audio/display/keys under sustained input. Negative availability
or a missing read cannot become a fake0xFF byte. Existing COBS+CRC,48-byte payload,
reserved-zero integrity gate and explicit opcode adapters remain unchanged.
Existing transport counters are now available for ESP32 as well as ESP8266.

A failed partial write remains outcome-unknown. Before a later independent
frame, the sender restores the COBS boundary with a delimiter, without resending
the earlier payload. That delimiter can complete an already CRC-valid prefix;
it is NOT proof that the earlier operation was rejected or unapplied. Existing
deadline/ACK/sequence quarantine/readback safety must remain authoritative.

Zero-copy/in-place scratch optimization, separate framing/CRC/response counters,
donor timestamp suffixes and complete event-runtime/client projection remain
explicit pending omissions. Do not wholesale copy a different raw opcode/schema
into this living unversioned contract or call those omitted parts business logic.

## Verification

- Actual shared-header tests pass for busy-wire, polling and buffered+ESP32:
  representative/maximum payloads, zeros, optional opcode dispatch, nested ACK,
  split receive/transmit safety,2048-byte noise yielding, delimiter recovery,
  malformed/CRC rejection, negative read/occupancy, partial-write boundary repair,
  explicit Stream pin/buffer ownership and counters. One tiny stable executable.
- Full local repository-quality phase passed for the flow change; ledger, build
  dispatch, fail-closed buffered ESP8266 rejection and versions0.2.7 agree.
- Final AVR compiles: buffered26840bytes, busy-wire26958bytes, polling26670bytes;
  static SRAM1226/2048bytes each. These are compiler reports, not maximum stack
  measurements or electrical/clock acceptance. All merged Urboot images32768bytes.
  Final merged image SHA256:
  buffered4533cc7556806c1af876f02962e1193ad4cbc8515396679eeaf1be12cbbf1147;
  busy-wireff5367944aa3d35bf4fbc938dec678c55577ad2601bbfda0ec3160222ef66a6f;
  polling10d848351cc48b718f44c2be94f30b01175ad4307110f6e24559a477414ed7d6.
- Prior diagnostics checkpoint60d8668 passed complete CI38140468478, including
  Windows, both firmware variants and Android. f0adf3a/new-flow CI are separate;
  no wait or all-green inference from the prior head.
- ASUS SSH1022 refuses and ledboard.local does not resolve. Desktop67948be and
  settings were preserved byte-identical. No board write, desktop replacement,
  APK rebuild or new dependency tree in this flow pass.

Next: integrate this profile with the real S3 shared application and buffered
UART adapter, maintain readback/reconnect/overflow ownership, then measure paired
UART/LED/ISP/OTA contention and event loss on the confirmed DevKit hardware.
