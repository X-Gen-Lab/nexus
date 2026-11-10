"""STM32F407 selected cold-resource admission policy."""
import re

from tools.configure.providers import hardware
from tools.configure.providers.common import fail, owned_claims
from .hardware import PWM_TIMERS


def _clock(controller):
    """Name a maintained exclusive gate, not an inferred silicon topology."""
    if controller.startswith(('USART', 'UART', 'SPI', 'I2C')):
        bus = hardware.maintained('stm32f4', controller).apb
    elif controller in PWM_TIMERS:
        bus = PWM_TIMERS[controller].apb
    elif controller in {'TIM1', 'TIM8', 'TIM6', 'TIM7'}:
        bus = 2 if controller in {'TIM1', 'TIM8'} else 1
    elif controller == 'DAC':
        bus = 1
    elif controller in {'CAN1', 'CAN2'}:
        bus = 1
    else:
        fail('No maintained cold clock assertion for ' + controller)
    return (f'RCC->APB{bus}ENR', f'RCC_APB{bus}ENR_{controller}EN')


def admission_declarations(result, pins, pin_count):
    """Project static physical selections; concrete C owns all observations."""
    source = []
    exti = 0
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    clocks = set()
    for item in result.controllers:
        if item.kind in {'uart', 'spi', 'i2c', 'i2c_target', 'pwm', 'encoder', 'timer_capture', 'advanced_pwm', 'can', 'dac'}:
            clocks.add(_clock(item.controller))
        elif item.kind in {'adc', 'adc_group'}:
            register = 'RCC->APB2ENR'
            names = ('ADC1', 'ADC2', 'ADC3')
            prefix = 'RCC_APB2ENR_'
            clocks.add((register, ' | '.join((prefix + name + 'EN' for name in names))))
        elif item.kind in {'usb_device', 'usb_host'}:
            if item.controller == 'OTG_FS':
                clocks.add(('RCC->AHB2ENR', 'RCC_AHB2ENR_OTGFSEN'))
            else:
                clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_OTGHSEN'))
                if item.mode in {'host-hs-ulpi', 'device-hs-ulpi', 'otg-hs-ulpi'}:
                    clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_OTGHSULPIEN'))
        elif item.kind == 'capture':
            clocks.add(('RCC->AHB2ENR', 'RCC_AHB2ENR_DCMIEN'))
        elif item.kind == 'audio':
            clocks.add(('RCC->APB1ENR', f'RCC_APB1ENR_SPI{item.controller[-1]}EN'))
        elif item.kind == 'block':
            clocks.add(('RCC->APB2ENR', 'RCC_APB2ENR_SDIOEN'))
        elif item.kind in {'eth', 'eth_frames'}:
            clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACTXEN | RCC_AHB1ENR_ETHMACRXEN'))
            if item.kind == 'eth_frames':
                clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_ETHMACPTPEN'))
        elif item.kind == 'exti':
            line = int(item.controller.removeprefix('EXTI'))
            exti |= 1 << line
    for item in claims:
        timer = item.key.removeprefix('controller:')
        if timer in PWM_TIMERS or timer in {'TIM1', 'TIM6', 'TIM7', 'TIM8'}:
            clocks.add(_clock(timer))
    if result.can_domain is not None:
        clocks.add(_clock('CAN1'))
    streams = []
    for key in sorted({item.key for item in claims if item.key.startswith('dma:')}):
        match = re.fullmatch('dma:(DMA[0-9]+):stream([0-7])', key)
        if match is None:
            fail('No maintained selected DMA lane admission: ' + key)
        dma, lane = match.groups()
        streams.append(f'{dma}_Stream{lane}')
    if streams:
        source += ['static DMA_Stream_TypeDef* const s_nx_platform_dma_streams[] = {',
                   *[f'    {stream},' for stream in streams], '};']
    source += ['static const nx_stm32_platform_config_t s_nx_platform_config = {',
               f'    .pins = {pins}, .pin_count = {pin_count}u,',
               '    .dma_streams = ' + ('s_nx_platform_dma_streams,' if streams else 'NULL,'),
               f'    .dma_stream_count = {len(streams)}u,']
    for field, register in (('apb1_clocks', 'RCC->APB1ENR'),
                            ('apb2_clocks', 'RCC->APB2ENR'),
                            ('ahb1_clocks', 'RCC->AHB1ENR'),
                            ('ahb2_clocks', 'RCC->AHB2ENR')):
        masks = sorted(mask for selected, mask in clocks if selected == register)
        source.append(f'    .{field} = ' + (' | '.join(masks) if masks else '0u') + ',')
    vectors = sorted({item.key.removeprefix('irq:') + '_IRQn' for item in claims
                      if item.key.startswith('irq:')})
    source += ['    .irq_masks = {']
    for bank in range(3):
        terms = [f'NX_STM32_PLATFORM_IRQ_MASK({vector}, {bank}u)'
                 for vector in vectors]
        source.append('        ' + (' | '.join(terms) if terms else '0u') + ',')
    source += ['    },', f'    .exti_lines = {exti}u', '};']
    return source
