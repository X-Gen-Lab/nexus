# GD32F470 optional memory development evidence

This bundle keeps original TDD output for selected ADDSRAM/TCM placement and
reset initialization. It is development software evidence. Physical reset,
memory timing, DMA traffic and power retention remain **not executed**.

The actual missing API, unaligned descriptor and descriptor-in-NOINIT failures
precede the production initializer changes. `maintained-initialize-green.xml`
executes all 12 real initializer cases. The two maintained reset targets execute
real `SystemInit` for default and both-selected variants, two cases each, with
controlled SDK register and address models. Default reset does not alter extra
storage or incoming TCM clock state; selected reset copies DATA, zeros BSS and
leaves NOINIT untouched. No implementation is mocked.

The retained `arm-red`, `arm-green` and `arm-green2` reports record actual
integration failures: missing Flash bounds, an unrelated new 32-bit arithmetic
warning, and generated selection variables not forwarded to the SoC source
graph. The last failure linked an unprovided initializer; forwarding the same
validated variables and registering the selected helper closed it.

The maintained real ARM command was:

```text
.venv/bin/python -B scripts/ci/gd32_memory.py --compiler /workspace/nexus-tools-arm/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-gcc --output tools/testing/evidence/memory-iteration/arm-green3
```

`arm-green3/report.json` records four passed whole production C11 configurations,
22 actual commands, independent resource budgets and section GC. Each selected
region reserves exactly 36 live bytes; the main SRAM reservation stays 2072
bytes. Loaded Flash is 2876 bytes by default, 3320 with ADDSRAM, 3352 with TCM and
3448 with both. Default retains no extra helper object, helper symbol, storage
or reset path. The actual ELF/map/BIN identities are bound to the same resolved
configuration and resource reports. These fixture totals are not application
capacity, cycle or throughput estimates.

Two real resource CLI executions reject a 35-byte region budget for the actual
36-byte section footprint. Two real whole-graph links reject deliberate manual
placement in unselected regions; these negative commands must return nonzero
with the exact expected diagnostic. A successful negative fixture would fail
the matrix. DATA Flash load ranges and section bounds are aligned and checked;
TCM is CPU data only, has no executable section and is excluded from DMA.

The public placement macros fail compilation when unselected, reject functions
and nonzero BSS/NOINIT initializers, and emit individual variable input sections
so dead variables disappear through GC. GNU compile/ELF regressions and driver
boundary tests retain their original commands and output alongside the Google
and ARM reports. An unavailable Clang compiler is not claimed as tested.

`artifact-relocation.json` maps 60 original generated `.c`/`.h` artifacts and
four original compilation databases to stored `.raw` files without changing
their bytes. Historical commands
and reports retain their original input paths. These snapshots are raw evidence,
not maintained sources or another configuration authority. `development.json`
binds the current owned source, original execution reports, original failures
and retained ELF artifacts. Its archival update records two later comment-only
style corrections and preserves both prior manifest and index as `.original.raw`.
It does not requalify historical executions against later source bytes. Actual
source/tool hashes were stable during the matrix;
this does not substitute for the later clean-commit whole-platform delivery gate.
