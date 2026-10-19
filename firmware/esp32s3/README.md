# ESP32-S3 migration adapters — not a production firmware yet

Canonical UART wiring: GPIO17/TX -> AVR D0/RXD; AVR D1/TXD -> GPIO18/RX.
This supersedes the earlier provisional reversed pair. ISP remains MOSI11,
MISO13, SCK12, RESET-control10 through a voltage-safe driver; see the migration
document for electrical protection and the exact module confirmation gate.

Optional W5500 uses a separate SPI3/HSPI bus: SCK5, MISO6, MOSI7, CS8,
RSTn9, INTn14. It must not share the AVR ISP bus/object. `-S3Ethernet` enables
the native ETH/lwIP adapter; without that build flag no W5500 bus is allocated.
New profiles leave Ethernet disabled until explicit commissioning; existing old
active Ethernet profiles migrate without being disabled or rewritten at boot.
Confirm module supply input before powering it. Actual wiring/link and Ethernet
acceptance remain open in issue#19; compiling the driver is not a cable test.

The saved Ethernet profile has independent enable/DHCP/static/DNS settings at
`GET/POST /api/network/ethernet`, contextual Native/Web/Flutter editors, observed
link/IP/speed/duplex and bounded LLDP diagnostics. `POST /api/network` can disable
both station and AP only when Ethernet actually has an IPv4 link. Disabling
Ethernet requires a connected Wi-Fi station or an actually started recovery AP.
USB recovery remains independent. No uncertain command is replayed onto a
different route. Serving, WebSocket, OTA, TLS, MQTT and remote firmware use the
same native IP stack and existing owners, not a separate WIZnet socket stack.

LLDP advertises link identity every30s (TTL120s). Receiving neighbors is an
explicit `lldp_receive` opt-in: the pinned W5500 driver filters non-IP multicast
in normal MAC-filter mode, so reception requires promiscuous Ethernet. At most
four sanitized neighbors are retained; no raw-frame capture API or layer2 bridge
is exposed. TX/filter changes run on the TCPIP thread, and the application-only
input hook preserves the SDK's original buffer/length/private-info ownership.
Actual peer/switch visibility and simultaneous Wi-Fi/Ethernet tests remain open.

S3 NBNS answers local UDP137 name and node-status queries on exact ready netifs
with bounded nonblocking service, rather than the SDK's global AsyncUDP task.
Hostnames longer than15 characters are not silently truncated; readiness and
bind state are reported in status. This is not SMB/WINS/LLMNR support. Existing
mDNS/DNS-SD, SSDP description/lifecycle and MNDP use the same stable hostname.
`tools/check-router-upnp.ps1` performs bounded read-only IGD discovery/status;
router port-forwarding acceptance is separate from board SSDP discovery and is
not inferred from WAN reachability or a successful host fixture.

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

The original migration checklist (not a claim these are still unported):
network/mDNS/SSDP/NetBIOS;
ESP8266 flash-image validation and partition-aware OTA/rollback; ESP flash read
and integrity APIs; BearSSL TLS to S3 trust validation; LittleFS partition sizing;
shared ISR/task ownership for AsyncTCP; telemetry hardware fields; USB CDC framing;
ISP explicit SPI pins and durable AVR/EEPROM recovery. Unsupported paths fail
closed rather than borrowing ESP8266 success. See the migration delivery document.
