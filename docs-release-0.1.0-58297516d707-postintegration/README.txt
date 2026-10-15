Nexus 0.1.0 local postintegration evidence addendum

Main source: 58297516d707fdee117bd27b5df6af09d72edd2e

The standalone examples archive, manifest and receipt preserve the exact locally
validated examples commit 601580990df0e8232c3b6f3cc641cc3a8ee30ab8, pinned to
the main Source SDK above. Their original scope and failure history remain.

The compact archive retains only the 52 explicitly allowlisted browser, offline
smoke, documentation verification, local publisher, HTTP and tool preparation
records. Original browser v1 failed and is preserved; v2 passed its actual local
acceptance. Original script reviews and diagnostics remain distinct evidence.
Publisher commit/HTTP checks are local and do not establish remote Pages.

Full final-main CI, GitHub Release and remote Pages success are not claimed.
No firmware candidate/offline reproducibility or physical HIL qualification is
created by this retention. Future actual remote publication receipts are separate.

No pr-monitor files, temporary signed transport URLs, tool distribution archives
or restored tool binaries are selected. Sizes and SHA-256 values are complete
in postintegration-allowlist.json and the global assets.json registry.

Download this prefix from one exact immutable Git commit and run:
    python3 -B verify-postintegration.py
The stdlib reader checks all standalone inputs and every member of both archives.
