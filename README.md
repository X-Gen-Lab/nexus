# Nexus documentation assets

Immutable release files are stored as Git objects. Use an exact commit URL;
branch names are discovery aids and do not establish byte identity.

`assets.json` lists each member path, byte count and SHA-256. Tool archives are
split into at most 32 MiB parts to satisfy GitHub file size limits. Concatenate
parts in the manifest order and verify the full archive before extraction.

This branch grants no firmware or physical-board qualification.

Historical example assets have been withdrawn from this branch tip after
private preservation. Read [docs-asset-retention-20261030/WITHDRAWAL.md](docs-asset-retention-20261030/WITHDRAWAL.md)
for the exact scope and authenticated recovery entry. Earlier immutable commits
and existing copies may still contain them; this is not a history rewrite.

Unsigned public P2 software evidence: `docs-public-p2-ab532250-20261104/transport.json` preserves the original local bundle through ordered <=32 MiB opaque parts. The original envelope is unchanged; recovery requires actual store restore and live producer verification. No product release, remote CI/OIDC, physical HIL or remote Native-program recovery is asserted.
