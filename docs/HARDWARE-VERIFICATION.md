# Hardware verification

This is the current evidence ledger for the connected NodeMCU and ATmega328P.
It deliberately separates electrical observations from source/build results.

## Confirmed

- ESP ISP reads the ATmega328P signature as `1E950F`.
- AVR application plus Urboot was written as a complete 32,768-byte image.
  Separate readback SHA-256 equals the built image:
  `BACB6343BF9C5836AFD68A46EDC80E404AC01A0CB05703190BF61A22565C210E`.
- EEPROM was preserved with SHA-256
  `5F4ECDB7B71C3E403983FE405CDDCDC2F2576B655FDB3E80D94A6F7C32E58BC2`.
- Fuse/lock bytes remained `LFUSE=F7`, `HFUSE=D7`, `EFUSE=FD`, `LOCK=3F`.
- AVR UART0 and ESP SoftwareSerial communicate bidirectionally at 38400 baud.
  During the full-brightness 480-pixel ESP rainbow, 129 consecutive polled
  replies arrived with both live protocol-error counters at zero.
- The Timer1/OC1A buzzer output on AVR D9 is electrically audible. The exact
  imported nonblocking PCController welcome sequence is queued on boot/reset.
- NodeMCU D2/GPIO4 drives the connected strip directly at 3.3 V logic. The
  temporary AVR D6 comparison renderer has been removed from production.

## Open physical check

- The TM1637 module remains dark and does not acknowledge. Both the imported
  PCController transport and an independent probe use the authoritative pins:
  DIO on AVR D11/PB3 and CLK on AVR D13/PB5. This now points to module power,
  common ground, reversed CLK/DIO wires, a connector fault, or a failed module.
- A human observation is still required to confirm that the final flashed
  welcome sequence and faster full-brightness rainbow look and sound correct;
  serial evidence confirms that both commands are running but cannot prove
  light or sound output by itself.

## Build consistency

Both Windows hosts use Arduino CLI 1.5.1, ESP8266 core 3.1.2, MiniCore 3.1.3,
and FastLED 3.10.6. The complete firmware source trees are hash-identical on
both hosts. The AVR build is byte-identical; the flashed ESP binary SHA-256 is
`53A8A0456F0FB320129DF08B4247F8C1F793E3CD6805C3C384F46113D3E32976`.
