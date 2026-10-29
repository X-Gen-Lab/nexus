"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
from tools.configure.ir import controller_ir
from tools.configure.calls import DirectCall, Address
from tools.configure import enhanced_endpoints, uart_formats
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def _gpdma_uart(item, result, state, name, capacity):
    """One selected finite-TX extension; ordinary byte state stays unchanged."""
    options, irq = item.options, result.irq
    facts = "s_nx_uart_gpdma_" + name
    chain = item.mode == "dma-chain-tx"
    provider = "uart_chain" if chain else "uart_gpdma"
    node = "s_nx_uart_chain_nodes_" + name
    config = "s_nx_uart_config_" + name
    definitions = [f"static uint8_t s_nx_rx_{name}[{capacity}];",
        *([f"_Alignas(32) static nx_stm32u575_uart_chain_node_t {node}[2];"]
          if chain else []),
        f"static const nx_stm32u575_uart_{'chain' if chain else 'gpdma'}_facts_t {facts} = {{",
        *(["    .base = {"] if chain else []),
        "    .engine = GPDMA1, .channel = GPDMA1_Channel0,",
        "    .irq = GPDMA1_Channel0_IRQn, .request = 25u,",
        f"    .priority = {options.irq.priority}u" +
        (f" }}, .nodes = {node} }};" if chain else " };"),
        f"static const nx_stm32u575_uart_gpdma_config_t {config} = {{",
        f"    .facts = &{facts}" + (".base," if chain else ","),
        f"    .rx_storage = s_nx_rx_{name}, .rx_capacity = {capacity}u,",
        f"    .baud = {options.baud}u,",
        f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
        "    .clock_hz = 4000000u, .poll_limit = 1000000u",
        "};", "void USART1_IRQHandler(void);",
        r"/** \brief One selected RX-byte and independent USART wire fact source. */",
        "void USART1_IRQHandler(void) { "
        f"nx_stm32u575_{provider}_uart_irq(&{state}); }}",
        "void GPDMA1_Channel0_IRQHandler(void);",
        r"/** \brief One exact GPDMA1 channel; no request settlement in IRQ. */",
        "void GPDMA1_Channel0_IRQHandler(void) { "
        f"nx_stm32u575_{provider}_irq(&{state}); }}"]
    return Construction(state_type="nx_stm32u575_uart_gpdma_state_t",
        definitions=definitions,
        initialize=DirectCall(f"nx_stm32u575_{provider}_prepare",
                              (Address(state), Address(config))),
        stop=[DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))],
        retain_on_failed_init=True)


def _lptim(item, result):
    """Construct the exact pinless RUN timer beside the other static faces."""
    if result.backend != "baremetal":
        fail("U575 LPTIM RUN reference requires baremetal")
    name = item.id.replace("-", "_")
    state = f"s_nx_port_{name}"
    definitions = ["void LPTIM2_IRQHandler(void);",
        r"/** \brief Exact UE observation; settlement remains task-owned. */",
        f"void LPTIM2_IRQHandler(void) {{ nx_stm32u575_lptim_irq(&{state}); }}"]
    return Construction(state_type="nx_stm32u575_lptim_state_t",
        definitions=definitions,
        initialize=DirectCall("nx_stm32u575_lptim_prepare", (Address(state),)),
        stop=[DirectCall("nx_lptim_port_stop", (Address(f"s_nx_face_{name}"),))],
        retain_on_failed_init=True)


def constructor(item, result, board_bindings, routes, devices):
    """Construct the selected exact static provider and cold resource leases."""
    from .contract import mode_implementation
    item = controller_ir(item, "stm32u575")
    if item.kind == "rtc":
        if result.backend != "baremetal":
            fail("U575 retained RTC RUN reference requires baremetal")
        name = item.id.replace("-", "_")
        state = f"s_nx_port_{name}"
        return Construction(state_type="nx_stm32u575_rtc_state_t",
            definitions=(),
            initialize=DirectCall("nx_stm32u575_rtc_prepare", (Address(state),)),
            stop=[DirectCall("nx_rtc_port_stop", (Address(f"s_nx_face_{name}"),))],
            retain_on_failed_init=False)
    if item.kind == "lptim":
        return _lptim(item, result)
    if item.provider != "stm32u575":
        fail("U575 constructor cannot reinterpret a foreign provider decision")
    implementation = mode_implementation(item.kind, item.mode)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        config = "s_nx_config_" + name
        definitions += [f"static const nx_stm32u575_gpio_config_t {config} = {{",
            f"    {item.controller}, {item.options.mask}u, {item.options.initial}u,",
            f"    {'true' if item.mode == 'output' else 'false'} }};"]
        initialize = DirectCall("nx_stm32u575_gpio_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall("nx_stm32u575_gpio_stop",
                          (Address(state), item.options.initial))
    elif item.kind == "uart":
        options, irq = item.options, result.irq
        capacity = options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            raise ValueError("U575 USART requires an explicit bounded byte ring")
        if item.mode in {"dma-tx", "dma-chain-tx"}:
            return _gpdma_uart(item, result, state, name, capacity)
        fifo = item.mode in uart_formats.FIFO_MODES
        if fifo:
            definitions += uart_formats.fifo_definitions(item)
        config = "s_nx_config_" + name
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
            f"static const nx_stm32u575_uart_config_t {config} = {{",
            f"    .storage = s_nx_rx_{name}, .capacity = {capacity}u,",
            f"    .baud = {options.baud}u, .controller = {item.controller[-1]}u,",
            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }} }};"]
        arguments = (Address(state), Address(config))
        if fifo:
            arguments += (Address("s_nx_fifo_profile_" + name),)
        initialize = DirectCall(f"nx_stm32u575_{implementation}_prepare", arguments)
        handler = implementation if fifo else "uart"
        definitions += [f"void {item.controller}_IRQHandler(void);",
            r"/** \brief Selected bounded UART observation; task owns settlement. */",
            f"void {item.controller}_IRQHandler(void) {{ nx_stm32u575_{handler}_irq(&{state}); }}"]
        stop = DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if not children:
            fail("U575 SPI requires a reviewed active-low CS endpoint")
        records = []
        for child in children:
            child_name = child.id.replace("-", "_")
            cs = f"s_nx_cs_{child_name}"
            endpoint = f"s_nx_endpoint_{child_name}"
            binding = board_bindings[child.options.cs_binding]
            pins = routes[binding["route"]]["pins"]
            if len(pins) != 1:
                fail("U575 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("U575 SPI active-low CS requires an initial high latch")
            rate, mode = child.options.max_hz, child.options.mode
            if type(rate) is not int or not 15625 <= rate <= item.options.max_hz:
                fail("U575 SPI endpoint rate is outside its hardware divider")
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("U575 SPI endpoint mode must be 0 through 3")
            definitions += [f"static nx_stm32u575_gpio_state_t {cs};",
                f"static nx_stm32u575_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32u575_spi_endpoint_ops, &{endpoint} }};",
                f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            records.append(f"    {{ &{cs}, &{endpoint}, GPIO{pin[1]}, {mask}u, {rate}u, {mode}u }},")
        config = "s_nx_config_" + name
        definitions += [f"static const nx_stm32u575_spi_endpoint_config_t s_nx_endpoints_{name}[] = {{",
            *records, "};",
            f"static const nx_stm32u575_alternate_config_t s_nx_pins_{name}[] = {{"]
        for pin in item.pins:
            wire = pin["pin"]
            definitions.append(f"    {{ GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u }},")
        definitions += ["};", f"static const nx_stm32u575_spi_config_t {config} = {{",
            f"    s_nx_endpoints_{name}, {len(children)}u, s_nx_pins_{name}, {len(item.pins)}u }};"]
        initialize = DirectCall("nx_stm32u575_spi_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall("nx_stm32u575_spi_release",
                          (Address(state), Address(config)))
    elif item.kind == "pwm":
        pin = item.pins[0]["pin"]
        idle = "s_nx_idle_" + name
        binding = board_bindings.get(item.binding, {})
        mask, initial = 1 << int(pin[2:]), binding.get("initial")
        if type(initial) is not int or initial not in {0, mask} or binding.get("mask") != mask:
            fail("U575 PWM requires explicit exact Board mask and inactive latch")
        config = "s_nx_config_" + name
        definitions += [f"static nx_stm32u575_gpio_state_t {idle};",
            f"static const nx_stm32u575_pwm_config_t {config} = {{",
            f"    &{idle}, GPIO{pin[1]}, {initial}u, {item.options.period_ticks}u,",
            f"    {item.options.duty_ticks}u, {item.options.tick_hz}u, {int(pin[2:])}u }};"]
        initialize = DirectCall("nx_stm32u575_pwm_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall("nx_stm32u575_pwm_close", (Address(state),))
    elif item.kind == "exti":
        options, irq = item.options, result.irq
        line = int(item.controller[-1])
        config = "s_nx_config_" + name
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
            f"static const nx_stm32u575_exti_config_t {config} = {{",
            f"    .storage = s_nx_events_{name}, .capacity = {options.event_capacity}u,",
            f"    .line = {line}u, .priority = {options.irq.priority}u, .edge = NX_EXTI_{options.edge.upper()},",
            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }} }};",
            f"void EXTI{line}_IRQHandler(void);",
            r"/** \brief One selected line, one bounded coalesced edge fact. */",
            f"void EXTI{line}_IRQHandler(void) {{ nx_stm32u575_exti_irq(&{state}); }}"]
        initialize = DirectCall("nx_stm32u575_exti_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "i2c":
        clock = f"s_nx_i2c_clock_{name}"
        config = f"s_nx_config_{name}"
        timing = (f"0x{item.options.timing.timingr:08x}"
                  if item.options.timing is not None else "0x10f42f2f")
        definitions += [f"static nx_stm32u575_i2c_clock_t {clock};",
                        f"static const nx_i2c_v2_config_t {config} = {{",
                        "    .registers = (nx_i2c_v2_registers_t*)I2C1,",
                        "    .reset = &RCC->APB1RSTR1, .reset_mask = RCC_APB1RSTR1_I2C1RST,",
                        f"    .timing = {timing}u, .poll_limit = 1000000u,",
                        "    .ignore_master_berr = true, .receive_reload_one = false",
                        "};"]
        initialize = DirectCall("nx_stm32u575_i2c_prepare",
                                (Address(state), Address(clock), Address(config)))
        children = [child for child in devices if child.controller == item.id]
        for child in children:
            child_name = child.id.replace("-", "_")
            endpoint = f"s_nx_endpoint_{child_name}"
            address = child.options.address
            if type(address) is not int or not 8 <= address <= 119:
                fail("U575 I2C requires an explicit nonreserved7bit address")
            definitions += [f"static nx_stm32u575_i2c_endpoint_state_t {endpoint} = {{ &{state}, {address}u }};",
                            *enhanced_endpoints.face(child, endpoint,
                                                     "nx_i2c_v2_endpoint_ops")]
        stop = DirectCall("nx_stm32u575_i2c_close",
                          (Address(state), Address(clock)))
    elif item.kind == "adc":
        config = "s_nx_config_" + name
        definitions += [f"static const nx_stm32u575_adc_config_t {config} = {{",
                        "    .registers = ADC4, .common = ADC4_COMMON,",
                        f"    .reference_mv = {item.options.reference_mv}u, .poll_limit = 1000000u",
                        "};"]
        initialize = DirectCall("nx_stm32u575_adc_prepare",
                                (Address(state), Address(config)))
        stop = DirectCall("nx_stm32u575_adc_close", (Address(state),))
    elif item.kind == "flash":
        initialize = DirectCall("nx_stm32u575_flash_prepare", (Address(state),))
        stop = DirectCall("nx_stm32u575_flash_close", (Address(state),))
    elif item.kind == "watchdog":
        initialize = DirectCall("nx_stm32u575_watchdog_prepare", (Address(state),))
        stop = DirectCall("nx_stm32u575_watchdog_stop", (Address(state),))
    else:
        raise ValueError("Unmaintained U575 constructor")
    return Construction(
        state_type=("nx_i2c_v2_state_t" if item.kind == "i2c" else
                    f"nx_stm32u575_{implementation}_state_t"),
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=True)
