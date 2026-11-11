"""Maintained STM32F4 capability and static assembly contract."""
from tools.configure.stream_profiles import StreamGeometry
from tools.configure.uart_formats import UartFormatPolicy
from tools.configure.calls import Symbol
from types import MappingProxyType

from tools.configure.providers import common, descriptor
from .hardware import (ADC_PIN_CHANNELS, ADC_INTERNAL_CHANNELS,
                       DAC_PIN_CHANNELS, PWM_PIN_CHANNELS, PWM_TIMERS, ADC_TRIGGERS,
                       FIXED_DMA as _FIXED_DMA)
from tools.configure.adc import RESOLUTION_BITS
from . import waveform

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
_BASE_CONTROLLER_INTERFACES = MappingProxyType({
    **{name: MappingProxyType({'i2c_target': ('target-irq', 'smbus-device')})
       for name in ('I2C1', 'I2C2', 'I2C3')},
    'OTG_FS': MappingProxyType({'usb_host': ('host-fs',)}),
    'OTG_HS': MappingProxyType({'usb_device': ('device-hs-fs', 'device-hs-ulpi',
                                                  'otg-hs-fs', 'otg-hs-ulpi')}),
    'TIM1': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    'TIM3': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    'TIM4': MappingProxyType({'pwm': ('fixed-pwm',), 'encoder': ('quadrature',)}),
    'TIM5': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    'TIM8': MappingProxyType({'encoder': ('quadrature',),
                              'timer_capture': ('edge-capture',)}),
    **{name: MappingProxyType({'timer_capture': ('edge-capture',)})
       for name in ('TIM9', 'TIM10', 'TIM11', 'TIM12', 'TIM13', 'TIM14')},
    'ETH': MappingProxyType({
        'eth_frames': ('rmii-vlan-sg-ptp', 'mii-vlan-sg-ptp')}),
    'FSMC': MappingProxyType({
        'memory_window': ('cpu-window', 'startup-memory', 'psram-window'),
        'flash_async': ('nor-cfi',), 'nand': ('raw-page',),
        'parallel16': ('8080-16',)})})
CONTROLLER_INTERFACES = MappingProxyType({
    name: MappingProxyType({**_BASE_CONTROLLER_INTERFACES.get(name, {}),
                            **waveform.controller_interfaces().get(name, {})})
    for name in sorted(set(_BASE_CONTROLLER_INTERFACES) |
                       set(waveform.controller_interfaces()))})
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
    ('adc_group', 'regular-sequence-dma'): 'adc_group',
    ('adc_group', 'injected-sequence'): 'adc_group',
    **{('capture', mode): 'capture'
       for mode in ('snapshot-raw', 'snapshot-jpeg', 'raw-blocks')},
    **{('audio', role + '-' + direction): 'audio'
       for role in ('master', 'slave') for direction in ('tx', 'rx', 'duplex')},
    **{('audio', role + '-' + direction + '-blocks'): 'audio_blocks'
       for role in ('master', 'slave') for direction in ('tx', 'rx', 'duplex')},
    ('pwm_group', 'coherent-pwm'): 'pwm_group',
    ('timer_pulse', 'one-pulse'): 'timer_pulse',
    ('timer_sequence', 'dma-burst'): 'timer_sequence',
    ('encoder', 'quadrature'): 'encoder',
    ('crc', 'crc32-words'): 'crc',
    ('entropy', 'rng-words'): 'entropy',
    ('dac', 'direct'): 'dac',
    ('dac', 'triggered'): 'dac',
    ('dac', 'noise'): 'dac',
    ('dac', 'triangle'): 'dac',
    ('dac', 'trigger-dma'): 'dac_dma',
    ('dac', 'trigger-dma-circular'): 'dac_dma',
    ('memory_window', 'startup-memory'): 'fsmc',
    ('external_bus', 'async16'): 'fsmc',
    ('external_bus', 'external-memory'): 'fsmc',
    ('memory_window', 'psram-window'): 'fsmc',
    ('flash_async', 'nor-cfi'): 'fsmc',
    ('nand', 'raw-page'): 'fsmc',
    ('memory_window', 'cpu-window'): 'fsmc',
    ('parallel16', '8080-16'): 'fsmc',
    ('rtc', 'adopt-retained'): 'rtc',
    ('rtc', 'provision-empty'): 'rtc',
    ('block', 'sdio-poll'): 'sdio',
    ('block', 'sdio-irq'): 'sdio',
    ('block', 'sdio-dma'): 'sdio',
    ('timer_capture', 'edge-capture'): 'timer_capture',
    ('advanced_pwm', 'complementary-pwm'): 'advanced_pwm',
    ('eth', 'raw-rmii'): 'eth',
    ('eth', 'raw-mii'): 'eth',
    ('eth_frames', 'rmii-vlan-sg-ptp'): 'eth_frames',
    ('eth_frames', 'mii-vlan-sg-ptp'): 'eth_frames',
    ('usb_device', 'device-fs'): 'usb_device',
    ('usb_device', 'otg-fs'): 'usb_device',
    ('usb_device', 'otg-hs-fs'): 'usb_device',
    ('usb_device', 'otg-hs-ulpi'): 'usb_device',
    ('usb_device', 'device-hs-fs'): 'usb_device',
    ('usb_device', 'device-hs-ulpi'): 'usb_device',
    ('usb_host', 'host-fs'): 'usb_host',
    ('usb_host', 'host-hs-fs'): 'usb_host',
    ('usb_host', 'host-hs-ulpi'): 'usb_host',
    ('can', 'classic-irq'): 'can',
    ('gpio', 'input'): 'gpio',
    ('gpio', 'output'): 'gpio',
    ('uart', 'irq-byte-event'): 'uart',
    ('uart', 'irq-wire'): 'uart',
    ('uart', 'irq-words'): 'uart_word',
    ('spi', 'short-poll'): 'spi',
    ('spi', 'irq-frames'): 'spi_frames',
    ('i2c_target', 'target-irq'): 'i2c_target',
    ('i2c_target', 'smbus-device'): 'i2c_target',
    ('i2c', 'poll-messages'): 'i2c',
    ('i2c', 'irq-messages'): 'i2c_async',
    ('i2c', 'dma-messages'): 'i2c_async',
    ('flash', 'poll-word'): 'flash',
    ('watchdog', 'independent'): 'watchdog',
    ('watchdog', 'window'): 'window_watchdog',
    ('exti', 'edge-event'): 'exti',
    ('exti', 'hardware-event'): 'exti_hardware',
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
    if kind in {"capture", "audio"}:
        from .capture_audio import validate
        validate(selection, route)
    mode_implementation(kind, selection["mode"])
    if kind == 'exti' and selection['mode'] in {'pvd-level', 'pvd-events'}:
        if route['controller'] != 'EXTI16' or route['pins']:
            common.fail('PVD requires its pinless EXTI16 controller')
    if kind in waveform.KINDS:
        waveform.validate(selection, route)
    if kind == "watchdog" and selection["mode"] == "window":
        if route["controller"] != "WWDG":
            common.fail("Window watchdog requires its exact WWDG controller")
        common.integer(selection.get("window_ticks", 1), 1, 62, "WWDG window_ticks")
    if kind == "flash" and type(selection.get("maintenance", False)) is not bool:
        common.fail("Flash maintenance must be an explicit boolean")
    if kind == "adc_group":
        from .adc_group import validate
        validate(selection, route)
    elif kind == "rtc":
        from .rtc import validate
        validate(selection, route)
    elif kind == "i2c_target":
        from .i2c_target import validate
        validate(selection, route)
    elif kind == "block":
        from .block import validate
        validate(selection, route)
    elif kind == "encoder":
        from .encoder import validate
        validate(selection, route)
    elif kind == "adc" and common.mode_traits(kind, selection["mode"]).dma:
        if (route["controller"], selection["mode"]) not in _FIXED_DMA:
            common.fail("ADC DMA requires a reviewed converter DMA path")
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
    elif kind == "spi":
        common.integer(selection.get("max_hz", 1000000),
                       *( (1, 42000000) if selection["mode"] == "irq-frames"
                          else LIMITS["spi_hz"]), "SPI max_hz")
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
    if route['kind'] == 'block':
        expected = ("DMA2:stream3:channel4",) if mode == "sdio-dma" else ()
    elif route['kind'] == 'timer_sequence':
        engine, stream, channel = waveform.DMA[int(route['controller'][3:])]
        expected = (f'DMA{engine}:stream{stream}:channel{channel}',)
    elif route['kind'] == 'capture':
        stream = 7 if route['id'].endswith('-s7') else 1
        expected = (f'DMA2:stream{stream}:channel1',)
    elif route['kind'] == 'audio':
        from .capture_audio import dma_paths
        expected = tuple(f'DMA{engine}:stream{lane}:channel{channel}'
            for engine, lane, channel in dma_paths(route['controller'], mode))
    elif route['kind'] == 'dac':
        from .hardware import DAC_DMA_ROUTES
        mask = sum(1 << (DAC_PIN_CHANNELS[("DAC", pin["pin"])] - 1)
                   for pin in route['pins'])
        expected = DAC_DMA_ROUTES[mask]
    else:
        expected = _FIXED_DMA.get((route["controller"], mode))
    common.validate_dma(route, mode, expected, 'stm32f4')
    if route['kind'] == 'adc' and route.get('mode_resources', {}).get(mode):
        common.fail('ADC trigger resource belongs to the selected assembly, not the analog route')


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
from . import adc_group as ADC_GROUP
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
def validate_controller_set(selected, *, clock_profile):
    """Both audio cells share exactly one compatible PLLI2S tuple."""
    del clock_profile
    waveform.resolve_edges(selected)
    tuples = {(item["pll_n"], item["pll_r"]) for item in selected
              if item["kind"] == "audio" and item["mode"].startswith("master-")}
    if len(tuples) > 1:
        common.fail("Selected I2S controllers require one compatible PLLI2S tuple")

validate_endpoint_clock = common.validate_endpoint_clock
def configuration_header(result):
    from .clocks import header_lines
    return [*common.configuration_header(result), *header_lines(result)]
