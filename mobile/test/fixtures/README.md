# Loopback TLS fixture

`lan_tls_cert.pem` and `lan_tls_key.pem` are deliberately public, generated
test-only credentials for localhost. Never use them outside the loopback test
server or deploy them with firmware/application assets.

The fixture tests default rejection of an untrusted certificate, explicit
test trust of this certificate, and rejection of a mismatched hostname.
Production code has no certificate-verification bypass.
