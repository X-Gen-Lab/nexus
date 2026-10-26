"""STM32H743 selected cold-resource admission policy."""
from tools.configure.providers.common import fail
from tools.configure.preflight import _reject


PREFLIGHT_CALL = 'nx_stm32h743_system_preflight(&g_nx_stm32h743_system)'


def selected_preflight(result, *, dma_conditions=None):
    """Cold GPIO/byte-UART profiles keep family-specific banks and SDK hooks."""
    hook = 'H743'
    gate_register = 'APB1LENR'
    uart = 'USART3'
    mux_register = 'D2CCIP2R'
    mux = 'RCC_D2CCIP2R_USART28SEL'
    gpio_register = 'AHB4ENR'
    source = []
    pads = {}
    if result.dma_topology.lanes:
        # A shared reset is another owner's observable responsibility, even
        # when its clock is off and stream reads would look reset-like.
        source += _reject('(NX_H743_READ(&RCC->AHB1RSTR) & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0u')
    for item in result.controllers:
        allowed = {'gpio', 'uart', 'spi', 'pwm', 'exti', 'watchdog', 'adc', 'i2c', 'flash'}
        if item.kind not in allowed:
            fail('New SoC cold profile has no maintained controller admission')
        if item.kind in {'spi', 'pwm'}:
            register = 'APB2ENR' if item.kind == 'spi' else 'APB1LENR'
            reset = 'APB2RSTR' if item.kind == 'spi' else 'APB1LRSTR'
            source += _reject(f'(NX_H743_READ(&RCC->{register}) & RCC_{register}_{item.controller}EN) != 0u || (NX_H743_READ(&RCC->{reset}) & RCC_{reset}_{item.controller}RST) != 0u')
            if item.kind == 'spi':
                source += _reject('(NX_H743_READ(&RCC->D2CCIP1R) & RCC_D2CCIP1R_SPI45SEL) != 0u')
                if item.mode == 'dma':
                    if dma_conditions is None:
                        fail('H743 DMA preflight requires its explicit provider admission contract')
                    for irq in ('DMA1_Stream2', 'DMA1_Stream3', 'SPI4'):
                        source += _reject(f'!NX_H743_IRQ_AVAILABLE({irq}_IRQn)')
                    source += ['    if ((NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u) {']
                    source += _reject(' || '.join(dma_conditions(result, spi_only=True)), indent=8)
                    source.append('    }')
        if item.kind == 'exti':
            line = int(item.controller.removeprefix('EXTI'))
            source += _reject(f'((NX_H743_READ(&EXTI->IMR1) | NX_H743_READ(&EXTI->EMR1) | NX_H743_READ(&EXTI->RTSR1) | NX_H743_READ(&EXTI->FTSR1) | NX_H743_READ(&EXTI->PR1)) & {1 << line}u) != 0u || !NX_H743_IRQ_AVAILABLE(EXTI{line}_IRQn)')
        if item.kind == 'i2c':
            source += _reject('(NX_H743_READ(&RCC->APB1LENR) & RCC_APB1LENR_I2C1EN) != 0u || (NX_H743_READ(&RCC->APB1LRSTR) & RCC_APB1LRSTR_I2C1RST) != 0u || (NX_H743_READ(&RCC->D2CCIP2R) & RCC_D2CCIP2R_I2C123SEL) != 0u')
        if item.kind == 'adc':
            source += _reject('(NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_ADC12EN) != 0u || (NX_H743_READ(&RCC->AHB1RSTR) & RCC_AHB1RSTR_ADC12RST) != 0u')
        if item.kind == 'flash':
            source += _reject('((NX_H743_READ(&FLASH->SR1) | NX_H743_READ(&FLASH->SR2)) & (FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE)) != 0u || (NX_H743_READ(&FLASH->OPTSR_CUR) & FLASH_OPTSR_SWAP_BANK_OPT) != 0u')
        if item.kind == 'uart':
            if item.controller != uart:
                fail('New SoC cold profile requires its exact reviewed UART')
            source += _reject(f'(NX_{hook}_READ(&RCC->{gate_register}) & RCC_{gate_register}_{uart}EN) != 0u')
            source += _reject(f'(NX_{hook}_READ(&RCC->{mux_register}) & {mux}) != 0u')
            source += _reject(f'!NX_{hook}_IRQ_AVAILABLE((IRQn_Type){uart}_IRQn)')
            if item.get('mode') in {'dma-tx', 'rx-dma-idle', 'rx-dma-continuous', 'dma-duplex-continuous'}:
                lanes = (0, 1) if item.get('mode') == 'dma-duplex-continuous' else (0,) if item.get('mode') == 'dma-tx' else (1,)
                for lane in lanes:
                    source += _reject(f'!NX_H743_IRQ_AVAILABLE(DMA1_Stream{lane}_IRQn)')
                source.append('    if ((NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0u) {')
                for lane in lanes:
                    source += _reject(f'NX_H743_READ(&DMA1_Stream{lane}->CR) != 0u || NX_H743_READ(&DMAMUX1_Channel{lane}->CCR) != 0u', indent=8)
                source.append('    }')
        for pin in item['pins']:
            name = pin['pin']
            pads.setdefault(name[1], set()).add(int(name[2:]))
    for item in result.devices:
        binding = result['board_bindings'].get(item.get('cs_binding'))
        if binding is not None:
            for pin in result['routes'][binding['route']]['pins']:
                name = pin['pin']
                pads.setdefault(name[1], set()).add(int(name[2:]))
    for port, numbers in sorted(pads.items()):
        mask = sum((1 << number for number in numbers))
        source += [f'    if ((NX_{hook}_READ(&RCC->{gpio_register}) & RCC_{gpio_register}_GPIO{port}EN) != 0u) {{', f'        uint32_t mode = NX_{hook}_READ(&GPIO{port}->MODER);']
        for number in sorted(numbers):
            bits = f'((mode >> {number * 2}u) & 3u)'
            source += _reject(f'{bits} != 0u && {bits} != 3u', indent=8)
        source += _reject(f'(NX_{hook}_READ(&GPIO{port}->LCKR) & {mask}u) != 0u', indent=8)
        source.append('    }')
    return source

