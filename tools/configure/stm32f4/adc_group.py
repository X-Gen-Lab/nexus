"""F407 common-ADC data layouts and static connections, without C algorithms."""
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer, sequence
from .hardware import ADC_PIN_CHANNELS, ADC_TRIGGERS

REGULAR = MappingProxyType({
    "dual-simultaneous": (2, "DUAL_REGULAR"),
    "triple-simultaneous": (3, "TRIPLE_REGULAR"),
    "dual-interleaved": (2, "DUAL_INTERLEAVED"),
    "triple-interleaved": (3, "TRIPLE_INTERLEAVED"),
})
INJECTED = MappingProxyType({"single": (1, "SINGLE_INJECTED"),
                            "dual": (2, "DUAL_INJECTED"),
                            "triple": (3, "TRIPLE_INJECTED")})
INJECTED_TRIGGERS = MappingProxyType({
    "tim1-ch4": "TIM1", "tim1-trgo": "TIM1", "tim3-ch2": "TIM3",
    "tim3-ch4": "TIM3", "tim4-ch1": "TIM4", "tim4-ch2": "TIM4",
    "tim4-ch3": "TIM4", "tim4-trgo": "TIM4", "tim5-ch4": "TIM5",
    "tim5-trgo": "TIM5", "tim8-ch2": "TIM8", "tim8-ch3": "TIM8",
    "tim8-ch4": "TIM8",
})


COMBINED_BLOCK_MODES = ("regular-software-injected-blocks",
                        "regular-timer-injected-blocks")
BLOCK_MODES = ("regular-blocks", *COMBINED_BLOCK_MODES)
COMBINED_MODES = (*COMBINED_BLOCK_MODES, "regular-auto-injected")


def _profile(selection):
    return (INJECTED if selection["mode"] == "injected-sequence" else REGULAR)


def _triggers(selection):
    return (INJECTED_TRIGGERS if selection["mode"] == "injected-sequence"
            else ADC_TRIGGERS)


def validate(selection, route):
    """Admit exact selected modes and the continuous service contract."""
    operation = selection.get("operation")
    profile = _profile(selection)
    if (route["controller"] != "ADC123" or type(operation) is not str or
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
    limit = 4 if selection["mode"] == "injected-sequence" else 16
    if (not channels or len(channels) % count or
            len(channels) // count > limit or len(samples) != len(channels)):
        fail("ADC group requires complete equal-length converter sequences")
    bits = selection.setdefault("resolution_bits", 12)
    if type(bits) is not int or bits not in (6, 8, 10, 12):
        fail("ADC group resolution must be 6, 8, 10 or 12")
    integer(selection.get("reference_mv"), 1, 3600, "ADC group reference_mv")
    for channel in channels:
        integer(channel, 0, 15, "ADC group external channel")
    for sample in samples:
        integer(sample, 0, 7, "ADC group sample time")
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
                (count == 3 and channels[0] not in (0, 1, 2, 3, 10, 11, 12, 13))):
            fail("ADC interleaved requires one identical physical input")
    else:
        inputs = {(converter, number): pin for (converter, pin), number
                  in ADC_PIN_CHANNELS.items()}
        for start in range(0, len(channels), count):
            physical = {inputs[(f"ADC{i + 1}", channels[start + i])]
                        for i in range(count)}
            if len(physical) != count:
                fail("ADC simultaneous cannot sample one physical input twice")
    for field, low, high, default in (("delay_cycles", 5, 20, 5),
                                      ("drain_timeout_us", 1, 0xffffffff, 1000),
                                      ("period_us", 0, 0xffffffff, 0)):
        integer(selection.setdefault(field, default), low, high,
                "ADC group " + field)
    source = selection.setdefault("trigger", "software" if interleaved or
        selection["mode"] == "injected-sequence" else "tim3-trgo")
    if type(source) is not str or (source != "software" and
                                    source not in _triggers(selection)):
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
        _validate_injected(selection, count)
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


def _validate_injected(selection, count):
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
        if type(source) is not str or source not in INJECTED_TRIGGERS:
            fail("ADC injected trigger is not a reviewed Table69 timer")
        if INJECTED_TRIGGERS[source] == ADC_TRIGGERS[selection["trigger"]]:
            fail("ADC regular and injected timer resources conflict")
        integer(injected["delay_us"], 1, 0xffffffff, "injected delay_us")
    channels = sequence(injected["channels"], "injected channels")
    samples = sequence(injected["sample_times"], "injected sample times")
    if (not channels or len(channels) % count or
            len(channels) // count > 4 or len(samples) != len(channels)):
        fail("ADC requires complete injected converter sequences up to four ranks")
    if automatic:
        injected["min_interval_us"] = 0
    else:
        interval = integer(injected["min_interval_us"], 1, 0xffffffff,
                           "injected min_interval_us")
        if interval < selection["period_us"] + 2:
            fail("injected min_interval_us must exceed regular period by two ticks")
    for channel, sample in zip(channels, samples):
        integer(channel, 0, 15, "injected channel")
        integer(sample, 0, 7, "injected sample time")
    timing = {(i % count, channel): sample for i, (channel, sample) in
              enumerate(zip(selection["channels"], selection["sample_times"]))}
    pads = {(int(converter[3:]) - 1, channel): pin
            for (converter, pin), channel in ADC_PIN_CHANNELS.items()}
    for i, (channel, sample) in enumerate(zip(channels, samples)):
        if sample != samples[i - i % count]:
            fail("ADC injected simultaneous sampling codes must match")
        key = i % count, channel
        if key in timing and timing[key] != sample:
            fail("ADC regular/injected sampling code conflicts on one channel")
        timing[key] = sample
        if i % count == 0:
            rank = {pads[(j, channels[i + j])] for j in range(count)}
            if len(rank) != count:
                fail("ADC injected simultaneous cannot sample one physical pad twice")


def selected_route(selection, route):
    """Acquire only explicitly authorized physical inputs, once per pad."""
    count, _ = _profile(selection)[selection["operation"]]
    sequences = [selection["channels"]]
    if "injected" in selection:
        sequences.append(selection["injected"]["channels"])
    pins = {pin for channels in sequences for index, channel in enumerate(channels)
            for (converter, pin), number in ADC_PIN_CHANNELS.items()
            if converter == f"ADC{index % count + 1}" and number == channel}
    if not pins.issubset({pin["pin"] for pin in route["pins"]}):
        fail("ADC group sequence exceeds the authorized physical input route")
    return {**route, "pins": [pin for pin in route["pins"] if pin["pin"] in pins]}


def trigger_resources(selection):
    source = selection["trigger"]
    if source == "software":
        return ()
    timers = [_triggers(selection)[source]]
    if selection["mode"] == "regular-timer-injected-blocks":
        timers.append(INJECTED_TRIGGERS[selection["injected"]["trigger"]])
    return tuple(resource for timer in timers
                 for resource in ("controller:" + timer, "timer:" + timer))


def resolve(selection, *, part, clock, clock_plan):
    """Derive one rank/layout/clock plan without a separate review engine."""
    del part
    count, _ = _profile(selection)[selection["operation"]]
    ranks = len(selection["channels"]) // count
    repeated = selection["operation"] == "triple-interleaved"
    total = len(selection["channels"]) * (2 if repeated else 1)
    injected_mode = selection["mode"] == "injected-sequence"
    storage = (1 if "interleaved" in selection["operation"] and
               selection["resolution_bits"] <= 8 else 2)
    width = (2 if selection["operation"] == "triple-simultaneous" or
             storage == 1 else 4)
    adc_clock = (clock_plan["domains"]["adc"]["nominal_hz"]
                 if clock_plan is not None else clock["apb2_hz"] // 4)
    source = selection["trigger"]
    timer_hz = 0
    if source != "software":
        timer = _triggers(selection)[source]
        bus = "apb2" if timer in {"TIM1", "TIM8"} else "apb1"
        timer_hz = (clock_plan["domains"][bus + "_timer"]["nominal_hz"]
                    if clock_plan is not None else clock[bus + "_hz"] * 2)
        cycles = sum((3, 15, 28, 56, 84, 112, 144, 480)[sample] +
                     selection["resolution_bits"]
                     for sample in selection["sample_times"][::count])
        if (selection["mode"] != "regular-auto-injected" and
                selection["period_us"] * adc_clock <= cycles * 1000000):
            fail("ADC group period_us does not exceed its full conversion budget")
    extra = {}
    if "injected" in selection:
        injected = selection["injected"]
        injected_ranks = len(injected["channels"]) // count
        costs = tuple((3, 15, 28, 56, 84, 112, 144, 480)[code] +
                      selection["resolution_bits"]
                      for code in selection["sample_times"][::count])
        injected_cost = sum((3, 15, 28, 56, 84, 112, 144, 480)[code] +
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
            timer = INJECTED_TRIGGERS[source]
            bus = "apb2" if timer in {"TIM1", "TIM8"} else "apb1"
            injected_timer_hz = (clock_plan["domains"][bus + "_timer"]["nominal_hz"]
                        if clock_plan is not None else clock[bus + "_hz"] * 2)
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
                index=tuple(INJECTED_TRIGGERS).index(source),
                delay_us=injected["delay_us"], timer_clock_hz=injected_timer_hz,
                prescaler=divisor - 1, period=ticks // divisor - 1,
                trigger_identity=0)
    return dict(converters=tuple(range(1, count + 1)) * ranks * (2 if repeated else 1),
                ranks=ranks, sample_stride=storage, scan_stride=total * storage,
                max_frames=1 if injected_mode or selection["mode"] == "regular-auto-injected" else 65535 * width // (total * storage),
                adc_clock_hz=adc_clock, timer_clock_hz=timer_hz,
                trigger_identity=0, **extra)


def constructor(item, result):
    """Emit only immutable facts and explicit lifecycle/vector connections."""
    from .bindings import dma_regions
    o = item.options
    name = item.id.replace("-", "_")
    symbol = "s_nx_adc_group_" + name
    _, mode = (INJECTED if item.mode == "injected-sequence" else REGULAR)[o.operation]
    definitions = []
    repeat = 2 if o.operation == "triple-interleaved" else 1
    for label, values in (("channels", o.channels * repeat),
                          ("samples", o.sample_times * repeat),
                          ("converters", o.converters)):
        data = ", ".join(str(value) + "u" for value in values)
        definitions.append(f"static const uint8_t {symbol}_{label}[] = {{ {data} }};")
    blocks = item.mode in BLOCK_MODES
    automatic = item.mode == "regular-auto-injected"
    combined = item.mode in COMBINED_MODES
    dma = item.mode in ("regular-sequence-dma", "regular-auto-injected", *BLOCK_MODES)
    if combined:
        for label, values in (("channels", o.injected.channels),
                              ("samples", o.injected.sample_times),
                              ("converters", o.injected.converters)):
            data = ", ".join(str(value) + "u" for value in values)
            definitions.append(f"static const uint8_t {symbol}_injected_{label}[] = {{ {data} }};")
    if combined and o.injected.timer is not None:
        t = o.injected.timer
        definitions += [
            f"static const nx_stm32_adc_group_injected_timer_t {symbol}_injected_timer = {{",
            f"    .timer = {t.timer}, .trigger = {t.index}u,",
            f"    .timer_clock_hz = {t.timer_clock_hz}u,",
            f"    .trigger_identity = {t.trigger_identity}u, .delay_us = {t.delay_us}u,",
            f"    .prescaler = {t.prescaler}u, .period = {t.period}u,",
            "};"]
    if dma:
        region_defs, region_count = dma_regions(name, result)
        definitions.extend(region_defs)
    trigger_map = INJECTED_TRIGGERS if not dma else ADC_TRIGGERS
    timer = trigger_map.get(o.trigger, "NULL")
    index = tuple(trigger_map).index(o.trigger) if o.trigger != "software" else 0
    fields = [
        "    .adc = {ADC1, ADC2, ADC3}, .common = ADC123_COMMON, .rcc = RCC,",
        f"    .timer = {timer}, .trigger = {index}u,",
        f"    .channels = {symbol}_channels, .sample_times = {symbol}_samples,",
        f"    .converters = {symbol}_converters, .ranks = {o.ranks}u,",
        f"    .adc_clock_hz = {o.adc_clock_hz}u, .timer_clock_hz = {o.timer_clock_hz}u,",
        f"    .period_us = {o.period_us}u, .drain_timeout_us = {o.drain_timeout_us}u,",
        f"    .trigger_identity = {o.trigger_identity}u,",
        f"    .resolution = {(12 - o.resolution_bits) // 2}u, .delay_cycles = {o.delay_cycles}u,",
        f"    .priority = {o.irq.priority}u, .mode = NX_STM32_ADC_{mode},"]
    if o.watchdog is not None:
        fields += [f"    .watchdog = true, .lower = {o.watchdog.lower}u, .upper = {o.watchdog.upper}u,"]
    if dma:
        fields += ["    .dma = DMA2, .memory = DMA2_Stream0,",
            f"    .regions = s_nx_dma_regions_{name}, .region_count = {region_count}u,"]
    implementation = ("adc_group_automatic" if automatic else
                      "adc_group_combined" if combined else
                      "adc_group_blocks" if blocks else "adc_group")
    config_type = "adc_group_combined" if automatic else implementation
    definitions += [f"static const nx_stm32_{config_type}_config_t {symbol} = {{"]
    if blocks or automatic:
        block_fields = ["    .adc = {", *["    " + line for line in fields],
            "    },", f"    .max_service_gap_us = {o.max_service_gap_us}u"]
        if combined:
            definitions += ["    .regular = {", *["    " + line for line in block_fields],
                "    },",
                f"    .injected_channels = {symbol}_injected_channels,",
                f"    .injected_samples = {symbol}_injected_samples,",
                f"    .injected_converters = {symbol}_injected_converters,",
                f"    .min_injected_interval_us = {o.injected.min_interval_us}u,",
                f"    .injected_ranks = {o.injected.ranks}u,"]
            if o.injected.timer is not None:
                definitions += [f"    .injected_timer = &{symbol}_injected_timer,"]
        else:
            definitions += block_fields
    else:
        definitions += fields
    definitions += ["};"]
    for vector in (("ADC", "DMA2_Stream0") if dma else ("ADC",)):
        definitions += [f"void {vector}_IRQHandler(void);",
            r"/** \brief Exact selected common-ADC owner publication. */",
            f"void {vector}_IRQHandler(void) {{ nx_stm32_{implementation}_irq(&s_nx_face_{name}); }}"]
    state = Address("s_nx_port_" + name)
    return Construction(state_type=f"nx_stm32_{implementation}_state_t",
        definitions=definitions,
        initialize=DirectCall(f"nx_stm32_{implementation}_prepare", (state, Address(symbol))),
        stop=[DirectCall(f"nx_stm32_{implementation}_close", (state,))],
        retain_on_failed_init=True)
