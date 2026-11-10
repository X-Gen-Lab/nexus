"""Exact timer static policies; C owns every hardware transition."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction
from .integration import TIMER_ADVANCED


def constructor(item, result):
    name = item.id.replace("-", "_")
    capture = item.kind == "timer_capture"
    selected = ("capture" if item.controller == 'TIM4' else
                item.controller.lower()+'_capture') if capture else (
        "tim8_advanced" if item.controller == 'TIM8' else "advanced")
    facts = TIMER_ADVANCED.binding(item.kind, item.controller)
    if facts is None:
        raise ValueError('Timer has no reviewed physical binding')
    route = facts.route(tuple((pin['pin'], pin['function'], pin['af'])
                              for pin in item.pins))
    if route is None:
        raise ValueError('Timer has no reviewed channel route')
    transport = (('nx_stm32_timer_capture' if item.controller == 'TIM4' else
                  f'nx_stm32_{item.controller.lower()}_timer_capture') if capture else
                 'nx_stm32_tim8_advanced_pwm' if item.controller == 'TIM8' else
                 'nx_stm32_advanced_pwm')
    config = f"s_nx_timer_config_{name}"
    snapshots = f"s_nx_timer_pin_saved_{name}"
    cold = f"s_nx_timer_cold_{name}"
    options = item.options
    lines = [f"static uint16_t {snapshots}[{len(item.pins)}];",
             f"static nx_stm32_timer_cold_t {cold};",
             f"static const nx_stm32_{'capture' if capture else 'advanced'}_cold_config_t {config} = {{",
             f"    .{'snapshot' if capture else 'snapshots'} = {snapshots},",
             *([f"    .registers = {item.controller},"] if capture else []),
             f"    .priority = {options.irq.priority}u,",
             f"    .kernel = {str(result.kernel is not None).lower()},",
             "    .timer = {",
             f"        .timer_hz = {item.integration.clock_hz}u,",
             f"        .channel = {route.channel}u,",
             f"        .tick_hz = {options.tick_hz}u, .exclusive = true,"]
    if capture:
        lines += [f"        .edge = NX_TIMER_EDGE_{options.edge.upper()},",
                  f"        .filter = {options.filter}u"]
    else:
        wire = next(pin for pin, role, _ in route.pins if role == 'break')
        mask = 1 << int(wire[2:])
        lines += [f"        .break_input = &GPIO{wire[1]}->IDR, .break_mask = {mask}u,",
                  f"        .period_ticks = {options.period_ticks}u,",
                  f"        .deadtime_ticks = {options.deadtime_ticks}u,",
                  f"        .break_active_high = {str(options.break_active_high).lower()},",
                  f"        .idle_high = {str(options.idle_high).lower()},",
                  f"        .idle_complement_high = {str(options.idle_complement_high).lower()}"]
    vector = facts.vector
    lines += ["    }", "};", f"void {vector}_IRQHandler(void);",
              r"/** \brief Bounded dispatch from the exclusively leased vector. */",
              f"void {vector}_IRQHandler(void) {{ {transport}_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(cold), Address(config))
    return Construction(
        state_type=f"nx_{item.kind}_controller_t", definitions=lines,
        initialize=DirectCall(f"nx_stm32_{selected}_cold_prepare", args),
        stop=[DirectCall(f"nx_stm32_{selected}_cold_close", args)],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]: f"{snapshots}[{index}]"
                       for index, pin in enumerate(item.pins)})
