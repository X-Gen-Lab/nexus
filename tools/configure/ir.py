"""Immutable, typed assembly decisions consumed by the C emitters.

JSON hardware facts and authored inputs are validated once by the resolver. This
module stores that decision; it does not select targets or repair configuration.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

try:
    from .resource_graph import ResourcePlanIR
    from .trigger_graph import TriggerPlanIR
    from .dma_topology import DmaTopologyIR
except ImportError:
    from resource_graph import ResourcePlanIR
    from trigger_graph import TriggerPlanIR
    from dma_topology import DmaTopologyIR


def freeze(value):
    if isinstance(value, dict):
        return MappingProxyType({key: freeze(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(freeze(item) for item in value)
    return value


def thaw(value):
    if isinstance(value, Mapping):
        return {key: thaw(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [thaw(item) for item in value]
    return value


class Record(Mapping):
    """Read-only projection used by existing narrow constructor emitters."""
    def __getitem__(self, key):
        return self.values[key]

    def __iter__(self):
        return iter(self.values)

    def __len__(self):
        return len(self.values)


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
    arch: str
    fpu: str
    float_abi: str
    enum_abi: str
    backend: str
    arch_macro: str
    has_basepri: bool
    atomic_backend: str
    irq: InterruptProfileIR
    compile_options: tuple[str, ...]
    definitions: Mapping
    values: Mapping

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "arch", "fpu", "float_abi", "enum_abi", "backend", "arch_macro",
            "has_basepri", "atomic_backend")),
                   InterruptProfileIR(**record["irq"]),
                   tuple(record["compile_options"]),
                   freeze(record["definitions"]), freeze(record))

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
    values: Mapping

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "profile", "tick_source", "tick_hz", "tick_reload", "max_priorities",
            "syscall_priority", "kernel_policy", "kernel_port",
            "memory_protection", "system_call_stack_words", "security_model",
            "secure_idle_stack_bytes",
            "idle_stack_words",
            "max_task_name_len", "mutexes", "counting_semaphores",
            "task_notifications", "notification_slots", "trace", "runtime_stats",
            "tickless", "lowpower_port", "min_stack_words", "stack_alignment_bytes")),
                   freeze(record["definitions"]), freeze(record))

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
    values: Mapping

    @classmethod
    def from_validated(cls, record):
        return cls(*(record[name] for name in (
            "backend", "profile", "tick_source", "tick_hz", "tick_reload",
            "max_priorities", "syscall_priority", "interrupt_mask",
            "kernel_policy", "kernel_port", "timer_process_in_isr",
            "min_stack_bytes", "stack_alignment_bytes")),
                   freeze(record["definitions"]), freeze(record))

    def to_dict(self):
        return thaw(self)


@dataclass(frozen=True)
class GpioOptions:
    mask: int
    initial: int


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
    values: Mapping

    @classmethod
    def from_validated(cls, values):
        fields = cls.__dataclass_fields__.keys() - {"values"}
        return cls(**{name: values[name] for name in fields},
                   values=freeze(values))


@dataclass(frozen=True)
class UartOptions:
    baud: int
    rx_profile: str
    rx_capacity: int | None
    irq: IrqOptions
    frame: UartFrameOptions | None = None
    stream_profile: StreamProfileIR | None = None


@dataclass(frozen=True)
class SpiOptions:
    max_hz: int
    irq: IrqOptions | None


@dataclass(frozen=True)
class I2cTimingSelectionIR(Record):
    timingr: int
    cr1_filter_bits: int
    provenance: str
    physical_qualified: bool
    source: str
    values: Mapping

    @classmethod
    def from_validated(cls, values):
        return cls(*(values[name] for name in (
            "timingr", "cr1_filter_bits", "provenance", "physical_qualified",
            "source")), freeze(values))


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
class AdcOptions:
    channels: tuple[int, ...]
    sample_times: tuple[int, ...]
    reference_mv: int
    timeout_ms: int | None
    irq: IrqOptions | None
    stream_profile: StreamProfileIR | None = None


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
class NoOptions:
    """Flash and watchdog have no authored construction options."""


ControllerOptions = (GpioOptions | UartOptions | SpiOptions | I2cOptions |
                     ExtiOptions | PwmOptions | PwmGroupOptions | EncoderOptions | AdcOptions | CanOptions |
                     UsbDeviceOptions | EthernetOptions | TimerCaptureOptions |
                     AdvancedPwmOptions | NoOptions)


def mode_options(values):
    """Project one validated mode into immutable named constructor arguments."""
    kind = values["kind"]
    irq = (IrqOptions(values["irq_priority"], values.get("calls_os", False),
                      values.get("irq_probe", False))
           if "irq_priority" in values else None)
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
    if kind == "eth":
        return EthernetOptions(values["tx_count"], values["rx_count"],
                               values["rx_storage_bytes"], tuple(values["address"]),
                               values["init_timeout_us"], values["drain_timeout_us"], irq)
    if kind == "gpio":
        return GpioOptions(values["mask"], values["initial"])
    if kind == "uart":
        return UartOptions(values["baud"], values["rx_profile"],
                           values.get("rx_capacity"), irq,
                           UartFrameOptions(**values["frame"])
                           if "frame" in values else None,
                           StreamProfileIR.from_validated(
                               values["stream_resolution"])
                           if "stream_resolution" in values else None)
    if kind == "spi":
        return SpiOptions(values.get("max_hz", 1000000), irq)
    if kind == "i2c":
        return I2cOptions(values.get("max_hz", 100000), irq,
                         I2cTimingSelectionIR.from_validated(
                             values["timing_resolution"])
                         if "timing_resolution" in values else None)
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
    if kind in {"flash", "watchdog"}:
        return NoOptions()
    raise ValueError(f"Unmaintained controller decision: {kind}")


@dataclass(frozen=True)
class ControllerIR(Record):
    id: str
    kind: str
    controller: str
    binding: str
    mode: str
    provider: str
    options: ControllerOptions
    values: Mapping
    interface_kind: str | None = None

    @classmethod
    def create(cls, values, provider):
        return cls(*(values[key] for key in
                     ("id", "kind", "controller", "binding", "mode")),
                   provider, mode_options(values), freeze(values),
                   values.get("interface_kind", values["kind"]))


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
    values: Mapping

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
                   kind, options, freeze(values))


@dataclass(frozen=True)
class MemoryRegionBudgetIR:
    id: str
    limit_bytes: int


@dataclass(frozen=True)
class MemoryBudgetIR:
    main_stack_bytes: int
    libc_heap_bytes: int
    static_ram_limit_bytes: int | None
    flash_load_limit_bytes: int | None
    regions: tuple[MemoryRegionBudgetIR, ...] = ()


@dataclass(frozen=True)
class FactoryOptions:
    capabilities: bool


@dataclass(frozen=True)
class ConfigurationIR(Record):
    board: str
    soc_family: str
    backend: str
    irq: InterruptProfileIR
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
    values: Mapping

    @classmethod
    def from_validated(cls, result):
        controllers = tuple(ControllerIR.create(item, result["soc_family"])
                            for item in result["controllers"])
        devices = tuple(EndpointIR.create(item, controllers)
                        for item in result["devices"])
        values = dict(freeze(result))
        values["controllers"] = controllers
        values["devices"] = devices
        budgets = result["memory_budgets"]
        memory = MemoryBudgetIR(budgets["main_stack_bytes"],
                                budgets.get("libc_heap_bytes", 0),
                                budgets.get("static_ram_bytes"),
                                budgets.get("flash_load_bytes"),
                                tuple(MemoryRegionBudgetIR(name, choice["limit_bytes"])
                                      for name, choice in sorted(
                                          budgets.get("regions", {}).items())))
        irq = InterruptProfileIR(**result["irq"])
        cpu = CpuProfileIR.from_validated(result["cpu_profile"])
        kernel_type = (ThreadXKernelProfileIR if result["backend"] == "threadx"
                       else KernelProfileIR)
        kernel = (kernel_type.from_validated(result["kernel_profile"])
                  if result.get("kernel_profile") is not None else None)
        return cls(result["board"], result["soc_family"], result["backend"], irq, cpu, kernel,
                   controllers, devices, memory,
                   FactoryOptions(result.get("factory", {}).get("capabilities", False)),
                   (CanDomainIR(result["can_domain"]["split"],
                                tuple(CanFilterIR(**item) for item in
                                      result["can_domain"]["filters"]))
                    if result.get("can_domain") is not None else None),
                   ResourcePlanIR.from_validated(result["resource_plan"]),
                   TriggerPlanIR.from_validated(result["trigger_plan"]),
                   DmaTopologyIR.from_validated(result["dma_topology"]),
                   MappingProxyType(values))

    def to_dict(self):
        return thaw(self)
