"""Source-reviewed H743 GPIO/USART byte-mode capability contract."""
from types import MappingProxyType
from .. import common


CPU_ABI = MappingProxyType({
    "arch": "cortex-m7", "fpu": "fpv5-d16", "float_abi": "hard"})
ENUM_ABI = "short-enums"
IRQ_PRIORITY_BITS = 4
EXTERNAL_IRQ_COUNT = 150
DWT_CYCCNT = True
FREERTOS_PORT = "GCC/ARM_CM7/r0p1"
CPU_FEATURES = MappingProxyType({
    "dwt_cyccnt": True, "mpu_version": 7, "mpu_regions": 16,
    "icache_line_bytes": 32, "dcache_line_bytes": 32, "security": "single",
    "sau": False, "mve": "none", "dsp": True})
PREFIX = "stm32h743"
FLASH_PROGRAM_UNIT = 32
FLASH_INPUT_ALIGNMENT = 1
ADC_PIN_CHANNELS = MappingProxyType({("ADC1", "PA3"): 15})
MODEL = False
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
DMA_VECTOR = "Stream"
DMA_TOPOLOGY = (frozenset((1,)), range(2), range(45, 47))
DMA_MEMORY_ENGINES = ("DMA1", "DMA2", "BDMA", "MDMA")
CONDITIONAL_PACKAGE_PADS = ("PC2_C", "PC3_C")
CLOCK_PROFILES = MappingProxyType({**{
    f"hsi{mhz}-fixed-cache-{cache}": MappingProxyType({
        "source": "hsi", "input_hz": 64000000, "core_hz": mhz * 1000000,
        "apb1_hz": mhz * 1000000, "apb2_hz": mhz * 1000000})
    for mhz in (32, 64) for cache in ("off", "on")},
    "hsi64-pll384-cache-off": MappingProxyType({
        "source": "hsi", "input_hz": 64000000,
        "core_hz": 64000000 * 6,
        "apb1_hz": 64000000 * 6 // 4,
        "apb2_hz": 64000000 * 6 // 4})})
LIMITS = MappingProxyType({"uart_baud": (977, 1000000), "spi_hz": (250000, 16000000), "pwm_period": 65535, "timer_hz": 64000000})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("uart", "dma-tx"): "uart_dma",
    ("uart", "rx-dma-idle"): "uart_receive",
    ("spi", "short-poll"): "spi", ("pwm", "fixed-pwm"): "pwm",
    ("exti", "edge-event"): "exti",
    ("watchdog", "independent"): "watchdog",
    ("i2c", "poll-messages"): "i2c",
    ("adc", "single-shot"): "adc", ("flash", "poll-word"): "flash"})


def cache_mask(profile):
    """One exact selected profile establishes compile-time cache ownership."""
    if profile not in CLOCK_PROFILES:
        common.fail(f"Unmaintained H743 clock/cache profile: {profile}")
    return 3 if profile.endswith("-cache-on") else 0


def mode_implementation(kind, mode):
    implementation = _MODES.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32h743/{kind}/{mode}")
    return implementation


def validate_selection(selection, route):
    """Accept only the actual fixed provider and reviewed route, not SKU potential."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "uart":
        expected = (("PD8", "tx", 7), ("PD9", "rx", 7))
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if route["controller"] != "USART3" or actual != expected:
            common.fail("Only reviewed USART3 PD8/PD9 AF7 is maintained")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "USART baud")
        divisor = (64000000 + baud // 2) // baud
        if not 16 <= divisor <= 0xffff:
            common.fail("USART baud differs from the actual PRESC1 divider")
        profile = "blocks" if selection["mode"] == "rx-dma-idle" else "bytes"
        if selection["rx_profile"] != profile:
            common.fail(f"H743 selected USART provider requires {profile} reception")
        if type(selection.get("irq_priority")) is not int or selection["irq_priority"] != 5:
            common.fail("H743 USART provider implements IRQ priority5 only")
    elif kind == "gpio":
        permitted = {"GPIOB": {"PB0"}, "GPIOE": {"PE11"}}
        pins = route["pins"]
        names = {pin["pin"] for pin in pins}
        if (not pins or len(names) != len(pins) or
                not names <= permitted.get(route["controller"], set()) or
                any(pin["af"] != 0 for pin in pins)):
            common.fail("H743 GPIO requires an exact reviewed package route")
        expected = sum(1 << int(pin[2:]) for pin in names)
        mask = common.integer(selection.get("mask", expected), 1, 0xffff, "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff, "GPIO initial")
        if mask != expected or initial & ~mask:
            common.fail("GPIO mask/initial differs from the selected route")
    elif kind == "spi":
        expected = (("PE12", "sck", 5), ("PE13", "miso", 5), ("PE14", "mosi", 5))
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if route["controller"] != "SPI4" or actual != expected:
            common.fail("H743 SPI4 requires exact PE12/13/14 AF5")
        common.integer(selection.get("max_hz"), *LIMITS["spi_hz"], "SPI rate")
    elif kind == "pwm":
        expected = (("PA6", "ch1", 2),)
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if route["controller"] != "TIM3" or actual != expected:
            common.fail("H743 PWM requires TIM3 CH1 PA6 AF2")
        tick = common.integer(selection.get("tick_hz"), 1, 32000000, "PWM tick")
        if 32000000 % tick or 64000000 // tick > 65536:
            common.fail("PWM tick must divide both selected fixed timer clocks")
        period = common.integer(selection.get("period_ticks"), 1, 65535, "PWM period")
        common.integer(selection.get("duty_ticks"), 0, period, "PWM duty")
    elif kind == "exti":
        controller = route["controller"]
        if controller not in {"EXTI0", "EXTI1"}:
            common.fail("H743 maintains exact independent EXTI0/1")
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if actual != (("PA" + controller[-1], "input", 0),):
            common.fail("H743 EXTI requires its exact PA0/PA1 input")
        if selection.get("edge") not in {"rising", "falling", "both"}:
            common.fail("H743 EXTI requires explicit raw edge")
        common.integer(selection.get("event_capacity"), 1, 4096, "EXTI capacity")
        common.integer(selection.get("irq_priority"), 0, 15, "EXTI priority")
    elif kind == "i2c":
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if route["controller"] != "I2C1" or actual != (("PB8", "scl", 4), ("PB9", "sda", 4)) or selection.get("max_hz", 100000) != 100000:
            common.fail("H743 I2C1 requires PB8/PB9 AF4 fixed100kHz")
    elif kind == "adc":
        actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
        if route["controller"] != "ADC1" or actual != (("PA3", "analog", 0),) or tuple(selection.get("channels", ())) != (15,) or tuple(selection.get("sample_times", ())) != (7,):
            common.fail("H743 ADC1 requires PA3 INP15 and sample code7")
    elif kind == "flash":
        if route["controller"] != "FLASH" or route["pins"]:
            common.fail("H743 Flash requires exact internal two-bank2MiB")
    elif kind == "watchdog":
        if route["controller"] != "IWDG1" or route["pins"]:
            common.fail("H743 watchdog requires internal IWDG1")


def validate_dma(route, mode, normalize):
    expected = ({"dma-tx": ("DMA1:stream0:channel46",),
                 "rx-dma-idle": ("DMA1:stream1:channel45",)}.get(mode)
                if route["controller"] == "USART3" else None)
    common.validate_dma(route, mode, expected, "stm32h743", normalize)


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


# Explicit static imports; importing this package performs no filesystem I/O.
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation, operations,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
