# USB demand and native OTG

USB uses the same unversioned COBS/CRC8 Pakar frames, peer admission and boot-bound
commands as the existing serial transport. USB does not require Wi-Fi. A phone or
tablet is the USB host; the ESP32-S3 is a CDC ACM device, not a USB peripheral host.

## Select the physical transport

`tools/build-firmware.ps1 -Target esp32s3 -S3HostTransport` accepts:

| Value | Framed application endpoint |
|---|---|
| `uart0` (default) | Existing UART0 USB bridge; production routing is unchanged. |
| `hwcdc` | S3 native hardware CDC/JTAG port. |
| `otg` | S3 native TinyUSB CDC ACM port for Android/PC hosts. |

UART0 images keep `build/esp32s3-buffered`; nondefault images use
`build/esp32s3-buffered-otg` or `build/esp32s3-buffered-hwcdc` so the existing
updater's default image path cannot silently select another connector. All three
reuse `build/cache/esp32s3-buffered`; these are output variants, not extra caches.

Keep the board's established flash, PSRAM, LED, Ethernet and ISP build options.
Native USB uses GPIO19 D− and GPIO20 D+ through the DevKit's native USB connector,
not its UART bridge connector. AVR hardware UART1 remains separate. The OTG image
owns CDC initialization, disables automatic line-state/1200-baud reboot before
enumeration, and identifies its product as `PakarController`; SDK VID/PID stay
unchanged. Manual BOOT/RESET and UART0 ROM recovery remain separate from this API.

Android lists only the explicitly identified PakarController Espressif CDC pair,
requires OS USB permission, and still validates HELLO/board identity before control.
It does not claim CP210x/CH340 driver support or silently fall back to networking.
The initial Android USB surface supports the table-selection board and its
acknowledged cell/pixel, melody, display and AVR Silent controls. USB firmware/ISP,
storage transfer and network/cloud/identity settings are not yet exposed. The
active live-state subscriber requests bounded metadata snapshots; pixels are
included only with actual preview/diagnostic demand. Android does not yet preempt
a snapshot for a command: overlapping operations fail explicitly rather than
queue or replay an uncertain write.

Check VBUS/external-supply isolation before attaching an Android host: USB supplies
power too. The controller's external 3.3 V supply must not backfeed the host. A
successful build does not verify a particular DevKit's connector or power circuit.

## Demand lease

Opcode `0x64` accepts `[flagsLE16, leaseMsLE16]`; correlated response `0xBA` returns
effective flags and remaining milliseconds. Flags are Status=1, Panel=2,
Peripheral=4, Pixels=8. Leases are 250–5000 ms, normally 3000 ms renewed while a real
consumer exists. Zero flags with zero duration releases the lease. Invalid flags
or durations reject and clear it. HELLO, native disconnect, expiry and short TX
also retire demand; UART0 uses expiry because bridge cable/DTR state is invisible.

Pixels enables RGB inclusion only in an explicitly requested immutable table
capture; it never starts automatic full-frame transmission. Hidden/background
clients must release demand, and crashes cannot leave it enabled indefinitely.
Explicit identity, status and table requests, command replies and post-command
verification do not depend on optional unsolicited status subscriptions.

Boot-fenced TableCommand `0x5E` can contain pixel diagnostic `0x65` with
`[indexLE16, count, R, G, B, off]`. Count is one or six; six-pixel groups must be
aligned. Existing renderer/programming/red-lock guards still apply. A compact
ACK is not RGB proof: clients request and compare the complete published frame.

## Acceptance boundary

Host fixtures, native worker tests, Dart transport tests and actual S3/Android
compilation are separate from physical USB permission, enumeration, unplug,
background/resume, power safety and LED acceptance. Production firmware changes
still require the current PBX-DTMF approval; this feature does not authorize one.
