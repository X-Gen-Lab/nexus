"""GD32F470 selected cold-resource admission policy."""
import re

from tools.configure.providers import hardware
from tools.configure.providers.common import fail, owned_claims


PREFLIGHT_CALL = 'nx_gd32_soc_preflight()'


def _clock(controller):
    """Name a maintained exclusive gate, not an inferred silicon topology."""
    if controller.startswith(('USART', 'SPI', 'I2C')):
        bus = hardware.maintained('gd32f470', controller).apb
    elif re.fullmatch('TIMER[0238]', controller):
        bus = 2 if controller in {'TIMER0', 'TIMER8'} else 1
    elif controller in {'CAN0', 'CAN1'}:
        bus = 1
    else:
        fail('No maintained cold clock assertion for ' + controller)
    return (f'RCU_APB{bus}EN', f'RCU_APB{bus}EN_{controller}EN')


def admission_declarations(result, pins, pin_count):
    """Project static physical selections; concrete C owns all observations."""
    source = []
    exti = 0
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    clocks = set()
    for item in result.controllers:
        if item.kind in {'uart', 'spi', 'i2c', 'pwm', 'timer_capture', 'advanced_pwm', 'can'}:
            clocks.add(_clock(item.controller))
        elif item.kind == 'adc':
            register = 'RCU_APB2EN'
            names = ('ADC0', 'ADC1', 'ADC2')
            prefix = 'RCU_APB2EN_'
            clocks.add((register, ' | '.join((prefix + name + 'EN' for name in names))))
        elif item.kind == 'usb_device':
            clocks.add(('RCU_AHB2EN', 'RCU_AHB2EN_USBFSEN'))
        elif item.kind in {'eth', 'eth_frames'}:
            clocks.add(('RCU_AHB1EN', 'RCU_AHB1EN_ENETEN | RCU_AHB1EN_ENETTXEN | RCU_AHB1EN_ENETRXEN'))
            if item.kind == 'eth_frames':
                clocks.add(('RCU_AHB1EN', 'RCU_AHB1EN_ENETPTPEN'))
        elif item.kind == 'exti':
            line = int(item.pins[0]['pin'][2:])
            exti |= 1 << line
    for item in claims:
        timer = item.key.removeprefix('controller:')
        if re.fullmatch('TIM[1349]|TIMER[0238]', timer):
            clocks.add(_clock(timer))
    if result.can_domain is not None:
        clocks.add(_clock('CAN0'))
    streams = []
    for key in sorted({item.key for item in claims if item.key.startswith('dma:')}):
        match = re.fullmatch('dma:(DMA[0-9]+):stream([0-7])', key)
        if match is None:
            fail('No maintained selected DMA lane admission: ' + key)
        dma, lane = match.groups()
        streams.append((dma, lane))
    if streams:
        source += ['static const nx_gd32_platform_dma_lane_t s_nx_platform_dma_lanes[] = {',
                   *[f'    {{ {dma}, {lane}u }},' for dma, lane in streams], '};']
    source += ['static const nx_gd32_platform_config_t s_nx_platform_config = {',
               f'    .pins = {pins}, .pin_count = {pin_count}u,',
               '    .dma_lanes = ' + ('s_nx_platform_dma_lanes,' if streams else 'NULL,'),
               f'    .dma_lane_count = {len(streams)}u,']
    for field, register in (('apb1_clocks', 'RCU_APB1EN'),
                            ('apb2_clocks', 'RCU_APB2EN'),
                            ('ahb1_clocks', 'RCU_AHB1EN'),
                            ('ahb2_clocks', 'RCU_AHB2EN')):
        masks = sorted(mask for selected, mask in clocks if selected == register)
        source.append(f'    .{field} = ' + (' | '.join(masks) if masks else '0u') + ',')
    vectors = sorted({item.key.removeprefix('irq:') + '_IRQn' for item in claims
                      if item.key.startswith('irq:')})
    source += ['    .irq_masks = {']
    for bank in range(3):
        terms = [f'NX_GD32_PLATFORM_IRQ_MASK({vector}, {bank}u)'
                 for vector in vectors]
        source.append('        ' + (' | '.join(terms) if terms else '0u') + ',')
    source += ['    },', f'    .exti_lines = {exti}u', '};']
    return source


def selected_preflight(result, *, dma_conditions=None):
    """Connect the reviewed immutable facts to the concrete C observer."""
    del dma_conditions
    if not result.controllers or not any(item.pins for item in result.controllers):
        return []
    return ['    result.primary = nx_gd32_platform_selected_preflight(&s_nx_platform_config);',
            '    if (result.primary != NX_SUCCESS) { return result; }']
