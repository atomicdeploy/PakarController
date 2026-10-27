# Source handoff

The consolidated source includes the ESP32-S3/shared ESP application migration,
AVR maintenance and radio work, runtime LED backends, pixel corrections/custom
groups, semantic state/effects, Rust and Flutter controls, and the single canonical
Web application. Existing donor and production acceptance ledgers remain open;
publishing source is not a new hardware deployment or complete feature acceptance.

## Release targets

The owner selected **ESP32-S3 + ATmega328P** as the release targets. The primary
workflow builds S3 buffered UART plus all three AVR link profiles, packages the
matched S3/AVR buffered pair, and retains native/Android and security checks.

ESP8266 source, pins, direct compiler commands, optional compression/reporting
and explicitly requested legacy packaging are preserved, not silently removed.
Its full current polling build does not fit: the measured `.bss` end is
`0x3fffdd20`, beyond the `0x3fffc000` DRAM boundary by 7,456 bytes; `.irom0.text`
is `0x1054c4` bytes, beyond its instruction mapping. This remains an unfinished
support/resource optimization item, not a successful build or hardware release.
No physical memory, OTA-slot or Urboot limits were enlarged to hide a failure.
The unfinished legacy checklist is [issue #25](https://github.com/atomicdeploy/PakarController/issues/25).

## Continue on another computer

Pull the repository's `main` branch only after the synchronization receipt confirms
its final commit. Use the canonical build wrappers and existing dependency pins;
do not copy another machine's build caches, settings or executable into source.
Review `AGENTS.md`, `docs/REUSE-PREFLIGHT.md`, `docs/WEB-PARITY.json` and issue 2
before continuing. The Web live-preview lease helper is staged and tested, but is
not yet embedded or wired to controls.

## Local-only data deliberately not published

`outputs/`, `build/`, `.build/`, native/Flutter caches, installed apps and device
settings are not source. They can contain private Wi-Fi profiles, firmware/EEPROM
backups and host-specific receipts. They are preserved on the original machine,
not deleted or treated as remotely available. Provision secrets privately on the
next machine. Production firmware/settings backups must remain local unless a
separate approved private transfer is arranged.

Historical one-off `outputs/PakarController/staging/keycontroller4` UART/ISP and
four-box scripts are preserved locally rather than promoted to runnable tooling:
they reference obsolete host ports, relative staging dependencies or fixed board
profiles. Continue with canonical `tools/capture-avr-rx.ps1`, `tools/serial-wire.ps1`,
`tools/invoke-usb-api.ps1`, `tools/update-firmware.mjs` and the firmware build
profiles instead. Generated cache sketches are not independent authored source.

No firmware update, reset, LED actuation or installed-client replacement is
authorized merely by pulling this checkpoint. The production hold remains.
