# Optional common components

Explicit CMake targets compose narrow ports: bounded BusOwner, SPI/UART owner,
BMP280 core and SPI transport, instance-level Log, two-bank commit-last Storage,
physical Flash region and Native file adapters, and pure Modbus RTU parsing.
Unreferenced component code does not enter a firmware image.

Owners have one executor, exact slot capacity, stable cancellation epochs and
caller-owned operations. Initialization links unused slots before publication.
Admission removes one free slot and appends one FIFO entry under the metadata
guard; settlement/rejection returns one reusable slot. Both use constant work
independent of capacity. The same slot link serves the free or FIFO chain, with
one free-head index per owner and no extra per-slot storage. A slot reaching its
final epoch is permanently retired, never rebound with a repeated identity.
UART's explicit inner provider request lets the owner release the outer request
only after its last access. Devices and protocols do
not choose Board pins, product registers, default workers or global lookup.
Products supply transports, authorization, regions, task policy and budgets.

Meaningful regressions in `tests/contracts` cover concurrent producers, late
cancel/TC, failure quarantine, interrupted persistence and protocol malformed
inputs. A durable host file is not a physical Flash power-cut qualification.
