"""Immutable hardware binding mechanisms with explicit SoC-owned records.

JSON SoC facts remain independent assertions, and C SDK/register assertions
remain in providers. This module removes repeated internal generator mappings;
it does not infer silicon capabilities or authorize additional Board routes.
"""
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.providers import maintained as maintained_provider
from tools.configure.providers.common import fail


@dataclass(frozen=True)
class ControllerBinding:
    """Configuration-only register-bank, vector and ordered DMA contract."""

    controller: str
    kind: str
    apb: int
    irqs: tuple
    dma_modes: tuple

    def dma(self, mode):
        """Return an exact reviewed tuple, never the nearest supported mode."""
        return next((paths for name, paths in self.dma_modes if name == mode),
                    None)


def uart_binding(name, apb, irq, tx, rx=None):
    modes = (("dma-tx", (tx,)),)
    if rx is not None:
        modes += (("rx-dma-idle", (rx,)), ("rx-dma-continuous", (rx,)),
                  ("dma-duplex", (tx, rx)),
                  ("dma-duplex-continuous", (tx, rx)))
    return ControllerBinding(name, "uart", apb, ((name, irq),), modes)


def spi_binding(name, apb, mode, tx, rx):
    return ControllerBinding(name, "spi", apb, (), ((mode, (tx, rx)),))


def i2c_binding(name, irq, rx, tx):
    return ControllerBinding(name, "i2c", 1,
                             ((name + "_EV", irq), (name + "_ER", irq + 1)),
                             (("dma-messages", (rx, tx)),))


def maintained(family, controller):
    """Only a maintained exact family/controller pair may produce bindings."""
    result = next((port for port in bindings(family)
                   if port.controller == controller), None)
    if result is None:
        fail(f"Unmaintained controller binding: {family}/{controller}")
    return result


def bindings(family):
    """Return the immutable reviewed bindings for one exact family."""
    return getattr(maintained_provider(family), "CONTROLLER_BINDINGS", ())


def fixed_dma(family):
    """Project normalized resources for independent SoC route validation."""
    return fixed_dma_bindings(bindings(family))


def fixed_dma_bindings(ports):
    """Project local records without loading a partially initialized family."""
    return MappingProxyType({
        (port.controller, mode): tuple(
            f"DMA{dma}:stream{stream}:channel{channel}"
            for dma, stream, channel in paths)
        for port in ports for mode, paths in port.dma_modes})


def dma_irq(family, dma, stream):
    """Reject an unknown engine or lane instead of producing another vector."""
    irqs = getattr(maintained_provider(family), "DMA_IRQS", ())
    lanes = next((lanes for number, lanes in irqs
                  if number == dma), None)
    if (lanes is None or not isinstance(stream, int) or isinstance(stream, bool)
            or not 0 <= stream < len(lanes)):
        fail(f"Unmaintained DMA vector: {family}/DMA{dma}/{stream}")
    return lanes[stream]


def dma_vector(family, path):
    """Return the exact SDK handler stem for a reviewed engine/lane pair."""
    dma, stream, _ = path
    dma_irq(family, dma, stream)
    suffix = maintained_provider(family).DMA_VECTOR
    if suffix is None:
        fail(f"Unmaintained DMA vector family: {family}")
    return f"DMA{dma}_{suffix}{stream}"
