"""Exact F407 controller banks, ordered DMA paths and SDK vector assertions."""
from types import MappingProxyType

from tools.configure.providers.hardware import (
    ControllerBinding, fixed_dma_bindings, i2c_binding, spi_binding, uart_binding)


CONTROLLER_BINDINGS = (
    uart_binding("USART1", 2, 37, (2, 7, 4), (2, 2, 4)),
    uart_binding("USART2", 1, 38, (1, 6, 4)),
    spi_binding("SPI1", 2, "dma", (2, 3, 3), (2, 0, 3)),
    spi_binding("SPI2", 1, "dma", (1, 4, 0), (1, 3, 0)),
    i2c_binding("I2C1", 31, (1, 0, 1), (1, 6, 1)),
    i2c_binding("I2C2", 33, (1, 2, 7), (1, 7, 7)),
    ControllerBinding("ADC1", "adc", 2, (), (
        ("trigger-dma", ((2, 4, 0),)),
        ("trigger-dma-continuous", ((2, 4, 0),)))),
)
DMA_IRQS = ((1, (11, 12, 13, 14, 15, 16, 17, 47)),
            (2, (56, 57, 58, 59, 60, 68, 69, 70)))
FIXED_DMA = fixed_dma_bindings(CONTROLLER_BINDINGS)
CONTROLLER_PAIRS = MappingProxyType({
    "uart": ("USART1", "USART2"), "spi": ("SPI1", "SPI2"),
    "i2c": ("I2C1", "I2C2")})
SECONDARY_FACTS = MappingProxyType({
    "USART2": MappingProxyType({
        "kind": "uart", "modes": ("dma-tx",), "pins": ("PA2", "PA3"),
        "irq": ("USART2_IRQn",), "clock_hz": 42000000}),
    "SPI2": MappingProxyType({
        "kind": "spi", "modes": ("dma",),
        "pins": ("PB13", "PB14", "PB15"), "irq": (),
        "clock_hz": 42000000}),
    "I2C2": MappingProxyType({
        "kind": "i2c", "modes": ("irq-messages", "dma-messages"),
        "pins": ("PB10", "PB11"), "irq": ("I2C2_EV", "I2C2_ER"),
        "clock_hz": 42000000}),
})
