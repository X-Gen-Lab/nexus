"""STM32G474 checked timer-capture and encoder construction."""
from tools.configure.calls import DirectCall
from tools.configure.providers.common import Construction


def constructor(item):
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
    for pin in item.pins:
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
        definitions=lines, initialize=DirectCall(f"prepare_{name}"),
        stop=[DirectCall(f"stop_{name}")], retain_on_failed_init=True)
