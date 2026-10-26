"""Exact GD32F303RCT6 IRC8, legacy GPIO and USART0 IRQ-byte contract."""
from types import MappingProxyType
from tools.configure.providers import common, descriptor


PARTS = MappingProxyType({
    "GD32F303RCT6": common.PartPolicy("LQFP64"),
})
PUBLIC_FORMATS = frozenset(("uart", "spi", "i2c", "adc", "can",
                            "eth", "eth_frames"))

CPU_ABI = MappingProxyType({
    "arch": "cortex-m4", "fpu": "fpv4-sp-d16", "float_abi": "hard"})
ENUM_ABI = "short-enums"
IRQ_PRIORITY_BITS = 4
EXTERNAL_IRQ_COUNT = 60
DWT_CYCCNT = True
FREERTOS_PORT = "GCC/ARM_CM4F"
CPU_FEATURES = MappingProxyType({
    "dwt_cyccnt": True, "mpu_version": 7, "mpu_regions": 8,
    "icache_line_bytes": 0, "dcache_line_bytes": 0, "security": "single",
    "sau": False, "mve": "none", "dsp": True})
PREFIX = "gd32f303"
FLASH_GEOMETRY = ("g_nx_gd32f303_flash_geometry", "0u")
FLASH_PROGRAM_UNIT = 2
FLASH_INPUT_ALIGNMENT = 1
MODEL = False
FACT_REFERENCES = descriptor.maintained(PREFIX).references
SOURCE_IMPORT_LOCK = descriptor.maintained(PREFIX).import_lock
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
DMA_VECTOR = "Channel"
DMA_TOPOLOGY = (frozenset((0,)), range(7), range(1))
CLOCK_PROFILES = MappingProxyType({
    "irc8": MappingProxyType({
        "source": "hsi", "input_hz": 8000000, "core_hz": 8000000,
        "apb1_hz": 8000000, "apb2_hz": 8000000})})
LIMITS = MappingProxyType({"uart_baud": (123, 500000),
                           "i2c_hz": (80000, 80000),
                           "spi_hz": (31250, 4000000), "pwm_period": 65535})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("spi", "short-poll"): "spi",
    ("uart", "dma-tx"): "uart_dma", ("uart", "dma-duplex"): "uart_dma",
    ("i2c", "irq-messages"): "i2c_async",
    ("watchdog", "independent"): "watchdog", ("flash", "poll-word"): "flash",
    ("adc", "single-shot"): "adc", ("pwm", "fixed-pwm"): "pwm",
    ("exti", "edge-event"): "exti"})


def mode_implementation(kind, mode):
    implementation = _MODES.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: gd32f303/{kind}/{mode}")
    return implementation


def validate_selection(selection, route):
    """Never infer F470 layout, AF multiplexers or unsupported SDK capability."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    actual = tuple((pin["pin"], pin["function"], pin["af"])
                   for pin in route["pins"])
    if kind == "uart":
        if (route["controller"] != "USART0" or actual != (
                ("PA9", "tx", 0), ("PA10", "rx", 0))):
            common.fail("GD303 requires default USART0 PA9/PA10, no remap")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "USART baud")
        divisor = (8000000 + baud // 2) // baud
        if not 16 <= divisor <= 0xffff:
            common.fail("GD303 baud exceeds actual oversample16 divider")
        if selection["rx_profile"] != ("blocks" if selection["mode"] == "dma-duplex" else "bytes"):
            common.fail("Legacy USART mode requires its exact byte/block profile")
        if (type(selection.get("irq_priority")) is not int or
                selection["irq_priority"] != 5):
            common.fail("GD303 USART implements IRQ priority5 only")
    elif kind == "i2c":
        if (route["controller"] != "I2C0" or actual != (
                ("PB6", "scl", 0), ("PB7", "sda", 0)) or
                selection.get("max_hz") != 80000 or
                type(selection.get("irq_priority")) is not int or
                selection["irq_priority"] != 0 or selection.get("calls_os", False)):
            common.fail("Legacy I2C requires PB6/PB7, nominal80kHz, IRQ0 and no kernel calls")
    elif kind == "gpio":
        if route["controller"] != "GPIOA" or actual not in {
                (("PA5", "output", 0),), (("PA4", "output", 0),)}:
            common.fail("GD303 requires an exact GPIOA PA4/PA5 silicon fixture")
        expected = 1 << int(actual[0][0][2:])
        mask = common.integer(selection.get("mask", expected), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != expected or initial & ~mask:
            common.fail("GPIO mask/initial differs from its reviewed route")
    elif kind == "spi":
        if route["controller"] != "SPI0" or actual != (
                ("PA5", "sck", 0), ("PA6", "miso", 0), ("PA7", "mosi", 0)):
            common.fail("GD303 SPI0 requires its default PA5/6/7 route")
        common.integer(selection.get("max_hz"), *LIMITS["spi_hz"],
                       "GD303 SPI rate")
    elif kind == "watchdog":
        if route["controller"] != "FWDGT" or route["pins"]:
            common.fail("GD303 watchdog requires internal FWDGT")
    elif kind == "exti":
        controller = route["controller"]
        if controller not in {"EXTI0", "EXTI1"} or actual != (
                ("PA" + controller[-1], "input", 0),):
            common.fail("GD303 requires exact independent PA0/PA1 EXTI")
        if (type(selection.get("edge")) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            common.fail("GD303 EXTI requires explicit raw edge selection")
        common.integer(selection.get("event_capacity"), 1, 4096,
                       "GD303 EXTI event capacity")
        common.integer(selection.get("irq_priority"), 0, 15,
                       "GD303 EXTI priority")
    elif kind == "flash":
        if route["controller"] != "FMC" or actual:
            common.fail("GD303 requires exact full physical FMC bank0")
    elif kind == "adc":
        if (route["controller"] != "ADC0" or actual != (("PA0", "analog", 0),)
                or tuple(selection.get("channels", ())) != (0,)
                or tuple(selection.get("sample_times", ())) != (7,)):
            common.fail("GD303 requires PA0 channel0 sample239.5cycles")
        common.integer(selection.get("reference_mv"), 2700, 3600,
                       "GD303 nominal ADC reference")
    elif kind == "pwm":
        if route["controller"] != "TIMER2" or actual != (("PA6", "ch0", 0),):
            common.fail("GD303 requires default TIMER2 CH0 PA6")
        tick = common.integer(selection.get("tick_hz"), 1, 8000000,
                              "GD303 PWM tick")
        if 8000000 % tick or 8000000 // tick > 65536:
            common.fail("GD303 PWM requires exact IRC8 divisor")
        period = common.integer(selection.get("period_ticks"), 1, 65535,
                                 "GD303 PWM period")
        common.integer(selection.get("duty_ticks"), 0, period, "GD303 PWM duty")


def validate_dma(route, mode, normalize):
    expected = {"dma-tx": ("DMA0:stream3:channel0",),
                "dma-duplex": ("DMA0:stream3:channel0", "DMA0:stream4:channel0")}.get(mode)
    common.validate_dma(route, mode, expected, "gd32f303", normalize)


def validate_dma_resource(resource):
    return common.validate_dma_resource(resource, DMA_TOPOLOGY)


def dma_irq(dma):
    return common.dma_irq(dma, DMA_VECTOR, DMA_TOPOLOGY)


def shares_irq(kind, key):
    del kind, key
    return False


def builtin_cs(controller):
    del controller
    return False


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
validate_user_io_context = common.validate_user_io_context
