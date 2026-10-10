# Security policy

There is no production support guarantee yet. Use current reviewed source and
do not expose the default first-install credentials to an untrusted network.
Change device access credentials before deployment. Firmware writes require
explicit owner intent, matched board/link-mode and verified image integrity.

Please use GitHub's **Security → Report a vulnerability** on this repository for
private disclosure. Do not put credentials, sensitive telemetry, EEPROM contents
or exploit details in a public issue. If private reporting is unavailable, request
a private contact channel without disclosing the vulnerability publicly.

CodeQL currently covers Actions and JavaScript only. Build tests and SHA-256
checksums are not authentication, a firmware signature, or proof of device safety.
