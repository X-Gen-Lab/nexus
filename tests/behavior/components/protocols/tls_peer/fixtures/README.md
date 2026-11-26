# Insecure host TLS fixtures

These DER certificates and private keys exist only for the isolated Native
mutual-authentication contract tests. They are public test data, provide no
confidentiality, and must never enter firmware, manufacturing credentials or a
Source SDK. The fixture server uses `server.test`; the client uses `client.test`.

The injected trusted UTC is 1800000000 Unix seconds, independent of the host
calendar and the test's advancing virtual monotonic deadline. Expiry tests use
1900000000. The peer process supplies explicit operating-system test entropy;
the Nexus client uses a deliberately insecure deterministic test source with an
explicit fixture-only quality declaration. Neither qualifies an MCU entropy
source, physical transport, secure time or production trust policy.

Keys are P-256, certificate signatures ECDSA/SHA-256. The CA and leaf certificates
were generated for this test on 2026-11-02; the raw OpenSSL generation receipts
are retained with the originating execution evidence. `wrong-ca.der` is a
different CA and verifies that a valid peer is rejected under untrusted roots.
