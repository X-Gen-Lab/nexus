"""Cold construction for reviewed F103/GD303 legacy transport instances."""
from .common import Construction, fail, kernel_policy_constant
try:
    from ..memory import dma_regions
except ImportError:
    from memory import dma_regions


def dma_selected(result):
    return next((item for item in result.controllers
                 if item.kind == "uart" and item.mode in
                 {"dma-tx", "dma-duplex"}), None)


def dma_state(result):
    return (["static uint32_t s_nx_transport_dma_gate_added;"]
            if dma_selected(result) else [])


def _dma_facts(family):
    return (("NX_F103", "RCC->AHBENR", "RCC_AHBENR_DMA1EN",
             "DMA1_Channel7", "DMA1_Channel6") if family == "stm32f103" else
            ("NX_GD303", "RCU_AHBEN", "RCU_AHBEN_DMA0EN",
             "DMA0_Channel3", "DMA0_Channel4"))


def dma_preflight(result, family):
    item = dma_selected(result)
    if item is None:
        return []
    hook, gate, bit, tx, rx = _dma_facts(family)
    vectors = [tx] + ([rx] if item.mode == "dma-duplex" else [])
    conditions = " || ".join(f"!{hook}_IRQ_AVAILABLE({vector}_IRQn)"
                              for vector in vectors)
    return [f"    if ({conditions}) {{ result.primary = NX_ERROR_BUSY; return result; }}"]


def dma_start(result, family, cleanup):
    if dma_selected(result) is None:
        return []
    hook, gate, bit, _, _ = _dma_facts(family)
    return [f"    s_nx_transport_dma_gate_added = ~{hook}_READ(&{gate}) & {bit};",
        f"    {hook}_WRITE(&{gate}, {hook}_READ(&{gate}) | {bit});",
        "    nx_arch_dsb();",
        f"    if (({hook}_READ(&{gate}) & {bit}) == 0u) {{",
        "        result.primary = NX_ERROR_IO;",
        f"        result.cleanup = {cleanup};",
        "        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;",
        "        return result;", "    }"]


def dma_stop(result, family):
    if dma_selected(result) is None:
        return []
    hook, gate, _, _, _ = _dma_facts(family)
    return [f"    {hook}_WRITE(&{gate}, {hook}_READ(&{gate}) & ~s_nx_transport_dma_gate_added);",
        "    nx_arch_dsb();",
        f"    if (({hook}_READ(&{gate}) & s_nx_transport_dma_gate_added) != 0u) {{ return NX_ERROR_IO; }}",
        "    s_nx_transport_dma_gate_added = 0u;"]


def constructor(item, result, devices, family):
    """Emit direct static faces; no lookup, allocation or runtime factory work."""
    f1 = family == "stm32f103"
    prefix = "nx_" + family
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    irq = result["irq"]
    policy = (f"(nx_irq_policy_t){{ {kernel_policy_constant(result)}, "
              f"{irq['external_count']}u, {irq['priority_bits']}u, "
              f"{irq['syscall_priority'] or 0}u }}")
    lines = []
    if item.kind == "uart":
        duplex = item.mode == "dma-duplex"
        owner = state
        state = owner + ".uart" if duplex else owner
        regions, count = dma_regions(name, result)
        if count != 1:
            fail("Legacy channel DMA requires one exact initialized SRAM domain")
        lines += regions
        if not duplex:
            lines += [f"static uint8_t s_nx_rx_{name}[{item.options.rx_capacity}];"]
        tx = "DMA1_Channel7" if f1 else "&DMA_CH3CTL(DMA0)"
        rx = "DMA1_Channel6" if f1 else "&DMA_CH4CTL(DMA0)"
        flags = "&DMA1->ISR" if f1 else "&DMA_INTF(DMA0)"
        clear = "&DMA1->IFCR" if f1 else "&DMA_INTC(DMA0)"
        rx_guard = (f" || {owner}.receive.stream != NULL || {owner}.receive.filling"
                    if duplex else "")
        lines += [f"static nx_result_t prepare_{name}(void) {{",
            f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {state}.wake != NULL || {state}.wake_publishers != 0u{rx_guard}) {{ return NX_ERROR_BUSY; }}",
            f"    {state}.registers = (nx_uart_v1_registers_t*)(uintptr_t){item.controller};",
            f"    {state}.tx = (nx_uart_v1_channel_t*)(uintptr_t){tx};",
            f"    {state}.flags = {flags}; {state}.clear = {clear};",
            f"    {state}.tx_shift = {24 if f1 else 12}u; {state}.rx_shift = {20 if f1 else 16}u;",
            f"    {state}.regions = s_nx_dma_regions_{name}; {state}.region_count = {count}u;"]
        if duplex:
            lines += [f"    {state}.receive = &{owner}.receive;",
                f"    {owner}.receive.channel = (nx_uart_v1_channel_t*)(uintptr_t){rx};"]
        else:
            lines += [f"    {state}.rx_storage = s_nx_rx_{name};"]
        lines += [f"    {state}.rx_capacity = {0 if duplex else item.options.rx_capacity}u;",
            f"    {state}.profile = {'NX_UART_RX_BLOCKS' if duplex else 'NX_UART_RX_BYTES'};",
            f"    {state}.baud = {item.options.baud}u;",
            f"    {state}.irq_policy = {policy};",
            f"    nx_result_t status = {prefix}_{'usart2' if f1 else 'usart0'}_pins_prepare();",
            "    if (status != NX_SUCCESS) { return status; }",
            f"    return {prefix}_{'uart_duplex' if duplex else 'uart_dma'}_initialize(&{owner});", "}"]
        vectors = [(item.controller, "nx_uart_v1_channel_dma_irq" if duplex else
                    "nx_uart_v1_channel_dma_tx_irq", "")]
        vectors += [("DMA1_Channel7" if f1 else "DMA0_Channel3",
                     "nx_uart_v1_channel_dma_channel_irq" if duplex else
                     "nx_uart_v1_channel_dma_tx_channel_irq",
                     ", false" if duplex else "")]
        if duplex:
            vectors += [("DMA1_Channel6" if f1 else "DMA0_Channel4",
                         "nx_uart_v1_channel_dma_channel_irq", ", true")]
        for vector, method, argument in vectors:
            lines += [f"void {vector}_IRQHandler(void);",
                r"/** \brief Exact selected USART/channel terminal observation. */",
                f"void {vector}_IRQHandler(void) {{ {method}(&{state}{argument}); }}"]
        return Construction(state_type=prefix + ("_uart_duplex_state_t" if duplex
                                                else "_uart_dma_state_t"),
            definitions=lines, initialize=f"prepare_{name}()",
            stop=[f"nx_uart_port_stop(&s_nx_face_{name})"],
            retain_on_failed_init=True)
    if item.kind != "i2c":
        fail("Unsupported legacy transport constructor")
    controller = "I2C1" if f1 else "I2C0"
    registers = f"(volatile uint32_t*)(uintptr_t){controller}"
    inputs = "&GPIOB->IDR" if f1 else "&GPIO_ISTAT(GPIOB)"
    for child in devices:
        if child["controller"] != item.id:
            continue
        child_name = child.id.replace("-", "_")
        lines += [f"static {prefix}_i2c_async_endpoint_state_t s_nx_endpoint_{child_name} = {{ .port = &{state}.core, .address = {child['address']}u }};",
            f"static const nx_i2c_endpoint_t s_nx_device_face_{child_name} = {{ &nx_i2c_v1_async_endpoint_ops, &s_nx_endpoint_{child_name} }};",
            f"const nx_i2c_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
    lines += [f"static nx_result_t prepare_{name}(void) {{",
        f"    if ({state}.retained || {state}.core.initialized || {state}.core.active != NULL || {state}.core.wake != NULL || {state}.core.wake_publishers != 0u) {{ return NX_ERROR_BUSY; }}",
        f"    {state}.core.registers = {registers};",
        f"    {state}.core.line_inputs = {inputs}; {state}.core.line_mask = 192u;",
        f"    {state}.core.peripheral_mhz = 8u; {state}.core.dma = NULL;",
        f"    {state}.irq_policy = {policy};",
        f"    return {prefix}_i2c_async_initialize(&{state});", "}"]
    for suffix in ("EV", "ER"):
        vector = controller + "_" + suffix
        lines += [f"void {vector}_IRQHandler(void);",
            r"/** \brief Exact selected I2C V1 bounded protocol window. */",
            f"void {vector}_IRQHandler(void) {{ nx_i2c_v1_irq(&{state}.core); }}"]
    return Construction(state_type=prefix + "_i2c_async_state_t",
        definitions=lines, initialize=f"prepare_{name}()",
        stop=[f"{prefix}_i2c_async_stop(&{state})"], retain_on_failed_init=True)
