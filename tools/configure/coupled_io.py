"""Declared coupled-owner routes; no MCU names or hardware algorithms."""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.generation.calls import Symbol
from tools.configure.pinmux import FixedRoutePinMuxPolicy, NumericPinMuxPolicy
from tools.configure.providers.common import fail, integer, normalize_resource
from tools.configure.records import freeze


def _conversion_cycles(row, sample_times):
    cycles = tuple(row["sample_cycles"][sample] + row["conversion_cycles"]
                   for sample in sample_times)
    return max(cycles) if row.get("power_stage", {}).get("pair") else sum(cycles)


@dataclass(frozen=True)
class CoupledIoPolicy:
    """Exact declared trigger, converter, DMA and pad ownership facts."""

    routes: Mapping

    @classmethod
    def from_validated(cls, value, *, pinmux):
        if not isinstance(pinmux, (NumericPinMuxPolicy, FixedRoutePinMuxPolicy)):
            fail("Coupled I/O requires an explicit pinmux policy")
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
                    set(row) - required - {"power_stage", "adc_clock_divider",
                                           "complementary_deadtime_max"}):
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
            for field in ("adc_clock_divider", "complementary_deadtime_max"):
                if field in row:
                    integer(row[field], 1, context="Coupled " + field)
            if "power_stage" in row:
                if "complementary_deadtime_max" in row:
                    fail("Power stage and finite-window deadtime facts are exclusive")
                power = row["power_stage"]
                required_power = {"minimum_deadtime_ticks", "deadtime_ranges",
                    "tick_multiple", "guard_us", "capture", "hardware", "domain",
                    "alignments"}
                if (not isinstance(power, Mapping) or
                        not required_power.issubset(power) or
                        set(power) - required_power - {"encoder", "max_encoder_filter",
                                                       "pair", "dc"}):
                    fail("Power stage route requires exact capture/timing facts")
                if ("encoder" in power) != ("max_encoder_filter" in power):
                    fail("Power stage angle route requires encoder and filter facts")
                if "encoder" in power:
                    Symbol(power["encoder"])
                    integer(power["max_encoder_filter"], 0,
                            context="Power stage encoder filter maximum")
                for name in ("minimum_deadtime_ticks", "tick_multiple", "guard_us"):
                    integer(power[name], 1, context="Power stage " + name)
                capture = power["capture"]
                if (not isinstance(capture, Mapping) or set(capture) != {
                        "current_count", "current_phase_mask", "sample_bus",
                        "adc_index", "trigger_selector"}):
                    fail("Power stage requires exact capture facts")
                dc = power.get("dc")
                count = integer(capture["current_count"], 0, 3,
                                "Power stage current count")
                mask = integer(capture["current_phase_mask"], 0, 7,
                               "Power stage measured phase mask")
                if dc is not None:
                    if (not isinstance(dc, Mapping) or set(dc) != {"rearm_guard_us"}
                            or count != 0 or mask != 0 or capture["sample_bus"] or
                            power.get("pair") is not None or
                            len(row["channels"]) != 1):
                        fail("Power stage DC-link requires one independent bus converter")
                    integer(dc["rearm_guard_us"], 1, context="DC-link rearm guard")
                elif count not in {2, 3} or mask.bit_count() != count:
                    fail("Power stage current count and phase mask differ")
                if type(capture["sample_bus"]) is not bool:
                    fail("Power stage bus capture requires a boolean")
                index = integer(capture["adc_index"], 0, 255,
                                "Power stage ADC index")
                integer(capture["trigger_selector"], 0, 0xffffffff,
                        "Power stage trigger selector")
                if dc is None and len(row["channels"]) != count + int(capture["sample_bus"]):
                    fail("Power stage rank count differs from its capture profile")
                hardware = power["hardware"]
                if (not isinstance(hardware, Mapping) or set(hardware) != {
                        "timer_index", "break_irq", "break_bank", "break_mask",
                        "peer0", "peer1", "break_handler", "update_handler"}):
                    fail("Power stage requires exact private timer facts")
                integer(hardware["timer_index"], 0, 255, "Power stage timer index")
                integer(hardware["break_mask"], 1, 0xffff, "Power stage break mask")
                Symbol(hardware["break_irq"])
                Symbol(hardware["break_bank"])
                Symbol(hardware["break_handler"])
                Symbol(hardware["update_handler"])
                Symbol(hardware["peer0"])
                Symbol(hardware["peer1"])
                domain = power["domain"]
                if (not isinstance(domain, Mapping) or set(domain) != {
                        "id", "resources", "state_type", "config_type", "admit",
                        "prepare", "close", "adc_irq", "handler",
                        "member_state_type", "member_config_type", "converters",
                        "time_shared"}):
                    fail("Power stage requires exact physical domain wiring")
                if type(domain["time_shared"]) is not bool:
                    fail("Power stage domain time_shared requires a boolean")
                for name in set(domain) - {"resources", "converters", "time_shared"}:
                    Symbol(domain[name])
                converters = domain["converters"]
                if (not isinstance(converters, (tuple, list)) or
                        not converters or len(converters) > 256 or
                        any(type(name) is not str for name in converters) or
                        len(set(converters)) != len(converters)):
                    fail("Power stage domain requires ordered distinct converters")
                for name in converters:
                    Symbol(name)
                if index >= len(converters) or converters[index] != row["adc"]:
                    fail("Power stage ADC index differs from its declared converter")
                if (not isinstance(power["alignments"], (tuple, list)) or
                        not power["alignments"] or
                        any(type(item) is not str for item in power["alignments"]) or
                        len(set(power["alignments"])) != len(power["alignments"]) or
                        set(power["alignments"]) - {"edge", "center"}):
                    fail("Power stage requires reviewed carrier alignments")
                if (not isinstance(domain["resources"], (tuple, list)) or
                        not domain["resources"] or
                        len(set(domain["resources"])) != len(domain["resources"])):
                    fail("Power stage domain requires distinct physical resources")
                for resource in domain["resources"]:
                    if not isinstance(resource, str):
                        fail("Power stage domain resource requires an identity")
                    normalize_resource(resource)
                if not {"controller:" + name for name in converters}.issubset(
                        domain["resources"]):
                    fail("Power stage domain must own its declared converters")
                pair = power.get("pair")
                if pair is not None:
                    if (not isinstance(pair, Mapping) or set(pair) != {"converters"} or
                            not isinstance(pair["converters"], (tuple, list)) or
                            len(pair["converters"]) != 2 or
                            len(set(pair["converters"])) != 2 or
                            not set(pair["converters"]).issubset(converters) or
                            pair["converters"][0] != row["adc"] or count != 2 or
                            capture["sample_bus"]):
                        fail("Power stage pair requires two reviewed current converters")
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
            if ("power_stage" in row and "encoder" in row["power_stage"] and
                    row["power_stage"]["encoder"] not in soc["controllers"]):
                fail("Power stage encoder must be an explicit physical controller")

    def route(self, route):
        row = self.routes.get(route["id"])
        pins = tuple((pin["pin"], pin["function"], pin.get("af"))
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
        elif "complementary_deadtime_max" in row:
            integer(selected["deadtime_ticks"], 1,
                    row["complementary_deadtime_max"], "Coupled deadtime")
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
                "compute_budget_us", "encoder_filter", "domain",
                "center_aligned", "phase_offset_ticks", "start_tolerance_us"}.intersection(selected):
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
                    "compute_budget_us", "max_irq_delay_us", "commit_before_ticks"}
        if "encoder" in facts:
            required.add("encoder_filter")
        if required - selected.keys():
            fail("Power stage requires explicit " +
                 ", ".join(sorted(required - selected.keys())))
        if "compute_budget_ticks" in selected:
            fail("Power stage uses compute_budget_us, not a legacy loop budget")
        center = selected.get("center_aligned", False)
        if type(center) is not bool:
            fail("Power stage center_aligned requires a boolean")
        alignment = "center" if center else "edge"
        if alignment not in facts["alignments"]:
            fail("Power stage alignment is outside the reviewed route")
        timing = {"phase_offset_ticks", "start_tolerance_us"}
        if facts["domain"]["time_shared"]:
            if timing - selected.keys():
                fail("Power stage time-shared axes require explicit phase and tolerance")
            integer(selected["phase_offset_ticks"], 0, period - 1,
                    "Power stage phase offset")
            integer(selected["start_tolerance_us"], 1, 0xffffffff,
                    "Power stage startup tolerance")
        elif timing.intersection(selected):
            fail("Power stage time-shared fields require a reviewed time-shared route")
        minimum = integer(selected["minimum_duty_ticks"], 1, period - 1,
                          "Power stage minimum duty")
        maximum = integer(selected["maximum_duty_ticks"], minimum + 1, period - 1,
                          "Power stage maximum duty")
        settling = integer(selected["settling_ticks"], 1, period - 1,
                           "Power stage settling window")
        if (facts.get("dc") is None and not center and
                maximum + settling >= selected["trigger_ticks"]):
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
        if "encoder" in facts:
            integer(selected["encoder_filter"], 0, facts["max_encoder_filter"],
                    "Power stage encoder filter")
        elif "encoder_filter" in selected:
            fail("Power stage without an angle route rejects encoder_filter")
        if facts.get("pair") is not None and (
                not isinstance(selected["sample_times"], (tuple, list)) or
                len(selected["sample_times"]) != 2 or
                selected["sample_times"][0] != selected["sample_times"][1]):
            fail("Power stage simultaneous sampling requires matching apertures")
        integer(selected["commit_before_ticks"], 1, period - 2,
                "Power stage commit boundary")
        multiple = facts["tick_multiple"]
        if selected["tick_hz"] % multiple:
            fail("Power stage tick_hz must preserve its declared timing unit")
        divisor = selected["tick_hz"] // multiple
        if center:
            if period % 2:
                fail("Power stage center carrier requires an even full period")
            if (selected["trigger_ticks"] <= period // 2 +
                    (selected["max_irq_delay_us"] + 2) * divisor):
                fail("Power stage center update gate cannot precede the trigger")
            if selected["trigger_ticks"] + settling >= period - maximum // 2:
                fail("Power stage center sampling exceeds the low-side window")
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
        divider = row.get("adc_clock_divider", 1)
        if adc % divider or adc < divider:
            fail("Coupled converter clock must preserve its declared divider")
        adc //= divider
        tick = selected["tick_hz"]
        if timer % tick or not 1 <= timer // tick <= row["prescaler_max"]:
            fail("Coupled tick must exactly divide its timer clock")
        cycles = _conversion_cycles(row, selected["sample_times"])
        if cycles * tick >= selected["period_ticks"] * adc:
            fail("Coupled ADC scan does not fit within one PWM period")
        if (selected["mode"] == "finite-injected" and
                (selected["period_ticks"] - selected["trigger_ticks"]) * adc
                <= cycles * tick):
            fail("Coupled injected conversion exceeds its remaining PWM window")
        if "power_stage" in row:
            power = row["power_stage"]
            deadtime_ticks = (selected["deadtime_ticks"] * tick + timer - 1) // timer
            edge = selected["maximum_duty_ticks"]
            if selected.get("center_aligned", False):
                edge //= 2
            if (power.get("dc") is None and edge + deadtime_ticks +
                    selected["settling_ticks"] >= selected["trigger_ticks"]):
                fail("Power stage deadtime/settling exceeds its sample window")
            conversion_us = (cycles * 1000000 + adc - 1) // adc
            conversion = (conversion_us * tick + 999999) // 1000000
            if (selected.get("center_aligned", False) and
                    selected["trigger_ticks"] + conversion +
                    selected["settling_ticks"] >= selected["period_ticks"] - edge):
                fail("Power stage center sampling conversion exceeds its window")
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

    def resolve_fields(self, selected, route, clock_plan):
        """Capture meaning comes from the reviewed route, never MCU switches."""
        from tools.configure.validation import identity
        from tools.configure.ir import PowerStageDcIR, PowerStagePairIR
        row = self.route(route)
        power = row["power_stage"]
        adc = (clock_plan["domains"][row["adc_clock_domain"]]["nominal_hz"]
               // row.get("adc_clock_divider", 1))
        cycles = _conversion_cycles(row, selected["sample_times"])
        span = (cycles * 1000000 + adc - 1) // adc
        to_ticks = lambda us: (us * selected["tick_hz"] + 999999) // 1000000
        dc = power.get("dc")
        timer_hz = clock_plan["domains"][row["timer_clock_domain"]]["nominal_hz"]
        deadtime = (selected["deadtime_ticks"] * selected["tick_hz"] +
                    timer_hz - 1) // timer_hz
        return {**power["capture"],
            "encoder_filter": selected.get("encoder_filter"),
            "center_aligned": selected.get("center_aligned", False),
            "domain": identity(selected.get("domain", selected["id"]),
                               "power stage domain"),
            "scan_span_us": span,
            "pair": (PowerStagePairIR(tuple(power["pair"]["converters"]))
                     if power.get("pair") is not None else None),
            "dc": (PowerStageDcIR(to_ticks(dc["rearm_guard_us"]),
                    to_ticks(span + selected["max_irq_delay_us"]), to_ticks(span),
                    deadtime + selected["settling_ticks"])
                   if dc is not None else None)}

    def domain_resources(self, selected, route):
        """One static domain owns common/IRQ effects across its selected axes."""
        if selected["mode"] != "injected-power-stage":
            return ()
        return self.route(route)["power_stage"]["domain"]["resources"]

    def domain_facts(self, route):
        return self.route(route)["power_stage"]["domain"]

    def resolve_domains(self, selected):
        """Freeze finite static groups; no dynamic scheduler is synthesized."""
        from tools.configure.ir import PowerStageDomainIR
        grouped = {}
        for item in selected:
            if item["kind"] == "pwm_adc" and item["mode"] == "injected-power-stage":
                grouped.setdefault(item["domain"], []).append(item)
        result = []
        for name, members in grouped.items():
            first = members[0]
            row = self.routes[first["route"]]
            facts = row["power_stage"]["domain"]
            carrier = tuple(first[key] for key in
                            ("tick_hz", "period_ticks", "center_aligned"))
            if any(item["dc"] is not None for item in members) and (
                    len(members) != 1 or facts["time_shared"]):
                fail("Power stage DC-link capture requires an exclusive domain")
            indices = set()
            for item in members:
                power = self.routes[item["route"]]["power_stage"]
                if power["domain"] != facts:
                    fail("Power stage domain requires one physical wiring contract")
                if tuple(item[key] for key in
                         ("tick_hz", "period_ticks", "center_aligned")) != carrier:
                    fail("Power stage domain carrier settings must agree")
                if item["irq_priority"] != first["irq_priority"]:
                    fail("Power stage domain IRQ priorities must agree")
                if not facts["time_shared"] and item["adc_index"] in indices:
                    fail("Power stage domain requires distinct independent ADC slots")
                indices.add(item["adc_index"])
            timing = self._resolve_slots(members) if facts["time_shared"] else None
            if not facts["time_shared"] and len(members) != 1 and any(
                    item["pair"] is not None for item in members):
                fail("Power stage paired capture requires an exclusive domain")
            if timing is None:
                self._validate_serial_windows(members)
                for item in members:
                    item["slot"] = None
            result.append(PowerStageDomainIR(name, facts["id"],
                tuple(item["id"] for item in members),
                *(facts[key] for key in ("state_type", "config_type",
                    "member_state_type", "member_config_type", "admit",
                    "prepare", "close", "adc_irq", "handler")), timing))
        return tuple(result)

    def _validate_serial_windows(self, members):
        """Independent converters still serialize callbacks on their common IRQ."""
        total = sum(item["compute_budget_us"] for item in members)
        delay = max(item["max_irq_delay_us"] for item in members)
        for item in members:
            power = self.routes[item["route"]]["power_stage"]
            budget = ((total + delay + power["guard_us"]) * item["tick_hz"] +
                      999999) // 1000000
            scan = (item["scan_span_us"] * item["tick_hz"] + 999999) // 1000000
            if item["trigger_ticks"] + scan + budget >= item["commit_before_ticks"]:
                fail("Power stage domain serial compute budget exceeds a window")

    def _resolve_slots(self, members):
        """Prove two circular capture-to-rearm windows do not overlap."""
        from tools.configure.ir import PowerStageSlotIR, PowerStageTimingIR
        if len(members) != 2 or not all(item["center_aligned"] for item in members):
            fail("Power stage time-shared domain requires two center-aligned axes")
        first = members[0]
        period = first["period_ticks"]
        if {item["phase_offset_ticks"] for item in members} != {0, period // 2}:
            fail("Power stage time-shared slots require half-carrier phase offsets")
        if first["phase_offset_ticks"] != 0:
            fail("Power stage time-shared first member must establish zero phase")
        tolerance = first["start_tolerance_us"]
        if any(item["start_tolerance_us"] != tolerance for item in members):
            fail("Power stage time-shared startup tolerance must agree")
        if any(item["adc_index"] != first["adc_index"] or
               item["pair"] != first["pair"] for item in members):
            fail("Power stage time-shared slots require the same converter route")
        intervals = []
        for item in members:
            scale = item["tick_hz"]
            offset = item["phase_offset_ticks"]
            if offset * 1000000 % scale:
                fail("Power stage time-shared phase requires complete timing units")
            # Each Core-to-carrier anchor contributes one microsecond quantum.
            margin = ((tolerance + 2) * scale + 999999) // 1000000
            begin = period // 2 + offset - margin
            # Conversion, callback and rearm retain the shared converter owner.
            span_us = (item["scan_span_us"] + item["max_irq_delay_us"] +
                       item["compute_budget_us"] +
                       self.routes[item["route"]]["power_stage"]["guard_us"])
            end = (item["trigger_ticks"] + offset + margin +
                   (span_us * scale + 999999) // 1000000)
            intervals.append((begin, end))
        (a, b), (c, d) = intervals
        if any(max(a, c + shift) <= min(b, d + shift)
               for shift in (-period, 0, period)):
            fail("Power stage time slots overlap their capture-to-rearm intervals")
        timing = PowerStageTimingIR(tolerance)
        for item in members:
            item["slot"] = PowerStageSlotIR(item.pop("phase_offset_ticks"), timing)
            item.pop("start_tolerance_us")
        return timing

    def binding_values(self, item, configuration=None):
        del configuration
        if item.kind != "pwm_adc" or item.mode != "injected-power-stage":
            return None
        row = self.routes[item.route]
        power = row["power_stage"]
        return {"power_stage": {**power["hardware"],
            "timer": row["timer"], "adc": row["adc"],
            "encoder": power.get("encoder"),
            "has_encoder": "encoder" in power}}


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
