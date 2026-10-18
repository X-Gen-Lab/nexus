---
id: explanation.gpio-pin-states
type: explanation
audience: maintainer
source_language: en
---

(explanation-gpio-pin-states)=

# GPIO pin states: explicit images under one retained lease

Use this explanation to select an explicitly authored `DEFAULT` or `SLEEP` GPIO
image without transferring pad ownership. The current named-state assembly is
limited to STM32G474RET6, GPIOA, baremetal and `hse24-pll170`. Its PA5 software
Board fixture does not establish PCB wiring, electrical safety or hardware
low-power behavior. Other providers do not acquire this mode from the public
header alone.

The public [pin-state declarations](../../io/include/nexus/io/pin_state.h) own
the API contract. The [I/O evolution design](../design/io-evolution.md) and its
execution ledger keep this mechanism separate from the remaining wake,
retention and power-mode work. This page explains integration and responsibility;
it does not replace those declarations or mark IO-31 complete.

## One authored plan, three distinct identities

Board facts explicitly supply both GPIO images. The ordinary TOML selects a
GPIO binding with `mode = "named-states"`; strict resolution produces immutable
register plans and existing static provider storage. No runtime strings, heap,
worker, dynamic registry or second pin-configuration authority is introduced.

- `nx_pin_state_id_t` identifies one generated factory instance.
- `nx_pin_state_port_t` is its immutable public face: shared readonly operations
  and that instance's private context.
- `nx_pin_state_selection_t` selects `NX_PIN_STATE_DEFAULT` or
  `NX_PIN_STATE_SLEEP`; it is not a factory instance ID.

`nx_factory_pin_state()` returns the existing face without touching hardware.
Ordinary GPIO lookup does not return a named-state instance. Lifecycle work
belongs to explicit Platform start/stop, and runtime selection enters the
provider directly through `nx_pin_state_select()`.

The selected configuration is maintained here:

```{literalinclude} ../../tools/configure/assemblies/g474-pin-states.toml
:language: toml
```

The software Board explicitly declares PA5 output push-pull, low speed, no pull
and level zero for `default`, and analog/no-pull for `sleep`. These are fixture
values, not inferred board-safe levels. Products must provide reviewed electrical
facts and their own policy instead of copying the names as safety guarantees.

![Current G474 named GPIO ownership: cold lease, checked selection and separate Platform cleanup](../assets/diagrams/gpio-pin-states.svg)

FIG-22 describes the current selected software lifecycle. Arrows distinguish
build-time generation, pure lookup, state selection and cleanup. Every failure
retains the responsibility it has not proved released; rendering does not
establish a physical transition or HIL result.

## Cold startup acquires responsibility before driving

The generated Platform owns the exact selected pads, bank gate and original cold
snapshot. Its cold admission checks the selected resources before accepting
hardware effects. Named-state initialization requires selected pads to be input
or analog, unlocked and in a powered, non-reset bank. Existing output or AF
ownership is not permission to take over that pad.

Initialization proves the authored `DEFAULT` image before opening the instance.
A failed startup can leave partial effects and cleanup responsibility. The
Platform's rollback must use checked provider stop and original-snapshot
restoration; it cannot discard the lease because the constructor returned an
error. Keep the owner, immutable plans and their complete storage live until
that cleanup actually succeeds.

## Selection is serialized privileged work

The caller provides one serialized, non-reentrant, privileged same-CPU Thread
executor. Incoming interrupt masks must be clear. No ISR/NMI, competing pad or
RCC writer, clock transition or register takeover may run concurrently. These
are integration preconditions, not permissions established by matching register
bits or by the factory pointer.

The provider uses a bounded masked section and restores the saved interrupt
state. Bank gate/reset checks precede GPIO register access; selected lock bits are
then checked without taking over a locked pad. An unavailable bank is rejected
without silently enabling its clock or clearing reset. A clean
committed image is checked even when selecting the same state; external drift
can return `STATE` and retain dirty responsibility instead of silently accepting
the altered image.

For a transition, the provider first proves that the entire selected group has
withdrawn its drivers. It then programs the required output latch and electrical
fields, applies the target mode and checks the actual target image. Only that
proof commits the new selection. Masks preserve unrelated pads. This ordering
is software behavior; it does not prove glitch-free pad voltage, external bias
or an electrical settling interval.

A partial failure may leave neither the previous image nor the target image.
The lease stays retained, and the caller must not reuse its storage or hand the
pads to a different writer. An initialized instance that has not stopped can
retry an explicit supported target; otherwise resume checked Platform cleanup.
A failed initialization or an already withdrawn provider cannot be reopened by
selection. There is no hidden background repair, automatic
clock recovery or deadline-based ownership release.

## Stop has two separate completion boundaries

Private provider stop proves selected drivers withdrawn. Its success is that
provider's final access, but it does not by itself restore the Platform's cold
snapshot or release the bank lease. Those remain separate generated Platform
cleanup steps.

The Platform restores the saved selected-pad image and proves that restoration
before releasing its GPIO gate responsibility. If restoration fails after provider stop succeeded, its pending snapshot and
lease remain live. If restoration is already proved and only gate cleanup fails,
only the remaining gate responsibility stays pending. Keep the complete storage
until the whole Platform stop succeeds, and retry only unfinished cleanup.
Do not repeat construction, zero
storage or interpret a first error as proof that every resource is still fully
running or fully released.

## Use the public selected face

After a successful Platform start, an external product uses only public and
generated headers. The fragment below is the named-state branch of the
maintained link fixture; the surrounding fixture handles startup and stop. It
is not a standalone application or a physical execution result.

```{literalinclude} ../../tests/contracts/pin_state_fixture/main.c
:language: c
:start-after: "#ifdef NX_PIN_STATE_FIXTURE"
:end-before: "#elif defined(NX_G474_IO_FIXTURE)"
```

Factory lookup, synchronous selection and final Platform cleanup allocate no
request or transfer buffer. The operation performs bounded work over at most
16 selected pad bits; readonly plans, mutable provider state and the Platform's
saved snapshots remain distinct costs. No measured MCU latency, total RAM or
worst-case interrupt bound is implied by that structural statement.

## Current scope and verification boundary

Current source implements named GPIO images, cold ownership and explicit
selection for the exact G474 reference tuple. The derived [capability
matrix](../delivery/capabilities.md) records selected modes and source definitions;
it does not qualify a board or execute a firmware image.

Native register/factory tests establish the exercised software and failure
paths. Actual ARM linking, installed SDK consumption and HIL require their own
same-source evidence. The public interface and figure do not establish AF
switching, wake routing, retention, deep Sleep/STOP entry, monotonic continuity,
PCB safety or real pad timing. Those remain separate contracts and work packages.
