"""Maintained gd32f470 capability and static assembly contract."""
from types import MappingProxyType
from .. import common

CPU_ABI = MappingProxyType({'arch': 'cortex-m4', 'fpu': 'fpv4-sp-d16', 'float_abi': 'hard'})
ENUM_ABI = 'short-enums'
IRQ_PRIORITY_BITS = 4
DWT_CYCCNT = True
FREERTOS_PORT = 'GCC/ARM_CM4F'
EXTERNAL_IRQ_COUNT = 91
CPU_FEATURES = MappingProxyType({
    'dwt_cyccnt': True, 'mpu_version': 7, 'icache_line_bytes': 0,
    'dcache_line_bytes': 0, 'security': 'single', 'sau': False, 'mve': 'none',
    'dsp': True, 'mpu_regions': 8,
})
PREFIX = 'gd32'
MODEL = False
DMA_VECTOR = 'Channel'
DMA_TOPOLOGY = (frozenset((0, 1)), range(8), range(8))
LIMITS = MappingProxyType({
    'uart_baud': (1526, 1000000), 'spi_hz': (390625, 50000000),
    'timer_hz': 100000000, 'pwm_period': 65535,
})
ADC_CHANNEL_SEQUENCES = ((0,), (0, 1))
SPI_REQUIRES_ENDPOINT = True
_FIXED_DMA = MappingProxyType({('USART0', 'dma-tx'): ('DMA1:stream7:channel4',), ('SPI4', 'dma-full-duplex'): ('DMA1:stream4:channel2', 'DMA1:stream3:channel2'), ('ADC0', 'trigger-dma'): ('DMA1:stream0:channel0',)})

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
    ('spi', 'dma-full-duplex'): 'spi_dma',
    ('adc', 'single-shot'): 'adc',
    ('adc', 'trigger-dma'): 'adc_stream',
})


def validate_selection(selection, route):
    """Validate limits of the implemented provider, not theoretical silicon."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "uart":
        common.integer(selection["baud"], *LIMITS["uart_baud"], "UART baud")
    elif kind == "pwm":
        tick = selection["tick_hz"]
        clock = LIMITS["timer_hz"]
        if clock % tick or not 1 <= clock // tick <= 65536:
            common.fail("PWM tick_hz is not an exact maintained timer divider")
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000), *LIMITS["spi_hz"],
                       "SPI max_hz")
    elif kind == "adc":
        if tuple(selection["channels"]) not in ADC_CHANNEL_SEQUENCES:
            common.fail("ADC channel sequence is not implemented by the provider")
        expected = 1 if selection["mode"] == "trigger-dma" else 7
        if selection["sample_times"] != [expected] * len(selection["channels"]):
            common.fail("GD32 trigger-dma sampling is fifteen cycles (encoding 1)"
                        if expected == 1 else
                        "GD32 polling sampling is 480 cycles (encoding 7)")
        common.integer(selection.get("timeout_ms"), 1, 1000,
                       "ADC calibration timeout_ms")


def mode_implementation(kind, mode):
    implementation = _MODE_IMPLEMENTATIONS.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: gd32f470/{kind}/{mode}")
    return implementation


def validate_dma(route, mode, normalize):
    common.validate_dma(route, mode,
                        _FIXED_DMA.get((route["controller"], mode)),
                        'gd32f470', normalize, 'TIMER2')


def dma_irq(dma):
    return common.dma_irq(dma, DMA_VECTOR, DMA_TOPOLOGY)


def validate_dma_resource(resource):
    return common.validate_dma_resource(resource, DMA_TOPOLOGY)


def shares_irq(kind, key):
    del kind, key
    return False


def builtin_cs(controller):
    return any(pin["function"] == "cs" for pin in controller["pins"])

# Import maintained emitters explicitly; importing a package performs no I/O.
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
