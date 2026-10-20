"""Cold assembly selects static contexts; hot I/O uses the one actual ops table."""
try:
    from ... import enhanced_endpoints, uart_formats
except ImportError:
    import enhanced_endpoints
    import uart_formats
from ..common import Construction, controller_ir, fail, kernel_policy_constant


def spi_dma_constructor(item, result, board_bindings, routes, devices):
    """One exact static owner; Platform already holds its gates and pads."""
    try:
        from ...emission import dma_regions
    except ImportError:
        from emission import dma_regions
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    children = [child for child in devices if child.controller == item.id]
    if len(children) != 1:
        fail("H743 SPI DMA requires one exact PE11 endpoint")
    child = children[0]
    binding = board_bindings[child["cs_binding"]]
    pins = routes[binding["route"]]["pins"]
    if (len(pins) != 1 or pins[0]["pin"] != "PE11" or
            binding.get("initial") != 2048 or binding.get("mask") != 2048 or
            child.get("mode", 0) != 0 or child.driver != "spi-endpoint"):
        fail("H743 SPI DMA requires mode0 and PE11 initial-high CS")
    rate = child.get("max_hz", item.options.max_hz)
    if (type(rate) is not int or not 253907 <= rate <= item.options.max_hz or
            item.options.max_hz * 256 < 65000000):
        fail("H743 SPI DMA rate is outside its hardware divider")
    memory = [region for region in result["memory"]
              if region["id"] == "axi-sram" and region["linker"]]
    region_lines, count = dma_regions(name, {"memory": memory}, engine="DMA1")
    if count != 1:
        fail("H743 SPI DMA requires initialized AXI SRAM only")
    child_name = child.id.replace("-", "_")
    cs = "s_nx_cs_" + child_name
    cs_face = "s_nx_cs_face_" + child_name
    endpoint = "s_nx_endpoint_" + child_name
    face = "s_nx_device_face_" + child_name
    definitions = region_lines + [
        f"static nx_stm32h743_gpio_state_t {cs};",
        f"static const nx_gpio_port_t {cs_face} = {{ &nx_stm32h743_gpio_ops, &{cs} }};",
        f"static nx_stm32h743_spi_dma_endpoint_t {endpoint};",
        f"static const nx_spi_endpoint_t {face} = {{ &nx_stm32h743_spi_dma_endpoint_ops, &{endpoint} }};",
        f"const nx_spi_endpoint_t* const nx_device_{child_name} = &{face};",
        f"static nx_result_t prepare_{name}(void) {{",
        f"    if ({state}.retained || {state}.initialized || {state}.active != NULL || {cs}.retained || {cs}.initialized) {{ return NX_ERROR_BUSY; }}",
        f"    {state}.wire.registers = SPI4;",
        f"    {state}.wire.reset = &RCC->APB2RSTR;",
        f"    {state}.wire.reset_mask = RCC_APB2RSTR_SPI4RST;",
        f"    {state}.wire.clock_hz = 64000000u;",
        f"    {state}.dma = DMA1;",
        f"    {state}.rx = DMA1_Stream2;",
        f"    {state}.tx = DMA1_Stream3;",
        f"    {state}.rx_mux = DMAMUX1_Channel2;",
        f"    {state}.tx_mux = DMAMUX1_Channel3;",
        f"    {state}.regions = s_nx_dma_regions_{name};",
        f"    {state}.region_count = 1u;",
        f"    {state}.face = &s_nx_face_{name};",
        f"    {cs}.registers = GPIOE;",
        f"    {cs}.mask = 2048u;",
        f"    {cs}.output = true;",
        f"    {endpoint}.wire.port = &{state}.wire;",
        f"    {endpoint}.wire.cs = &{cs};",
        f"    {endpoint}.wire.cs_mask = 2048u;",
        f"    {endpoint}.wire.frequency_hz = {rate}u;",
        f"    {endpoint}.wire.mode = 0u;",
        f"    {endpoint}.dma = &{state};",
        f"    {endpoint}.face = &{face};",
        f"    {endpoint}.cs_face = &{cs_face};",
        f"    nx_result_t status = nx_stm32h743_gpio_initialize(&{cs}, 2048u);",
        "    if (status != NX_SUCCESS) { return status; }"]
    for pin in (12, 13, 14):
        definitions += [
            f"    status = nx_stm32h743_pin_alternate(GPIOE, {pin}u, 5u);",
            "    if (status != NX_SUCCESS) { return status; }"]
    definitions += [
        f"    return nx_stm32h743_spi_dma_initialize(&{state});", "}",
        f"static nx_result_t stop_{name}(void) {{",
        f"    nx_result_t status = nx_spi_port_stop(&s_nx_face_{name});",
        "    if (status != NX_SUCCESS) { return status; }",
        f"    return nx_stm32h743_gpio_stop(&{cs}, 2048u);", "}"]
    for vector, call in (
            ("DMA1_Stream2", f"nx_stm32h743_spi_dma_irq(&{state}, true)"),
            ("DMA1_Stream3", f"nx_stm32h743_spi_dma_irq(&{state}, false)"),
            ("SPI4", f"nx_stm32h743_spi_dma_wire_irq(&{state})")):
        definitions += [f"void {vector}_IRQHandler(void);",
                        r"/** \brief Selected bounded observer; task owns final withdrawal. */",
                        f"void {vector}_IRQHandler(void) {{ {call}; }}"]
    return Construction("nx_stm32h743_spi_dma_state_t", definitions,
                        f"prepare_{name}()", [f"stop_{name}()"], True)


def constructor(item, result, board_bindings, routes, devices):
    """Construct only the exact GPIOB/byte USART3 implementation."""
    from . import mode_implementation, validate_selection
    item = controller_ir(item, "stm32h743")
    if item.provider != "stm32h743":
        fail("H743 constructor cannot reinterpret a foreign provider decision")
    validate_selection(item, item)
    implementation = mode_implementation(item.kind, item.mode)
    if implementation == "spi_dma":
        return spi_dma_constructor(item, result, board_bindings, routes, devices)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    if item.kind == "gpio":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.mask = {item.options.mask}u;",
                        f"    {state}.output = {'true' if item.mode == 'output' else 'false'};",
                        f"    return nx_stm32h743_gpio_initialize(&{state}, {item.options.initial}u);",
                        "}"]
        stop = f"nx_stm32h743_gpio_stop(&{state}, {item.options.initial}u)"
    elif item.kind == "uart":
        options = item.options
        continuous = item.mode == "rx-dma-continuous"
        receive = item.mode in ("rx-dma-idle", "rx-dma-continuous")
        dma = item.mode in ("dma-tx", "rx-dma-idle", "rx-dma-continuous")
        fifo = item.mode in uart_formats.FIFO_MODES
        flow = item.mode == uart_formats.RTSCTS_MODE
        owner = state
        if dma or fifo:
            state += ".uart"
        if fifo:
            definitions += uart_formats.fifo_definitions(item)
        plan = result.get("clock_plan")
        clock = (plan["domains"]["usart3"]["nominal_hz"] if plan is not None
                 else result["clock"]["apb1_hz"])
        divisor = (clock + options.baud // 2) // options.baud
        if not 16 <= divisor <= 0xffff:
            fail("USART baud is not representable by the selected local kernel")
        if dma:
            try:
                from ...emission import dma_regions
            except ImportError:
                from emission import dma_regions
            # Reachability does not authorize unselected, uninitialized RAM.
            allowed_memory = {"axi-sram"} if continuous else {"axi-sram", "sram1"}
            memory = [region for region in result["memory"]
                      if region["id"] in allowed_memory and region["linker"]]
            region_lines, region_count = dma_regions(name, {"memory": memory},
                                                       engine="DMA1")
            if region_count != len(memory) or not 1 <= region_count <= 2:
                fail("H743 DMA requires selected initialized AXI/SRAM1")
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
                        "    nx_result_t status = nx_stm32h743_usart3"
                        + ("_rtscts" if flow else "") + "_pins_prepare();",
                        "    if (status != NX_SUCCESS) { return status; }",
                        ]
        if fifo:
            definitions += [f"    {owner}.fifo = &s_nx_fifo_profile_{name};",
                            f"    return nx_stm32h743_{implementation}_initialize("
                            f"&{owner}, {clock}u, 1000000u);"]
        elif dma:
            lane = 1 if receive else 0
            member = "rx" if receive else "tx"
            initialize = ("uart_continuous" if continuous else
                          "uart_receive" if receive else "uart_dma")
            definitions += [f"    {owner}.dma = DMA1;",
                            f"    {owner}.{member} = DMA1_Stream{lane};",
                            f"    {owner}.mux = DMAMUX1_Channel{lane};",
                            f"    {owner}.regions = s_nx_dma_regions_{name};",
                            f"    {owner}.region_count = {region_count}u;",
                            f"    return nx_stm32h743_{initialize}_initialize(&{owner}, {clock}u, 1000000u);"]
        else:
            definitions += [f"    return nx_stm32h743_uart_initialize(&{state}, {clock}u, 1000000u);"]
        handler = ("uart_continuous_uart" if continuous else
                   "uart_receive_uart" if receive else
                   "uart_dma_uart" if dma else implementation if fifo else "uart")
        brief = ("Selected bounded FIFO RX/TX budgets." if fifo else
                 "One selected controller, at most one TX/RX byte.")
        definitions += ["}", "void USART3_IRQHandler(void);",
                        "/** \\brief " + brief + " */",
                        f"void USART3_IRQHandler(void) {{ nx_stm32h743_{handler}_irq(&{owner}); }}"]
        if dma:
            dma_handler = ("uart_continuous_dma" if continuous else
                           "uart_receive_dma" if receive else "uart_dma")
            definitions += [f"void DMA1_Stream{lane}_IRQHandler(void);",
                            r"/** \brief Selected DMA1 terminal observation. */",
                            f"void DMA1_Stream{lane}_IRQHandler(void) {{ nx_stm32h743_{dma_handler}_irq(&{owner}); }}"]
        stop = f"nx_uart_port_stop(&s_nx_face_{name})"
    elif item.kind == "spi":
        children = [child for child in devices if child["controller"] == item.id]
        if not children:
            fail("H743 SPI requires a reviewed active-low CS endpoint")
        plan = result.get("clock_plan")
        source_max = (plan["domains"]["spi4"]["maximum_hz"] if plan is not None
                      else {32000000: 32500000, 64000000: 65000000}.get(
                          result["clock"]["apb2_hz"]))
        if source_max is None:
            fail("H743 SPI requires an exact reviewed kernel profile")
        if item.options.max_hz * 256 < source_max:
            fail("H743 SPI controller rate is outside its hardware divider")
        body = [f"static nx_result_t prepare_{name}(void) {{",
                f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                f"    {state}.registers = {item.controller};",
                f"    {state}.clock_hz = {result['clock']['apb2_hz']}u;",
                f"    {state}.poll_limit = 1000000u;"]
        register = "APB2RSTR"
        body += [f"    {state}.reset = &RCC->{register};",
                 f"    {state}.reset_mask = RCC_{register}_{item.controller}RST;",
                 f"    nx_result_t status = nx_stm32h743_spi_initialize(&{state});",
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
                fail("H743 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("H743 SPI active-low CS requires an initial high latch")
            rate = child.get("max_hz", item.options.max_hz)
            if (type(rate) is not int or
                    not 250000 <= rate <= item.options.max_hz or
                    rate * 256 < source_max):
                fail("H743 SPI endpoint rate is outside its hardware divider")
            mode = child.get("mode", 0)
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("H743 SPI endpoint mode must be 0 through 3")
            definitions += [f"static nx_stm32h743_gpio_state_t {cs};",
                            f"static nx_stm32h743_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                            f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32h743_spi_endpoint_ops, &{endpoint} }};",
                            f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            body += [f"    if ({cs}.retained || {cs}.initialized) {{ return NX_ERROR_BUSY; }}",
                     f"    {cs}.registers = GPIO{pin[1]};",
                     f"    {cs}.mask = {mask}u;",
                     f"    {cs}.output = true;",
                     f"    status = nx_stm32h743_gpio_initialize(&{cs}, {mask}u);",
                     "    if (status != NX_SUCCESS) { return status; }",
                     f"    {endpoint}.cs = &{cs};",
                     f"    {endpoint}.cs_mask = {mask}u;",
                     f"    {endpoint}.frequency_hz = {rate}u;",
                     f"    {endpoint}.mode = {mode}u;"]
            stops += [f"    status = nx_stm32h743_gpio_stop(&{cs}, {mask}u);",
                      "    if (status != NX_SUCCESS) { return status; }"]
        for pin in item["pins"]:
            wire = pin["pin"]
            body += [f"    status = nx_stm32h743_pin_alternate(GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u);",
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
            fail("H743 PWM requires explicit exact Board mask and inactive latch")
        definitions += [f"static nx_stm32h743_gpio_state_t {idle};",
                        f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {idle}.retained || {idle}.initialized) {{ return NX_ERROR_BUSY; }}",
                        f"    {idle}.registers = GPIO{pin[1]};",
                        f"    {idle}.mask = {1 << int(pin[2:])}u;",
                        f"    {idle}.output = true;",
                        f"    {state}.registers = {item.controller};",
                        f"    {state}.idle = &{idle};",
                        f"    {state}.reset = &RCC->APB1LRSTR;",
                        f"    {state}.reset_mask = RCC_APB1LRSTR_{item.controller}RST;",
                        f"    {state}.pin = {int(pin[2:])}u;",
                        f"    {state}.inactive = {initial}u;",
                        f"    {state}.period_ticks = {item.options.period_ticks}u;",
                        f"    {state}.duty_ticks = {item.options.duty_ticks}u;",
                        f"    {state}.tick_hz = {item.options.tick_hz}u;",
                        f"    {state}.poll_limit = 1000000u;",
                        f"    return nx_stm32h743_pwm_initialize(&{state});", "}"]
        stop = f"nx_stm32h743_pwm_close(&{state})"
    elif item.kind == "i2c":
        timing = "0x70f22727" if result["clock"]["apb1_hz"] == 64000000 else "0x30f22727"
        if item.options.timing is not None:
            timing = f"0x{item.options.timing.timingr:08x}"
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = (nx_i2c_v2_registers_t*)(void*)I2C1;",
                        f"    {state}.reset = &RCC->APB1LRSTR;",
                        f"    {state}.reset_mask = RCC_APB1LRSTR_I2C1RST;",
                        f"    {state}.timing = {timing}u;",
                        f"    {state}.ignore_master_berr = true;",
                        f"    {state}.receive_reload_one = true;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    nx_result_t status = nx_stm32h743_i2c_pins_prepare();",
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
    elif item.kind == "adc":
        supply = board_bindings.get(item.binding, {}).get("supply_mv")
        if type(supply) is not int or not 2400 <= supply <= 3300 or not 2000 <= item.options.reference_mv <= supply:
            fail("H743 ADC requires explicit Board supply_mv2400..3300 and reference2000..supply")
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.retained || {state}.initialized || {state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = ADC1;",
                        f"    {state}.peer = ADC2;",
                        f"    {state}.common = ADC12_COMMON;",
                        f"    {state}.gpio = GPIOA;",
                        f"    {state}.reference_mv = {item.options.reference_mv}u;",
                        f"    {state}.supply_mv = {supply}u;",
                        f"    {state}.poll_limit = 1000000u;",
                        f"    return nx_stm32h743_adc_initialize(&{state});", "}"]
        stop = f"nx_stm32h743_adc_stop(&{state})"
    elif item.kind == "flash":
        supply = board_bindings.get(item.binding, {}).get("supply_mv")
        if type(supply) is not int or not 2700 <= supply <= 3600:
            fail("H743 Flash requires explicit Board supply_mv2700..3600")
        if not result["clock_profile"].endswith("-cache-off"):
            fail("H743 Flash pulse mode requires the reviewed Cache-off profile")
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.active) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = FLASH;",
                        f"    {state}.memory = (volatile uint8_t*)FLASH_BASE;",
                        f"    {state}.supply_mv = {supply}u;",
                        f"    {state}.poll_limit = 500000000u;",
                        f"    return nx_stm32h743_flash_ram_initialize(&{state}, &g_nx_stm32h743_flash_ram_workspace);", "}"]
        stop = "nx_stm32h743_flash_ram_stop(&g_nx_stm32h743_flash_ram_workspace)"
    elif item.kind == "watchdog":
        definitions += [f"static nx_result_t prepare_{name}(void) {{",
                        f"    if ({state}.state.enabled || {state}.lsi_added || {state}.debug_pending) {{ return NX_ERROR_BUSY; }}",
                        f"    {state}.registers = IWDG1;",
                        f"    {state}.rcc = RCC;",
                        f"    {state}.flash = FLASH;",
                        f"    {state}.debug = DBGMCU;",
                        f"    {state}.poll_limit = 1000000u;",
                        "    return NX_SUCCESS;", "}",
                        f"static nx_result_t stop_{name}(void) {{",
                        f"    return nx_stm32h743_watchdog_stop(&{state});",
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
                        f"    return nx_stm32h743_exti_initialize(&{state});",
                        "}", f"void EXTI{line}_IRQHandler(void);",
                        r"/** \brief One selected line, one bounded coalesced edge fact. */",
                        f"void EXTI{line}_IRQHandler(void) {{ nx_stm32h743_exti_irq(&{state}); }}"]
        stop = f"nx_exti_port_stop(&s_nx_face_{name})"
    else:
        raise ValueError("Unmaintained H743 constructor")
    return Construction(
        state_type=f"nx_stm32h743_{implementation}_state_t",
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[stop], retain_on_failed_init=True)
