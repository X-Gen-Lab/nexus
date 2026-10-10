"""Maintained native capability and static assembly contract."""
from types import MappingProxyType
from .. import common

CPU_ABI = MappingProxyType({'arch': 'native', 'fpu': 'none', 'float_abi': 'native'})
ENUM_ABI = 'native-int'
# Preserve the authored model priority range; Native has no physical NVIC.
IRQ_PRIORITY_BITS = 4
DWT_CYCCNT = False
FREERTOS_PORT = None
PREFIX = 'native'
MODEL = True
DMA_VECTOR = None
SPI_REQUIRES_ENDPOINT = False
_FIXED_DMA = MappingProxyType({})

_MODE_IMPLEMENTATIONS = MappingProxyType({
    ('gpio', 'input'): 'gpio',
    ('gpio', 'output'): 'gpio',
    ('uart', 'irq-byte-event'): 'uart',
    ('spi', 'short-poll'): 'spi',
    ('i2c', 'poll-messages'): 'i2c',
    ('flash', 'poll-byte'): 'flash',
    ('watchdog', 'independent'): 'watchdog',
    ('exti', 'edge-event'): 'exti',
    ('pwm', 'fixed-pwm'): 'pwm',
    ('adc', 'low-rate-scan'): 'adc',
})


def validate_selection(selection, route):
    """Validate limits of the implemented provider, not theoretical silicon."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "uart":
        common.integer(selection["baud"], 1200, 1000000, "UART baud")
    elif kind == "pwm":
        tick = selection["tick_hz"]
        clock = 100000000
        if clock % tick or not 1 <= clock // tick <= 65536:
            common.fail("PWM tick_hz is not an exact maintained timer divider")
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000), 390625, 50000000,
                       "SPI max_hz")


def mode_implementation(kind, mode):
    implementation = _MODE_IMPLEMENTATIONS.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: native/{kind}/{mode}")
    return implementation


def validate_dma(route, mode, normalize):
    common.validate_dma(route, mode,
                        _FIXED_DMA.get((route["controller"], mode)),
                        'native', normalize, None)


def dma_irq(dma):
    return common.dma_irq(dma, DMA_VECTOR)


def shares_irq(kind, key):
    del kind, key
    return False


def builtin_cs(controller):
    del controller
    return True

# Import maintained emitters explicitly; importing a package performs no I/O.
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
