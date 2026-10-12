"""Source-reviewed G474 GPIO, LPUART bytes and exact EXTI line contracts."""
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
SPI_REQUIRES_ENDPOINT = False
DMA_VECTOR = None
DMA_TOPOLOGY = (frozenset(), range(0), range(0))
CLOCK_PROFILES = MappingProxyType({
    "hse24-pll170": MappingProxyType({
        "hse_hz": 24000000, "core_hz": 170000000,
        "apb1_hz": 170000000, "apb2_hz": 170000000})})
LIMITS = MappingProxyType({"uart_baud": (2594, 1000000)})
_MODES = MappingProxyType({
    ("gpio", "input"): "gpio", ("gpio", "output"): "gpio",
    ("uart", "irq-bytes"): "uart", ("exti", "edge-event"): "exti"})


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
        expected = (("PA2", "tx", 12), ("PA3", "rx", 12))
        actual = tuple((pin["pin"], pin["function"], pin["af"])
                       for pin in route["pins"])
        if route["controller"] != "LPUART1" or actual != expected:
            common.fail("Only reviewed LPUART1 PA2/PA3 AF12 is maintained")
        baud = common.integer(selection["baud"], *LIMITS["uart_baud"],
                              "LPUART baud")
        divisor = (170000000 * 16 + baud // 2) // baud
        if not 0x300 <= divisor <= 0xfffff:
            common.fail("LPUART baud differs from the actual PRESC16 divider")
        if selection["rx_profile"] != "bytes":
            common.fail("G474 LPUART provider requires byte reception")
        if type(selection.get("irq_priority")) is not int or selection["irq_priority"] != 5:
            common.fail("G474 LPUART provider implements IRQ priority5 only")
    elif kind == "gpio":
        pins = route["pins"]
        if (route["controller"] != "GPIOA" or not pins or
                any(pin["pin"] != "PA5" or pin["af"] != 0 for pin in pins)):
            common.fail("Only reviewed GPIOA PA5 route is maintained")
        mask = common.integer(selection.get("mask", 32), 1, 0xffff,
                              "GPIO mask")
        initial = common.integer(selection.get("initial", 0), 0, 0xffff,
                                 "GPIO initial")
        if mask != 32 or initial & ~mask:
            common.fail("GPIO mask/initial differs from the reviewed PA5 route")
    elif kind == "exti":
        controller = route["controller"]
        if controller not in {"EXTI0", "EXTI1"}:
            common.fail("Only independent G474 EXTI0/1 vectors are maintained")
        pin = "PA" + controller[-1]
        if tuple((item["pin"], item["function"], item["af"])
                 for item in route["pins"]) != ((pin, "input", 0),):
            common.fail("G474 EXTI requires its exact GPIOA input route")
        if selection.get("edge") not in {"rising", "falling", "both"}:
            common.fail("G474 EXTI requires an explicit raw edge selection")
        common.integer(selection.get("event_capacity"), 1, 4096,
                       "G474 EXTI event capacity")
        common.integer(selection.get("irq_priority"), 0, 15,
                       "G474 EXTI IRQ priority")


def validate_dma(route, mode, normalize):
    del route, mode, normalize
    common.fail("G474 DMA provider is unimplemented")


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
