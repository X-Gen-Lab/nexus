Final immutable HTTP restoration and public Release metadata proof

Source C: 58297516d707fdee117bd27b5df6af09d72edd2e
Delivery builder E2: 51bebd47dd63dce90c7fded1efafef27cd20fd3e
Input asset commit: 0928f73809ca6bd3c9cae1f7d29f308db6bb201f
Input prefix: docs-release-0.1.0-58297516d707-final

The retained utility downloaded the exact committed registry and all nine final
asset files from public immutable raw URLs, without credentials. Every file's
size and SHA-256 matched the pinned registry before the downloaded stdlib
reader executed. Actual reader return code/stdout/stderr are in restore.json.
The additional independent archive read verified all 98 outer members and
1063 sealed restoration files with size/hash/mode checks, all 887 directories,
and the sealed inventory's self-copy. Both original normal hook raw archives
were fully read: 14293 E1 and 15733 E2 entries. Member observations are retained.
No archive extraction, SDK builds/tests, cryptographic verifier rerun or new
publication occurs in this proof utility.

After that successful HTTP restoration, root updated only the public Release
body with bilingual immutable evidence links. release-metadata-update.json is
the exact root receipt, including the complete body, old/new body hashes and
fresh readback of all 13 unchanged asset identities. Source, annotated tag,
draft/prerelease state and published_at remain unchanged. The body links the
immutable input data commit above; no self-referential proof commit is required.
The earlier completion/retention records truthfully retain their pre-HTTP and
pre-metadata-patch snapshot wording and have not been rewritten.

To repeat the byte restoration, download this utility from the same exact proof
commit and run python3 -B verify-remote-restore.py --destination /new/data/path
--proof-output /new/proof/path using fresh directories. It verifies the pinned
data commit and generates a new observation; it does not overwrite this proof.

Observed software-preview records establish actual R3 public SDK publication,
independent public download, E2 attestation cryptography, immutable C/dependency
source comparison, fresh relocated consumer success and completed cleanup.
Physical HIL remains not_executed; production/safety/signed-firmware/offline or
reproducible-build qualification and zero-vulnerability inventory are not
claimed. Eight historical firmware archives and eight historical reviews remain
unavailable; historical records are unchanged and unqualified by this preview.
