"""STM32F407 selected cold-resource admission policy."""
import re

from tools.configure import gpio_phase
from tools.configure.providers import hardware
from tools.configure.providers.common import fail, owned_claims
from tools.configure.preflight import _reject


PREFLIGHT_CALL = 'nx_stm32_system_preflight(&g_nx_stm32_system)'


def _clock(controller):
    """Name a maintained exclusive gate, not an inferred silicon topology."""
    if controller.startswith(('USART', 'SPI', 'I2C')):
        bus = hardware.maintained('stm32f407', controller).apb
    elif re.fullmatch('TIM[1349]', controller):
        bus = 2 if controller in {'TIM1', 'TIM9'} else 1
    elif controller in {'CAN1', 'CAN2'}:
        bus = 1
    else:
        fail('No maintained cold clock assertion for ' + controller)
    return (f'RCC->APB{bus}ENR', f'RCC_APB{bus}ENR_{controller}EN')


def selected_preflight(result, *, dma_conditions=None):
    """Project exact selected leases; base preflight owns the reserved timer."""
    source = []
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    clocks = set()
    for item in result.controllers:
        if item.kind in {'uart', 'spi', 'i2c', 'pwm', 'timer_capture', 'advanced_pwm', 'can'}:
            clocks.add(_clock(item.controller))
        elif item.kind == 'adc':
            register = 'RCC->APB2ENR'
            names = ('ADC1', 'ADC2', 'ADC3')
            prefix = 'RCC_APB2ENR_'
            clocks.add((register, ' | '.join((prefix + name + 'EN' for name in names))))
        elif item.kind == 'usb_device':
            clocks.add(('RCC->AHB2ENR', 'RCC_AHB2ENR_OTGFSEN'))
        elif item.kind in {'eth', 'eth_frames'}:
            clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACTXEN | RCC_AHB1ENR_ETHMACRXEN'))
            if item.kind == 'eth_frames':
                clocks.add(('RCC->AHB1ENR', 'RCC_AHB1ENR_ETHMACPTPEN'))
        elif item.kind == 'exti':
            line = int(item.pins[0]['pin'][2:])
            registers = 'EXTI->IMR | EXTI->EMR | EXTI->PR'
            source += _reject(f'(({registers}) & {1 << line}u) != 0u')
    for item in claims:
        timer = item.key.removeprefix('controller:')
        if re.fullmatch('TIM[1349]|TIMER[0238]', timer):
            clocks.add(_clock(timer))
    if result.can_domain is not None:
        clocks.add(_clock('CAN1'))
    for register, mask in sorted(clocks):
        source += _reject(f'({register} & ({mask})) != 0u')
    for key in sorted({item.key for item in claims if item.key.startswith('irq:')}):
        vector = key.removeprefix('irq:') + '_IRQn'
        bank = f'(uint32_t){vector} / 32u'
        mask = f'(1u << ((uint32_t){vector} % 32u))'
        source += _reject(f'((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | NVIC->IABR[{bank}]) & {mask}) != 0u')
    for key in sorted({item.key for item in claims if item.key.startswith('dma:')}):
        match = re.fullmatch('dma:(DMA[0-9]+):stream([0-7])', key)
        if match is None:
            fail('No maintained selected DMA lane admission: ' + key)
        dma, lane = match.groups()
        control = f'{dma}_Stream{lane}->CR'
        source += _reject(f'({control} & 1u) != 0u')
    return source + gpio_phase.preflight(result)

