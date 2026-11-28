"""Package-owned wired DMA routing with no programmable request selector."""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.generation.calls import Symbol
from tools.configure.providers.common import (
    fail, fixed_dma_channel, integer, mode_traits, normalize_resource, route_dma)
from tools.configure.validation import obj


@dataclass(frozen=True)
class FixedChannel:
    engine: str
    lane: int
    channel: str
    irq: str


@dataclass(frozen=True)
class FixedChannelDmaPolicy:
    lanes: Mapping
    transfers: Mapping

    @classmethod
    def from_validated(cls, value):
        obj(value, {"encoding", "lanes", "transfers"}, context="Fixed channel DMA")
        if (value["encoding"] != "fixed-channel" or
                any(not isinstance(value[name], Mapping) or not value[name]
                    for name in ("lanes", "transfers"))):
            fail("Fixed DMA requires explicit channel and transfer mappings")
        lanes = {}
        for path, row in value["lanes"].items():
            physical = fixed_dma_channel("dma:" + path) if isinstance(path, str) else None
            if physical is None:
                fail("Fixed DMA requires canonical physical channel identities")
            obj(row, {"channel", "irq"}, context="Fixed DMA channel")
            for symbol in row.values():
                Symbol(symbol)
            engine, lane = physical
            integer(lane, 1, 65535, "Fixed DMA channel number")
            lanes["dma:" + path] = FixedChannel(engine, lane, row["channel"], row["irq"])
        transfers = {}
        for controller, wires in value["transfers"].items():
            Symbol(controller)
            obj(wires, (), {"tx", "rx"}, "Fixed DMA transfer")
            if not wires or any(not isinstance(path, str) or "dma:" + path not in lanes
                                for path in wires.values()):
                fail("Fixed DMA transfer requires a declared physical channel")
            transfers[controller] = MappingProxyType(dict(wires))
        return cls(MappingProxyType(lanes), MappingProxyType(transfers))

    def channel(self, resource):
        lane = self.lanes.get(resource)
        if lane is None:
            fail("DMA resource is outside the fixed channel topology: " + resource)
        return lane

    def irq(self, path):
        return normalize_resource("irq:" + self.channel("dma:" + path).irq)

    def validate_route(self, route, mode):
        traits = mode_traits(route["kind"], mode)
        declared = self.transfers.get(route["controller"], {})
        directions = (direction for direction, selected in (
            ("tx", traits.tx_dma), ("rx", traits.rx_dma)) if selected)
        expected = tuple(declared.get(direction) for direction in directions)
        if None in expected or tuple(route_dma(route, mode)) != expected:
            fail("DMA route differs from the wired DMA requests for " + route["controller"])
        if len(set(expected)) != len(expected):
            fail("Selected DMA directions cannot alias one physical lane")
