"""Immutable reviewed hardware bindings for the maintained paired controllers.

JSON SoC facts remain independent assertions, and C SDK/register assertions
remain in providers. This module removes repeated internal generator mappings;
it does not infer silicon capabilities or authorize additional Board routes.
"""
from dataclasses import dataclass
from types import MappingProxyType

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


def _uart(name, apb, irq, tx, rx=None):
    modes = (("dma-tx", (tx,)),)
    if rx is not None:
        modes += (("rx-dma-idle", (rx,)), ("rx-dma-continuous", (rx,)),
                  ("dma-duplex", (tx, rx)),
                  ("dma-duplex-continuous", (tx, rx)))
    return ControllerBinding(name, "uart", apb, ((name, irq),), modes)


def _spi(name, apb, mode, tx, rx):
    return ControllerBinding(name, "spi", apb, (), ((mode, (tx, rx)),))


def _i2c(name, irq, rx, tx):
    return ControllerBinding(name, "i2c", 1,
                             ((name + "_EV", irq), (name + "_ER", irq + 1)),
                             (("dma-messages", (rx, tx)),))


_BINDINGS = MappingProxyType({
    "stm32f407": (
        _uart("USART1", 2, 37, (2, 7, 4), (2, 2, 4)),
        _uart("USART2", 1, 38, (1, 6, 4)),
        _spi("SPI1", 2, "dma", (2, 3, 3), (2, 0, 3)),
        _spi("SPI2", 1, "dma", (1, 4, 0), (1, 3, 0)),
        _i2c("I2C1", 31, (1, 0, 1), (1, 6, 1)),
        _i2c("I2C2", 33, (1, 2, 7), (1, 7, 7)),
        ControllerBinding("ADC1", "adc", 2, (), (
            ("trigger-dma", ((2, 4, 0),)),
            ("trigger-dma-continuous", ((2, 4, 0),)))),
    ),
    "gd32f470": (
        _uart("USART0", 2, 37, (1, 7, 4), (1, 2, 4)),
        _uart("USART1", 1, 38, (0, 6, 4)),
        _spi("SPI4", 2, "dma-full-duplex", (1, 4, 2), (1, 3, 2)),
        _spi("SPI0", 2, "dma-full-duplex", (1, 5, 3), (1, 2, 3)),
        _i2c("I2C0", 31, (0, 0, 1), (0, 6, 1)),
        _i2c("I2C1", 33, (0, 2, 7), (0, 7, 7)),
        ControllerBinding("ADC0", "adc", 2, (), (
            ("trigger-dma", ((1, 0, 0),)),
            ("trigger-dma-continuous", ((1, 0, 0),)))),
    ),
})
_DMA_IRQS = MappingProxyType({
    "stm32f407": ((1, (11, 12, 13, 14, 15, 16, 17, 47)),
                  (2, (56, 57, 58, 59, 60, 68, 69, 70))),
    "gd32f470": ((0, (11, 12, 13, 14, 15, 16, 17, 47)),
                 (1, (56, 57, 58, 59, 60, 68, 69, 70))),
})


def maintained(family, controller):
    """Only a maintained exact family/controller pair may produce bindings."""
    result = next((port for port in _BINDINGS.get(family, ())
                   if port.controller == controller), None)
    if result is None:
        fail(f"Unmaintained controller binding: {family}/{controller}")
    return result


def bindings(family):
    """Return the immutable reviewed bindings for one exact family."""
    return _BINDINGS.get(family, ())


def fixed_dma(family):
    """Project normalized resources for independent SoC route validation."""
    return MappingProxyType({
        (port.controller, mode): tuple(
            f"DMA{dma}:stream{stream}:channel{channel}"
            for dma, stream, channel in paths)
        for port in bindings(family) for mode, paths in port.dma_modes})


def dma_irq(family, dma, stream):
    """Reject an unknown engine or lane instead of producing another vector."""
    lanes = next((lanes for number, lanes in _DMA_IRQS.get(family, ())
                  if number == dma), None)
    if (lanes is None or not isinstance(stream, int) or isinstance(stream, bool)
            or not 0 <= stream < len(lanes)):
        fail(f"Unmaintained DMA vector: {family}/DMA{dma}/{stream}")
    return lanes[stream]


def dma_vector(family, path):
    """Return the exact SDK handler stem for a reviewed engine/lane pair."""
    dma, stream, _ = path
    dma_irq(family, dma, stream)
    suffix = {"stm32f407": "Stream", "gd32f470": "Channel"}.get(family)
    if suffix is None:
        fail(f"Unmaintained DMA vector family: {family}")
    return f"DMA{dma}_{suffix}{stream}"
