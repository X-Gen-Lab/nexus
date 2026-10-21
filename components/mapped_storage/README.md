# Optional mapped storage

`Nexus::MappedStorage` supplies a bounded, single-executor ownership mechanism.
Each immutable face shares one read-only operations table and addresses separate
caller-owned state. The caller supplies exact read-loan slots, requests, backend
context, aperture and monotonic clock. Lookup, allocation, queues, workers and OS
locks are absent. Map acquisition/release and writer service perform constant
metadata work; backend service uses its explicitly bounded work budget.

A live CPU, XIP or DMA-read loan excludes every writer to the entire aperture.
The reader tag must be supported by the selected backend; it is not evidence of
executable memory or DMA reachability. Consumers join before exact slot/epoch
release. The final release withdraws the mapping; failed withdrawal retains the
loan. Slot and writer epochs never wrap. Stop closes admission and waits for
loans and the current writer, without revocation.

PROGRAM and ERASE requests use the existing Core request publication contract.
Admission validates geometry, address overflow and aliasing with owner, slot,
backend and aperture storage before touching the input or hardware. Rejection
retains nothing. Accepted requests retain their metadata and program input until
acquire-observed SETTLED. Cancel and deadline start source withdrawal; they do
not release storage. Failed drain quarantines the request, and continued service
can retry withdrawal. Partial effects remain visible and are never replayed.
No command/cache/DMA writer may bypass the sole backend. A physical adapter must
supply real aperture/cache/DMA/IRQ withdrawal and XIP execution exclusion.

`Nexus::NativeMappedStorage` is available only in the Native platform graph. It
preserves supplied host bytes, programs 1-to-0, erases to 0xff and changes at most
the configured byte quantum per service call. Cancellation latches a stop before
further modeled mutation; drain detaches the input. Explicit failure and retained
drain controls support interruption regressions. This coherent CPU model rejects
XIP and DMA reader tags and provides no durability or physical memory driver.

This is one IO-37 software increment. SDMMC block commands, an official H743
QUADSPI/serial-memory provider, factory/resource assembly, physical cache/DMA/XIP
qualification and HIL remain open. No Board wiring is inferred.
