# Private I/O mechanisms

These headers and implementation templates are private contracts between typed
I/O mechanisms and selected SoC providers. They never enter a product's public
include path. Public faces contain immutable operations and an instance context;
mutable state and vendor register types remain private.

Reviewed CAN, USB FS device, Ethernet, timer, FIFO and pin-state hardware
implementations live in `drivers/<kind>/<reviewed-ip>/`. A SoC wrapper supplies
exact register and clock facts, IRQ/DMA bindings and checked silicon semantics.
Private build-time header dependencies do not select sources or add runtime
dispatch. The selected SoC CMake target still owns every compiled engine.

`io/src/api/` owns public checked dispatch and `io/src/mechanisms/` owns reusable
I/O rules. This directory keeps hardware-independent validation, sequence-owner
identity and bounded calendar arithmetic. Native models live in
`soc/native/drivers/`; MCU wrappers live under
`soc/<vendor>/<family>/drivers/`. Identical names do not establish identical
hardware semantics.

The selected SoC CMake target owns which controller sources are linked. Maintain
private SDK includes and C11 firmware. No common engine may allocate implicit
storage, discover devices, start a worker or acquire a mandatory OS lock.

IRQ work records bounded hardware facts. Task-side service continues deadline,
wire and source withdrawal. SETTLED follows final reference withdrawal;
QUARANTINED retains every outstanding borrow. Queue, endpoint, descriptor and
block capacities come from the authored assembly or an explicit caller-owned
contract. Their costs must be observed in the real linked ELF.

See [execution budgets](../../docs/design/soc-execution-budgets.md), the public
operation contracts and the controller-specific design documents for limits and
physical qualification boundaries.
