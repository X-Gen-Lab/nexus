# Typed device ownership core

This iteration implements ADR 010's discovery and device-reference boundary in
`hal/base/nx_device.h` and the production `hal/src/nx_device.c`. Device descriptors
carry a declared class; names do not authorize casts. Existing platform
registration macros declare classes, and `NX_DEVICE_REGISTER_TYPED` additionally
preserves constructor failure reasons. Construction binds an instance; typed
`nx_device_open` separately starts its hardware lifecycle.

## Product contract

`nx_device_discover(name, class, &descriptor)` has no construction or hardware
side effects. `nx_device_open(name, class, owner, &reference)` grants one exclusive
reference. Its descriptor, owner and monotonically increasing generation are
validated for every product action. A copied reference becomes stale after close
or reopen. Generation exhaustion fails rather than reusing a token. Open and
close have no-wait admission: contention reports BUSY, and bounded lifecycle
callbacks execute outside the architecture metadata mask. Constructors using the
new status-returning hook preserve errors such as NO_MEMORY. A legacy constructor
returning NULL reports GENERIC because its actual reason is unavailable.

Hardware init success is checked against actual RUNNING state. Typed GPIO actions
reject suspended/error hardware before invoking legacy void-valued GPIO methods.
Typed GPIO read/write/toggle dispatch and UART submit/poll/cancel/RX-event dispatch
never return a driver implementation pointer. The short metadata regions use Arch
directly, with no OSAL or vendor dependency. Active driver calls are pinned and
serialized per GPIO/UART instance; a concurrent or reentrant close reports BUSY. Open
failure attempts hardware cleanup. A cleanup failure quarantines the instance,
blocks reopening and requires explicit recovery by the matching owner. Close
failure retains the same reference for retry; it never pretends hardware stopped.

UART admission keeps a persistent buffer lease beyond the submit call. Neither
close nor registry reset can release a device with an outstanding ticket. A
failed cancellation retains that lease. Only a settled poll or successful cancel
returns storage ownership. The most recent ticket also belongs to the current
reference generation: a later owner cannot poll or cancel the previous owner's
operation. Memory settlement and wire-idle are separate fields; a successful poll
means a valid query, and the result contains the actual operation status. Drivers
must provide nonzero nonrepeating sequence numbers and a service/poll path that
enforces the stated total deadline. No hidden worker or timer is claimed.

Capability queries inspect the actual optional UART operation port. Native's
existing byte-buffer UART has no operation port, so it advertises no ticket,
wire-completion or arrival-timestamp support. STM32/GD32 operation-port behavior
has separate driver and target evidence; a common header is not physical proof.

## SPI controller and child ownership

`nx_device_open("SPI0", NX_DEVICE_CLASS_SPI, owner, &controller)` opens the
controller. `nx_device_spi_open(controller, &config, &child)` creates an isolated
child with its own copied configuration. The public child contains only a
controller reference and a pool slot/generation; its underlying provider handle
stays inside HAL. The default global pool holds 16 children, configurable at HAL
build time with `NX_DEVICE_SPI_MAX_CHILDREN`. It never allocates during open,
submit, transfer, cancel, callback or close. Native controller construction also
owns static configuration-sized capture buffers and slave storage. Native OSAL
mutex construction remains a management allocation in its existing backend.

A live child pins its parent even while idle, so controller close and product
shutdown cannot invalidate a child. Failed child close retains the reference.
Malformed successful provider opens are cleaned up; cleanup failure quarantines
that child reservation until `nx_device_spi_recover(controller)` succeeds. Slot
generations and ticket counters survive reuse; exhaustion fails closed. Child
capability queries inspect the actual transfer/submit/cancel methods and service
port, so a sync-only backend never advertises a working queue.

Blocking `nx_device_spi_transfer` retains child and controller ownership through
the provider call and its optional terminal callback. Its timeout is the
provider's single budget, including bus lock contention and hardware work; zero
does not start hardware. `nx_device_spi_cancel_transfer` requests cancellation
from another task. Neither request success nor error permits buffer reclamation:
the original transfer must return. An early request can report NO_DATA/NOT_FOUND
before provider admission and may be retried. Unsupported provider cancellation
reports NOT_SUPPORTED. Blocking transfer/service reject an incoming architecture
interrupt mask, preserving the saved mask without starting provider work.

Async `nx_device_spi_submit` requires a finite nonzero timeout and returns a
ticket scoped to child slot, generation and a nonrepeating sequence. The facade
supports poll-only observation as well as an optional user callback. A dedicated
application task calls `nx_device_spi_service(controller)`; there is no hidden
worker. Queue delay consumes the provider's original admission deadline. Async
cancel is a request even when it succeeds: storage remains borrowed until the
terminal callback returns and poll reports `settled`. An error preserves the
lease. Same-child reentry, child close and parent close remain BUSY throughout
the terminal callback. Concurrent cancellation can enter an active service call
without a HAL mutex or OSAL dependency. A stale or foreign ticket cannot cancel
new work; failed queue admission returns an invalid ticket and preserves the
previous terminal result. Software settlement does not imply DMA or physical
wire telemetry that the provider does not expose.

`nx_device_shutdown_begin/end` provides an admission fence around platform
shutdown: a live typed owner or construction prevents teardown; after a successful
begin, new opens, construction, registration and reset are rejected until end.
Native platform shutdown uses this fence before stopping cached legacy devices.
Registry-reset never resets lifetime generations and never performs implicit
hardware teardown.

Linker sections are traversed from checked integer bounds rather than pointer
ordering across linker symbols. Missing both bounds denotes an empty table; one
missing bound, reversed bounds, pointer misalignment and partial records are
rejected. Descriptors and their name/config/state have static lifetime; Native
manual-registration storage is protected by the same Arch metadata lock.

## STM32 GPIO migration

Each enabled STM32 GPIO now owns static constructor, runtime and interface
storage. Its descriptor declares READ, WRITE or combined GPIO according to the
configured interface layout, preventing a read-only object from being cast to a
combined interface. No GPIO construction heap allocation remains. The common
`applications/blinky` uses product boot, typed GPIO actions, and closes its
reference before finite Native product shutdown. `config_demo` and
`freertos_demo` also use product boot: their Native finite runs release component
objects before product shutdown. FreeRTOS configuration work and startup-gate
release execute inside scheduled tasks, so operational kernel calls are never
performed in the bootstrap phase. MCU global shutdown is deliberately unsupported
until a controlled reset; running scheduler ownership is not reported as released.

Legacy factories remain construction-only migration entrances for existing
components. They check the registered class and reject an instance claimed by the
typed API. Previously escaped legacy pointers cannot gain generation protection
retroactively; new product code must use references. Plain TIMER registrations
are no longer cast to incompatible PWM/encoder layouts. Typed I2C slave dispatch,
remaining device categories, all legacy component migrations and installed SDK
packaging are separate work; this core does not assert they are finished.

## Executed validation

The targeted production-core suite covers class confusion, side-effect-free
discovery, exclusive owners, precise construction errors, failed-open cleanup,
quarantine/recovery, legacy ownership, stale copies, generation exhaustion,
close failure, reentrant/concurrent dispatch, injected ISR-context rejection,
shutdown admission, UART lease retention, failed cancellation, old tickets and
capability absence. Its locking uses the real Native Arch implementation; only
the ISR probe is replaced in the fault-model executable. These are software
ownership outcomes, not ISR timing or electrical evidence.

SPI fault-port tests exercise idle parent retention, bounded pools, stale child
and controller copies, failed open/close, quarantined cleanup, total-budget
forwarding, zero-budget refusal, saved-mask refusal, cross-task blocking and
queued cancellation, callback reentry, retained leases, old/foreign tickets,
failed admission and capability absence. A separate executable links the actual
Product assembly, production Native SPI and typed HAL to execute configuration
isolation, static binding, product shutdown admission, queue deadline expiry,
async cancellation and cross-task synchronous cancellation. Capture/echo and
host delays remain a Native simulation rather than electrical timing evidence.

A separate executable compiles the real STM32 GPIO registration/lifecycle/ops
sources with test-only vendor ports and three READ/WRITE/RW configured pins. It
executes typed discovery, hardware opening, actions, closing and stale-reference
rejection, and verifies discovery/legacy binding does not touch hardware. Full
source-identity and target-matrix evidence belongs to the integrated delivery
record. Physical MCU IRQ/DMA, safe-output waveforms, timing and HIL remain separate.
