"""Exact optional timer controllers and owned cold pin/vector/clock leases.

Idle levels are explicit assembly policy, not a claim about an output stage or
physical break circuit. Capture ring storage remains supplied by the caller.
"""
from tools.configure.calls import DirectCall
from dataclasses import dataclass

from tools.configure.gpio_phase import withdraw_pin
from tools.configure.providers import maintained
from tools.configure.providers.common import Construction, fail, integer


@dataclass(frozen=True)
class TimerRegisters:
    """Exact cold-state register boundary for a shared timer constructor."""

    apb: str
    gate: str
    cr1: str
    ccer: str
    dier: str
    bdtr: str | None = None
    shared_interrupt: str | None = None
    break_input: str | None = None
    break_mask: int = 0


@dataclass(frozen=True)
class TimerBinding:
    """One maintained timer personality and its physical ownership facts."""

    kind: str
    controller: str
    vector: str
    irq: int
    clock_hz: int
    pins: tuple
    resources: tuple
    registers: TimerRegisters | None = None


@dataclass(frozen=True)
class TimerPolicy:
    """Explicit family timer bindings and an optional retained GPIO gate."""

    bindings: tuple
    gpio_clock: str | None

    def binding(self, kind):
        return next((item for item in self.bindings if item.kind == kind), None)


KINDS = {"timer_capture", "advanced_pwm"}
CAPTURE_FIELDS = {"tick_hz", "edge", "filter", "exclusive", "irq_priority", "calls_os"}
ADVANCED_FIELDS = {"tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
                   "idle_high", "idle_complement_high", "exclusive", "irq_priority", "calls_os"}


def validate_soc(soc, provider):
    for name, item in soc["controllers"].items():
        kind = item["kind"]
        if kind not in KINDS:
            continue
        policy = getattr(provider, "TIMER_ADVANCED", None)
        facts = policy.binding(kind) if policy is not None else None
        mode = "edge-capture" if kind == "timer_capture" else "complementary-pwm"
        if (facts is None or name != facts.controller or item["modes"] != [mode] or
                item["irq"] != [facts.vector] or item.get("irq_numbers") != [facts.irq] or
                type(item.get("clock_hz")) is not int or item["clock_hz"] != facts.clock_hz or
                item["pins"] != [pin for pin, _, _ in facts.pins] or
                item.get("resources") != list(facts.resources)):
            fail("Timer controller, shared vector, clock or pin facts differ from the SDK")


def validate_selection(selection, route, provider):
    kind = route["kind"]
    fields = CAPTURE_FIELDS if kind == "timer_capture" else ADVANCED_FIELDS
    if not (fields - {"calls_os"}).issubset(selection):
        fail("Timer requires explicit clock, mode, safe policy, exclusive owner and IRQ")
    policy = getattr(provider, "TIMER_ADVANCED", None)
    facts = policy.binding(kind) if policy is not None else None
    actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    if facts is None or route["controller"] != facts.controller or actual != facts.pins:
        fail("Timer selection differs from the reviewed exclusive pad route")
    tick = integer(selection["tick_hz"], 1, context="Timer tick_hz")
    if facts.clock_hz % tick or not 1 <= facts.clock_hz // tick <= 65536:
        fail("Timer tick_hz must exactly divide the selected clock within 16 bits")
    if type(selection["exclusive"]) is not bool or not selection["exclusive"]:
        fail("Timer requires an explicit exclusive controller owner")
    if kind == "timer_capture":
        if (type(selection["edge"]) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            fail("Timer capture edge is not maintained")
        integer(selection["filter"], 0, 15, "Timer capture filter")
    else:
        integer(selection["period_ticks"], 1, 65535, "Timer period_ticks")
        deadtime = integer(selection["deadtime_ticks"], 0, 1008, "Timer deadtime")
        if not (deadtime <= 127 or
                128 <= deadtime <= 254 and deadtime % 2 == 0 or
                256 <= deadtime <= 504 and deadtime % 8 == 0 or
                512 <= deadtime <= 1008 and deadtime % 16 == 0):
            fail("Timer deadtime has no exact hardware encoding; rounding is forbidden")
        for key in ("break_active_high", "idle_high", "idle_complement_high"):
            if type(selection[key]) is not bool:
                fail("Timer safe polarity requires explicit boolean policy")


def constructor(item, result, prefix):
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    kind = item.kind
    capture = kind == "timer_capture"
    policy = maintained(result.soc_family).TIMER_ADVANCED
    facts = policy.binding(kind)
    if facts is None or facts.registers is None:
        fail("No maintained shared timer constructor for " + result.soc_family)
    vector, irq, hz = facts.vector, facts.irq, facts.clock_hz
    bank, bit = irq // 32, 1 << (irq % 32)
    registers = facts.registers
    apb, gate = registers.apb, registers.gate
    cr1, ccer, dier, bdtr = (registers.cr1, registers.ccer, registers.dier,
                           registers.bdtr)
    gpio = maintained(result.soc_family).GPIO_PHASE
    options = item.options
    lines = [f"static uint32_t s_nx_timer_clock_added_{name};",
             f"static uint16_t s_nx_timer_pin_saved_{name}[{len(item.pins)}];",
             f"static uint8_t s_nx_timer_pin_leases_{name};",
             f"static uint8_t s_nx_timer_priority_saved_{name};",
             f"static bool s_nx_timer_irq_leased_{name};"]
    config_type = "nx_timer_capture_config_t" if capture else "nx_advanced_pwm_config_t"
    lines += [f"static const {config_type} s_nx_timer_config_{name} = {{",
              f"    .timer_hz = {hz}u, .tick_hz = {options.tick_hz}u, .exclusive = true,"]
    if capture:
        lines += [f"    .edge = NX_TIMER_EDGE_{options.edge.upper()}, .filter = {options.filter}u"]
    else:
        lines += [f"    .break_input = {registers.break_input}, .break_mask = {registers.break_mask}u,",
                  f"    .period_ticks = {options.period_ticks}u, .deadtime_ticks = {options.deadtime_ticks}u,",
                  f"    .break_active_high = {str(options.break_active_high).lower()},",
                  f"    .idle_high = {str(options.idle_high).lower()}, .idle_complement_high = {str(options.idle_complement_high).lower()}"]
    lines += ["};", f"static nx_result_t stop_{name}(void) {{", "    nx_result_t status;",
              f"    if ({state}.config != NULL) {{",
              f"        status = nx_{kind}_stop(&s_nx_face_{name});",
              "        if (status != NX_SUCCESS) { return status; }", "    }",
              f"    if (s_nx_timer_irq_leased_{name}) {{",
              f"        NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[{bank}], {bit}u);",
              f"        NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[{bank}], {bit}u);", "        nx_arch_dsb();",
              f"        if (((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | NVIC->IABR[{bank}]) & {bit}u) != 0u) {{ return NX_ERROR_BUSY; }}",
              f"        NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[{irq}], s_nx_timer_priority_saved_{name});",
              "        nx_arch_dsb();",
              f"        if (NVIC->IPR[{irq}] != s_nx_timer_priority_saved_{name}) {{ return NX_ERROR_IO; }}",
              f"        s_nx_timer_irq_leased_{name} = false;", "    }",
              f"    status = nx_{prefix}_{kind}_release(&{state});",
              "    if (status != NX_SUCCESS) { return status; }"]
    # Both outputs must have observed inactive drive before any pin lease
    # is released. A refused second store retains the entire output group.
    if not capture:
        for index, pin in enumerate(item.pins):
            if pin["function"] == "break":
                continue
            wire = pin["pin"]
            number = int(wire[2:])
            high = (options.idle_high if pin["function"] == "output"
                    else options.idle_complement_high)
            value = (1 << number) if high else (1 << (number + 16))
            bop = gpio.output_set(wire[1])
            odr = gpio.output_read(wire[1])
            mode = gpio.mode(wire[1])
            mode_mask = 3 << (number * 2)
            output_mode = 1 << (number * 2)
            # Keep the owned AF/electrical fields until GPIO drive is observed.
            # Changing AF first could connect a different active peripheral.
            lines += [f"    if ((s_nx_timer_pin_leases_{name} & {1 << index}u) != 0u) {{",
                      f"        NX_PLATFORM_REGISTER_WRITE({bop}, {value}u);", "        nx_arch_dsb();",
                      f"        if (({odr} & {1 << number}u) != {1 << number if high else 0}u) {{ return NX_ERROR_IO; }}",
                      f"        NX_PLATFORM_REGISTER_WRITE({mode}, ({mode} & ~{mode_mask}u) | {output_mode}u);",
                      "        nx_arch_dsb();",
                      f"        if (({mode} & {mode_mask}u) != {output_mode}u) {{ return NX_ERROR_IO; }}",
                      f"        if (({odr} & {1 << number}u) != {1 << number if high else 0}u) {{ return NX_ERROR_IO; }}",
                      "    }"]
    for index, pin in reversed(list(enumerate(item.pins))):
        wire = pin["pin"]
        number = int(wire[2:])
        lease = 1 << index
        lines += [f"    if ((s_nx_timer_pin_leases_{name} & {lease}u) != 0u) {{"]
        lines += withdraw_pin(result.soc_family, wire)
        lines += [f"        nx_{prefix}_pin_restore(GPIO{wire[1]}, {number}u, s_nx_timer_pin_saved_{name}[{index}]);",
                  f"        if (nx_{prefix}_pin_capture(GPIO{wire[1]}, {number}u) != s_nx_timer_pin_saved_{name}[{index}]) {{ return NX_ERROR_IO; }}",
                  f"        s_nx_timer_pin_leases_{name} &= (uint8_t)~{lease}u;", "    }"]
    lines += [f"    if (s_nx_timer_clock_added_{name} != 0u) {{",
              f"        NX_PLATFORM_REGISTER_WRITE({apb}, {apb} & ~s_nx_timer_clock_added_{name});", "        nx_arch_dsb();",
              f"        if (({apb} & s_nx_timer_clock_added_{name}) != 0u) {{ return NX_ERROR_IO; }}",
              f"        s_nx_timer_clock_added_{name} = 0u;", "    }", "    return NX_SUCCESS;", "}",
              f"static nx_result_t prepare_{name}(void) {{", "    nx_result_t status;",
              f"    if ({state}.config != NULL) {{ return NX_ERROR_BUSY; }}",
              f"    if (((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | NVIC->IABR[{bank}]) & {bit}u) != 0u) {{ return NX_ERROR_BUSY; }}"]
    if result.kernel is not None:
        lines += ["    if (((SCB->AIRCR >> 8u) & 7u) > 3u) { return NX_ERROR_PERMISSION; }"]
    lines += [f"    s_nx_timer_clock_added_{name} = ~{apb} & {gate};",
              f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {gate});", "    nx_arch_dsb();",
              f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}",
              f"    if (({cr1} & 1u) != 0u || {ccer} != 0u || {dier} != 0u" +
              (f" || ({bdtr} & 33536u) != 0u" if not capture else "") + ") { return NX_ERROR_BUSY; }"]
    if not capture:
        shared = registers.shared_interrupt
        lines += [f"    if ({shared} != 0u) {{ return NX_ERROR_BUSY; }}"]
    lines += [f"    s_nx_timer_priority_saved_{name} = NVIC->IPR[{irq}];",
              f"    s_nx_timer_irq_leased_{name} = true;"]
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        number = int(wire[2:])
        lines += [f"    status = nx_{prefix}_gpio_clock_enable({ord(wire[1])-ord('A')}u);",
                  "    if (status != NX_SUCCESS) { return status; }",
                  f"    s_nx_timer_pin_saved_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{wire[1]}, {number}u);",
                  f"    s_nx_timer_pin_leases_{name} |= {1 << index}u;",
                  f"    status = nx_{prefix}_pin_configure(GPIO{wire[1]}, {number}u, 2u, {pin['af']}u, 0u, false);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += [f"    status = nx_{prefix}_{kind}_initialize(&{state}, &s_nx_timer_config_{name});",
              "    if (status != NX_SUCCESS) { return status; }",
              f"    NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[{irq}], {options.irq.priority << 4}u);",
              f"    NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[{bank}], {bit}u);",
              f"    NX_PLATFORM_REGISTER_WRITE(NVIC->ISER[{bank}], {bit}u);", "    nx_arch_dsb();",
              f"    if (NVIC->IPR[{irq}] != {options.irq.priority << 4}u || (NVIC->ISER[{bank}] & {bit}u) == 0u || (NVIC->ISPR[{bank}] & {bit}u) != 0u) {{ return NX_ERROR_IO; }}",
              "    return NX_SUCCESS;", "}", f"void {vector}_IRQHandler(void);",
              "/** \\brief Bounded dispatch from the exclusively leased vector. */",
              f"void {vector}_IRQHandler(void) {{ nx_{prefix}_{kind}_irq(&{state}); }}"]
    return Construction(
        state_type=f"nx_{kind}_controller_t", definitions=lines,
        initialize=DirectCall(f"prepare_{name}"), stop=[DirectCall(f"stop_{name}")],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]:
                       f"s_nx_timer_pin_saved_{name}[{index}]"
                       for index, pin in enumerate(item.pins)})


def _gpio_mask(result):
    return sum(1 << (ord(port) - ord('A')) for port in {
        pin["pin"][1] for item in result.controllers if item.kind in KINDS
        for pin in item.pins})


def _gpio_clock(result):
    policy = getattr(maintained(result.soc_family), "TIMER_ADVANCED", None)
    return policy.gpio_clock if policy is not None else None


def state_declarations(result, prefix):
    if _gpio_clock(result) is None or not _gpio_mask(result):
        return []
    return ["static uint32_t s_nx_timer_gpio_clocks_added;"]


def start_resources(result, prefix):
    mask = _gpio_mask(result)
    clock = _gpio_clock(result)
    return ([f"    s_nx_timer_gpio_clocks_added = ~{clock} & {mask}u;"]
            if clock is not None and mask else [])


def stop_system(result, prefix):
    clock = _gpio_clock(result)
    if clock is None or not _gpio_mask(result):
        return []
    return ["    if (s_nx_platform_started && s_nx_timer_gpio_clocks_added != 0u) {",
            f"        NX_PLATFORM_REGISTER_WRITE({clock}, {clock} & ~s_nx_timer_gpio_clocks_added);",
            "        nx_arch_dsb();",
            f"        if (({clock} & s_nx_timer_gpio_clocks_added) != 0u) {{ return NX_ERROR_IO; }}",
            "        s_nx_timer_gpio_clocks_added = 0u;", "    }"]
