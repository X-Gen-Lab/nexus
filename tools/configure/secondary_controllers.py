"""Static paired controllers with exact cold clock, pin and vector leases.

This module executes only at configuration time. Selected instances retain
their existing provider layout and readonly operations tables. It emits no
runtime registry, allocator, worker or dispatch layer.
"""
from tools.configure.gpio_phase import withdraw_pin
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, fail, mode_traits
from tools.configure.providers import hardware, maintained
from tools.configure import enhanced_endpoints


def validate_soc(soc, provider):
    """Compare independent JSON facts with the selected SDK constructor."""
    for name, expected in getattr(provider, "SECONDARY_FACTS", {}).items():
        item = soc["controllers"].get(name)
        if item is None or any(
                item.get(key) != (list(value) if isinstance(value, tuple)
                                  else value)
                for key, value in expected.items()):
            fail("Secondary controller facts differ from the maintained SDK constructor")


def applies(item, result, prefix):
    provider = maintained(result.soc_family)
    if provider.PREFIX != prefix:
        fail("Paired constructor differs from the selected provider")
    pair = getattr(provider, "CONTROLLER_PAIRS", {}).get(item.kind)
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


def _gate(item, policy):
    return policy.gate(item.controller, _binding(item).apb)


def _pins(item, result):
    integration = getattr(item, "integration", None)
    pins = [dict(pin) for pin in (integration.pins if integration is not None
                                  else item["pins"])]
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
    family = item.provider
    port = _binding(item)
    for vector, irq in port.irqs:
        method = (f"nx_{prefix}_uart_dma_uart_irq" if item.kind == "uart"
                  else "nx_i2c_v1_irq")
        result.append((vector, irq, method))
    if mode_traits(item.kind, item.mode).dma:
        for index, path in enumerate(dma_paths(item)):
            dma, stream, _ = path
            receive = (index == 1 if item.kind == "spi" else index == 0)
            arguments = ", true" if receive else ", false"
            method = (f"nx_{prefix}_uart_dma_irq" if item.kind == "uart" else
                      f"nx_{prefix}_spi_dma_irq" + arguments if item.kind == "spi" else
                      "nx_i2c_v1_dma_irq" + arguments)
            vector = hardware.dma_vector(family, path)
            result.append((vector, hardware.dma_irq(family, dma, stream), method))
    return result


def dma_paths(item):
    paths = _binding(item).dma(item.mode)
    if paths is None:
        fail("Paired controller requires its exact reviewed DMA mode")
    integration = getattr(item, "integration", None)
    if integration is not None:
        selected = tuple((lane.engine, lane.lane, lane.request)
                         for lane in integration.dma)
        expected = tuple((f"DMA{engine}", lane, request)
                         for engine, lane, request in paths)
        if (selected != expected or
                any(lane.owner != item.id or lane.controller != item.controller
                    for lane in integration.dma)):
            fail("Paired DMA integration differs from its reviewed owner")
        return tuple((int(lane.engine[3:]), lane.lane, lane.request)
                     for lane in integration.dma)
    return paths


def _binding(item):
    """Legacy constructor fixtures adapt the same explicit selected family."""
    integration = getattr(item, "integration", None)
    if integration is not None:
        if (integration.hardware is None or
                integration.controller != item.controller or
                integration.hardware.controller != item.controller):
            fail("Paired controller has no reviewed hardware integration")
        return integration.hardware
    return hardware.maintained(item.provider, item.controller)


def _clock_hz(item, result):
    integration = getattr(item, "integration", None)
    if integration is not None:
        if integration.clock_hz is None:
            fail("Paired controller has no reviewed nominal clock input")
        return integration.clock_hz
    bus = _binding(item).apb
    return result["clock"][f"apb{bus}_hz"]


def _endpoint_faces(item, result, prefix, policy):
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
                                                 operations, item.provider)
        if item.kind != "i2c":
            pins = (result["routes"][result["board_bindings"][device["cs_binding"]]["route"]]["pins"]
                    if "cs_binding" in device else [pin for pin in item["pins"] if pin["function"] == "cs"])
            if len(pins) != 1:
                fail("Paired SPI requires one explicit reviewed CS per child")
            wire = pins[0]["pin"]
            mask = 1 << int(wire[2:])
            mode = device.options.mode
            rate = device.options.max_hz
            storage, setup = policy.spi_endpoint(
                name, child, endpoint, wire, mask, mode, rate)
            declarations += storage
            assignments += setup
    return declarations, assignments, children


def constructor(item, result, prefix):
    name = _symbol(item.id)
    state = "s_nx_port_" + name
    provider = maintained(result.soc_family)
    if provider.PREFIX != prefix or item.provider != result.soc_family:
        fail("Paired constructor differs from the selected provider")
    policy, gpio = provider.SECONDARY_INTEGRATION, provider.GPIO_PHASE
    pins = _pins(item, result)
    if len(pins) > 16:
        fail("Paired controller pin lease exceeds the maintained exact bitmap")
    vectors = _vectors(item, prefix)
    apb, gate = _gate(item, policy)
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
        endpoint_lines, endpoint_assignments, children = _endpoint_faces(
            item, result, prefix, policy)
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
        for dma, stream, _ in dma_paths(item):
            control = policy.dma_control(dma, stream)
            body += [f"    if (({control} & 1u) != 0u) {{ return NX_ERROR_BUSY; }}"]
    if result.get("kernel_profile") is not None:
        body += ["    if (((SCB->AIRCR >> 8u) & 7u) > 3u) { return NX_ERROR_PERMISSION; }"]
    acquire_gate = policy.acquire_gate(item.kind)
    if not acquire_gate:
        body += [f"    if (({apb} & {gate}) != 0u) {{ return NX_ERROR_BUSY; }}"]
    body += [f"    s_nx_pair_clock_added_{name} = ~{apb} & {gate};"]
    if acquire_gate:
        body += [f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {gate});", "    nx_arch_dsb();",
                 f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}"]
    controller = item.controller
    control, active_mask = policy.control(controller, item.kind)
    if acquire_gate:
        body += [f"    if (({control} & {active_mask}u) != 0u) {{ return NX_ERROR_BUSY; }}"]
        busy = policy.busy_condition(controller, item.kind)
        if busy is not None:
            body += [f"    if ({busy}) {{ return NX_ERROR_BUSY; }}"]
    for index, (_, irq, _) in enumerate(vectors):
        body += [f"    s_nx_pair_priorities_{name}[{index}] = NVIC->IPR[{irq}];"]
    body += [f"    s_nx_pair_vectors_{name} = true;"]
    for index, pin in enumerate(pins):
        wire, function = pin["pin"], pin["function"]
        port, number = wire[1], int(wire[2:])
        _check(body, f"nx_{prefix}_gpio_clock_enable({ord(port)-ord('A')}u)")
        body += [f"    s_nx_pair_pin_saved_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{port}, {number}u);",
                 f"    s_nx_pair_pin_leases_{name} |= {1 << index}u;"]
        if function in {"cs", "tx"}:
            bop, odr = gpio.output_set(port), gpio.output_read(port)
            body += [f"    NX_PLATFORM_REGISTER_WRITE({bop}, {1 << number}u);", "    nx_arch_dsb();",
                     f"    if (({odr} & {1 << number}u) == 0u) {{ return NX_ERROR_IO; }}"]
        mode = 1 if function == "cs" else 2
        pull = 1 if function == "rx" else 0
        open_drain = "true" if item.kind == "i2c" else "false"
        _check(body, f"nx_{prefix}_pin_configure(GPIO{port}, {number}u, {mode}u, {pin['af']}u, {pull}u, {open_drain})")
    if item.kind == "uart":
        body += [f"    {state}.resources.regions = s_nx_dma_regions_{name};",
                 f"    {state}.resources.region_count = {region_count}u;"]
        setup, call = policy.setup_uart(
            item, name, state, dma_paths(item), _clock_hz(item, result))
        body += setup
        _check(body, call)
        body += [f"    s_nx_pair_constructed_{name} = true;"]
    elif item.kind == "i2c":
        if mode_traits(item.kind, item.mode).dma:
            definitions += [f"static const nx_i2c_v1_dma_t s_nx_pair_dma_{name} = {policy.I2C_DMA_PREFIX}_{controller}_DMA(s_nx_dma_regions_{name}, {region_count}u);"]
        body += [f"    {state}.registers = (volatile uint32_t*){controller};",
                 f"    {state}.line_inputs = {policy.I2C_LINE_INPUTS};",
                 f"    {state}.line_mask = {sum(1 << int(pin['pin'][2:]) for pin in item['pins'])}u;",
                 f"    {state}.peripheral_mhz = {_clock_hz(item, result) // 1000000}u;",
                 f"    {state}.dma = {'&s_nx_pair_dma_' + name if mode_traits(item.kind, item.mode).dma else 'NULL'};"]
        _check(body, f"nx_{prefix}_i2c_async_initialize(&{state}, {item.options.irq.priority}u)")
        body += [f"    s_nx_pair_constructed_{name} = true;"]
    else:
        body += [f"    {state}.regions = s_nx_dma_regions_{name};",
                 f"    {state}.region_count = {region_count}u;"]
        setup, call, after = policy.setup_spi(
            item, result, name, state, dma_paths(item), _clock_hz(item, result),
            children, endpoint_assignments)
        body += setup
        _check(body, call)
        body += [f"    s_nx_pair_constructed_{name} = true;", *after]
    # Providers have no failure return after their first MMIO effect. Preserve
    # the completed cold session before inspecting writes or publisher state.
    if item.kind == "i2c":
        body += endpoint_assignments
    body += [f"    if (({apb} & {gate}) == 0u) {{ return NX_ERROR_IO; }}"]
    for _, irq, _ in vectors:
        bank, mask = irq // 32, 1 << (irq % 32)
        body += [f"    NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[{irq}], {item.options.irq.priority << 4}u);",
                 "    nx_arch_dsb();",
                 f"    if (NVIC->IPR[{irq}] != {item.options.irq.priority << 4}u || (NVIC->ISER[{bank}] & {mask}u) == 0u) {{ return NX_ERROR_IO; }}"]
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
                 f"        NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[{irq}], s_nx_pair_priorities_{name}[{index}]);",
                 "        nx_arch_dsb();",
                 f"        if (NVIC->IPR[{irq}] != s_nx_pair_priorities_{name}[{index}]) {{ return NX_ERROR_IO; }}"]
    stop += [f"        s_nx_pair_vectors_{name} = false;", "    }"]
    # All active-low CS outputs observe inactive drive before any pin release.
    for index, pin in enumerate(pins):
        if pin["function"] != "cs":
            continue
        wire, bit = pin["pin"], 1 << int(pin["pin"][2:])
        bop, odr = gpio.output_set(wire[1]), gpio.output_read(wire[1])
        stop += [f"    if ((s_nx_pair_pin_leases_{name} & {1 << index}u) != 0u) {{",
                 f"        NX_PLATFORM_REGISTER_WRITE({bop}, {bit}u);", "        nx_arch_dsb();",
                 f"        if (({odr} & {bit}u) == 0u) {{ return NX_ERROR_IO; }}", "    }"]
    for index, pin in reversed(list(enumerate(pins))):
        wire, bit = pin["pin"], 1 << index
        stop += [f"    if ((s_nx_pair_pin_leases_{name} & {bit}u) != 0u) {{"]
        stop += withdraw_pin(result.soc_family, wire)
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
    policy = maintained(result.soc_family).SECONDARY_INTEGRATION
    return (["static uint32_t s_nx_pair_gpio_clocks_added;"]
            if policy.PAIR_GPIO_CLOCK is not None and _gpio_mask(result, prefix)
            else [])


def start_resources(result, prefix):
    clock = maintained(result.soc_family).SECONDARY_INTEGRATION.PAIR_GPIO_CLOCK
    mask = _gpio_mask(result, prefix)
    return ([f"    s_nx_pair_gpio_clocks_added = ~{clock} & {mask}u;"]
            if clock is not None and mask else [])


def stop_system(result, prefix):
    clock = maintained(result.soc_family).SECONDARY_INTEGRATION.PAIR_GPIO_CLOCK
    if clock is None or not _gpio_mask(result, prefix):
        return []
    return ["    if (s_nx_platform_started && s_nx_pair_gpio_clocks_added != 0u) {",
            f"        NX_PLATFORM_REGISTER_WRITE({clock}, {clock} & ~s_nx_pair_gpio_clocks_added);",
            "        nx_arch_dsb();",
            f"        if (({clock} & s_nx_pair_gpio_clocks_added) != 0u) {{ return NX_ERROR_IO; }}",
            "        s_nx_pair_gpio_clocks_added = 0u;", "    }"]
