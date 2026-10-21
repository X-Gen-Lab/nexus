# User I/O broker

The scalar GPIO path of this optional kernel-invoked component admits one
message against an explicit principal/device/mask policy. `Nexus::UserIoBroker` has no instances,
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

The same optional target also provides explicit copied UART TX mediation through
`user_io_async.h` and trusted caller-owned `user_io_async_storage.h`. It uses the
existing BusOwner/UARTOwner queue, copies user bytes before acceptance and retains
only privileged cells. Exact trusted session/slot/epoch identities, real provider
drain and owner publication join precede consumption. Closed privileged reap
additionally needs actual outer-gateway drain proof; final join releases the
exclusive externally claimed role. Copy errors, cancellation, quarantine and
failed proof never authorize replay or premature storage reuse.

This common mechanism does not supply SVC/CMSE entry, kernel identity/copy/session
authority, MPU/TrustZone isolation, NS RTOS or physical qualification. The single
canonical contract and its reviewed Chinese translation are linked above; full
IO-39/SOC-36 exits remain open.
