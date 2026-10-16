"""Source-reviewed U575 GPIO/USART byte-mode and inactive IWDG contract."""
from types import MappingProxyType
from .. import common


CPU_ABI = MappingProxyType({
    "arch": "cortex-m33", "fpu": "fpv5-sp-d16", "float_abi": "hard"})
ENUM_ABI = "short-enums"
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
SPI_REQUIRES_ENDPOINT = False
DMA_VECTOR = None
DMA_TOPOLOGY = (frozenset(), range(0), range(0))
DMA_MEMORY_ENGINES = ("GPDMA1", "LPDMA1")
CLOCK_PROFILES = MappingProxyType({
    "msis4": MappingProxyType({
        "source": "msis", "input_hz": 4000000, "core_hz": 4000000,
        "apb1_hz": 4000000, "apb2_hz": 4000000})})
LIMITS = MappingProxyType({"uart_baud": (62, 250000)})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart",
    ("watchdog", "independent"): "watchdog"})
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
    if kind == "uart":
        expected = _SERIAL_PINS.get(route["controller"])
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if expected is None or actual != expected:
            common.fail("Only reviewed USART1 PA9/PA10 and USART2 PA2/PA3 AF7 are maintained")
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


def validate_dma(route, mode, normalize):
    del route, mode, normalize
    common.fail("U575 DMA provider is unimplemented")


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
