# Security provider and authenticated configuration records

`nexus_security` routes cryptography through a maintained implementation. Native
builds use OpenSSL 3 (CSPRNG, AES-128/256-GCM, SHA-256 and Ed25519 verification).
No AES round function, PRNG fallback or signing credential exists in this module.
An MCU port must register a reviewed provider backed by a maintained crypto library
or hardware implementation and an actual entropy source. Until then operations
return `NX_CRYPTO_UNSUPPORTED` / `CONFIG_ERROR_UNSUPPORTED`.

Register and change providers only while callers are quiescent. Provider callbacks
are independent capabilities: a Config AES-GCM provider needs random, seal, open
and SHA256 but need not implement the optional Ed25519 verifier. Every missing
operation fails with UNSUPPORTED, and an empty provider cannot be registered.
Provider callbacks
and context are borrowed and must stay alive. Calls are task-only and may allocate
or block; a management task owns them and enforces the product execution budget.
AEAD messages and AAD are limited by `NX_CRYPTO_MAX_AEAD_SIZE` (default 4096 bytes).
Authenticated open uses bounded scratch allocation and only copies plaintext to
the caller after successful authentication. Scratch and retired keys are erased
with volatile writes. This is a software erasure contract, not protection against
debug access, RAM dumps, hardware faults or privileged code. Key vaults, debug
policy, per-device provisioning, entropy characterization and side-channel review
are separate product/MCU responsibilities.

## Configuration record v1

The serialized format is explicit little-endian metadata, not a native C struct:

| Offset | Length | Field |
| --- | --- | --- |
| 0 | 4 | `NXCF` magic |
| 4 | 1 | Format version (1) |
| 5 | 1 | Algorithm (1 = AES-128-GCM, 2 = AES-256-GCM) |
| 6 | 1 | Nonce length (12) |
| 7 | 1 | Authentication tag length (16) |
| 8 | 16 | Public key ID |
| 24 | 4 | Plaintext length |
| 28 | 12 | Random nonce |
| 40 | plaintext length | Ciphertext |
| 40 + plaintext length | 16 | Authentication tag |

AAD contains the complete 28-byte header, namespace byte, value type byte, key
length byte and exact key bytes. Changing ciphertext, tag, key name, namespace,
value type, key ID, algorithm or record length fails authentication or parsing.
Legacy unauthenticated CBC records are rejected. Existing deployments need a
separate explicitly authorized offline migration retaining their original source;
this module does not guess formats or silently downgrade authentication. Device
binding is supplied by per-device key provisioning; a shared key across devices
would permit records with the same key/namespace/type to move between them.

Every encryption requests a new 96-bit CSPRNG nonce. Key bytes do not seed the
CSPRNG, and setting/reloading a key does not restart an entropy sequence. Adjacent
repeated nonces from a broken provider are rejected, and each loaded key is limited
to 2^20 encryption invocations per process. Random nonce collision resistance is
probabilistic; this limit is not a durable fleet-wide usage counter. The product
must enforce its total key-usage budget across resets/devices and rotate keys,
or implement a reviewed persistent nonce allocation protocol. MCU entropy source
reset behavior and hardware performance have not been verified by Native tests.

## Recoverable rotation

Configuration and keyring access must be serialized by the management owner.
The keyring has four slots. Public IDs are domain-separated SHA-256 of algorithm
and key, truncated to 16 bytes; they are metadata, not credentials. `config_set_encryption_key`
selects an existing generation or replaces the keyring for an unknown generation.
Use `config_register_encryption_key` to reload multiple generations explicitly.

The product securely persists both old and new keys before rotating records.
Rotation authenticates every old record and stages every new record without
changing the store, then replaces the RAM records as one validated batch and
commits a whole backend snapshot when configured. Staging/entropy/authentication
failure leaves records untouched. A reported commit failure restores the old RAM
records and active key while retaining both registered generations, because an
I/O error can be ambiguous after the durable commit marker was written.

After restart, reload the externally protected old/new generations before
`config_load`, then mount and read the recovered snapshot. Only after durable
read-back confirms migration may the product retire the old vault key. The
`config_retire_encryption_key` guard checks current RAM references; it cannot
check old flash bank snapshots or external recovery copies. The service never
stores keys alongside ciphertext. If the product fails to preserve/reload the
keyring, persistence alone cannot recover encrypted records.

Rotation workspace grows with the number and actual sizes of encrypted values
(two record copies plus metadata); allocation failure is explicit. Configuration
record sizes include the 56-byte AEAD overhead and must fit the configured store
limit. Firmware and ISR/control loops must not use rotation synchronously.

## Execution evidence

`tests/storage_security/test_crypto.c` uses NIST AES-GCM known-answer vectors,
SHA-256 and RFC 8032 Ed25519 verification, plus tamper/no-plaintext and entropy
failure cases. `test_crypto_disabled.c` compiles the boundary with no default
provider and verifies fail-closed behavior. Config tests cover all record bytes,
context binding, complete multi-value rotation, all-or-nothing authentication and
entropy failures, reloaded generations and key-retirement guards. These are
Native software checks, not MCU entropy qualification, FIPS validation or HIL.

## Validated import/export

JSON exports encode every encrypted record as hex with `encrypted: true`, whatever
its logical type. Import decodes and authenticates the complete record before it
can reach the live store. Type/value/encrypted field order does not matter. JSON
strings use UTF-8 and real Unicode escape decoding, including surrogate pairs;
invalid UTF-8, embedded NUL, malformed escapes, unknown fields/nesting, duplicate
entries, integer overflow, negative unsigned values and nonfinite JSON floats
are rejected. Escaping streams into the caller buffer, so long control-character
values expand completely instead of being silently truncated.

Binary export v2 uses fixed offsets and little-endian metadata and scalar values;
no C struct padding is serialized. Header: magic value `0x43464742` (4 bytes),
version 2 (1), reserved zeros (3), entry count (4), exact data length (4). Entry:
key length (1), type (1), flags (1), namespace ID (1), value length (2), key bytes
and value bytes. Plain float values require IEEE-754 binary32; authenticated
records, strings and blobs remain exact bytes. V1 is rejected. A DECRYPT export
requires all records to authenticate and reports the actual resulting data size.

Imports stage validated values and apply one RAM batch after the complete frame
has parsed. Failure, including CLEAR imports, leaves live values unchanged;
optional auto-commit failure restores the old RAM batch. SKIP_ERRORS explicitly
allows valid entries to survive semantic failures in other entries; malformed
framing still aborts. Readonly entries cannot be overwritten/cleared and existing
encrypted entries cannot silently become plaintext. Staging uses bounded heap
copies of the current and incoming values; CLEAR retains these policy checks
against the original key and namespace identity. A product-authorized plaintext
replacement requires an explicit key deletion before importing its new value.
Low-memory products must budget this
management operation or handle its NO_MEMORY result.

Global JSON rejects stores containing non-default namespace entries; use explicit
per-namespace JSON for a known stable namespace ID. This prevents duplicate key
collisions and accidental reassignment to the default namespace. Binary preserves
IDs but requires their existing namespace map.
The NXCS persistence snapshot is the canonical format that carries namespace
names and IDs together; JSON/binary exports do not replace that snapshot.
