"""Exact GD470 controller banks, ordered DMA paths and SDK vector assertions."""
from types import MappingProxyType

from tools.configure.providers.hardware import (
    ControllerBinding, fixed_dma_bindings, i2c_binding, spi_binding, uart_binding)


CONTROLLER_BINDINGS = (
    uart_binding("USART0", 2, 37, (1, 7, 4), (1, 2, 4)),
    uart_binding("USART1", 1, 38, (0, 6, 4)),
    spi_binding("SPI4", 2, "dma-full-duplex", (1, 4, 2), (1, 3, 2)),
    spi_binding("SPI0", 2, "dma-full-duplex", (1, 5, 3), (1, 2, 3)),
    i2c_binding("I2C0", 31, (0, 0, 1), (0, 6, 1)),
    i2c_binding("I2C1", 33, (0, 2, 7), (0, 7, 7)),
    ControllerBinding("ADC0", "adc", 2, (), (
        ("trigger-dma", ((1, 0, 0),)),
        ("trigger-dma-continuous", ((1, 0, 0),)))),
)
DMA_IRQS = ((0, (11, 12, 13, 14, 15, 16, 17, 47)),
            (1, (56, 57, 58, 59, 60, 68, 69, 70)))
FIXED_DMA = fixed_dma_bindings(CONTROLLER_BINDINGS)
CONTROLLER_PAIRS = MappingProxyType({
    "uart": ("USART0", "USART1"), "spi": ("SPI4", "SPI0"),
    "i2c": ("I2C0", "I2C1")})
SECONDARY_FACTS = MappingProxyType({
    "USART1": MappingProxyType({
        "kind": "uart", "modes": ("irq-byte-event", "irq-blocks", "dma-tx"),
        "pins": ("PA2", "PA3"), "irq": ("USART1_IRQn",),
        "clock_hz": 50000000}),
    "SPI0": MappingProxyType({
        "kind": "spi", "modes": ("short-poll", "dma-full-duplex"),
        "pins": ("PA5", "PA6", "PA7"), "irq": (),
        "clock_hz": 100000000}),
    "I2C1": MappingProxyType({
        "kind": "i2c", "modes": ("poll-messages", "irq-messages", "dma-messages"),
        "pins": ("PB10", "PB11"), "irq": ("I2C1_EV", "I2C1_ER"),
        "clock_hz": 50000000}),
})
