# User I/O broker

An optional kernel-invoked component admits one scalar GPIO message against an
explicit principal/device/mask policy. `Nexus::UserIoBroker` has no instances,
maximum loan pool, worker, allocation or implicit OS dependency. Provider state
and construction remain private; each integrating kernel supplies its own
caller-owned broker, call slots and trusted memory ports.

The public wire message contains scalar device/operation/mask fields. The kernel
identity port supplies the principal. Both user spans require a successful
address-space pin before the bounded typed GPIO call. A successful pin transfers
ownership even if later revocation or cleanup fails. Exact private loan tickets
require actual provider settlement and observer join before unpin. Failed drain
or unpin retains the slot and mapping; cleanup retries never replay GPIO.

This component does not provide a user-mode entry or privilege elevation.
FreeRTOS SVC routing, task identity, actual MPU/TrustZone enforcement, user fault
containment and the kernel memory pinner still need an integrating kernel. No
physical isolation or electrical qualification follows from these software tests.
See [the contract](../../docs/design/user-io-broker.md).
