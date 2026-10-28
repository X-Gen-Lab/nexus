"""Immutable, typed assembly decisions consumed by the C emitters.

JSON hardware facts and authored inputs are validated once by the resolver. This
module stores that decision; it does not select targets or repair configuration.
"""
from collections.abc import Mapping
from dataclasses import dataclass, fields, is_dataclass, replace
from types import MappingProxyType
from typing import ClassVar, TYPE_CHECKING

from tools.configure.resource_graph import ResourceClaimIR, ResourcePlanIR
from tools.configure.trigger_graph import TriggerPlanIR
from tools.configure.dma_topology import DmaLaneIR, DmaTopologyIR
from tools.configure.i2s2 import I2s2RoleIR
if TYPE_CHECKING:
    from tools.configure.providers.hardware import ControllerBinding


def freeze(value):
    if isinstance(value, dict):
        return MappingProxyType({key: freeze(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(freeze(item) for item in value)
    return value


def thaw(value):
    if isinstance(value, Record):
        return {key: thaw(item) for key, item in value.projection().items()}
    if is_dataclass(value):
        return {item.name: thaw(getattr(value, item.name))
                for item in fields(value)}
    if isinstance(value, Mapping):
        return {key: thaw(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [thaw(item) for item in value]
    return value


class Record:
    """Derived read-only projection; named fields are the sole stored values."""
    def projection(self):
        return {item.name: getattr(self, item.name) for item in fields(self)}

    def to_dict(self):
        return thaw(self)


@dataclass(frozen=True)
class IrqOptions:
    priority: int
    calls_os: bool
    probe: bool = False


@dataclass(frozen=True)
class InterruptProfileIR:
    priority_bits: int
    maximum_priority: int
    syscall_priority: int | None
    kernel_port: str | None
    external_count: int
    mask_kind: str


@dataclass(frozen=True)
class CpuProfileIR(Record):
    enum_abi: str
    backend: str
    arch_macro: str
    has_basepri: bool
    atomic_backend: str
    irq: InterruptProfileIR
    compile_options: tuple[str, ...]
    definitions: Mapping
    cpu: Mapping

    @property
    def arch(self) -> str:
        return self.cpu["arch"]

    @property
    def fpu(self) -> str:
        return self.cpu["fpu"]

    @property
    def float_abi(self) -> str:
        return self.cpu["float_abi"]

    def projection(self):
        return {**super().projection(), "arch": self.arch, "fpu": self.fpu,
                "float_abi": self.float_abi}

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "enum_abi", "backend", "arch_macro",
            "has_basepri", "atomic_backend")),
                   InterruptProfileIR(**record["irq"]),
                   tuple(record["compile_options"]),
                   freeze(record["definitions"]), freeze(record["cpu"]))

    def to_dict(self):
        return thaw(self)


@dataclass(frozen=True)
class KernelProfileIR(Record):
    profile: str
    tick_source: str
    tick_hz: int
    tick_reload: int | None
    max_priorities: int
    syscall_priority: int
    kernel_policy: str
    kernel_port: str
    memory_protection: bool
    user_io: str
    system_call_stack_words: int
    security_model: str
    secure_idle_stack_bytes: int
    idle_stack_words: int
    max_task_name_len: int
    mutexes: bool
    counting_semaphores: bool
    task_notifications: bool
    notification_slots: int
    trace: bool
    runtime_stats: bool
    tickless: bool
    lowpower_port: str
    min_stack_words: int
    stack_alignment_bytes: int
    definitions: Mapping

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "profile", "tick_source", "tick_hz", "tick_reload", "max_priorities",
            "syscall_priority", "kernel_policy", "kernel_port",
            "memory_protection", "user_io", "system_call_stack_words", "security_model",
            "secure_idle_stack_bytes",
            "idle_stack_words",
            "max_task_name_len", "mutexes", "counting_semaphores",
            "task_notifications", "notification_slots", "trace", "runtime_stats",
            "tickless", "lowpower_port", "min_stack_words", "stack_alignment_bytes")),
                   freeze(record["definitions"]))

    def to_dict(self):
        return thaw(self)


@dataclass(frozen=True)
class ThreadXKernelProfileIR(Record):
    """Immutable ThreadX decisions, independent of FreeRTOS policy fields."""
    backend: str
    profile: str
    tick_source: str
    tick_hz: int
    tick_reload: int | None
    max_priorities: int
    syscall_priority: int
    interrupt_mask: str
    kernel_policy: str
    kernel_port: str
    timer_process_in_isr: bool
    min_stack_bytes: int
    stack_alignment_bytes: int
    definitions: Mapping

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "backend", "profile", "tick_source", "tick_hz", "tick_reload",
            "max_priorities", "syscall_priority", "interrupt_mask",
            "kernel_policy", "kernel_port", "timer_process_in_isr",
            "min_stack_bytes", "stack_alignment_bytes")),
                   freeze(record["definitions"]))

    def to_dict(self):
        return thaw(self)


@dataclass(frozen=True)
class GpioOptions:
    mask: int
    initial: int


@dataclass(frozen=True)
class PinStateValueOptions:
    mode: int
    output_type: int
    speed: int
    pull: int
    levels: int


@dataclass(frozen=True)
class PinStateOptions:
    mask: int
    default: PinStateValueOptions
    sleep: PinStateValueOptions


@dataclass(frozen=True)
class UartFrameOptions:
    data_bits: int
    parity: str
    stop_bits: int
    flow: str


@dataclass(frozen=True)
class StreamProfileIR(Record):
    max_units_per_second: int
    units_per_block: int
    blocks: int
    release_delay_us: int
    target_delay_us: int
    hardware_targets: int
    spare_blocks: int
    unit_bytes: int
    counter_units_per_block: int
    retained_blocks: int
    required_blocks: int
    required_payload_bytes: int
    configured_payload_bytes: int
    block_fill_us: int
    producer_control: str
    provenance: str
    physical_qualified: bool
    source: str
    on_exhaustion: str

    @classmethod
    def from_validated(cls, values):
        return cls(**{item.name: values[item.name] for item in fields(cls)})


@dataclass(frozen=True)
class UartFifoOptions:
    rx_threshold: str
    tx_threshold: str
    rx_budget_bytes: int
    tx_budget_bytes: int


@dataclass(frozen=True)
class UartOptions:
    baud: int
    rx_profile: str
    rx_capacity: int | None
    irq: IrqOptions
    frame: UartFrameOptions | None = None
    stream_profile: StreamProfileIR | None = None
    fifo: UartFifoOptions | None = None
    dma_cache_line_bytes: int = 0


@dataclass(frozen=True)
class SpiOptions:
    max_hz: int
    irq: IrqOptions | None
    dma_cache_line_bytes: int = 0


@dataclass(frozen=True)
class I2cTimingSelectionIR(Record):
    timingr: int
    cr1_filter_bits: int
    provenance: str
    physical_qualified: bool
    source: str
    bus_hz_min: int
    bus_hz_max: int
    low_ns_min: int
    high_ns_min: int
    scldel_ns_min: int
    sdadel_ns_min: int
    sdadel_ns_max: int
    time_unit: str
    rate_scope: str
    reference: str
    assumptions: Mapping

@dataclass(frozen=True)
class I2cOptions:
    max_hz: int
    irq: IrqOptions | None
    timing: I2cTimingSelectionIR | None = None


@dataclass(frozen=True)
class ExtiOptions:
    edge: str
    event_capacity: int
    irq: IrqOptions


@dataclass(frozen=True)
class EncoderOptions:
    counting: str
    filter_a: int
    filter_b: int
    invert_a: bool
    invert_b: bool
    exclusive: bool


@dataclass(frozen=True)
class PwmGroupOptions:
    tick_hz: int
    period_ticks: int
    deadtime_ticks: int
    exclusive: bool
    irq: IrqOptions


@dataclass(frozen=True)
class PwmOptions:
    period_ticks: int
    duty_ticks: int
    tick_hz: int


@dataclass(frozen=True)
class PwmSharedChannelIR:
    id: str
    channel: int
    pin: str
    duty_ticks: int
    inactive: int


@dataclass(frozen=True)
class PwmSharedOwnerIR:
    provider: str
    controller: str
    mode: str
    kernel_hz: int
    tick_hz: int
    period_ticks: int
    members: tuple[PwmSharedChannelIR, ...]

    @classmethod
    def from_validated(cls, values):
        return cls(*(values[key] for key in (
            "provider", "controller", "mode", "kernel_hz", "tick_hz", "period_ticks")),
                   tuple(PwmSharedChannelIR(**item) for item in values["members"]))


@dataclass(frozen=True)
class AdcOptions:
    channels: tuple[int, ...]
    sample_times: tuple[int, ...]
    reference_mv: int
    timeout_ms: int | None
    irq: IrqOptions | None
    stream_profile: StreamProfileIR | None = None


@dataclass(frozen=True)
class SiliconRevisionRuleIR:
    revision_id: int
    required_workarounds: int


@dataclass(frozen=True)
class SiliconAdmissionIR:
    """Named software expectation; hardware is read only at cold admission."""
    profile: str
    provider: str
    part: str
    device_id: int
    flash_bytes: int
    rules: tuple[SiliconRevisionRuleIR, ...]
    applied_workarounds: int
    review_sha256: str
    review_scope: str
    unknown_revision: str = "reject"
    physical_qualified: bool = False

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[key] for key in (
            "profile", "provider", "part", "device_id", "flash_bytes")),
            tuple(SiliconRevisionRuleIR(**rule) for rule in record["rules"]),
            *(record[key] for key in ("applied_workarounds", "review_sha256",
                                     "review_scope")))


@dataclass(frozen=True)
class AdcWatchdogOptions:
    lower: int
    upper: int


@dataclass(frozen=True)
class AdcGroupOptions:
    channels: tuple[int, ...]
    sample_times: tuple[int, ...]
    reference_mv: int
    exclusive: bool
    silicon_profile: str
    trigger: str
    period_us: int
    watchdog: AdcWatchdogOptions | None
    irq: IrqOptions
    rank_repeat: int
    sample_stride: int
    scan_stride: int
    max_frames: int
    adc_clock_hz: int
    hclk_hz: int
    trigger_identity: int
    admission: SiliconAdmissionIR


@dataclass(frozen=True)
class TimerCaptureOptions:
    tick_hz: int
    edge: str
    filter: int
    exclusive: bool
    irq: IrqOptions


@dataclass(frozen=True)
class AdvancedPwmOptions:
    tick_hz: int
    period_ticks: int
    deadtime_ticks: int
    break_active_high: bool
    idle_high: bool
    idle_complement_high: bool
    exclusive: bool
    irq: IrqOptions


@dataclass(frozen=True)
class EthernetOptions:
    tx_count: int
    rx_count: int
    rx_storage_bytes: int
    address: tuple[int, ...]
    init_timeout_us: int
    drain_timeout_us: int
    irq: IrqOptions


@dataclass(frozen=True)
class EthernetFramesOptions(EthernetOptions):
    """One selected MAC counter; domain identity is never authored or measured."""
    clock_domain: int


@dataclass(frozen=True)
class UsbDeviceOptions:
    event_capacity: int
    rx_fifo_words: int
    vbus_sensing: bool
    poll_limit: int
    drain_timeout_us: int
    clock_timeout_us: int
    irq: IrqOptions


@dataclass(frozen=True)
class UsbEndpointOptions:
    index: int
    number: int
    direction: str
    transfer: str
    max_packet: int
    fifo_start: int
    fifo_words: int


@dataclass(frozen=True)
class CanTimingOptions:
    prescaler: int
    segment1: int
    segment2: int
    sjw: int


@dataclass(frozen=True)
class CanOptions:
    operation: str
    event_capacity: int
    bitrate: int
    timing: CanTimingOptions
    irq: IrqOptions
    drain_timeout_us: int
    poll_limit: int
    fifo_order: bool


@dataclass(frozen=True)
class CanFilterIR:
    controller: str
    controller_index: int
    bank: int
    id: int
    mask: int
    format: str
    fifo: int
    remote: bool
    match_remote: bool


@dataclass(frozen=True)
class CanDomainIR:
    split: int
    filters: tuple[CanFilterIR, ...]


@dataclass(frozen=True)
class LptimOptions:
    """Fixed declared source units, independent of the Core deadline clock."""
    tick_hz: int
    irq: IrqOptions | None = None


@dataclass(frozen=True)
class NoOptions:
    """Flash, watchdog and raw RTC have no authored construction options."""


@dataclass(frozen=True)
class PwmAdcOptions:
    tick_hz: int
    period_ticks: int
    trigger_ticks: int
    deadtime_ticks: int
    channels: tuple[int, ...]
    sample_times: tuple[int, ...]
    reference_mv: int
    exclusive: bool
    irq: IrqOptions


@dataclass(frozen=True)
class CanFdFilterOptions:
    id: int
    mask: int
    format: str
    fifo: int


@dataclass(frozen=True)
class CanFdOptions:
    bitrate: int
    data_bitrate: int
    event_capacity: int
    filters: tuple[CanFdFilterOptions, ...]
    drain_timeout_us: int
    poll_limit: int
    exclusive: bool
    message_ram_words: int
    irq: IrqOptions


ControllerOptions = (GpioOptions | PinStateOptions | UartOptions | SpiOptions | I2cOptions |
                     ExtiOptions | PwmOptions | PwmGroupOptions | EncoderOptions | AdcOptions | CanOptions |
                     UsbDeviceOptions | EthernetOptions | EthernetFramesOptions |
                     TimerCaptureOptions |
                     AdvancedPwmOptions | PwmAdcOptions | CanFdOptions |
                     AdcGroupOptions | LptimOptions | NoOptions)


def mode_options(values):
    """Project one validated mode into immutable named constructor arguments."""
    kind = values["kind"]
    irq = (IrqOptions(values["irq_priority"], values.get("calls_os", False),
                      values.get("irq_probe", False))
           if "irq_priority" in values else None)
    if kind == "adc_group":
        plan = values["adc_group_resolution"]
        return AdcGroupOptions(
            tuple(values["channels"]), tuple(values["sample_times"]),
            values["reference_mv"], values["exclusive"],
            values["silicon_profile"], values["trigger"],
            values.get("period_us", 0),
            (AdcWatchdogOptions(**values["watchdog"])
             if "watchdog" in values else None), irq,
            *(plan[key] for key in (
                "rank_repeat", "sample_stride", "scan_stride", "max_frames",
                "adc_clock_hz", "hclk_hz", "trigger_identity")),
            SiliconAdmissionIR.from_validated(plan["admission"]))
    if kind == "pwm_adc":
        return PwmAdcOptions(*(values[key] for key in (
            "tick_hz", "period_ticks", "trigger_ticks", "deadtime_ticks")),
            tuple(values["channels"]), tuple(values["sample_times"]),
            values["reference_mv"], values["exclusive"], irq)
    if kind == "can_fd":
        return CanFdOptions(values["bitrate"], values["data_bitrate"],
                            values["event_capacity"],
                            tuple(CanFdFilterOptions(**value) for value in values["filters"]),
                            *(values[key] for key in (
                                "drain_timeout_us", "poll_limit", "exclusive",
                                "message_ram_words")), irq)
    if kind == "can":
        return CanOptions(values["operation"], values["event_capacity"],
                          values["bitrate"], CanTimingOptions(**values["timing"]),
                          irq, values["drain_timeout_us"], values["poll_limit"],
                          values["fifo_order"])
    if kind == "usb_device":
        return UsbDeviceOptions(*(values[key] for key in (
            "event_capacity", "rx_fifo_words", "vbus_sensing", "poll_limit",
            "drain_timeout_us", "clock_timeout_us")), irq)
    if kind == "encoder":
        return EncoderOptions(*(values[key] for key in (
            "counting", "filter_a", "filter_b", "invert_a", "invert_b", "exclusive")))
    if kind == "pwm_group":
        return PwmGroupOptions(*(values[key] for key in (
            "tick_hz", "period_ticks", "deadtime_ticks", "exclusive")), irq)
    if kind == "timer_capture":
        return TimerCaptureOptions(*(values[key] for key in (
            "tick_hz", "edge", "filter", "exclusive")), irq)
    if kind == "advanced_pwm":
        return AdvancedPwmOptions(*(values[key] for key in (
            "tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
            "idle_high", "idle_complement_high", "exclusive")), irq)
    if kind in {"eth", "eth_frames"}:
        arguments = (values["tx_count"], values["rx_count"],
                     values["rx_storage_bytes"], tuple(values["address"]),
                     values["init_timeout_us"], values["drain_timeout_us"], irq)
        return (EthernetFramesOptions(*arguments, values["clock_domain"])
                if kind == "eth_frames" else EthernetOptions(*arguments))
    if kind == "gpio":
        if values["mode"] == "named-states":
            states = values["pin_state_resolution"]
            return PinStateOptions(values["mask"],
                                   PinStateValueOptions(**states["default"]),
                                   PinStateValueOptions(**states["sleep"]))
        return GpioOptions(values["mask"], values["initial"])
    if kind == "uart":
        return UartOptions(values["baud"], values["rx_profile"],
                           values.get("rx_capacity"), irq,
                           UartFrameOptions(**values["frame"])
                           if "frame" in values else None,
                           StreamProfileIR.from_validated(
                               values["stream_resolution"])
                           if "stream_resolution" in values else None,
                           UartFifoOptions(**values["fifo"])
                           if "fifo" in values else None,
                           values.get("dma_cache_line_bytes", 0))
    if kind == "spi":
        return SpiOptions(values.get("max_hz", 1000000), irq,
                          values.get("dma_cache_line_bytes", 0))
    if kind == "i2c":
        return I2cOptions(values.get("max_hz", 100000), irq,
                         values.get("timing_resolution"))
    if kind == "exti":
        return ExtiOptions(values["edge"], values["event_capacity"], irq)
    if kind == "pwm":
        return PwmOptions(values["period_ticks"], values["duty_ticks"],
                          values["tick_hz"])
    if kind == "adc":
        return AdcOptions(tuple(values["channels"]),
                          tuple(values["sample_times"]), values["reference_mv"],
                          values.get("timeout_ms"), irq,
                          StreamProfileIR.from_validated(
                              values["stream_resolution"])
                          if "stream_resolution" in values else None)
    if kind == "lptim":
        return LptimOptions(values["tick_hz"], irq)
    if kind in {"flash", "watchdog", "rtc"}:
        return NoOptions()
    raise ValueError(f"Unmaintained controller decision: {kind}")


def option_projection(options):
    """Export selected options without retaining a parallel input dictionary."""
    result = {item.name: getattr(options, item.name) for item in fields(options)}
    irq = result.pop("irq", None)
    if irq is not None:
        result.update(irq_priority=irq.priority, calls_os=irq.calls_os,
                      irq_probe=irq.probe)
    stream = result.pop("stream_profile", None)
    if stream is not None:
        result["stream_resolution"] = stream
    return result


@dataclass(frozen=True)
class ControllerIR(Record):
    id: str
    kind: str
    controller: str
    binding: str
    mode: str
    provider: str
    options: ControllerOptions
    pins: tuple[Mapping, ...]
    route: str
    dma: tuple[str, ...]
    can_irqs: tuple[str, ...]
    source_fields: tuple[str, ...]
    route_mask: int | None = None
    route_initial: int | None = None
    stream_fields: tuple[str, ...] = ()
    interface_kind: str | None = None
    integration: "ControllerIntegrationIR | None" = None

    @property
    def mask(self):
        return (self.route_mask if self.route_mask is not None
                else self.options.mask)

    @property
    def initial(self):
        return (self.route_initial if self.route_initial is not None
                else self.options.initial)

    def projection(self):
        result = {name: getattr(self, name) for name in (
            "id", "kind", "controller", "binding", "mode", "pins", "route",
            "dma", "can_irqs", "interface_kind", "mask", "initial")}
        result.update(option_projection(self.options))
        if isinstance(self.options, PinStateOptions):
            result["pin_state_resolution"] = {
                "default": self.options.default, "sleep": self.options.sleep}
        if isinstance(self.options, I2cOptions) and self.options.timing is not None:
            timing = self.options.timing
            result["timing_resolution"] = timing
            result["timing"] = {key: timing.assumptions[key] for key in (
                "kernel_hz_min", "kernel_hz_max", "pclk_hz_min", "bus_hz_min",
                "rise_ns_max", "fall_ns_max")}
            result["timing"]["source"] = timing.source
        stream = getattr(self.options, "stream_profile", None)
        if stream is not None:
            result["stream"] = {name: getattr(stream, name)
                                for name in self.stream_fields}
        if isinstance(self.options, AdcGroupOptions):
            result["adc_group_resolution"] = {name: getattr(self.options, name)
                for name in ("rank_repeat", "sample_stride", "scan_stride",
                    "max_frames", "adc_clock_hz", "hclk_hz", "trigger_identity",
                    "admission")}
        return {name: result[name] for name in self.source_fields}

    @classmethod
    def create(cls, values, provider):
        options = mode_options(values)
        result = cls(*(values[key] for key in
                     ("id", "kind", "controller", "binding", "mode")),
                     provider, options, freeze(values["pins"]), values["route"],
                     tuple(values.get("dma", ())),
                     tuple(values.get("can_irqs", ())), tuple(values),
                     route_mask=(None if hasattr(options, "mask") else values["mask"]),
                     route_initial=(None if hasattr(options, "initial") else values["initial"]),
                     stream_fields=tuple(values.get("stream", ())),
                     interface_kind=values.get("interface_kind", values["kind"]))
        # The projection is deliberately exhaustive: a new resolved field must
        # acquire a typed owner instead of silently disappearing into metadata.
        result.projection()
        return result


@dataclass(frozen=True)
class SpiEndpointOptions:
    mode: int
    max_hz: int
    cs_binding: str | None
    model_bytes: int | None


@dataclass(frozen=True)
class SpiTransactionEndpointOptions:
    mode: int
    max_hz: int
    cs_binding: str | None
    model_bytes: int | None


@dataclass(frozen=True)
class SpiDMATransactionEndpointOptions:
    mode: int
    max_hz: int
    cs_binding: str | None
    model_bytes: int | None


@dataclass(frozen=True)
class I2cEndpointOptions:
    address: int
    model_bytes: int | None


@dataclass(frozen=True)
class I2cLongEndpointOptions:
    address: int
    model_bytes: int | None


@dataclass(frozen=True)
class EndpointIR(Record):
    id: str
    controller: str
    driver: str
    kind: str
    options: (SpiEndpointOptions | SpiTransactionEndpointOptions |
              SpiDMATransactionEndpointOptions |
              I2cEndpointOptions | I2cLongEndpointOptions | UsbEndpointOptions)
    source_fields: tuple[str, ...]

    def projection(self):
        result = {"id": self.id, "controller": self.controller,
                  "driver": self.driver, **option_projection(self.options)}
        if isinstance(self.options, UsbEndpointOptions):
            result["endpoint_index"] = self.options.index
        return {name: result[name] for name in self.source_fields}

    @classmethod
    def create(cls, values, controllers):
        controller = next(item for item in controllers
                          if item.id == values["controller"])
        kind = controller.kind
        if kind == "spi":
            option_type = {"spi-transaction": SpiTransactionEndpointOptions,
                           "spi-dma-transaction":
                               SpiDMATransactionEndpointOptions}.get(
                                   values["driver"], SpiEndpointOptions)
            options = option_type(values.get("mode", 0),
                values.get("max_hz", controller.options.max_hz),
                values.get("cs_binding"), values.get("model_bytes"))
        elif kind == "i2c":
            option_type = (I2cLongEndpointOptions
                           if values["driver"] == "i2c-long-endpoint"
                           else I2cEndpointOptions)
            options = option_type(values["address"], values.get("model_bytes"))
        elif kind == "usb_device":
            options = UsbEndpointOptions(values["endpoint_index"], *(values[key]
                for key in ("number", "direction", "transfer", "max_packet",
                            "fifo_start", "fifo_words")))
        else:
            raise ValueError(f"Unmaintained endpoint decision: {kind}")
        return cls(values["id"], values["controller"], values["driver"],
                   kind, options, tuple(values))


@dataclass(frozen=True)
class MemoryRegionBudgetIR:
    id: str
    limit_bytes: int


@dataclass(frozen=True)
class MpuMemoryReservationsIR:
    privileged_flash_bytes: int
    user_flash_bytes: int
    syscall_flash_bytes: int
    privileged_ram_bytes: int
    user_ram_bytes: int


@dataclass(frozen=True)
class MemoryBudgetIR:
    main_stack_bytes: int
    libc_heap_bytes: int
    static_ram_limit_bytes: int | None
    flash_load_limit_bytes: int | None
    regions: tuple[MemoryRegionBudgetIR, ...] = ()
    mpu: MpuMemoryReservationsIR | None = None

    def projection(self):
        result = {"main_stack_bytes": self.main_stack_bytes,
                  "libc_heap_bytes": self.libc_heap_bytes}
        for key, value in (("static_ram_bytes", self.static_ram_limit_bytes),
                           ("flash_load_bytes", self.flash_load_limit_bytes)):
            if value is not None:
                result[key] = value
        if self.regions:
            result["regions"] = {item.id: {"limit_bytes": item.limit_bytes}
                                 for item in self.regions}
        if self.mpu is not None:
            result["mpu"] = thaw(self.mpu)
        return result


@dataclass(frozen=True)
class FactoryOptions:
    capabilities: bool


@dataclass(frozen=True)
class ClockDomainIR:
    """A reviewed source envelope, separate from board measurement."""
    source: str
    nominal_hz: int
    minimum_hz: int
    maximum_hz: int


@dataclass(frozen=True)
class ClockPlanIR:
    """Selected-only cold clock facts; no public runtime clock manager."""
    profile: str
    input: ClockDomainIR
    domains: Mapping
    reviewed_revision_ids: tuple[int, ...]
    provenance: str
    physical_qualified: bool

    @classmethod
    def from_validated(cls, record):
        return cls(record["profile"], ClockDomainIR(**record["input"]),
                   MappingProxyType({name: ClockDomainIR(**value)
                                     for name, value in record["domains"].items()}),
                   tuple(record["reviewed_revision_ids"]),
                   record["provenance"], record["physical_qualified"])


@dataclass(frozen=True)
class PhysicalPartIR:
    """Views of validated physical facts; no second geometry authority."""

    part: str
    package: str
    flash: Mapping
    memory: tuple[Mapping, ...]


@dataclass(frozen=True)
class ControllerIntegrationIR:
    """One selected owner composed from the already validated immutable plans.

    Clock Hz is a nominal APB input where the exact hardware binding names it.
    Only clock_plan can establish reviewed domain bounds; None is not a guessed
    clock or a grant of hardware support. Construction still owns static state.
    """

    controller: str
    resources: tuple[ResourceClaimIR, ...]
    dma: tuple[DmaLaneIR, ...]
    irq: IrqOptions | None
    irq_profile: InterruptProfileIR
    clock_plan: ClockPlanIR | None
    clock_inputs: Mapping
    clock_hz: int | None
    pins: tuple[Mapping, ...]
    hardware: "ControllerBinding | None"


@dataclass(frozen=True)
class RuntimePowerIR:
    """Selected provider lifecycle enrollment, never nominal clock bounds."""
    mode: str
    controllers: tuple[str, ...]


@dataclass(frozen=True)
class ConfigurationIR(Record):
    source_epoch: ClassVar[int] = 2
    board: str
    soc_family: str
    cpu_profile: CpuProfileIR
    kernel: KernelProfileIR | ThreadXKernelProfileIR | None
    controllers: tuple[ControllerIR, ...]
    devices: tuple[EndpointIR, ...]
    memory_budget: MemoryBudgetIR
    factory: FactoryOptions
    can_domain: CanDomainIR | None
    resource_plan: ResourcePlanIR
    trigger_plan: TriggerPlanIR
    dma_topology: DmaTopologyIR
    clock_plan: ClockPlanIR | None
    physical_part: PhysicalPartIR
    clock: Mapping
    clock_profile: str
    layout: Mapping
    board_bindings: Mapping
    routes: Mapping
    platform_metadata: Mapping
    inputs: tuple[Mapping, ...]
    source_dependencies: tuple[Mapping, ...]
    components: tuple[str, ...]
    can_controllers: tuple[str, ...]
    optimization: str
    physical_pcb_revision: str | None
    limitations: tuple[str, ...]
    physical_qualified: bool
    fact_references: Mapping | None = None
    pwm_shared_owners: tuple[PwmSharedOwnerIR, ...] = ()
    runtime_power: RuntimePowerIR | None = None
    i2s2: I2s2RoleIR | None = None
    configuration_sha256: str = ""
    schema_version: int = 1

    @property
    def backend(self):
        return self.cpu_profile.backend

    @property
    def enum_abi(self):
        return self.cpu_profile.enum_abi

    @property
    def irq(self):
        return self.cpu_profile.irq

    @property
    def cpu(self):
        return self.cpu_profile.cpu

    @property
    def part(self):
        return self.physical_part.part

    @property
    def package(self):
        return self.physical_part.package

    @property
    def flash(self):
        return self.physical_part.flash

    @property
    def memory(self):
        return self.physical_part.memory

    @property
    def claims(self):
        return self.resource_plan.claims

    def projection(self):
        result = {item.name: getattr(self, item.name) for item in fields(self)
                  if item.name not in {
                      "kernel", "memory_budget", "physical_part", "fact_references",
                      "pwm_shared_owners", "runtime_power", "i2s2", "clock_plan"}}
        result.update(backend=self.backend, enum_abi=self.enum_abi,
                      irq=thaw(self.irq), cpu=self.cpu, part=self.part,
                      package=self.package, flash=self.flash, memory=self.memory,
                      kernel_profile=self.kernel,
                      memory_budgets=self.memory_budget.projection(),
                      claims=tuple({name: getattr(claim, name) for name in
                          ("key", "owner", "mode", "source")} for claim in self.claims))
        for name in ("fact_references", "runtime_power", "i2s2", "clock_plan"):
            if getattr(self, name) is not None:
                result[name] = getattr(self, name)
        if self.pwm_shared_owners:
            result["pwm_shared_owners"] = self.pwm_shared_owners
        return result

    def with_integrations(self):
        """Reference canonical selected plans without copying or re-resolving."""
        from tools.configure.providers import maintained
        provider = maintained(self.soc_family)
        integrated = []
        for item in self.controllers:
            hardware = next((binding for binding in
                getattr(provider, "CONTROLLER_BINDINGS", ())
                if binding.controller == item.controller and
                binding.kind == item.kind), None)
            integration = ControllerIntegrationIR(
                item.controller,
                tuple(claim for claim in self.resource_plan.claims
                      if item.id in claim.owner.split(";") and
                      claim.mode not in {"reserved", "kernel"}),
                tuple(lane for lane in self.dma_topology.lanes if lane.owner == item.id),
                getattr(item.options, "irq", None), self.irq, self.clock_plan,
                self.clock,
                (self.clock[f"apb{hardware.apb}_hz"]
                 if hardware is not None else None), item.pins, hardware)
            integrated.append(replace(item, integration=integration))
        return replace(self, controllers=tuple(integrated))


def controller_ir(item, family):
    """Require one resolved owner; constructors never reinterpret raw inputs."""
    if not isinstance(item, ControllerIR):
        raise TypeError("Provider construction requires ControllerIR")
    if item.provider != family:
        raise ValueError("Controller provider differs from the selected family")
    return item
