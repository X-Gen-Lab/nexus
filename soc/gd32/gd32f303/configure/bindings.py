"""One selected immutable face, actual shared ops and bounded private storage."""
from .transport import PROFILE
from tools.configure.calls import Address, DirectCall
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Guard old ownership before updating any static constructor input."""
    if not isinstance(item, ControllerIR):
        fail("gd32f303 constructor requires a typed controller decision")
    if item.provider != "gd32f303":
        fail("GD303 constructor cannot reinterpret a foreign provider")
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
            f"    {state}.mask = {item.options.mask}u;",
            f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
            f"    return nx_gd32f303_gpio_initialize(&{state}, {item.options.initial}u);",
            "}"]
        stop = DirectCall("nx_gd32f303_gpio_stop", (Address(state), item.options.initial))
    elif item.kind == "uart":
        capacity = item.options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            fail("GD303 requires an explicit bounded byte ring")
        irq = result.irq
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
            f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.registers = (nx_gd32f303_uart_registers_t*)(uintptr_t)USART0;",
            f"    {state}.rx_storage = s_nx_rx_{name};",
            f"    {state}.rx_capacity = {capacity}u;",
            f"    {state}.baud = {item.options.baud}u;",
            f"    {state}.irq = USART0_IRQn;",
            f"    {state}.profile = NX_UART_RX_BYTES;",
            f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }};",
            "    nx_result_t status = nx_gd32f303_usart0_pins_prepare();",
            "    if (status != NX_SUCCESS) { return status; }",
            f"    return nx_gd32f303_uart_initialize(&{state}, 8000000u, 1000000u);",
            "}", "void USART0_IRQHandler(void);",
            r"/** \brief Exact selected USART0; at most one TX/RX byte. */",
            f"void USART0_IRQHandler(void) {{ nx_gd32f303_uart_irq(&{state}); }}"]
        stop = DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "watchdog":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.initialized || {state}.state.enabled || {state}.preparation_retained) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.poll_limit = 1000000u;",
            f"    return nx_gd32f303_watchdog_initialize(&{state});", "}"]
        stop = DirectCall("nx_gd32f303_watchdog_stop", (Address(state),))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if not children:
            fail("GD303 SPI0 requires an explicit reviewed CS endpoint")
        body = [f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {state}.active || {state}.held_cs != NULL) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.registers = (nx_gd32f303_spi_registers_t*)(uintptr_t)SPI0;",
            f"    {state}.clock_hz = 8000000u;",
            f"    {state}.poll_limit = 1000000u;",
            f"    nx_result_t status = nx_gd32f303_spi_initialize(&{state});",
            "    if (status != NX_SUCCESS) { return status; }"]
        stops = [f"static nx_result_t stop_{name}(void) {{",
            f"    nx_result_t status = nx_spi_port_stop(&s_nx_face_{name});",
            "    if (status != NX_SUCCESS) { return status; }"]
        for child in children:
            child_name = child.id.replace("-", "_")
            binding = board_bindings[child.options.cs_binding]
            pins = routes[binding["route"]]["pins"]
            if tuple((pin["pin"], pin["af"]) for pin in pins) != (("PA4", 0),):
                fail("GD303 SPI fixture requires its exact PA4 CS")
            if binding.get("initial") != 16 or binding.get("mask") != 16:
                fail("GD303 SPI active-low CS must initially be high")
            rate = child.options.max_hz
            if type(rate) is not int or not 31250 <= rate <= item.options.max_hz:
                fail("GD303 endpoint rate differs from actual SPI divider")
            mode = child.options.mode
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("GD303 endpoint mode must be 0..3")
            cs = f"s_nx_cs_{child_name}"
            endpoint = f"s_nx_endpoint_{child_name}"
            definitions += [f"static nx_gd32f303_gpio_state_t {cs};",
                f"static nx_gd32f303_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_gd32f303_spi_endpoint_ops, &{endpoint} }};",
                f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            body += [f"    if ({cs}.retained || {cs}.initialized) {{ return NX_ERROR_BUSY; }}",
                f"    {cs}.mask = 16u; {cs}.output = true;",
                f"    status = nx_gd32f303_gpio_initialize(&{cs}, 16u);",
                "    if (status != NX_SUCCESS) { return status; }",
                f"    {endpoint}.cs = &{cs}; {endpoint}.cs_mask = 16u;",
                f"    {endpoint}.frequency_hz = {rate}u; {endpoint}.mode = {mode}u;"]
            stops += [f"    if ({cs}.retained) {{",
                f"        status = nx_gd32f303_gpio_stop(&{cs}, 16u);",
                "        if (status != NX_SUCCESS) { return status; }", "    }"]
        definitions += body + ["    return nx_gd32f303_spi0_pins_prepare();", "}"]
        definitions += stops + ["    return NX_SUCCESS;", "}"]
        stop = DirectCall(f"stop_{name}")
    elif item.kind == "exti":
        options = item.options
        irq = result.irq
        line = int(item.controller[-1])
        definitions += [
            f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
            f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.registers = NX_GD303_EXTI;",
            f"    {state}.mux = NX_GD303_EXTI_MUX;",
            f"    {state}.gpio = NX_GD303_EXTI_GPIO;",
            f"    {state}.storage = s_nx_events_{name};",
            f"    {state}.capacity = {options.event_capacity}u;",
            f"    {state}.line = {line}u;",
            f"    {state}.priority = {options.irq.priority}u;",
            f"    {state}.edge = NX_EXTI_{options.edge.upper()};",
            f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }};",
            f"    return nx_gd32f303_exti_initialize(&{state});", "}",
            f"void EXTI{line}_IRQHandler(void);",
            r"/** \brief One exact line and one bounded coalesced fact. */",
            f"void EXTI{line}_IRQHandler(void) {{ nx_gd32f303_exti_irq(&{state}); }}"]
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind in {"adc", "pwm", "flash"}:
        if item.kind == "flash":
            supply = board_bindings[item.binding].get("supply_mv")
            if type(supply) is not int or not 2700 <= supply <= 3600:
                fail("GD303 Flash requires explicit Board supply_mv2700..3600")
            initialize = DirectCall("nx_gd32f303_flash_prepare", (Address(state), supply))
        elif item.kind == "adc":
            initialize = DirectCall("nx_gd32f303_adc_prepare",
                                    (Address(state), item.options.reference_mv))
        else:
            initial = board_bindings[item.binding].get("initial")
            if type(initial) is not int or initial not in {0, 64}:
                fail("GD303 PWM requires explicit PA6 inactive level")
            initialize = DirectCall("nx_gd32f303_pwm_prepare",
                (Address(state), item.options.tick_hz, item.options.period_ticks,
                 item.options.duty_ticks, initial))
        stop = DirectCall(f"nx_legacy_{item.kind}_close", (Address(state),))
    else:
        fail("Unmaintained GD303 constructor")
    return Construction(state_type=f"nx_gd32f303_{item.kind}_state_t",
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=item.kind != "flash")
