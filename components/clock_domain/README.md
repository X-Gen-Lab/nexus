# Explicit clock admission

This optional allocation-free component tracks exact caller-owned consumer tokens
under the saved Arch mask. The privileged owner closes admission, drives each
consumer's independent shutdown, and checks an epoch-bound zero-borrow barrier.
It creates no workers, fixed maximum pools or global power registry.

Storage and construction are private assembly interfaces. A token stays live and
immutable until its exact successful release; copying its fields cannot release
the retained object. Capacity bounds admission/output alias scans. Lifecycle calls
have one serialized privileged Thread owner; consumer acquire/release may also run
in same-CPU configurable IRQs. NMI, fault handlers, SMP and host signals are outside
this guard contract.

`close` does not cancel I/O and `quiescent` does not prove unregistered consumers
stopped. The component does not stop Tick, change PLLs, reset deadline domains or
perform an OS policy decision. See [sleep scope](../../docs/design/clock-domain-sleep.md).
