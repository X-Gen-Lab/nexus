"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
from tools.configure.calls import Address, DirectCall
from tools.configure.ir import ControllerIR
from tools.configure import enhanced_endpoints
from tools.configure.providers.common import Construction, fail, kernel_policy_constant
from tools.configure.memory import dma_regions
from . import timer


def constructor(item, result, board_bindings, routes, devices):
    """Construct exact static controller state, endpoints and bounded storage."""
    if not isinstance(item, ControllerIR):
        fail("stm32g474 constructor requires a resolved controller")
    if item.provider != "stm32g474":
        fail("G474 constructor cannot reinterpret a foreign provider decision")
    if item.kind == "pwm" and item.mode == "shared-fixed-pwm":
        fail("Shared PWM requires its one typed physical construction")
    if item.kind == "adc_group":
        from . import adc_group
        return adc_group.constructor(item, result, board_bindings[item.binding])
    if item.kind in {"pwm_adc", "can_fd"}:
        from . import coupled
        return coupled.constructor(item, result, board_bindings)
    if item.kind in {"timer_capture", "encoder"}:
        return timer.constructor(item)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio" and item.mode == "named-states":
        options = item.options
        plan = "s_nx_pin_plan_" + name
        config = "s_nx_config_" + name
        definitions += [f"static const nx_pin_state_plan_t {plan} = {{",
                        f"    .pins = {options.mask}u, .states = {{"]
        for value in (options.default, options.sleep):
            definitions.append("        { " + ", ".join(str(getattr(value, key)) + "u"
                for key in ("mode", "output_type", "speed", "pull", "levels")) + " },")
        definitions += ["    } };",
                        f"static const nx_stm32g474_pin_state_config_t {config} = {{",
                        f"    {item.controller}, RCC, &{plan}, RCC_AHB2ENR_GPIO{item.controller[-1]}EN }};"]
        return Construction(
            state_type="nx_stm32g474_pin_state_t", definitions=definitions,
            initialize=DirectCall("nx_stm32g474_pin_state_prepare",
                                  (Address(state), Address(config))),
            stop=[DirectCall("nx_stm32g474_pin_state_stop", (Address(state),))],
            retain_on_failed_init=True)
    if item.kind == "gpio":
        config = "s_nx_config_" + name
        definitions += [f"static const nx_stm32g474_gpio_config_t {config} = {{",
                        f"    {item.controller}, {item.options.mask}u, {item.options.initial}u, {'true' if item.mode == 'output' else 'false'} }};"]
        initialize = DirectCall("nx_stm32g474_gpio_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_stm32g474_gpio_stop", (Address(state), item.options.initial))
    elif item.kind == "uart":
        options, irq = item.options, result.irq
        capacity = options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            raise ValueError("G474 LPUART requires an explicit bounded byte ring")
        tx, rx = item.pins
        config = "s_nx_config_" + name
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
                        f"static const nx_stm32g474_uart_config_t {config} = {{",
                        f"    {item.controller}, s_nx_rx_{name}, {capacity}u, {options.baud}u, {item.controller}_IRQn,",
                        f"    {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
                        f"    GPIO{tx['pin'][1]}, GPIO{rx['pin'][1]}, {int(tx['pin'][2:])}u, {int(rx['pin'][2:])}u, {tx['af']}u }};",
                        f"void {item.controller}_IRQHandler(void);",
                        r"/** \brief One selected controller, at most one TX/RX byte. */",
                        f"void {item.controller}_IRQHandler(void) {{ nx_stm32g474_uart_irq(&{state}); }}"]
        initialize = DirectCall("nx_stm32g474_uart_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if not children:
            fail("G474 SPI requires a reviewed active-low CS endpoint")
        config = "s_nx_config_" + name
        definitions += [f"static const nx_stm32g474_alternate_t s_nx_pins_{name}[] = {{"]
        for pin in item.pins:
            wire = pin["pin"]
            definitions.append(f"    {{ GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u }},")
        definitions += ["};"]
        records = []
        for child in children:
            child_name = child.id.replace("-", "_")
            cs, endpoint = f"s_nx_cs_{child_name}", f"s_nx_endpoint_{child_name}"
            binding = board_bindings[child.options.cs_binding]
            pins = routes[binding["route"]]["pins"]
            if len(pins) != 1:
                fail("G474 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("G474 SPI active-low CS requires an initial high latch")
            rate, mode = child.options.max_hz, child.options.mode
            if type(rate) is not int or not 664063 <= rate <= item.options.max_hz:
                fail("G474 SPI endpoint rate is outside its hardware divider")
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("G474 SPI endpoint mode must be 0 through 3")
            definitions += [f"static nx_stm32g474_gpio_state_t {cs};",
                            f"static nx_stm32g474_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                            f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32g474_spi_endpoint_ops, &{endpoint} }};",
                            f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            records.append(f"    {{ &{endpoint}, &{cs}, GPIO{pin[1]}, {mask}u, {rate}u, {mode}u }},")
        register = "APB2RSTR" if item.controller == "SPI1" else "APB1RSTR1"
        definitions += [f"static const nx_stm32g474_spi_device_config_t s_nx_devices_{name}[] = {{",
                        *records, "};",
                        f"static const nx_stm32g474_spi_config_t {config} = {{",
                        f"    {item.controller}, &RCC->{register}, RCC_{register}_{item.controller}RST,",
                        f"    s_nx_pins_{name}, {len(item.pins)}u, s_nx_devices_{name}, {len(children)}u }};"]
        initialize = DirectCall("nx_stm32g474_spi_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_stm32g474_spi_close", (Address(state), Address(config)))
    elif item.kind == "pwm_group":
        if board_bindings[item.binding].get("mask") != 1810 or board_bindings[item.binding].get("initial") != 0:
            fail("G474 coherent PWM requires explicit three-pin inactive-low Board facts")
        cfg = "s_nx_config_" + name
        receipt = "s_nx_cold_" + name
        definitions += [
            f"static const nx_stm32g474_pwm_group_config_t {cfg} = {{",
            f"    {item.options.tick_hz}u, {item.options.period_ticks}u, {item.options.deadtime_ticks}u,",
            "    COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1, true };",
            f"static nx_stm32g474_control_cold_t {receipt};",
            "void TIM1_BRK_TIM15_IRQHandler(void);",
            r"/** \brief One exclusive TIM1/COMP1 latched break observation. */",
            f"void TIM1_BRK_TIM15_IRQHandler(void) {{ nx_stm32g474_pwm_group_irq(&{state}); }}"]
        initialize = DirectCall("nx_stm32g474_pwm_group_prepare",
                                (Address(state), Address(cfg), Address(receipt)))
        stop = DirectCall("nx_stm32g474_pwm_group_release",
                          (Address(state), Address(receipt)))
    elif item.kind == "pwm":
        pin = item.pins[0]["pin"]
        idle, config = "s_nx_idle_" + name, "s_nx_config_" + name
        binding = board_bindings.get(item.binding, {})
        mask = 1 << int(pin[2:])
        initial = binding.get("initial")
        if type(initial) is not int or initial not in {0, mask} or binding.get("mask") != mask:
            fail("G474 PWM requires explicit exact Board mask and inactive latch")
        definitions += [f"static nx_stm32g474_gpio_state_t {idle};",
                        f"static const nx_stm32g474_pwm_config_t {config} = {{",
                        f"    {item.controller}, &{idle}, GPIO{pin[1]}, &RCC->APB1RSTR1, RCC_APB1RSTR1_{item.controller}RST,",
                        f"    {initial}u, {item.options.period_ticks}u, {item.options.duty_ticks}u, {item.options.tick_hz}u, {int(pin[2:])}u }};"]
        initialize = DirectCall("nx_stm32g474_pwm_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_stm32g474_pwm_close", (Address(state),))
    elif item.kind == "i2c":
        timing = (f"0x{item.options.timing.timingr:08x}"
                  if item.options.timing is not None else "0x30420f13")
        config = "s_nx_config_" + name
        definitions += [f"static const nx_i2c_v2_config_t {config} = {{",
                        "    .registers = (nx_i2c_v2_registers_t*)(void*)I2C1,",
                        "    .reset = &RCC->APB1RSTR1, .reset_mask = RCC_APB1RSTR1_I2C1RST,",
                        f"    .timing = {timing}u, .poll_limit = 1000000u,",
                        "    .ignore_master_berr = true, .receive_reload_one = false",
                        "};"]
        initialize = DirectCall("nx_stm32g474_i2c_prepare",
                                (Address(state), Address(config)))
        for child in devices:
            if child.controller != item.id:
                continue
            child_name = child.id.replace("-", "_")
            endpoint = "s_nx_endpoint_" + child_name
            definitions += [f"static nx_i2c_v2_endpoint_state_t {endpoint} = {{ &{state}, {child.options.address}u }};",
                            *enhanced_endpoints.face(child, endpoint,
                                                     "nx_i2c_v2_endpoint_ops")]
        stop = DirectCall("nx_i2c_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "flash":
        initialize = DirectCall("nx_stm32g474_flash_prepare", (Address(state),))
        stop = DirectCall("nx_stm32g474_flash_close", (Address(state),))
    elif item.kind == "adc":
        stream = item.mode == "trigger-dma"
        config = "s_nx_config_" + name
        if stream:
            lines, count = dma_regions(name, result)
            if count == 0:
                fail("G474 ADC DMA requires an explicit visible SRAM domain")
            definitions += lines
        config_type = ("nx_stm32g474_adc_stream_config_t" if stream else
                       "nx_stm32g474_adc_config_t")
        definitions += [f"static const {config_type} {config} = {{"]
        if stream:
            definitions.append("    .adc = {")
        definitions += ["    .registers = ADC1, .peer = ADC2, .common = ADC12_COMMON,",
                        f"    .reference_mv = {item.options.reference_mv}u, .poll_limit = 1000000u"]
        if stream:
            irq = result.irq
            definitions += ["    }, .timer = TIM6, .dma = DMA1,",
                            "    .channel = DMA1_Channel1, .mux = DMAMUX1_Channel0,",
                            f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
                            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }}"]
        definitions.append("};")
        implementation = "adc_stream" if stream else "adc"
        initialize = DirectCall(f"nx_stm32g474_{implementation}_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall(f"nx_stm32g474_{implementation}_close", (Address(state),))
        if stream:
            definitions += ["void DMA1_Channel1_IRQHandler(void);",
                            r"/** \brief One selected finite ADC block terminal observation. */",
                            f"void DMA1_Channel1_IRQHandler(void) {{ nx_stm32g474_adc_stream_irq(&{state}); }}"]
    elif item.kind == "watchdog":
        initialize = DirectCall("nx_stm32g474_watchdog_prepare", (Address(state),))
        stop = DirectCall("nx_stm32g474_watchdog_stop", (Address(state),))
    elif item.kind == "exti":
        options, irq = item.options, result.irq
        line = int(item.controller[-1])
        config = "s_nx_config_" + name
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
                        f"static const nx_stm32g474_exti_config_t {config} = {{",
                        f"    s_nx_events_{name}, {options.event_capacity}u,",
                        f"    {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
                        f"    NX_EXTI_{options.edge.upper()}, {line}u, {options.irq.priority}u }};",
                        f"void EXTI{line}_IRQHandler(void);",
                        r"/** \brief One selected line, one bounded coalesced edge fact. */",
                        f"void EXTI{line}_IRQHandler(void) {{ nx_stm32g474_exti_irq(&{state}); }}"]
        initialize = DirectCall("nx_stm32g474_exti_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    else:
        raise ValueError("Unmaintained G474 constructor")
    state_type = ("nx_i2c_v2_state_t" if item.kind == "i2c" else
                  "nx_stm32g474_adc_stream_state_t" if item.kind == "adc" and
                  item.mode == "trigger-dma" else
                  f"nx_stm32g474_{item.kind}_state_t")
    return Construction(
        state_type=state_type,
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=True)
