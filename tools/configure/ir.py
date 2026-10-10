"""Immutable, typed assembly decisions consumed by the C emitters.

JSON hardware facts and authored inputs are validated once by the resolver. This
module stores that decision; it does not select targets or repair configuration.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType


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


@dataclass(frozen=True)
class GpioOptions:
    mask: int
    initial: int


@dataclass(frozen=True)
class UartOptions:
    baud: int
    rx_profile: str
    rx_capacity: int | None
    irq: IrqOptions


@dataclass(frozen=True)
class SpiOptions:
    max_hz: int
    irq: IrqOptions | None


@dataclass(frozen=True)
class I2cOptions:
    max_hz: int


@dataclass(frozen=True)
class ExtiOptions:
    edge: str
    event_capacity: int
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


@dataclass(frozen=True)
class NoOptions:
    """Flash and watchdog have no authored construction options."""


ControllerOptions = (GpioOptions | UartOptions | SpiOptions | I2cOptions |
                     ExtiOptions | PwmOptions | AdcOptions | NoOptions)


def mode_options(values):
    """Project one validated mode into immutable named constructor arguments."""
    kind = values["kind"]
    irq = (IrqOptions(values["irq_priority"], values.get("calls_os", False))
           if "irq_priority" in values else None)
    if kind == "gpio":
        return GpioOptions(values["mask"], values["initial"])
    if kind == "uart":
        return UartOptions(values["baud"], values["rx_profile"],
                           values.get("rx_capacity"), irq)
    if kind == "spi":
        return SpiOptions(values.get("max_hz", 1000000), irq)
    if kind == "i2c":
        return I2cOptions(values.get("max_hz", 100000))
    if kind == "exti":
        return ExtiOptions(values["edge"], values["event_capacity"], irq)
    if kind == "pwm":
        return PwmOptions(values["period_ticks"], values["duty_ticks"],
                          values["tick_hz"])
    if kind == "adc":
        return AdcOptions(tuple(values["channels"]),
                          tuple(values["sample_times"]), values["reference_mv"],
                          values.get("timeout_ms"), irq)
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

    @classmethod
    def create(cls, values, provider):
        return cls(*(values[key] for key in
                     ("id", "kind", "controller", "binding", "mode")),
                   provider, mode_options(values), freeze(values))


@dataclass(frozen=True)
class EndpointIR(Record):
    id: str
    controller: str
    driver: str
    kind: str
    values: Mapping

    @classmethod
    def create(cls, values, controllers):
        kind = next(item.kind for item in controllers
                    if item.id == values["controller"])
        return cls(values["id"], values["controller"], values["driver"],
                   kind, freeze(values))


@dataclass(frozen=True)
class MemoryBudgetIR:
    main_stack_bytes: int
    libc_heap_bytes: int
    static_ram_limit_bytes: int | None
    flash_load_limit_bytes: int | None


@dataclass(frozen=True)
class ConfigurationIR(Record):
    board: str
    soc_family: str
    backend: str
    controllers: tuple[ControllerIR, ...]
    devices: tuple[EndpointIR, ...]
    memory_budget: MemoryBudgetIR
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
                                budgets.get("flash_load_bytes"))
        return cls(result["board"], result["soc_family"], result["backend"],
                   controllers, devices, memory, MappingProxyType(values))

    def to_dict(self):
        return thaw(self)
