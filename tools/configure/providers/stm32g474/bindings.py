"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
try:
    from ... import enhanced_endpoints, timer_advanced
except ImportError:
    import enhanced_endpoints
    import timer_advanced
from ..common import Construction, controller_ir, fail, kernel_policy_constant
try:
    from ...emission import dma_regions
except ImportError:
    from emission import dma_regions


def constructor(item, result, board_bindings, routes, devices):
    """Construct exact static controller state, endpoints and bounded storage."""
    from . import validate_selection
    item = controller_ir(item, "stm32g474")
    if item.provider != "stm32g474":
        fail("G474 constructor cannot reinterpret a foreign provider decision")
    validate_selection(item, routes[item["route"]] if item.kind in {
        "pwm_adc", "can_fd"} else item)
    if item.kind in {"pwm_adc", "can_fd"}:
        from . import coupled
        return coupled.constructor(item, result, board_bindings)
    if item.kind in {"timer_capture", "encoder"}:
        return timer_advanced.g474_constructor(item)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.mask = {item.options.mask}u;",
                        f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
                        f"    return nx_stm32g474_gpio_initialize(&{state}, {item.options.initial}u);",
                        "}"]
        stop = f"nx_stm32g474_gpio_stop(&{state}, {item.options.initial}u)"
    elif item.kind == "uart":
        options = item.options
        capacity = options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            raise ValueError("G474 LPUART requires an explicit bounded byte ring")
        irq = result["irq"]
        tx, rx = item["pins"]
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.rx_storage = s_nx_rx_{name};",
                        f"    {state}.rx_capacity = {capacity}u;",
                        f"    {state}.baud = {options.baud}u;",
                        f"    {state}.irq = {item.controller}_IRQn;",
                        f"    {state}.profile = NX_UART_RX_BYTES;",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
                        ("    nx_result_t status = nx_stm32g474_lpuart1_pins_prepare();"
                         if item.controller == "LPUART1" else
                         f"    nx_result_t status = nx_stm32g474_uart_pins_prepare(GPIO{tx['pin'][1]}, {int(tx['pin'][2:])}u, GPIO{rx['pin'][1]}, {int(rx['pin'][2:])}u, {tx['af']}u);"),
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32g474_uart_initialize(&{state}, 170000000u, 1000000u);",
                        "}", f"void {item.controller}_IRQHandler(void);",
                        r"/** \brief One selected controller, at most one TX/RX byte. */",
                        f"void {item.controller}_IRQHandler(void) {{ nx_stm32g474_uart_irq(&{state}); }}"]
        stop = f"nx_uart_port_stop(&s_nx_face_{name})"
    elif item.kind == "spi":
        children = [child for child in devices if child["controller"] == item.id]
        if not children:
            fail("G474 SPI requires a reviewed active-low CS endpoint")
        body = [f"static nx_result_t prepare_{name}(void) {{",
                f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                f"    {state}.registers = {item.controller};",
                f"    {state}.clock_hz = 170000000u;",
                f"    {state}.poll_limit = 1000000u;"]
        register = "APB2RSTR" if item.controller == "SPI1" else "APB1RSTR1"
        body += [f"    {state}.reset = &RCC->{register};",
                 f"    {state}.reset_mask = RCC_{register}_{item.controller}RST;",
                 f"    nx_result_t status = nx_stm32g474_spi_initialize(&{state});",
                 "    if (status != NX_SUCCESS) { return status; }"]
        stops = [f"static nx_result_t stop_{name}(void) {{",
                 f"    nx_result_t status = nx_spi_port_stop(&s_nx_face_{name});",
                 "    if (status != NX_SUCCESS) { return status; }"]
        for child in children:
            child_name = child["id"].replace("-", "_")
            cs = f"s_nx_cs_{child_name}"
            endpoint = f"s_nx_endpoint_{child_name}"
            binding = board_bindings[child["cs_binding"]]
            pins = routes[binding["route"]]["pins"]
            if len(pins) != 1:
                fail("G474 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("G474 SPI active-low CS requires an initial high latch")
            rate = child.get("max_hz", item.options.max_hz)
            if type(rate) is not int or not 664063 <= rate <= item.options.max_hz:
                fail("G474 SPI endpoint rate is outside its hardware divider")
            mode = child.get("mode", 0)
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("G474 SPI endpoint mode must be 0 through 3")
            definitions += [f"static nx_stm32g474_gpio_state_t {cs};",
                            f"static nx_stm32g474_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                            f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32g474_spi_endpoint_ops, &{endpoint} }};",
                            f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            body += [f"    if ({cs}.retained || {cs}.initialized) {{ return NX_ERROR_BUSY; }}",
                     f"    {cs}.registers = GPIO{pin[1]};",
                     f"    {cs}.mask = {mask}u;",
                     f"    {cs}.output = true;",
                     f"    status = nx_stm32g474_gpio_initialize(&{cs}, {mask}u);",
                     "    if (status != NX_SUCCESS) { return status; }",
                     f"    {endpoint}.cs = &{cs};",
                     f"    {endpoint}.cs_mask = {mask}u;",
                     f"    {endpoint}.frequency_hz = {rate}u;",
                     f"    {endpoint}.mode = {mode}u;"]
            stops += [f"    status = nx_stm32g474_gpio_stop(&{cs}, {mask}u);",
                      "    if (status != NX_SUCCESS) { return status; }"]
        for pin in item["pins"]:
            wire = pin["pin"]
            body += [f"    status = nx_stm32g474_pin_alternate(GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u);",
                     "    if (status != NX_SUCCESS) { return status; }"]
        definitions += body + ["    return NX_SUCCESS;", "}"]
        definitions += stops + ["    return NX_SUCCESS;", "}"]
        stop = f"stop_{name}()"
    elif item.kind == "pwm_group":
        if board_bindings[item.binding].get("mask") != 1810 or board_bindings[item.binding].get("initial") != 0:
            fail("G474 coherent PWM requires explicit three-pin inactive-low Board facts")
        cfg = "s_nx_config_" + name
        comp = "s_nx_comp_" + name
        vector = "s_nx_vector_" + name
        priority = "s_nx_priority_" + name
        definitions += [
            f"static const nx_stm32g474_pwm_group_config_t {cfg} = {{",
            f"    {item.options.tick_hz}u, {item.options.period_ticks}u, {item.options.deadtime_ticks}u,",
            "    COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1, true };",
            f"static struct {{ uint32_t saved; bool retained; }} {comp};",
            f"static bool {vector};",
            f"static uint8_t {priority};",
            f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {comp}.retained || {vector}) {{ return NX_ERROR_BUSY; }}",
            "    (void)NX_G474_READ(&RCC->APB2ENR);",
            "    (void)NX_G474_READ(&RCC->APB2ENR);",
            "    if ((NX_G474_READ(&RCC->APB2RSTR) & (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) != 0u ||",
            "        (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0u || !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) { return NX_ERROR_BUSY; }",
            f"    {comp}.saved = NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE;",
            f"    {comp}.retained = true;",
            "    NX_G474_WRITE(&GPIOA->BSRR, 1792u << 16u);",
            "    nx_arch_dsb();",
            "    if ((NX_G474_READ(&GPIOA->ODR) & 1792u) != 0u) { return NX_ERROR_IO; }",
            "    NX_G474_WRITE(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~((3u << 2u) | (3u << 8u)));",
            "    NX_G474_WRITE(&GPIOA->MODER, NX_G474_READ(&GPIOA->MODER) | (3u << 2u) | (3u << 8u));",
            "    nx_arch_dsb();",
            "    if ((NX_G474_READ(&GPIOA->PUPDR) & ((3u << 2u) | (3u << 8u))) != 0u ||",
            "        (NX_G474_READ(&GPIOA->MODER) & ((3u << 2u) | (3u << 8u))) != ((3u << 2u) | (3u << 8u))) { return NX_ERROR_IO; }",
            f"    NX_G474_WRITE(&COMP1->CSR, {cfg}.comparator_csr);",
            "    nx_arch_dsb();",
            f"    if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != {cfg}.comparator_csr) {{ return NX_ERROR_IO; }}",
            "    uint64_t began = nx_stm32g474_system_now(&g_nx_stm32g474_system);",
            "    uint32_t polls = 1000000u;",
            "    while (nx_stm32g474_system_now(&g_nx_stm32g474_system) - began < 6u) {",
            "        if (polls-- == 0u) { return NX_ERROR_TIMEOUT; }",
            "        NX_G474_SYSTEM_POLL(&g_nx_stm32g474_system);",
            "    }",
            f"    nx_result_t status = nx_stm32g474_pwm_group_initialize(&{state}, &{cfg});",
            "    if (status != NX_SUCCESS) { return status; }"]
        for number in (8, 9, 10):
            definitions += [f"    status = nx_stm32g474_pin_alternate(GPIOA, {number}u, 6u);",
                            "    if (status != NX_SUCCESS) { return status; }"]
        definitions += [
            f"    {priority} = (uint8_t)NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn);",
            f"    {vector} = true;",
            "    return NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, true, 5u);", "}",
            "void TIM1_BRK_TIM15_IRQHandler(void);",
            r"/** \brief One exclusive TIM1/COMP1 latched break observation. */",
            f"void TIM1_BRK_TIM15_IRQHandler(void) {{ nx_stm32g474_pwm_group_irq(&{state}); }}",
            f"static nx_result_t stop_{name}(void) {{",
            "    nx_result_t status;",
            f"    if ({state}.initialized) {{",
            f"        status = nx_pwm_group_stop(&s_nx_face_{name});",
            "        if (status != NX_SUCCESS) { return status; }", "    }",
            f"    if ({vector}) {{",
            "        status = NX_G474_IRQ_CONTROL(TIM1_BRK_TIM15_IRQn, false, 0u);",
            "        if (status != NX_SUCCESS) { return status; }",
            "        if (!NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn)) { return NX_ERROR_BUSY; }",
            f"        NX_G474_IRQ_PRIORITY_WRITE(TIM1_BRK_TIM15_IRQn, {priority});",
            "        nx_arch_dsb(); nx_arch_isb();",
            f"        if (NX_G474_IRQ_PRIORITY(TIM1_BRK_TIM15_IRQn) != {priority}) {{ return NX_ERROR_IO; }}",
            f"        {vector} = false;", "    }",
            f"    status = nx_stm32g474_pwm_group_close(&{state});",
            "    if (status != NX_SUCCESS) { return status; }",
            f"    if ({comp}.retained) {{",
            f"        NX_G474_WRITE(&COMP1->CSR, {comp}.saved);",
            "        nx_arch_dsb();",
            f"        if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != {comp}.saved) {{ return NX_ERROR_IO; }}",
            f"        {comp}.retained = false;", "    }",
            "    return NX_SUCCESS;", "}"]
        stop = f"stop_{name}()"
    elif item.kind == "pwm":
        pin = item["pins"][0]["pin"]
        idle = "s_nx_idle_" + name
        binding = board_bindings.get(item["binding"], {})
        mask = 1 << int(pin[2:])
        initial = binding.get("initial")
        if type(initial) is not int or initial not in {0, mask} or binding.get("mask") != mask:
            fail("G474 PWM requires explicit exact Board mask and inactive latch")
        definitions += [f"static nx_stm32g474_gpio_state_t {idle};",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {idle}.retained || {idle}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {idle}.registers = GPIO{pin[1]};",
                        f"    {idle}.mask = {1 << int(pin[2:])}u;",
                        f"    {idle}.output = true;",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.idle = &{idle};",
                        f"    {state}.reset = &RCC->APB1RSTR1;",
                        f"    {state}.reset_mask = RCC_APB1RSTR1_{item.controller}RST;",
                        f"    {state}.pin = {int(pin[2:])}u;",
                        f"    {state}.inactive = {initial}u;",
                        f"    {state}.period_ticks = {item.options.period_ticks}u;",
                        f"    {state}.duty_ticks = {item.options.duty_ticks}u;",
                        f"    {state}.tick_hz = {item.options.tick_hz}u;",
                        f"    {state}.poll_limit = 1000000u;",
                        f"    return nx_stm32g474_pwm_initialize(&{state});", "}"]
        stop = f"nx_stm32g474_pwm_close(&{state})"
    elif item.kind == "i2c":
        timing = (f"0x{item.options.timing.timingr:08x}"
                  if item.options.timing is not None else "0x30420f13")
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = (nx_i2c_v2_registers_t*)(void*)I2C1;",
                        f"    {state}.reset = &RCC->APB1RSTR1;",
                        f"    {state}.reset_mask = RCC_APB1RSTR1_I2C1RST;",
                        f"    {state}.timing = {timing}u;",
                        f"    {state}.poll_limit = 1000000u;",
                        f"    {state}.ignore_master_berr = true;",
                        "    nx_result_t status = nx_stm32g474_i2c_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_i2c_v2_initialize(&{state});", "}"]
        for child in devices:
            if child["controller"] != item.id:
                continue
            child_name = child["id"].replace("-", "_")
            endpoint = "s_nx_endpoint_" + child_name
            definitions += [f"static nx_i2c_v2_endpoint_state_t {endpoint} = {{ &{state}, {child['address']}u }};",
                            *enhanced_endpoints.face(child, endpoint,
                                                     "nx_i2c_v2_endpoint_ops")]
        stop = f"nx_i2c_port_stop(&s_nx_face_{name})"
    elif item.kind == "flash":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.active || {state}.restore_pending || (FLASH->SR & FLASH_SR_BSY) != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = FLASH;",
                        f"    {state}.memory = (volatile uint8_t*)FLASH_BASE;",
                        f"    {state}.supply_mv = 3300u;",
                        "    return NX_SUCCESS;", "}"]
        stop = f"nx_stm32g474_flash_close(&{state})"
    elif item.kind == "adc":
        stream = item.mode == "trigger-dma"
        target = state + (".adc" if stream else "")
        if stream:
            lines, count = dma_regions(name, result)
            if count == 0:
                fail("G474 ADC DMA requires an explicit visible SRAM domain")
            definitions += lines
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({target}.retained || {target}.initialized || {target}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {target}.registers = ADC1;",
                        f"    {target}.peer = ADC2;",
                        f"    {target}.common = ADC12_COMMON;",
                        f"    {target}.reference_mv = {item.options.reference_mv}u;",
                        f"    {target}.poll_limit = 1000000u;",
                        "    NX_G474_WRITE(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~3u);",
                        "    NX_G474_WRITE(&GPIOA->MODER, NX_G474_READ(&GPIOA->MODER) | 3u);",
                        "    nx_arch_dsb();",
                        "    if ((NX_G474_READ(&GPIOA->MODER) & 3u) != 3u || (NX_G474_READ(&GPIOA->PUPDR) & 3u) != 0u) { return NX_ERROR_IO; }"]
        if stream:
            irq = result["irq"]
            definitions += [f"    {state}.timer = TIM6;",
                            f"    {state}.dma = DMA1;",
                            f"    {state}.channel = DMA1_Channel1;",
                            f"    {state}.mux = DMAMUX1_Channel0;",
                            f"    {state}.regions = s_nx_dma_regions_{name};",
                            f"    {state}.region_count = {count}u;",
                            f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
                            f"    return nx_stm32g474_adc_stream_initialize(&{state});", "}",
                            "void DMA1_Channel1_IRQHandler(void);",
                            r"/** \brief One selected finite ADC block terminal observation. */",
                            f"void DMA1_Channel1_IRQHandler(void) {{ nx_stm32g474_adc_stream_irq(&{state}); }}"]
            stop = f"nx_stm32g474_adc_stream_close(&{state})"
        else:
            definitions += [f"    return nx_stm32g474_adc_initialize(&{state});", "}"]
            stop = f"nx_stm32g474_adc_close(&{state})"
    elif item.kind == "watchdog":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.state.enabled || {state}.lsi_added || {state}.debug_pending) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = IWDG;",
                        f"    {state}.rcc = RCC;",
                        f"    {state}.flash = FLASH;",
                        f"    {state}.debug = DBGMCU;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    return NX_SUCCESS;", "}",
                        f"static nx_result_t stop_{name}(void) {{",
                        f"    return nx_stm32g474_watchdog_stop(&{state});",
                        "}"]
        stop = f"stop_{name}()"
    elif item.kind == "exti":
        options = item.options
        irq = result["irq"]
        line = int(item.controller[-1])
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = EXTI;",
                        f"    {state}.mux = SYSCFG;",
                        f"    {state}.gpio = GPIOA;",
                        f"    {state}.storage = s_nx_events_{name};",
                        f"    {state}.capacity = {options.event_capacity}u;",
                        f"    {state}.line = {line}u;",
                        f"    {state}.priority = {options.irq.priority}u;",
                        f"    {state}.edge = NX_EXTI_{options.edge.upper()};",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
                        f"    return nx_stm32g474_exti_initialize(&{state});",
                        "}", f"void EXTI{line}_IRQHandler(void);",
                        r"/** \brief One selected line, one bounded coalesced edge fact. */",
                        f"void EXTI{line}_IRQHandler(void) {{ nx_stm32g474_exti_irq(&{state}); }}"]
        stop = f"nx_exti_port_stop(&s_nx_face_{name})"
    else:
        raise ValueError("Unmaintained G474 constructor")
    state_type = ("nx_i2c_v2_state_t" if item.kind == "i2c" else
                  "nx_stm32g474_adc_stream_state_t" if item.kind == "adc" and
                  item.mode == "trigger-dma" else
                  f"nx_stm32g474_{item.kind}_state_t")
    return Construction(
        state_type=state_type,
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
