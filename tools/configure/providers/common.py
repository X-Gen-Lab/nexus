"""Shared input checks; provider packages own silicon-specific limits."""
from collections.abc import Mapping
from dataclasses import dataclass, field
import re
from types import MappingProxyType

UINT32_MAX = (1 << 32) - 1


class ConfigurationError(ValueError):
    """A rejected input has no usable generated configuration."""


def fail(message):
    raise ConfigurationError(message)


@dataclass(frozen=True)
class Construction(Mapping):
    """One provider's explicit static storage and cold lifecycle contract.

    Mapping reads serve maintained constructor fixtures. Production assembly
    uses the named fields; no family inference or runtime metadata is emitted.
    """
    state_type: str
    definitions: tuple[str, ...]
    initialize: str
    stop: tuple[str, ...]
    retain_on_failed_init: bool
    exports: tuple[str, ...] = ()
    pin_snapshots: Mapping[str, str] = field(default_factory=dict)

    _FIELDS = ("state_type", "definitions", "initialize", "stop",
               "retain_on_failed_init", "exports", "pin_snapshots")

    def __post_init__(self):
        if (not isinstance(self.state_type, str) or
                re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", self.state_type)
                is None):
            fail("Construction requires an explicit C state type")
        if not isinstance(self.initialize, str) or not self.initialize:
            fail("Construction requires an explicit initializer")
        if type(self.retain_on_failed_init) is not bool:
            fail("Construction requires boolean failed-init responsibility")
        for name in ("definitions", "stop", "exports"):
            values = tuple(getattr(self, name))
            if any(not isinstance(value, str) for value in values):
                fail(f"Construction {name} requires C source strings")
            object.__setattr__(self, name, values)
        snapshots = dict(self.pin_snapshots)
        if any(not isinstance(key, str) or not isinstance(value, str)
               for key, value in snapshots.items()):
            fail("Construction pin snapshots require explicit names")
        object.__setattr__(self, "pin_snapshots", MappingProxyType(snapshots))

    def __getitem__(self, key):
        if key not in self._FIELDS:
            raise KeyError(key)
        return getattr(self, key)

    def __iter__(self):
        return iter(self._FIELDS)

    def __len__(self):
        return len(self._FIELDS)


def owned_claims(result, owners):
    """Instance names never acquire Board/SoC reservations or kernel claims."""
    return (claim for claim in result["claims"]
            if claim.get("mode") not in {"reserved", "kernel"} and
            owners.intersection(claim["owner"].split(";")))


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
    ("timer_capture", "edge-capture"): ModeTraits(irq=True),
    ("advanced_pwm", "complementary-pwm"): ModeTraits(irq=True),
    ("eth", "raw-rmii"): ModeTraits(irq=True, bus_master=True),
    ("usb_device", "device-fs"): ModeTraits(irq=True),
    ("can", "classic-irq"): ModeTraits(irq=True),
    ("gpio", "input"): ModeTraits(),
    ("gpio", "output"): ModeTraits(),
    ("uart", "irq-byte-event"): ModeTraits(irq=True),
    ("uart", "irq-bytes"): ModeTraits(irq=True),
    ("uart", "irq-blocks"): ModeTraits(irq=True, block_stream=True),
    ("uart", "dma-tx"): ModeTraits(irq=True, tx_dma=True),
    ("uart", "rx-dma-idle"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True),
    ("uart", "dma-duplex"): ModeTraits(
        irq=True, block_stream=True, tx_dma=True, rx_dma=True),
    ("uart", "rx-dma-continuous"): ModeTraits(
        irq=True, block_stream=True, rx_dma=True, continuous=True),
    ("uart", "dma-duplex-continuous"): ModeTraits(
        irq=True, block_stream=True, tx_dma=True, rx_dma=True, continuous=True),
    ("spi", "short-poll"): ModeTraits(polling=True),
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
    ("exti", "edge-event"): ModeTraits(irq=True),
    ("pwm", "fixed-pwm"): ModeTraits(),
    ("adc", "single-shot"): ModeTraits(polling=True),
    ("adc", "low-rate-scan"): ModeTraits(polling=True),
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
    profile = result.get("kernel_profile")
    return ("NX_IRQ_KERNEL_" + profile["kernel_policy"].upper()
            if profile is not None else "NX_IRQ_KERNEL_NONE")


def integer(value, minimum=0, maximum=UINT32_MAX, context="integer"):
    if type(value) is not int or not minimum <= value <= maximum:
        fail(f"{context}: integer required in [{minimum}, {maximum}]")
    return value


def sequence(value, context):
    if not isinstance(value, list):
        fail(f"{context}: expected array")
    return value


def cpu_abi(cpu):
    """Capabilities are chip facts, never additional authored ABI choices."""
    return {key: cpu[key] for key in ("arch", "fpu", "float_abi")}


def validate_clock_profiles(soc, provider):
    """Fixed cold implementations accept only their exact reviewed clock tuple."""
    if provider.MODEL:
        return
    supported = provider.CLOCK_PROFILES
    actual = soc["clock_profiles"]
    if set(actual) != set(supported):
        fail("Clock profile identity differs from the fixed cold implementation")
    try:
        from .. import clocks
    except ImportError:
        import clocks
    for name, expected in supported.items():
        if clocks.normalize(actual[name]) != clocks.normalize(expected):
            fail(f"Clock profile differs from the fixed cold implementation: {name}")


def resolve_interrupts(cpu, irq, backend, provider):
    """Bind logical IRQ range and syscall policy to a maintained kernel port."""
    try:
        from .. import cpu as profiles
    except ImportError:
        import cpu as profiles
    if cpu_abi(cpu) != provider.CPU_ABI:
        fail("FreeRTOS kernel CPU ABI differs from the selected SoC")
    # Direct provider-contract tests may supply ABI assertions alone. Their
    # missing capabilities come from the exact maintained provider, never an
    # architecture-name guess; public CPU resolution requires every fact.
    facts = {**provider.CPU_FEATURES, **cpu}
    irq = {"external_count": provider.EXTERNAL_IRQ_COUNT, **irq}
    profile = profiles.resolve(facts, irq, backend, enum_abi=provider.ENUM_ABI)
    result = profile.to_dict()["irq"]
    if backend == "freertos" and result["kernel_port"] != provider.FREERTOS_PORT:
        fail(f"Unmaintained FreeRTOS port: {provider.FREERTOS_PORT}")
    return result


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


def validate_selection(selection, route, provider, ram_size):
    """One maintained mode has one bounded, explicit configuration contract."""
    kind = route["kind"]
    fields = {
        "timer_capture": {"tick_hz", "edge", "filter", "exclusive", "irq_priority", "calls_os"},
        "advanced_pwm": {"tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
                         "idle_high", "idle_complement_high", "exclusive", "irq_priority", "calls_os"},
        "eth": {"tx_count", "rx_count", "rx_storage_bytes", "address",
                "init_timeout_us", "drain_timeout_us", "irq_priority", "calls_os"},
        "usb_device": {"event_capacity", "rx_fifo_words", "vbus_sensing",
                       "poll_limit", "drain_timeout_us", "clock_timeout_us",
                       "irq_priority", "calls_os"},
        "can": {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
                "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"},
        "gpio": set(), "uart": {"baud", "rx_capacity", "rx_profile", "irq_priority", "calls_os"},
        "spi": {"max_hz", "irq_priority", "calls_os"}, "i2c": {"max_hz", "irq_priority", "calls_os"}, "flash": set(), "watchdog": set(),
        "exti": {"edge", "event_capacity", "irq_priority", "calls_os"},
        "pwm": {"period_ticks", "duty_ticks", "tick_hz"},
        "adc": {"channels", "sample_times", "reference_mv", "timeout_ms", "irq_priority", "calls_os"},
    }
    unknown = selection.keys() - {"id", "binding", "mode"} - fields.get(kind, set())
    if kind not in fields or unknown:
        fail(f"Unsupported {kind} configuration fields: {sorted(unknown)}")
    if kind in {"timer_capture", "advanced_pwm"}:
        try:
            from .. import timer_advanced
        except ImportError:
            import timer_advanced
        timer_advanced.validate_selection(selection, route, provider)
    elif kind == "eth":
        try:
            from .. import ethernet
        except ImportError:
            import ethernet
        ethernet.validate_selection(selection, route, provider, ram_size)
    elif kind == "usb_device":
        try:
            from .. import usb_device
        except ImportError:
            import usb_device
        usb_device.validate_selection(selection, route, provider, ram_size)
    elif kind == "can":
        try:
            from .. import can
        except ImportError:
            import can
        can.validate_selection(selection, provider, ram_size)
    elif kind == "uart":
        if not {"baud", "rx_profile", "irq_priority"}.issubset(selection):
            fail("UART requires baud, RX profile and IRQ priority")
        integer(selection["baud"], *provider.LIMITS["uart_baud"], "UART baud")
        if selection["rx_profile"] == "blocks":
            if not mode_traits(kind, selection["mode"]).block_stream:
                fail("UART RX blocks requires irq-blocks or a reviewed RX-DMA provider")
            if "rx_capacity" in selection:
                fail("UART RX blocks has no generated ring capacity; storage is caller-owned")
        elif mode_traits(kind, selection["mode"]).block_stream:
            fail("UART block provider requires RX blocks format")
        elif selection["rx_profile"] in {"bytes", "events"}:
            integer(selection.get("rx_capacity"), 1, min(4096, ram_size // 16), "UART RX capacity")
        else:
            fail("Unsupported UART receive profile")
    elif kind == "exti":
        if not {"edge", "event_capacity", "irq_priority"}.issubset(selection):
            fail("EXTI requires edge, event capacity and IRQ priority")
        if (type(selection["edge"]) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            fail("Unsupported EXTI edge")
        integer(selection["event_capacity"], 1, min(4096, ram_size // 24), "EXTI event capacity")
    elif kind == "pwm":
        if not {"period_ticks", "duty_ticks", "tick_hz"}.issubset(selection):
            fail("PWM requires explicit period, duty and tick_hz")
        integer(selection["period_ticks"], 1, provider.LIMITS["pwm_period"], "PWM period")
        integer(selection["duty_ticks"], 0, selection["period_ticks"], "PWM duty")
        integer(selection["tick_hz"], 1, context="PWM tick_hz")
    elif kind == "adc":
        if not {"channels", "sample_times", "reference_mv"}.issubset(selection):
            fail("ADC requires channels, sample_times and reference_mv")
        channels = sequence(selection["channels"], "ADC channels")
        expected = [int(pin["pin"][2:]) for pin in route["pins"]]
        if channels != expected or not channels or len(channels) > 16:
            fail("ADC channel sequence differs from reviewed analog pins")
        for channel in channels:
            integer(channel, 0, 15, "ADC channel")
        samples = sequence(selection["sample_times"], "ADC sample_times")
        if len(samples) != len(channels):
            fail("ADC sample_times must match the channel sequence")
        for sample in samples:
            integer(sample, 0, 7, "ADC sample encoding")
        integer(selection["reference_mv"], 1, 3600, "ADC reference_mv")
        if selection["mode"] in {"single-shot", "low-rate-scan"} and (selection["mode"] == "single-shot") != (len(channels) == 1):
            fail("ADC mode contradicts its reviewed channel count")
        if mode_traits(kind, selection["mode"]).dma and "irq_priority" not in selection:
            fail("ADC trigger-dma requires an explicit IRQ priority")
        if not mode_traits(kind, selection["mode"]).dma and ({"irq_priority", "calls_os"} & selection.keys()):
            fail("Polling ADC has no IRQ options")
    elif kind == "i2c":
        if selection.get("max_hz", 100000) != 100000:
            fail("Only reviewed 100 kHz I2C mode is maintained")
        if mode_traits(kind, selection["mode"]).irq:
            if "irq_priority" not in selection:
                fail("Asynchronous I2C requires an explicit IRQ priority")
        elif {"irq_priority", "calls_os"} & selection.keys():
            fail("Polling I2C has no IRQ options")
    provider.validate_selection(selection, route)


def controller_ir(item, family):
    """Accept a validated projection at direct model-constructor boundaries."""
    try:
        from ..ir import ControllerIR
    except ImportError:
        from ir import ControllerIR
    return (item if isinstance(item, ControllerIR) else
            ControllerIR.create(item, family))
