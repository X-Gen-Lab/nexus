"""One selected immutable face, actual shared ops and bounded private storage."""
from ..common import Construction, controller_ir, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Guard old ownership before updating any static constructor input."""
    from . import validate_selection
    item = controller_ir(item, "gd32f303")
    if item.provider != "gd32f303":
        fail("GD303 constructor cannot reinterpret a foreign provider")
    validate_selection(item, item)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.mask = {item.options.mask}u;",
            f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
            f"    return nx_gd32f303_gpio_initialize(&{state}, {item.options.initial}u);",
            "}"]
        stop = f"nx_gd32f303_gpio_stop(&{state}, {item.options.initial}u)"
    elif item.kind == "uart":
        capacity = item.options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            fail("GD303 requires an explicit bounded byte ring")
        irq = result["irq"]
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
            f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.registers = (nx_gd32f303_uart_registers_t*)(uintptr_t)USART0;",
            f"    {state}.rx_storage = s_nx_rx_{name};",
            f"    {state}.rx_capacity = {capacity}u;",
            f"    {state}.baud = {item.options.baud}u;",
            f"    {state}.irq = USART0_IRQn;",
            f"    {state}.profile = NX_UART_RX_BYTES;",
            f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
            "    nx_result_t status = nx_gd32f303_usart0_pins_prepare();",
            "    if (status != NX_SUCCESS) { return status; }",
            f"    return nx_gd32f303_uart_initialize(&{state}, 8000000u, 1000000u);",
            "}", "void USART0_IRQHandler(void);",
            r"/** \brief Exact selected USART0; at most one TX/RX byte. */",
            f"void USART0_IRQHandler(void) {{ nx_gd32f303_uart_irq(&{state}); }}"]
        stop = f"nx_uart_port_stop(&s_nx_face_{name})"
    elif item.kind == "watchdog":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.initialized || {state}.state.enabled || {state}.preparation_retained) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.poll_limit = 1000000u;",
            f"    return nx_gd32f303_watchdog_initialize(&{state});", "}"]
        stop = f"nx_gd32f303_watchdog_stop(&{state})"
    elif item.kind == "spi":
        children = [child for child in devices if child["controller"] == item.id]
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
            child_name = child["id"].replace("-", "_")
            binding = board_bindings[child["cs_binding"]]
            pins = routes[binding["route"]]["pins"]
            if tuple((pin["pin"], pin["af"]) for pin in pins) != (("PA4", 0),):
                fail("GD303 SPI fixture requires its exact PA4 CS")
            if binding.get("initial") != 16 or binding.get("mask") != 16:
                fail("GD303 SPI active-low CS must initially be high")
            rate = child.get("max_hz", item.options.max_hz)
            if type(rate) is not int or not 31250 <= rate <= item.options.max_hz:
                fail("GD303 endpoint rate differs from actual SPI divider")
            mode = child.get("mode", 0)
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
        stop = f"stop_{name}()"
    else:
        fail("Unmaintained GD303 constructor")
    return Construction(state_type=f"nx_gd32f303_{item.kind}_state_t",
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
