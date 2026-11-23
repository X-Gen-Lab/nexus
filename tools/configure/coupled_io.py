"""Declared coupled-owner routes; no MCU names or hardware algorithms."""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


@dataclass(frozen=True)
class CoupledIoPolicy:
    """Exact declared trigger, converter, DMA and pad ownership facts."""

    routes: Mapping

    @classmethod
    def from_validated(cls, value):
        if not isinstance(value, Mapping) or set(value) != {"routes"}:
            fail("Coupled I/O requires explicit route records")
        routes = {}
        required = {"id", "controller", "timer", "adc", "modes", "dma",
            "pins", "channels", "sample_cycles", "conversion_cycles",
            "timer_clock_domain", "adc_clock_domain", "clock_multiplier",
            "prescaler_max", "period_max", "trigger_event", "trigger_route",
            "resources"}
        for row in value["routes"]:
            if not isinstance(row, Mapping) or set(row) != required:
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
                integer(pin[2], 0, 15, "Coupled pin alternate")
            for name in ("conversion_cycles", "clock_multiplier", "prescaler_max",
                         "period_max"):
                integer(row[name], 1, context="Coupled " + name)
            for channel in row["channels"]:
                integer(channel, 0, 31, "Coupled channel")
            for cycles in row["sample_cycles"]:
                integer(cycles, 1, context="Coupled sample cycles")
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
        if selected["deadtime_ticks"] != 0:
            fail("Selected non-complementary coupled route uses zero deadtime")
        if tuple(selected["channels"]) != row["channels"]:
            fail("Coupled ADC sequence differs from its exact analog route")
        if len(selected["sample_times"]) != len(row["channels"]):
            fail("Coupled ADC sample times require one value per rank")
        for sample in selected["sample_times"]:
            integer(sample, 0, len(row["sample_cycles"]) - 1, "Coupled sample time")
        integer(selected["reference_mv"], 1, 3600, "Coupled reference")
        budgets = {"commit_before_ticks", "compute_budget_ticks", "max_irq_delay_us"}
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
