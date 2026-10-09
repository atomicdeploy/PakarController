# Firmware toolchain

Both supported Windows build hosts should update their package indexes and use
the latest compatible releases of Arduino CLI, MiniCore, the ESP8266 core,
and FastLED. Versions are observed in verification output rather than
hard-coded as build policy.

On a host carrying the project-local `tools/arduino-cli.exe`, that binary takes
precedence; otherwise the build resolves Arduino CLI from `PATH`. The
executable itself is ignored by Git and must come from the official Arduino CLI
distribution.

The UART envelope carries its own schema version so dependency upgrades do not
silently change the on-wire contract.
