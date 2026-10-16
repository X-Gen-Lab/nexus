"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
from ..common import Construction, controller_ir, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Construct the selected exact static provider and cold resource leases."""
    from . import validate_selection
    item = controller_ir(item, "stm32u575")
    if item.provider != "stm32u575":
        fail("U575 constructor cannot reinterpret a foreign provider decision")
    validate_selection(item, item)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.mask = {item.options.mask}u;",
                        f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
                        f"    return nx_stm32u575_gpio_initialize(&{state}, {item.options.initial}u);",
                        "}"]
        stop = f"nx_stm32u575_gpio_stop(&{state}, {item.options.initial}u)"
    elif item.kind == "uart":
        options = item.options
        capacity = options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            raise ValueError("U575 USART requires an explicit bounded byte ring")
        irq = result["irq"]
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
                        f"    nx_result_t status = nx_stm32u575_{item.controller.lower()}_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32u575_uart_initialize(&{state}, 4000000u, 1000000u);",
                        "}", f"void {item.controller}_IRQHandler(void);",
                        r"/** \brief One selected controller, at most one TX/RX byte. */",
                        f"void {item.controller}_IRQHandler(void) {{ nx_stm32u575_uart_irq(&{state}); }}"]
        stop = f"nx_uart_port_stop(&s_nx_face_{name})"
    elif item.kind == "spi":
        children = [child for child in devices if child["controller"] == item.id]
        if not children:
            fail("U575 SPI requires a reviewed active-low CS endpoint")
        body = [f"static nx_result_t prepare_{name}(void) {{",
                f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                f"    {state}.registers = {item.controller};",
                f"    {state}.clock_hz = 4000000u;",
                f"    {state}.poll_limit = 1000000u;"]
        register = "APB2RSTR" if item.controller == "SPI1" else "APB1RSTR1"
        body += [f"    {state}.reset = &RCC->{register};",
                 f"    {state}.reset_mask = RCC_{register}_{item.controller}RST;",
                 f"    nx_result_t status = nx_stm32u575_spi_initialize(&{state});",
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
                fail("U575 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("U575 SPI active-low CS requires an initial high latch")
            rate = child.get("max_hz", item.options.max_hz)
            if type(rate) is not int or not 15625 <= rate <= item.options.max_hz:
                fail("U575 SPI endpoint rate is outside its hardware divider")
            mode = child.get("mode", 0)
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("U575 SPI endpoint mode must be 0 through 3")
            definitions += [f"static nx_stm32u575_gpio_state_t {cs};",
                            f"static nx_stm32u575_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                            f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32u575_spi_endpoint_ops, &{endpoint} }};",
                            f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            body += [f"    if ({cs}.retained || {cs}.initialized) {{ return NX_ERROR_BUSY; }}",
                     f"    {cs}.registers = GPIO{pin[1]};",
                     f"    {cs}.mask = {mask}u;",
                     f"    {cs}.output = true;",
                     f"    status = nx_stm32u575_gpio_initialize(&{cs}, {mask}u);",
                     "    if (status != NX_SUCCESS) { return status; }",
                     f"    {endpoint}.cs = &{cs};",
                     f"    {endpoint}.cs_mask = {mask}u;",
                     f"    {endpoint}.frequency_hz = {rate}u;",
                     f"    {endpoint}.mode = {mode}u;"]
            stops += [f"    status = nx_stm32u575_gpio_stop(&{cs}, {mask}u);",
                      "    if (status != NX_SUCCESS) { return status; }"]
        for pin in item["pins"]:
            wire = pin["pin"]
            body += [f"    status = nx_stm32u575_pin_alternate(GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u);",
                     "    if (status != NX_SUCCESS) { return status; }"]
        definitions += body + ["    return NX_SUCCESS;", "}"]
        definitions += stops + ["    return NX_SUCCESS;", "}"]
        stop = f"stop_{name}()"
    elif item.kind == "pwm":
        pin = item["pins"][0]["pin"]
        idle = "s_nx_idle_" + name
        binding = board_bindings.get(item["binding"], {})
        mask = 1 << int(pin[2:])
        initial = binding.get("initial")
        if type(initial) is not int or initial not in {0, mask} or binding.get("mask") != mask:
            fail("U575 PWM requires explicit exact Board mask and inactive latch")
        definitions += [f"static nx_stm32u575_gpio_state_t {idle};",
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
                        f"    return nx_stm32u575_pwm_initialize(&{state});", "}"]
        stop = f"nx_stm32u575_pwm_close(&{state})"
    elif item.kind == "exti":
        options = item.options
        irq = result["irq"]
        line = int(item.controller[-1])
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = EXTI;",
                        f"    {state}.mux = EXTI;",
                        f"    {state}.gpio = GPIOA;",
                        f"    {state}.storage = s_nx_events_{name};",
                        f"    {state}.capacity = {options.event_capacity}u;",
                        f"    {state}.line = {line}u;",
                        f"    {state}.priority = {options.irq.priority}u;",
                        f"    {state}.edge = NX_EXTI_{options.edge.upper()};",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
                        f"    return nx_stm32u575_exti_initialize(&{state});",
                        "}", f"void EXTI{line}_IRQHandler(void);",
                        r"/** \brief One selected line, one bounded coalesced edge fact. */",
                        f"void EXTI{line}_IRQHandler(void) {{ nx_stm32u575_exti_irq(&{state}); }}"]
        stop = f"nx_exti_port_stop(&s_nx_face_{name})"
    elif item.kind == "i2c":
        clock = f"s_nx_i2c_clock_{name}"
        definitions += [f"static nx_stm32u575_i2c_clock_t {clock};",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = (nx_i2c_v2_registers_t*)I2C1;",
                        f"    {state}.reset = &RCC->APB1RSTR1;",
                        f"    {state}.reset_mask = RCC_APB1RSTR1_I2C1RST;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    nx_result_t status = nx_stm32u575_i2c1_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    status = nx_stm32u575_i2c_clock_start(&{clock}, &{state});",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32u575_i2c_initialize(&{state});", "}",
                        f"static nx_result_t stop_{name}(void) {{",
                        f"    nx_result_t status = nx_i2c_port_stop(&s_nx_face_{name});",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32u575_i2c_clock_stop(&{clock});", "}"]
        children = [child for child in devices if child["controller"] == item.id]
        for child in children:
            child_name = child["id"].replace("-", "_")
            endpoint = f"s_nx_endpoint_{child_name}"
            address = child["address"]
            if type(address) is not int or not 8 <= address <= 119:
                fail("U575 I2C requires an explicit nonreserved7bit address")
            definitions += [f"static nx_stm32u575_i2c_endpoint_state_t {endpoint} = {{ &{state}, {address}u }};",
                            f"static const nx_i2c_endpoint_t s_nx_device_face_{child_name} = {{ &nx_i2c_v2_endpoint_ops, &{endpoint} }};",
                            f"const nx_i2c_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
        stop = f"stop_{name}()"
    elif item.kind == "adc":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = ADC4;",
                        f"    {state}.common = ADC4_COMMON;",
                        f"    {state}.reference_mv = {item.options.reference_mv}u;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    NX_U575_WRITE(&GPIOC->MODER, NX_U575_READ(&GPIOC->MODER) | 3u);",
                        "    NX_U575_WRITE(&GPIOC->PUPDR, NX_U575_READ(&GPIOC->PUPDR) & ~3u);",
                        "    nx_arch_dsb();",
                        "    if ((NX_U575_READ(&GPIOC->MODER) & 3u) != 3u || (NX_U575_READ(&GPIOC->PUPDR) & 3u) != 0u) { return NX_ERROR_IO; }",
                        f"    return nx_stm32u575_adc_initialize(&{state});", "}"]
        stop = f"nx_stm32u575_adc_close(&{state})"
    elif item.kind == "flash":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = FLASH;",
                        f"    {state}.memory = (volatile uint8_t*)0x08000000u;",
                        f"    return nx_stm32u575_flash_initialize(&{state});", "}"]
        stop = f"nx_stm32u575_flash_close(&{state})"
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
                        f"    return nx_stm32u575_watchdog_stop(&{state});",
                        "}"]
        stop = f"stop_{name}()"
    else:
        raise ValueError("Unmaintained U575 constructor")
    return Construction(
        state_type=("nx_i2c_v2_state_t" if item.kind == "i2c" else
                    f"nx_stm32u575_{item.kind}_state_t"),
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
