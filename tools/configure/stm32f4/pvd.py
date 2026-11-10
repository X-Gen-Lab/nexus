"""Static comparator selections; all PWR/EXTI behavior is handwritten C."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction


def constructor(item):
    name = item.id.replace('-', '_')
    state = Address('s_nx_port_' + name)
    events = item.mode == 'pvd-events'
    stem = 'nx_stm32_pvd' + ('_events' if events else '')
    definitions = []
    if events:
        options = item.options
        edge = {'rising': 'NX_EXTI_RISING', 'falling': 'NX_EXTI_FALLING',
                'both': 'NX_EXTI_BOTH'}[options.edge]
        config = 's_nx_pvd_config_' + name
        definitions += [
            f'static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];',
            f'static const nx_stm32_pvd_events_config_t {config} = {{',
            f'    .storage = s_nx_events_{name}, .capacity = {options.event_capacity}u,',
            f'    .edge = {edge}, .threshold_code = {options.threshold_code}u,',
            f'    .priority = {options.irq.priority}u', '};',
            f'static nx_stm32_exti_state_t* const s_nx_pvd_lines_{name}[] = {{',
            f'    &s_nx_port_{name}.events', '};',
            'void PVD_IRQHandler(void);',
            '/** \\brief Exact selected internal comparator publisher. */',
            'void PVD_IRQHandler(void) {',
            f'    nx_stm32_exti_dispatch(s_nx_pvd_lines_{name}, 1u, 65536u);', '}']
        arguments = (state, Address(config))
    else:
        arguments = (state, item.options.threshold_code)
    return Construction(
        state_type=stem + '_state_t', definitions=definitions,
        admission=(DirectCall(stem + '_admit', arguments),),
        initialize=DirectCall(stem + '_prepare', arguments),
        stop=(DirectCall(stem + '_stop', (state,)),),
        retain_on_failed_init=True)
