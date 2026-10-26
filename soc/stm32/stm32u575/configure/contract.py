"""Source-reviewed U575 GPIO/USART byte-mode and inactive IWDG contract."""
from types import MappingProxyType
import re
from tools.configure.providers import common


CPU_ABI = MappingProxyType({
    "arch": "cortex-m33", "fpu": "fpv5-sp-d16", "float_abi": "hard"})
ENUM_ABI = "short-enums"
FLASH_PROGRAM_UNIT = 16
FLASH_INPUT_ALIGNMENT = 1
IRQ_PRIORITY_BITS = 4
EXTERNAL_IRQ_COUNT = 126
DWT_CYCCNT = False
FREERTOS_PORT = "GCC/ARM_CM33_NTZ/non_secure"
CPU_FEATURES = MappingProxyType({
    "dwt_cyccnt": False, "mpu_version": 8, "mpu_regions": 8,
    "icache_line_bytes": 0, "dcache_line_bytes": 0, "security": "single",
    "sau": False, "mve": "none", "dsp": True})
PREFIX = "stm32u575"
MODEL = False
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
# Stream-style helpers remain unavailable; GPDMA channel facts are explicit.
DMA_VECTOR = None
DMA_TOPOLOGY = (frozenset(), range(0), range(0))
DMA_MEMORY_ENGINES = ("GPDMA1", "LPDMA1")
CLOCK_PROFILES = MappingProxyType({
    "msis4": MappingProxyType({
        "source": "msis", "input_hz": 4000000, "core_hz": 4000000,
        "apb1_hz": 4000000, "apb2_hz": 4000000})})
LIMITS = MappingProxyType({"uart_baud": (62, 250000),
                           "spi_hz": (15625, 2000000),
                           "pwm_period": 65535, "timer_hz": 4000000})
ADC_PIN_CHANNELS = MappingProxyType({("ADC4", "PC0"): 1})
_MODES = MappingProxyType({
    ("lptim", "irq-counter16-oneshot"): "lptim",
    ("rtc", "retained-calendar-alarm"): "rtc",
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart",
    ("uart", "irq-fifo-bytes"): "uart_fifo",
    ("uart", "irq-fifo-rtscts-bytes"): "uart_rtscts",
    ("uart", "dma-tx"): "uart_gpdma",
    ("uart", "dma-chain-tx"): "uart_chain",
    ("watchdog", "independent"): "watchdog",
    ("spi", "short-poll"): "spi", ("i2c", "poll-messages"): "i2c",
    ("adc", "single-shot"): "adc", ("pwm", "fixed-pwm"): "pwm",
    ("exti", "edge-event"): "exti", ("flash", "poll-word"): "flash"})
_GPIO_MASKS = MappingProxyType({
    "GPIOA": 0xffff, "GPIOB": 0xefff, "GPIOC": 0xffcf,
    "GPIOD": 0xffff, "GPIOE": 0xffff, "GPIOF": 0xffff,
    "GPIOG": 0xf7ff, "GPIOH": 0x000b})
_SERIAL_PINS = MappingProxyType({
    "USART1": (("PA9", "tx", 7), ("PA10", "rx", 7)),
    "USART2": (("PA2", "tx", 7), ("PA3", "rx", 7))})


def mode_implementation(kind, mode):
    implementation = _MODES.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32u575/{kind}/{mode}")
    return implementation


def validate_selection(selection, route):
    """Accept only the actual fixed provider and reviewed route, not SKU potential."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "rtc":
        if route["controller"] != "RTC0" or route["pins"]:
            common.fail("U575 retained RTC requires exact pinless RTC0")
        return
    if kind == "lptim":
        if (route["controller"] != "LPTIM2" or route["pins"] or
                selection["mode"] != "irq-counter16-oneshot"):
            common.fail(
                "U575 LPTIM requires the exact internal LPTIM2 IRQ one-shot")
        common.integer(selection.get("tick_hz"), 1000000, 1000000,
                       "U575 LPTIM RUN tick")
        common.integer(selection.get("irq_priority"), 5, 5,
                       "U575 LPTIM IRQ priority")
        if selection.get("calls_os", False) is not False:
            common.fail("U575 LPTIM IRQ never calls a scheduler")
        return
    if kind == "uart":
        expected = _SERIAL_PINS.get(route["controller"])
        if selection["mode"] == "irq-fifo-rtscts-bytes":
            expected = ((("PA9", "tx", 7), ("PA10", "rx", 7),
                         ("PA11", "cts", 7), ("PA12", "rts", 7))
                        if route["controller"] == "USART1" else None)
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if expected is None or actual != expected:
            common.fail("USART requires the exact controller and ordered AF7 pads for its selected mode")
        if selection["mode"] in {"dma-tx", "dma-chain-tx"} and (
                route["controller"] != "USART1" or
                selection.get("frame") not in (None, {
                    "data_bits": 8, "parity": "none", "stop_bits": 1,
                    "flow": "none"})):
            common.fail("U575 finite GPDMA TX requires exact USART1 8N1")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "USART baud")
        divisor = (4000000 + baud // 2) // baud
        if not 16 <= divisor <= 0xffff:
            common.fail("USART baud differs from the actual PRESC1 oversample16 divider")
        if selection["rx_profile"] != "bytes":
            common.fail("U575 USART provider requires byte reception")
        if type(selection.get("irq_priority")) is not int or selection["irq_priority"] != 5:
            common.fail("U575 USART provider implements IRQ priority5 only")
    elif kind == "gpio":
        pins = route["pins"]
        available = _GPIO_MASKS.get(route["controller"], 0)
        route_mask = 0
        for pin in pins:
            name = pin["pin"]
            if (len(name) < 3 or name[:2] != "P" + route["controller"][-1]
                    or not name[2:].isdigit() or int(name[2:]) > 15
                    or pin["af"] != 0
                    or pin["function"] not in {"input", "output"}):
                common.fail("GPIO route requires exact package pins and GPIO function")
            route_mask |= 1 << int(name[2:])
        if not route_mask or route_mask & ~available:
            common.fail("GPIO route exceeds the exact U575ZIQ pin inventory")
        mask = common.integer(selection.get("mask", route_mask), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != route_mask or initial & ~mask:
            common.fail("GPIO mask/initial differs from the reviewed route")
    elif kind == "watchdog":
        if route["controller"] != "IWDG" or route["pins"]:
            common.fail("Only the pinless independent IWDG is maintained")
    else:
        reviewed = {
            "spi": ("SPI1", (("PA5", "sck", 5), ("PA6", "miso", 5),
                             ("PA7", "mosi", 5))),
            "i2c": ("I2C1", (("PB8", "scl", 4), ("PB9", "sda", 4))),
            "pwm": ("TIM3", (("PA6", "ch1", 2),)),
            "adc": ("ADC4", (("PC0", "analog", 0),)),
            "flash": ("FLASH", ())}
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if kind == "exti":
            controller = route["controller"]
            if controller not in {"EXTI0", "EXTI1"} or actual != (
                    ("PA" + controller[-1], "input", 0),):
                common.fail("U575 EXTI requires exact PA0/PA1 independent vector")
            if selection.get("edge") not in {"rising", "falling", "both"}:
                common.fail("U575 EXTI requires explicit raw edge selection")
            common.integer(selection.get("event_capacity"), 1, 4096,
                           "U575 event capacity")
            common.integer(selection.get("irq_priority"), 0, 15,
                           "U575 EXTI IRQ priority")
        elif (route["controller"], actual) != reviewed[kind]:
            common.fail("U575 selected peripheral requires exact reviewed route")
        if kind == "spi":
            common.integer(selection.get("max_hz"), *LIMITS["spi_hz"],
                           "U575 SPI frequency")
        elif kind == "i2c":
            common.integer(selection.get("max_hz", 100000), 100000, 100000,
                           "U575 I2C maximum frequency")
        elif kind == "adc":
            channels = selection.get("channels")
            samples = selection.get("sample_times")
            if (not isinstance(channels, (list, tuple)) or
                    not isinstance(samples, (list, tuple)) or
                    tuple(channels) != (1,) or tuple(samples) != (7,) or
                    any(type(value) is not int for value in (*channels, *samples))):
                common.fail("U575 ADC4 supports PC0/channel1 and samplecode7 only")
            common.integer(selection.get("reference_mv"), 1000, 3600,
                           "U575 nominal reference")
        elif kind == "pwm":
            tick = common.integer(selection.get("tick_hz"), 1, 4000000,
                                  "U575 PWM tick")
            if 4000000 % tick or 4000000 // tick > 65536:
                common.fail("U575 PWM tick must divide the4MHz timer clock")
            period = common.integer(selection.get("period_ticks"), 1, 65535,
                                    "U575 PWM period")
            common.integer(selection.get("duty_ticks"), 0, period,
                           "U575 PWM duty")


def validate_dma(route, mode, normalize):
    expected = (("GPDMA1:channel0:request25",) if
                route["kind"] == "uart" and route["controller"] == "USART1"
                and mode in {"dma-tx", "dma-chain-tx"} else None)
    common.validate_dma(route, mode, expected, "stm32u575", normalize)


def validate_dma_resource(resource):
    if (type(resource) is not str or re.fullmatch(
            r"dma:GPDMA1:channel0(?::request25)?", resource) is None):
        common.fail("U575 DMA requires reviewed GPDMA1 channel0/request25")
    return "GPDMA1", 0, 25 if resource.endswith(":request25") else None


def dma_physical_key(resource):
    """Canonical channel ownership excludes the independently checked request."""
    validate_dma_resource(resource)
    return "dma:GPDMA1:channel0"


def dma_chain_plan(item):
    """Symbolic private storage facts; actual linked addresses are checked cold."""
    if item.get("mode") != "dma-chain-tx":
        return None
    if (item.get("kind") != "uart" or item.get("controller") != "USART1" or
            tuple(item.get("dma", ())) != ("GPDMA1:channel0:request25",)):
        common.fail("U575 chain requires exact USART1 GPDMA1 channel0/request25")
    return {"owner": item["id"], "engine": "GPDMA1", "lane": 0,
            "node_capacity": 2, "node_bytes": 12, "alignment": 32,
            "window_bytes": 65536, "region": "SRAM1", "block_bytes": 65535,
            "maximum_bytes": 196605, "finite": True}


def dma_irq(dma):
    if validate_dma_resource("dma:" + dma)[2] != 25:
        common.fail("U575 GPDMA route requires explicit USART1 TX request25")
    return "irq:GPDMA1_Channel0"


def shares_irq(kind, key):
    del kind, key
    return False


def builtin_cs(controller):
    del controller
    return False


# Explicit static imports; importing this package performs no filesystem I/O.
# Explicit software capabilities do not authorize an unreviewed route.
ADC_GROUP = None
PWM_SHARED = None
ENDPOINT_POLICY = common.EndpointPolicy(i2c_long=True)
TIMEBASE_ALARM = False
mode_fields = common.mode_fields
validate_controller_set = common.validate_controller_set
validate_endpoint_clock = common.validate_endpoint_clock
configuration_header = common.configuration_header


def validate_mode_context(selection, route, *, backend, clock_profile):
    mode = selection["mode"]
    if route["kind"] in {"lptim", "rtc"} and (
            backend != "baremetal" or clock_profile != "msis4"):
        common.fail(f"U575 {route['kind'].upper()} requires baremetal/fixed MSIS4")
    if mode in {"irq-fifo-bytes", "irq-fifo-rtscts-bytes"} and (
            clock_profile != "msis4"):
        common.fail("FIFO UART requires its reviewed fixed clock profile")
    if mode == "irq-fifo-rtscts-bytes" and backend != "baremetal":
        common.fail("RTSCTS FIFO UART requires its reviewed baremetal owner")


def validate_user_io_context(assembly, part):
    if (part != "STM32U575ZIT6Q" or assembly["clock_profile"] != "msis4" or
            assembly["layout"] is not None or "power" in assembly):
        common.fail("GPIO user I/O requires the exact U575 TZEN0 MSIS4 reference")
