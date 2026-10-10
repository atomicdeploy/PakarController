# Firmware toolchain

Both supported Windows build hosts should update their package indexes and use
the latest compatible releases of Arduino CLI, MiniCore, the ESP8266 core,
FastLED, and arduinoWebSockets. CI currently reproduces the verified dependency
set recorded by `sketch.yaml`; host builds may move forward after both link
variants and physical transports pass.

On a host carrying the project-local `tools/arduino-cli.exe`, that binary takes
precedence; otherwise the build resolves Arduino CLI from `PATH`. The
executable itself is ignored by Git and must come from the official Arduino CLI
distribution.

The UART envelope carries its own schema version so dependency upgrades do not
silently change the on-wire contract.

`PAKAR_LINK_MODE=polling|busy-wire`, `build.cmd polling|busy-wire`, and the
equivalent PowerShell/Linux entrypoints all select the same
`PAKAR_USE_BUSY_WIRE=0|1` compile-time flag on both boards.
