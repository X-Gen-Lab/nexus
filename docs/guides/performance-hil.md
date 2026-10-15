---
id: docs-guides-performance-hil
type: how-to
audience: maintainer
source_language: en
---

(docs-guides-performance-hil)=
# Measure resources and prepare hardware acceptance

Use this guide to separate linked software costs from physical MCU behavior.
The platform supplies measurement and HIL tooling; products choose workloads,
electrical limits, safe outputs and release policy. No Board is physically qualified
until real equipment executes its reviewed scope.

Prepare the [fixed source and recursive dependencies](source-checkout.md), then
the locked toolchain for the exact selected assembly.

## Measure an exact image

Record clean source, assembly/resolved digest, dependencies, compiler and flags.
Build the same workload before and after a change. Check actual ELF/BIN/map and
resource reports, not estimates derived from object counts. Separate Flash LOAD,
Flash footprint, each physical RAM reservation, MSP, task stack/TCB, queue and I/O
buffers. Stack capacity is not a measured stack high-water value.

The [build contract](../design/build-system.md) owns resource checks.
Use maintained `tools/measurement/` gates; [execution budgets](../design/soc-execution-budgets.md)
state scope. A speed claim needs units, clock, input size, load, sampling method
and uncertainty. DWT 32-bit snapshots require a bounded wrap interval; host arrival
timestamps do not establish MCU cycles or IRQ latency.

![H743 exact memory and Cache ownership reference](../assets/diagrams/h743-memory-cache.svg)

FIG-09 is an exact H743 reference. Cache-enabled TX requires complete 32-byte
line ownership and AXI SRAM reachability; DMA completion, wire completion and
borrow settlement are separate. It does not authorize RX DMA or TCM placement.

## Prepare without operating equipment

First configure and build the selected maintained assembly. For the Liangshan
baremetal reference, these repository-root commands consume its actual output:

```sh
python tools/dev/dev.py configure --preset gd32-liangshan-baremetal
python tools/dev/dev.py build --preset gd32-liangshan-baremetal --parallel 4
python tools/hil/prepare.py     --fixture tools/hil/fixtures/gd32f470_liangshan.json     --resolved build/gd32-liangshan-baremetal/generated/resolved.json     --report /tmp/nexus-hil-preparation.json
python tools/hil/measure.py plan     --fixture tools/hil/fixtures/gd32f470_liangshan.json     --resolved build/gd32-liangshan-baremetal/generated/resolved.json     --report /tmp/nexus-hil-measurement-plan.json
```

The locked ARM compiler must be on `PATH`. Check the part, pins, selected modes
and `not_executed` cases in reports. Preparation does not open serial, flash or
power equipment. Numeric limits and instruments remain unbound until reviewed.
G474/H743/U575 software-reference fixtures deliberately reject physical station
admission; a future physical fixture needs separately reviewed tooling integration.

## Execute only a real reviewed station

Bind observed PCB/part/UID, probe, stable serial path, wiring and supply to the exact
ELF, resolved configuration and resource report. Pin acquisition tools, recipe,
instruments and numeric lower/upper limits. Follow the maintained HIL acquisition
contract: identify, flash, verify, reset, serial, cleanup. Failure preserves raw logs;
cleanup failure retains station quarantine and requires physical inspection.

Capture completion means executed but unqualified. Validate the actual smoke,
measurement and OS records separately. Do not synthesize physical traces or
convert a software model into HIL. Qualification covers recorded observations;
it does not establish unobserved WCET, electrical safety or a product release.
