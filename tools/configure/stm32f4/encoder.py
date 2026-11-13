"""Reviewed F407 quadrature facts and static descriptor construction."""
from types import MappingProxyType

from tools.configure.calls import Address, DirectCall
from tools.configure.ir import controller_ir
from tools.configure.providers.common import Construction, fail, integer

COUNTING = MappingProxyType({
    'x2-a': 'NX_ENCODER_MODE_X2_A',
    'x2-b': 'NX_ENCODER_MODE_X2_B',
    'x4': 'NX_ENCODER_MODE_X4',
})
PINS = MappingProxyType({
    'TIM1': (('PA8', 'ti1', 1), ('PA9', 'ti2', 1)),
    'TIM8': (('PC6', 'ti1', 3), ('PC7', 'ti2', 3)),
    'TIM3': (('PA6', 'ti1', 2), ('PA7', 'ti2', 2)),
    'TIM4': (('PB6', 'ti1', 2), ('PB7', 'ti2', 2)),
    'TIM5': (('PA0', 'ti1', 2), ('PA1', 'ti2', 2)),
})


def validate(selection, route):
    """Admit exact input pairs and exclusive raw hardware personalities."""
    pins = tuple((p['pin'], p['function'], p.get('af')) for p in route['pins'])
    if pins != PINS.get(route['controller']):
        fail('Encoder requires a reviewed F407 TI1/TI2 pair')
    counting = selection['counting']
    if type(counting) is not str or counting not in COUNTING:
        fail('Encoder counting must select x2-a, x2-b or x4')
    integer(selection['filter_a'], 0, 15, 'Encoder filter_a')
    integer(selection['filter_b'], 0, 15, 'Encoder filter_b')
    for field in ('invert_a', 'invert_b', 'exclusive'):
        if type(selection[field]) is not bool:
            fail('Encoder ' + field + ' must be boolean')
    if not selection['exclusive']:
        fail('Encoder requires an exclusive controller owner')


def constructor(item):
    """Connect one static descriptor; hardware ordering stays in C."""
    item = controller_ir(item, 'stm32f4')
    name = item.id.replace('-', '_')
    options = item.options
    config = 's_nx_encoder_config_' + name
    state = Address('s_nx_port_' + name)
    definitions = (
        f'static const nx_stm32_encoder_config_t {config} = {{',
        f'    .registers = {item.controller}, .mode = {COUNTING[options.counting]},',
        f'    .filter_a = {options.filter_a}u, .filter_b = {options.filter_b}u,',
        f'    .invert_a = {str(options.invert_a).lower()},',
        f'    .invert_b = {str(options.invert_b).lower()}', '};')
    return Construction(state_type='nx_stm32_encoder_state_t',
        definitions=definitions,
        initialize=DirectCall('nx_stm32_encoder_prepare', (state, Address(config))),
        stop=[DirectCall('nx_stm32_encoder_close', (state,))],
        retain_on_failed_init=True)
