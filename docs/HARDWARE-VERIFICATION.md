# Hardware verification

This is the current evidence ledger for the connected NodeMCU and ATmega328P.
It deliberately separates electrical observations from source/build results.

## Confirmed

- ESP ISP reads the ATmega328P signature as `1E950F`.
- The full PakarController ESP application entered ISP, backed up the target,
  wrote the AVR application plus Urboot as a complete 32,768-byte image, and
  byte-verified it without installing a separate programmer image. The built
  merged image SHA-256 is
  `C713E5F2B94CE1999ECA5B6DDE6E9FBD038F200B502D3F8FABFE1025BC78EBB5`.
- EEPROM was preserved with SHA-256
  `5F4ECDB7B71C3E403983FE405CDDCDC2F2576B655FDB3E80D94A6F7C32E58BC2`.
- Fuse/lock bytes remained `LFUSE=F7`, `HFUSE=D7`, `EFUSE=FD`, `LOCK=3F`.
- AVR UART0 and ESP SoftwareSerial communicate bidirectionally at 38400 baud.
  During the full-brightness 480-pixel ESP rainbow, 129 consecutive polled
  replies arrived with both live protocol-error counters at zero. After the
  later OTA/ISP reset sequence left cumulative counters at ESP 3 / AVR 6, a
  further 272 animated response frames arrived over 30 seconds with neither
  counter increasing; reset-transition history is therefore kept visible but
  steady-state communication is clean.
- The Timer1/OC1A buzzer output on AVR D9 is electrically audible. The exact
  imported nonblocking PCController welcome sequence is queued on boot/reset.
- NodeMCU D2/GPIO4 drives the connected strip directly at 3.3 V logic. The
  user confirmed the full-brightness rainbow; the temporary AVR D6 comparison
  renderer has been removed from production. The selected brightness/effect
  now survive ESP OTA reboot in checksum-backed LittleFS.
- The PuzzleBoard TM1637 wiring (`DIO=A4/SDA`, `CLK=A5/SCL`) now returns a live
  ACK and the user confirmed that the module visibly shows `1`. The prior
  PCController D11/D13 mapping was wrong for this board.
- The ESP recovery AP and station client run concurrently. The connected unit
  retained `192.168.4.1` while associating as `10.247.0.160`, and its
  authenticated API was exercised through the station address.
- ESP HTTP self-OTA completed twice, including an effect-persistence reboot.
  The AVR integrated update completed backup, legacy migration cue, full
  program/verify, new Prog-latch arm/clear, UART recovery, and success response.

## Human-confirmed physical output

- TM1637 visibly shows the selected cell `1` on the corrected A4/A5 wiring.
- The ESP-driven strip visibly runs the full-brightness rainbow correctly.
- The AVR Timer1 buzzer is electrically audible; melody quality is therefore
  human-observation evidence rather than an API inference.

## Build consistency

Both Windows hosts use Arduino CLI 1.5.1, ESP8266 core 3.1.2, MiniCore 3.1.3,
and FastLED 3.10.6. The final tested ESP OTA image SHA-256 is
`67921773E5D992D106E0F8445E268F3E355FC61E46A7A547E90399BD168A2E70`.
