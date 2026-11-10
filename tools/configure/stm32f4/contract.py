"""Maintained STM32F4 capability and static assembly contract."""
from tools.configure.stream_profiles import StreamGeometry
from tools.configure.uart_formats import UartFormatPolicy
from tools.configure.calls import Symbol
from types import MappingProxyType

from tools.configure.providers import common, descriptor
from .hardware import (ADC_PIN_CHANNELS, ADC_INTERNAL_CHANNELS,
                       DAC_PIN_CHANNELS, PWM_PIN_CHANNELS, PWM_TIMERS,
                       FIXED_DMA as _FIXED_DMA)
from tools.configure.adc import RESOLUTION_BITS

PARTS = MappingProxyType({
    "STM32F407VET6": common.PartPolicy("LQFP100", frozenset(('ccm',))),
    "STM32F407ZGT6": common.PartPolicy("LQFP144", frozenset(('ccm',))),
})
PUBLIC_FORMATS = frozenset(("uart", "spi", "i2c", "adc", "can",
                            "eth", "eth_frames"))
CLASSIC_CAN_MAILBOXES = 3

UART_FORMATS = UartFormatPolicy(
    words=("USART1", "USART2", "USART3", "UART4", "UART5", "USART6"),
    irq_probe=True, byte_frames=True)

# Exact shared admission helper used by the byte-mode transport itself.
UART_TX_VALIDATORS = MappingProxyType({
    ("stm32f4", "irq-byte-event", "uart"):
        Symbol("nx_stm32_uart_tx_validate"),
})

STREAM_GEOMETRY = StreamGeometry(2, 1, 65535)

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
# The reviewed official startup table fits a 512-byte vector reservation.
OFFSET_IMAGE_ALIGNMENT = 512
OFFSET_IMAGE_VECTOR_SYMBOL = 'g_pfnVectors'
FACT_REFERENCES = descriptor.maintained("stm32f4").references
ETH_CONTROLLER = 'ETH'
CONTROLLER_INTERFACES = MappingProxyType({
    'TIM3': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    'TIM4': MappingProxyType({'pwm': ('fixed-pwm',), 'encoder': ('quadrature',)}),
    'TIM5': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    'ETH': MappingProxyType({
        'eth_frames': ('rmii-vlan-sg-ptp',)}),
    'FSMC': MappingProxyType({
        'memory_window': ('cpu-window', 'startup-memory'),
        'parallel16': ('8080-16',)})})
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
    'i2c_hz': (5129, 400000),
    'timer_hz': 84000000, 'pwm_period': 0xFFFFFFFF,
    'adc_resolution_bits': RESOLUTION_BITS,
})
SPI_REQUIRES_ENDPOINT = True
PIN_ELECTRICAL = True

_MODE_IMPLEMENTATIONS = MappingProxyType({
    ('encoder', 'quadrature'): 'encoder',
    ('crc', 'crc32-words'): 'crc',
    ('dac', 'direct'): 'dac',
    ('dac', 'triggered'): 'dac',
    ('dac', 'noise'): 'dac',
    ('dac', 'triangle'): 'dac',
    ('dac', 'trigger-dma'): 'dac_dma',
    ('dac', 'trigger-dma-circular'): 'dac_dma',
    ('memory_window', 'startup-memory'): 'fsmc',
    ('external_bus', 'async16'): 'fsmc',
    ('memory_window', 'cpu-window'): 'fsmc',
    ('parallel16', '8080-16'): 'fsmc',
    ('rtc', 'adopt-retained'): 'rtc',
    ('rtc', 'provision-empty'): 'rtc',
    ('block', 'sdio-poll'): 'sdio',
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
    ('watchdog', 'window'): 'window_watchdog',
    ('exti', 'edge-event'): 'exti',
    ('exti', 'pvd-level'): 'pvd',
    ('exti', 'pvd-events'): 'pvd_events',
    ('pwm', 'fixed-pwm'): 'pwm',
    ('adc', 'low-rate-scan'): 'adc',
    ('uart', 'dma-tx'): 'uart_hal',
    ('uart', 'irq-blocks'): 'uart_stream',
    ('uart', 'rx-dma-idle'): 'uart_receive',
    ('uart', 'dma-duplex'): 'uart_duplex',
    ('uart', 'rx-dma-continuous'): 'uart_receive',
    ('uart', 'dma-duplex-continuous'): 'uart_duplex',
    ('spi', 'dma'): 'spi_dma',
    ('adc', 'single-shot'): 'adc',
    ('adc', 'injected-shot'): 'adc_injected',
    ('adc', 'trigger-dma'): 'adc_stream',
    ('adc', 'trigger-dma-continuous'): 'adc_continuous',
})


def validate_selection(selection, route):
    """Validate limits of the implemented provider, not theoretical silicon."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == 'exti' and selection['mode'] in {'pvd-level', 'pvd-events'}:
        if route['controller'] != 'EXTI16' or route['pins']:
            common.fail('PVD requires its pinless EXTI16 controller')
    if kind == "watchdog" and selection["mode"] == "window":
        if route["controller"] != "WWDG":
            common.fail("Window watchdog requires its exact WWDG controller")
        common.integer(selection.get("window_ticks", 1), 1, 62, "WWDG window_ticks")
    if kind == "rtc":
        from .rtc import validate
        validate(selection, route)
    elif kind == "block":
        from .block import validate
        validate(selection)
    elif kind == "encoder":
        from .encoder import validate
        validate(selection, route)
    elif kind == "adc" and common.mode_traits(kind, selection["mode"]).dma:
        if (route["controller"], selection["mode"]) not in _FIXED_DMA:
            common.fail("ADC DMA requires a reviewed converter/TIM3 path")
    elif kind == "uart":
        common.integer(selection["baud"], *LIMITS["uart_baud"], "UART baud")
    elif kind == "pwm":
        pins = route["pins"]
        channel = (PWM_PIN_CHANNELS.get((route["controller"], pins[0]["pin"]))
                   if len(pins) == 1 else None)
        timer = PWM_TIMERS.get(route["controller"])
        if (timer is None or channel is None or
                pins[0]["function"] != f"ch{channel}" or
                pins[0].get("af") != timer.af):
            common.fail("PWM requires a reviewed general timer channel route")
        common.integer(selection["period_ticks"], 1,
                       0xFFFFFFFF if route["controller"] == "TIM5" else 65535,
                       "PWM period")
        tick = selection["tick_hz"]
        clock = timer.clock_hz
        if clock % tick or not 1 <= clock // tick <= 65536:
            common.fail("PWM tick_hz is not an exact maintained timer divider")
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000), *LIMITS["spi_hz"],
                       "SPI max_hz")
    if kind == "adc":
        if selection['mode'] == 'injected-shot' and len(selection['channels']) > 4:
            common.fail('Injected ADC requires at most four ranks')
        internal = ADC_INTERNAL_CHANNELS.get(route["controller"], ())
        if any(channel in internal and sample != 7 for channel, sample in
               zip(selection["channels"], selection["sample_times"])):
            common.fail("Internal ADC sources require the reviewed 480-cycle acquisition")


def mode_implementation(kind, mode):
    implementation = _MODE_IMPLEMENTATIONS.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32f4/{kind}/{mode}")
    return implementation


def validate_dma(route, mode, normalize):
    if route['kind'] == 'dac':
        from .hardware import DAC_DMA_ROUTES
        mask = sum(1 << (DAC_PIN_CHANNELS[("DAC", pin["pin"])] - 1)
                   for pin in route['pins'])
        expected = DAC_DMA_ROUTES[mask]
    else:
        expected = _FIXED_DMA.get((route["controller"], mode))
    common.validate_dma(route, mode,
                        expected,
                        'stm32f4', normalize, 'TIM3')


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
    spi_transaction=True, spi_dma=(("SPI1", "SPI2", "SPI3"), "dma"),
    spi_lsb_first=True, i2c_ten_bit=True)
TIMEBASE_ALARM = True


def validate_mode_context(selection, route, *, backend, clock_profile):
    """Selected policies may narrow the ordinary family RUN contract."""
    from .rtc import validate_context
    validate_context(selection, route, backend=backend,
                     clock_profile=clock_profile)
    if route['kind'] == 'external_bus' and (
            backend != 'baremetal' or clock_profile != 'hse8-pll168'):
        common.fail('FSMC requires exact baremetal hse8-pll168 RUN ownership')


mode_fields = common.mode_fields
validate_controller_set = common.validate_controller_set
validate_endpoint_clock = common.validate_endpoint_clock
configuration_header = common.configuration_header
