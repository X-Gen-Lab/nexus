Independent immutable-URL local evidence restoration

Input asset commit: a85bafd6c9f816a5f92dd70a426318436f237d2c
Source main commit: 58297516d707fdee117bd27b5df6af09d72edd2e

Nine newly published files were independently downloaded from exact full-commit
raw URLs. Their byte counts and hashes, all 385 capsule members, 194 retained
POT files and all members of the three normal hook archives were verified/read.
The downloaded stdlib reader executed successfully. restore.json preserves the
actual requested immutable URLs, observed sizes/hashes and reader process result.
No temporary signed URLs or PR-monitor transport responses are retained.

This is byte restoration evidence only. It establishes no final-main CI success,
release publication, new firmware candidate qualification or physical HIL.
The original local and historical receipts keep their own execution/source scope.

To repeat without credentials or external packages, run verify-remote-restore.py
with fresh --destination and --proof-output directories. The script pins the
original asset commit and registry hash; it does not follow a moving branch.
