"""Immutable domain contracts assembled from explicit package facts.

This module chooses no MCU family, imports no family adapter and emits no C.
Its values reuse the existing narrow protocol contracts; package/route validation
and concrete C owners independently establish actual device and mode support.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.can import CanPolicy
from tools.configure.ethernet import EthernetPolicy
from tools.configure.providers.common import EndpointPolicy, fail, integer
from tools.configure.records import freeze
from tools.configure.stream_profiles import StreamGeometry
from tools.configure.timer_advanced import TimerBinding, TimerPolicy, TimerRoute
from tools.configure.uart_formats import UartFormatPolicy
from tools.configure.usb_device import UsbDevicePolicy


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Domain policy requires exact named fields")


def _boolean(value, name):
    if type(value) is not bool:
        fail("Domain " + name + " requires a boolean")
    return value


def _names(values, name, *, symbols=False, empty=True):
    if (not isinstance(values, (tuple, list)) or (not empty and not values) or
            any(not isinstance(value, str) or not value for value in values) or
            len(set(values)) != len(values)):
        fail("Domain " + name + " requires distinct explicit names")
    if symbols:
        for value in values:
            Symbol(value)
    return tuple(values)


def _pins(values, width):
    if not isinstance(values, (tuple, list)):
        fail("Domain pins require an ordered list")
    pins = []
    for value in values:
        if (not isinstance(value, (tuple, list)) or len(value) != width or
                any(not isinstance(item, str) or not item for item in value[:2])):
            fail("Domain pin requires wire/function and optional alternate")
        if width == 3:
            integer(value[2], 0, 15, "Domain alternate function")
        pins.append(tuple(value))
    if len(set(pins)) != len(pins):
        fail("Domain pin route contains duplicate signals")
    return tuple(pins)


def _uart(value):
    _fields(value, {"words", "irq_probe", "byte_frames"}, {"fifo"})
    frames = []
    if not isinstance(value["byte_frames"], (tuple, list)):
        fail("Domain UART byte frames require an ordered list")
    for frame in value["byte_frames"]:
        if (not isinstance(frame, (tuple, list)) or len(frame) != 2 or
                type(frame[0]) is not int or not 5 <= frame[0] <= 9 or
                type(frame[1]) is not str or
                frame[1] not in {"none", "even", "odd"}):
            fail("Domain UART byte frame requires reviewed payload/parity")
        frames.append(tuple(frame))
    if len(set(frames)) != len(frames):
        fail("Domain UART byte frames must be distinct")
    fifo = value.get("fifo")
    if fifo is not None:
        if not isinstance(fifo, (tuple, list)) or len(fifo) != 2:
            fail("Domain UART FIFO requires controller and exact capacity")
        Symbol(fifo[0])
        integer(fifo[1], 1, context="Domain UART FIFO capacity")
        fifo = tuple(fifo)
    return UartFormatPolicy(fifo, _names(value["words"], "UART word controllers",
        symbols=True), _boolean(value["irq_probe"], "UART IRQ probe"), tuple(frames))


def _can(value, soc):
    _fields(value, {"irq_roles", "fifo_order_supported"})
    roles = _names(value["irq_roles"], "CAN IRQ roles", empty=False)
    if roles != ("tx", "rx0", "rx1", "error"):
        fail("Domain CAN requires reviewed bxCAN IRQ role order")
    from tools.configure.can import domain_topology
    controllers, _ = domain_topology(soc)
    groups = ([soc["controllers"]] if "controllers" in soc else
              [group["controllers"] for group in soc.get("controller_groups", {}).values()])
    clocks = set()
    for name in controllers:
        selected = [group[name] for group in groups if name in group]
        if not selected or any(row["kind"] != "can" for row in selected):
            fail("Domain CAN controller requires its authored physical facts")
        clocks.update(integer(row.get("clock_hz"), 1, context="Domain CAN clock")
                      for row in selected)
    if len(clocks) != 1:
        fail("Domain CAN filter owners require one shared authored clock")
    return (controllers, clocks.pop(), CanPolicy(roles,
            _boolean(value["fifo_order_supported"], "CAN FIFO order")))


def _usb(value):
    _fields(value, {"controller", "irq", "clock_hz", "fifo_words",
                    "endpoint_numbers", "pins", "resources"})
    Symbol(value["controller"])
    return UsbDevicePolicy(value["controller"],
        integer(value["irq"], context="Domain USB IRQ"),
        integer(value["clock_hz"], 1, context="Domain USB clock"),
        integer(value["fifo_words"], 1, context="Domain USB FIFO words"),
        integer(value["endpoint_numbers"], 1, context="Domain USB endpoint numbers"),
        _pins(value["pins"], 3), _names(value["resources"], "USB resources"))


def _ethernet(value):
    _fields(value, {"controller", "irq", "clock_hz", "pins", "resources",
                    "alternate", "memory_region"}, {"mii_pins"})
    Symbol(value["controller"])
    Symbol(value["memory_region"])
    return EthernetPolicy(value["controller"],
        integer(value["irq"], context="Domain Ethernet IRQ"),
        integer(value["clock_hz"], 1, context="Domain Ethernet clock"),
        _pins(value["pins"], 2), _names(value["resources"], "Ethernet resources"),
        integer(value["alternate"], 0, 15, "Domain Ethernet alternate"),
        value["memory_region"], _pins(value.get("mii_pins", ()), 2))


def _timers(values):
    if not isinstance(values, (tuple, list)) or not values:
        fail("Domain timers require explicit bindings")
    bindings, keys = [], set()
    for value in values:
        _fields(value, {"kind", "controller", "vector", "irq", "clock_hz",
                        "routes", "resources"})
        if value["kind"] not in {"timer_capture", "advanced_pwm"}:
            fail("Domain timer kind has no maintained public contract")
        for name in ("controller", "vector"):
            Symbol(value[name])
        key = value["kind"], value["controller"]
        if key in keys:
            fail("Duplicate domain timer binding")
        keys.add(key)
        routes, connections = [], set()
        if not isinstance(value["routes"], (tuple, list)) or not value["routes"]:
            fail("Domain timer requires selected channel routes")
        for route in value["routes"]:
            _fields(route, {"channel", "pins"})
            channel = integer(route["channel"], 1, 4, "Domain timer channel")
            pins = _pins(route["pins"], 3)
            connection = channel, pins
            if connection in connections:
                fail("Duplicate domain timer channel route")
            connections.add(connection)
            routes.append(TimerRoute(channel, pins))
        bindings.append(TimerBinding(value["kind"], value["controller"],
            value["vector"], integer(value["irq"], context="Domain timer IRQ"),
            integer(value["clock_hz"], 1, context="Domain timer clock"),
            tuple(routes), _names(value["resources"], "timer resources")))
    return TimerPolicy(tuple(bindings))


def _endpoints(value):
    _fields(value, (), {"spi_transaction", "spi_dma", "model_async_echo",
                        "spi_lsb_first", "i2c_ten_bit", "spi_transaction_ops",
                        "spi_dma_owner"})
    result = {name: _boolean(item, "endpoint " + name)
              for name, item in value.items()
              if name not in {"spi_dma", "spi_dma_owner", "spi_transaction_ops"}}
    if "spi_transaction_ops" in value:
        result["spi_transaction_ops"] = Symbol(value["spi_transaction_ops"])
    if "spi_dma_owner" in value:
        owner = value["spi_dma_owner"]
        _fields(owner, {"contains", "attach_wake"})
        result["spi_dma_owner"] = (Symbol(owner["contains"]), Symbol(owner["attach_wake"]))
    dma = value.get("spi_dma")
    if dma is not None:
        _fields(dma, {"controllers", "mode"})
        if not isinstance(dma["mode"], str) or not dma["mode"]:
            fail("Domain SPI DMA requires one explicit mode")
        result["spi_dma"] = (_names(dma["controllers"], "SPI DMA controllers",
                                    symbols=True, empty=False), dma["mode"])
    return EndpointPolicy(**result)


def _capabilities(value):
    booleans = {"pin_electrical": "PIN_ELECTRICAL", "timebase_alarm": "TIMEBASE_ALARM",
                "spi_requires_endpoint": "SPI_REQUIRES_ENDPOINT"}
    positive = {"flash_program_unit": "FLASH_PROGRAM_UNIT",
                "flash_input_alignment": "FLASH_INPUT_ALIGNMENT",
                "offset_image_alignment": "OFFSET_IMAGE_ALIGNMENT"}
    _fields(value, (), {*booleans, *positive, "offset_image_vector_symbol",
                        "public_formats", "classic_can_mailboxes"})
    result = {booleans[name]: _boolean(item, name) if name in booleans else item
              for name, item in value.items() if name in booleans}
    for name, target in positive.items():
        if name in value:
            result[target] = integer(value[name], 1, context="Domain " + name)
    if "offset_image_vector_symbol" in value:
        Symbol(value["offset_image_vector_symbol"])
        result["OFFSET_IMAGE_VECTOR_SYMBOL"] = value["offset_image_vector_symbol"]
    if "public_formats" in value:
        result["PUBLIC_FORMATS"] = frozenset(_names(value["public_formats"], "public formats"))
    if "classic_can_mailboxes" in value:
        result["CLASSIC_CAN_MAILBOXES"] = integer(value["classic_can_mailboxes"], 1,
                                                 context="Classic CAN mailboxes")
    return result


def _dma(value):
    _fields(value, {"vector", "engines", "lanes", "requests"})
    Symbol(value["vector"])
    topology = []
    for name in ("engines", "lanes", "requests"):
        values = value[name]
        if not isinstance(values, (tuple, list)) or not values:
            fail("Domain DMA topology requires explicit numeric lanes")
        numbers = tuple(integer(item, context="Domain DMA " + name) for item in values)
        if len(set(numbers)) != len(numbers):
            fail("Duplicate domain DMA topology value")
        topology.append(numbers)
    return value["vector"], (frozenset(topology[0]), topology[1], topology[2])


@dataclass(frozen=True)
class DomainPolicies:
    """Selected protocol capabilities; absent facts grant no optional domain."""

    UART_FORMATS: UartFormatPolicy = field(default_factory=UartFormatPolicy)
    STREAM_GEOMETRY: StreamGeometry | None = None
    CAN_INTEGRATION: CanPolicy | None = None
    CAN_CONTROLLERS: tuple[str, ...] = ()
    CAN_CLOCK_HZ: int = 0
    USB_DEVICE: UsbDevicePolicy | None = None
    USB_CONTROLLER: str | None = None
    ETHERNET: EthernetPolicy | None = None
    ETH_CONTROLLER: str | None = None
    TIMER_ADVANCED: TimerPolicy | None = None
    ADC_GROUP: object | None = None
    CAPTURE_AUDIO: object | None = None
    EXTERNAL_BUS: object | None = None
    GPIO_PHASE: object | None = None
    SAI: object | None = None
    WAVEFORM: object | None = None
    SELECTION: object | None = None
    DAC_TRIGGERS: Mapping = field(default_factory=lambda: MappingProxyType({}))
    PLATFORM: object | None = None
    EXTERNAL_MEMORY: object | None = None
    PACKET: object | None = None
    HEADER: object | None = None
    PUBLIC_FORMATS: frozenset[str] = frozenset()
    CLASSIC_CAN_MAILBOXES: int | None = None
    ENDPOINT_POLICY: EndpointPolicy = field(default_factory=EndpointPolicy)
    LIMITS: Mapping = field(default_factory=lambda: MappingProxyType({}))
    PIN_ELECTRICAL: bool = False
    TIMEBASE_ALARM: bool = False
    SPI_REQUIRES_ENDPOINT: bool = True
    FLASH_PROGRAM_UNIT: int = 1
    FLASH_INPUT_ALIGNMENT: int = 1
    OFFSET_IMAGE_ALIGNMENT: int = 1
    OFFSET_IMAGE_VECTOR_SYMBOL: str | None = None
    DMA_VECTOR: str | None = None
    DMA_TOPOLOGY: tuple = (frozenset(), (), ())
    shared_irqs: Mapping = field(default_factory=lambda: MappingProxyType({}))
    clock: object | None = None

    @classmethod
    def from_validated(cls, package, soc, bindings_document):
        """Load explicit domain data; never infer behavior from a vendor name."""
        del package
        value = bindings_document.get("domains")
        if value is None:
            return cls()
        _fields(value, {"schema_version"}, {"uart_formats", "stream_geometry",
            "can", "usb_device", "ethernet", "timers", "endpoints", "limits",
            "capabilities", "dma", "shared_irqs", "clock", "adc_group",
            "capture_audio", "external_bus", "gpio_phase", "sai", "waveform",
            "selection", "platform", "startup_memory", "packet", "headers",
            "dac_triggers"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported domain policy schema")
        result = {}
        if "uart_formats" in value:
            result["UART_FORMATS"] = _uart(value["uart_formats"])
        if "stream_geometry" in value:
            geometry = value["stream_geometry"]
            _fields(geometry, StreamGeometry.__dataclass_fields__)
            result["STREAM_GEOMETRY"] = StreamGeometry(**{
                name: integer(item, 0 if name == "spare_blocks" else 1,
                              context="Domain stream " + name)
                for name, item in geometry.items()})
        if "can" in value:
            controllers, frequency, policy = _can(value["can"], soc)
            result.update(CAN_CONTROLLERS=controllers, CAN_CLOCK_HZ=frequency,
                          CAN_INTEGRATION=policy)
        if "usb_device" in value:
            result["USB_DEVICE"] = _usb(value["usb_device"])
            result["USB_CONTROLLER"] = result["USB_DEVICE"].controller
        if "ethernet" in value:
            result["ETHERNET"] = _ethernet(value["ethernet"])
            result["ETH_CONTROLLER"] = result["ETHERNET"].controller
        if "timers" in value:
            result["TIMER_ADVANCED"] = _timers(value["timers"])
        if "endpoints" in value:
            result["ENDPOINT_POLICY"] = _endpoints(value["endpoints"])
        if "limits" in value:
            limits = value["limits"]
            if not isinstance(limits, Mapping):
                fail("Domain limits require named numeric facts")
            for name, bound in limits.items():
                Symbol(name)
                for item in bound if isinstance(bound, (list, tuple)) else (bound,):
                    integer(item, context="Domain " + name)
            result["LIMITS"] = freeze(limits)
        if "capabilities" in value:
            result.update(_capabilities(value["capabilities"]))
        if "dma" in value:
            result["DMA_VECTOR"], result["DMA_TOPOLOGY"] = _dma(value["dma"])
        shared = value.get("shared_irqs", {})
        if not isinstance(shared, Mapping):
            fail("Shared IRQ facts require a domain mapping")
        result["shared_irqs"] = MappingProxyType({
            name: _names(items, "shared IRQ resources") for name, items in shared.items()})
        if "clock" in value:
            from tools.configure.domains.pll_mnpq import PllMnpqPolicy
            result["clock"] = PllMnpqPolicy.from_validated(value["clock"])
        if "adc_group" in value:
            from tools.configure.policies.adc_group import AdcGroupPolicy
            result["ADC_GROUP"] = AdcGroupPolicy.from_validated(
                value["adc_group"], bindings_document.get("hardware", {}))
        if "capture_audio" in value:
            from tools.configure.policies.capture_audio import CaptureAudioPolicy
            result["CAPTURE_AUDIO"] = CaptureAudioPolicy.from_validated(value["capture_audio"])
        if "external_bus" in value:
            from tools.configure.policies.external_bus import ExternalBusPolicy
            result["EXTERNAL_BUS"] = ExternalBusPolicy.from_validated(value["external_bus"])
        if "gpio_phase" in value:
            from tools.configure.policies.gpio_phase import GpioPhasePolicy
            result["GPIO_PHASE"] = GpioPhasePolicy.from_validated(value["gpio_phase"])
        if "selection" in value:
            from tools.configure.policies.selection import SelectionPolicy
            result["SELECTION"] = SelectionPolicy.from_validated(value["selection"])
        if "dac_triggers" in value:
            from tools.configure import dac
            result["DAC_TRIGGERS"] = dac.trigger_facts(value["dac_triggers"])
        if "waveform" in value:
            from tools.configure.policies.timer_waveform import TimerWaveformPolicy
            result["WAVEFORM"] = TimerWaveformPolicy.from_validated(value["waveform"])
        if "sai" in value:
            from tools.configure.policies.sai import SaiPolicy
            result["SAI"] = SaiPolicy.from_validated(value["sai"])
        if "platform" in value:
            from tools.configure.platform_bindings import PlatformBindings
            result["PLATFORM"] = PlatformBindings.from_validated(value["platform"])
        if "startup_memory" in value:
            from tools.configure.startup_memory import StartupMemoryPolicy
            result["EXTERNAL_MEMORY"] = StartupMemoryPolicy.from_validated(
                value["startup_memory"], external_bus=result.get("EXTERNAL_BUS"))
        if "packet" in value:
            from tools.configure.policies.packet import PacketPolicy
            result["PACKET"] = PacketPolicy.from_validated(value["packet"])
        if "headers" in value:
            from tools.configure.header_projection import HeaderProjection
            result["HEADER"] = HeaderProjection.from_validated(value["headers"])
        return cls(**result)

    def shares_irq(self, kind, key):
        return key in self.shared_irqs.get(kind, ())

    def binding_values(self, item, configuration=None):
        """Derive semantic scalars from the same admitted domain facts."""
        values = {}
        if item.kind == "can" and item.controller in self.CAN_CONTROLLERS:
            values["controller_index"] = self.CAN_CONTROLLERS.index(item.controller)
        if item.kind in {"timer_capture", "advanced_pwm"}:
            binding = (self.TIMER_ADVANCED.binding(item.kind, item.controller)
                       if self.TIMER_ADVANCED is not None else None)
            pins = tuple((pin["pin"], pin["function"], pin["af"]) for pin in item.pins)
            route = binding.route(pins) if binding is not None else None
            if route is None:
                fail("Selected timer has no admitted domain channel route")
            values["channel"] = route.channel
        if item.kind == "pwm":
            if configuration is None or len(item.pins) != 1:
                fail("Selected PWM channel requires its physical binding context")
            provider = configuration.provider_context.provider
            channel = provider.PWM_PIN_CHANNELS.get((item.controller, item.pins[0]["pin"]))
            if channel is None:
                fail("Selected PWM route has no reviewed channel")
            values["channel"] = channel
        if item.kind == "exti" and self.GPIO_PHASE is not None:
            mask = self.GPIO_PHASE.exti_lines.get(item.controller)
            if mask is not None:
                values["exti_mask"] = mask
        if item.kind == "spi" and item.mode != "irq-frames":
            from tools.configure.spi_connections import binding_values
            values["spi"] = binding_values(item, configuration)
        for name in ("ADC_GROUP", "CAPTURE_AUDIO", "SAI", "WAVEFORM", "EXTERNAL_BUS", "PACKET"):
            policy = getattr(self, name)
            project = getattr(policy, "binding_values", None)
            if project is not None:
                result = project(item, configuration=configuration)
                if result is not None:
                    if not isinstance(result, Mapping):
                        fail("Private domain binding requires named immutable values")
                    for key, value in result.items():
                        Symbol(key)
                        if key in values:
                            fail("Private domain binding identity is duplicated: " + key)
                        values[key] = value
        return freeze(values)

    def execution_profile(self, item, children):
        """Select one finite layout from public semantics, never a predicate VM."""
        if item.kind == "uart":
            from tools.configure.uart_formats import execution_profile
            return execution_profile(item)
        if item.kind == "spi":
            return ("sequence" if item.interface_kind == "spi_dma_transaction"
                    else "default")
        if self.EXTERNAL_BUS is not None:
            return self.EXTERNAL_BUS.endpoint_profile(item, children)
        return "default"
