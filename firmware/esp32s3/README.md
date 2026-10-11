# ESP32-S3 migration adapters — not a production firmware yet

The shared production application currently lives in `../esp8266` and the AVR
application in `../avr`. These S3-specific adapters start the hardware boundary;
they do not replace the integrated application with an ISP-only image, advertise
unfinished features, or certify an S3 build/physical deployment.

`src/BoardPins.h` is a conservative, overridable **provisional** DevKitC-1 GPIO
plan. `src/AvrUart.h` owns UART1 with explicit pins/buffers, reuses the actual
BoardCore `SerialLink`/COBS/CRC codec and counts hardware errors separately.
`src/MenuAudio.h` uses existing AVR cue IDs, per-feature HELLO acknowledgement,
bounded cadence and correlated ACK; transmission success is not playback proof.
The integrated menu must call this adapter after a navigation action, not from
idle repaints. Shared native/Web/Flutter menu feedback integration is pending.

Keep USB host serial and AVR UART physically/logically separate; framing does not
make a multi-drop UART bus. No raw diagnostic text may corrupt either binary path.
The current AVR no-busy build still disables unsolicited events: a paired explicit
buffered-UART build flag and overflow/reconnect testing are required before removing
polling. Do not simply disconnect the wire on the existing busy-wire AVR image.

LED plan: reuse the current `LedGrid` effects/mapping/stream ownership and FastLED
3.10.6 RMT5 backend with Arduino-ESP32 3.3.12 (both installed here). Its actual
`channel_driver_rmt.cpp.hpp::createChannel` attempts S3 DMA for the first nonempty
TX channel and may fall back to non-DMA. That is not unconditional DMA proof.
No second effects engine or 20 FPS software cap. Runtime DMA status, shared GPIO
selection, UART stress under Wi-Fi, and channel timing/color-order acceptance
remain required before a release claim.

Port before enabling the production build target: network/mDNS/SSDP/NetBIOS;
ESP8266 flash-image validation and partition-aware OTA/rollback; ESP flash read
and integrity APIs; BearSSL TLS to S3 trust validation; LittleFS partition sizing;
shared ISR/task ownership for AsyncTCP; telemetry hardware fields; USB CDC framing;
ISP explicit SPI pins and durable AVR/EEPROM recovery. Unsupported paths fail
closed rather than borrowing ESP8266 success. See the migration delivery document.
