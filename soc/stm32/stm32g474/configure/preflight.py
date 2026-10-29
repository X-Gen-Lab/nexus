"""Immutable G474 resource records; all hardware admission executes in C."""
import re

from tools.configure.providers.common import fail, owned_claims

PREFLIGHT_CALL = 'nx_stm32g474_system_preflight(&g_nx_stm32g474_system)'


def admission_declarations(result, pins, pin_count):
    from .emission import _peripheral_gates, _syscfg
    selected = {name: set() for name in ('apb1', 'apb1_2', 'apb2', 'ahb1', 'ahb2', 'reset_apb1', 'mux')}
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    exti = 0
    control = False
    strict_dma = False
    for item in result.controllers:
        controller = item.controller
        if item.kind in {'uart', 'spi', 'pwm', 'i2c'}:
            field, register = ('apb1_2', 'APB1ENR2') if controller == 'LPUART1' else (
                ('apb2', 'APB2ENR') if controller in {'USART1', 'SPI1'} else ('apb1', 'APB1ENR1'))
            selected[field].add(f'RCC_{register}_{controller}EN')
            if item.kind == 'pwm':
                selected['reset_apb1'].add(f'RCC_APB1RSTR1_{controller}RST')
            if item.kind in {'uart', 'i2c'}:
                selected['mux'].add(f'RCC_CCIPR_{controller}SEL')
        if item.kind in {'timer_capture', 'encoder'}:
            selected['apb1'].add(f'RCC_APB1ENR1_{controller}EN')
            selected['reset_apb1'].add(f'RCC_APB1RSTR1_{controller}RST')
        if item.kind == 'can_fd':
            selected['apb1'].add('RCC_APB1ENR1_FDCANEN')
            selected['reset_apb1'].add('RCC_APB1RSTR1_FDCANRST')
            selected['mux'].add('RCC_CCIPR_FDCANSEL')
        if item.kind in {'pwm_group', 'pwm_adc'}:
            selected['apb2'].add('RCC_APB2ENR_TIM1EN')
            control = True
        if item.kind in {'adc', 'pwm_adc', 'adc_group'}:
            selected['mux'].add('RCC_CCIPR_ADC12SEL')
            selected['ahb2'].update({'RCC_AHB2ENR_ADC12EN', 'RCC_AHB2ENR_ADC345EN'})
            if item.mode == 'trigger-dma' or (item.kind == 'adc_group' and item.options.trigger == 'tim6-trgo'):
                selected['apb1'].add('RCC_APB1ENR1_TIM6EN')
            if item.kind == 'adc_group':
                strict_dma |= item.mode == 'regular-sequence-dma'
                if item.options.trigger == 'tim6-trgo':
                    selected['reset_apb1'].add('RCC_APB1RSTR1_TIM6RST')
            if item.kind == 'pwm_adc':
                selected['ahb1'].update({'RCC_AHB1ENR_DMA1EN', 'RCC_AHB1ENR_DMAMUX1EN'})
                selected['apb2'].add('RCC_APB2ENR_TIM15EN')
        if item.kind == 'exti':
            exti |= 1 << int(controller.removeprefix('EXTI'))
    for owner in result.pwm_shared_owners:
        selected['apb1'].add(f'RCC_APB1ENR1_{owner.controller}EN')
        selected['reset_apb1'].add(f'RCC_APB1RSTR1_{owner.controller}RST')
    dma = False
    for key in sorted({claim.key for claim in claims if claim.key.startswith('dma:')}):
        if re.fullmatch('dma:DMA1:stream1', key) is None:
            fail('No maintained selected G474 DMA admission: ' + key)
        dma = True
    vectors = sorted({claim.key.removeprefix('irq:') + '_IRQn' for claim in claims if claim.key.startswith('irq:')})
    for owner in result.pwm_shared_owners:
        vectors.append(owner.controller + '_IRQn')
    lines = ['static const nx_stm32g474_platform_config_t s_nx_platform_config = {',
             f'    .pins = {pins}, .pin_count = {pin_count}u,']
    for field, values in selected.items():
        lines.append(f'    .admit_{field} = ' + (' | '.join(sorted(values)) if values else '0u') + ',')
    lines += ['    .irq_masks = {']
    for bank in range(4):
        masks = [f'NX_G474_PLATFORM_IRQ_MASK({vector}, {bank}u)' for vector in sorted(set(vectors))]
        lines.append('        ' + (' | '.join(masks) if masks else '0u') + ',')
    lines += ['    },', f'    .exti_lines = {exti}u,']
    groups = dict(_peripheral_gates(result))
    for field, register in (('apb1', 'APB1ENR1'), ('apb1_2', 'APB1ENR2'),
                             ('apb2', 'APB2ENR'), ('ahb1', 'AHB1ENR'), ('ahb2', 'AHB2ENR')):
        lines.append(f'    .clocks_{field} = {groups.get(register, "0u")},')
        slot = '&s_nx_platform_peripheral_' + register.lower() + '_added' if register in groups else 'NULL'
        lines.append(f'    .added_{field} = {slot},')
    def pointer(condition, name):
        return '&s_nx_platform_' + name if condition else 'NULL'
    lines += [f'    .added_syscfg = {pointer(_syscfg(result), "syscfg_clock_added")},',
              f'    .owned_i2c_mux = {pointer(any(item.kind == "i2c" for item in result.controllers), "i2c_mux_owned")},',
              f'    .owned_adc_mux = {pointer(any(item.kind in {"adc", "pwm_adc", "adc_group"} for item in result.controllers), "adc_mux_owned")},',
              f'    .control_group = {str(control).lower()}, .dma_channel1 = {str(dma).lower()},',
              f'    .strict_dma_channel1 = {str(strict_dma).lower()}, .flash = {str(any(item.kind == "flash" for item in result.controllers)).lower()}', '};']
    return lines


def selected_preflight(result, *, dma_conditions=None):
    del dma_conditions
    if not result.controllers:
        return []
    return ['    result.primary = nx_stm32g474_platform_preflight(&s_nx_platform_config);',
            '    if (result.primary != NX_SUCCESS) { return result; }']
