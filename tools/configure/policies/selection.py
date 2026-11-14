"""Explicit controller narrowing around stable public peripheral contracts.

These finite domain records contain support facts, not executable predicates.
Pin routes, clock plans and DMA tuples are checked against their sole authors;
hardware admission and peripheral state changes remain in C.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import re

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer, mode_traits
from tools.configure.records import freeze


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Selection policy requires exact named facts")


def _names(values, label):
    if (not isinstance(values, (tuple, list)) or not values or
            any(not isinstance(value, str) or not value for value in values) or
            len(set(values)) != len(values)):
        fail("Selection " + label + " requires distinct names")


def _mapping(value, label):
    if not isinstance(value, Mapping):
        fail("Selection " + label + " requires named records")
    return value


@dataclass(frozen=True)
class ClockContext:
    """Finite execution constraints for one package-owned clock profile."""

    backends: tuple[str, ...]
    allow_power: bool
    forbidden_kinds: tuple[str, ...]

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"backends", "allow_power", "forbidden_kinds"})
        _names(value["backends"], "clock backends")
        if any(name not in {"baremetal", "native", "freertos", "threadx"}
               for name in value["backends"]):
            fail("Selection clock context requires known backend names")
        if type(value["allow_power"]) is not bool:
            fail("Selection clock power permission requires a boolean")
        kinds = value["forbidden_kinds"]
        if not isinstance(kinds, (list, tuple)):
            fail("Selection clock kinds require an ordered name list")
        if kinds:
            _names(kinds, "clock kinds")
        if any(re.fullmatch(r"[a-z][a-z0-9_-]*", name) is None for name in kinds):
            fail("Selection clock kinds require ordinary kind identifiers")
        return cls(tuple(value["backends"]), value["allow_power"], tuple(kinds))


@dataclass(frozen=True)
class SelectionPolicy:
    """One immutable snapshot of silicon and reviewed-mode restrictions."""

    data: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "internal", "encoder", "i2c_target",
            "block", "rtc", "contexts", "dma", "pwm_period_max",
            "adc_internal_sample"}, {"limits", "watchdog_controller", "i2c_nominal",
                                      "serial_modes", "clock_contexts"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported selection domain schema")
        for name in ("internal", "contexts", "dma", "limits"):
            if not isinstance(value.get(name, ()), (list, tuple)):
                fail("Selection " + name + " requires an ordered record list")
        for row in value["internal"]:
            _fields(row, {"kind", "modes", "controller", "irq_required"})
            Symbol(row["controller"])
            _names(row["modes"], "internal modes")
            if type(row["irq_required"]) is not bool:
                fail("Selection IRQ requirement must be boolean")
        _fields(value["encoder"], {"controllers"})
        for controller, alternate in _mapping(value["encoder"]["controllers"], "encoders").items():
            Symbol(controller)
            integer(alternate, 0, 15, "Encoder alternate function")
        _fields(value["i2c_target"], {"controllers", "alternate"})
        _names(value["i2c_target"]["controllers"], "target controllers")
        integer(value["i2c_target"]["alternate"], 0, 15, "Target alternate function")
        block = value["block"]
        _fields(block, {"controller", "alternate", "pins", "max_blocks", "poll_budget"})
        Symbol(block["controller"])
        integer(block["alternate"], 0, 15, "Card alternate function")
        if set(_mapping(block["pins"], "card pins")) != {"1", "4", "8"}:
            fail("Card pin catalog requires exact supported bus widths")
        for pins in block["pins"].values():
            _names(pins, "card pins")
        for field in ("max_blocks", "poll_budget"):
            integer(block[field], 1, context="Card " + field)
        rtc = value["rtc"]
        _fields(rtc, {"controller", "crystal_route", "bypass_route",
                      "default_context", "part_contexts"}, {"crystal_resources"})
        resources = rtc.get("crystal_resources", ())
        if (not isinstance(resources, (tuple, list)) or
                any(not isinstance(item, str) for item in resources) or
                len(set(resources)) != len(resources)):
            fail("RTC crystal conflicts require distinct explicit resources")
        from tools.configure.resource_graph import normalize_resource
        for resource in resources:
            if normalize_resource(resource) != resource:
                fail("RTC crystal conflict requires a canonical resource")
        Symbol(rtc["controller"])
        for context in (rtc["default_context"], *_mapping(rtc["part_contexts"], "RTC parts").values()):
            _fields(context, {"backend", "profile"},
                    {"parameters", "forbidden_options", "forbid_auxiliary"})
            if not isinstance(context["backend"], str) or not isinstance(context["profile"], str):
                fail("RTC context requires explicit backend and profile")
            if "forbid_auxiliary" in context and type(context["forbid_auxiliary"]) is not bool:
                fail("RTC auxiliary restriction requires a boolean")
            if "forbidden_options" in context:
                _names(context["forbidden_options"], "RTC forbidden options")
            for scalar in _mapping(context.get("parameters", {}), "RTC parameters").values():
                if type(scalar) not in {bool, int, str}:
                    fail("RTC parameter review requires scalar values")
        for row in value["contexts"]:
            _fields(row, {"kind", "modes", "backend", "profile"})
            _names(row["modes"], "context modes")
        for row in value["dma"]:
            _fields(row, {"kind", "controller", "mode", "paths"})
            Symbol(row["controller"])
            if not isinstance(row["paths"], (tuple, list)) or not row["paths"]:
                fail("Selection DMA requires explicit legal tuples")
            for paths in row["paths"]:
                _names(paths, "DMA tuple")
        for controller, maximum in _mapping(value["pwm_period_max"], "PWM limits").items():
            Symbol(controller)
            integer(maximum, 1, context="PWM period maximum")
        integer(value["adc_internal_sample"], 0, 7, "Internal ADC sample encoding")
        for row in value.get("limits", ()):
            _fields(row, {"kind", "modes", "field", "minimum", "maximum"})
            _names(row["modes"], "limit modes")
            low = integer(row["minimum"], context="Selection limit minimum")
            integer(row["maximum"], low, context="Selection limit maximum")
        if "i2c_nominal" in value:
            divider = value["i2c_nominal"]
            _fields(divider, {"clock_quantum_hz", "clock_hz", "fast_min_hz",
                "fast_threshold_hz", "maximum_hz", "standard_ceiling_hz",
                "standard_factor", "fast_factor", "standard_min_divisor",
                "maximum_divisor", "rise_quantum_hz", "rise_budget_hz"})
            bounds = divider["clock_hz"]
            if not isinstance(bounds, (tuple, list)) or len(bounds) != 2:
                fail("I2C nominal clock requires explicit minimum/maximum")
            low = integer(bounds[0], 1, context="I2C minimum clock")
            integer(bounds[1], low, context="I2C maximum clock")
            for name, item in divider.items():
                if name != "clock_hz":
                    integer(item, 1, context="I2C nominal " + name)
        if "serial_modes" in value:
            serial = value["serial_modes"]
            _fields(serial, (), {"uart_wire", "spi_frames"})
            from tools.configure import spi_frames, uart_wires
            for name, policy in (("uart_wire", uart_wires), ("spi_frames", spi_frames)):
                if name in serial:
                    policy.validate_policy(serial[name])
        if "clock_contexts" in value:
            contexts = {}
            for name, row in _mapping(value["clock_contexts"], "clock contexts").items():
                if (not isinstance(name, str) or
                        re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{0,63}", name) is None):
                    fail("Selection clock context requires an ordinary profile identifier")
                contexts[name] = ClockContext.from_validated(row)
            value = {**value, "clock_contexts": contexts}
        return cls(freeze(value))

    def validate(self, selection, route, hardware):
        kind, mode, controller = route["kind"], selection["mode"], route["controller"]
        for row in self.data["internal"]:
            if kind == row["kind"] and mode in row["modes"]:
                if (controller != row["controller"] or route["pins"] or
                        row["irq_required"] and "irq_priority" not in selection):
                    fail("Selected mode requires its exact internal controller and IRQ policy")
        for row in self.data.get("limits", ()):
            if kind == row["kind"] and mode in row["modes"] and row["field"] in selection:
                integer(selection[row["field"]], row["minimum"], row["maximum"],
                        "Selected " + row["field"])
        if kind == "rtc":
            value = self.data["rtc"]
            expected = value["bypass_route"] if selection.get("lse_bypass", False) else value["crystal_route"]
            if controller != value["controller"] or route["pins"] or route["id"] != expected:
                fail("RTC oscillator source differs from its reviewed pad route")
        elif kind == "i2c_target":
            value, pins = self.data["i2c_target"], route["pins"]
            if (controller not in value["controllers"] or
                    tuple(pin["function"] for pin in pins) not in {
                        ("scl", "sda"), ("scl", "sda", "smba")} or
                    len({pin["pin"] for pin in pins}) != len(pins) or
                    any(pin.get("af") != value["alternate"] for pin in pins)):
                fail("I2C target route differs from its reviewed alternate pads")
        elif kind == "encoder":
            alternate = self.data["encoder"]["controllers"].get(controller)
            pins = route["pins"]
            if (alternate is None or len(pins) != 2 or
                    tuple(pin["function"] for pin in pins) != ("ti1", "ti2") or
                    pins[0]["pin"] == pins[1]["pin"] or
                    any(pin.get("af") != alternate for pin in pins)):
                fail("Encoder requires distinct reviewed TI1/TI2 alternate-function pads")
            if selection.get("counting") not in {"x2-a", "x2-b", "x4"}:
                fail("Encoder counting must select x2-a, x2-b or x4")
            for field in ("filter_a", "filter_b"):
                integer(selection[field], 0, 15, "Encoder " + field)
            for field in ("invert_a", "invert_b", "exclusive"):
                if type(selection[field]) is not bool:
                    fail("Encoder " + field + " must be boolean")
            if not selection["exclusive"]:
                fail("Encoder requires an exclusive controller owner")
        elif kind == "block":
            self._block(selection, route)
        elif kind == "flash" and type(selection.get("maintenance", False)) is not bool:
            fail("Flash maintenance must be an explicit boolean")
        elif kind == "watchdog" and mode == "window":
            if controller != self.data.get("watchdog_controller"):
                fail("Window watchdog requires its reviewed controller")
            integer(selection.get("window_ticks", 1), 1, 62, "WWDG window_ticks")
        elif kind == "pwm":
            pins = route["pins"]
            channel = (hardware.PWM_PIN_CHANNELS.get((controller, pins[0]["pin"]))
                       if len(pins) == 1 else None)
            timer = hardware.PWM_TIMERS.get(controller)
            if (timer is None or channel is None or
                    pins[0]["function"] != "ch" + str(channel) or pins[0].get("af") != timer.af):
                fail("PWM requires a reviewed general timer channel route")
            integer(selection["period_ticks"], 1, self.data["pwm_period_max"][controller],
                    "PWM period")
        if kind == "adc":
            if mode == "injected-shot" and len(selection["channels"]) > 4:
                fail("Injected ADC requires at most four ranks")
            internal = hardware.ADC_INTERNAL_CHANNELS.get(controller, ())
            if any(channel in internal and sample != self.data["adc_internal_sample"]
                   for channel, sample in zip(selection["channels"], selection["sample_times"])):
                fail("Internal ADC sources require the reviewed acquisition period")

    def _block(self, selection, route):
        value = self.data["block"]
        required = {"bus_width", "max_blocks", "poll_budget", "stop_timeout_us"}
        if not required.issubset(selection):
            fail("SDIO requires explicit bus_width and finite transfer/stop bounds")
        if selection.get("speed", "default") not in {"default", "high-speed"}:
            fail("SDIO speed must be explicit default or high-speed")
        width, protocol = selection["bus_width"], selection.get("protocol", "sd-memory")
        if protocol not in {"sd-memory", "mmc", "sdio"}:
            fail("SDIO protocol must be explicit sd-memory, mmc or sdio")
        if type(width) is not int or width not in {1, 4, 8} or width == 8 and protocol != "mmc":
            fail("SDIO bus_width must be 1 or 4; 8 requires explicit MMC")
        if (route["controller"] != value["controller"] or
                tuple(pin["pin"] for pin in route["pins"]) != value["pins"][str(width)] or
                any(pin["af"] != value["alternate"] for pin in route["pins"])):
            fail("SDIO bus width requires its exact reviewed pad route")
        for field, maximum in (("max_blocks", value["max_blocks"]),
                ("poll_budget", value["poll_budget"]), ("stop_timeout_us", 0xffffffff)):
            integer(selection[field], 1, maximum, "SDIO " + field)
        if selection["mode"] != "sdio-poll" and "irq_priority" not in selection:
            fail("SDIO asynchronous engines require an explicit IRQ priority")
        if selection.get("calls_os", False):
            fail("SDIO IRQs do not call an OS or create a hidden worker")

    def validate_context(self, selection, route, *, backend, clock_profile,
                         part=None, clock_plan=None):
        if route["kind"] == "rtc":
            rtc = self.data["rtc"]
            value = rtc["part_contexts"].get(part, rtc["default_context"])
            if backend != value["backend"] or clock_profile != value["profile"]:
                fail("RTC requires its reviewed RUN ownership")
            if (value.get("forbid_auxiliary", False) and
                    (clock_plan is None or clock_plan.pll_sai is not None) or
                    any(selection.get(field, False) for field in value.get("forbidden_options", ()))):
                fail("RTC requires its reviewed crystal and System profile")
            parameters = value.get("parameters", {})
            if parameters and (clock_plan is None or clock_plan.parameters is None or
                    any(clock_plan.parameters[key] != expected for key, expected in parameters.items())):
                fail("RTC requires its reviewed exact-part RUN tuple")
        for row in self.data["contexts"]:
            if route["kind"] == row["kind"] and selection["mode"] in row["modes"]:
                if backend != row["backend"] or clock_profile != row["profile"]:
                    fail("External memory requires its exact reviewed RUN ownership")

    def validate_assembly_context(self, *, backend, clock_profile, power, controllers):
        """Apply only the selected profile's explicit whole-assembly review."""
        context = self.data.get("clock_contexts", {}).get(clock_profile)
        if context is not None and (
                backend not in context.backends or
                not context.allow_power and power is not None or
                any(item.kind in context.forbidden_kinds for item in controllers)):
            fail("Selected clock profile requires its reviewed execution and power context")

    def dma_paths(self, route, mode, hardware):
        rows = [row for row in self.data["dma"] if
                (row["kind"], row["controller"], row["mode"]) ==
                (route["kind"], route["controller"], mode)]
        actual = tuple(route.get("mode_dma", {}).get(mode, route.get("dma", ())))
        if rows:
            allowed = tuple(paths for row in rows for paths in row["paths"])
            return actual if actual in allowed else None
        if route["kind"] == "dac":
            mask = sum(1 << (hardware.DAC_PIN_CHANNELS[(route["controller"], pin["pin"])] - 1)
                       for pin in route["pins"])
            return hardware.DAC_DMA_ROUTES.get(mask)
        return hardware.FIXED_DMA.get((route["controller"], mode))

    def selected_format(self, item, shape):
        """Nominal divider projection; C independently admits real hardware."""
        del shape
        value = self.data.get("i2c_nominal")
        if item.kind != "i2c" or item.integration is None or value is None:
            return {}
        clock, maximum = item.integration.clock_hz, item.options.max_hz
        if (type(clock) is not int or clock % value["clock_quantum_hz"] or
                not value["clock_hz"][0] <= clock <= value["clock_hz"][1] or
                not 0 < maximum <= value["maximum_hz"]):
            fail("I2C nominal timing requires its reviewed quantized clock")
        fast = maximum > value["fast_threshold_hz"]
        if fast and clock < value["fast_min_hz"]:
            fail("Fast I2C requires its reviewed peripheral clock")
        ceiling = maximum if fast else min(maximum, value["standard_ceiling_hz"])
        factor = value["fast_factor"] if fast else value["standard_factor"]
        divisor = (clock + ceiling * factor - 1) // (ceiling * factor)
        if fast and clock % value["rise_quantum_hz"]:
            divisor = max(divisor, clock // value["rise_budget_hz"] + 1)
        if not (1 if fast else value["standard_min_divisor"]) <= divisor <= value["maximum_divisor"]:
            fail("I2C ceiling has no representable reviewed clock divider")
        return {"fields": {"address_bits": 0, "configured_max_hz": maximum,
                           "nominal_hz": clock // (divisor * factor)}}
