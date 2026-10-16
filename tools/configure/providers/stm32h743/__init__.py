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
MODEL = False
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = False
DMA_VECTOR = "Stream"
DMA_TOPOLOGY = (frozenset((1,)), range(2), range(45, 47))
DMA_MEMORY_ENGINES = ("DMA1", "DMA2", "BDMA", "MDMA")
CONDITIONAL_PACKAGE_PADS = ("PC2_C", "PC3_C")
CLOCK_PROFILES = MappingProxyType({
    f"hsi{mhz}-fixed-cache-{cache}": MappingProxyType({
        "source": "hsi", "input_hz": 64000000, "core_hz": mhz * 1000000,
        "apb1_hz": mhz * 1000000, "apb2_hz": mhz * 1000000})
    for mhz in (32, 64) for cache in ("off", "on")})
LIMITS = MappingProxyType({"uart_baud": (977, 1000000)})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("uart", "dma-tx"): "uart_dma",
    ("uart", "rx-dma-idle"): "uart_receive"})


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
        pins = route["pins"]
        if (route["controller"] != "GPIOB" or not pins or
                any(pin["pin"] != "PB0" or pin["af"] != 0 for pin in pins)):
            common.fail("Only reviewed GPIOB PB0 route is maintained")
        mask = common.integer(selection.get("mask", 1), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != 1 or initial & ~mask:
            common.fail("GPIO mask/initial differs from the reviewed PB0 route")


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
