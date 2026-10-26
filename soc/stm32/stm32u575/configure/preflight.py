"""STM32U575 selected cold-resource admission policy."""
from tools.configure.providers.common import fail, owned_claims
from tools.configure.preflight import _reject


PREFLIGHT_CALL = 'nx_stm32u575_system_preflight(&g_nx_stm32u575_system)'


def selected_preflight(result, *, dma_conditions=None):
    """Cold GPIO/byte-UART profiles keep family-specific banks and SDK hooks."""
    hook = 'U575'
    gate_register = 'APB2ENR'
    uart = 'USART1'
    mux_register = 'CCIPR1'
    mux = 'RCC_CCIPR1_USART1SEL'
    gpio_register = 'AHB2ENR1'
    source = []
    pads = {}
    for item in result.controllers:
        allowed = {'gpio', 'uart', 'spi', 'pwm', 'exti', 'watchdog', 'adc', 'i2c', 'flash'}
        allowed.update({'lptim', 'rtc'})
        if item.kind == 'rtc':
            source += ['    result.primary = nx_stm32u575_rtc_preflight();', '    if (result.primary != NX_SUCCESS) { return result; }']
        if item.kind not in allowed:
            fail('New SoC cold profile has no maintained controller admission')
        if item.kind == 'lptim':
            source += _reject('(NX_U575_READ(&RCC->APB1RSTR2) & RCC_APB1RSTR2_LPTIM2RST) != 0u || (NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_LPTIM2SEL) != 0u || !NX_U575_IRQ_AVAILABLE(LPTIM2_IRQn)')
            # A disabled gate cannot establish cold peripheral ownership.
            # The constructor repeats admission after acquiring only its gate.
            source.append('    if ((NX_U575_READ(&RCC->APB1ENR2) & RCC_APB1ENR2_LPTIM2EN) != 0u) {')
            conditions = [f'NX_U575_READ(&LPTIM2->{name}) != 0u' for name in ('CR', 'CFGR', 'CFGR2', 'DIER', 'ISR', 'CCMR1', 'RCR', 'CNT', 'CCR1', 'CCR2')]
            conditions.append('NX_U575_READ(&LPTIM2->ARR) != 1u')
            source += _reject(' || '.join(conditions), indent=8)
            source.append('    }')
        if item.kind == 'uart':
            uart = item.controller
            if uart not in {'USART1', 'USART2'}:
                fail('U575 cold profile requires a reviewed USART')
            gate_register = 'APB2ENR' if uart == 'USART1' else 'APB1ENR1'
            mux = 'RCC_CCIPR1_' + uart + 'SEL'
            if item.controller != uart:
                fail('New SoC cold profile requires its exact reviewed UART')
            source += _reject(f'(NX_{hook}_READ(&RCC->{gate_register}) & RCC_{gate_register}_{uart}EN) != 0u')
            source += _reject(f'(NX_{hook}_READ(&RCC->{mux_register}) & {mux}) != 0u')
            source += _reject(f'!NX_{hook}_IRQ_AVAILABLE((IRQn_Type){uart}_IRQn)')
            if item.get('mode') in {'dma-tx', 'dma-chain-tx'}:
                source += _reject('!NX_U575_IRQ_AVAILABLE(GPDMA1_Channel0_IRQn)')
        elif item.kind in {'spi', 'pwm', 'i2c', 'adc'}:
            register = 'APB2ENR' if item.controller == 'SPI1' else 'AHB3ENR' if item.controller == 'ADC4' else 'APB1ENR1'
            source += _reject(f'(NX_U575_READ(&RCC->{register}) & RCC_{register}_{item.controller}EN) != 0u')
            if item.kind in {'spi', 'i2c'}:
                source += _reject(f'(NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_{item.controller}SEL) != 0u')
                if item.kind == 'i2c':
                    source += _reject('(NX_U575_READ(&RCC->CR) & (RCC_CR_HSION | RCC_CR_HSIKERON | RCC_CR_HSIRDY)) != 0u')
                    source += _reject('(NX_U575_READ(&RCC->ICSCR3) & RCC_ICSCR3_HSITRIM) != RCC_ICSCR3_HSITRIM_4')
            elif item.kind == 'adc':
                source += _reject('(NX_U575_READ(&RCC->CCIPR3) & RCC_CCIPR3_ADCDACSEL) != 0u || (NX_U575_READ(&RCC->AHB2ENR1) & RCC_AHB2ENR1_ADC12EN) != 0u || (NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0u || (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_ASV) == 0u')
        elif item.kind == 'flash':
            source += _reject('!NX_U575_IRQ_AVAILABLE(FLASH_IRQn)')
        elif item.kind == 'exti':
            line = int(item.controller[-1])
            source += _reject(f'!NX_U575_IRQ_AVAILABLE((IRQn_Type)EXTI{line}_IRQn)')
            source += _reject(f'((NX_U575_READ(&EXTI->IMR1) | NX_U575_READ(&EXTI->EMR1) | NX_U575_READ(&EXTI->RTSR1) | NX_U575_READ(&EXTI->FTSR1) | NX_U575_READ(&EXTI->RPR1) | NX_U575_READ(&EXTI->FPR1)) & {1 << line}u) != 0u')
        for pin in item['pins']:
            name = pin['pin']
            pads.setdefault(name[1], set()).add(int(name[2:]))
    if 'claims' in result:
        owners = {item.id for item in result.controllers}
        owners.update((item['id'] for item in result['devices']))
        for claim in owned_claims(result, owners):
            if claim['key'].startswith('pin:'):
                pin = claim['key'][4:]
                pads.setdefault(pin[1], set()).add(int(pin[2:]))
    for port, numbers in sorted(pads.items()):
        mask = sum((1 << number for number in numbers))
        if port == 'G' and mask & ~3:
            source += _reject('(NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0u || (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_IO2SV) == 0u')
        source += [f'    if ((NX_{hook}_READ(&RCC->{gpio_register}) & RCC_{gpio_register}_GPIO{port}EN) != 0u) {{', f'        uint32_t mode = NX_{hook}_READ(&GPIO{port}->MODER);']
        for number in sorted(numbers):
            bits = f'((mode >> {number * 2}u) & 3u)'
            source += _reject(f'{bits} != 0u && {bits} != 3u', indent=8)
        source += _reject(f'(NX_{hook}_READ(&GPIO{port}->LCKR) & {mask}u) != 0u', indent=8)
        source.append('    }')
    return source

