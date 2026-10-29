"""Immutable H743 construction; every hardware transition lives in C."""
from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.ir import ControllerIR
from tools.configure import enhanced_endpoints, uart_formats
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def _irq(name, call, brief):
    return [f"void {name}_IRQHandler(void);", "/** \\brief " + brief + " */",
            f"void {name}_IRQHandler(void) {{ {call}; }}"]


def spi_dma_constructor(item, result, board_bindings, routes, devices):
    """One exact owner and static endpoint; C holds the cold transaction."""
    from tools.configure.memory import dma_regions_from_facts
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    children = [child for child in devices if child.controller == item.id]
    if len(children) != 1:
        fail("H743 SPI DMA requires one exact PE11 endpoint")
    child = children[0]
    binding = board_bindings[child.options.cs_binding]
    pins = routes[binding["route"]]["pins"]
    if (len(pins) != 1 or pins[0]["pin"] != "PE11" or
            binding.get("initial") != 2048 or binding.get("mask") != 2048 or
            child.options.mode != 0 or child.driver != "spi-endpoint"):
        fail("H743 SPI DMA requires mode0 and PE11 initial-high CS")
    rate = child.options.max_hz
    if (type(rate) is not int or not 253907 <= rate <= item.options.max_hz or
            item.options.max_hz * 256 < 65000000):
        fail("H743 SPI DMA rate is outside its hardware divider")
    memory = [region for region in result.memory
              if region["id"] == "axi-sram" and region["linker"]]
    definitions, count = dma_regions_from_facts(name, memory, engine="DMA1")
    if count != 1:
        fail("H743 SPI DMA requires initialized AXI SRAM only")
    child_name = child.id.replace("-", "_")
    cs = "s_nx_cs_" + child_name
    cs_face = "s_nx_cs_face_" + child_name
    endpoint = "s_nx_endpoint_" + child_name
    face = "s_nx_device_face_" + child_name
    config = "s_nx_config_" + name
    definitions += [
        f"static nx_stm32h743_gpio_state_t {cs};",
        f"static const nx_gpio_port_t {cs_face} = {{ &nx_stm32h743_gpio_ops, &{cs} }};",
        f"static nx_stm32h743_spi_dma_endpoint_t {endpoint};",
        f"static const nx_spi_endpoint_t {face} = {{ &nx_stm32h743_spi_dma_endpoint_ops, &{endpoint} }};",
        f"const nx_spi_endpoint_t* const nx_device_{child_name} = &{face};",
        f"static const nx_stm32h743_spi_dma_config_t {config} = {{",
        f"    .cs = &{cs}, .endpoint = &{endpoint}, .face = &s_nx_face_{name},",
        f"    .endpoint_face = &{face}, .cs_face = &{cs_face},",
        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
        f"    .frequency_hz = {rate}u", "};"]
    for vector, call in (
            ("DMA1_Stream2", f"nx_stm32h743_spi_dma_irq(&{state}, true)"),
            ("DMA1_Stream3", f"nx_stm32h743_spi_dma_irq(&{state}, false)"),
            ("SPI4", f"nx_stm32h743_spi_dma_wire_irq(&{state})")):
        definitions += _irq(vector, call, "Bounded observer; task owns withdrawal.")
    arguments = (Address(state), Address(config))
    return Construction("nx_stm32h743_spi_dma_state_t", definitions,
        DirectCall("nx_stm32h743_spi_dma_prepare", arguments),
        (DirectCall("nx_stm32h743_spi_dma_release", arguments),), True)


def _uart(item, result):
    from tools.configure.memory import dma_regions_from_facts
    from .contract import mode_implementation
    options = item.options
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    config = "s_nx_config_" + name
    duplex = item.mode == "dma-duplex-continuous"
    continuous = item.mode in ("rx-dma-continuous", "dma-duplex-continuous")
    receive = item.mode in ("rx-dma-idle", "rx-dma-continuous",
                           "dma-duplex-continuous")
    dma = item.mode in ("dma-tx", "rx-dma-idle", "rx-dma-continuous",
                       "dma-duplex-continuous")
    fifo = item.mode in uart_formats.FIFO_MODES
    implementation = mode_implementation(item.kind, item.mode)
    plan = result.clock_plan
    clock = (plan.domains["usart3"].nominal_hz if plan is not None
             else result.clock["apb1_hz"])
    divisor = (clock + options.baud // 2) // options.baud
    if not 16 <= divisor <= 0xffff:
        fail("USART baud is not representable by the selected local kernel")
    definitions = uart_formats.fifo_definitions(item) if fifo else []
    count = 0
    if dma:
        allowed = {"axi-sram"} if continuous else {"axi-sram", "sram1"}
        memory = [region for region in result.memory
                  if region["id"] in allowed and region["linker"]]
        regions, count = dma_regions_from_facts(name, memory, engine="DMA1")
        if count != len(memory) or not 1 <= count <= 2:
            fail("H743 DMA requires selected initialized AXI/SRAM1")
        definitions += regions
    capacity = options.rx_capacity
    if not receive:
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            fail("H743 USART3 requires an explicit bounded byte ring")
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];"]
    irq = result.irq
    definitions += [f"static const nx_stm32h743_uart_config_t {config} = {{",
        f"    .storage = {'NULL' if receive else 's_nx_rx_' + name},",
        f"    .capacity = {0 if receive else capacity}u, .baud = {options.baud}u,",
        f"    .clock_hz = {clock}u,",
        f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
        f"    .profile = {'NX_UART_RX_BLOCKS' if receive else 'NX_UART_RX_BYTES'}", "};"]
    arguments = [Address(state), Address(config)]
    if dma:
        implementation = ("uart_duplex" if duplex else
                          "uart_continuous" if continuous else
                          "uart_receive" if receive else "uart_dma")
        arguments += [Symbol("s_nx_dma_regions_" + name), count]
    elif fifo:
        arguments += [Address("s_nx_fifo_profile_" + name)]
    handler = ("uart_duplex_uart" if duplex else
               "uart_continuous_uart" if continuous else
               "uart_receive_uart" if receive else
               "uart_dma_uart" if dma else implementation)
    definitions += _irq("USART3", f"nx_stm32h743_{handler}_irq(&{state})",
                        "One selected bounded UART publisher.")
    if dma:
        lane = 1 if receive else 0
        handler = ("uart_duplex_receive" if duplex else
                   "uart_continuous_dma" if continuous else
                   "uart_receive_dma" if receive else "uart_dma")
        definitions += _irq(f"DMA1_Stream{lane}",
            f"nx_stm32h743_{handler}_irq(&{state})", "Selected DMA terminal observer.")
        if duplex:
            definitions += _irq("DMA1_Stream0",
                f"nx_stm32h743_uart_duplex_dma_irq(&{state})",
                "Independent TX DMA terminal observer.")
    return Construction(f"nx_stm32h743_{implementation}_state_t", definitions,
        DirectCall(f"nx_stm32h743_{implementation}_prepare", arguments),
        (DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),)),), True)


def constructor(item, result, board_bindings, routes, devices):
    """Select exact readonly configurations and direct typed C lifecycle calls."""
    from .contract import mode_implementation
    if not isinstance(item, ControllerIR) or item.provider != "stm32h743":
        fail("H743 constructor requires its resolved controller decision")
    implementation = mode_implementation(item.kind, item.mode)
    if implementation == "spi_dma":
        return spi_dma_constructor(item, result, board_bindings, routes, devices)
    if item.kind == "uart":
        return _uart(item, result)
    name = item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_config_" + name
    definitions = []
    arguments = (Address(state), Address(config))
    initialize = DirectCall(f"nx_stm32h743_{implementation}_prepare", arguments)
    if item.kind == "gpio":
        definitions += [f"static const nx_stm32h743_gpio_config_t {config} = {{",
            f"    {item.controller}, {item.options.mask}u, {item.options.initial}u,",
            f"    {'true' if item.mode == 'output' else 'false'}", "};"]
        stop = DirectCall("nx_stm32h743_gpio_stop", (Address(state), item.options.initial))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if not children:
            fail("H743 SPI requires a reviewed active-low CS endpoint")
        plan = result.clock_plan
        maximum = (plan.domains["spi4"].maximum_hz if plan is not None else
                   {32000000: 32500000, 64000000: 65000000}.get(result.clock["apb2_hz"]))
        if maximum is None or item.options.max_hz * 256 < maximum:
            fail("H743 SPI controller rate is outside its hardware divider")
        rows = []
        for child in children:
            child_name = child.id.replace("-", "_")
            cs, endpoint = "s_nx_cs_" + child_name, "s_nx_endpoint_" + child_name
            binding = board_bindings[child.options.cs_binding]
            pins = routes[binding["route"]]["pins"]
            if len(pins) != 1:
                fail("H743 SPI endpoint CS requires one reviewed pad")
            pin = pins[0]["pin"]
            mask = 1 << int(pin[2:])
            if binding.get("initial") != mask:
                fail("H743 SPI active-low CS requires an initial high latch")
            rate, mode = child.options.max_hz, child.options.mode
            if (type(rate) is not int or not 250000 <= rate <= item.options.max_hz or
                    rate * 256 < maximum or type(mode) is not int or not 0 <= mode <= 3):
                fail("H743 SPI endpoint rate/mode is outside its hardware divider")
            definitions += [f"static nx_stm32h743_gpio_state_t {cs};",
                f"static nx_stm32h743_spi_endpoint_state_t {endpoint} = {{ .port = &{state} }};",
                f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_stm32h743_spi_endpoint_ops, &{endpoint} }};",
                f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
            rows += [f"    {{ &{endpoint}, &{cs}, GPIO{pin[1]}, {mask}u, {rate}u, {mode}u }},"]
        definitions += [f"static const nx_stm32h743_spi_endpoint_config_t s_nx_endpoints_{name}[] = {{",
            *rows, "};", f"static const nx_stm32h743_spi_config_t {config} = {{",
            f"    .endpoints = s_nx_endpoints_{name}, .count = {len(children)}u,",
            f"    .clock_hz = {result.clock['apb2_hz']}u", "};"]
        stop = DirectCall("nx_stm32h743_spi_release", arguments)
    elif item.kind == "pwm":
        pin = item.pins[0]["pin"]
        binding = board_bindings.get(item.binding, {})
        initial, mask = binding.get("initial"), 1 << int(pin[2:])
        if type(initial) is not int or initial not in {0, mask} or binding.get("mask") != mask:
            fail("H743 PWM requires explicit exact Board mask and inactive latch")
        idle = "s_nx_idle_" + name
        definitions += [f"static nx_stm32h743_gpio_state_t {idle};",
            f"static const nx_stm32h743_pwm_config_t {config} = {{",
            f"    &{idle}, GPIO{pin[1]}, {initial}u, {item.options.period_ticks}u,",
            f"    {item.options.duty_ticks}u, {item.options.tick_hz}u, {int(pin[2:])}u", "};"]
        stop = DirectCall("nx_stm32h743_pwm_close", (Address(state),))
    elif item.kind == "i2c":
        timing = "0x70f22727" if result.clock["apb1_hz"] == 64000000 else "0x30f22727"
        if item.options.timing is not None:
            timing = f"0x{item.options.timing.timingr:08x}"
        definitions += [f"static const nx_i2c_v2_config_t {config} = {{",
            "    .registers = (nx_i2c_v2_registers_t*)(void*)I2C1,",
            "    .reset = &RCC->APB1LRSTR, .reset_mask = RCC_APB1LRSTR_I2C1RST,",
            f"    .timing = {timing}u, .poll_limit = 1000000u,",
            "    .ignore_master_berr = true, .receive_reload_one = true", "};"]
        for child in devices:
            if child.controller != item.id:
                continue
            child_name = child.id.replace("-", "_")
            endpoint = "s_nx_endpoint_" + child_name
            definitions += [f"static nx_i2c_v2_endpoint_state_t {endpoint} = {{ &{state}, {child.options.address}u }};",
                *enhanced_endpoints.face(child, endpoint, "nx_i2c_v2_endpoint_ops")]
        stop = DirectCall("nx_i2c_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "adc":
        supply = board_bindings.get(item.binding, {}).get("supply_mv")
        if type(supply) is not int or not 2400 <= supply <= 3300 or not 2000 <= item.options.reference_mv <= supply:
            fail("H743 ADC requires explicit Board supply_mv2400..3300 and reference2000..supply")
        definitions += [f"static const nx_stm32h743_adc_config_t {config} = {{",
            "    .registers = ADC1, .peer = ADC2, .common = ADC12_COMMON, .gpio = GPIOA,",
            f"    .reference_mv = {item.options.reference_mv}u, .supply_mv = {supply}u,",
            "    .poll_limit = 1000000u", "};"]
        stop = DirectCall("nx_stm32h743_adc_stop", (Address(state),))
    elif item.kind == "flash":
        supply = board_bindings.get(item.binding, {}).get("supply_mv")
        if type(supply) is not int or not 2700 <= supply <= 3600:
            fail("H743 Flash requires explicit Board supply_mv2700..3600")
        if not result.clock_profile.endswith("-cache-off"):
            fail("H743 Flash pulse mode requires the reviewed Cache-off profile")
        initialize = DirectCall("nx_stm32h743_flash_prepare",
            (Address(state), Address("g_nx_stm32h743_flash_ram_workspace"), supply))
        stop = DirectCall("nx_stm32h743_flash_ram_stop", (Address("g_nx_stm32h743_flash_ram_workspace"),))
    elif item.kind == "watchdog":
        initialize = DirectCall("nx_stm32h743_watchdog_prepare", (Address(state),))
        stop = DirectCall("nx_stm32h743_watchdog_stop", (Address(state),))
    elif item.kind == "exti":
        options, irq = item.options, result.irq
        line = int(item.controller[-1])
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
            f"static const nx_stm32h743_exti_config_t {config} = {{",
            f"    .storage = s_nx_events_{name}, .capacity = {options.event_capacity}u,",
            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
            f"    .edge = NX_EXTI_{options.edge.upper()}, .line = {line}u,",
            f"    .priority = {options.irq.priority}u", "};"]
        definitions += _irq(f"EXTI{line}", f"nx_stm32h743_exti_irq(&{state})",
                            "One selected bounded EXTI publisher.")
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    else:
        fail("Unmaintained H743 constructor")
    return Construction("nx_i2c_v2_state_t" if item.kind == "i2c" else
        f"nx_stm32h743_{implementation}_state_t", definitions, initialize,
        (stop,), True)
