"""Finite shared-publisher wiring over already resolved immutable owners."""
from collections.abc import Mapping
from dataclasses import dataclass
import re

from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.static_ir import Array, Declaration, IrqConnection
from tools.configure.static_recipe import _immutable, _path, _read


class IrqClaims:
    """Admit sharing only through the selected static dispatcher contract.

    This local normalizer updates the resolver's sole claim ledger. Final
    source-mask overlap is checked by connections against canonical typed IR.
    """

    def __init__(self, records, claim):
        self.records = records
        self.exclusive_claim = claim
        self.publishers = {}

    def claim(self, key, owner, mode, source, *, kind, priority, calls_os,
              shared=False, contract=None):
        previous = self.publishers.get(key)
        signature = (kind, priority, calls_os, contract)
        if previous is not None and self.records[key]["owner"] != owner:
            if (not shared or not isinstance(contract, SharedIrqGroup) or
                    previous != signature):
                fail("Shared IRQ dispatcher, priorities and OS constraints must agree: " + key)
            self.records[key]["owner"] += ";" + owner
            self.records[key]["mode"] = "static-irq-dispatch"
            return
        self.exclusive_claim(key, owner, mode, source)
        # An exclusive first publisher never grants sharing to a later owner.
        self.publishers[key] = signature if shared else None


@dataclass(frozen=True)
class SharedIrqMember:
    group: "SharedIrqGroup"
    handler: str
    priority: int
    mask: int
    state: Address
    owner: str


@dataclass(frozen=True)
class SharedIrqGroup:
    """One explicit dispatch contract; source paths use the static IR grammar."""
    state_type: str
    dispatch: str
    barrier: str | None
    handler: str
    priority: str
    mask: str

    @classmethod
    def from_validated(cls, value):
        names = {"state_type", "dispatch", "barrier", "handler", "priority", "mask"}
        if not isinstance(value, Mapping) or set(value) != names:
            fail("Shared IRQ requires exact connection fields")
        for name in ("state_type", "dispatch"):
            Symbol(value[name])
        if value["barrier"] is not None:
            Symbol(value["barrier"])
        for name in ("handler", "priority", "mask"):
            _path(value[name])
        return cls(*(value[name] for name in (
            "state_type", "dispatch", "barrier", "handler", "priority", "mask")))

    def member(self, item):
        _immutable(item)
        if (not isinstance(item.id, str) or
                re.fullmatch(r"[a-z][a-z0-9_-]*", item.id) is None):
            fail("Shared IRQ owner requires an ordinary instance identity")
        context = {"item": item, "selected": item.integration}
        handler = _read(self.handler, context)
        Symbol(handler)
        if handler not in {vector.handler for vector in item.integration.vector_bindings}:
            fail("Shared IRQ handler is outside the selected publisher bindings")
        priority = integer(_read(self.priority, context), 0, 255, "Shared IRQ priority")
        if item.integration.irq is None or priority != item.integration.irq.priority:
            fail("Shared IRQ priority differs from its resolved owner")
        mask = integer(_read(self.mask, context), 1, context="Shared IRQ source mask")
        return SharedIrqMember(self, handler, priority, mask,
            Address("s_nx_port_" + item.id.replace("-", "_")), item.id)


def connections(selections, *, claims=None, vector_handlers=None):
    """Build one static array and dispatcher per exact selected handler."""
    groups = {}
    for specification, item in selections:
        member = specification.member(item)
        groups.setdefault(member.handler, []).append(member)
    if claims is not None:
        if not isinstance(vector_handlers, Mapping):
            fail("Shared IRQ claims require the selected physical vector bindings")
        for handler, members in groups.items():
            actual = {owner for claim in claims
                      if vector_handlers.get(claim.key) == handler
                      for owner in claim.owner.split(";")}
            if actual != {member.owner for member in members}:
                fail("Shared IRQ dispatcher differs from canonical vector ownership: " + handler)
        for claim in claims:
            if (claim.mode == "static-irq-dispatch" and
                    vector_handlers.get(claim.key) not in groups):
                fail("Shared IRQ claim has no selected static dispatcher: " + claim.key)
    nodes = []
    for handler, members in groups.items():
        first = members[0]
        signature = first.group.state_type, first.group.dispatch, first.group.barrier
        mask, states = 0, []
        for member in members:
            actual = member.group.state_type, member.group.dispatch, member.group.barrier
            if actual != signature or member.priority != first.priority:
                fail("Shared IRQ dispatch contract and priorities must agree")
            if mask & member.mask or member.state in states:
                fail("Shared IRQ source ownership overlaps another selected member")
            mask |= member.mask
            states.append(member.state)
        array = "s_nx_irq_" + handler + "_ports"
        nodes.append(Declaration(first.group.state_type, array, Array(tuple(states)),
            count=len(states), pointer=True, const=True))
        nodes.append(IrqConnection(handler, (DirectCall(first.group.dispatch,
            (Symbol(array), len(states), mask)),),
            exit_barrier=Symbol(first.group.barrier) if first.group.barrier else None))
    return tuple(nodes)
