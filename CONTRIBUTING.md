# Contributing to PakarController

Use the existing issue checklist; label the board, interface and verification
scope. Source parity is not physical acceptance. Keep donor repositories read-only.
Follow `AGENTS.md` and the donor-first preflight when those files are present.

Use one incremental cache, the locked build wrappers and affected-component tests.
Never clean/rebuild all targets as a routine fix. Generated UI/branding and source
comparison reports must be regenerated from their authorities, not edited to
bypass a failed gate. Never commit Wi-Fi passwords, MQTT credentials, EEPROM
backups, private telemetry or signing material.

Submit a focused PR with exact donor paths, retained behavior, justified omissions,
test commands/results and remaining visual/physical acceptance. Include screenshots
for UI changes; label fixtures clearly. No test, CI or screenshot implies a flash.
Use capability acknowledgements instead of hardcoded board assumptions.
