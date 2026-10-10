"""Maintained stm32f407 capability and static assembly contract."""
from types import MappingProxyType
from .. import common

CPU_ABI = MappingProxyType({'arch': 'cortex-m4', 'fpu': 'fpv4-sp-d16', 'float_abi': 'hard'})
ENUM_ABI = 'short-enums'
IRQ_PRIORITY_BITS = 4
DWT_CYCCNT = True
FREERTOS_PORT = 'GCC/ARM_CM4F'
EXTERNAL_IRQ_COUNT = 82
CPU_FEATURES = MappingProxyType({
    'dwt_cyccnt': True, 'mpu_version': 7, 'icache_line_bytes': 0,
    'dcache_line_bytes': 0, 'security': 'single', 'sau': False, 'mve': 'none',
    'dsp': True,
})
PREFIX = 'stm32'
MODEL = False
DMA_VECTOR = 'Stream'
SPI_REQUIRES_ENDPOINT = True
_FIXED_DMA = MappingProxyType({('USART1', 'dma-tx'): ('DMA2:stream7:channel4',), ('SPI1', 'dma'): ('DMA2:stream3:channel3', 'DMA2:stream0:channel3'), ('ADC1', 'trigger-dma'): ('DMA2:stream4:channel0',)})

_MODE_IMPLEMENTATIONS = MappingProxyType({
    ('gpio', 'input'): 'gpio',
    ('gpio', 'output'): 'gpio',
    ('uart', 'irq-byte-event'): 'uart',
    ('spi', 'short-poll'): 'spi',
    ('i2c', 'poll-messages'): 'i2c',
    ('flash', 'poll-word'): 'flash',
    ('watchdog', 'independent'): 'watchdog',
    ('exti', 'edge-event'): 'exti',
    ('pwm', 'fixed-pwm'): 'pwm',
    ('adc', 'low-rate-scan'): 'adc',
    ('uart', 'dma-tx'): 'uart_dma',
    ('uart', 'irq-blocks'): 'uart_stream',
    ('spi', 'dma'): 'spi_dma',
    ('adc', 'single-shot'): 'adc',
    ('adc', 'trigger-dma'): 'adc_stream',
})


def validate_selection(selection, route):
    """Validate limits of the implemented provider, not theoretical silicon."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "uart":
        common.integer(selection["baud"], 1282, 1000000, "UART baud")
    elif kind == "pwm":
        tick = selection["tick_hz"]
        clock = 84000000
        if clock % tick or not 1 <= clock // tick <= 65536:
            common.fail("PWM tick_hz is not an exact maintained timer divider")
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000), 328125, 42000000,
                       "SPI max_hz")


def mode_implementation(kind, mode):
    implementation = _MODE_IMPLEMENTATIONS.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32f407/{kind}/{mode}")
    return implementation


def validate_dma(route, mode, normalize):
    common.validate_dma(route, mode,
                        _FIXED_DMA.get((route["controller"], mode)),
                        'stm32f407', normalize, 'TIM3')


def dma_irq(dma):
    return common.dma_irq(dma, DMA_VECTOR)


def shares_irq(kind, key):
    return kind == "exti" and key in {"irq:EXTI9_5", "irq:EXTI15_10"}


def builtin_cs(controller):
    del controller
    return False

# Import maintained emitters explicitly; importing a package performs no I/O.
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
