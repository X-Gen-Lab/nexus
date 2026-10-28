"""Reviewed STM32 fixed construction, never a runtime resource allocator."""
from tools.configure.ir import controller_ir
from tools.configure.calls import DirectCall, Address, Symbol
from tools.configure.memory import dma_regions, flash_geometry
from tools.configure.gpio_phase import rollback_pin, withdraw_pin
from tools.configure import enhanced_endpoints, uart_formats
from tools.configure.providers.common import Construction, exact_part, mode_traits
from tools.configure.providers import hardware
from tools.configure import can, usb_device, ethernet, timer_advanced, secondary_controllers
from .contract import ADC_CHANNEL_SEQUENCES, FLASH_PROGRAM_UNIT, LIMITS, PARTS


def _symbol(value):
    return value.replace("-", "_")


def _check(lines, expression):
    lines.extend([f"    status = {expression};",
                  "    if (status != NX_SUCCESS) {",
                  "        goto rollback;", "    }"])


def _pin(lines, pin, mode, pull=0, open_drain=False):
    name = pin["pin"]
    port = name[1]
    number = int(name[2:])
    _check(lines, f"nx_stm32_gpio_clock_enable({ord(port) - ord('A')}u)")
    _capture(lines, name)
    _check(lines, f"nx_stm32_pin_configure(GPIO{port}, {number}u, {mode}u, "
                  f"{pin['af']}u, {pull}u, "
                  f"{'true' if open_drain else 'false'})")


def _capture(lines, name):
    lines += [f"    saved_{name} = nx_stm32_pin_capture(GPIO{name[1]}, {name[2:]}u);",
              f"    captured_{name} = true;"]


def _release(pins):
    lines = []
    for pin in pins:
        wire = pin["pin"]
        lines += withdraw_pin("stm32f407", wire, indent=4)
        lines += [f"    (void)nx_stm32_pin_configure(GPIO{wire[1]}, "
                  f"{wire[2:]}u, 0u, 0u, 0u, false);"]
    return lines


def _endpoint_face(kind, name, state, implementation=None):
    implementation = implementation or kind
    operations = ("nx_i2c_v1_async_endpoint_ops"
                  if implementation == "i2c_async" else
                  f"nx_stm32_{implementation}_endpoint_ops")
    return [f"static const nx_{kind}_endpoint_t s_nx_device_face_{name} = {{",
            f"    &{operations}, &{state}", "};",
            f"const nx_{kind}_endpoint_t* const nx_device_{name} = &s_nx_device_face_{name};"]


def _spi_constructor(item, result, board_bindings, routes, devices, *, paired=False):
    """Describe exact static connections; C owns cold hardware transitions."""
    selected = item.integration
    if selected is None or selected.clock_hz is None:
        raise ValueError("SPI requires its resolved controller integration")
    if item.controller not in {"SPI1", "SPI2"} or len(item.pins) != 3:
        raise ValueError("SPI requires its reviewed three-pad controller route")
    name = _symbol(item.id)
    dma = item.mode == "dma"
    if item.mode not in {"short-poll", "dma"} or (item.controller == "SPI2" and not paired):
        raise ValueError("SPI requires its maintained polling or paired DMA mode")
    implementation = "spi_dma" if dma else "spi"
    children = [child for child in devices if child.controller == item.id]
    if not children:
        raise ValueError("STM32 SPI requires a reviewed CS endpoint")
    config = f"s_nx_spi_config_{name}"
    state = f"s_nx_port_{name}"
    saved = f"s_nx_spi_pin_state_{name}"
    pin_records = list(item.pins)
    endpoint_records = []
    definitions = []
    cold_type = "nx_stm32_spi_dma_cold_t" if paired else "nx_stm32_spi_cold_receipt_t"
    definitions.append(f"static {cold_type} s_nx_spi_cold_{name};")
    for index, child in enumerate(children):
        child_name = _symbol(child.id)
        binding = board_bindings[child.options.cs_binding]
        cs_pins = routes[binding["route"]]["pins"]
        if len(cs_pins) != 1:
            raise ValueError("SPI CS requires exactly one reviewed pin")
        pin = cs_pins[0]
        if binding.get("initial") != 1 << int(pin["pin"][2:]):
            raise ValueError("SPI active-low CS must declare initial high")
        pin_records.append(pin)
        endpoint = (f"s_nx_endpoint_{name}" if index == 0 else
                    f"s_nx_endpoint_{child_name}")
        if index:
            definitions.append(
                f"static nx_stm32_{implementation}_endpoint_state_t {endpoint};")
        chip_select = f"s_nx_cs_{child_name}"
        definitions.append(f"static nx_stm32_gpio_state_t {chip_select};")
        definitions += enhanced_endpoints.face(
            child, endpoint, f"nx_stm32_{implementation}_endpoint_ops",
            "stm32f407")
        base = endpoint + (".endpoint" if dma else "")
        endpoint_records.append(
            f"    {{ &{base}, &{chip_select}, {child.options.max_hz}u, "
            f"{child.options.mode}u }}")
    definitions += [f"static uint16_t {saved}[{len(pin_records)}];",
                    f"static const nx_stm32_spi_pin_config_t s_nx_spi_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(pin_records):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(
            f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, "
            f"{int(wire[2:])}u, {pin['af']}u }},")
    definitions += ["};",
                    f"static const nx_stm32_spi_endpoint_config_t s_nx_spi_endpoints_{name}[] = {{",
                    ",\n".join(endpoint_records), "};"]
    if dma:
        if len(selected.dma) != 2 or selected.irq is None:
            raise ValueError("SPI DMA requires two selected lanes and IRQ policy")
        secondary_controllers.dma_paths(item)
        tx, rx = selected.dma
        tx_vector = f"{tx.engine}_Stream{tx.lane}"
        rx_vector = f"{rx.engine}_Stream{rx.lane}"
        regions, count = dma_regions(name, result, include_flash=True)
        definitions += regions
        definitions += [f"static const nx_stm32_spi_dma_config_t {config} = {{",
                        "    .spi = {"]
    else:
        definitions += [f"static const nx_stm32_spi_config_t {config} = {{"]
    definitions += [f"    .registers = {item.controller}, .clock_hz = {selected.clock_hz}u,",
                    f"    .pins = s_nx_spi_pins_{name}, .pin_count = {len(pin_records)}u,",
                    f"    .endpoints = s_nx_spi_endpoints_{name}, .endpoint_count = {len(children)}u"]
    if dma:
        definitions += ["    },",
                        f"    .dma = {tx.engine}, .tx = {tx_vector}, .rx = {rx_vector},",
                        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
                        f"    .priority = {selected.irq.priority}u,",
                        f"    .kernel_bound = {'true' if result.kernel is not None else 'false'}"]
    suffix = "_pair" if paired else ""
    arguments = (Address(state), Address(config),
                 Address(f"s_nx_spi_cold_{name}"))
    initialize = DirectCall(f"nx_stm32_{implementation}{suffix}_prepare", arguments)
    stop = ([DirectCall("nx_spi_port_stop", (Symbol(f"nx_binding_{name}"),))]
            if dma else [])
    stop.append(DirectCall(f"nx_stm32_{implementation}{suffix}_close", arguments))
    definitions += ["};"]
    if dma:
        for vector, receive in ((tx_vector, "false"), (rx_vector, "true")):
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Exact selected SPI DMA terminal observation. */",
                            f"void {vector}_IRQHandler(void) {{ nx_stm32_spi_dma_irq(&{state}, {receive}); }}"]
    return Construction(
        state_type=f"nx_stm32_{implementation}_state_t",
        definitions=definitions, initialize=initialize,
        stop=stop, retain_on_failed_init=True,
        pin_snapshots=snapshots)


def _i2c_constructor(item, result, devices):
    """Describe exact resources; concrete C owns every I2C cold transition."""
    selected = item.integration
    if (selected is None or selected.hardware is None or
            selected.hardware.kind != "i2c" or
            selected.hardware.controller != item.controller or
            selected.clock_hz != 42000000 or len(selected.pins) != 2):
        raise ValueError("I2C requires its reviewed controller integration")
    if item.mode not in {"poll-messages", "irq-messages", "dma-messages"}:
        raise ValueError("I2C requires its maintained execution mode")
    asynchronous = mode_traits(item.kind, item.mode).irq
    dma = mode_traits(item.kind, item.mode).dma
    if asynchronous and selected.irq is None:
        raise ValueError("I2C requires its selected IRQ integration")
    if dma:
        secondary_controllers.dma_paths(item)
    elif selected.dma:
        raise ValueError("I2C polling/IRQ integration cannot retain DMA lanes")
    name = _symbol(item.id)
    saved = f"s_nx_i2c_pin_state_{name}"
    config = f"s_nx_i2c_config_{name}"
    receipt = f"s_nx_i2c_cold_{name}"
    definitions = [f"static uint16_t {saved}[2];",
                   f"static nx_stm32_i2c_cold_receipt_t {receipt};",
                   f"static const nx_stm32_i2c_pin_config_t s_nx_i2c_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(selected.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ &{slot}, {int(wire[2:])}u }},")
    definitions += ["};"]
    if dma:
        regions, count = dma_regions(name, result, include_flash=True)
        definitions += regions
        definitions += [f"static const nx_i2c_v1_dma_t s_nx_i2c_dma_{name} =",
                        f"    NX_STM32_{item.controller}_DMA(s_nx_dma_regions_{name}, {count}u);"]
    definitions += [f"static const nx_stm32_i2c_config_t {config} = {{",
                    f"    .registers = {item.controller}, .pins = s_nx_i2c_pins_{name},",
                    f"    .dma = {'&s_nx_i2c_dma_' + name if dma else 'NULL'},",
                    f"    .clock_hz = {selected.clock_hz}u,",
                    f"    .priority = {selected.irq.priority if asynchronous else 0}u,",
                    f"    .asynchronous = {'true' if asynchronous else 'false'}", "};"]
    implementation = "i2c_async" if asynchronous else "i2c"
    children = [child for child in devices if child.controller == item.id]
    if not children:
        raise ValueError("I2C requires at least one reviewed address endpoint")
    for index, child in enumerate(children):
        child_name = _symbol(child.id)
        endpoint = (f"s_nx_endpoint_{name}" if index == 0 else
                    f"s_nx_endpoint_{child_name}")
        if index:
            definitions += [f"static nx_stm32_{implementation}_endpoint_state_t {endpoint} = {{",
                            f"    .port = &s_nx_port_{name}, .address = {child.options.address}u", "};"]
        definitions += _endpoint_face("i2c", child_name, endpoint,
                                      implementation)
    if asynchronous:
        for vector, _ in selected.hardware.irqs:
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Fixed selected I2C message publisher. */",
                            f"void {vector}_IRQHandler(void) {{ nx_i2c_v1_irq(&s_nx_port_{name}); }}"]
    if dma:
        for lane, receive in zip(selected.dma, ("true", "false")):
            vector = f"{lane.engine}_Stream{lane.lane}"
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Fixed selected I2C DMA publisher. */",
                            f"void {vector}_IRQHandler(void) {{ nx_i2c_v1_dma_irq(&s_nx_port_{name}, {receive}); }}"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config),
                 Address(receipt))
    return Construction(
        state_type=f"nx_stm32_{implementation}_state_t",
        definitions=definitions,
        initialize=DirectCall(f"nx_stm32_{implementation}_prepare", arguments),
        stop=[DirectCall(f"nx_stm32_{implementation}_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)


def _controller_constructor(item, result, board_bindings, routes, devices):
    """Emit static contexts after resolver resource and mode validation.

    No pin is inferred from a Board name. SPI/I2C child identities and CS/address
    facts must already be reviewed and claimed by the resolver. Any unsupported
    controller, channel or incomplete electrical/resource description fails.
    """
    item = controller_ir(item, 'stm32f407')
    name = _symbol(item.id)
    kind = item.kind
    declarations = []
    body = [f"static nx_result_t prepare_{name}(void) {{",
            "    nx_result_t status = NX_SUCCESS;"]
    stop = []
    captured = [pin["pin"] for pin in item.pins]
    rollback_extra = []
    clock_bits = {"APB1ENR": "0u", "APB2ENR": "0u"}
    if kind == "exti":
        if item.controller not in {"EXTI0", "EXTI5", "EXTI6"} or len(item.pins) != 1:
            raise ValueError("EXTI line must use a reviewed package route")
        line = int(item.controller[4:])
        pin = item.pins[0]
        if int(pin["pin"][2:]) != line:
            raise ValueError("EXTI pin and mux line mismatch")
        irq = "EXTI0_IRQn" if line == 0 else "EXTI9_5_IRQn"
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}.get(item.options.edge)
        capacity = item.options.event_capacity
        priority = item.options.irq.priority
        if edge is None or not isinstance(capacity, int) or not 1 <= capacity <= 4096:
            raise ValueError("EXTI requires an edge and bounded event_capacity")
        if not isinstance(priority, int) or not 0 <= priority <= 15:
            raise ValueError("EXTI requires a reviewed IRQ priority")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_SYSCFGEN"
        declarations.append(f"static nx_exti_event_t s_nx_events_{name}[{capacity}];")
        body += [f"    if ((EXTI->IMR & {1 << line}u) != 0u) {{",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        _pin(body, pin, 0)
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;", "    nx_arch_dsb();",
                 f"    s_nx_port_{name}.registers = EXTI;",
                 f"    s_nx_port_{name}.gpio = GPIO{pin['pin'][1]};",
                 f"    s_nx_port_{name}.storage = s_nx_events_{name};",
                 f"    s_nx_port_{name}.capacity = {capacity}u;", f"    s_nx_port_{name}.line = {line}u;",
                 f"    s_nx_port_{name}.edge = {edge};"]
        body += [f"    status = nx_stm32_exti_initialize(&s_nx_port_{name}, SYSCFG, {ord(pin['pin'][1]) - ord('A')}u);",
                 "    if (status != NX_SUCCESS) {",
                 f"        if (s_nx_port_{name}.initialized) {{ return status; }}",
                 "        goto rollback;", "    }",
                 # Cold preflight already rejected foreign pending state.
                 # Once armed, a new edge or selected peer owns the hint.
                 # Publication must neither clear nor reject that pending bit.
                 "    nx_arch_dsb();", "    nx_arch_isb();",
                 f"    NVIC_SetPriority({irq}, {priority}u);",
                 f"    NVIC_EnableIRQ({irq});",
                 "    nx_arch_dsb();", "    nx_arch_isb();",
                 f"    if (NVIC_GetPriority({irq}) != {priority}u ||",
                 f"            NVIC_GetEnableIRQ({irq}) == 0u ||",
                 f"            NVIC_GetActive({irq}) != 0u) {{",
                 "        return NX_ERROR_IO;", "    }"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    bool owned = s_nx_port_{name}.initialized;",
                         f"    if (!owned) {{ return NX_SUCCESS; }}",
                         f"    nx_result_t status = nx_exti_port_stop(&s_nx_face_{name});",
                         "    if (status != NX_SUCCESS) { return status; }"] + _release(item.pins) + [
                         "    return NX_SUCCESS;", "}"]
        stop = [DirectCall(f"stop_{name}")]
    elif kind == "pwm":
        if item.controller != "TIM3" or [pin["pin"] for pin in item.pins] != ["PA6"]:
            raise ValueError("Only reviewed TIM3 CH1 PA6 PWM route is maintained")
        period, duty, tick = (item.options.period_ticks, item.options.duty_ticks,
                              item.options.tick_hz)
        if not all(isinstance(value, int) for value in (period, duty, tick)):
            raise ValueError("PWM requires explicit period/duty/tick_hz")
        if not 1 <= period <= LIMITS["pwm_period"] or not 0 <= duty <= period or tick <= 0:
            raise ValueError("Invalid fixed TIM3 PWM counts")
        timer_hz = LIMITS["timer_hz"]
        if timer_hz % tick != 0 or not 1 <= timer_hz // tick <= 65536:
            raise ValueError("PWM tick_hz is not an exact reviewed TIM3 divider")
        inactive = item.initial
        if inactive not in (0, 64):
            raise ValueError("PWM idle must be explicit PA6 masked output level")
        clock_bits["APB1ENR"] = "RCC_APB1ENR_TIM3EN"
        declarations.append(f"static nx_stm32_gpio_state_t s_nx_idle_{name};")
        body += ["    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;",
                 "    nx_arch_dsb();",
                 f"    if ((TIM3->CCER & 1u) != 0u || ((TIM3->CR1 & TIM_CR1_CEN) != 0u &&",
                 f"        (TIM3->PSC != {timer_hz // tick - 1}u || TIM3->ARR != {period - 1}u))) {{",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        _check(body, "nx_stm32_gpio_clock_enable(0u)")
        _capture(body, "PA6")
        rollback_extra.append(f"    s_nx_idle_{name}.initialized = false;")
        body += [f"    s_nx_idle_{name}.registers = GPIOA;",
                 f"    s_nx_idle_{name}.mask = 64u;", f"    s_nx_idle_{name}.output = true;"]
        _check(body, f"nx_stm32_gpio_initialize(&s_nx_idle_{name}, {inactive}u)")
        body += [f"    s_nx_port_{name}.registers = TIM3;",
                 f"    s_nx_port_{name}.inactive_gpio = &s_nx_idle_{name};",
                 f"    s_nx_port_{name}.inactive_mask = 64u;",
                 f"    s_nx_port_{name}.inactive_high = {'true' if inactive else 'false'};",
                 f"    s_nx_port_{name}.channel = 1u;", f"    s_nx_port_{name}.period_ticks = {period}u;",
                 f"    s_nx_port_{name}.duty_ticks = {duty}u;", f"    s_nx_port_{name}.tick_hz = {tick}u;",
                 f"    s_nx_port_{name}.prescaler = {timer_hz // tick - 1}u;"]
        _check(body, f"nx_stm32_pwm_initialize(&s_nx_port_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    nx_result_t status = nx_pwm_port_stop(&s_nx_face_{name});",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    s_nx_port_{name}.initialized = false;",
                         f"    s_nx_idle_{name}.initialized = false;",
                         "    if ((TIM3->CCER & 0x1111u) == 0u) {",
                         "        RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;",
                         "    }", "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [DirectCall(f"stop_{name}")]
    elif kind == "adc":
        traits = mode_traits(kind, item.mode)
        stream, continuous = traits.dma, traits.continuous
        stream_state = f"s_nx_port_{name}" + (".base" if continuous else "")
        adc = stream_state + (".adc" if stream else "")
        channels = list(item.options.channels)
        reviewed = [int(pin["pin"][2:]) for pin in item.pins]
        if item.controller != "ADC1" or channels != reviewed or tuple(channels) not in ADC_CHANNEL_SEQUENCES:
            raise ValueError("ADC channel sequence must match the reviewed PA0/PA1 route")
        sampling = list(item.options.sample_times)
        reference = item.options.reference_mv
        if not isinstance(sampling, list) or len(sampling) != len(channels) or not all(
                isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 7
                for value in sampling):
            raise ValueError("ADC requires one encoded sample_time (0..7) per channel")
        if not isinstance(reference, int) or not 1 <= reference <= 3600:
            raise ValueError("ADC requires a declared nominal reference voltage")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_ADC1EN"
        channel_values = ", ".join(f"{value}u" for value in channels)
        sampling_values = ", ".join(f"{value}u" for value in sampling)
        declarations += [f"static const uint8_t s_nx_adc_channels_{name}[] = {{ {channel_values} }};",
                         f"static const uint8_t s_nx_adc_samples_{name}[] = {{ {sampling_values} }};"]
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;", "    nx_arch_dsb();",
                 "    if ((ADC1->CR2 & ADC_CR2_ADON) != 0u) {",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        for pin in item.pins:
            _pin(body, pin, 3)
        body += [
                 f"    {adc}.registers = ADC1;", f"    {adc}.common = ADC;",
                 f"    {adc}.channels = s_nx_adc_channels_{name};",
                 f"    {adc}.sample_times = s_nx_adc_samples_{name};",
                 f"    {adc}.channel_count = {len(channels)}u;", f"    {adc}.reference_mv = {reference}u;"]
        if stream:
            clock_bits["APB1ENR"] = "RCC_APB1ENR_TIM3EN"
            path = hardware.maintained("stm32f407", item.controller).dma(item.mode)[0]
            vector = hardware.dma_vector("stm32f407", path)
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            body += ["    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;", "    nx_arch_dsb();",
                     f"    {stream_state}.timer = TIM3;",
                     f"    {stream_state}.dma = DMA{path[0]};",
                     f"    {stream_state}.memory = {vector};",
                     f"    {stream_state}.regions = s_nx_dma_regions_{name};",
                     f"    {stream_state}.region_count = {region_count}u;",
                     f"    {stream_state}.timer_clock_hz = 84000000u;",
                     f"    {stream_state}.adc_clock_hz = 21000000u;"]
            _check(body, f"nx_stm32_adc_{'continuous' if continuous else 'stream'}_initialize(&s_nx_port_{name})")
            body.append(f"    NVIC_SetPriority({vector}_IRQn, {item.options.irq.priority}u);")
            declarations += [f"void {vector}_IRQHandler(void);",
                             "/** \\brief Exact ADC DMA block terminal observation. */",
                             f"void {vector}_IRQHandler(void) {{ nx_stm32_adc_{'continuous' if continuous else 'stream'}_irq(&s_nx_port_{name}); }}"]
        else:
            _check(body, f"nx_stm32_adc_initialize(&s_nx_port_{name})")
        stop_body = [f"static nx_result_t stop_{name}(void) {{"]
        if stream:
            stop_body += [f"    nx_result_t stopped = nx_adc_port_stream_stop(&s_nx_face_{name});",
                          "    if (stopped != NX_SUCCESS) { return stopped; }",
                          f"    NVIC_DisableIRQ({vector}_IRQn);",
                          f"    NVIC_ClearPendingIRQ({vector}_IRQn);"]
        stop_body += [f"    if ({adc}.active) {{ return NX_ERROR_BUSY; }}",
                      "    ADC1->CR2 = 0u;", f"    {adc}.initialized = false;"]
        if stream:
            stop_body.append("    RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;")
        declarations += stop_body + _release(item.pins) + [
                         "    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_ADC1EN;",
                         "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [DirectCall(f"stop_{name}")]
    else:
        raise ValueError(f"No maintained STM32 constructor for {kind}")
    body.append("    return status;")
    if captured:
        # Cold-path snapshots occupy the stack only. They never add a runtime
        # factory, resident pin table or hidden shutdown ownership context.
        pins = list(dict.fromkeys(captured))
        mask = sum(1 << (ord(pin[1]) - ord("A")) for pin in
                   {name[:2] for name in pins})
        locals_ = ["    uint32_t retained_gpio_clocks = 0u;",
                   f"    uint32_t saved_ahb1 = RCC->AHB1ENR & {mask}u;",
                   f"    uint32_t saved_apb1 = RCC->APB1ENR & {clock_bits['APB1ENR']};",
                   f"    uint32_t saved_apb2 = RCC->APB2ENR & {clock_bits['APB2ENR']};"]
        for pin in pins:
            locals_ += [f"    uint16_t saved_{pin} = 0u;",
                        f"    bool captured_{pin} = false;"]
        body[2:2] = locals_
        body += ["rollback:"] + rollback_extra
        for pin in reversed(pins):
            body += [f"    if (captured_{pin}) {{"]
            body += rollback_pin("stm32f407", pin, f"saved_{pin}")
            body += ["    }"]
        body += [f"    RCC->APB1ENR = (RCC->APB1ENR & ~(uint32_t)({clock_bits['APB1ENR']})) | saved_apb1;",
                 f"    RCC->APB2ENR = (RCC->APB2ENR & ~(uint32_t)({clock_bits['APB2ENR']})) | saved_apb2;",
                 f"    RCC->AHB1ENR = (RCC->AHB1ENR & ~{mask}u) | saved_ahb1 | retained_gpio_clocks;",
                 "    nx_arch_dsb();", "    return status;"]
    body.append("}")
    from .contract import mode_implementation
    implementation = mode_implementation(item.kind, item.mode)
    return Construction(
        state_type=f"nx_stm32_{implementation}_state_t",
        definitions=declarations + body, initialize=DirectCall(f"prepare_{name}"),
        stop=stop, retain_on_failed_init=kind == "exti")


def shared_irq_definitions(selections):
    """Emit one bounded static dispatcher per selected hardware vector."""
    selections = [controller_ir(item, "stm32f407") for item in selections]
    groups = {}
    for item in selections:
        if item.kind != "exti":
            continue
        line = int(item.controller[4:])
        vector = "EXTI0" if line == 0 else "EXTI9_5"
        groups.setdefault(vector, []).append(item)
    lines = []
    for vector, items in groups.items():
        if len({item.options.irq.priority for item in items}) != 1:
            raise ValueError("Shared EXTI vector priorities must agree")
        pointers = ", ".join("&s_nx_port_" + _symbol(item.id) for item in items)
        array = "s_nx_irq_" + vector.lower() + "_ports"
        mask = sum(1 << int(item.controller[4:]) for item in items)
        lines.extend([f"static nx_stm32_exti_state_t* const {array}[] = {{ {pointers} }};",
                      f"void {vector}_IRQHandler(void);",
                      "/** \\brief Static bounded EXTI shared-vector dispatch. */",
                      f"void {vector}_IRQHandler(void) {{",
                      f"    nx_stm32_exti_dispatch({array}, {len(items)}u, {mask}u);",
                      "}"])
    return lines


def _gpio_constructor(item):
    """Describe static GPIO authorization; C owns clock and pin operations."""
    name = _symbol(item.id)
    port = item.pins[0]["pin"][1]
    mask = item.options.mask or sum(1 << int(pin["pin"][2:])
                                   for pin in item.pins)
    initial = item.options.initial
    config = f"s_nx_gpio_config_{name}"
    definitions = [f"static const nx_stm32_gpio_config_t {config} = {{",
                   f"    .registers = GPIO{port}, .mask = {mask}u,",
                   f"    .initial = {initial}u,",
                   f"    .output = {'true' if item.mode == 'output' else 'false'}", "};"]
    return Construction(
        state_type="nx_stm32_gpio_state_t", definitions=definitions,
        initialize=DirectCall("nx_stm32_gpio_prepare",
                             (Address(f"s_nx_port_{name}"), Address(config))),
        stop=[DirectCall("nx_stm32_gpio_stop",
                         (Address(f"s_nx_port_{name}"), initial))],
        retain_on_failed_init=False)


def _basic_constructor(item, result):
    """Keep immutable Flash geometry and fixed IWDG binding declarative."""
    name = _symbol(item.id)
    state = Address(f"s_nx_port_{name}")
    if item.kind == "watchdog":
        return Construction(
            state_type="nx_stm32_watchdog_state_t", definitions=(),
            initialize=DirectCall("nx_stm32_watchdog_prepare", (state, 1000000)),
            stop=[DirectCall("nx_stm32_watchdog_stop", (state,))],
            retain_on_failed_init=False)
    if item.kind != "flash":
        raise ValueError("No basic STM32 constructor for selected kind")
    exact_part(PARTS, result.part)
    geometry = f"s_nx_flash_geometry_{name}"
    definitions = flash_geometry(
        result, symbol=geometry, sector_symbol=f"s_nx_flash_sectors_{name}",
        program_unit=FLASH_PROGRAM_UNIT)
    flash = result.physical_part.flash
    definitions += [f'_Static_assert(FLASH_BASE == 0x{flash["origin"]:08x}u,',
                    '               "Reviewed Flash base differs from the SDK");']
    return Construction(
        state_type="nx_stm32_flash_state_t", definitions=definitions,
        initialize=DirectCall("nx_stm32_flash_prepare",
                             (state, Address(geometry), 3300)),
        stop=[DirectCall("nx_stm32_flash_close", (state,))],
        retain_on_failed_init=False)


def _uart_constructor(item, result):
    name = _symbol(item.id)
    traits = mode_traits("uart", item.mode)
    dma_uart, receive_uart = traits.tx_dma, traits.rx_dma
    block_uart = traits.block_stream
    words = item.mode == "irq-words"
    implementation = ("uart_word" if words else
                      "uart_duplex" if receive_uart and dma_uart else
                      "uart_receive" if receive_uart else
                      "uart_dma" if dma_uart else
                      "uart_stream" if block_uart else "uart")
    options = item.options
    profile = ("NX_UART_RX_BLOCKS" if block_uart else
               "NX_UART_RX_EVENTS" if options.rx_profile == "events" else
               "NX_UART_RX_BYTES")
    storage = ("uint16_t" if words else
               "nx_uart_rx_event_t" if options.rx_profile == "events" else
               "uint8_t")
    capacity = options.rx_capacity or 0
    definitions = []
    if not block_uart:
        definitions.append(f"static {storage} s_nx_rx_{name}[{capacity}];")
    dma_state = f"s_nx_port_{name}" + (".receive" if receive_uart and dma_uart else "")
    tx_state = f"s_nx_port_{name}.transmit"
    uart_state = (dma_state + ".uart" if receive_uart or dma_uart or block_uart
                  else dma_state)
    rx_storage = 'NULL' if block_uart else f's_nx_rx_{name}'
    definitions += [f'static nx_result_t prepare_{name}(void) {{',
               f'    {uart_state}.registers = {item.controller};']
    if not words:
        definitions += [f'    {uart_state}.rx_storage = {rx_storage};',
                        f'    {uart_state}.rx_capacity = {capacity}u;',
                        f'    {uart_state}.baud = {item.options.baud}u;',
                        f'    {uart_state}.profile = {profile};']
    initialize = (f"nx_stm32_uart_word_initialize(&s_nx_port_{name}, "
                  f"84000000u, {options.baud}u, s_nx_rx_{name}, {capacity}u)"
                  if words else
                  f"nx_stm32_{implementation}_initialize(&s_nx_port_{name}, "
                  "84000000u)")
    if not words and uart_formats.changed(item):
        initialize = (f"nx_stm32_uart_initialize_format(&s_nx_port_{name}, "
                      f"84000000u, {uart_formats.expression(options.frame)})")
    stop_function = "nx_uart_word_port_stop" if words else "nx_uart_port_stop"
    definitions += [f'    {uart_state}.irq = {item.controller}_IRQn;',
               '    uint32_t saved_clock = RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN;',
               '    uint32_t saved_uart_clock = RCC->APB2ENR & RCC_APB2ENR_USART1EN;',
               '    nx_result_t clock = nx_stm32_gpio_clock_enable(0u);',
               '    if (clock != NX_SUCCESS) {',
               '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock;',
               '        return clock;', '    }',
               '    uint16_t saved_tx = nx_stm32_pin_capture(GPIOA, 9u);',
               '    uint16_t saved_rx = nx_stm32_pin_capture(GPIOA, 10u);',
               '    nx_result_t pins = nx_stm32_uart1_pins_prepare();',
               f'    nx_result_t uart = pins == NX_SUCCESS ? {initialize} : pins;',
               '    if (uart != NX_SUCCESS) {',
               '        uint32_t retained_gpio_clocks = 0u;']
    definitions += rollback_pin("stm32f407", "PA9", "saved_tx")
    definitions += rollback_pin("stm32f407", "PA10", "saved_rx")
    definitions += [
               '        RCC->APB2ENR = (RCC->APB2ENR & ~(uint32_t)RCC_APB2ENR_USART1EN) | saved_uart_clock;',
               '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock | retained_gpio_clocks;',
               '        nx_arch_dsb();', '        return uart;', '    }',
               f'    NVIC_SetPriority({uart_state}.irq, {item.options.irq.priority}u);',
               '    return NX_SUCCESS;', '}',
               f'static nx_result_t stop_{name}(void) {{',
               f'    nx_result_t status = {stop_function}(&s_nx_face_{name});',
               '    if (status != NX_SUCCESS) { return status; }']
    definitions += withdraw_pin("stm32f407", "PA9", indent=4)
    definitions += withdraw_pin("stm32f407", "PA10", indent=4)
    definitions += [
               '    (void)nx_stm32_pin_configure(GPIOA, 9u, 0u, 0u, 0u, false);',
               '    (void)nx_stm32_pin_configure(GPIOA, 10u, 0u, 0u, 0u, false);',
               '    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_USART1EN;',
               '    nx_arch_dsb();', '    return NX_SUCCESS;', '}']
    init = DirectCall(f"prepare_{name}")
    irq = item.controller + '_IRQHandler'
    irq_implementation = ("uart_word" if words else
                         "uart_format" if uart_formats.changed(item) else
                         "uart_duplex" if receive_uart and dma_uart else
                      "uart_receive" if receive_uart else
                          "uart_dma_uart" if dma_uart else
                          "uart_stream" if block_uart else "uart")
    irq_call = uart_formats.irq_call(item, f"nx_stm32_{irq_implementation}_irq(&s_nx_port_{name});")
    definitions += [f'void {irq}(void);', '/** \\brief Fixed single-controller IRQ dispatch. */',
               f'void {irq}(void) {{ {irq_call} }}']
    if words or uart_formats.changed(item):
        rejected = definitions.index('    if (uart != NX_SUCCESS) {') + 1
        definitions[rejected:rejected] = [
            f'        if (s_nx_port_{name}.initialized) {{ return uart; }}']
    if traits.dma:
        from .contract import _FIXED_DMA
        expected = _FIXED_DMA.get((item.controller, item.mode))
        if tuple(item.dma) != expected:
            raise ValueError("UART DMA differs from its reviewed fixed route")
        region_lines, region_count = dma_regions(
            name, result, include_flash=traits.tx_dma)
        position = definitions.index(f"static nx_result_t prepare_{name}(void) {{")
        definitions[position:position] = region_lines
        port_binding = hardware.maintained("stm32f407", item.controller)
        path = port_binding.dma(item.mode)[0]
        prefix = [f"    {dma_state}.resources.dma = DMA{path[0]};",
                  f"    {dma_state}.resources.regions = s_nx_dma_regions_{name};",
                  f"    {dma_state}.resources.region_count = {region_count}u;"]
        if receive_uart:
            rx = port_binding.dma("rx-dma-idle")[0]
            rx_vector = hardware.dma_vector("stm32f407", rx)
            prefix += [f"    {dma_state}.receive = {rx_vector};",
                       f"    {dma_state}.continuous = {'true' if traits.continuous else 'false'};"]
        if dma_uart:
            tx = port_binding.dma("dma-tx")[0]
            tx_vector = hardware.dma_vector("stm32f407", tx)
            prefix += [f"    {tx_state}.tx = {tx_vector};",
                       f"    {tx_state}.dma_irq = {tx_vector}_IRQn;",
                       f"    {tx_state}.stream = {tx[1]}u;",
                       f"    {tx_state}.channel = {tx[2]}u;"]
        position += len(region_lines) + 1
        definitions[position:position] = prefix
        priority_line = definitions.index(
            f"    NVIC_SetPriority({uart_state}.irq, {item.options.irq.priority}u);")
        priorities = []
        if receive_uart:
            priorities.append(f"    NVIC_SetPriority({rx_vector}_IRQn, {item.options.irq.priority}u);")
            definitions += [f"void {rx_vector}_IRQHandler(void);",
                            "/** \\brief Exact selected DMA receive publisher. */",
                            f"void {rx_vector}_IRQHandler(void) {{ nx_stm32_uart_receive_dma_irq(&{dma_state}); }}"]
        if dma_uart:
            priorities.append(f"    NVIC_SetPriority({tx_state}.dma_irq, {item.options.irq.priority}u);")
            definitions += [f"void {tx_vector}_IRQHandler(void);",
                            "/** \\brief Exact selected DMA TX observation. */",
                            f"void {tx_vector}_IRQHandler(void) {{ nx_stm32_uart_{'duplex_dma' if receive_uart else 'dma'}_irq(&s_nx_port_{name}); }}"]
        definitions[priority_line + 1:priority_line + 1] = priorities
    return Construction(
        state_type=f"nx_stm32_{implementation}_state_t",
        definitions=definitions, initialize=init, stop=[DirectCall(f"stop_{name}")],
        retain_on_failed_init=words or uart_formats.changed(item))


def constructor(item, result, board_bindings, routes, devices):
    """Emit one reviewed STM32 instance, including its rollback and stop."""
    item = controller_ir(item, "stm32f407")
    if item.kind == "spi":
        construction = _spi_constructor(
            item, result, board_bindings, routes, devices,
            paired=secondary_controllers.applies(item, result, "stm32"))
        return enhanced_endpoints.dma_construction(item, construction, "stm32")
    if item.kind == "i2c":
        return _i2c_constructor(item, result, devices)
    if secondary_controllers.applies(item, result, "stm32"):
        construction = secondary_controllers.constructor(item, result, "stm32")
        return enhanced_endpoints.dma_construction(item, construction, "stm32")
    if item.kind in timer_advanced.KINDS:
        return timer_advanced.constructor(item, result, "stm32")
    if item.kind == "can":
        return can.constructor(item, result, "stm32")
    if item.kind in {"eth", "eth_frames"}:
        return ethernet.constructor(item, result, "stm32")
    if item.kind == "usb_device":
        return usb_device.constructor(item, result, "stm32")
    if item.kind == "gpio":
        return _gpio_constructor(item)
    if item.kind in {"flash", "watchdog"}:
        return _basic_constructor(item, result)
    if item.kind == "uart":
        return _uart_constructor(item, result)
    construction = _controller_constructor(item, result, board_bindings,
                                            routes, devices)
    return enhanced_endpoints.dma_construction(item, construction, "stm32")
