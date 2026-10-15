# DP83848

An optional caller-owned Clause22 component: explicit identity/address, bounded
BMCR soft reset, externally selected negotiation/fixed mode and observed link.
It owns no GPIO, power rail, MAC, worker, task, stack, descriptor pool or network
protocol. A configuration acknowledgement never substitutes for link truth.

The caller keeps its MDIO face and component state live, serializes operations,
provides finite deadlines and explicitly applies observed link facts to its MAC.
TI and NS IDs are chosen explicitly; unknown models reject. See
[the Ethernet contract](../../docs/design/ethernet.md) for register sources,
ownership, board-route restrictions and physical qualification boundaries.
