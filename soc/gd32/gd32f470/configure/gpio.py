"""GD32F470 static pad records and direct cold C lifecycle connections."""


def static_declarations(result, pins, banks, slots):
    """Only immutable pad/resource records and one existing-size C receipt."""
    from .preflight import admission_declarations
    del banks
    source = ['static nx_gd32_platform_pin_receipt_t s_nx_platform_pins;',
              'static const nx_gd32_platform_pin_config_t s_nx_platform_pins_config[] = {']
    for pin in pins:
        source.append(f'    {{ GPIO{pin[1]}, &{slots[pin]}, '
                      f'{ord(pin[1]) - ord("A")}u, {int(pin[2:])}u }},')
    source += ['};']
    source += admission_declarations(result, 's_nx_platform_pins_config', len(pins))
    return source


def cold_call(operation):
    """Connect a fixed C function; no register algorithm remains in this hook."""
    arguments = '&s_nx_platform_config, &s_nx_platform_pins'
    functions = {
        'admit': 'nx_gd32_platform_pins_admit(&s_nx_platform_config, true)',
        'acquire': f'nx_gd32_platform_pins_acquire({arguments})',
        'release': f'nx_gd32_platform_pins_release({arguments})',
        'clocks_release': 'nx_gd32_platform_gpio_clocks_release(&s_nx_platform_pins)',
        'abort': 'nx_gd32_platform_pins_abort(&s_nx_platform_pins)',
    }
    return functions[operation]
