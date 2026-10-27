# Private controller engines

These headers and implementation templates are private contracts between typed
I/O mechanisms and selected SoC providers. They never enter a product's public
include path. Public faces contain immutable operations and an instance context;
mutable state and vendor register types remain private.

The shared CAN, USB FS device, raw Ethernet and timer engines implement the
maintained controller ownership protocol. A SoC wrapper supplies exact register
and clock facts, IRQ/DMA bindings and checked silicon semantics. Textual provider
templates bind direct operations macros to concrete readonly tables; they do not
create a runtime mode registry or a forwarding layer.

Use `drivers/` for a reviewed shared register algorithm such as I2C V1. `io/src/api/` owns public checked dispatch, `io/src/mechanisms/` owns reusable
I/O rules, and `io/src/controllers/` owns shared controller engines. Use this
directory for private structures, external hardware boundaries or direct provider
templates. Native models live in `soc/native/drivers/`; MCU wrappers live under
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
