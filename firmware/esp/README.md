# Shared ESP application

ESP32-S3 is the primary hardware target. `application.ino` and `src/` own the
single application implementation used by the target sketches in `../esp32s3/`
and `../esp8266/`. Build a target with `tools/build-firmware.ps1`, not this folder.

Target-specific S3 hardware adapters remain in `../esp32s3/src/`. The ESP8266
folder retains only its compatibility sketch, compile wrappers and SDK profile.
Source files were moved with `git mv`; use `git log --follow` on a file to inspect
its earlier history. Binary names and reusable build-cache paths remain unchanged.
