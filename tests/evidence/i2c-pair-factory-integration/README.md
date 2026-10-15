# Paired I2C factory and cold lifecycle integration

The final regular and ASAN/UBSAN executions each run all 83 discovered Google
cases: STM DMA 15, STM IRQ 14, GD DMA 15, GD IRQ 14, Native factory 10 and
G474 factory 15. Each has zero failures or skips and unchanged captured source.
These are scoped dirty development executions, not a clean software candidate,
physical qualification or a binary compatibility promise.

Paired tests compile the actual generated factory, common I2C V1 algorithm,
family adapters and official GPIO code. Only external CPU/MMIO/NVIC/hardware
clock leaves and the separately tested SoC system-clock port are substituted.
They do not mock request admission, completion, cancel, stop, static factory or
pin helpers. GD vendor reset dependencies fail the test if invoked. The actual
configured SRAM domain is mapped for DMA buffers; no generated memory facts are
rewritten to admit ordinary host pointers.

The two controllers expose independent static faces and addressed children,
share read-only methods, and accept the same address on distinct buses. Tests
exercise event/error/DMA isolation, buffer rejection, cancellation, wire drain,
failed DMA withdrawal, IRQ quarantine, pin restoration and restart. IRQ mode
checks final three-byte receive windows without creating DMA ownership.

Genuine RED records establish these corrective contracts:

- Pure lookup confirms controller/address identity before hardware startup.
  Four paired cases and the Native cold case initially failed. Static bindings
  and pure polling predicates close the discrepancy; lookup never initializes.
- First-use cold I2C stop originally terminated its child with SIGSEGV. The
  exit test requires a normal STATE/INVALID result with zero hardware effects.
  Prepared non-null state still performs cleanup after its initialized flag is
  cleared; the fix does not discard partial or post-stop responsibilities.
- G474 pin restoration failure leaves residual resources after providers drain.
  An unprivileged retry originally performed 17 reads and 9 writes and revoked
  pin/clock fields. The platform now rejects before any hardware mutation and
  allows the privileged lifecycle owner to finish the retained cleanup.

Initial missing routes/modes, fixture symbol/link errors and the stale reserved
ThreadX derivation refusal are preserved as build failures. They are not claimed
as executed production regression failures. The final ordinary CMake build
accepts only the reviewed predecessor derivation through the maintained tool;
no derived output was deleted to bypass its refusal.

Actual host ELF symbols show each V1 child remains 16 bytes on the 64-bit host.
Static logical initializers move these existing objects from BSS to DATA, which
requires load-image bytes; this is not a measured MCU or total-product cost.
The maintained selected-provider ARM gate independently measures actual MCU
images. This record does not claim PCB pull-ups, bus speed, electrical timing,
physical DMA behavior, power behavior or connected hardware execution.

`manifest.json` binds original and preserved raw byte identities. Historical
source/generation files have `.raw` suffixes and are never reformatted. Original
reports retain their original paths and hashes; the preservation table maps
these to repository copies. Host executable bytes remain in immutable external
phase copies and are explicitly identified rather than asserted to remain at
reused build paths. Normal staged commit hooks and final clean-SDK/offline
qualification are separate root-maintained gates.
