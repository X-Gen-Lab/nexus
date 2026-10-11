# Kernel-invoked user I/O admission

The optional broker separates untrusted scalar input from trusted kernel policy
and retained user-memory ownership. The implemented operation is a bounded typed
GPIO read or set/reset write. There is no user-provided port, MMIO address,
callback, principal, private slot pointer or asynchronous DMA operation.

The 32-byte input and 8-byte output contain explicit `uint32_t` fields. The
trusted gateway calls the public API from privileged Thread mode with incoming
interrupt masks clear. A kernel identity callback authenticates the current
principal; the public message cannot choose it. Construction retains immutable
nonoverlapping nominal user RAM windows, exact device/mask grants, a positive
loan capacity and the kernel memory port. All known broker, policy, port-table
and private context spans stay outside those user windows.

Admission validates full-span arithmetic, alignment, access and one-window
containment before pinning. `pin` must establish the specified CPU access and
retain address-space/process/mapping lifetime until successful `unpin`. A failed
pin must leave no mapping references or residual effects. The whitelist alone
does not establish actual mapped RAM, DMA reachability, MMIO exclusion or MPU
isolation; the trusted kernel and SoC memory layout establish those facts.

The input is pinned and copied once into kernel storage. Policy checks use that
snapshot. The reply span is pinned before any GPIO operation. A final saved-mask
guard serializes revocation with the bounded scalar provider operation. Close
before this guard prohibits the effect; close after dispatch cannot undo it.
Every dispatched result copies a fully initialized reply, including provider
errors. A cancelled admission leaves the reply untouched. Pins remain held
through all copies.

`result` reports admission/device execution and `cleanup_result` reports the
most recent release attempt. `accepted` becomes true at the first successful
pin. `device_effect_possible` is set before calling the WRITE provider, even if
that provider returns an error. `retained` requires the exact kernel call slot,
user memory and address-space identity to stay alive. Cleanup tries output then
input release, records errors and preserves every failed pin independently.
`gpio_finish` retries only retained cleanup; it never rereads users, writes a
reply or repeats a device operation. Neither an operation error nor successful
cleanup authorizes replay of an earlier write.

The private generic loan mechanism retains one exact slot pointer with a
monotonic 64-bit epoch. Copied tokens and stale epochs cannot release it; epoch
exhaustion rejects reuse. Read-only spans may share bytes; any writable overlap
with a peer loan rejects. Known policy, provider, other loan and drain-control
aliases reject before mutation. Only the two exact internal GPIO child slots
may share their own call proof. A trusted drain callback must prove actual
peripheral/DMA/IRQ/deferred/copy final access and observer join. User input cannot
claim this proof. A failed proof or unpin keeps the exact pin and token retained.

One privileged serialized kernel owner controls a broker and its slots. All
memory/proof callbacks are bounded and run outside Arch guards; they may mark
close, but cannot recursively admit, release, execute or retain the cookie
output. Close inhibits admission and preserves every live loan. Quiescence is
valid only after close, with zero loans/list entries and no executor. It does
not implicitly stop providers or establish an application shutdown barrier.
There is no reopen or hidden identity registry. NMI/fault/SMP publishers and
unserialized privileged memory writers are outside this contract.

This is a component callable by an integrating privileged kernel, not an
implemented userspace transport. FreeRTOS SVC/MPU wrappers, task principal
authentication, real memory pins, fault-safe copy mechanisms and TrustZone
veneers are not connected in this slice. Generic loans provide ownership
mechanics; asynchronous UART/SPI/CAN user messages are not implemented. Those
future transports must supply their actual provider drain and memory-port
contracts rather than accepting a user-supplied “drained” flag.

Google tests run the actual broker, typed GPIO wrapper, Native GPIO provider,
Core request state and Native saved-mask implementation. They model only the
external kernel identity/pin/unpin/proof boundary and privileged context.
Behavioral RED/GREEN logs, sanitizer runs, public-header probes and strict ARM
compilation are recorded in
`tools/testing/evidence/user-io-broker-iteration/manifest.json`. These establish
software behavior and compilation, not physical isolation, HIL timing or a
qualified product release. The optional archive defines no mutable globals;
default factory/source/ELF omission is verified separately by the integrating
build graph.
