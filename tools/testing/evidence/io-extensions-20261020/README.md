# Borrowed-memory, handoff, RTC and frame development evidence

These records belong to the increment based on `4992df81`. They preserve actual
RED/GREEN executions, command results, model boundaries and ARM artifacts. The
immutable package and verification receipt identify every retained member.
`integrated-source.json` records the integrated development source bytes; it is
not a clean-source candidate or physical qualification.

The initial focused GoogleTest/GoogleMock executions contain 141 passing cases:

| Mechanism | Cases | Evidence member |
| --- | ---: | --- |
| Mapped owner and coherent Native NOR | 19 | `mapped/green-release-alias.xml` |
| AMP epoch/cache core | 27 | `amp/final-green/results.xml` |
| F407 VE masked handoff | 18 | `handoff/host-524288/results.xml` |
| F407 ZG masked handoff | 18 | `handoff/host-1048576/results.xml` |
| U575 retained RTC AlarmA | 20 | `rtc/final-result.xml` |
| Complete frame contracts | 25 | `frames/final-frame.xml` |
| H743 RGB565 DMA2D | 14 | `frames/final-dma2d.xml` |

Earlier exploratory failures remain visible. In particular,
`integration/sdk-call-green.log` contains three missing-CMake-PATH errors and is
not a passing result despite its original filename. The ordinary complete-index
commit gate supplies the final integrated execution; focused results and archive
verification do not substitute for it. Older mapped-storage mode/ARM reports
precede the final release-token alias regression and retain that scope.

F407 handoff, mapped ownership and U575 RTC have linked ARM firmware/resource
records. The DMA2D ARM consumer is a public C11 link fixture, without startup,
resource-budget or installed-SDK qualification. Native NOR admits CPU reads only;
AMP has no exact multicore hardware port. Neither a public type nor model
execution establishes a new physical peripheral or reviewed PCB route.

The complete original I/O and SoC exits remain authoritative. Stop2/LPDMA,
physical serial-memory/SDMMC/XIP adapters, I3C, asynchronous TrustZone services,
exact multicore ownership, SAI/DCMI/LTDC and physical station execution remain
separate work. See the [current contracts](../../../../docs/design/io-extension-boundaries.md).

Verify retained bytes with the existing tool:

```sh
proof=tools/testing/evidence/io-extensions-20261020
python tools/evidence/store.py verify --package "$proof/package.json" \
  --archive "$proof/a066f25e735317e244b61a3214f37b536f89958a0f53bebb541c504f9d53bd1f.tar.gz" \
  --report build/quality/io-extensions-evidence.json
```

The additional `integration/` package retains the first ordinary commit failure
(1,291 tool cases; stale capability documentation and physical-case count), the
actual SDK selection/header-closure RED/GREEN logs, successful integrated Native
configuration/build, and the reversible dependency Git stat-cache repair. The
repair preserved every dependency HEAD and blob tree; the stale zero-byte lock
is retained in the package. These diagnostic records do not replace the fresh
complete-index gate or establish installed-SDK/software-candidate qualification.

```sh
python tools/evidence/store.py verify --package "$proof/integration/package.json" \
  --archive "$proof/integration/1f255e1b7613a1ad619d27c984a9b14946e2658eadd787be9127f5ac5bb09ae1.tar.gz" \
  --report build/quality/io-extensions-integration-evidence.json
```

`rtc-configuration/` retains the second ordinary attempt: all 1,292 testing
cases passed, then the 912-case configuration suite exposed late rejection of
an unsupported RTC FreeRTOS combination. The fix rejects it during typed IR
resolution, with both RTC configuration cases passing. An exploratory unittest
discovery import failure is retained separately and is not GREEN evidence.

```sh
python tools/evidence/store.py verify --package "$proof/rtc-configuration/package.json" \
  --archive "$proof/rtc-configuration/6573b1c6bc3242a4b6a9c34a28cdd93ce23b3cf0de9136391de95a3df574f2e1.tar.gz" \
  --report build/quality/io-extensions-rtc-configuration-evidence.json
```

`final-integration/` retains the complete pre-LSE-guard Native execution:
356 CTest entries and 5,972 Google cases passed. Its development input manifest
records that earlier source. The third ordinary attempt's four stale transport
fixture assertions remain visible alongside their four-case GREEN correction;
the interrupted exploratory run is not accepted evidence. Previous 31/36-root
negative cases remain exact, while the current plan requires 44 roots/47 images.

The same package retains the LSE guard's real RED (20 pass, four fail), final
formatted 24-case GREEN, strict gettext validation and a paraphrased vendor
reference summary. The reference PDF and original development record remain
external. Those four regressions raise the focused total to 145. Neither the
earlier Native run nor these focused results replace the final ordinary gate.

```sh
python tools/evidence/store.py verify --package "$proof/final-integration/package.json" \
  --archive "$proof/final-integration/6e1e6aea647ccfe1f5416d04d4cfbaaaceb992bfa2387c1a2e6d8a936a6b1476.tar.gz" \
  --report build/quality/io-extensions-final-integration-evidence.json
```
