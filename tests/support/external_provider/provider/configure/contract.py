"""Software-only external GPIO reference capability contract."""
from tools.configure.uart_formats import UartFormatPolicy
from types import MappingProxyType
from tools.configure.providers import common

PAD_FACTS = MappingProxyType({
    "PA0": MappingProxyType({"registers": "GPIOA", "port_index": 0, "number": 0}),
    "PA3": MappingProxyType({"registers": "GPIOA", "port_index": 0, "number": 3}),
})

PARTS = MappingProxyType({
    "NATIVE_MODEL": common.PartPolicy("host-model"),
})
PUBLIC_FORMATS = frozenset()

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

_MODE_IMPLEMENTATIONS = MappingProxyType({('gpio', 'output'): 'gpio', ('gpio', 'input'): 'gpio'})


def validate_selection(selection, route):
    """Validate limits of the implemented provider, not theoretical silicon."""
    mode_implementation(route["kind"], selection["mode"], route["controller"])


def mode_implementation(kind, mode, controller=None):
    del controller
    implementation = _MODE_IMPLEMENTATIONS.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: external_lifecycle_model/{kind}/{mode}")
    return implementation


def validate_dma(route, mode, normalize):
    del normalize
    common.validate_dma(route, mode,
                        _FIXED_DMA.get((route["controller"], mode)),
                        'native')


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
ENDPOINT_POLICY = common.EndpointPolicy()
TIMEBASE_ALARM = False
validate_mode_context = common.validate_mode_context
mode_fields = common.mode_fields
validate_controller_set = common.validate_controller_set
validate_endpoint_clock = common.validate_endpoint_clock
configuration_header = common.configuration_header
