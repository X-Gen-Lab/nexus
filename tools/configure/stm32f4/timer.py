"""Exact TIM4/TIM1 static policies; C owns every hardware transition."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction


def constructor(item, result):
    name = item.id.replace("-", "_")
    capture = item.kind == "timer_capture"
    selected = "capture" if capture else "advanced"
    config = f"s_nx_timer_config_{name}"
    snapshots = f"s_nx_timer_pin_saved_{name}"
    cold = f"s_nx_timer_cold_{name}"
    options = item.options
    lines = [f"static uint16_t {snapshots}[{len(item.pins)}];",
             f"static nx_stm32_timer_cold_t {cold};",
             f"static const nx_stm32_{selected}_cold_config_t {config} = {{",
             f"    .{'snapshot' if capture else 'snapshots'} = {snapshots},",
             f"    .priority = {options.irq.priority}u,",
             f"    .kernel = {str(result.kernel is not None).lower()},",
             "    .timer = {",
             f"        .timer_hz = {84000000 if capture else 168000000}u,",
             f"        .tick_hz = {options.tick_hz}u, .exclusive = true,"]
    if capture:
        lines += [f"        .edge = NX_TIMER_EDGE_{options.edge.upper()},",
                  f"        .filter = {options.filter}u"]
    else:
        lines += ["        .break_input = &GPIOB->IDR, .break_mask = 4096u,",
                  f"        .period_ticks = {options.period_ticks}u,",
                  f"        .deadtime_ticks = {options.deadtime_ticks}u,",
                  f"        .break_active_high = {str(options.break_active_high).lower()},",
                  f"        .idle_high = {str(options.idle_high).lower()},",
                  f"        .idle_complement_high = {str(options.idle_complement_high).lower()}"]
    vector = "TIM4" if capture else "TIM1_BRK_TIM9"
    lines += ["    }", "};", f"void {vector}_IRQHandler(void);",
              r"/** \brief Bounded dispatch from the exclusively leased vector. */",
              f"void {vector}_IRQHandler(void) {{ nx_stm32_{item.kind}_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(cold), Address(config))
    return Construction(
        state_type=f"nx_{item.kind}_controller_t", definitions=lines,
        initialize=DirectCall(f"nx_stm32_{selected}_cold_prepare", args),
        stop=[DirectCall(f"nx_stm32_{selected}_cold_close", args)],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]: f"{snapshots}[{index}]"
                       for index, pin in enumerate(item.pins)})
