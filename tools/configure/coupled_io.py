"""Declared coupled-owner routes; no MCU names or hardware algorithms."""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.calls import Symbol
from tools.configure.pinmux import NumericPinMuxPolicy
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


@dataclass(frozen=True)
class CoupledIoPolicy:
    """Exact declared trigger, converter, DMA and pad ownership facts."""

    routes: Mapping

    @classmethod
    def from_validated(cls, value, *, pinmux):
        if not isinstance(pinmux, NumericPinMuxPolicy):
            fail("Coupled I/O requires a selected numeric pinmux policy")
        if not isinstance(value, Mapping) or set(value) != {"routes"}:
            fail("Coupled I/O requires explicit route records")
        routes = {}
        required = {"id", "controller", "timer", "adc", "modes", "dma",
            "pins", "channels", "sample_cycles", "conversion_cycles",
            "timer_clock_domain", "adc_clock_domain", "clock_multiplier",
            "prescaler_max", "period_max", "trigger_event", "trigger_route",
            "resources"}
        for row in value["routes"]:
            if (not isinstance(row, Mapping) or not required.issubset(row) or
                    set(row) - required - {"power_stage"}):
                fail("Coupled I/O route requires exact named facts")
            if row["id"] in routes:
                fail("Duplicate coupled I/O route")
            for name in ("controller", "timer", "adc", "timer_clock_domain",
                         "adc_clock_domain"):
                Symbol(row[name])
            for name in ("modes", "dma", "pins", "channels", "sample_cycles",
                         "resources"):
                if (not isinstance(row[name], (tuple, list)) or
                        (name != "dma" and not row[name])):
                    fail("Coupled I/O requires nonempty ordered facts")
            for pin in row["pins"]:
                if not isinstance(pin, (list, tuple)) or len(pin) != 3:
                    fail("Coupled I/O pin requires identity, role and alternate")
                Symbol(pin[0])
                pinmux.validate(pin[2], "Coupled pin alternate")
            for name in ("conversion_cycles", "clock_multiplier", "prescaler_max",
                         "period_max"):
                integer(row[name], 1, context="Coupled " + name)
            for channel in row["channels"]:
                integer(channel, 0, 31, "Coupled channel")
            for cycles in row["sample_cycles"]:
                integer(cycles, 1, context="Coupled sample cycles")
            if "power_stage" in row:
                power = row["power_stage"]
                if (not isinstance(power, Mapping) or set(power) != {
                        "encoder", "minimum_deadtime_ticks", "deadtime_ranges",
                        "tick_multiple", "max_encoder_filter", "guard_us"}):
                    fail("Power stage route requires exact encoder/timing facts")
                Symbol(power["encoder"])
                for name in ("minimum_deadtime_ticks", "tick_multiple", "guard_us"):
                    integer(power[name], 1, context="Power stage " + name)
                integer(power["max_encoder_filter"], 0,
                        context="Power stage encoder filter maximum")
                if not isinstance(power["deadtime_ranges"], (list, tuple)):
                    fail("Power stage requires exact deadtime encoding ranges")
                for limits in power["deadtime_ranges"]:
                    if not isinstance(limits, (list, tuple)) or len(limits) != 3:
                        fail("Power stage deadtime range requires low/high/step")
                    low = integer(limits[0], 0, context="Deadtime range low")
                    integer(limits[1], low, context="Deadtime range high")
                    integer(limits[2], 1, context="Deadtime range step")
                if not power["deadtime_ranges"]:
                    fail("Power stage requires a reviewed deadtime encoding")
            routes[row["id"]] = freeze(dict(row))
        if not routes:
            fail("Coupled I/O requires a maintained route")
        return cls(freeze(routes))

    def validate_soc(self, soc):
        for row in self.routes.values():
            owner = soc["controllers"].get(row["controller"])
            if owner is None:
                continue
            if (owner["kind"] != "pwm_adc" or
                    not set(row["resources"]).issubset(owner.get("resources", ()))):
                fail("Coupled I/O must own its physical controller resources")
            if ("power_stage" in row and
                    row["power_stage"]["encoder"] not in soc["controllers"]):
                fail("Power stage encoder must be an explicit physical controller")

    def route(self, route):
        row = self.routes.get(route["id"])
        pins = tuple((pin["pin"], pin["function"], pin["af"])
                     for pin in route["pins"])
        if (row is None or route["kind"] != "pwm_adc" or
                row["controller"] != route["controller"] or pins != row["pins"]):
            fail("Coupled I/O requires its exact reviewed pad route")
        return row

    def validate(self, selected, route):
        row = self.route(route)
        required = {"tick_hz", "period_ticks", "trigger_ticks", "deadtime_ticks",
                    "channels", "sample_times", "reference_mv", "exclusive",
                    "irq_priority"}
        if required - selected.keys():
            fail("Coupled I/O requires explicit timing, sequence and IRQ")
        if selected["mode"] not in row["modes"]:
            fail("Coupled I/O mode is outside the declared route")
        if selected["exclusive"] is not True or selected.get("calls_os", False):
            fail("Coupled I/O requires one exclusive owner and non-OS IRQs")
        integer(selected["tick_hz"], 1, context="Coupled tick_hz")
        period = integer(selected["period_ticks"], 2, row["period_max"],
                         "Coupled period")
        integer(selected["trigger_ticks"], 1, period - 1, "Coupled trigger")
        power = row.get("power_stage")
        if power is not None:
            self._validate_power_stage(selected, power, period)
        elif selected["deadtime_ticks"] != 0:
            fail("Selected non-complementary coupled route uses zero deadtime")
        if tuple(selected["channels"]) != row["channels"]:
            fail("Coupled ADC sequence differs from its exact analog route")
        if len(selected["sample_times"]) != len(row["channels"]):
            fail("Coupled ADC sample times require one value per rank")
        for sample in selected["sample_times"]:
            integer(sample, 0, len(row["sample_cycles"]) - 1, "Coupled sample time")
        integer(selected["reference_mv"], 1, 3600, "Coupled reference")
        budgets = {"commit_before_ticks", "compute_budget_ticks", "max_irq_delay_us"}
        if power is not None:
            return
        if {"minimum_duty_ticks", "maximum_duty_ticks", "settling_ticks",
                "compute_budget_us", "encoder_filter"}.intersection(selected):
            fail("Ordinary coupled mode does not accept power-stage-only fields")
        if selected["mode"] == "injected-control":
            if budgets - selected.keys():
                fail("Coupled IRQ loop requires explicit compute_budget_ticks, "
                     "commit_before_ticks and max_irq_delay_us budgets")
            integer(selected["commit_before_ticks"], 1, period - 2,
                    "Coupled commit boundary")
            integer(selected["compute_budget_ticks"], 1, period - 1,
                    "Coupled compute budget")
            integer(selected["max_irq_delay_us"], 1, 0xffffffff,
                    "Coupled IRQ delay budget")
        elif budgets.intersection(selected):
            fail("Coupled finite window does not accept loop-only budgets")

    @staticmethod
    def _validate_power_stage(selected, facts, period):
        """Validate declared output-window geometry, not a physical waveform."""
        required = {"minimum_duty_ticks", "maximum_duty_ticks", "settling_ticks",
                    "compute_budget_us", "max_irq_delay_us", "encoder_filter",
                    "commit_before_ticks"}
        if required - selected.keys():
            fail("Power stage requires explicit " +
                 ", ".join(sorted(required - selected.keys())))
        if "compute_budget_ticks" in selected:
            fail("Power stage uses compute_budget_us, not a legacy loop budget")
        minimum = integer(selected["minimum_duty_ticks"], 1, period - 1,
                          "Power stage minimum duty")
        maximum = integer(selected["maximum_duty_ticks"], minimum + 1, period - 1,
                          "Power stage maximum duty")
        settling = integer(selected["settling_ticks"], 1, period - 1,
                           "Power stage settling window")
        if maximum + settling >= selected["trigger_ticks"]:
            fail("Power stage output and settling must precede its sample window")
        deadtime = integer(selected["deadtime_ticks"],
                           facts["minimum_deadtime_ticks"],
                           context="Power stage deadtime")
        if not any(low <= deadtime <= high and (deadtime - low) % step == 0
                   for low, high, step in facts["deadtime_ranges"]):
            fail("Power stage deadtime has no exact reviewed encoding")
        integer(selected["compute_budget_us"], 1,
                context="Power stage compute budget")
        integer(selected["max_irq_delay_us"], 1,
                context="Power stage IRQ delay budget")
        integer(selected["encoder_filter"], 0, facts["max_encoder_filter"],
                "Power stage encoder filter")
        integer(selected["commit_before_ticks"], 1, period - 2,
                "Power stage commit boundary")
        multiple = facts["tick_multiple"]
        if selected["tick_hz"] % multiple:
            fail("Power stage tick_hz must preserve its declared timing unit")
        divisor = selected["tick_hz"] // multiple
        if (period < 4 * divisor or
                selected["commit_before_ticks"] > period - 2 * divisor):
            fail("Power stage commit boundary requires two complete timing units")
        if any(selected[name] % divisor for name in (
                "period_ticks", "trigger_ticks", "commit_before_ticks")):
            fail("Power stage period/trigger/commit must preserve timing units")

    def validate_clock(self, selected, route, clock, plan):
        del clock
        row = self.route(route)
        if plan is None:
            fail("Coupled I/O requires an explicit clock plan")
        try:
            timer = plan["domains"][row["timer_clock_domain"]]["nominal_hz"]
            adc = plan["domains"][row["adc_clock_domain"]]["nominal_hz"]
        except KeyError:
            fail("Coupled I/O clock plan lacks its declared domains")
        tick = selected["tick_hz"]
        if timer % tick or not 1 <= timer // tick <= row["prescaler_max"]:
            fail("Coupled tick must exactly divide its timer clock")
        cycles = sum(row["sample_cycles"][sample] + row["conversion_cycles"]
                     for sample in selected["sample_times"])
        if cycles * tick >= selected["period_ticks"] * adc:
            fail("Coupled ADC scan does not fit within one PWM period")
        if "power_stage" in row:
            power = row["power_stage"]
            deadtime_ticks = (selected["deadtime_ticks"] * tick + timer - 1) // timer
            if (selected["maximum_duty_ticks"] + deadtime_ticks +
                    selected["settling_ticks"] >= selected["trigger_ticks"]):
                fail("Power stage deadtime/settling exceeds its sample window")
            conversion_us = (cycles * 1000000 + adc - 1) // adc
            conversion = (conversion_us * tick + 999999) // 1000000
            budget = ((selected["max_irq_delay_us"] +
                       selected["compute_budget_us"] + power["guard_us"]) *
                      tick + 999999) // 1000000
            if (selected["trigger_ticks"] + conversion + budget >=
                    selected["commit_before_ticks"]):
                fail("Power stage scan, IRQ and compute budget exceed its window")
            return
        if selected["mode"] == "injected-control":
            conversion_ticks = (cycles * tick + adc - 1) // adc
            delay_ticks = (selected["max_irq_delay_us"] * tick + 999999) // 1000000
            guard_ticks = (4 * tick + 999999) // 1000000
            if (selected["trigger_ticks"] + conversion_ticks + delay_ticks +
                    selected["compute_budget_ticks"] + guard_ticks >=
                    selected["commit_before_ticks"]):
                fail("Coupled IRQ loop conversion, delay and compute budget "
                     "must fit strictly within its commit window")

    def dma_paths(self, route, mode):
        row = self.route(route)
        if mode not in row["modes"]:
            fail("Coupled DMA mode is outside the reviewed route")
        return row["dma"]


def validate_soc(soc, provider):
    """Unreviewed providers cannot inherit a coupled physical personality."""
    review = getattr(provider, "COUPLED_IO", None)
    if review is not None:
        review.validate_soc(soc)
        return
    if any(controller["kind"] == "can_fd" or
           "pwm_adc" in controller.get("interfaces", {})
           for controller in soc["controllers"].values()):
        fail("Coupled I/O facts require an explicitly reviewed SoC domain")
