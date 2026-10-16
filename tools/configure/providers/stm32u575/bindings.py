"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
from ..common import Construction, controller_ir, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Construct only the exact GPIOC/byte USART implementation."""
    del board_bindings, routes, devices
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
        state_type=f"nx_stm32u575_{item.kind}_state_t",
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
