"""Reviewed F407 timer topology and static waveform owner construction."""
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, fail, integer
from .hardware import PWM_PIN_CHANNELS, PWM_TIMERS

KINDS = frozenset(("pwm_group", "timer_pulse", "timer_sequence"))
MODES = MappingProxyType({"pwm_group": "coherent-pwm",
                         "timer_pulse": "one-pulse", "timer_sequence": "dma-burst"})
FIELDS = frozenset(("tick_hz", "period_ticks", "deadtime_ticks", "exclusive",
                    "poll_limit", "break_active_high", "idle_high",
                    "idle_complement_high", "irq_priority", "calls_os",
                    "master_trigger", "synchronization"))
DMA = MappingProxyType({1: (2, 5, 6), 3: (1, 2, 5), 4: (1, 6, 2),
                       5: (1, 0, 6), 8: (2, 1, 7)})
# RM0090 Rev22 Tables94/98/101. OC inputs are deliberately not TRGO aliases.
ITR = MappingProxyType({1: (5, 2, 3, 4), 8: (1, 2, 4, 5),
                       3: (1, 2, 5, 4), 4: (1, 2, 3, 8), 5: (2, 3, 4, 8),
                       9: (2, 3, "TIM10_OC", "TIM11_OC"),
                       12: (4, 5, "TIM13_OC", "TIM14_OC")})
MASTER = MappingProxyType({"none": 0, "enable": 1, "update": 2, "oc4ref": 7})
SYNC = MappingProxyType({"none": 0, "reset": 4, "gated": 5, "trigger": 6})


@dataclass(frozen=True)
class WaveformRoute:
    controller: int
    kind: str
    channels: tuple[int, ...]
    pins: tuple[tuple[str, str, int], ...]
    complementary: bool = False
    internal_trigger: bool = False


def _normal(number, channels):
    if number in (1, 8):
        pins = tuple((f"PA{channel + 7}" if number == 1 else f"PC{channel + 5}",
                      f"ch{channel}", 1 if number == 1 else 3) for channel in channels)
    else:
        pins = tuple((next(pin for (timer, pin), value in PWM_PIN_CHANNELS.items()
                           if timer == f"TIM{number}" and value == channel),
                      f"ch{channel}", PWM_TIMERS[f"TIM{number}"].af) for channel in channels)
    if number in (1, 8):
        pins += (("PB12", "break", 1),) if number == 1 else (("PA6", "break", 3),)
    return pins


def _routes():
    result = {}
    for number in (1, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14):
        count = 2 if number in (9, 12) else 1 if number in (10, 11, 13, 14) else 4
        channels = tuple(range(1, count + 1))
        if count > 1:
            result[f"tim{number}-group{count}"] = WaveformRoute(number, "pwm_group", channels, _normal(number, channels))
        for width in sorted({1, count}):
            selection = tuple(range(1, width + 1))
            result[f"tim{number}-pulse{width}"] = WaveformRoute(number, "timer_pulse", selection, _normal(number, selection))
        if number in DMA:
            result[f"tim{number}-sequence"] = WaveformRoute(number, "timer_sequence", channels, _normal(number, channels))
    for number in (1, 8):
        primary = _normal(number, (1, 2, 3))
        complementary = ("PB13", "PB14", "PB15") if number == 1 else ("PA7", "PB0", "PB1")
        pins = tuple(pin for index in range(3) for pin in (
            primary[index], (complementary[index], f"ch{index + 1}n", primary[index][2]))) + primary[-1:]
        result[f"tim{number}-group-complementary"] = WaveformRoute(number, "pwm_group", (1, 2, 3), pins, True, True)
    return MappingProxyType(result)


ROUTES = _routes()


def controller_interfaces():
    return {f"TIM{number}": {kind: (MODES[kind],) for kind in sorted(KINDS)
                            if any(route.controller == number and route.kind == kind for route in ROUTES.values())}
            for number in sorted({route.controller for route in ROUTES.values()})}


def extra_pins(controller):
    return tuple(dict.fromkeys(pin for route in ROUTES.values()
                               if controller == f"TIM{route.controller}"
                               for pin, _, _ in route.pins))


def validate(selection, route):
    facts = ROUTES.get(route["id"])
    actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    if (facts is None or facts.kind != route["kind"] or
            route["controller"] != f"TIM{facts.controller}" or actual != facts.pins):
        fail("Timer waveform requires its exact reviewed channel/pad route")
    required = FIELDS - {"calls_os", "master_trigger", "synchronization"}
    if required - selection.keys():
        fail("Timer waveform requires explicit units, safe levels, budget and IRQ")
    integer(selection["tick_hz"], 1, context="Timer tick_hz")
    integer(selection["period_ticks"], 1, 0xffffffff if facts.controller == 5 and facts.kind != "timer_sequence" else 65535, "Timer period_ticks")
    integer(selection["poll_limit"], 1, 10000000, "Timer poll_limit")
    deadtime = integer(selection["deadtime_ticks"], 0, 1008, "Timer deadtime")
    exact = (deadtime <= 127 or 128 <= deadtime <= 254 and deadtime % 2 == 0 or
             256 <= deadtime <= 504 and deadtime % 8 == 0 or 512 <= deadtime <= 1008 and deadtime % 16 == 0)
    if not exact or (facts.controller not in (1, 8) and deadtime):
        fail("Timer deadtime has no exact selected hardware encoding")
    for field in ("break_active_high", "idle_high", "idle_complement_high", "exclusive"):
        if type(selection[field]) is not bool:
            fail("Timer waveform safe policy requires explicit booleans")
    if not selection["exclusive"] or selection.get("calls_os", False):
        fail("Timer waveform requires one exclusive owner and non-OS IRQs")
    master = selection.get("master_trigger", "oc4ref" if facts.internal_trigger else "none")
    if (not isinstance(master, str) or master not in MASTER or
            (master != "none" and facts.controller not in DMA) or
            (master == "oc4ref" and not facts.internal_trigger)):
        fail("Timer master trigger is unavailable for its selected channels")
    sync = selection.get("synchronization")
    if sync is not None and (not isinstance(sync, dict) or set(sync) != {"source", "mode"} or
            not isinstance(sync["source"], str) or not sync["source"] or
            not isinstance(sync["mode"], str) or sync["mode"] not in SYNC or
            sync["mode"] == "none" or facts.controller not in ITR):
        fail("Timer synchronization requires an explicit source and real ITR input")
    if facts.internal_trigger and selection["period_ticks"] < 2:
        fail("Timer internal compare requires at least two ticks")
    if facts.kind == "timer_sequence":
        integer(selection.get("burst_budget_hclk_cycles"), 1, context="Timer burst budget")
    elif "burst_budget_hclk_cycles" in selection:
        fail("Timer burst budget belongs only to DMA sequence mode")


def resolve_fields(selection, route, clock):
    facts = ROUTES[route["id"]]
    bus = clock["apb2_hz" if facts.controller in (1, 8, 9, 10, 11) else "apb1_hz"]
    timer_hz = bus * (1 if bus == clock["core_hz"] else 2)
    tick = selection["tick_hz"]
    if timer_hz % tick or not 1 <= timer_hz // tick <= 65536:
        fail("Timer tick_hz must exactly divide the actual timer source within 16 bits")
    budget = selection.get("burst_budget_hclk_cycles", 0)
    if budget * tick >= selection["period_ticks"] * clock["core_hz"]:
        fail("Timer burst budget must fit strictly inside the shortest period")
    return dict(channels=facts.channels, complementary=facts.complementary,
                internal_trigger=facts.internal_trigger,
                counter_bits=32 if facts.controller == 5 else 16,
                native_bits=32 if facts.controller in (1, 8) else 16,
                burst_base=11, burst_items=6, burst_budget_hclk_cycles=budget,
                master_trigger=selection.get("master_trigger", "oc4ref" if facts.internal_trigger else "none"),
                sync_source=None, sync_mode="none", itr=0)


def resolve_edges(selected):
    owners = {item["id"]: item for item in selected if item["kind"] in KINDS}
    for name, item in owners.items():
        sync = item.get("synchronization")
        if sync is None:
            continue
        source = owners.get(sync["source"])
        number = int(item["controller"][3:])
        if (source is None or source is item or
                source["waveform_resolution"]["master_trigger"] == "none"):
            fail("Timer internal trigger source must be a selected distinct TRGO owner")
        master = int(source["controller"][3:])
        if master not in ITR[number]:
            fail("Timer source has no real ITR/internal trigger connection to target")
        item["waveform_resolution"].update(sync_source=source["id"],
                                          sync_mode=sync["mode"], itr=ITR[number].index(master))
    for name in owners:
        visited = set()
        current = name
        while current is not None:
            if current in visited:
                fail("Timer internal trigger synchronization cycle")
            visited.add(current)
            current = owners[current]["waveform_resolution"]["sync_source"]


def constructor(item, result):
    """Emit immutable facts, exact storage and fixed calls; never hardware code."""
    o, name = item.options, item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_wave_config_" + name
    saved, pins = "s_nx_wave_saved_" + name, "s_nx_wave_pins_" + name
    lines = [f"static uint16_t {saved}[{len(item.pins)}];",
             f"static const nx_stm32_av_pin_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]; slot = f"{saved}[{index}u]"; snapshots[wire] = slot
        lines += [f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1])-ord('A')}u, {int(wire[2:])}u, {pin['af']}u }},"]
    lines += ["};"]
    regions, count = dma_regions(name, result, include_flash=True) if item.kind == "timer_sequence" else ([], 0)
    lines += regions
    lines += [f"static const nx_stm32_waveform_config_t {config} = {{", "    .common = {",
              f"        .pins = {pins}, .pin_count = {len(item.pins)}u,",
              f"        .poll_limit = {o.poll_limit}u, .priority = {o.irq.priority}u,"]
    if count:
        lines += [f"        .regions = s_nx_dma_regions_{name}, .region_count = {count}u,"]
    number = int(item.controller[3:]); advanced = number in (1, 8)
    lines += ["    },", f"    .registers = {item.controller}, .controller = {number}u,",
              f"    .timer_hz = NEXUS_APB{2 if advanced or number in (9,10,11) else 1}_TIMER_HZ,",
              f"    .tick_hz = {o.tick_hz}u, .period_ticks = {o.period_ticks}u,",
              f"    .deadtime_ticks = {o.deadtime_ticks}u, .channels = {sum(1 << (ch-1) for ch in o.channels)}u,",
              f"    .master_mode = {MASTER[o.master_trigger]}u, .sync_mode = {SYNC[o.sync_mode]}u, .itr = {o.itr}u,",
              f"    .kind = NX_STM32_WAVE_{ {'pwm_group':'GROUP','timer_pulse':'PULSE','timer_sequence':'SEQUENCE'}[item.kind] },",
              f"    .complementary = {str(o.complementary).lower()}, .internal_trigger = {str(o.internal_trigger).lower()},",
              f"    .break_active_high = {str(o.break_active_high).lower()}, .idle_high = {str(o.idle_high).lower()},",
              f"    .idle_complement_high = {str(o.idle_complement_high).lower()},"]
    irqs = []
    if advanced:
        pin = item.pins[-1]["pin"]; vector = "TIM1_BRK_TIM9" if number == 1 else "TIM8_BRK_TIM12"
        lines += [f"    .break_input = &GPIO{pin[1]}->IDR, .break_mask = {1 << int(pin[2:])}u,",
                  f"    .irq = {vector}_IRQn,"]
        irqs += [(vector, "nx_stm32_waveform_irq")]
    else:
        lines += ["    .irq = (IRQn_Type)-1,"]
    if item.kind == "timer_sequence":
        engine, lane, channel = DMA[number]; vector = f"DMA{engine}_Stream{lane}"
        lines += [f"    .dma = {{ DMA{engine}, {vector}, {vector}_IRQn, {lane}u, {channel}u }},",
                  f"    .burst_budget_hclk_cycles = {o.burst_budget_hclk_cycles}u,"]
        irqs += [(vector, "nx_stm32_waveform_dma_irq")]
    lines += ["};"]
    for vector, function in irqs:
        lines += [f"void {vector}_IRQHandler(void);", r"/** \brief Latch only this selected hardware owner. */",
                  f"void {vector}_IRQHandler(void) {{ {function}(&{state}); }}"]
    args = (Address(state), Address(config))
    return Construction(state_type="nx_stm32_waveform_state_t", definitions=lines,
        admission=(DirectCall("nx_stm32_waveform_admit", args),),
        initialize=DirectCall("nx_stm32_waveform_prepare", args),
        stop=(DirectCall("nx_stm32_waveform_close", (Address(state),)),),
        retain_on_failed_init=True, pin_snapshots=snapshots)
