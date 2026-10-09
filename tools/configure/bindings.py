"""Narrow static hardware assembly emission from already validated facts."""
try:
    from . import stm32_bindings
except ImportError:
    import stm32_bindings


def symbol(name):
    return name.replace("-", "_")


def emit(result):
    family = result["soc_family"]
    selections = result["controllers"]
    header = ['/**', ' * \\file            nexus_bindings.h',
              ' * \\brief           Fixed typed aliases selected by one assembly.',
              ' * \\author          Nexus Team', ' */',
              '#ifndef NEXUS_BINDINGS_H', '#define NEXUS_BINDINGS_H',
              '#include "nexus/core/status.h"']
    for kind in sorted({item["kind"] for item in selections}):
        header.append(f'#include "nexus/io/{"timer" if kind == "pwm" else kind}.h"')
    header += ['#ifdef __cplusplus', 'extern "C" {', '#endif', 'typedef struct {', '    nx_result_t primary;',
               '    nx_result_t cleanup;', '    uint32_t remaining_effects;',
               '} nx_platform_start_result_t;',
               '/** \\brief Startup-only; no scheduler or product worker is created.',
               ' * \\return Primary/cleanup errors and retained effects. On error',
               ' * all ports remain unusable; preserve contexts until cleanup.',
               ' */', 'nx_platform_start_result_t nx_platform_start(void);',
               '/** \\brief Stop after direct writers/IRQ consumers quiesce.',
               ' * \\return SUCCESS after drain; BUSY retains all storage and clock.',
               ' * The owner must continue service before retrying BUSY.',
               ' */', 'nx_result_t nx_platform_stop(void);']
    source = ['/**', ' * \\file            bindings.c',
              ' * \\brief           Generated fixed typed hardware construction.',
              ' * \\author          Nexus Team', ' * \\version         1.0.0',
              ' * \\date            2026-10-09',
              ' * \\copyright       Copyright (c) 2026 Nexus Team', ' */',
              '#include "nexus_bindings.h"', '#include "nexus_config.h"',
              '#include "nexus/arch/arch.h"']
    if family == "stm32f407":
        source.append('#include "stm32f407_provider.h"')
    elif family == "gd32f470":
        source.append('#include "gd32f470_provider.h"')
    else:
        source += ['#include "nexus/io/native/model.h"',
                   '#include "provider.h"']
    if family != "native":
        source += ['void _init(void);', 'void _fini(void);',
                   r'/** \brief C runtime hooks; constructors use .init_array. */',
                   'void _init(void) {}', 'void _fini(void) {}']
    source += ['static bool s_started;', 'static size_t s_initialized;']
    gpio_clock_mask = 0
    if family == "stm32f407":
        pins = [pin["pin"] for item in selections for pin in item["pins"]]
        for device in result["devices"]:
            if "cs_binding" in device:
                binding = result["board_bindings"][device["cs_binding"]]
                pins.extend(pin["pin"] for pin in result["routes"][binding["route"]]["pins"])
        gpio_clock_mask = sum(1 << index for index in {ord(pin[1]) - ord("A") for pin in pins})
        if gpio_clock_mask:
            source.append('static uint32_t s_gpio_clocks_added;')
        if any(item["kind"] == "exti" for item in selections):
            source.append('static uint32_t s_syscfg_clock_added;')
    initialize = []
    stop = []
    for item in selections:
        name = symbol(item["id"])
        kind = item["kind"]
        kind_type = "pwm" if kind == "pwm" else kind
        const = "const " if kind in {"spi", "i2c"} else ""
        exported_type = f'nx_{kind_type}_{"endpoint" if const else "port"}_t'
        header += ['/** \\brief Fixed binding; no lookup, ownership or teardown check. */',
                   f'extern {const}{exported_type}* const nx_binding_{name};']
        if family == "native":
            model_object = f'&g_nx_native_{kind}'
            source.append(f'{const}{exported_type}* const nx_binding_{name} = {model_object};')
        else:
            source.append(f'static nx_{kind_type}_port_t s_{name};')
            if kind in {"spi", "i2c"}:
                source.append(f'static nx_{kind}_endpoint_t s_{name}_endpoint;')
                source.append(f'const {exported_type}* const nx_binding_{name} = &s_{name}_endpoint;')
            else:
                source.append(f'{exported_type}* const nx_binding_{name} = &s_{name};')
        if family == "stm32f407" and kind not in {"gpio", "uart"}:
            construction = stm32_bindings.constructor(
                item, result, result["board_bindings"], result["routes"], result["devices"])
            source.extend(construction["definitions"])
            init = construction["initialize"]
            stop.extend((len(initialize) + 1, expression)
                        for expression in construction["stop"])
        elif family == "gd32f470" and kind not in {"gpio", "uart"}:
            try:
                from . import gd32_bindings
            except ImportError:
                import gd32_bindings
            construction = gd32_bindings.constructor(
                item, result, result["board_bindings"], result["routes"], result["devices"])
            source.extend(construction["definitions"])
            init = construction["initialize"]
            stop.extend((len(initialize) + 1, expression)
                        for expression in construction["stop"])
        elif kind == "gpio":
            port = item["pins"][0]["pin"][1]
            index = ord(port) - ord("A")
            mask = item["mask"] or sum(1 << int(pin["pin"][2:]) for pin in item["pins"])
            initial = item["initial"]
            if family == "native":
                init = f'nx_native_gpio_configure({mask}u, {initial}u)'
            elif family == "stm32f407":
                source += [f'/* Fixed {item["controller"]} authorization. */',
                           f'static nx_result_t prepare_{name}(void) {{',
                           f'    s_{name}.registers = GPIO{port};',
                           f'    s_{name}.mask = {mask}u;',
                           f'    s_{name}.output = {"true" if item["mode"] == "output" else "false"};',
                           f'    uint32_t saved_clock = RCC->AHB1ENR & {1 << index}u;',
                           f'    nx_result_t clock = nx_stm32_gpio_clock_enable({index}u);',
                           f'    if (clock != NX_SUCCESS) {{ RCC->AHB1ENR = (RCC->AHB1ENR & ~{1 << index}u) | saved_clock; return clock; }}']
                for pin in item["pins"]:
                    number = pin["pin"][2:]
                    source.append(f'    uint16_t saved_{number} = nx_stm32_pin_capture(GPIO{port}, {number}u);')
                source += [f'    nx_result_t status = nx_stm32_gpio_initialize(&s_{name}, {initial}u);',
                           '    if (status != NX_SUCCESS) {']
                for pin in item["pins"]:
                    number = pin["pin"][2:]
                    source.append(f'        nx_stm32_pin_restore(GPIO{port}, {number}u, saved_{number});')
                source += [f'        RCC->AHB1ENR = (RCC->AHB1ENR & ~{1 << index}u) | saved_clock;',
                           '        nx_arch_dsb();', '    }', '    return status;', '}']
                init = f'prepare_{name}()'
                stop.append((len(initialize) + 1, f'nx_stm32_gpio_stop(&s_{name}, {initial}u)'))
            else:
                init = f'nx_gd32_gpio_initialize(&s_{name}, {index}u, {mask}u, {initial}u, {"true" if item["mode"] == "output" else "false"})'
                stop.append((len(initialize) + 1, f'nx_gd32_gpio_stop(&s_{name}, {initial}u)'))
        elif kind == "uart":
            profile = 'NX_UART_RX_EVENTS' if item["rx_profile"] == "events" else 'NX_UART_RX_BYTES'
            storage = 'nx_uart_rx_event_t' if item["rx_profile"] == "events" else 'uint8_t'
            capacity = item["rx_capacity"]
            source.append(f'static {storage} s_{name}_rx[{capacity}];')
            if family == "native":
                source += [f'static const nx_native_uart_config_t s_{name}_config = {{',
                           f'    .profile = {profile}, .rx_storage = s_{name}_rx,',
                           f'    .rx_capacity = {capacity}u, .automatic_irq = true', '} ;']
                init = f'nx_native_uart_configure(&s_{name}_config)'
            elif family == "stm32f407":
                source += [f'static nx_result_t prepare_{name}(void) {{',
                           f'    s_{name}.registers = {item["controller"]};',
                           f'    s_{name}.rx_storage = s_{name}_rx;',
                           f'    s_{name}.rx_capacity = {capacity}u;',
                           f'    s_{name}.baud = {item["baud"]}u;',
                           f'    s_{name}.profile = {profile};',
                           f'    s_{name}.irq = {item["controller"]}_IRQn;',
                           '    uint32_t saved_clock = RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN;',
                           '    uint32_t saved_uart_clock = RCC->APB2ENR & RCC_APB2ENR_USART1EN;',
                           '    nx_result_t clock = nx_stm32_gpio_clock_enable(0u);',
                           '    if (clock != NX_SUCCESS) {',
                           '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock;',
                           '        return clock;', '    }',
                           '    uint16_t saved_tx = nx_stm32_pin_capture(GPIOA, 9u);',
                           '    uint16_t saved_rx = nx_stm32_pin_capture(GPIOA, 10u);',
                           '    nx_result_t pins = nx_stm32_uart1_pins_prepare();',
                           f'    nx_result_t uart = pins == NX_SUCCESS ? nx_stm32_uart_initialize(&s_{name}, 84000000u) : pins;',
                           '    if (uart != NX_SUCCESS) {',
                           '        nx_stm32_pin_restore(GPIOA, 9u, saved_tx);',
                           '        nx_stm32_pin_restore(GPIOA, 10u, saved_rx);',
                           '        RCC->APB2ENR = (RCC->APB2ENR & ~(uint32_t)RCC_APB2ENR_USART1EN) | saved_uart_clock;',
                           '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock;',
                           '        nx_arch_dsb();', '        return uart;', '    }',
                           f'    NVIC_SetPriority(s_{name}.irq, {item["irq_priority"]}u);',
                           '    return NX_SUCCESS;', '}',
                           f'static nx_result_t stop_{name}(void) {{',
                           f'    nx_result_t status = nx_uart_port_stop(&s_{name});',
                           '    if (status != NX_SUCCESS) { return status; }',
                           '    (void)nx_stm32_pin_configure(GPIOA, 9u, 0u, 0u, 0u, false);',
                           '    (void)nx_stm32_pin_configure(GPIOA, 10u, 0u, 0u, 0u, false);',
                           '    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_USART1EN;',
                           '    nx_arch_dsb();', '    return NX_SUCCESS;', '}']
                init = f'prepare_{name}()'
                irq = item["controller"] + '_IRQHandler'
                source += [f'void {irq}(void);', '/** \\brief Fixed single-controller IRQ dispatch. */',
                           f'void {irq}(void) {{ nx_stm32_uart_irq(&s_{name}); }}']
            else:
                init = f'nx_gd32_uart_initialize(&s_{name}, {item["baud"]}u, {profile}, s_{name}_rx, {capacity}u, {item["irq_priority"]}u)'
            stop.append((len(initialize) + 1, f'stop_{name}()' if family == "stm32f407" else f'nx_uart_port_stop(nx_binding_{name})'))
        elif kind == "spi":
            if family == "native":
                init = 'nx_native_spi_configure(NULL, 0u)'
            elif family == "gd32f470":
                init = f'nx_gd32_spi_initialize(&s_{name}, &s_{name}_endpoint, {item.get("max_hz", 1000000)}u, 0u)'
            else:
                # A SPI controller needs an explicit reviewed CS child; do not
                # fabricate an endpoint from an unbound connector pin.
                raise ValueError('STM32 SPI selection requires a reviewed CS device binding')
        elif kind == "flash":
            if family == "gd32f470":
                init = f'nx_gd32_flash_initialize(&s_{name})'
            elif family == "stm32f407":
                geometry = 've' if result["part"] == 'STM32F407VET6' else 'zg'
                source += [f'static nx_result_t prepare_{name}(void) {{',
                           f'    s_{name}.registers = FLASH;',
                           f'    s_{name}.geometry = &g_nx_stm32_flash_{geometry};',
                           f'    s_{name}.memory = (volatile uint8_t*)0x08000000u;',
                           '    return NX_SUCCESS;', '}']
                init = f'prepare_{name}()'
            else:
                raise ValueError('Native Flash requires explicit model geometry storage')
        elif kind == "watchdog":
            if family == "gd32f470":
                init = f'nx_gd32_watchdog_initialize(&s_{name})'
            elif family == "stm32f407":
                source += [f'static nx_result_t prepare_{name}(void) {{',
                           f'    s_{name}.registers = IWDG; s_{name}.debug = DBGMCU;',
                           f'    s_{name}.rcc = RCC; s_{name}.poll_limit = 1000000u;',
                           '    return NX_SUCCESS;', '}']
                init = f'prepare_{name}()'
            else:
                init = '(nx_native_watchdog_boot(0u), NX_SUCCESS)'
        else:
            raise ValueError(f'Static construction is not maintained for {family}/{kind}')
        initialize.append(init)
    for device in result["devices"]:
        controller = next(item for item in selections if item["id"] == device["controller"])
        kind = controller["kind"]
        header.append(f'extern const nx_{kind}_endpoint_t* const nx_device_{symbol(device["id"])};')
    if family == "stm32f407":
        source.extend(stm32_bindings.shared_irq_definitions(selections))
    source += ['/** \\brief Stop only the initialized selected interfaces. */',
               'nx_result_t nx_platform_stop(void) {',
               '    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) { return NX_ERROR_CONTEXT; }']
    for index in range(len(initialize), 0, -1):
        source.append(f'    if (s_initialized >= {index}u) {{')
        for counter, (_, expression) in enumerate(entry for entry in stop if entry[0] == index):
            source += [f'        nx_result_t status_{counter} = {expression};',
                       f'        if (status_{counter} != NX_SUCCESS) {{ return status_{counter}; }}']
        source += [f'        s_initialized = {index - 1}u;', '    }']
    if family == "stm32f407":
        if gpio_clock_mask:
            source += ['    if (s_started) { RCC->AHB1ENR &= ~s_gpio_clocks_added; }']
        if any(item["kind"] == "exti" for item in selections):
            source += ['    if (s_started) { RCC->APB2ENR &= ~s_syscfg_clock_added; }']
        source.append('    nx_arch_dsb();')
        source += ['    if (s_started && nx_stm32_system_stop(&g_nx_stm32_system, 1000000u) != 0) {',
                   '        return NX_ERROR_IO;', '    }']
    elif family == "gd32f470":
        source += ['    if (s_started && nx_gd32_soc_stop() != 0) { return NX_ERROR_IO; }']
    source += ['    s_started = false;', '    s_initialized = 0u;', '    return NX_SUCCESS;', '}']
    source += ['/** \\brief Start explicit hardware only; rollback retains error facts. */',
               'nx_platform_start_result_t nx_platform_start(void) {',
               '    nx_platform_start_result_t result = { NX_SUCCESS, NX_SUCCESS, 0u };',
               '    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {',
               '        result.primary = NX_ERROR_CONTEXT; return result; }',
               '    if (s_started) { result.primary = NX_ERROR_STATE; return result; }']
    if family == "stm32f407":
        if gpio_clock_mask:
            source.append('    s_gpio_clocks_added = 0u;')
        if any(item["kind"] == "exti" for item in selections):
            source.append('    s_syscfg_clock_added = 0u;')
        source += [f'    result.primary = nx_stm32_validate_identity({result["flash"]["size"]}u);',
                   '    if (result.primary != NX_SUCCESS) { return result; }']
        source += ['    nx_stm32_start_result_t system = nx_stm32_system_start(&g_nx_stm32_system, 1000000u);',
                   '    if (system.primary != 0) {',
                   '        result.primary = NX_ERROR_IO;',
                   '        result.cleanup = system.cleanup == 0 ? NX_SUCCESS : NX_ERROR_IO;',
                   '        result.remaining_effects = system.remaining_effects;',
                   '        s_started = result.remaining_effects != 0u;',
                   '        return result;', '    }']
    elif family == "gd32f470":
        source += ['    if (nx_gd32_soc_start() != 0) {',
                   '        result.primary = NX_ERROR_IO;',
                   '        result.cleanup = nx_gd32_soc_stop() == 0 ? NX_SUCCESS : NX_ERROR_IO;',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        s_started = result.remaining_effects != 0u;',
                   '        return result;', '    }']
    source.append('    s_started = true;')
    if family == "stm32f407":
        if gpio_clock_mask:
            source.append(f'    s_gpio_clocks_added = ~RCC->AHB1ENR & {gpio_clock_mask}u;')
        if any(item["kind"] == "exti" for item in selections):
            source.append('    s_syscfg_clock_added = ~RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;')
    for index, expression in enumerate(initialize):
        source += [f'    result.primary = {expression};',
                   '    if (result.primary != NX_SUCCESS) {',
                   '        result.cleanup = nx_platform_stop();',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }', f'    s_initialized = {index + 1}u;']
    source += ['    return result;', '}']
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return '\n'.join(header), '\n'.join(source) + '\n'
