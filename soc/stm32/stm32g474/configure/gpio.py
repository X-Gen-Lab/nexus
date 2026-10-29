"""Static G474 pad/resource connections to the one concrete C lease."""


def static_declarations(result, pins, banks, slots):
    from .preflight import admission_declarations
    del banks
    source = ['static nx_stm32g474_platform_pins_t s_nx_platform_pins;',
              'static const nx_stm32g474_platform_pin_t s_nx_platform_pin_config[] = {']
    for pin in pins:
        source.append(f'    {{ GPIO{pin[1]}, &{slots[pin]}, {ord(pin[1]) - ord("A")}u, {int(pin[2:])}u }},')
    source += ['};']
    return source + admission_declarations(result, 's_nx_platform_pin_config', len(pins))


def cold_call(operation):
    arguments = '&s_nx_platform_config, &s_nx_platform_pins'
    return {
        'admit': 'nx_stm32g474_platform_pins_admit(&s_nx_platform_config, true)',
        'acquire': f'nx_stm32g474_platform_pins_acquire({arguments})',
        'release': f'nx_stm32g474_platform_pins_release({arguments})',
        'clocks_release': 'nx_stm32g474_platform_gpio_clocks_release(&s_nx_platform_pins)',
        'abort': 'nx_stm32g474_platform_pins_abort(&s_nx_platform_pins)',
    }[operation]
