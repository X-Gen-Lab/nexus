"""Source-reviewed G474 GPIO, IRQ UART, FIFO SPI, IWDG and EXTI contracts."""
from types import MappingProxyType
from .. import common, descriptor


CPU_ABI = MappingProxyType({
    "arch": "cortex-m4", "fpu": "fpv4-sp-d16", "float_abi": "hard"})
ENUM_ABI = "short-enums"
IRQ_PRIORITY_BITS = 4
EXTERNAL_IRQ_COUNT = 102
DWT_CYCCNT = True
FREERTOS_PORT = "GCC/ARM_CM4F"
CPU_FEATURES = MappingProxyType({
    "dwt_cyccnt": True, "mpu_version": 7, "mpu_regions": 8,
    "icache_line_bytes": 0, "dcache_line_bytes": 0, "security": "single",
    "sau": False, "mve": "none", "dsp": True})
PREFIX = "stm32g474"
MODEL = False
FACT_REFERENCES = descriptor.maintained("stm32g474").references
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
DMA_VECTOR = "Channel"
DMA_TOPOLOGY = (frozenset((1,)), range(1, 2), range(5, 6))
ADC_PIN_CHANNELS = MappingProxyType({("ADC1", "PA0"): 1})
FLASH_PROGRAM_UNIT = 8
FLASH_INPUT_ALIGNMENT = 1
CLOCK_PROFILES = MappingProxyType({
    "hse24-pll170": MappingProxyType({
        "hse_hz": 24000000, "core_hz": 170000000,
        "apb1_hz": 170000000, "apb2_hz": 170000000})})
LIMITS = MappingProxyType({"uart_baud": (2594, 1000000),
                           "spi_hz": (664063, 42500000),
                           "pwm_period": 65535, "timer_hz": 170000000})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("exti", "edge-event"): "exti",
    ("watchdog", "independent"): "watchdog",
    ("spi", "short-poll"): "spi",
    ("pwm", "fixed-pwm"): "pwm",
    ("adc", "single-shot"): "adc",
    ("adc", "trigger-dma"): "adc_stream",
    ("flash", "poll-word"): "flash",
    ("i2c", "poll-messages"): "i2c"})


def mode_implementation(kind, mode):
    implementation = _MODES.get((kind, mode))
    if implementation is None:
        common.fail(f"Unmaintained provider mode: stm32g474/{kind}/{mode}")
    return implementation


def validate_selection(selection, route):
    """Accept only the actual fixed provider and reviewed route, not SKU potential."""
    kind = route["kind"]
    mode_implementation(kind, selection["mode"])
    if kind == "uart":
        reviewed = {
            "LPUART1": (("PA2", "tx", 12), ("PA3", "rx", 12)),
            "USART1": (("PA9", "tx", 7), ("PA10", "rx", 7)),
            "USART2": (("PA2", "tx", 7), ("PA3", "rx", 7)),
            "USART3": (("PB10", "tx", 7), ("PB11", "rx", 7)),
            "UART4": (("PC10", "tx", 5), ("PC11", "rx", 5)),
            "UART5": (("PC12", "tx", 5), ("PD2", "rx", 5))}
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if actual != reviewed.get(route["controller"]):
            common.fail("G474 UART requires an exact reviewed controller route")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "UART baud")
        low_power = route["controller"] == "LPUART1"
        divisor = (170000000 * (16 if low_power else 1) + baud // 2) // baud
        if not (0x300 if low_power else 16) <= divisor <= (
                0xfffff if low_power else 0xffff):
            common.fail("UART baud differs from its selected hardware divider")
        if selection["rx_profile"] != "bytes":
            common.fail("G474 UART provider requires byte reception")
        if type(selection.get("irq_priority")) is not int or selection["irq_priority"] != 5:
            common.fail("G474 UART provider implements IRQ priority5 only")
    elif kind == "gpio":
        pins = route["pins"]
        controller = route["controller"]
        if controller not in {"GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOF", "GPIOG"} or not pins:
            common.fail("G474 GPIO requires a package port and explicit pins")
        expected = 0
        available = {"A": range(16), "B": range(16), "C": range(16),
                     "D": (2,), "F": (0, 1), "G": (10,)}
        bank = controller[-1]
        for pin in pins:
            name = pin["pin"]
            if (not name.startswith("P" + bank) or not name[2:].isdigit() or
                    int(name[2:]) not in available[bank] or pin["af"] != 0):
                common.fail("G474 GPIO route differs from the exact RE package")
            bit = 1 << int(name[2:])
            if expected & bit:
                common.fail("G474 GPIO route repeats a pin")
            expected |= bit
        mask = common.integer(selection.get("mask", expected), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != expected or initial & ~mask:
            common.fail("GPIO mask/initial differs from the reviewed route")
    elif kind == "spi":
        reviewed = {
            "SPI1": (("PA5", "sck", 5), ("PA6", "miso", 5), ("PA7", "mosi", 5)),
            "SPI2": (("PB13", "sck", 5), ("PB14", "miso", 5), ("PB15", "mosi", 5))}
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if actual != reviewed.get(route["controller"]):
            common.fail("G474 SPI requires its exact reviewed AF5 route")
        common.integer(selection.get("max_hz"), *LIMITS["spi_hz"],
                       "G474 SPI maximum frequency")
    elif kind == "pwm":
        expected = {"TIM3": "PA6", "TIM4": "PB6"}.get(route["controller"])
        if tuple((pin["pin"], pin["function"], pin["af"])
                 for pin in route["pins"]) != ((expected, "ch1", 2),):
            common.fail("G474 PWM requires exact TIM3/4 CH1 AF2 route")
        tick = common.integer(selection.get("tick_hz"), 1, 170000000, "PWM tick")
        if 170000000 % tick or 170000000 // tick > 65536:
            common.fail("PWM tick must exactly divide the 170MHz timer clock")
        period = common.integer(selection.get("period_ticks"), 1, 65535, "PWM period")
        common.integer(selection.get("duty_ticks"), 0, period, "PWM duty")
    elif kind == "adc":
        if (route["controller"] != "ADC1" or
                tuple((pin["pin"], pin["function"], pin["af"])
                      for pin in route["pins"]) != (("PA0", "analog", 0),) or
                tuple(selection["channels"]) != (1,) or
                tuple(selection["sample_times"]) != (0,)):
            common.fail("G474 ADC requires ADC1 PA0 channel1 sample code0")
        common.integer(selection["reference_mv"], 1620, 3600, "ADC reference")
        if selection["mode"] == "trigger-dma" and selection.get("irq_priority") != 5:
            common.fail("G474 ADC DMA requires fixed IRQ priority5")
    elif kind == "i2c":
        if (route["controller"] != "I2C1" or
                tuple((pin["pin"], pin["function"], pin["af"])
                      for pin in route["pins"]) !=
                (("PB8", "scl", 4), ("PB9", "sda", 4)) or
                selection.get("max_hz", 100000) != 100000):
            common.fail("G474 I2C1 requires PB8/PB9 AF4 and fixed100kHz")
    elif kind == "flash":
        if route["controller"] != "FLASH" or route["pins"]:
            common.fail("G474 physical Flash requires internal dual-bank512KiB")
    elif kind == "watchdog":
        if route["controller"] != "IWDG" or route["pins"]:
            common.fail("G474 independent watchdog requires internal IWDG")
    elif kind == "exti":
        controller = route["controller"]
        if controller not in {"EXTI0", "EXTI1"}:
            common.fail("Only independent G474 EXTI0/1 vectors are maintained")
        pin = "PA" + controller[-1]
        if tuple((item["pin"], item["function"], item["af"])
                 for item in route["pins"]) != ((pin, "input", 0),):
            common.fail("G474 EXTI requires its exact GPIOA input route")
        if (type(selection.get("edge")) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            common.fail("G474 EXTI requires an explicit raw edge selection")
        common.integer(selection.get("event_capacity"), 1, 4096,
                       "G474 EXTI event capacity")
        common.integer(selection.get("irq_priority"), 0, 15,
                       "G474 EXTI IRQ priority")


def validate_dma(route, mode, normalize):
    expected = (("DMA1:stream1:channel5",) if route["controller"] == "ADC1" and
                mode == "trigger-dma" else None)
    common.validate_dma(route, mode, expected, "stm32g474", normalize,
                        trigger="TIM6")


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
