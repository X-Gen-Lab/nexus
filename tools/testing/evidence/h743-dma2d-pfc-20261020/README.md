# H743 finite DMA2D copy and pixel conversion evidence

The optional existing H743 V owner now accepts RGB565 copy and fixed
little-endian ARGB8888-to-RGB565 conversion. It keeps the same four shared
operations, caller storage and complete-frame loan contract. No queue, worker,
CLUT, blending or Board connection is added. Cache must remain off and every
payload must occupy AXI SRAM.

The same increment corrects the 14-bit pixel-mode foreground/output gap bounds,
checks all seven controls against every pool slot's full capacity, and preserves
the first observed error. Observed active RCC gate loss or reset assertion
permanently retains the loan. Restoring zero registers does not establish drain;
an unobserved reset pulse remains excluded by mandatory exclusive ownership.

The original source and diagram archives are retained once under `raw/` with
their package and byte-verification reports. The source archive includes actual
RED failures before each behavioral correction, final 32/32 Google cases without
skips, focused static analysis, normal public strict-C11 development ARM links,
and source identities. Source and English/Chinese meaning have independent
reviews; root also read all 15 translation pairs and both rendered diagrams.
The API catalogs preserve unchanged entry order and translations.

On the compared ARM fixtures, request storage stays 64 bytes and owner storage
stays 64 bytes. Host request storage grows from 96 to 104 bytes. Combined PFC and
validation hardening changes Flash from 8,280 to 8,548 bytes; AXI reservation stays
2,632 bytes, including 2,048-byte MSP. Per-function local stack records are
unchanged; they do not measure transitive stack or physical timing. These source
links do not establish an installed SDK, whole-matrix or hardware result.

The ordinary complete-index gate and fresh remote checks occur after these
development packets. Physical HIL is `not_executed`. No throughput, latency,
product release or complete IO-26 exit is claimed.

The later `final-gate-package.json` records commit `b0319115`, tree `103e83bf`:
6,225 Google cases in 346 binaries and 2,868 tool cases, all passing without
skips. It also preserves the cancelled first gate, full withdrawal receipt and
obsolete SDK NULL-probe regression followed by the final 25-case selection
GREEN. All 40 final source paths match the committed blobs. The bilingual site
retains its original dirty 39-path identity; the later contract-test correction
changes no documentation input and is not relabelled as a clean site build.
These results do not qualify subsequent MPU source or its consumer matrix.

Verify the retained bytes using the maintained tool:

```sh
python tools/evidence/store.py verify \
  --package tools/testing/evidence/h743-dma2d-pfc-20261020/source-package.json \
  --archive tools/testing/evidence/h743-dma2d-pfc-20261020/raw/f82ecec509a3c76e1dbb82b4382c67302652125813baa6bcf051a240f4e48265.tar.gz \
  --report /tmp/nexus-h743-pfc-source-verification.json
```

## b031 final remote readback

The 36 actual check-runs for `b031911584800898f1cdc29a7d96afd0f8335b2b`
completed successfully, including both language analyses and the final CodeQL
aggregate. `remote-b031-checks.json` is the complete check-name/status projection,
not a rerun or acceptance of later source. The follow-up package retains all 86
CodeQL annotations (14 warnings, 72 notices, no errors) and their independent
triage. The three prior uninitialized-variable errors are absent; no new
functional counterexample was confirmed and no suppression was introduced.
The verified raw copy is `raw/ea4567d6375362649412a79e372b2b189d54d313c1a1244bd9c22bdddaf26a31.tar.gz`.
