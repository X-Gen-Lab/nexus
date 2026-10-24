"""Maintained stm32f407 capability and static assembly contract."""
from types import MappingProxyType

from .. import common, descriptor, hardware

CPU_ABI = MappingProxyType({'arch': 'cortex-m4', 'fpu': 'fpv4-sp-d16', 'float_abi': 'hard'})
ENUM_ABI = 'short-enums'
IRQ_PRIORITY_BITS = 4
DWT_CYCCNT = True
FREERTOS_PORT = 'GCC/ARM_CM4F'
EXTERNAL_IRQ_COUNT = 82
CPU_FEATURES = MappingProxyType({
    'dwt_cyccnt': True, 'mpu_version': 7, 'icache_line_bytes': 0,
    'dcache_line_bytes': 0, 'security': 'single', 'sau': False, 'mve': 'none',
    'dsp': True, 'mpu_regions': 8,
})
PREFIX = 'stm32'
MODEL = False
FLASH_PROGRAM_UNIT = 4
FLASH_INPUT_ALIGNMENT = 1
FACT_REFERENCES = descriptor.maintained("stm32f407").references
ETH_CONTROLLER = 'ETH'
CONTROLLER_INTERFACES = MappingProxyType({
    'ETH': MappingProxyType({
        'eth_frames': ('rmii-vlan-sg-ptp',)})})
USB_CONTROLLER = 'OTG_FS'
CAN_CONTROLLERS = ('CAN1', 'CAN2')
CAN_CLOCK_HZ = 42000000
# Capability assertion for the actual fixed system.c cold implementation.
CLOCK_PROFILES = MappingProxyType({
    'hse8-pll168': MappingProxyType({
        'hse_hz': 8000000, 'core_hz': 168000000,
        'apb1_hz': 42000000, 'apb2_hz': 84000000,
    }),
})
DMA_VECTOR = 'Stream'
DMA_TOPOLOGY = (frozenset((1, 2)), range(8), range(8))
LIMITS = MappingProxyType({
    'uart_baud': (1282, 1000000), 'spi_hz': (328125, 42000000),
    'timer_hz': 84000000, 'pwm_period': 65535,
})
ADC_CHANNEL_SEQUENCES = ((0,), (0, 1))
SPI_REQUIRES_ENDPOINT = True
_FIXED_DMA = hardware.fixed_dma("stm32f407")

_MODE_IMPLEMENTATIONS = MappingProxyType({
    ('timer_capture', 'edge-capture'): 'timer_capture',
    ('advanced_pwm', 'complementary-pwm'): 'advanced_pwm',
    ('eth', 'raw-rmii'): 'eth',
    ('eth_frames', 'rmii-vlan-sg-ptp'): 'eth_frames',
    ('usb_device', 'device-fs'): 'usb_device',
    ('can', 'classic-irq'): 'can',
    ('gpio', 'input'): 'gpio',
    ('gpio', 'output'): 'gpio',
    ('uart', 'irq-byte-event'): 'uart',
    ('uart', 'irq-words'): 'uart_word',
    ('spi', 'short-poll'): 'spi',
    ('i2c', 'poll-messages'): 'i2c',
    ('i2c', 'irq-messages'): 'i2c_async',
    ('i2c', 'dma-messages'): 'i2c_async',
    ('flash', 'poll-word'): 'flash',
    ('watchdog', 'independent'): 'watchdog',
    ('exti', 'edge-event'): 'exti',
    ('pwm', 'fixed-pwm'): 'pwm',
    ('adc', 'low-rate-scan'): 'adc',
    ('uart', 'dma-tx'): 'uart_dma',
    ('uart', 'irq-blocks'): 'uart_stream',
    ('uart', 'rx-dma-idle'): 'uart_receive',
    ('uart', 'dma-duplex'): 'uart_duplex',
    ('uart', 'rx-dma-continuous'): 'uart_receive',
    ('uart', 'dma-duplex-continuous'): 'uart_duplex',
    ('spi', 'dma'): 'spi_dma',
    ('adc', 'single-shot'): 'adc',
    ('adc', 'trigger-dma'): 'adc_stream',
    ('adc', 'trigger-dma-continuous'): 'adc_continuous',
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
    elif kind == "adc" and tuple(selection["channels"]) not in ADC_CHANNEL_SEQUENCES:
        common.fail("ADC channel sequence is not implemented by the provider")


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
    return common.dma_irq(dma, DMA_VECTOR, DMA_TOPOLOGY)


def validate_dma_resource(resource):
    return common.validate_dma_resource(resource, DMA_TOPOLOGY)


def shares_irq(kind, key):
    return kind == "exti" and key in {"irq:EXTI9_5", "irq:EXTI15_10"}


def builtin_cs(controller):
    del controller
    return False

# Explicit software capabilities do not authorize an unreviewed route.
ADC_GROUP = None
PWM_SHARED = None
ENDPOINT_POLICY = common.EndpointPolicy(
    spi_transaction=True, spi_dma=("SPI1", "dma"))
TIMEBASE_ALARM = True
validate_mode_context = common.validate_mode_context
mode_fields = common.mode_fields
validate_controller_set = common.validate_controller_set
validate_endpoint_clock = common.validate_endpoint_clock
configuration_header = common.configuration_header
validate_user_io_context = common.validate_user_io_context
