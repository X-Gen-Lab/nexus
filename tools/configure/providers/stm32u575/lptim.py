"""Exact pinless U575 LPTIM2 RUN one-shot, no backup or Stop promises."""
from ..common import Construction, fail, integer


def validate(selection, route):
    if (route['controller'] != 'LPTIM2' or route['pins'] or
            selection['mode'] != 'irq-counter16-oneshot'):
        fail('U575 LPTIM requires the exact internal LPTIM2 IRQ one-shot')
    integer(selection.get('tick_hz'), 1000000, 1000000, 'U575 LPTIM RUN tick')
    integer(selection.get('irq_priority'), 5, 5, 'U575 LPTIM IRQ priority')
    if selection.get('calls_os', False) is not False:
        fail('U575 LPTIM IRQ never calls a scheduler')


def constructor(item, result):
    if result['backend'] != 'baremetal':
        fail('U575 LPTIM RUN reference requires baremetal')
    name = item.id.replace('-', '_')
    state = f's_nx_port_{name}'
    definitions = [f'static nx_result_t prepare_{name}(void) {{',
        f'    if ({state}.registers == NULL) {{ {state}.registers = LPTIM2; }}',
        f'    return nx_stm32u575_lptim_initialize(&{state});', '}',
        'void LPTIM2_IRQHandler(void);',
        r'/** \brief Exact UE observation; settlement remains task-owned. */',
        f'void LPTIM2_IRQHandler(void) {{ nx_stm32u575_lptim_irq(&{state}); }}']
    return Construction(state_type='nx_stm32u575_lptim_state_t',
        definitions=definitions, initialize=f'prepare_{name}()',
        stop=[f'nx_lptim_port_stop(&s_nx_face_{name})'],
        retain_on_failed_init=True)
