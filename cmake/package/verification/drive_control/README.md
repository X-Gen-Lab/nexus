# Public CiA 402 consumer

The same C11 source consumes selected `Nexus::CiA402` PV and PP profiles through
installed public headers. Each assembly explicitly selects `components =
["cia402"]`; Native executes profile initialization, supported-mode/state checks,
workspace queries and release. F407 links the same calls with explicit Flash,
static RAM and stack budgets. Unobserved actual mode stays zero.

This consumer does not instantiate a CAN controller, allocate a live node arena,
exchange frames or drive an axis. Its resource result covers only the linked
initialization/query path, not a complete CANopen node. Real standard OD, SDO,
NMT, PDO, feedback and handshake behavior belongs to the GoogleTest contract
suite. CAN physical timing and hardware acceptance remain separate.
