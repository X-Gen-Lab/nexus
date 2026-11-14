"""Pure planning for the reviewed common-sequencer ADC mechanism.

Package facts supply converter/pad/trigger identities, sample-cycle tables,
clock relationships and supported packing. This module performs no hardware
access, emits no C and selects no vendor or MCU by name.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer, sequence
from tools.configure.records import freeze

COMBINED_BLOCK_MODES = ("regular-software-injected-blocks", "regular-timer-injected-blocks")
BLOCK_MODES = ("regular-blocks", *COMBINED_BLOCK_MODES)
COMBINED_MODES = (*COMBINED_BLOCK_MODES, "regular-auto-injected")


def _fields(value, required):
    if not isinstance(value, Mapping) or set(value) != set(required):
        fail("ADC common mechanism requires exact named facts")


@dataclass(frozen=True)
class AdcGroupPolicy:
    controller: str
    converter_names: tuple
    REGULAR: Mapping
    INJECTED: Mapping
    INJECTED_TRIGGERS: Mapping
    regular_triggers: Mapping
    pad_channels: Mapping
    timer_domains: Mapping
    sample_cycles: tuple
    resolution_bits: tuple
    reference_mv_max: int
    external_channel_max: int
    regular_ranks: int
    injected_ranks: int
    delay_cycles: tuple
    default_trigger: str
    adc_clock_domain: str
    clock_fallbacks: Mapping
    layouts: Mapping

    @classmethod
    def from_validated(cls, value, hardware):
        required = {"mechanism", "controller", "converter_names", "regular",
            "injected", "injected_triggers", "timer_domains", "sample_cycles",
            "resolution_bits", "reference_mv_max", "external_channel_max",
            "regular_ranks", "injected_ranks", "delay_cycles", "default_trigger",
            "adc_clock_domain", "clock_fallbacks", "layouts"}
        _fields(value, required)
        if value["mechanism"] != "common-sequencer-v1":
            fail("ADC common mechanism is not reviewed")
        names = tuple(value["converter_names"])
        if not names or len(names) > 3 or len(set(names)) != len(names):
            fail("ADC converter identities require one to three distinct owners")
        for name in (value["controller"], *names):
            Symbol(name)
        profiles = []
        for key in ("regular", "injected"):
            raw = value[key]
            if not isinstance(raw, Mapping) or not raw:
                fail("ADC requires explicit operation profiles")
            decoded = {}
            for name, row in raw.items():
                if not isinstance(row, (list, tuple)) or len(row) != 2:
                    fail("ADC operation requires converter count and semantic code")
                integer(row[0], 1, len(names), "ADC operation converter count")
                Symbol(row[1])
                decoded[name] = tuple(row)
            profiles.append(MappingProxyType(decoded))
        pads = {}
        for row in hardware.get("adc_channels", ()):
            _fields(row, {"controllers", "pins"})
            for converter in row["controllers"]:
                if converter not in names:
                    continue
                for pin, channel in row["pins"].items():
                    Symbol(pin)
                    integer(channel, 0, value["external_channel_max"], "ADC pad channel")
                    identity = (converter, pin)
                    if identity in pads:
                        fail("ADC duplicate converter/pad fact")
                    pads[identity] = channel
        if any(not any(owner == name for owner, _ in pads) for name in names):
            fail("ADC converter lacks external pad facts")
        triggers = hardware.get("adc_triggers", {})
        domains = value["timer_domains"]
        fallbacks = value["clock_fallbacks"]
        if not isinstance(domains, Mapping) or not isinstance(fallbacks, Mapping):
            fail("ADC clocks require named explicit domains")
        for name, row in fallbacks.items():
            Symbol(name)
            if not isinstance(row, (tuple, list)) or len(row) != 3:
                fail("ADC fallback clock requires source, multiplier and divisor")
            Symbol(row[0])
            integer(row[1], 1, context="ADC clock multiplier")
            integer(row[2], 1, context="ADC clock divisor")
        for timer in set(triggers.values()) | set(value["injected_triggers"].values()):
            Symbol(timer)
            if timer not in domains or domains[timer] not in fallbacks:
                fail("ADC trigger lacks an explicit clock domain")
        if value["adc_clock_domain"] not in fallbacks or value["default_trigger"] not in triggers:
            fail("ADC requires explicit common clock and default trigger")
        cycles = tuple(value["sample_cycles"])
        resolutions = tuple(value["resolution_bits"])
        if not cycles or not resolutions or len(set(resolutions)) != len(resolutions):
            fail("ADC requires nonempty sample cycles and resolutions")
        for item in (*cycles, *resolutions):
            integer(item, 1, context="ADC conversion cycle or resolution")
        for name in ("reference_mv_max", "external_channel_max", "regular_ranks", "injected_ranks"):
            integer(value[name], 1, context="ADC " + name)
        delay = tuple(value["delay_cycles"])
        if len(delay) != 2 or not 0 < delay[0] <= delay[1]:
            fail("ADC requires explicit delay bounds")
        layouts = value["layouts"]
        if not isinstance(layouts, Mapping) or set(layouts) != set(profiles[0]) | set(profiles[1]):
            fail("ADC operation packing requires complete named layouts")
        for row in layouts.values():
            _fields(row, {"repeats", "dma_word_bytes", "byte_dma_word_bytes"})
            if (row["repeats"] not in (1, 2) or
                    row["dma_word_bytes"] not in (2, 4) or
                    row["byte_dma_word_bytes"] not in (2, 4)):
                fail("ADC packing lacks a reviewed common-sequencer layout")
        return cls(value["controller"], names, *profiles,
            freeze(value["injected_triggers"]), freeze(triggers), freeze(pads),
            freeze(domains), cycles, resolutions, value["reference_mv_max"],
            value["external_channel_max"], value["regular_ranks"], value["injected_ranks"],
            delay, value["default_trigger"], value["adc_clock_domain"],
            freeze(fallbacks), freeze(layouts))

    def _physical(self, index, channel, route=None):
        authorized = (None if route is None or "pins" not in route else
                      {pin["pin"] for pin in route["pins"]})
        pins = [pin for (owner, pin), number in self.pad_channels.items()
                if owner == self.converter_names[index] and number == channel
                and (authorized is None or pin in authorized)]
        if len(pins) != 1:
            fail("ADC channel lacks one unambiguous physical pad")
        return pins[0]

    def _clock(self, domain, clock, plan):
        if plan is not None:
            try:
                return plan["domains"][domain]["nominal_hz"]
            except KeyError:
                fail("ADC clock plan lacks the selected domain")
        source, multiplier, divisor = self.clock_fallbacks[domain]
        if source not in clock or clock[source] * multiplier % divisor:
            fail("ADC clock relationship is not exactly representable")
        return clock[source] * multiplier // divisor

    def _profile(self, selection):
        return (self.INJECTED if selection["mode"] == "injected-sequence" else self.REGULAR)


    def _triggers(self, selection):
        return (self.INJECTED_TRIGGERS if selection["mode"] == "injected-sequence"
                else self.regular_triggers)


    def validate(self, selection, route):
        """Admit exact selected modes and the continuous service contract."""
        operation = selection.get("operation")
        profile = self._profile(selection)
        if (route["controller"] != self.controller or type(operation) is not str or
                operation not in profile):
            fail("ADC group requires an exact common-domain operation")
        blocks = selection["mode"] in BLOCK_MODES
        automatic = selection["mode"] == "regular-auto-injected"
        if selection["mode"] in COMBINED_BLOCK_MODES or automatic:
            if operation not in ("dual-simultaneous", "triple-simultaneous"):
                fail("ADC blocks/automatic injection require dual/triple simultaneous sampling")
        if blocks:
            integer(selection.get("max_service_gap_us"), 1, 0xffffffff,
                    "ADC group max_service_gap_us")
        elif "max_service_gap_us" in selection:
            fail("ADC max_service_gap_us requires regular-blocks mode")
        if selection.get("exclusive") is not True or "irq_priority" not in selection:
            fail("ADC group requires exclusive ownership and explicit IRQ priority")
        channels = sequence(selection.get("channels"), "ADC group channels")
        samples = sequence(selection.get("sample_times"), "ADC group sample_times")
        count, _ = profile[operation]
        limit = self.injected_ranks if selection["mode"] == "injected-sequence" else self.regular_ranks
        if (not channels or len(channels) % count or
                len(channels) // count > limit or len(samples) != len(channels)):
            fail("ADC group requires complete equal-length converter sequences")
        bits = selection.setdefault("resolution_bits", max(self.resolution_bits))
        if type(bits) is not int or bits not in self.resolution_bits:
            fail("ADC group resolution lacks a selected supported mode")
        integer(selection.get("reference_mv"), 1, self.reference_mv_max, "ADC group reference_mv")
        for channel in channels:
            integer(channel, 0, self.external_channel_max, "ADC group external channel")
        for sample in samples:
            integer(sample, 0, len(self.sample_cycles) - 1, "ADC group sample time")
        timings = {}
        for index, (channel, sample) in enumerate(zip(channels, samples)):
            converter = index % count + 1
            key = converter, channel
            if key in timings and timings[key] != sample:
                fail("ADC group repeats require a consistent physical sample time")
            timings[key] = sample
            if sample != samples[index - index % count]:
                fail("ADC group requires matching sample times at each rank")
        interleaved = "interleaved" in operation
        if interleaved:
            if (len(channels) != count or len(set(channels)) != 1 or
                    len({self._physical(i, channels[i], route)
                         for i in range(count)}) != 1):
                fail("ADC interleaved requires one identical physical input")
        else:
            for start in range(0, len(channels), count):
                physical = {self._physical(i, channels[start + i], route)
                            for i in range(count)}
                if len(physical) != count:
                    fail("ADC simultaneous cannot sample one physical input twice")
        for field, low, high, default in (("delay_cycles", *self.delay_cycles, self.delay_cycles[0]),
                                          ("drain_timeout_us", 1, 0xffffffff, 1000),
                                          ("period_us", 0, 0xffffffff, 0)):
            integer(selection.setdefault(field, default), low, high,
                    "ADC group " + field)
        source = selection.setdefault("trigger", "software" if interleaved or
            selection["mode"] == "injected-sequence" else self.default_trigger)
        if type(source) is not str or (source != "software" and
                                        source not in self._triggers(selection)):
            fail("ADC group trigger lacks an independently owned internal event")
        if interleaved and source != "software":
            fail("ADC interleaved starts one finite continuous sequence in software")
        if (not interleaved and selection["mode"] in
                ("regular-sequence-dma", "regular-auto-injected", *BLOCK_MODES) and
                source == "software"):
            fail("ADC simultaneous regular acquisition requires a timer trigger")
        if source != "software" and selection["period_us"] == 0:
            fail("ADC timer trigger requires a positive period_us")
        if source == "software":
            if selection["period_us"] != 0:
                fail("ADC software trigger requires period_us=0")
        if selection["mode"] in COMBINED_MODES:
            self._validate_injected(selection, count, route)
        elif "injected" in selection:
            fail("ADC injected ranks require the explicit combined mode")
        if "watchdog" in selection:
            thresholds = selection["watchdog"]
            if type(thresholds) is not dict or set(thresholds) != {"lower", "upper"}:
                fail("ADC watchdog requires lower and upper raw thresholds")
            for name, value in thresholds.items():
                integer(value, 0, (1 << bits) - 1, "ADC watchdog " + name)
            if thresholds["lower"] > thresholds["upper"]:
                fail("ADC watchdog lower exceeds upper")


    def _validate_injected(self, selection, count, route):
        injected = selection.get("injected")
        automatic = selection["mode"] == "regular-auto-injected"
        required = {"channels", "sample_times"}
        if not automatic:
            required.add("min_interval_us")
        timed = selection["mode"] == "regular-timer-injected-blocks"
        if timed:
            required |= {"trigger", "delay_us"}
        if type(injected) is not dict or set(injected) != required:
            fail("ADC injected requires exact " + "/".join(sorted(required)))
        if timed:
            source = injected["trigger"]
            if type(source) is not str or source not in self.INJECTED_TRIGGERS:
                fail("ADC injected trigger lacks a reviewed timer event")
            if self.INJECTED_TRIGGERS[source] == self.regular_triggers[selection["trigger"]]:
                fail("ADC regular and injected timer resources conflict")
            integer(injected["delay_us"], 1, 0xffffffff, "injected delay_us")
        channels = sequence(injected["channels"], "injected channels")
        samples = sequence(injected["sample_times"], "injected sample times")
        if (not channels or len(channels) % count or
                len(channels) // count > self.injected_ranks or len(samples) != len(channels)):
            fail("ADC requires complete injected sequences within the reviewed rank bound")
        if automatic:
            injected["min_interval_us"] = 0
        else:
            interval = integer(injected["min_interval_us"], 1, 0xffffffff,
                               "injected min_interval_us")
            if interval < selection["period_us"] + 2:
                fail("injected min_interval_us must exceed regular period by two ticks")
        for channel, sample in zip(channels, samples):
            integer(channel, 0, self.external_channel_max, "injected channel")
            integer(sample, 0, len(self.sample_cycles) - 1, "injected sample time")
        timing = {(i % count, channel): sample for i, (channel, sample) in
                  enumerate(zip(selection["channels"], selection["sample_times"]))}
        for i, (channel, sample) in enumerate(zip(channels, samples)):
            if sample != samples[i - i % count]:
                fail("ADC injected simultaneous sampling codes must match")
            key = i % count, channel
            if key in timing and timing[key] != sample:
                fail("ADC regular/injected sampling code conflicts on one channel")
            timing[key] = sample
            if i % count == 0:
                rank = {self._physical(j, channels[i + j], route)
                        for j in range(count)}
                if len(rank) != count:
                    fail("ADC injected simultaneous cannot sample one physical pad twice")


    def selected_route(self, selection, route):
        """Acquire only explicitly authorized physical inputs, once per pad."""
        count, _ = self._profile(selection)[selection["operation"]]
        sequences = [selection["channels"]]
        if "injected" in selection:
            sequences.append(selection["injected"]["channels"])
        pins = {self._physical(index % count, channel, route)
                for channels in sequences
                for index, channel in enumerate(channels)}
        if not pins.issubset({pin["pin"] for pin in route["pins"]}):
            fail("ADC group sequence exceeds the authorized physical input route")
        return {**route, "pins": [pin for pin in route["pins"] if pin["pin"] in pins]}


    def trigger_resources(self, selection):
        source = selection["trigger"]
        if source == "software":
            return ()
        timers = [self._triggers(selection)[source]]
        if selection["mode"] == "regular-timer-injected-blocks":
            timers.append(self.INJECTED_TRIGGERS[selection["injected"]["trigger"]])
        return tuple(resource for timer in timers
                     for resource in ("controller:" + timer, "timer:" + timer))


    def resolve(self, selection, *, part, clock, clock_plan):
        """Derive one rank/layout/clock plan without a separate review engine."""
        del part
        count, _ = self._profile(selection)[selection["operation"]]
        ranks = len(selection["channels"]) // count
        layout = self.layouts[selection["operation"]]
        repeated = layout["repeats"] == 2
        total = len(selection["channels"]) * (2 if repeated else 1)
        injected_mode = selection["mode"] == "injected-sequence"
        storage = (1 if "interleaved" in selection["operation"] and
                   selection["resolution_bits"] <= 8 else 2)
        width = layout["byte_dma_word_bytes"] if storage == 1 else layout["dma_word_bytes"]
        adc_clock = self._clock(self.adc_clock_domain, clock, clock_plan)
        source = selection["trigger"]
        timer_hz = 0
        if source != "software":
            timer = self._triggers(selection)[source]
            timer_hz = self._clock(self.timer_domains[timer], clock, clock_plan)
            cycles = sum(self.sample_cycles[sample] +
                         selection["resolution_bits"]
                         for sample in selection["sample_times"][::count])
            if (selection["mode"] != "regular-auto-injected" and
                    selection["period_us"] * adc_clock <= cycles * 1000000):
                fail("ADC group period_us does not exceed its full conversion budget")
        extra = {}
        if "injected" in selection:
            injected = selection["injected"]
            injected_ranks = len(injected["channels"]) // count
            costs = tuple(self.sample_cycles[code] +
                          selection["resolution_bits"]
                          for code in selection["sample_times"][::count])
            injected_cost = sum(self.sample_cycles[code] +
                                selection["resolution_bits"]
                                for code in injected["sample_times"][::count])
            if (selection["mode"] != "regular-auto-injected" and
                    selection["period_us"] * adc_clock <=
                    (sum(costs) + injected_cost + max(costs) + 2) * 1000000):
                fail("ADC combined preemption exceeds the full conversion budget")
            extra = dict(injected_ranks=injected_ranks,
                         injected_converters=tuple(range(1, count + 1)) * injected_ranks)
            if selection["mode"] == "regular-timer-injected-blocks":
                source = injected["trigger"]
                timer = self.INJECTED_TRIGGERS[source]
                injected_timer_hz = self._clock(self.timer_domains[timer], clock, clock_plan)
                numerator = injected["delay_us"] * injected_timer_hz
                if numerator % 1000000 or not 0 < numerator // 1000000 <= 1 << 32:
                    fail("ADC injected timer delay is not exactly representable")
                ticks = numerator // 1000000
                divisor = next((value for value in range((ticks - 1) // 65536 + 1,
                                                         65537)
                                if ticks % value == 0 and ticks // value <= 65536),
                               None)
                if divisor is None:
                    fail("ADC injected timer delay has no exact PSC/ARR plan")
                extra["injected_timer"] = dict(source=source, timer=timer,
                    index=tuple(self.INJECTED_TRIGGERS).index(source),
                    delay_us=injected["delay_us"], timer_clock_hz=injected_timer_hz,
                    prescaler=divisor - 1, period=ticks // divisor - 1,
                    trigger_identity=0)
        maximum = (1 if injected_mode or selection["mode"] == "regular-auto-injected"
                   else 65535 * width // (total * storage))
        return dict(converters=tuple(range(1, count + 1)) * ranks * (2 if repeated else 1),
                    ranks=ranks, sample_stride=storage, scan_stride=total * storage,
                    max_frames=maximum,
                    adc_clock_hz=adc_clock, timer_clock_hz=timer_hz,
                    trigger_identity=0, **extra)
