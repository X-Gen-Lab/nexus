"""Exact F407 controller banks, ordered DMA paths and SDK vector assertions."""
from types import MappingProxyType

from tools.configure.providers.hardware import (
    ControllerBinding, fixed_dma_bindings, i2c_binding, spi_binding, uart_binding)


# DS8626 Rev12 Table7: ADC1/2 share the input map; ADC3 has distinct upper pads.
ADC_PIN_CHANNELS = MappingProxyType({
    **{(controller, pin): channel
       for controller in ("ADC1", "ADC2")
       for channel, pin in enumerate(
           (*[f"PA{i}" for i in range(8)], "PB0", "PB1",
            *[f"PC{i}" for i in range(6)]))},
    **{("ADC3", pin): channel for channel, pin in enumerate(
        ("PA0", "PA1", "PA2", "PA3", "PF6", "PF7", "PF8", "PF9", "PF10",
         "PF3", "PC0", "PC1", "PC2", "PC3", "PF4", "PF5"))},
})

DAC_PIN_CHANNELS = MappingProxyType({("DAC", "PA4"): 1,
                                   ("DAC", "PA5"): 2})
PWM_PIN_CHANNELS = MappingProxyType({
    ("TIM3", "PA6"): 1, ("TIM3", "PA7"): 2,
    ("TIM3", "PB0"): 3, ("TIM3", "PB1"): 4,
    **{("TIM5", f"PA{channel - 1}"): channel for channel in range(1, 5)},
})
# RM0090 Rev22 Tables42/43 and section14.3.7: one request for a packed pair.
DAC_DMA_ROUTES = MappingProxyType({
    1: ("DMA1:stream5:channel7",),
    2: ("DMA1:stream6:channel7",),
    3: ("DMA1:stream5:channel7",),
})


CONTROLLER_BINDINGS = (
    uart_binding("USART1", 2, 37, (2, 7, 4), (2, 2, 4)),
    uart_binding("USART2", 1, 38, (1, 6, 4), (1, 5, 4)),
    uart_binding("USART3", 1, 39, (1, 3, 4), (1, 1, 4)),
    uart_binding("UART4", 1, 52, (1, 4, 4), (1, 2, 4)),
    uart_binding("UART5", 1, 53, (1, 7, 4), (1, 0, 4)),
    uart_binding("USART6", 2, 71, (2, 6, 5), (2, 1, 5)),
    spi_binding("SPI1", 2, "dma", (2, 3, 3), (2, 0, 3)),
    spi_binding("SPI2", 1, "dma", (1, 4, 0), (1, 3, 0)),
    spi_binding("SPI3", 1, "dma", (1, 5, 0), (1, 0, 0)),
    i2c_binding("I2C1", 31, (1, 0, 1), (1, 6, 1)),
    i2c_binding("I2C2", 33, (1, 2, 7), (1, 7, 7)),
    i2c_binding("I2C3", 72, (1, 2, 3), (1, 4, 3)),
    ControllerBinding("ADC1", "adc", 2, (), (
        ("trigger-dma", ((2, 4, 0),)),
        ("trigger-dma-continuous", ((2, 4, 0),)))),
    ControllerBinding("ADC2", "adc", 2, (), (
        ("trigger-dma", ((2, 2, 1),)),
        ("trigger-dma-continuous", ((2, 2, 1),)))),
    ControllerBinding("ADC3", "adc", 2, (), (
        ("trigger-dma", ((2, 1, 2),)),
        ("trigger-dma-continuous", ((2, 1, 2),)))),
)
DMA_IRQS = ((1, (11, 12, 13, 14, 15, 16, 17, 47)),
            (2, (56, 57, 58, 59, 60, 68, 69, 70)))
FIXED_DMA = fixed_dma_bindings(CONTROLLER_BINDINGS)
CONTROLLER_PAIRS = MappingProxyType({
    "uart": ("USART1", "USART2"), "spi": ("SPI1", "SPI2"),
    "i2c": ("I2C1", "I2C2")})
SECONDARY_FACTS = MappingProxyType({
    "USART2": MappingProxyType({
        "kind": "uart", "modes": ("dma-tx", "irq-byte-event", "rx-dma-idle",
                  "rx-dma-continuous", "dma-duplex", "dma-duplex-continuous",
                  "irq-words", "irq-blocks"),
        "pins": ("PA2", "PA3"),
        "irq": ("USART2_IRQn",), "clock_hz": 42000000}),
    "USART3": MappingProxyType({
        "kind": "uart", "modes": ("irq-byte-event", "dma-tx", "rx-dma-idle",
                  "rx-dma-continuous", "dma-duplex", "dma-duplex-continuous",
                  "irq-words", "irq-blocks"),
        "pins": ("PB10", "PB11"), "irq": ("USART3_IRQn",),
        "clock_hz": 42000000}),
    "UART4": MappingProxyType({
        "kind": "uart", "modes": ("irq-byte-event", "dma-tx", "rx-dma-idle",
                  "rx-dma-continuous", "dma-duplex", "dma-duplex-continuous",
                  "irq-words", "irq-blocks"),
        "pins": ("PA0", "PA1"), "irq": ("UART4_IRQn",),
        "clock_hz": 42000000}),
    "UART5": MappingProxyType({
        "kind": "uart", "modes": ("irq-byte-event", "dma-tx", "rx-dma-idle",
                  "rx-dma-continuous", "dma-duplex", "dma-duplex-continuous",
                  "irq-words", "irq-blocks"),
        "pins": ("PC12", "PD2"), "irq": ("UART5_IRQn",),
        "clock_hz": 42000000}),
    "USART6": MappingProxyType({
        "kind": "uart", "modes": ("irq-byte-event", "dma-tx", "rx-dma-idle",
                  "rx-dma-continuous", "dma-duplex", "dma-duplex-continuous",
                  "irq-words", "irq-blocks"),
        "pins": ("PC6", "PC7"), "irq": ("USART6_IRQn",),
        "clock_hz": 84000000}),
    "SPI2": MappingProxyType({
        "kind": "spi", "modes": ("short-poll", "dma"),
        "pins": ("PB13", "PB14", "PB15"), "irq": (),
        "clock_hz": 42000000}),
    "SPI3": MappingProxyType({
        "kind": "spi", "modes": ("short-poll", "dma"),
        "pins": ("PC10", "PC11", "PC12", "PB3", "PB4", "PB5"),
        "irq": (), "clock_hz": 42000000}),
    "I2C3": MappingProxyType({
        "kind": "i2c", "modes": ("poll-messages", "irq-messages", "dma-messages"),
        "pins": ("PA8", "PC9"), "irq": ("I2C3_EV", "I2C3_ER"),
        "clock_hz": 42000000}),
    "I2C2": MappingProxyType({
        "kind": "i2c", "modes": ("poll-messages", "irq-messages", "dma-messages"),
        "pins": ("PB10", "PB11"), "irq": ("I2C2_EV", "I2C2_ER"),
        "clock_hz": 42000000}),
})
