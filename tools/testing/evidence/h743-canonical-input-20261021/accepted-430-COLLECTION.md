# Accepted H743 ordinary-gate capture

This successor records commit `430b0aea` and its actual complete-index gate:
6,288 Google cases in 348 programs and 2,951 tool cases, zero failure/error/skip.
All 32 frozen index paths, including one deletion, match the committed bytes.
The command master and commit-msg completed with exit 0. The later 2,950 counting
assertion was a reporting error; actual retained reports total 2,951.

[Package](accepted-430-package.json), [verification](accepted-430-verified.json)
and [phase binding](accepted-430-delivery.json) bind the one compact raw archive.
The original archive `0e00...` was copied with a distinct inode and verified.
Its payload was fully restored and compared before local expansion retirement;
original provenance identities remain unchanged. No test report was reused.
This captures `430b0aea` acceptance, not the later USB gate, full SDK/formal/OCI
qualification or physical HIL.
