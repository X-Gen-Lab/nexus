"""Cold static F103 contexts; typed factory never constructs on the I/O path."""
from .transport import PROFILE
from tools.configure.calls import Address, DirectCall
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Construct exact static states and child endpoints outside the I/O path."""
    from .contract import LIMITS
    if not isinstance(item, ControllerIR):
        fail("stm32f103 constructor requires a typed controller decision")
    if item.provider != "stm32f103":
        fail("F103 constructor cannot reinterpret a foreign provider decision")
    if item.kind == "i2c" or (item.kind == "uart" and item.mode != "irq-bytes"):
        from tools.configure.providers.legacy_transport import constructor as transport_constructor
        return transport_constructor(item, result, devices, PROFILE)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    initialize = DirectCall(f"prepare_{name}")
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.mask = {item.options.mask}u;",
                        f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
                        f"    return nx_stm32f103_gpio_initialize(&{state}, {item.options.initial}u);",
                        "}"]
        stop = DirectCall("nx_stm32f103_gpio_stop", (Address(state), item.options.initial))
    elif item.kind == "uart":
        options = item.options
        capacity = options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            raise ValueError("F103 USART requires an explicit bounded byte ring")
        irq = result.irq
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.rx_storage = s_nx_rx_{name};",
                        f"    {state}.rx_capacity = {capacity}u;",
                        f"    {state}.baud = {options.baud}u;",
                        f"    {state}.irq = {item.controller}_IRQn;",
                        f"    {state}.profile = NX_UART_RX_BYTES;",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }};",
                        "    nx_result_t status = nx_stm32f103_usart2_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32f103_uart_initialize(&{state}, 8000000u, 1000000u);",
                        "}", f"void {item.controller}_IRQHandler(void);",
                        r"/** \brief One selected controller, at most one TX/RX byte. */",
                        f"void {item.controller}_IRQHandler(void) {{ nx_stm32f103_uart_irq(&{state}); }}"]
        stop = DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if len(children) != 1:
            fail("F103 SPI1 requires one explicit PA4 CS endpoint")
        child = children[0]
        child_name = child.id.replace("-", "_")
        binding = board_bindings[child.options.cs_binding]
        cs_route = routes[binding["route"]]
        if (binding.get("mask") != 16 or binding.get("initial") != 16 or
                tuple((pin["pin"], pin["function"], pin["af"])
                      for pin in cs_route["pins"]) != (("PA4", "output", 0),)):
            fail("F103 SPI1 requires active-low PA4 CS initially high")
        minimum, maximum = LIMITS["spi_hz"]
        if (type(child.options.max_hz) is not int or
                not minimum <= child.options.max_hz <= maximum):
            fail("F103 SPI endpoint rate exceeds maintained divider range")
        cs = f"s_nx_cs_{child_name}"
        endpoint = f"s_nx_endpoint_{child_name}"
        definitions += [f"static nx_stm32f103_gpio_state_t {cs};",
                        f"static nx_stm32f103_spi_endpoint_state_t {endpoint} = {{",
                        f"    .port = &{state}, .cs = &{cs}, .cs_mask = 16u,",
                        f"    .frequency_hz = {child.options.max_hz}u, .mode = {child.options.mode}u", "};",
                        f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{",
                        f"    &nx_stm32f103_spi_endpoint_ops, &{endpoint}", "};",
                        f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active || {cs}.retained) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = SPI1; {state}.rcc = RCC;",
                        f"    {state}.clock_hz = 8000000u;",
                        f"    {cs}.registers = GPIOA; {cs}.mask = 16u; {cs}.output = true;",
                        f"    nx_result_t status = nx_stm32f103_gpio_initialize(&{cs}, 16u);",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32f103_spi_initialize(&{state});", "}",
                        f"static nx_result_t stop_{name}(void) {{",
                        f"    nx_result_t status = nx_spi_port_stop(&s_nx_face_{name});",
                        "    if (status != NX_SUCCESS) { return status; }",
                        f"    return nx_stm32f103_gpio_stop(&{cs}, 16u);", "}"]
        stop = DirectCall(f"stop_{name}")
    elif item.kind == "watchdog":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.state.enabled || {state}.preparation_retained) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = IWDG;",
                        f"    {state}.rcc = RCC;",
                        f"    {state}.flash = FLASH;",
                        f"    {state}.debug = DBGMCU;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    return NX_SUCCESS;", "}"]
        stop = DirectCall("nx_stm32f103_watchdog_stop", (Address(state),))
    elif item.kind == "exti":
        options = item.options
        irq = result.irq
        line = int(item.controller[-1])
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = EXTI;",
                        f"    {state}.mux = AFIO;",
                        f"    {state}.gpio = GPIOA;",
                        f"    {state}.storage = s_nx_events_{name};",
                        f"    {state}.capacity = {options.event_capacity}u;",
                        f"    {state}.line = {line}u;",
                        f"    {state}.priority = {options.irq.priority}u;",
                        f"    {state}.edge = NX_EXTI_{options.edge.upper()};",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }};",
                        f"    return nx_stm32f103_exti_initialize(&{state});",
                        "}", f"void EXTI{line}_IRQHandler(void);",
                        r"/** \brief One selected line, one bounded coalesced edge fact. */",
                        f"void EXTI{line}_IRQHandler(void) {{ nx_stm32f103_exti_irq(&{state}); }}"]
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind in {"adc", "pwm", "flash"}:
        if item.kind == "flash":
            supply = board_bindings[item.binding].get("supply_mv")
            if type(supply) is not int or not 2700 <= supply <= 3600:
                fail("F103 Flash requires explicit Board supply_mv2700..3600")
            initialize = DirectCall("nx_stm32f103_flash_prepare", (Address(state), supply))
        elif item.kind == "adc":
            initialize = DirectCall("nx_stm32f103_adc_prepare",
                                    (Address(state), item.options.reference_mv))
        else:
            initial = board_bindings[item.binding].get("initial")
            if type(initial) is not int or initial not in {0, 64}:
                fail("F103 PWM requires explicit PA6 inactive level")
            initialize = DirectCall("nx_stm32f103_pwm_prepare",
                (Address(state), item.options.tick_hz, item.options.period_ticks,
                 item.options.duty_ticks, initial))
        stop = DirectCall(f"nx_legacy_{item.kind}_close", (Address(state),))
    else:
        raise ValueError("Unmaintained F103 constructor")
    return Construction(
        state_type=f"nx_stm32f103_{item.kind}_state_t",
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=item.kind != "flash")
