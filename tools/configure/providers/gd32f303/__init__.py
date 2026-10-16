"""Exact GD32F303RCT6 IRC8, legacy GPIO and USART0 IRQ-byte contract."""
from types import MappingProxyType
from .. import common, descriptor


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
MODEL = False
FACT_REFERENCES = descriptor.maintained(PREFIX).references
SOURCE_IMPORT_LOCK = descriptor.maintained(PREFIX).import_lock
CAN_CONTROLLERS = ()
ETH_CONTROLLER = None
USB_CONTROLLER = None
SPI_REQUIRES_ENDPOINT = True
DMA_VECTOR = None
DMA_TOPOLOGY = (frozenset(), range(0), range(0))
CLOCK_PROFILES = MappingProxyType({
    "irc8": MappingProxyType({
        "source": "hsi", "input_hz": 8000000, "core_hz": 8000000,
        "apb1_hz": 8000000, "apb2_hz": 8000000})})
LIMITS = MappingProxyType({"uart_baud": (123, 500000),
                           "spi_hz": (31250, 4000000)})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("spi", "short-poll"): "spi",
    ("watchdog", "independent"): "watchdog"})


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
        if selection["rx_profile"] != "bytes":
            common.fail("GD303 USART supports an explicit byte ring only")
        if (type(selection.get("irq_priority")) is not int or
                selection["irq_priority"] != 5):
            common.fail("GD303 USART implements IRQ priority5 only")
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


def validate_dma(route, mode, normalize):
    del route, mode, normalize
    common.fail("GD303 DMA provider is unimplemented")


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


from .bindings import constructor
from .emission import (includes, instance_declarations, implementation, operations,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
