"""Shared input checks; provider packages own silicon-specific limits."""
from collections.abc import Mapping
from dataclasses import dataclass, field
import re
from types import MappingProxyType

from tools.configure.calls import DirectCall, InitializeSequence, Symbol

UINT32_MAX = (1 << 32) - 1


class ConfigurationError(ValueError):
    """A rejected input has no usable bundle; details describe the rejection."""

    def __init__(self, message, *, code="invalid_configuration", **details):
        super().__init__(message)
        self.details = {"code": code, **details}

    def diagnostic(self, source=None):
        """Keep machine-readable evidence beside the original human message."""
        result = {**self.details, "message": str(self)}
        if source is not None:
            result.setdefault("source", str(source))
        return result


def fail(message, **details):
    raise ConfigurationError(message, **details)


def validate_vendor_device(value):
    """Admit data identifiers only, never compiler flags or startup paths."""
    if (not isinstance(value, Mapping) or
            set(value) != {"macro", "startup"}):
        fail("Vendor device requires exact macro and startup fields")
    macro, startup = value["macro"], value["startup"]
    if (not isinstance(macro, str) or
            re.fullmatch(r"STM32F4[A-Za-z0-9]+", macro) is None or
            startup != "startup_" + macro.lower() + ".s"):
        fail("Invalid STM32F4 vendor device identifiers")


@dataclass(frozen=True)
class VendorDeviceProfile:
    """Catalog compile facts; existence does not grant a supported exact part."""

    identifier: str
    macro: str
    startup: str
    priority_bits: int
    external_irq_count: int


def vendor_device_catalog(value):
    """Validate declaration data without opening any vendor SDK input."""
    if (not isinstance(value, Mapping) or
            set(value) != {"schema_version", "scope", "sdk", "devices", "inputs"} or
            type(value["schema_version"]) is not int or value["schema_version"] != 1 or
            value["scope"] != "sdk_device_compile"):
        fail("Vendor device catalog schema differs from the maintained contract")
    sdk = value["sdk"]
    if (not isinstance(sdk, Mapping) or
            set(sdk) != {"directory", "commit", "core_directory", "core_commit"} or
            sdk["directory"] != "vendors/st/cmsis_device_f4" or
            sdk["core_directory"] != "vendors/arm/CMSIS_6" or
            any(not isinstance(sdk[name], str) or
                re.fullmatch(r"[0-9a-f]{40}", sdk[name]) is None
                for name in ("commit", "core_commit"))):
        fail("Vendor device catalog requires exact source identities")
    entries = {}
    macros = set()
    for row in sequence(value["devices"], "Vendor device catalog entries"):
        if (not isinstance(row, Mapping) or set(row) != {
                "id", "define", "header", "startup", "external_irq_count",
                "last_external_irq", "nvic_priority_bits", "timebase_presence"}):
            fail("Vendor device catalog requires exact entry fields")
        macro, identifier = row["define"], row["id"]
        if (not isinstance(macro, str) or not isinstance(identifier, str) or
                identifier != macro.lower().removesuffix("xx")):
            fail("Vendor device catalog ID differs from its declared selector")
        startup = "startup_" + macro.lower() + ".s"
        validate_vendor_device({"macro": macro, "startup": startup})
        if (row["header"] != "Include/" + macro.lower() + ".h" or
                row["startup"] != "Source/Templates/gcc/" + startup):
            fail("Vendor device catalog header/startup identity mismatch")
        if identifier in entries or macro in macros:
            fail("Vendor device catalog contains duplicate device IDs or selectors")
        bits = integer(row["nvic_priority_bits"], 1, 8, "Catalog IRQ priority bits")
        count = integer(row["external_irq_count"], 1, 496, "Catalog external IRQ count")
        last = row["last_external_irq"]
        if (not isinstance(last, Mapping) or set(last) != {"name", "number"} or
                not isinstance(last["name"], str) or
                re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*_IRQn", last["name"]) is None or
                type(last["number"]) is not int or last["number"] != count - 1):
            fail("Vendor device catalog has inconsistent last IRQ facts")
        timebases = row["timebase_presence"]
        if (not isinstance(timebases, Mapping) or
                set(timebases) != {"TIM2", "TIM5", "LPTIM1"} or
                any(type(flag) is not bool for flag in timebases.values())):
            fail("Vendor device catalog requires boolean timebase presence")
        entries[identifier] = VendorDeviceProfile(identifier, macro, startup, bits, count)
        macros.add(macro)
    if not entries or not isinstance(value["inputs"], Mapping) or not value["inputs"]:
        fail("Vendor device catalog entries and input identities are required")
    for name, record in value["inputs"].items():
        if (not isinstance(name, str) or "\\" in name or name.startswith("/") or
                any(part in {"", ".", ".."} for part in name.split("/")) or
                not isinstance(record, Mapping) or set(record) != {"bytes", "sha256"} or
                type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(record["sha256"], str) or
                re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is None):
            fail("Vendor device catalog requires ordinary input identity records")
    return MappingProxyType(entries)


@dataclass(frozen=True)
class PartPolicy:
    """Reviewed package and optional placement, separate from physical sizes."""

    package: str
    optional_memory: frozenset[str] = frozenset()

    def validate(self, variant):
        if variant["package"] != self.package:
            fail("Physical package differs from the reviewed exact part")
        domains = {region["id"]: region for region in variant["memory"]}
        if not self.optional_memory.issubset(domains) or any(
                domains[name]["linker"] for name in self.optional_memory):
            fail("Optional memory policy differs from physical part domains")


def exact_part(parts, part):
    """An unregistered density/package never inherits another part's policy."""
    if part not in parts:
        fail(f"Unmaintained exact part: {part}")
    return parts[part]


@dataclass(frozen=True)
class I2cTimingClock:
    """Selected local source bounds; electrical assumptions remain authored."""

    nominal_hz: int
    pclk_hz: int
    minimum_hz: int | None = None
    maximum_hz: int | None = None
    pclk_minimum_hz: int | None = None
    requires_explicit: bool = False


@dataclass(frozen=True)
class EndpointPolicy:
    """Family-owned endpoint capabilities; hardware routes remain separate."""

    spi_transaction: bool = False
    i2c_long: bool = False
    spi_dma: tuple[tuple[str, ...] | None, str] | None = None
    model_async_echo: bool = False
    spi_lsb_first: bool = False
    i2c_ten_bit: bool = False

    def validate(self, device, controller):
        driver, kind, mode = (device["driver"], controller["kind"],
                              controller["mode"])
        if kind == "i2c":
            width = device.get("address_bits", 7)
            if type(width) is not int or width not in (7, 10):
                fail("I2C address_bits must be exactly 7 or 10")
            if width == 10 and not self.i2c_ten_bit:
                fail("I2C requires a reviewed 10-bit endpoint provider")
        if "lsb_first" in device:
            if type(device["lsb_first"]) is not bool:
                fail("SPI lsb_first must be a boolean")
            if device["lsb_first"] and (not self.spi_lsb_first or driver not in {
                    "spi-endpoint", "spi-transaction", "spi-dma-transaction"}):
                fail("SPI LSB-first requires a reviewed endpoint provider")
        if driver == "spi-transaction":
            if kind != "spi":
                fail("SPI transaction requires its SPI controller")
            if not self.spi_transaction:
                fail("SPI transaction provider is not maintained")
            if mode != "short-poll":
                fail("SPI transaction requires the polling provider mode")
        if driver == "i2c-long-endpoint" and (
                kind != "i2c" or not self.i2c_long or mode != "poll-messages"):
            fail("Long I2C requires a maintained polling V2 provider")
        if driver == "spi-dma-transaction":
            if (kind != "spi" or self.spi_dma is None or
                    mode != self.spi_dma[1] or
                    (self.spi_dma[0] is not None and
                     controller["controller"] not in self.spi_dma[0])):
                fail("SPI DMA transaction requires a reviewed DMA mode")
        if self.model_async_echo and mode == "async-model":
            if driver != "spi-dma-transaction" or "model_bytes" in device:
                fail("Native async SPI requires enhanced echo transactions "
                     "without model_bytes")


def validate_mode_context(selection, route, *, backend, clock_profile):
    """Default families impose no additional mode/profile coupling here."""


def mode_fields(selection, route, *, clock_profile):
    """Families explicitly derive private hardware admission fields."""
    return {}


def validate_controller_set(selected, *, clock_profile):
    """Default families have no additional whole-selection constraints."""


def validate_endpoint_clock(speed, *, clock_plan):
    """Default fixed-clock bounds are already checked by controller facts."""


def configuration_header(result):
    """An ordinary family has no extra compile-time public constants."""
    return []


def _static_definitions(values):
    """Reject executable algorithms while retaining static facts and thin IRQs.

    This is an assembly boundary, not a C parser or a compiler replacement.
    Address constants and initializer expressions remain compiler-checked;
    generated function bodies may only forward fixed calls to concrete C.
    """
    code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  " ", "\n".join(values), flags=re.DOTALL)
    identifier = r"[A-Za-z_][A-Za-z0-9_]*"
    reference = rf"(?:&{identifier}(?:\.{identifier})*|{identifier}|[0-9]+[uU]?)"
    call = rf"nx_{identifier}\s*\(\s*(?:{reference}(?:\s*,\s*{reference})*)?\s*\)\s*;"

    def irq(match):
        name, body = match.groups()
        probe = re.fullmatch(rf"\s*NX_IO_IRQ_PROBE_BEGIN\(\s*({identifier})\s*\);"
                             rf"\s*(.*?)\s*NX_IO_IRQ_PROBE_END\(\s*\1\s*\);\s*",
                             body, flags=re.DOTALL)
        if probe is not None:
            body = probe.group(2)
        if (not name.endswith("IRQHandler") or
                re.fullmatch(rf"\s*(?:{call}\s*)+", body) is None):
            fail("Construction static assembly permits only fixed thin IRQ calls")
        return " "

    code = re.sub(rf"\bvoid\s+({identifier})\s*\(\s*void\s*\)\s*\{{([^{{}}]*)\}}",
                  irq, code)
    if (re.search(r"\b(?:if|else|while|do|for|switch|case|default|goto|return)\b", code)
            or re.search(rf"\b{identifier}[\s*]+{identifier}\s*"
                         r"\([^;{}]*\)\s*\{", code)
            or re.search(rf"->\s*{identifier}(?:\s*\[[^\]]*\])?\s*"
                         r"(?:[+\-*/%&|^]?=(?!=)|\+\+|--)", code)
            or re.search(rf"(?:\+\+|--)\s*{identifier}\s*->", code)):
        fail("Construction static assembly cannot contain executable hardware algorithms")




@dataclass(frozen=True)
class Construction:
    """One provider's explicit static storage and cold lifecycle contract.

    Production assembly uses named fields; no family inference or runtime
    metadata is emitted. initial_state names a concrete C aggregate initializer,
    never generated assignment code. pre_c_effects identifies image-lifetime
    responsibility that already exists before Platform startup can succeed.
    """
    state_type: str
    definitions: tuple[str, ...]
    initialize: DirectCall
    stop: tuple[DirectCall, ...]
    retain_on_failed_init: bool
    exports: tuple[str, ...] = ()
    pin_snapshots: Mapping[str, str] = field(default_factory=dict)
    face_context: str | None = None
    face_definitions: tuple[str, ...] = ()
    admission: tuple[DirectCall, ...] = ()
    initialize_sequences: tuple[InitializeSequence, ...] = ()
    initial_state: Symbol | None = None
    pre_c_effects: bool = False

    def __post_init__(self):
        if (self.initial_state is not None and
                not isinstance(self.initial_state, Symbol)):
            fail("Construction initial state requires a typed C initializer symbol")
        if type(self.pre_c_effects) is not bool:
            fail("Construction pre-C effects require an explicit boolean")
        if not isinstance(self.initialize_sequences, (tuple, list)):
            fail("Construction initialization sequences require an ordered sequence")
        sequences = tuple(self.initialize_sequences)
        if any(not isinstance(value, InitializeSequence) for value in sequences):
            fail("Construction initialization sequences require typed direct calls")
        if len({value.function for value in sequences}) != len(sequences):
            fail("Construction initialization sequence names must be unique")
        object.__setattr__(self, "initialize_sequences", sequences)
        if (not isinstance(self.state_type, str) or
                re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", self.state_type)
                is None):
            fail("Construction requires an explicit C state type")
        if not isinstance(self.initialize, DirectCall):
            fail("Construction initializer requires an explicit direct call")
        if not isinstance(self.admission, (tuple, list)):
            fail("Construction admission requires ordered direct calls")
        admission = tuple(self.admission)
        if any(not isinstance(value, DirectCall) for value in admission):
            fail("Construction admission requires explicit direct calls")
        object.__setattr__(self, "admission", admission)
        if not isinstance(self.stop, (tuple, list)):
            fail("Construction stop requires an ordered direct call sequence")
        stops = tuple(self.stop)
        if any(not isinstance(value, DirectCall) for value in stops):
            fail("Construction stop requires explicit direct calls")
        object.__setattr__(self, "stop", stops)
        if type(self.retain_on_failed_init) is not bool:
            fail("Construction requires boolean failed-init responsibility")
        if (self.face_context is not None and
                re.fullmatch(r"&[A-Za-z_][A-Za-z0-9_]*",
                             self.face_context) is None):
            fail("Construction face context requires an explicit static object")
        for name in ("definitions", "exports", "face_definitions"):
            values = tuple(getattr(self, name))
            if any(not isinstance(value, str) for value in values):
                fail(f"Construction {name} requires C source strings")
            _static_definitions(values)
            object.__setattr__(self, name, values)
        snapshots = dict(self.pin_snapshots)
        if any(not isinstance(key, str) or not isinstance(value, str)
               for key, value in snapshots.items()):
            fail("Construction pin snapshots require explicit names")
        object.__setattr__(self, "pin_snapshots", MappingProxyType(snapshots))

def owned_claims(result, owners):
    """Instance names never acquire Board/SoC reservations or kernel claims."""
    return (claim for claim in result.resource_plan.claims
            if claim.mode not in {"reserved", "kernel"} and
            owners.intersection(claim.owner.split(";")))


@dataclass(frozen=True)
class ModeTraits:
    """Execution shape only; provider mode tables still establish support."""
    irq: bool = False
    polling: bool = False
    block_stream: bool = False
    tx_dma: bool = False
    rx_dma: bool = False
    continuous: bool = False
    bus_master: bool = False

    @property
    def dma(self):
        return self.tx_dma or self.rx_dma


_MODE_TRAITS = MappingProxyType({
    ("dac", "direct"): ModeTraits(polling=True),
    ("crc", "crc32-words"): ModeTraits(polling=True),
    ("dac", "triggered"): ModeTraits(polling=True),
    ("dac", "noise"): ModeTraits(polling=True),
    ("dac", "triangle"): ModeTraits(polling=True),
    ("dac", "trigger-dma"): ModeTraits(irq=True, tx_dma=True),
    ("dac", "trigger-dma-circular"): ModeTraits(irq=True, tx_dma=True,
                                               continuous=True),
    ("memory_window", "startup-memory"): ModeTraits(),
    ("external_bus", "async16"): ModeTraits(polling=True),
    ("memory_window", "cpu-window"): ModeTraits(),
    ("parallel16", "8080-16"): ModeTraits(polling=True),
    ("block", "sdio-poll"): ModeTraits(polling=True),
    ("rtc", "calendar-alarm"): ModeTraits(),
    ("rtc", "retained-calendar-alarm"): ModeTraits(),
    ("rtc", "adopt-retained"): ModeTraits(polling=True),
    ("rtc", "provision-empty"): ModeTraits(polling=True),
    ("lptim", "counter16-oneshot"): ModeTraits(),
    ("lptim", "irq-counter16-oneshot"): ModeTraits(irq=True),
    ("adc_group", "regular-sequence-dma"): ModeTraits(irq=True, rx_dma=True),
    ("adc_group", "injected-sequence"): ModeTraits(irq=True),
    ("pwm_adc", "finite-regular-dma"): ModeTraits(irq=True, rx_dma=True),
    ("can_fd", "iso-fd-loopback"): ModeTraits(irq=True),
    ("can_fd", "iso-fd-normal"): ModeTraits(irq=True),
    ("pwm_group", "coherent-pwm"): ModeTraits(irq=True),
    ("encoder", "quadrature"): ModeTraits(),
    ("timer_capture", "edge-capture"): ModeTraits(irq=True),
    ("advanced_pwm", "complementary-pwm"): ModeTraits(irq=True),
    ("eth", "raw-rmii"): ModeTraits(irq=True, bus_master=True),
    ("eth_frames", "rmii-vlan-sg-ptp"): ModeTraits(irq=True, bus_master=True),
    ("usb_device", "device-fs"): ModeTraits(irq=True),
    ("can", "classic-irq"): ModeTraits(irq=True),
    ("gpio", "input"): ModeTraits(),
    ("gpio", "output"): ModeTraits(),
    ("gpio", "named-states"): ModeTraits(),
    ("uart", "irq-byte-event"): ModeTraits(irq=True),
    ("uart", "irq-words"): ModeTraits(irq=True),
    ("uart", "irq-bytes"): ModeTraits(irq=True),
    ("uart", "irq-fifo-bytes"): ModeTraits(irq=True),
    ("uart", "irq-fifo-rtscts-bytes"): ModeTraits(irq=True),
    ("uart", "irq-blocks"): ModeTraits(irq=True, block_stream=True),
    ("uart", "dma-tx"): ModeTraits(irq=True, tx_dma=True),
    ("uart", "dma-chain-tx"): ModeTraits(irq=True, tx_dma=True),
    ("uart", "rx-dma-idle"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True),
    ("uart", "dma-duplex"): ModeTraits(
        irq=True, block_stream=True, tx_dma=True, rx_dma=True),
    ("uart", "rx-dma-continuous"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True, continuous=True),
    ("uart", "dma-duplex-continuous"): ModeTraits(
        irq=True, block_stream=True, tx_dma=True, rx_dma=True, continuous=True),
    ("spi", "short-poll"): ModeTraits(polling=True),
    ("spi", "async-model"): ModeTraits(irq=True),
    ("spi", "dma"): ModeTraits(irq=True, tx_dma=True, rx_dma=True),
    ("spi", "dma-full-duplex"): ModeTraits(
        irq=True, tx_dma=True, rx_dma=True),
    ("i2c", "poll-messages"): ModeTraits(polling=True),
    ("i2c", "irq-messages"): ModeTraits(irq=True),
    ("i2c", "dma-messages"): ModeTraits(
        irq=True, tx_dma=True, rx_dma=True),
    ("flash", "poll-byte"): ModeTraits(polling=True),
    ("flash", "poll-word"): ModeTraits(polling=True),
    ("watchdog", "independent"): ModeTraits(),
    ("watchdog", "window"): ModeTraits(),
    ("exti", "edge-event"): ModeTraits(irq=True),
    ("pwm", "fixed-pwm"): ModeTraits(),
    ("pwm", "shared-fixed-pwm"): ModeTraits(),
    ("adc", "single-shot"): ModeTraits(polling=True),
    ("adc", "low-rate-scan"): ModeTraits(polling=True),
    ("adc", "injected-shot"): ModeTraits(polling=True),
    ("adc", "trigger-dma"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True),
    ("adc", "trigger-dma-continuous"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True, continuous=True),
})


def mode_traits(kind, mode):
    traits = _MODE_TRAITS.get((kind, mode))
    if traits is None:
        fail(f"Unmaintained execution mode: {kind}/{mode}")
    return traits


def kernel_policy_constant(result):
    """Emit the configured kernel policy, not a physical CPU mask primitive."""
    profile = result.kernel
    return ("NX_IRQ_KERNEL_" + profile.kernel_policy.upper()
            if profile is not None else "NX_IRQ_KERNEL_NONE")


def integer(value, minimum=0, maximum=UINT32_MAX, context="integer"):
    if type(value) is not int or not minimum <= value <= maximum:
        fail(f"{context}: integer required in [{minimum}, {maximum}]")
    return value


def sequence(value, context):
    """Admit ordered JSON/frozen arrays without consuming other iterables."""
    if not isinstance(value, (list, tuple)):
        fail(f"{context}: expected array")
    return value


def cpu_abi(cpu):
    """Capabilities are chip facts, never additional authored ABI choices."""
    return {key: cpu[key] for key in ("arch", "fpu", "float_abi")}






def validate_dma_resource(resource, topology):
    """The selected provider supplies its actual numeric DMA topology."""
    match = re.fullmatch(r"dma:DMA(0|[1-9][0-9]*):stream(0|[1-9][0-9]*)"
                         r"(?::channel(0|[1-9][0-9]*))?", resource)
    if match is None:
        fail(f"DMA resource is outside provider topology: {resource}")
    controller, stream = int(match[1]), int(match[2])
    channel = int(match[3]) if match[3] is not None else None
    if (controller not in topology[0] or stream not in topology[1] or
            (channel is not None and channel not in topology[2])):
        fail(f"DMA resource is outside provider topology: {resource}")
    return controller, stream, channel


def dma_irq(dma, vector, topology):
    """Name the selected family vector after its explicit topology check."""
    controller, stream, channel = validate_dma_resource("dma:" + dma, topology)
    if channel is None or vector is None:
        fail(f"DMA route requires an explicit request selector: {dma}")
    return f"irq:DMA{controller}_{vector}{stream}"


def route_dma(route, mode):
    """The exact selected mode chooses a reviewed physical tuple."""
    return route.get("mode_dma", {}).get(mode, route.get("dma", []))


def validate_dma(route, mode, expected, family, normalize, trigger=None):
    actual = tuple(sequence(route_dma(route, mode), "DMA route"))
    if expected is None or actual != expected:
        fail(f"DMA route differs from fixed provider: {family}/{route['controller']}/{mode}")
    if route["kind"] == "adc" and mode_traits("adc", mode).dma:
        resources = tuple(normalize(resource) for resource in
                          route.get("mode_resources", {}).get(mode, []))
        if trigger is None or resources != ("controller:" + trigger,):
            fail(f"Trigger resource differs from fixed provider: {family}/{route['controller']}")
