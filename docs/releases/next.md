# PakarController development packages

- Contextual Windows workspace and embedded device-settings contract.
- ESP32-S3 + ATmega328P buffered firmware pair; AVR polling and busy-wire build coverage.
- ESP8266 source/tooling preserved as unfinished support, excluded from default
  release artifacts because the full image exceeds its RAM/flash budgets.
- Android preview; not production-signed or feature-complete.
- Repository health, security analysis and hash/provenance packaging.

This is a **draft prerelease**. Physical and full production acceptance remain
open in issue #2. Match both board images to the same link-mode; preserve a verified
backup before programming. CI never applies these files to connected boards.
