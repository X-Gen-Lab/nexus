"""Static paired controllers with exact cold clock, pin and vector leases.

This module executes only at configuration time. Selected instances retain
their existing provider layout and readonly operations tables. It emits no
runtime registry, allocator, worker or dispatch layer.
"""
from types import MappingProxyType

try:
    from .gpio_phase import withdraw_pin
    from .emission import dma_regions
    from .providers.common import Construction, fail, mode_traits
    from .providers import hardware
    from . import enhanced_endpoints
except ImportError:
    from gpio_phase import withdraw_pin
    from emission import dma_regions
    from providers.common import Construction, fail, mode_traits
    from providers import hardware
    import enhanced_endpoints


# Assertions for the maintained SDK cold constructors, not Board wiring.
PAIRS = {
    "stm32": {"uart": ("USART1", "USART2"), "spi": ("SPI1", "SPI2"),
              "i2c": ("I2C1", "I2C2")},
    "gd32": {"uart": ("USART0", "USART1"), "spi": ("SPI4", "SPI0"),
             "i2c": ("I2C0", "I2C1")},
}
_FAMILIES = MappingProxyType({"stm32": "stm32f407", "gd32": "gd32f470"})
DMA_FACTS = MappingProxyType({
    prefix: MappingProxyType({
        port.controller: port.dma("dma-tx" if port.kind == "uart" else
                                  "dma" if prefix == "stm32" and port.kind == "spi"
                                  else "dma-full-duplex" if port.kind == "spi"
                                  else "dma-messages")
        for port in hardware.bindings(family)
        if port.kind in {"uart", "spi", "i2c"}})
    for prefix, family in _FAMILIES.items()})


def validate_soc(soc, provider):
    prefix = provider.PREFIX
    if prefix not in PAIRS:
        return
    stm = prefix == "stm32"
    pads = {"uart": ["PA2", "PA3"],
            "spi": ["PB13", "PB14", "PB15"] if stm else ["PA5", "PA6", "PA7"],
            "i2c": ["PB10", "PB11"]}
    for kind, pair in PAIRS[prefix].items():
        name = pair[1]
        item = soc["controllers"].get(name)
        expected = {"uart": ["dma-tx"] if stm else ["irq-byte-event", "irq-blocks", "dma-tx"],
                    "spi": ["dma"] if stm else ["short-poll", "dma-full-duplex"],
                    "i2c": ["irq-messages", "dma-messages"] if stm else ["poll-messages", "irq-messages", "dma-messages"]}[kind]
        irqs = [name + "_IRQn"] if kind == "uart" else ([] if kind == "spi" else [name + "_EV", name + "_ER"])
        hz = (42000000 if stm else 50000000) if kind != "spi" else (42000000 if stm else 100000000)
        if (item is None or item["kind"] != kind or item["modes"] != expected or
                item["pins"] != pads[kind] or item["irq"] != irqs or
                item.get("clock_hz") != hz):
            fail("Secondary controller facts differ from the maintained SDK constructor")


def applies(item, result, prefix):
    pair = PAIRS.get(prefix, {}).get(item.kind)
    if pair is None or item.controller not in pair:
        return False
    if not any(other.kind == item.kind and other.controller == pair[1]
               for other in result.controllers):
        return False
    return (item.mode == "dma-tx" if item.kind == "uart" else
            mode_traits(item.kind, item.mode).dma if item.kind == "spi" else
            mode_traits(item.kind, item.mode).irq)


def selected(result, prefix):
    return [item for item in result.controllers if applies(item, result, prefix)]


def _symbol(name):
    return name.replace("-", "_")


def _check(lines, expression):
    lines += [f"    status = {expression};",
              "    if (status != NX_SUCCESS) { return status; }"]


def _gate(item, prefix):
    controller = item.controller
    apb2 = hardware.maintained(_FAMILIES[prefix], controller).apb == 2
    register = ("RCC->APB2ENR" if apb2 else "RCC->APB1ENR") if prefix == "stm32" else (
        "RCU_APB2EN" if apb2 else "RCU_APB1EN")
    bit = (f"RCC_APB{'2' if apb2 else '1'}ENR_{controller}EN" if prefix == "stm32" else
           f"RCU_APB{'2' if apb2 else '1'}EN_{controller}EN")
    return register, bit


def _pins(item, result):
    pins = [dict(pin) for pin in item["pins"]]
    if item.kind == "spi":
        for device in result.devices:
            if device["controller"] != item.id or "cs_binding" not in device:
                continue
            binding = result["board_bindings"][device["cs_binding"]]
            for pin in result["routes"][binding["route"]]["pins"]:
                pins.append({**dict(pin), "function": "cs"})
    return pins


def _vectors(item, prefix):
    result = []
    controller = item.controller
    family = _FAMILIES[prefix]
    port = hardware.maintained(family, controller)
    for vector, irq in port.irqs:
        method = (f"nx_{prefix}_uart_dma_uart_irq" if item.kind == "uart"
                  else "nx_i2c_v1_irq")
        result.append((vector, irq, method))
    if mode_traits(item.kind, item.mode).dma:
        for index, (dma, stream, _) in enumerate(DMA_FACTS[prefix][controller]):
            receive = (index == 1 if item.kind == "spi" else index == 0)
            arguments = ", true" if receive else ", false"
            method = (f"nx_{prefix}_uart_dma_irq" if item.kind == "uart" else
                      f"nx_{prefix}_spi_dma_irq" + arguments if item.kind == "spi" else
                      "nx_i2c_v1_dma_irq" + arguments)
            vector = f"DMA{dma}_{'Stream' if prefix == 'stm32' else 'Channel'}{stream}"
            result.append((vector, hardware.dma_irq(family, dma, stream), method))
    return result


def _endpoint_faces(item, result, prefix):
    name = _symbol(item.id)
    children = [device for device in result.devices if device["controller"] == item.id]
    declarations = []
    assignments = []
    for index, device in enumerate(children):
        child = _symbol(device["id"])
        endpoint = "s_nx_endpoint_" + (name if index == 0 else child)
        implementation = "spi_dma" if item.kind == "spi" else "i2c_async"
        if index:
            initializer = (f" = {{ .port = &s_nx_port_{name}, .address = {device.options.address}u }}"
                           if item.kind == "i2c" else "")
            declarations.append(f"static nx_{prefix}_{implementation}_endpoint_state_t {endpoint}{initializer};")
        operations = (f"nx_{prefix}_spi_dma_endpoint_ops" if item.kind == "spi" else
                      "nx_i2c_v1_async_endpoint_ops")
        declarations += enhanced_endpoints.face(device, endpoint,
                                                 operations, prefix)
        if item.kind != "i2c":
            pins = (result["routes"][result["board_bindings"][device["cs_binding"]]["route"]]["pins"]
                    if "cs_binding" in device else [pin for pin in item["pins"] if pin["function"] == "cs"])
            if len(pins) != 1:
                fail("Paired SPI requires one explicit reviewed CS per child")
            wire = pins[0]["pin"]
            mask = 1 << int(wire[2:])
            mode = device.options.mode
            rate = device.options.max_hz
            if prefix == "stm32":
                cs = "s_nx_cs_" + child
                declarations.append(f"static nx_stm32_gpio_state_t {cs};")
                assignments += [f"    {cs} = (nx_stm32_gpio_state_t) {{ .registers = GPIO{wire[1]}, .mask = {mask}u, .output = true, .initialized = true }};",
                                f"    {endpoint}.endpoint = (nx_stm32_spi_endpoint_state_t) {{ .port = &s_nx_port_{name}.spi, .cs = &{cs}, .cs_mask = {mask}u, .mode = {mode}u, .frequency_hz = {rate}u }};",
                                f"    {endpoint}.dma = &s_nx_port_{name};"]
            else:
                assignments += [f"    status = nx_gd32_spi_endpoint_initialize(&s_nx_port_{name}.spi, &{endpoint}.endpoint, GPIO{wire[1]}, {mask}u, {rate}u, {mode}u);",
                                "    if (status != NX_SUCCESS) { return status; }",
                                f"    {endpoint}.dma = &s_nx_port_{name};"]
    return declarations, assignments, children


def constructor(item, result, prefix):
    name = _symbol(item.id)
    state = "s_nx_port_" + name
    stm = prefix == "stm32"
    pins = _pins(item, result)
    if len(pins) > 16:
        fail("Paired controller pin lease exceeds the maintained exact bitmap")
    vectors = _vectors(item, prefix)
    apb, gate = _gate(item, prefix)
    region_lines, region_count = (dma_regions(name, result, include_flash=True)
                                  if mode_traits(item.kind, item.mode).dma else ([], 0))
    definitions = [*region_lines,
                   f"static uint32_t s_nx_pair_clock_added_{name};",
                   f"static uint16_t s_nx_pair_pin_saved_{name}[{len(pins)}];",
                   f"static uint16_t s_nx_pair_pin_leases_{name};",
                   f"static uint8_t s_nx_pair_priorities_{name}[{len(vectors)}];",
                   f"static bool s_nx_pair_vectors_{name};",
                   f"static bool s_nx_pair_constructed_{name};"]
    endpoint_assignments, children = [], []
    if item.kind != "uart":
        endpoint_lines, endpoint_assignments, children = _endpoint_faces(item, result, prefix)
        definitions += endpoint_lines
    else:
        storage = "uint8_t" if item.options.rx_profile == "bytes" else "nx_uart_rx_event_t"
        definitions += [f"static {storage} s_nx_rx_{name}[{item.options.rx_capacity}];"]
    body = [f"static nx_result_t prepare_{name}(void) {{", "    nx_result_t status;",
            f"    if (s_nx_pair_constructed_{name} || s_nx_pair_vectors_{name} || s_nx_pair_pin_leases_{name} != 0u) {{ return NX_ERROR_BUSY; }}"]
    for _, irq, _ in vectors:
        bank, mask = irq // 32, 1 << (irq % 32)
        body += [f"    if (((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | NVIC->IABR[{bank}]) & {mask}u) != 0u) {{ return NX_ERROR_BUSY; }}"]
    if mode_traits(item.kind, item.mode).dma:
        for dma, stream, _ in DMA_FACTS[prefix][item.controller]:
            control = (f"DMA{dma}_Stream{stream}->CR" if stm else
                       f"DMA_CHCTL(DMA{dma}, DMA_CH{stream})")
            body += [f"    if (({control} & 1u) != 0u) {{ return NX_ERROR_BUSY; }}"]
    if result.get("kernel_profile") is not None:
        body += ["    if (((SCB->AIRCR >> 8u) & 7u) > 3u) { return NX_ERROR_PERMISSION; }"]
    # GD UART/SPI constructors own the APB acquisition and reject a live gate.
    if not stm and item.kind in {"uart", "spi"}:
        body += [f"    if (({apb} & {gate}) != 0u) {{ return NX_ERROR_BUSY; }}"]
    body += [f"    s_nx_pair_clock_added_{name} = ~{apb} & {gate};"]
    if stm or item.kind == "i2c":
        body += [f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {gate});", "    nx_arch_dsb();",
                 f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}"]
    controller = item.controller
    control = (f"{controller}->CR1" if stm else
               f"USART_CTL0({controller})" if item.kind == "uart" else
               f"SPI_CTL0({controller})" if item.kind == "spi" else f"I2C_CTL0({controller})")
    active_mask = (8192 if stm else 8192) if item.kind == "uart" else (64 if item.kind == "spi" else 1)
    if stm or item.kind == "i2c":
        body += [f"    if (({control} & {active_mask}u) != 0u) {{ return NX_ERROR_BUSY; }}"]
        if item.kind == "spi":
            body += [f"    if (({controller}->SR & SPI_SR_BSY) != 0u) {{ return NX_ERROR_BUSY; }}"]
    for index, (_, irq, _) in enumerate(vectors):
        body += [f"    s_nx_pair_priorities_{name}[{index}] = NVIC->IP[{irq}];"]
    body += [f"    s_nx_pair_vectors_{name} = true;"]
    for index, pin in enumerate(pins):
        wire, function = pin["pin"], pin["function"]
        port, number = wire[1], int(wire[2:])
        _check(body, f"nx_{prefix}_gpio_clock_enable({ord(port)-ord('A')}u)")
        body += [f"    s_nx_pair_pin_saved_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{port}, {number}u);",
                 f"    s_nx_pair_pin_leases_{name} |= {1 << index}u;"]
        if function in {"cs", "tx"}:
            bop = f"GPIO{port}->BSRR" if stm else f"GPIO_BOP(GPIO{port})"
            odr = f"GPIO{port}->ODR" if stm else f"GPIO_OCTL(GPIO{port})"
            body += [f"    NX_PLATFORM_REGISTER_WRITE({bop}, {1 << number}u);", "    nx_arch_dsb();",
                     f"    if (({odr} & {1 << number}u) == 0u) {{ return NX_ERROR_IO; }}"]
        mode = 1 if function == "cs" else 2
        pull = 1 if function == "rx" else 0
        open_drain = "true" if item.kind == "i2c" else "false"
        _check(body, f"nx_{prefix}_pin_configure(GPIO{port}, {number}u, {mode}u, {pin['af']}u, {pull}u, {open_drain})")
    if item.kind == "uart":
        body += [f"    {state}.regions = s_nx_dma_regions_{name};",
                 f"    {state}.region_count = {region_count}u;"]
        if stm:
            dma, stream, channel = DMA_FACTS[prefix][controller][0]
            body += [f"    {state}.uart.registers = {controller};",
                     f"    {state}.uart.baud = {item.options.baud}u;",
                     f"    {state}.uart.profile = NX_UART_RX_{item.options.rx_profile.upper()};",
                     f"    {state}.uart.rx_storage = s_nx_rx_{name};",
                     f"    {state}.uart.rx_capacity = {item.options.rx_capacity}u;",
                     f"    {state}.uart.irq = {controller}_IRQn;",
                     f"    {state}.dma = DMA{dma};", f"    {state}.tx = DMA{dma}_Stream{stream};",
                     f"    {state}.dma_irq = DMA{dma}_Stream{stream}_IRQn;",
                     f"    {state}.stream = {stream}u;", f"    {state}.channel = {channel}u;"]
            call = f"nx_stm32_uart_dma_initialize(&{state}, {84000000 if controller == 'USART1' else 42000000}u)"
        else:
            call = f"nx_gd32_uart_dma_initialize_at(&{state}, &nx_gd32_{controller.lower()}_controller, {item.options.baud}u, NX_UART_RX_{item.options.rx_profile.upper()}, s_nx_rx_{name}, {item.options.rx_capacity}u, {item.options.irq.priority}u)"
        _check(body, call)
        body += [f"    s_nx_pair_constructed_{name} = true;"]
    elif item.kind == "i2c":
        if mode_traits(item.kind, item.mode).dma:
            definitions += [f"static const nx_i2c_v1_dma_t s_nx_pair_dma_{name} = NX_{'STM32' if stm else 'GD32'}_{controller}_DMA(s_nx_dma_regions_{name}, {region_count}u);"]
        body += [f"    {state}.registers = (volatile uint32_t*){controller};",
                 f"    {state}.line_inputs = {'&GPIOB->IDR' if stm else '&GPIO_ISTAT(GPIOB)'};",
                 f"    {state}.line_mask = {sum(1 << int(pin['pin'][2:]) for pin in item['pins'])}u;",
                 f"    {state}.peripheral_mhz = {42 if stm else 50}u;",
                 f"    {state}.dma = {'&s_nx_pair_dma_' + name if mode_traits(item.kind, item.mode).dma else 'NULL'};"]
        _check(body, f"nx_{prefix}_i2c_async_initialize(&{state}, {item.options.irq.priority}u)")
        body += [f"    s_nx_pair_constructed_{name} = true;"]
    else:
        body += [f"    {state}.regions = s_nx_dma_regions_{name};",
                 f"    {state}.region_count = {region_count}u;"]
        if stm:
            tx, rx = DMA_FACTS[prefix][controller]
            body += [f"    {state}.spi.registers = {controller};", f"    {state}.spi.rcc = RCC;",
                     f"    {state}.spi.clock_hz = {84000000 if controller == 'SPI1' else 42000000}u;",
                     f"    {state}.dma = DMA{tx[0]};", f"    {state}.tx = DMA{tx[0]}_Stream{tx[1]};",
                     f"    {state}.rx = DMA{rx[0]}_Stream{rx[1]};"]
            body += endpoint_assignments
            _check(body, f"nx_stm32_spi_dma_initialize(&{state})")
            body += [f"    s_nx_pair_constructed_{name} = true;"]
        else:
            device = children[0]
            wire = (result["routes"][result["board_bindings"][device["cs_binding"]]["route"]]["pins"][0]["pin"]
                    if "cs_binding" in device else next(pin["pin"] for pin in item["pins"] if pin["function"] == "cs"))
            _check(body, f"nx_gd32_spi_dma_initialize_at(&{state}, &s_nx_endpoint_{name}, &nx_gd32_{controller.lower()}_controller, GPIO{wire[1]}, {1 << int(wire[2:])}u, {device.options.max_hz}u, {device.options.mode}u, {item.options.irq.priority}u)")
            body += [f"    s_nx_pair_constructed_{name} = true;"]
            body += endpoint_assignments
    # Providers have no failure return after their first MMIO effect. Preserve
    # the completed cold session before inspecting writes or publisher state.
    if item.kind == "i2c":
        body += endpoint_assignments
    body += [f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}"]
    for _, irq, _ in vectors:
        bank, mask = irq // 32, 1 << (irq % 32)
        body += [f"    NX_PLATFORM_PRIORITY_WRITE(NVIC->IP[{irq}], {item.options.irq.priority << 4}u);",
                 "    nx_arch_dsb();",
                 f"    if (NVIC->IP[{irq}] != {item.options.irq.priority << 4}u || (NVIC->ISER[{bank}] & {mask}u) == 0u) {{ return NX_ERROR_IO; }}"]
    body += ["    return NX_SUCCESS;", "}"]
    stop = [f"static nx_result_t stop_{name}(void) {{", "    nx_result_t status;",
            f"    if (s_nx_pair_constructed_{name}) {{",
            f"        status = nx_{item.kind}_port_stop(&s_nx_face_{name});",
            "        if (status != NX_SUCCESS) { return status; }",
            f"        s_nx_pair_constructed_{name} = false;", "    }"]
    stop += [f"    if (s_nx_pair_vectors_{name}) {{"]
    for index, (_, irq, _) in enumerate(vectors):
        bank, mask = irq // 32, 1 << (irq % 32)
        stop += [f"        NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[{bank}], {mask}u);",
                 f"        NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[{bank}], {mask}u);", "        nx_arch_dsb();",
                 f"        if (((NVIC->ISER[{bank}] | NVIC->ISPR[{bank}] | NVIC->IABR[{bank}]) & {mask}u) != 0u) {{ return NX_ERROR_BUSY; }}",
                 f"        NX_PLATFORM_PRIORITY_WRITE(NVIC->IP[{irq}], s_nx_pair_priorities_{name}[{index}]);",
                 "        nx_arch_dsb();",
                 f"        if (NVIC->IP[{irq}] != s_nx_pair_priorities_{name}[{index}]) {{ return NX_ERROR_IO; }}"]
    stop += [f"        s_nx_pair_vectors_{name} = false;", "    }"]
    # All active-low CS outputs observe inactive drive before any pin release.
    for index, pin in enumerate(pins):
        if pin["function"] != "cs":
            continue
        wire, bit = pin["pin"], 1 << int(pin["pin"][2:])
        bop = f"GPIO{wire[1]}->BSRR" if stm else f"GPIO_BOP(GPIO{wire[1]})"
        odr = f"GPIO{wire[1]}->ODR" if stm else f"GPIO_OCTL(GPIO{wire[1]})"
        stop += [f"    if ((s_nx_pair_pin_leases_{name} & {1 << index}u) != 0u) {{",
                 f"        NX_PLATFORM_REGISTER_WRITE({bop}, {bit}u);", "        nx_arch_dsb();",
                 f"        if (({odr} & {bit}u) == 0u) {{ return NX_ERROR_IO; }}", "    }"]
    for index, pin in reversed(list(enumerate(pins))):
        wire, bit = pin["pin"], 1 << index
        stop += [f"    if ((s_nx_pair_pin_leases_{name} & {bit}u) != 0u) {{"]
        stop += withdraw_pin(prefix, wire)
        stop += [f"        nx_{prefix}_pin_restore(GPIO{wire[1]}, {int(wire[2:])}u, s_nx_pair_pin_saved_{name}[{index}]);",
                 f"        if (nx_{prefix}_pin_capture(GPIO{wire[1]}, {int(wire[2:])}u) != s_nx_pair_pin_saved_{name}[{index}]) {{ return NX_ERROR_IO; }}",
                 f"        s_nx_pair_pin_leases_{name} &= (uint16_t)~{bit}u;", "    }"]
    stop += [f"    if (s_nx_pair_clock_added_{name} != 0u) {{",
             f"        NX_PLATFORM_REGISTER_WRITE({apb}, {apb} & ~s_nx_pair_clock_added_{name});", "        nx_arch_dsb();",
             f"        if (({apb} & s_nx_pair_clock_added_{name}) != 0u) {{ return NX_ERROR_IO; }}",
             f"        s_nx_pair_clock_added_{name} = 0u;", "    }",
             f"    s_nx_pair_constructed_{name} = false;", "    return NX_SUCCESS;", "}"]
    definitions += stop + body
    for vector, _, method in vectors:
        callback, _, extra = method.partition(",")
        definitions += [f"void {vector}_IRQHandler(void);",
                        "/** \\brief Dispatch the exact independently owned controller. */",
                        f"void {vector}_IRQHandler(void) {{ {callback}(&{state}{',' + extra if extra else ''}); }}"]
    implementation = {"uart": "uart_dma", "spi": "spi_dma",
                      "i2c": "i2c_async"}[item.kind]
    return Construction(
        state_type=f"nx_{prefix}_{implementation}_state_t",
        definitions=definitions, initialize=f"prepare_{name}()",
        stop=[f"stop_{name}()"], retain_on_failed_init=True,
        pin_snapshots={pin["pin"]:
                       f"s_nx_pair_pin_saved_{name}[{index}]"
                       for index, pin in enumerate(pins)})


def _gpio_mask(result, prefix):
    pins = [pin for item in selected(result, prefix) for pin in _pins(item, result)]
    return sum(1 << (ord(port) - ord('A')) for port in {pin["pin"][1] for pin in pins})


def state_declarations(result, prefix):
    return ["static uint32_t s_nx_pair_gpio_clocks_added;"] if prefix == "gd32" and _gpio_mask(result, prefix) else []


def start_resources(result, prefix):
    mask = _gpio_mask(result, prefix)
    return [f"    s_nx_pair_gpio_clocks_added = ~RCU_AHB1EN & {mask}u;"] if prefix == "gd32" and mask else []


def stop_system(result, prefix):
    if prefix != "gd32" or not _gpio_mask(result, prefix):
        return []
    return ["    if (s_nx_platform_started && s_nx_pair_gpio_clocks_added != 0u) {",
            "        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN & ~s_nx_pair_gpio_clocks_added);",
            "        nx_arch_dsb();",
            "        if ((RCU_AHB1EN & s_nx_pair_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }",
            "        s_nx_pair_gpio_clocks_added = 0u;", "    }"]
