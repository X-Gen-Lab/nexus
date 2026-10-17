"""Exact optional timer controllers and owned cold pin/vector/clock leases.

Idle levels are explicit assembly policy, not a claim about an output stage or
physical break circuit. Capture ring storage remains supplied by the caller.
"""
try:
    from .gpio_phase import withdraw_pin
    from .providers.common import Construction, fail, integer
except ImportError:
    from gpio_phase import withdraw_pin
    from providers.common import Construction, fail, integer


KINDS = {"timer_capture", "advanced_pwm"}
CAPTURE_FIELDS = {"tick_hz", "edge", "filter", "exclusive", "irq_priority", "calls_os"}
ADVANCED_FIELDS = {"tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
                   "idle_high", "idle_complement_high", "exclusive", "irq_priority", "calls_os"}
PINS = {
    "timer_capture": (("PB6", "capture", 2),),
    "advanced_pwm": (("PA8", "output", 1), ("PB13", "complement", 1),
                     ("PB12", "break", 1))}
FACTS = {
    "stm32g474": {"timer_capture": ("TIM5", "TIM5", 50, 170000000)},
    "stm32": {"timer_capture": ("TIM4", "TIM4", 30, 84000000),
              "advanced_pwm": ("TIM1", "TIM1_BRK_TIM9", 24, 168000000)},
    "gd32": {"timer_capture": ("TIMER3", "TIMER3", 30, 100000000),
             "advanced_pwm": ("TIMER0", "TIMER0_BRK_TIMER8", 24, 200000000)}}


def pins(prefix, kind):
    """Keep each reviewed package route separate from its public personality."""
    return ((("PA0", "capture", 2),) if prefix == "stm32g474" and
            kind == "timer_capture" else PINS[kind])


def resources(prefix, kind):
    controller = FACTS[prefix][kind][0]
    result = ["clock:" + controller]
    if kind == "advanced_pwm":
        result += ["controller:" + ("TIM9" if prefix == "stm32" else "TIMER8")]
    return result


def validate_soc(soc, provider):
    for name, item in soc["controllers"].items():
        kind = item["kind"]
        if kind not in KINDS:
            continue
        facts = FACTS.get(provider.PREFIX, {}).get(kind)
        mode = "edge-capture" if kind == "timer_capture" else "complementary-pwm"
        if (facts is None or name != facts[0] or item["modes"] != [mode] or
                item["irq"] != [facts[1]] or item.get("irq_numbers") != [facts[2]] or
                type(item.get("clock_hz")) is not int or item["clock_hz"] != facts[3] or
                item["pins"] != [pin for pin, _, _ in pins(provider.PREFIX, kind)] or
                item.get("resources") != resources(provider.PREFIX, kind)):
            fail("Timer controller, shared vector, clock or pin facts differ from the SDK")


def validate_selection(selection, route, provider):
    kind = route["kind"]
    fields = CAPTURE_FIELDS if kind == "timer_capture" else ADVANCED_FIELDS
    if not (fields - {"calls_os"}).issubset(selection):
        fail("Timer requires explicit clock, mode, safe policy, exclusive owner and IRQ")
    facts = FACTS.get(provider.PREFIX, {}).get(kind)
    actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    if facts is None or route["controller"] != facts[0] or actual != pins(provider.PREFIX, kind):
        fail("Timer selection differs from the reviewed exclusive pad route")
    tick = integer(selection["tick_hz"], 1, context="Timer tick_hz")
    if facts[3] % tick or not 1 <= facts[3] // tick <= 65536:
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
    stm = prefix == "stm32"
    controller, vector, irq, hz = FACTS[prefix][kind]
    bank, bit = irq // 32, 1 << (irq % 32)
    apb = ("RCC->APB1ENR" if capture else "RCC->APB2ENR") if stm else (
        "RCU_APB1EN" if capture else "RCU_APB2EN")
    gate = ("RCC_APB1ENR_TIM4EN" if capture else "RCC_APB2ENR_TIM1EN") if stm else (
        "RCU_APB1EN_TIMER3EN" if capture else "RCU_APB2EN_TIMER0EN")
    cr1 = f"{controller}->CR1" if stm else f"TIMER_CTL0({controller})"
    ccer = f"{controller}->CCER" if stm else f"TIMER_CHCTL2({controller})"
    dier = f"{controller}->DIER" if stm else f"TIMER_DMAINTEN({controller})"
    bdtr = "TIM1->BDTR" if stm else "TIMER_CCHP(TIMER0)"
    options = item.options
    lines = [f"static uint32_t s_nx_timer_clock_added_{name};",
             f"static uint16_t s_nx_timer_pin_saved_{name}[{len(item['pins'])}];",
             f"static uint8_t s_nx_timer_pin_leases_{name};",
             f"static uint8_t s_nx_timer_priority_saved_{name};",
             f"static bool s_nx_timer_irq_leased_{name};"]
    config_type = "nx_timer_capture_config_t" if capture else "nx_advanced_pwm_config_t"
    lines += [f"static const {config_type} s_nx_timer_config_{name} = {{",
              f"    .timer_hz = {hz}u, .tick_hz = {options.tick_hz}u, .exclusive = true,"]
    if capture:
        lines += [f"    .edge = NX_TIMER_EDGE_{options.edge.upper()}, .filter = {options.filter}u"]
    else:
        lines += [f"    .break_input = {'&GPIOB->IDR' if stm else '&GPIO_ISTAT(GPIOB)'}, .break_mask = 4096u,",
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
              f"        NX_PLATFORM_PRIORITY_WRITE(NVIC->IP[{irq}], s_nx_timer_priority_saved_{name});",
              "        nx_arch_dsb();",
              f"        if (NVIC->IP[{irq}] != s_nx_timer_priority_saved_{name}) {{ return NX_ERROR_IO; }}",
              f"        s_nx_timer_irq_leased_{name} = false;", "    }",
              f"    status = nx_{prefix}_{kind}_release(&{state});",
              "    if (status != NX_SUCCESS) { return status; }"]
    # Both outputs must have observed inactive drive before any pin lease
    # is released. A refused second store retains the entire output group.
    if not capture:
        for index, pin in enumerate(item["pins"]):
            if pin["function"] == "break":
                continue
            wire = pin["pin"]
            number = int(wire[2:])
            high = (options.idle_high if pin["function"] == "output"
                    else options.idle_complement_high)
            value = (1 << number) if high else (1 << (number + 16))
            bop = f"GPIO{wire[1]}->BSRR" if stm else f"GPIO_BOP(GPIO{wire[1]})"
            odr = f"GPIO{wire[1]}->ODR" if stm else f"GPIO_OCTL(GPIO{wire[1]})"
            mode = f"GPIO{wire[1]}->MODER" if stm else f"GPIO_CTL(GPIO{wire[1]})"
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
    for index, pin in reversed(list(enumerate(item["pins"]))):
        wire = pin["pin"]
        number = int(wire[2:])
        lease = 1 << index
        lines += [f"    if ((s_nx_timer_pin_leases_{name} & {lease}u) != 0u) {{"]
        lines += withdraw_pin(prefix, wire)
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
    if result.get("kernel_profile") is not None:
        lines += ["    if (((SCB->AIRCR >> 8u) & 7u) > 3u) { return NX_ERROR_PERMISSION; }"]
    lines += [f"    s_nx_timer_clock_added_{name} = ~{apb} & {gate};",
              f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {gate});", "    nx_arch_dsb();",
              f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}",
              f"    if (({cr1} & 1u) != 0u || {ccer} != 0u || {dier} != 0u" +
              (f" || ({bdtr} & 33536u) != 0u" if not capture else "") + ") { return NX_ERROR_BUSY; }"]
    if not capture:
        shared = "TIM9->DIER" if stm else "TIMER_DMAINTEN(TIMER8)"
        lines += [f"    if ({shared} != 0u) {{ return NX_ERROR_BUSY; }}"]
    lines += [f"    s_nx_timer_priority_saved_{name} = NVIC->IP[{irq}];",
              f"    s_nx_timer_irq_leased_{name} = true;"]
    for index, pin in enumerate(item["pins"]):
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
              f"    NX_PLATFORM_PRIORITY_WRITE(NVIC->IP[{irq}], {options.irq.priority << 4}u);",
              f"    NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[{bank}], {bit}u);",
              f"    NX_PLATFORM_REGISTER_WRITE(NVIC->ISER[{bank}], {bit}u);", "    nx_arch_dsb();",
              f"    if (NVIC->IP[{irq}] != {options.irq.priority << 4}u || (NVIC->ISER[{bank}] & {bit}u) == 0u || (NVIC->ISPR[{bank}] & {bit}u) != 0u) {{ return NX_ERROR_IO; }}",
              "    return NX_SUCCESS;", "}", f"void {vector}_IRQHandler(void);",
              "/** \\brief Bounded dispatch from the exclusively leased vector. */",
              f"void {vector}_IRQHandler(void) {{ nx_{prefix}_{kind}_irq(&{state}); }}"]
    return Construction(
        state_type=f"nx_{kind}_controller_t", definitions=lines,
        initialize=f"prepare_{name}()", stop=[f"stop_{name}()"],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]:
                       f"s_nx_timer_pin_saved_{name}[{index}]"
                       for index, pin in enumerate(item["pins"])})


def _gpio_mask(result):
    return sum(1 << (ord(port) - ord('A')) for port in {
        pin["pin"][1] for item in result.controllers if item.kind in KINDS
        for pin in item["pins"]})


def state_declarations(result, prefix):
    if prefix != "gd32" or not _gpio_mask(result):
        return []
    return ["static uint32_t s_nx_timer_gpio_clocks_added;"]


def start_resources(result, prefix):
    mask = _gpio_mask(result)
    return ([f"    s_nx_timer_gpio_clocks_added = ~RCU_AHB1EN & {mask}u;"]
            if prefix == "gd32" and mask else [])


def stop_system(result, prefix):
    if prefix != "gd32" or not _gpio_mask(result):
        return []
    return ["    if (s_nx_platform_started && s_nx_timer_gpio_clocks_added != 0u) {",
            "        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN & ~s_nx_timer_gpio_clocks_added);",
            "        nx_arch_dsb();",
            "        if ((RCU_AHB1EN & s_nx_timer_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }",
            "        s_nx_timer_gpio_clocks_added = 0u;", "    }"]


def g474_constructor(item):
    """One selected timer personality, with caller-owned capture storage."""
    capture = item.kind == "timer_capture"
    name = item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_timer_config_" + name
    vector, priority = "s_nx_vector_" + name, "s_nx_priority_" + name
    options = item.options
    lines = []
    if capture:
        lines += [f"static bool {vector};", f"static uint8_t {priority};",
                  f"static const nx_timer_capture_config_t {config} = {{",
                  f"    170000000u, {options.tick_hz}u, NX_TIMER_EDGE_{options.edge.upper()}, {options.filter}u, true }};"]
    else:
        mode = {"x2-ti1": "NX_ENCODER_MODE_X2_TI1",
                "x2-ti2": "NX_ENCODER_MODE_X2_TI2",
                "x4-ti12": "NX_ENCODER_MODE_X4_TI12"}[options.counting]
        lines += [f"static const nx_stm32g474_encoder_config_t {config} = {{",
                  f"    {mode}, {options.filter_a}u, {options.filter_b}u,",
                  f"    {str(options.invert_a).lower()}, {str(options.invert_b).lower()}, true }};"]
    lines += [f"static nx_result_t prepare_{name}(void) {{",
              f"    if ({state}.config != NULL || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
              "    nx_result_t status;"]
    for pin in item["pins"]:
        lines += [f"    status = nx_stm32g474_pin_alternate(GPIOA, {int(pin['pin'][2:])}u, 2u);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    kind = item.kind
    lines += [f"    status = nx_stm32g474_{kind}_initialize(&{state}, &{config});",
              "    if (status != NX_SUCCESS) { return status; }"]
    if capture:
        lines += [f"    {priority} = (uint8_t)NX_G474_IRQ_PRIORITY(TIM5_IRQn);",
                  f"    {vector} = true;",
                  f"    return NX_G474_IRQ_CONTROL(TIM5_IRQn, true, {options.irq.priority}u);"]
    else:
        lines += ["    return NX_SUCCESS;"]
    lines += ["}", f"static nx_result_t stop_{name}(void) {{",
              "    nx_result_t status;",
              f"    if ({state}.config != NULL) {{",
              f"        status = nx_{kind}_stop(&s_nx_face_{name});",
              "        if (status != NX_SUCCESS) { return status; }", "    }"]
    if capture:
        lines += [f"    if ({vector}) {{",
                  "        status = NX_G474_IRQ_CONTROL(TIM5_IRQn, false, 0u);",
                  "        if (status != NX_SUCCESS) { return status; }",
                  "        if (!NX_G474_IRQ_AVAILABLE(TIM5_IRQn)) { return NX_ERROR_BUSY; }",
                  f"        NX_G474_IRQ_PRIORITY_WRITE(TIM5_IRQn, {priority});",
                  "        nx_arch_dsb(); nx_arch_isb();",
                  f"        if (NX_G474_IRQ_PRIORITY(TIM5_IRQn) != {priority}) {{ return NX_ERROR_IO; }}",
                  f"        {vector} = false;", "    }",
                  f"    return nx_stm32g474_timer_capture_release(&{state});", "}",
                  "void TIM5_IRQHandler(void);",
                  r"/** \brief One selected raw32-bit TIM5 capture observation. */",
                  f"void TIM5_IRQHandler(void) {{ nx_stm32g474_timer_capture_irq(&{state}); }}"]
    else:
        lines += [f"    return nx_stm32g474_encoder_close(&{state});", "}"]
    return Construction(
        state_type=("nx_timer_capture_controller_t" if capture else
                    "nx_stm32g474_encoder_state_t"),
        definitions=lines, initialize=f"prepare_{name}()",
        stop=[f"stop_{name}()"], retain_on_failed_init=True)
