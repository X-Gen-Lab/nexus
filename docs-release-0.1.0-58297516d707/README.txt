Nexus 0.1.0 local evidence retention

Final main source: 58297516d707fdee117bd27b5df6af09d72edd2e
Tree: 06feab8ef3e89df60b4cdc5671c66ab0e3c08c21

This capsule retains original local RED/GREEN, normal hook attempts 01/02/03,
prose/catalog/source reviews, 194 actual POT files, merge records, sanitized
CodeQL annotations, baseline comparison, independent risk triage and historical
candidate-availability recovery receipts. Each receipt keeps its original source
and execution scope. The local capsule does not establish successful final-main
CI or release publication; later receipts record those outcomes separately.

The Security workflow snapshot succeeded with 63 remaining CodeQL annotations.
The 28 annotated C/C++ files match baseline 78c3bc90 byte for byte. The retained
independent targeted triage found no demonstrated production blocker; it did
not execute new tests or prove absence of defects. No zero-finding claim follows.

Original eight historical candidate archives and eight review manifests remain
unavailable. Their original records and hashes are preserved, without replacing
old qualification or claiming new firmware reproducibility or physical HIL.

Generated API XML and doctrees are omitted with complete byte hashes. Actual POT
outputs and extraction receipts remain packed. Duplicate old retained assets
are referenced by immutable full-commit URLs. Raw PR-monitor transport responses
and temporary signed download URLs are excluded.

Download all files in this prefix from one exact Git commit, verify them against
the repository assets.json, then run: python3 -B verify-local-evidence.py
The stdlib reader checks each standalone input and every packed member.
No extraction or external package installation is needed.
