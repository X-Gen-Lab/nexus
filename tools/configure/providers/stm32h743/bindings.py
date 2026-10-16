"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
from ..common import Construction, controller_ir, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Construct only the exact GPIOB/byte USART3 implementation."""
    del board_bindings, routes, devices
    from . import validate_selection
    item = controller_ir(item, "stm32h743")
    if item.provider != "stm32h743":
        fail("H743 constructor cannot reinterpret a foreign provider decision")
    validate_selection(item, item)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = GPIOB;",
                        f"    {state}.mask = {item.options.mask}u;",
                        f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
                        f"    return nx_stm32h743_gpio_initialize(&{state}, {item.options.initial}u);",
                        "}"]
        stop = f"nx_stm32h743_gpio_stop(&{state}, {item.options.initial}u)"
    elif item.kind == "uart":
        options = item.options
        receive = item.mode == "rx-dma-idle"
        dma = item.mode in ("dma-tx", "rx-dma-idle")
        owner = state
        if dma:
            state += ".uart"
        clock = result["clock"]["apb1_hz"]
        divisor = (clock + options.baud // 2) // options.baud
        if not 16 <= divisor <= 0xffff:
            fail("USART baud is not representable by the selected APB1 clock")
        if dma:
            try:
                from ...emission import dma_regions
            except ImportError:
                from emission import dma_regions
            # Reachability does not authorize unselected, uninitialized RAM.
            memory = [region for region in result["memory"]
                      if region["id"] == "axi-sram" and region["linker"]]
            region_lines, region_count = dma_regions(name, {"memory": memory},
                                                       engine="DMA1")
            if region_count != 1:
                fail("H743 DMA requires selected initialized AXI SRAM")
            definitions += region_lines
        capacity = options.rx_capacity
        if not receive and (type(capacity) is not int or not 1 <= capacity <= 4096):
            raise ValueError("H743 USART3 requires an explicit bounded byte ring")
        irq = result["irq"]
        if not receive:
            definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];"]
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = USART3;",
                        f"    {state}.rx_storage = {'NULL' if receive else 's_nx_rx_' + name};",
                        f"    {state}.rx_capacity = {0 if receive else capacity}u;",
                        f"    {state}.baud = {options.baud}u;",
                        f"    {state}.irq = USART3_IRQn;",
                        f"    {state}.profile = {'NX_UART_RX_BLOCKS' if receive else 'NX_UART_RX_BYTES'};",
                        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
                        "    nx_result_t status = nx_stm32h743_usart3_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        ]
        if dma:
            lane = 1 if receive else 0
            member = "rx" if receive else "tx"
            initialize = "uart_receive" if receive else "uart_dma"
            definitions += [f"    {owner}.dma = DMA1;",
                            f"    {owner}.{member} = DMA1_Stream{lane};",
                            f"    {owner}.mux = DMAMUX1_Channel{lane};",
                            f"    {owner}.regions = s_nx_dma_regions_{name};",
                            f"    {owner}.region_count = {region_count}u;",
                            f"    return nx_stm32h743_{initialize}_initialize(&{owner}, {clock}u, 1000000u);"]
        else:
            definitions += [f"    return nx_stm32h743_uart_initialize(&{state}, {clock}u, 1000000u);"]
        handler = ("uart_receive_uart" if receive else
                   "uart_dma_uart" if dma else "uart")
        definitions += ["}", "void USART3_IRQHandler(void);",
                        r"/** \brief One selected controller, at most one TX/RX byte. */",
                        f"void USART3_IRQHandler(void) {{ nx_stm32h743_{handler}_irq(&{owner}); }}"]
        if dma:
            dma_handler = "uart_receive_dma" if receive else "uart_dma"
            definitions += [f"void DMA1_Stream{lane}_IRQHandler(void);",
                            r"/** \brief Selected DMA1 terminal observation. */",
                            f"void DMA1_Stream{lane}_IRQHandler(void) {{ nx_stm32h743_{dma_handler}_irq(&{owner}); }}"]
        stop = f"nx_uart_port_stop(&s_nx_face_{name})"
    else:
        raise ValueError("Unmaintained H743 constructor")
    from . import mode_implementation
    implementation = mode_implementation(item.kind, item.mode)
    return Construction(
        state_type=f"nx_stm32h743_{implementation}_state_t",
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
