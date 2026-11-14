"""Immutable SPI endpoint wiring from the same selected Board and pad facts.

This projection adds no hardware behavior or private C names. Controller pads,
endpoint identities and GPIO bindings keep their original authors; only their
static relationship is assembled here.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.ir import EndpointIR, PadBindingIR
from tools.configure.providers.common import fail


@dataclass(frozen=True)
class SpiEndpointBindingIR:
    child: EndpointIR
    cs: PadBindingIR


@dataclass(frozen=True)
class SpiBindingIR:
    pads: tuple[PadBindingIR, ...]
    endpoints: tuple[SpiEndpointBindingIR, ...]
    endpoint_bindings: Mapping[str, SpiEndpointBindingIR]


def binding_values(item, configuration):
    """Reference exact CS pads; ordinary SPI owns one active-low CS per child."""
    if item.kind != "spi" or item.mode == "irq-frames" or configuration is None:
        fail("SPI endpoint wiring requires its selected controller configuration")
    policy = configuration.provider_context.binding_policy
    if policy is None:
        fail("SPI endpoint wiring requires its selected static pad bindings")
    children = tuple(child for child in configuration.devices if child.controller == item.id)
    if not children:
        fail("SPI requires a reviewed CS endpoint")
    pads = list(policy.pad_bindings(item.pins))
    endpoints = []
    for child in children:
        binding = configuration.board_bindings.get(child.options.cs_binding)
        if binding is None:
            fail("SPI CS requires an explicit Board binding")
        pins = configuration.routes[binding["route"]]["pins"]
        if len(pins) != 1:
            fail("SPI CS requires exactly one reviewed pin")
        pad, = policy.pad_bindings(pins)
        if binding.get("initial") != 1 << pad.number:
            fail("SPI active-low CS must declare initial high")
        pads.append(pad)
        endpoints.append(SpiEndpointBindingIR(child, pad))
    if len({pad.identity for pad in pads}) != len(pads):
        fail("SPI controller and endpoint pads must be distinct")
    return SpiBindingIR(tuple(pads), tuple(endpoints),
        MappingProxyType({entry.child.id: entry for entry in endpoints}))
