"""STM32G474 selected cold-resource admission policy."""
import re

from tools.configure import gpio_phase
from tools.configure.providers.common import fail, owned_claims
from tools.configure.preflight import _reject


PREFLIGHT_CALL = 'nx_stm32g474_system_preflight(&g_nx_stm32g474_system)'


def selected_preflight(result, *, dma_conditions=None):
    """Project exact selected leases; base preflight owns the reserved timer."""
    source = []
    owners = {item.id for item in result.controllers}
    claims = tuple(owned_claims(result, owners))
    for item in result.controllers:
        if item.kind in {'uart', 'spi', 'pwm', 'i2c'}:
            controller = item.controller
            register = 'APB1ENR2' if controller == 'LPUART1' else 'APB2ENR' if controller in {'USART1', 'SPI1'} else 'APB1ENR1'
            source += _reject(f'(NX_G474_READ(&RCC->{register}) & RCC_{register}_{controller}EN) != 0u')
            if item.kind == 'pwm':
                source += _reject(f'(NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_{controller}RST) != 0u')
            if item.kind == 'uart':
                source += _reject(f'(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_{controller}SEL) != 0u')
            if item.kind == 'i2c':
                source += _reject('(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_I2C1SEL) != 0u')
        if item.kind == 'can_fd':
            source += _reject('(NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_FDCANEN) != 0u || (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_FDCANRST) != 0u || (NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_FDCANSEL) != 0u')
        if item.kind in {'adc', 'pwm_adc', 'adc_group'}:
            source += _reject('(NX_G474_READ(&RCC->CCIPR) & RCC_CCIPR_ADC12SEL) != 0u')
            source += _reject('(NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC12EN) != 0u')
            source += _reject('(NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_ADC345EN) != 0u')
            if item.mode == 'trigger-dma':
                source += _reject('(NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM6EN) != 0u')
            if item.kind == 'adc_group':
                source += _reject('!NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn)')
                if item.options.trigger == 'tim6-trgo':
                    source += _reject('(NX_G474_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM6EN) != 0u || (NX_G474_READ(&RCC->APB1RSTR1) & RCC_APB1RSTR1_TIM6RST) != 0u')
                if item.mode == 'regular-sequence-dma':
                    source += _reject('!NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn)')
                    source += ['    if ((NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u) {']
                    source += _reject('NX_G474_READ(&DMA1_Channel1->CCR) != 0u || NX_G474_READ(&DMA1_Channel1->CNDTR) != 0u || NX_G474_READ(&DMA1_Channel1->CPAR) != 0u || NX_G474_READ(&DMA1_Channel1->CMAR) != 0u', indent=8)
                    source += ['    }']
            if item.kind == 'pwm_adc':
                source += _reject('(NX_G474_READ(&RCC->AHB1ENR) & (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN)) != 0u || (NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM15EN) != 0u')
    for item in result.controllers:
        if item.kind == 'flash':
            source += _reject('(NX_G474_READ(&FLASH->ECCR) & FLASH_ECCR_ECCIE) != 0u')
        if item.kind == 'exti':
            line = int(item.controller.removeprefix('EXTI'))
            source += _reject(f'((NX_G474_READ(&EXTI->IMR1) | NX_G474_READ(&EXTI->EMR1) | NX_G474_READ(&EXTI->RTSR1) | NX_G474_READ(&EXTI->FTSR1) | NX_G474_READ(&EXTI->PR1)) & {1 << line}u) != 0u')
    for key in sorted({item.key for item in claims if item.key.startswith('irq:')}):
        vector = key.removeprefix('irq:') + '_IRQn'
        source += _reject(f'!NX_G474_IRQ_AVAILABLE((IRQn_Type){vector})')
    for key in sorted({item.key for item in claims if item.key.startswith('dma:')}):
        match = re.fullmatch('dma:(DMA[0-9]+):stream([0-7])', key)
        if match is None:
            fail('No maintained selected DMA lane admission: ' + key)
        dma, lane = match.groups()
        source += _reject(f'(NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u && NX_G474_READ(&{dma}_Channel{lane}->CCR) != 0u')
        source += _reject('(NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMAMUX1EN) != 0u && NX_G474_READ(&DMAMUX1_Channel0->CCR) != 0u')
    return source + gpio_phase.preflight(result)

