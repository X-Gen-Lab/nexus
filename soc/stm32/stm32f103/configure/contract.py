"""Exact medium-density F103RB facts and selected device capabilities."""
from types import MappingProxyType
from tools.configure.providers import common, descriptor

CPU_ABI = MappingProxyType({
    "arch": "cortex-m3", "fpu": "none", "float_abi": "soft"})
ENUM_ABI = "short-enums"
IRQ_PRIORITY_BITS = 4
EXTERNAL_IRQ_COUNT = 43
DWT_CYCCNT = True
FREERTOS_PORT = "GCC/ARM_CM3"
CPU_FEATURES = MappingProxyType({
    "dwt_cyccnt": True, "mpu_version": 0, "mpu_regions": 0,
    "icache_line_bytes": 0, "dcache_line_bytes": 0, "security": "single",
    "sau": False, "mve": "none", "dsp": False})
PREFIX = "stm32f103"
FLASH_PROGRAM_UNIT = 2
FLASH_INPUT_ALIGNMENT = 1
MODEL = False
FACT_REFERENCES = descriptor.maintained("stm32f103").references
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
DMA_VECTOR = "Channel"
DMA_TOPOLOGY = (frozenset((1,)), range(1, 8), range(1))
CLOCK_PROFILES = MappingProxyType({
    "hsi8-fixed": MappingProxyType({
        "source": "hsi", "input_hz": 8000000, "core_hz": 8000000,
        "apb1_hz": 8000000, "apb2_hz": 8000000})})
LIMITS = MappingProxyType({"uart_baud": (123, 500000),
                           "i2c_hz": (80000, 80000),
                           "spi_hz": (31250, 4000000), "pwm_period": 65535})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart",
    ("uart", "dma-tx"): "uart_dma",
    ("uart", "dma-duplex"): "uart_dma",
    ("i2c", "irq-messages"): "i2c_async",
    ("watchdog", "independent"): "watchdog",
    ("exti", "edge-event"): "exti",
    ("spi", "short-poll"): "spi", ("flash", "poll-word"): "flash",
    ("adc", "single-shot"): "adc", ("pwm", "fixed-pwm"): "pwm"})


def mode_implementation(kind, mode):
    implementation = _MODES.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32f103/{kind}/{mode}")
    return implementation


def validate_selection(selection, route):
    """Reject other densities, mappings, modes and electrical choices."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    actual = tuple((pin["pin"], pin["function"], pin["af"])
                   for pin in route["pins"])
    if kind == "uart":
        if (route["controller"] != "USART2" or
                actual != (("PA2", "tx", 0), ("PA3", "rx", 0))):
            common.fail("F103 requires reviewed USART2 PA2/PA3 default route")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "F103 USART baud")
        divisor = (8000000 + baud // 2) // baud
        if not 16 <= divisor <= 0xffff:
            common.fail("F103 USART baud exceeds oversample16 divisor")
        if selection["rx_profile"] != ("blocks" if selection["mode"] == "dma-duplex" else "bytes"):
            common.fail("Legacy USART mode requires its exact byte/block profile")
        if (type(selection.get("irq_priority")) is not int or
                selection["irq_priority"] != 5):
            common.fail("F103 USART2 provider implements IRQ priority5 only")
    elif kind == "i2c":
        if (route["controller"] != "I2C1" or actual != (
                ("PB6", "scl", 0), ("PB7", "sda", 0)) or
                selection.get("max_hz") != 80000 or
                type(selection.get("irq_priority")) is not int or
                selection["irq_priority"] != 0 or selection.get("calls_os", False)):
            common.fail("Legacy I2C requires PB6/PB7, nominal80kHz, IRQ0 and no kernel calls")
    elif kind == "gpio":
        if (route["controller"] != "GPIOA" or len(actual) != 1 or
                actual[0] not in {("PA4", "output", 0), ("PA5", "output", 0)}):
            common.fail("F103 GPIO requires reviewed GPIOA PA4/PA5")
        expected = 1 << int(actual[0][0][2:])
        mask = common.integer(selection.get("mask", expected), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != expected or initial & ~mask:
            common.fail("F103 GPIO mask/initial differs from exact route")
    elif kind == "watchdog":
        if route["controller"] != "IWDG" or actual:
            common.fail("F103 watchdog requires the exact internal IWDG route")
    elif kind == "exti":
        controller = route["controller"]
        if controller not in {"EXTI0", "EXTI1"}:
            common.fail("F103 only maintains independent EXTI0/1 vectors")
        expected = (("PA" + controller[-1], "input", 0),)
        if actual != expected:
            common.fail("F103 EXTI requires exact PA0/PA1 input route")
        if (type(selection.get("edge")) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            common.fail("F103 EXTI requires explicit raw edge selection")
        common.integer(selection.get("event_capacity"), 1, 4096,
                       "F103 EXTI capacity")
        common.integer(selection.get("irq_priority"), 0, 15,
                       "F103 EXTI priority")
    elif kind == "spi":
        if (route["controller"] != "SPI1" or actual != (
                ("PA5", "sck", 0), ("PA6", "miso", 0), ("PA7", "mosi", 0))):
            common.fail("F103 SPI requires exact default PA5/PA6/PA7 route")
        common.integer(selection.get("max_hz"), *LIMITS["spi_hz"],
                       "F103 SPI maximum frequency")
    elif kind == "flash":
        if route["controller"] != "FLASH" or actual:
            common.fail("F103 requires exact full physical Flash")
    elif kind == "adc":
        if (route["controller"] != "ADC1" or actual != (("PA0", "analog", 0),)
                or tuple(selection.get("channels", ())) != (0,)
                or tuple(selection.get("sample_times", ())) != (7,)):
            common.fail("F103 requires PA0 channel0 sample239.5cycles")
        common.integer(selection.get("reference_mv"), 2700, 3600,
                       "F103 nominal ADC reference")
    elif kind == "pwm":
        if route["controller"] != "TIM3" or actual != (("PA6", "ch1", 0),):
            common.fail("F103 requires default TIM3 CH1 PA6")
        tick = common.integer(selection.get("tick_hz"), 1, 8000000,
                              "F103 PWM tick")
        if 8000000 % tick or 8000000 // tick > 65536:
            common.fail("F103 PWM requires exact 8MHz divisor")
        period = common.integer(selection.get("period_ticks"), 1, 65535,
                                 "F103 PWM period")
        common.integer(selection.get("duty_ticks"), 0, period, "F103 PWM duty")


def validate_dma(route, mode, normalize):
    expected = {"dma-tx": ("DMA1:stream7:channel0",),
                "dma-duplex": ("DMA1:stream7:channel0", "DMA1:stream6:channel0")}.get(mode)
    common.validate_dma(route, mode, expected, "stm32f103", normalize)


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
