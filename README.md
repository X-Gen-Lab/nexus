# Nexus documentation assets

Immutable release files are stored as Git objects. Use an exact commit URL;
branch names are discovery aids and do not establish byte identity.

`assets.json` lists each member path, byte count and SHA-256. Tool archives are
split into at most 32 MiB parts to satisfy GitHub file size limits. Concatenate
parts in the manifest order and verify the full archive before extraction.

This branch grants no firmware or physical-board qualification.
