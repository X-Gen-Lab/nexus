"""Maintained native capability and static assembly contract."""
from tools.configure.uart_formats import UartFormatPolicy
from types import MappingProxyType
from tools.configure.providers import common

PARTS = MappingProxyType({
    "NATIVE_MODEL": common.PartPolicy("host-model"),
})
PUBLIC_FORMATS = frozenset(("uart", "spi", "i2c", "adc", "can",
                            "eth", "eth_frames"))

UART_FORMATS = UartFormatPolicy(words=("UART0",))

CPU_ABI = MappingProxyType({'arch': 'native', 'fpu': 'none', 'float_abi': 'native'})
ENUM_ABI = 'native-int'
# Preserve the authored model priority range; Native has no physical NVIC.
IRQ_PRIORITY_BITS = 4
DWT_CYCCNT = False
FREERTOS_PORT = None
EXTERNAL_IRQ_COUNT = 0
CPU_FEATURES = MappingProxyType({
    'dwt_cyccnt': False, 'mpu_version': 0, 'icache_line_bytes': 0,
    'dcache_line_bytes': 0, 'security': 'single', 'sau': False, 'mve': 'none',
    'dsp': False,
})
PREFIX = 'native'
MODEL = True
DMA_VECTOR = None
DMA_TOPOLOGY = (frozenset(), range(0), range(0))
LIMITS = MappingProxyType({
    'uart_baud': (1200, 1000000), 'spi_hz': (390625, 50000000),
    'timer_hz': 100000000, 'pwm_period': 65535,
})
SPI_REQUIRES_ENDPOINT = False
_FIXED_DMA = MappingProxyType({})

_MODE_IMPLEMENTATIONS = MappingProxyType({
    ("rtc", "calendar-alarm"): "rtc",
    ("lptim", "counter16-oneshot"): "lptim",
    ('gpio', 'input'): 'gpio',
    ('gpio', 'output'): 'gpio',
    ('uart', 'irq-byte-event'): 'uart',
    ('uart', 'irq-words'): 'uart_word',
    ('spi', 'short-poll'): 'spi',
    ('spi', 'async-model'): 'spi_dma',
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
        common.integer(selection["baud"], *LIMITS["uart_baud"], "UART baud")
    elif kind == "pwm":
        tick = selection["tick_hz"]
        clock = LIMITS["timer_hz"]
        if clock % tick or not 1 <= clock // tick <= 65536:
            common.fail("PWM tick_hz is not an exact maintained timer divider")
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000), *LIMITS["spi_hz"],
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
    return common.dma_irq(dma, DMA_VECTOR, DMA_TOPOLOGY)


def validate_dma_resource(resource):
    return common.validate_dma_resource(resource, DMA_TOPOLOGY)


def shares_irq(kind, key):
    del kind, key
    return False


def builtin_cs(controller):
    del controller
    return True

# Explicit software capabilities do not authorize an unreviewed route.
ADC_GROUP = None
PWM_SHARED = None
ENDPOINT_POLICY = common.EndpointPolicy(
    spi_transaction=True, spi_dma=(None, "async-model"), model_async_echo=True)
TIMEBASE_ALARM = False
validate_mode_context = common.validate_mode_context
mode_fields = common.mode_fields
validate_controller_set = common.validate_controller_set
validate_endpoint_clock = common.validate_endpoint_clock
configuration_header = common.configuration_header
validate_user_io_context = common.validate_user_io_context
