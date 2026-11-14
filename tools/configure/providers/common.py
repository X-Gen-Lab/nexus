"""Shared input checks; provider packages own silicon-specific limits."""
from collections.abc import Mapping
from dataclasses import dataclass, field
import re
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol

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
    """Admit an explicit compiler selector and one safe startup basename."""
    if (not isinstance(value, Mapping) or
            set(value) != {"macro", "startup"}):
        fail("Vendor device requires exact macro and startup fields")
    _catalog_symbol(value["macro"], "Vendor device macro")
    _catalog_path(value["startup"], "Vendor startup basename", basename=True)


def _catalog_symbol(value, purpose):
    try:
        Symbol(value)
    except ValueError:
        fail(purpose + " requires an explicit C identifier")


def _catalog_path(value, purpose, *, basename=False):
    """Only canonical relative names can enter a source or SDK path."""
    component = r"[A-Za-z0-9_][A-Za-z0-9_.-]*"
    pattern = component if basename else component + r"(?:/" + component + r")*"
    if not isinstance(value, str) or re.fullmatch(pattern, value) is None:
        fail(purpose + " requires a safe canonical relative path")
    return value


def _catalog_identifier(value, purpose):
    if (not isinstance(value, str) or
            re.fullmatch(r"[A-Za-z0-9_][A-Za-z0-9_.-]*", value) is None):
        fail(purpose + " requires an explicit metadata identifier")
    return value


@dataclass(frozen=True)
class VendorIrqBinding:
    """An enum slot and the independently reviewed official startup symbol."""

    number: int
    handler: str


@dataclass(frozen=True)
class VendorIrqResource:
    """A canonical resource retains its actual SDK enum and startup entry."""

    enum: str
    number: int
    handler: str


def irq_resource_bindings(bindings):
    """Index actual declarations by existing resource grammar, never guess."""
    from tools.configure.resource_graph import normalize_resource
    result = {}
    for enum, binding in bindings.items():
        name = normalize_resource("irq:" + enum)[4:]
        if name in result:
            fail("Catalog IRQ resource collision: " + name)
        result[name] = VendorIrqResource(enum, binding.number, binding.handler)
    return MappingProxyType(result)


@dataclass(frozen=True)
class VendorDeviceProfile:
    """Catalog compile facts; existence does not grant a supported exact part."""

    identifier: str
    macro: str
    startup: str
    priority_bits: int
    external_irq_count: int
    irq_bindings: Mapping


def vendor_irq_bindings(value, count):
    """Validate declared bindings; original SDK comparison remains independent."""
    if not isinstance(value, Mapping) or not value:
        fail("Vendor IRQ bindings require a nonempty reviewed enum map")
    bindings, numbers = {}, set()
    for name, item in value.items():
        _catalog_symbol(name, "Vendor IRQ enum")
        if (not isinstance(item, Mapping) or set(item) != {"number", "handler"}):
            fail("Vendor IRQ binding requires exact enum, number and handler")
        number = integer(item["number"], 0, count - 1, "Vendor IRQ number")
        handler = item["handler"]
        _catalog_symbol(handler, "Vendor IRQ handler")
        if number in numbers:
            fail("Vendor IRQ bindings require unique slots and exact handlers")
        numbers.add(number)
        bindings[name] = VendorIrqBinding(number, handler)
    irq_resource_bindings(bindings)
    return MappingProxyType(bindings)


def vendor_device_catalog(value):
    """Validate declaration data without opening any vendor SDK input."""
    if (not isinstance(value, Mapping) or
            set(value) != {"schema_version", "scope", "sdk", "devices", "inputs"} or
            type(value["schema_version"]) is not int or value["schema_version"] != 1 or
            value["scope"] != "sdk_device_compile"):
        fail("Vendor device catalog schema differs from the maintained contract")
    sdk = value["sdk"]
    if (not isinstance(sdk, Mapping) or
            set(sdk) != {"directory", "dependency", "commit", "core_directory",
                         "core_dependency", "core_commit"} or
            any(not isinstance(sdk[name], str) or
                re.fullmatch(r"[0-9a-f]{40}", sdk[name]) is None
                for name in ("commit", "core_commit"))):
        fail("Vendor device catalog requires exact source identities")
    for name in ("directory", "core_directory"):
        _catalog_path(sdk[name], "Catalog SDK directory")
    for name in ("dependency", "core_dependency"):
        _catalog_identifier(sdk[name], "Catalog SDK dependency alias")
    entries = {}
    macros = set()
    for row in sequence(value["devices"], "Vendor device catalog entries"):
        fields = {"id", "define", "header", "startup", "external_irq_count",
                  "last_external_irq", "nvic_priority_bits", "timebase_presence"}
        if (not isinstance(row, Mapping) or not fields.issubset(row) or
                set(row) - fields - {"irq_bindings"}):
            fail("Vendor device catalog requires exact entry fields")
        macro, identifier = row["define"], row["id"]
        _catalog_identifier(identifier, "Vendor device catalog ID")
        _catalog_path(row["header"], "Catalog header")
        _catalog_path(row["startup"], "Catalog startup")
        startup = row["startup"].rsplit("/", 1)[-1]
        validate_vendor_device({"macro": macro, "startup": startup})
        if identifier in entries or macro in macros:
            fail("Vendor device catalog contains duplicate device IDs or selectors")
        bits = integer(row["nvic_priority_bits"], 1, 8, "Catalog IRQ priority bits")
        count = integer(row["external_irq_count"], 1, 496, "Catalog external IRQ count")
        last = row["last_external_irq"]
        if (not isinstance(last, Mapping) or set(last) != {"name", "number"} or
                type(last["number"]) is not int or last["number"] != count - 1):
            fail("Vendor device catalog has inconsistent last IRQ facts")
        _catalog_symbol(last["name"], "Catalog last IRQ enum")
        timebases = row["timebase_presence"]
        if (not isinstance(timebases, Mapping) or
                any(type(flag) is not bool for flag in timebases.values())):
            fail("Vendor device catalog requires boolean timebase presence")
        for name in timebases:
            _catalog_identifier(name, "Catalog timebase")
        bindings = (vendor_irq_bindings(row["irq_bindings"], count)
                    if "irq_bindings" in row else MappingProxyType({}))
        entries[identifier] = VendorDeviceProfile(
            identifier, macro, startup, bits, count, bindings)
        macros.add(macro)
    if not entries or not isinstance(value["inputs"], Mapping) or not value["inputs"]:
        fail("Vendor device catalog entries and input identities are required")
    for name, record in value["inputs"].items():
        _catalog_path(name, "Catalog input")
        if (not isinstance(record, Mapping) or set(record) != {"bytes", "sha256"} or
                type(record["bytes"]) is not int or record["bytes"] <= 0 or
                not isinstance(record["sha256"], str) or
                re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is None):
            fail("Vendor device catalog requires ordinary input identity records")
    for row in value["devices"]:
        for field in ("header", "startup"):
            if row[field] not in value["inputs"]:
                fail("Catalog " + field + " lacks its declared input identity")
    return MappingProxyType(entries)


@dataclass(frozen=True)
class PartPolicy:
    """Reviewed package and optional placement, separate from physical sizes."""

    package: str
    optional_memory: frozenset[str] = frozenset()
    device: str | None = None

    def validate(self, variant):
        if variant["package"] != self.package:
            fail("Physical package differs from the reviewed exact part")
        if self.device is not None and variant.get("device") != self.device:
            fail("Exact part device selector differs from its reviewed policy")
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
    spi_dma: tuple[tuple[str, ...] | None, str] | None = None
    spi_transaction_ops: Symbol | None = None
    spi_dma_owner: tuple[Symbol, Symbol] | None = None
    model_async_echo: bool = False
    spi_lsb_first: bool = False
    i2c_ten_bit: bool = False

    def __post_init__(self):
        if (self.spi_transaction_ops is not None and
                not isinstance(self.spi_transaction_ops, Symbol)):
            fail("SPI transaction operations require an explicit Symbol")
        if ((self.spi_dma is None) != (self.spi_dma_owner is None) or
                self.spi_dma_owner is not None and (
                    not isinstance(self.spi_dma_owner, tuple) or
                    len(self.spi_dma_owner) != 2 or
                    any(not isinstance(value, Symbol) for value in self.spi_dma_owner))):
            fail("SPI DMA requires its explicit contains/attach_wake Symbol pair")

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


def validate_mode_context(selection, route, *, backend, clock_profile,
                          part=None, clock_plan=None):
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
    face_context: Address | None = None
    face_definitions: tuple[str, ...] = ()
    admission: tuple[DirectCall, ...] = ()
    initialize_sequences: tuple[InitializeSequence, ...] = ()
    initial_state: Symbol | None = None
    pre_c_effects: bool = False
    irq_connections: tuple = ()
    static_declarations: tuple = ()

    def __post_init__(self):
        from tools.configure.static_ir import Declaration, IrqConnection
        for name, kind in (("irq_connections", IrqConnection),
                           ("static_declarations", Declaration)):
            values = getattr(self, name)
            if (not isinstance(values, (tuple, list)) or
                    any(not isinstance(value, kind) for value in values)):
                fail("Construction " + name + " requires immutable typed nodes")
            object.__setattr__(self, name, tuple(values))
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
        if self.face_context is not None and not isinstance(self.face_context, Address):
            fail("Construction face context requires a typed static object address")
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
    ('capture', 'snapshot-raw'): ModeTraits(irq=True, rx_dma=True),
    ('capture', 'snapshot-jpeg'): ModeTraits(irq=True, rx_dma=True),
    ('capture', 'frames-raw'): ModeTraits(irq=True, rx_dma=True),
    ('capture', 'frames-jpeg'): ModeTraits(irq=True, rx_dma=True),
    ('capture', 'raw-blocks'): ModeTraits(irq=True, rx_dma=True, continuous=True),
    ('capture', 'jpeg-blocks'): ModeTraits(irq=True, rx_dma=True, continuous=True),
    **{('audio', role + '-' + direction): ModeTraits(
        irq=True, tx_dma=direction != 'rx', rx_dma=direction != 'tx')
       for role in ('master', 'slave') for direction in ('tx', 'rx', 'duplex')},
    **{('audio', role + '-' + direction + '-blocks'): ModeTraits(
        irq=True, tx_dma=direction != 'rx', rx_dma=direction != 'tx',
        continuous=True)
       for role in ('master', 'slave') for direction in ('tx', 'rx', 'duplex')},
    ("dac", "direct"): ModeTraits(polling=True),
    ("crc", "crc32-words"): ModeTraits(polling=True),
    ("cipher", "irq-blocks"): ModeTraits(irq=True),
    ("cipher", "dma-blocks"): ModeTraits(irq=True, tx_dma=True, rx_dma=True),
    ("hash", "irq-blocks"): ModeTraits(irq=True),
    ("hash", "dma-blocks"): ModeTraits(irq=True, tx_dma=True),
    ("display", "rgb-scanout"): ModeTraits(irq=True, bus_master=True,
                                          continuous=True),
    ("image", "irq-blit"): ModeTraits(irq=True, bus_master=True),
    ("dac", "triggered"): ModeTraits(polling=True),
    ("dac", "noise"): ModeTraits(polling=True),
    ("dac", "triangle"): ModeTraits(polling=True),
    ("dac", "trigger-dma"): ModeTraits(irq=True, tx_dma=True),
    ("dac", "trigger-dma-circular"): ModeTraits(irq=True, tx_dma=True,
                                               continuous=True),
    ("memory_window", "startup-memory"): ModeTraits(),
    ("external_bus", "async16"): ModeTraits(polling=True),
    ("external_bus", "external-memory"): ModeTraits(polling=True),
    ("external_bus", "sdram-controller"): ModeTraits(polling=True),
    ("external_bus", "qspi-indirect-sdr"): ModeTraits(polling=True),
    ("external_bus", "qspi-indirect-dma"): ModeTraits(polling=True, tx_dma=True),
    ("mapped_storage", "nor-mapped-cpu"): ModeTraits(polling=True),
    ("flash_async", "nor-indirect"): ModeTraits(polling=True),
    ("flash_async", "nor-indirect-quad-read"): ModeTraits(polling=True),
    ("memory_window", "psram-window"): ModeTraits(),
    ("flash_async", "nor-cfi"): ModeTraits(polling=True),
    ("nand", "raw-page"): ModeTraits(polling=True),
    ("memory_window", "cpu-window"): ModeTraits(),
    ("memory_window", "graphics-owned"): ModeTraits(),
    ("parallel16", "8080-16"): ModeTraits(polling=True),
    ("block", "sdio-poll"): ModeTraits(polling=True),
    ("block", "sdio-irq"): ModeTraits(irq=True),
    ("block", "sdio-dma"): ModeTraits(irq=True, tx_dma=True, rx_dma=True),
    ("rtc", "calendar-alarm"): ModeTraits(),
    ("rtc", "retained-calendar-alarm"): ModeTraits(),
    ("rtc", "adopt-retained"): ModeTraits(polling=True),
    ("rtc", "provision-empty"): ModeTraits(polling=True),
    ("lptim", "counter16-oneshot"): ModeTraits(),
    ("lptim", "irq-counter16-oneshot"): ModeTraits(irq=True),
    ("adc_group", "regular-sequence-dma"): ModeTraits(irq=True, rx_dma=True),
    ("adc_group", "regular-blocks"): ModeTraits(
        irq=True, rx_dma=True, block_stream=True, continuous=True),
    ("adc_group", "regular-software-injected-blocks"): ModeTraits(
        irq=True, rx_dma=True, block_stream=True, continuous=True),
    ("adc_group", "regular-timer-injected-blocks"): ModeTraits(
        irq=True, rx_dma=True, block_stream=True, continuous=True),
    ("adc_group", "regular-auto-injected"): ModeTraits(irq=True, rx_dma=True),
    ("adc_group", "injected-sequence"): ModeTraits(irq=True),
    ("pwm_adc", "finite-regular-dma"): ModeTraits(irq=True, rx_dma=True),
    ("can_fd", "iso-fd-loopback"): ModeTraits(irq=True),
    ("can_fd", "iso-fd-normal"): ModeTraits(irq=True),
    ("pwm_group", "coherent-pwm"): ModeTraits(irq=True),
    ("timer_pulse", "one-pulse"): ModeTraits(irq=True),
    ("timer_sequence", "dma-burst"): ModeTraits(irq=True, tx_dma=True),
    ("encoder", "quadrature"): ModeTraits(),
    ("timer_capture", "edge-capture"): ModeTraits(irq=True),
    ("advanced_pwm", "complementary-pwm"): ModeTraits(irq=True),
    ("eth", "raw-rmii"): ModeTraits(irq=True, bus_master=True),
    ("eth", "raw-mii"): ModeTraits(irq=True, bus_master=True),
    ("eth_frames", "rmii-vlan-sg-ptp"): ModeTraits(irq=True, bus_master=True),
    ("eth_frames", "mii-vlan-sg-ptp"): ModeTraits(irq=True, bus_master=True),
    ("usb_device", "device-fs"): ModeTraits(irq=True),
    ("usb_device", "otg-fs"): ModeTraits(irq=True),
    ("usb_device", "otg-hs-fs"): ModeTraits(irq=True),
    ("usb_device", "otg-hs-ulpi"): ModeTraits(irq=True),
    ("usb_device", "device-hs-fs"): ModeTraits(irq=True),
    ("usb_device", "device-hs-ulpi"): ModeTraits(irq=True),
    ("usb_host", "host-fs"): ModeTraits(irq=True),
    ("usb_host", "host-hs-fs"): ModeTraits(irq=True),
    ("usb_host", "host-hs-ulpi"): ModeTraits(irq=True),
    ("can", "classic-irq"): ModeTraits(irq=True),
    ("gpio", "input"): ModeTraits(),
    ("gpio", "output"): ModeTraits(),
    ("gpio", "named-states"): ModeTraits(),
    ("uart", "irq-byte-event"): ModeTraits(irq=True),
    ("uart", "irq-wire"): ModeTraits(irq=True),
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
    ("entropy", "rng-words"): ModeTraits(polling=True),
    ("spi", "short-poll"): ModeTraits(polling=True),
    ("spi", "irq-frames"): ModeTraits(irq=True),
    ("spi", "async-model"): ModeTraits(irq=True),
    ("spi", "dma"): ModeTraits(irq=True, tx_dma=True, rx_dma=True),
    ("spi", "dma-full-duplex"): ModeTraits(
        irq=True, tx_dma=True, rx_dma=True),
    ("i2c_target", "target-irq"): ModeTraits(irq=True),
    ("i2c_target", "smbus-device"): ModeTraits(irq=True),
    ("i2c", "poll-messages"): ModeTraits(polling=True),
    ("i2c", "irq-messages"): ModeTraits(irq=True),
    ("i2c", "dma-messages"): ModeTraits(
        irq=True, tx_dma=True, rx_dma=True),
    ("flash", "poll-byte"): ModeTraits(polling=True),
    ("flash", "poll-word"): ModeTraits(polling=True),
    ("watchdog", "independent"): ModeTraits(),
    ("watchdog", "window"): ModeTraits(),
    ("exti", "edge-event"): ModeTraits(irq=True),
    ("exti", "hardware-event"): ModeTraits(),
    ("exti", "pvd-level"): ModeTraits(polling=True),
    ("exti", "pvd-events"): ModeTraits(irq=True),
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


def validate_dma(route, mode, expected, family):
    actual = tuple(sequence(route_dma(route, mode), "DMA route"))
    if expected is None or actual != expected:
        fail(f"DMA route differs from fixed provider: {family}/{route['controller']}/{mode}")
