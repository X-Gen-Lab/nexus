# U575 finite LPTIM1 LPBAM development evidence

This increment adds one optional STM32U575ZIT6Q, TZEN0, baremetal LPTIM1
producer to the same concrete Stop2 owner. It borrows one immutable SRAM4
CCR1/RCR=0/ARR tuple and a separate two-descriptor workspace. LPTIM1 update
request 13 uses BREQ=1; finite DMA writes CCR1, RCR and ARR in that order.

Fresh terminal DMA and all three write-OK receipts are latched before any
DIER withdrawal. Error/cancel/wake records the first failure before withdrawal.
UEDE withdrawal and acknowledgement precede DMA suspend, master idle, full
RESET acknowledgement, address/link/IRQ removal and the owned timer reset.
Clock restoration and Stop2 detach precede the private final request release.
Unknown clock identity retains the loan; external reset permanently quarantines
it. Public service/stop cannot independently publish compound SETTLED.

The six original compact archives are retained once under `raw/`, each with its
original `*-package.json` and a new `*-verified.json` for these exact bytes:

- `source`: original 30-file implementation, real initial RED/GREEN, public ARM
  links and independent review. Its original manual receipt is historical: the
  later errata package corrects the assumption that write-OK survives DIER.
- `errata`: five-file ES0499/liveness correction, actual RED with five failures,
  24-case debug and size-optimized GREEN, old 31+17-case copy compatibility and
  strict M33 relink. The old copy owner stays 88 bytes; this selected owner is
  104 bytes, including its 16-byte producer ledger.
- `selection`: actual maintained CMake targets, four new mutual-exclusion
  failures followed by 12-case GREEN. No test-authored personality property.
- `documentation`: canonical contract, reviewed English/Chinese catalogs and
  source-bound figure. Its focused bilingual build is a development result.
- `workflow`: actual full 47-case RED with six failures, then full 52-case GREEN
  for policy-v8 selection, historical recovery and exact missing-image refusal.
- `visual`: six-file layout successor, with actual English/Chinese image review;
  labels and edge semantics remain unchanged. Apply it after documentation.

The source chain is frozen30 → errata5 → selection1 → documentation33 →
workflow1 → visual6 → release-README1. Root independently reviewed behavior,
ordering, source identity, bilingual meaning and rendered arrows. A subsequent
comment-only test header correction is recorded separately; original packets
are not rewritten. Changed production files pass the existing repository style.

A single real relocated full installed SDK used normal `find_package(Nexus)`
and `nexus_add_firmware`, its canonical public strict C11 consumer, private
include isolation and the ordinary resource postlink gate. The actual ELF is
`25dc6f8a12095641c513c5476727d477c8baf885ff962a374c12b43a33f0afd9`:
17,964 Flash bytes, 2,400 SRAM1 bytes including 2,048-byte MSP, and 60 SRAM4
bytes (12-byte initialized tuple plus 48-byte NOLOAD descriptors). These are
consumer-fixture costs. The 16-byte immutable table has four concrete methods.
The actual Reset → SystemInit → selected SRAM4 initialization → main link chain
is checked in that ELF; linking does not establish execution on silicon.

The SDK run binds parent HEAD `71bd3c32d459e18e034c15c54a6b00e372526513`
plus explicit dirty development source hashes, not a clean or publishable
candidate. Source/index equality is checked before/after that execution. The
release README and later test-comment update have separate chronological
bindings. Separate source and SDK links do not establish reproducibility.

Current policy-v8 requires 48 consumer roots and 51 images. This single SDK
consumer does not execute that complete matrix. Ordinary complete-index gates,
fresh remote CI/security, exact clean-source formal/SDK/OCI/offline validation
and physical HIL are separate scopes and cannot inherit older results.

Physical HIL is **not_executed**. No repeated/general LPBAM, PCB waveform,
residency, wake timing, power, broad IO32/SOC35 completion, product release or
hardware qualification is claimed.

Verify any archive from this checkout using the maintained tool, for example:

```sh
python tools/evidence/store.py verify \
  --package tools/testing/evidence/u575-lptim1-lpbam-20261020/source-package.json \
  --archive tools/testing/evidence/u575-lptim1-lpbam-20261020/raw/e2fc81ad88badbe728be1e596553c650542e08f4cce2dce6c8a1de5b9d0100fc.tar.gz
```

The separate `installed-root-package.json` and verified `raw/cb4c03a7…` archive
retain the actual SDK and root preflight once: 69 executed compile actions
(68 C, one assembly), distinct from 96 declared compile-database entries. The
27 unbuilt entries do not establish inclusion. Root preflight actually ran
135 Google cases across six binaries plus 75 selected Python and 52 release
workflow cases, all passing without skips. The collection includes source
phase bindings, original failed and corrected style receipts, exact SDK
artifacts, public compilation evidence and independent reviews. These focused
results do not substitute for the subsequent ordinary complete-index gate.

The later `final-gate-package.json` and its single `raw/d74786b4…` archive retain
the actual ordinary gate that produced commit `578235b2`: 6,207 Google cases in
346 binaries and 2,867 tool cases (1,335 testing, 925 configure, 485 CI and 122
workflows), all passing without skips. The final bilingual site has 125 pages
per language and 28 canonical diagrams with 56 localized renditions. Its
original identity remains parent `71bd3c32` plus the recorded dirty working
hash; it is not relabelled as a clean build of `578235b2`. The archive preserves
all command receipts, raw reports and the 94-file freeze-to-commit comparison.
These results belong to that commit, not later PFC or MPU changes.
