"""Reviewed STM32 fixed construction, never a runtime resource allocator."""
try:
    from ...emission import dma_regions
except ImportError:
    from emission import dma_regions
from ..common import controller_ir


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
    return [f"    (void)nx_stm32_pin_configure(GPIO{pin['pin'][1]}, "
            f"{pin['pin'][2:]}u, 0u, 0u, 0u, false);" for pin in pins]


def _endpoint_face(kind, name, state, implementation=None):
    implementation = implementation or kind
    return [f"static const nx_{kind}_endpoint_t s_nx_device_face_{name} = {{",
            f"    &nx_stm32_{implementation}_endpoint_ops, &{state}", "};",
            f"const nx_{kind}_endpoint_t* const nx_device_{name} = &s_nx_device_face_{name};"]


def _controller_constructor(item, result, board_bindings, routes, devices):
    """Emit static contexts after resolver resource and mode validation.

    No pin is inferred from a Board name. SPI/I2C child identities and CS/address
    facts must already be reviewed and claimed by the resolver. Any unsupported
    controller, channel or incomplete electrical/resource description fails.
    """
    item = controller_ir(item, 'stm32f407')
    name = _symbol(item["id"])
    kind = item["kind"]
    declarations = []
    body = [f"static nx_result_t prepare_{name}(void) {{",
            "    nx_result_t status = NX_SUCCESS;"]
    stop = []
    captured = [pin["pin"] for pin in item["pins"]]
    rollback_extra = []
    clock_bits = {"APB1ENR": "0u", "APB2ENR": "0u"}
    if kind == "spi":
        dma = item["mode"] == "dma"
        bus = f"s_nx_port_{name}" + (".spi" if dma else "")
        if item["controller"] != "SPI1":
            raise ValueError("Only reviewed SPI1 is maintained")
        children = [child for child in devices
                    if child["controller"] == item["id"]]
        if not children:
            raise ValueError("STM32 SPI requires a reviewed CS endpoint")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_SPI1EN"
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;",
                 "    nx_arch_dsb();",
                 f"    {bus}.registers = SPI1;",
                 f"    {bus}.rcc = RCC;",
                 f"    {bus}.clock_hz = 84000000u;",
                 "    saved_spi_cr1 = SPI1->CR1;",
                 "    saved_spi_cr2 = SPI1->CR2;",
                 "    captured_spi = true;",
                 "    if ((saved_spi_cr1 & SPI_CR1_SPE) != 0u) {",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }",
                 "    SPI1->CR1 = 0u;", "    SPI1->CR2 = 0u;"]
        rollback_extra += ["    if (captured_spi) { SPI1->CR1 = saved_spi_cr1; SPI1->CR2 = saved_spi_cr2; }"]
        for pin in item["pins"]:
            _pin(body, pin, 2)
        for index, child in enumerate(children):
            child_name = _symbol(child["id"])
            binding = board_bindings[child["cs_binding"]]
            cs_route = routes[binding["route"]]
            if len(cs_route["pins"]) != 1:
                raise ValueError("SPI CS requires exactly one reviewed pin")
            cs_pin = cs_route["pins"][0]["pin"]
            captured.append(cs_pin)
            bit = 1 << int(cs_pin[2:])
            if binding.get("initial") != bit:
                raise ValueError("SPI active-low CS must declare initial high")
            endpoint = f"s_nx_endpoint_{name}" if index == 0 else f"s_nx_endpoint_{child_name}"
            implementation = "spi_dma" if dma else "spi"
            if index != 0:
                declarations.append(f"static nx_stm32_{implementation}_endpoint_state_t {endpoint};")
            declarations += [f"static nx_stm32_gpio_state_t s_nx_cs_{child_name};",
                             *_endpoint_face("spi", child_name, endpoint, implementation)]
            _check(body, f"nx_stm32_gpio_clock_enable({ord(cs_pin[1]) - ord('A')}u)")
            _capture(body, cs_pin)
            rollback_extra.append(f"    s_nx_cs_{child_name}.initialized = false;")
            body += [f"    s_nx_cs_{child_name}.registers = GPIO{cs_pin[1]};",
                     f"    s_nx_cs_{child_name}.mask = {bit}u;",
                     f"    s_nx_cs_{child_name}.output = true;"]
            _check(body, f"nx_stm32_gpio_initialize(&s_nx_cs_{child_name}, {bit}u)")
            mode = child.get("mode", 0)
            maximum = child.get("max_hz", item.options.max_hz)
            if not isinstance(mode, int) or isinstance(mode, bool) or not 0 <= mode <= 3:
                raise ValueError("SPI mode must be 0 through 3")
            if not isinstance(maximum, int) or maximum < 328125 or maximum > 42000000:
                raise ValueError("SPI1 maintained frequency is 328125 through 42000000 Hz")
            base = endpoint + (".endpoint" if dma else "")
            body += [f"    {base}.port = &{bus};",
                     f"    {base}.cs = &s_nx_cs_{child_name};",
                     f"    {base}.cs_mask = {bit}u;",
                     f"    {base}.mode = {mode}u;",
                     f"    {base}.frequency_hz = {maximum}u;"]
            if dma:
                body.append(f"    {endpoint}.dma = &s_nx_port_{name};")
        if dma:
            region_lines, region_count = dma_regions(name, result, include_flash=True)
            declarations += region_lines
            body += [f"    s_nx_port_{name}.dma = DMA2;",
                     f"    s_nx_port_{name}.tx = DMA2_Stream3;",
                     f"    s_nx_port_{name}.rx = DMA2_Stream0;",
                     f"    s_nx_port_{name}.regions = s_nx_dma_regions_{name};",
                     f"    s_nx_port_{name}.region_count = {region_count}u;"]
            _check(body, f"nx_stm32_spi_dma_initialize(&s_nx_port_{name})")
            body += [f"    NVIC_SetPriority(DMA2_Stream3_IRQn, {item.options.irq.priority}u);",
                     f"    NVIC_SetPriority(DMA2_Stream0_IRQn, {item.options.irq.priority}u);"]
            for stream, receive in ((3, "false"), (0, "true")):
                declarations += [f"void DMA2_Stream{stream}_IRQHandler(void);",
                                 "/** \\brief Exact selected SPI DMA terminal observation. */",
                                 f"void DMA2_Stream{stream}_IRQHandler(void) {{ nx_stm32_spi_dma_irq(&s_nx_port_{name}, {receive}); }}"]
        stop_name = f"stop_{name}"
        stop_body = [f"static nx_result_t {stop_name}(void) {{"]
        if dma:
            stop_body += [f"    nx_result_t stopped = nx_spi_port_stop(nx_binding_{name});",
                          "    if (stopped != NX_SUCCESS) { return stopped; }"]
        else:
            stop_body.append(f"    if ({bus}.active) {{ return NX_ERROR_BUSY; }}")
        stop_body.append("    SPI1->CR1 = 0u;")
        for child in children:
            child_name = _symbol(child["id"])
            binding = board_bindings[child["cs_binding"]]
            stop_body += [f"    nx_result_t result_{child_name} = nx_stm32_gpio_stop(",
                          f"        &s_nx_cs_{child_name}, {binding['initial']}u);",
                          f"    if (result_{child_name} != NX_SUCCESS) {{ return result_{child_name}; }}"]
        stop_body += _release(item["pins"])
        stop_body += ["    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_SPI1EN;",
                      "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        declarations += stop_body
        stop = [f"{stop_name}()"]
    elif kind == "i2c":
        if item["controller"] != "I2C1" or item.get("max_hz", 100000) != 100000:
            raise ValueError("Only I2C1 100 kHz standard mode is maintained")
        children = [child for child in devices
                    if child["controller"] == item["id"]]
        if not children:
            raise ValueError("I2C requires at least one reviewed 7-bit address endpoint")
        clock_bits["APB1ENR"] = "RCC_APB1ENR_I2C1EN"
        body += ["    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;", "    nx_arch_dsb();",
                 "    if ((I2C1->CR1 & I2C_CR1_PE) != 0u) {",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        for pin in item["pins"]:
            # External pull-ups are a reviewed Board requirement; enabling an
            # internal pull-up does not stand in for that electrical contract.
            _pin(body, pin, 2, open_drain=True)
        # The maintained route has two lines on one GPIO port. Retain the
        # resolved facts for physical line-level observation during recover.
        line_ports = {pin["pin"][1] for pin in item["pins"]}
        if len(item["pins"]) != 2 or len(line_ports) != 1:
            raise ValueError("I2C requires two reviewed lines on one GPIO port")
        line_port = next(iter(line_ports))
        line_mask = sum(1 << int(pin["pin"][2:]) for pin in item["pins"])
        body += [f"    s_nx_port_{name}.registers = I2C1;",
                 f"    s_nx_port_{name}.line_gpio = GPIO{line_port};",
                 f"    s_nx_port_{name}.line_mask = {line_mask}u;",
                 f"    s_nx_port_{name}.peripheral_mhz = 42u;",
                 f"    s_nx_port_{name}.rate_hz = 100000u;"]
        for index, child in enumerate(children):
            child_name = _symbol(child["id"])
            address = child["address"]
            if not 8 <= address <= 119:
                raise ValueError("Reserved I2C addresses are not maintained")
            endpoint = f"s_nx_endpoint_{name}" if index == 0 else f"s_nx_endpoint_{child_name}"
            if index != 0:
                declarations.append(f"static nx_stm32_i2c_endpoint_state_t {endpoint};")
            declarations += [*_endpoint_face("i2c", child_name, endpoint)]
            body += [f"    {endpoint}.port = &s_nx_port_{name};",
                     f"    {endpoint}.address = {address}u;"]
        _check(body, f"nx_stm32_i2c_initialize(&s_nx_port_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    if (s_nx_port_{name}.active) {{ return NX_ERROR_BUSY; }}",
                         "    I2C1->CR1 = 0u;",
                         f"    s_nx_port_{name}.initialized = false;"] + _release(item["pins"]) + [
                         "    RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_I2C1EN;",
                         "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "flash":
        if item["controller"] != "FLASH":
            raise ValueError("Only the full internal Flash provider is maintained")
        geometry = "ve" if result["part"] == "STM32F407VET6" else "zg"
        body += [f"    s_nx_port_{name}.registers = FLASH;",
                 f"    s_nx_port_{name}.geometry = &g_nx_stm32_flash_{geometry};",
                 f"    s_nx_port_{name}.memory = (volatile uint8_t*)FLASH_BASE;",
                 f"    s_nx_port_{name}.supply_mv = 3300u;"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    return s_nx_port_{name}.active || (FLASH->SR & FLASH_SR_BSY) != 0u",
                         "        ? NX_ERROR_BUSY : NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "watchdog":
        if item["controller"] != "IWDG":
            raise ValueError("Only independent IWDG is maintained")
        body += [f"    s_nx_port_{name}.registers = IWDG;",
                 f"    s_nx_port_{name}.flash = FLASH;", f"    s_nx_port_{name}.debug = DBGMCU;",
                 f"    s_nx_port_{name}.rcc = RCC;", f"    s_nx_port_{name}.poll_limit = 1000000u;"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         "    nx_watchdog_state_t state;",
                         f"    nx_result_t result = nx_watchdog_port_state(&s_nx_face_{name}, &state);",
                         "    if (result != NX_SUCCESS) { return result; }",
                         "    return state.enabled ? NX_ERROR_STATE : NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "exti":
        if item["controller"] not in {"EXTI0", "EXTI5", "EXTI6"} or len(item["pins"]) != 1:
            raise ValueError("EXTI line must use a reviewed package route")
        line = int(item["controller"][4:])
        pin = item["pins"][0]
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
        _check(body, f"nx_stm32_exti_initialize(&s_nx_port_{name}, SYSCFG, {ord(pin['pin'][1]) - ord('A')}u)")
        body += [f"    NVIC_SetPriority({irq}, {priority}u);",
                 f"    NVIC_ClearPendingIRQ({irq});", f"    NVIC_EnableIRQ({irq});"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    nx_result_t status = nx_exti_port_stop(&s_nx_face_{name});",
                         "    if (status != NX_SUCCESS) { return status; }"] + _release(item["pins"]) + [
                         "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "pwm":
        if item["controller"] != "TIM3" or [pin["pin"] for pin in item["pins"]] != ["PA6"]:
            raise ValueError("Only reviewed TIM3 CH1 PA6 PWM route is maintained")
        period, duty, tick = (item.options.period_ticks, item.options.duty_ticks,
                              item.options.tick_hz)
        if not all(isinstance(value, int) for value in (period, duty, tick)):
            raise ValueError("PWM requires explicit period/duty/tick_hz")
        if not 1 <= period <= 65535 or not 0 <= duty <= period or tick <= 0:
            raise ValueError("Invalid fixed TIM3 PWM counts")
        if 84000000 % tick != 0 or not 1 <= 84000000 // tick <= 65536:
            raise ValueError("PWM tick_hz is not an exact reviewed TIM3 divider")
        inactive = item["initial"]
        if inactive not in (0, 64):
            raise ValueError("PWM idle must be explicit PA6 masked output level")
        clock_bits["APB1ENR"] = "RCC_APB1ENR_TIM3EN"
        declarations.append(f"static nx_stm32_gpio_state_t s_nx_idle_{name};")
        body += ["    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;",
                 "    nx_arch_dsb();",
                 f"    if ((TIM3->CCER & 1u) != 0u || ((TIM3->CR1 & TIM_CR1_CEN) != 0u &&",
                 f"        (TIM3->PSC != {84000000 // tick - 1}u || TIM3->ARR != {period - 1}u))) {{",
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
                 f"    s_nx_port_{name}.prescaler = {84000000 // tick - 1}u;"]
        _check(body, f"nx_stm32_pwm_initialize(&s_nx_port_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    nx_result_t status = nx_pwm_port_stop(&s_nx_face_{name});",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    s_nx_port_{name}.initialized = false;",
                         f"    s_nx_idle_{name}.initialized = false;",
                         "    if ((TIM3->CCER & 0x1111u) == 0u) {",
                         "        RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;",
                         "    }", "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "adc":
        stream = item["mode"] == "trigger-dma"
        adc = f"s_nx_port_{name}" + (".adc" if stream else "")
        channels = list(item.options.channels)
        reviewed = [int(pin["pin"][2:]) for pin in item["pins"]]
        if item["controller"] != "ADC1" or channels != reviewed or channels not in ([0], [0, 1]):
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
        for pin in item["pins"]:
            _pin(body, pin, 3)
        body += [
                 f"    {adc}.registers = ADC1;", f"    {adc}.common = ADC;",
                 f"    {adc}.channels = s_nx_adc_channels_{name};",
                 f"    {adc}.sample_times = s_nx_adc_samples_{name};",
                 f"    {adc}.channel_count = {len(channels)}u;", f"    {adc}.reference_mv = {reference}u;"]
        if stream:
            clock_bits["APB1ENR"] = "RCC_APB1ENR_TIM3EN"
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            body += ["    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;", "    nx_arch_dsb();",
                     f"    s_nx_port_{name}.timer = TIM3;",
                     f"    s_nx_port_{name}.dma = DMA2;",
                     f"    s_nx_port_{name}.memory = DMA2_Stream4;",
                     f"    s_nx_port_{name}.regions = s_nx_dma_regions_{name};",
                     f"    s_nx_port_{name}.region_count = {region_count}u;",
                     f"    s_nx_port_{name}.timer_clock_hz = 84000000u;",
                     f"    s_nx_port_{name}.adc_clock_hz = 21000000u;"]
            _check(body, f"nx_stm32_adc_stream_initialize(&s_nx_port_{name})")
            body.append(f"    NVIC_SetPriority(DMA2_Stream4_IRQn, {item.options.irq.priority}u);")
            declarations += ["void DMA2_Stream4_IRQHandler(void);",
                             "/** \\brief Exact ADC DMA block terminal observation. */",
                             f"void DMA2_Stream4_IRQHandler(void) {{ nx_stm32_adc_stream_irq(&s_nx_port_{name}); }}"]
        else:
            _check(body, f"nx_stm32_adc_initialize(&s_nx_port_{name})")
        stop_body = [f"static nx_result_t stop_{name}(void) {{"]
        if stream:
            stop_body += [f"    nx_result_t stopped = nx_adc_port_stream_stop(&s_nx_face_{name});",
                          "    if (stopped != NX_SUCCESS) { return stopped; }",
                          "    NVIC_DisableIRQ(DMA2_Stream4_IRQn);",
                          "    NVIC_ClearPendingIRQ(DMA2_Stream4_IRQn);"]
        stop_body += [f"    if ({adc}.active) {{ return NX_ERROR_BUSY; }}",
                      "    ADC1->CR2 = 0u;", f"    {adc}.initialized = false;"]
        if stream:
            stop_body.append("    RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;")
        declarations += stop_body + _release(item["pins"]) + [
                         "    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_ADC1EN;",
                         "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    else:
        raise ValueError(f"No maintained STM32 constructor for {kind}")
    body.append("    return status;")
    if captured:
        # Cold-path snapshots occupy the stack only. They never add a runtime
        # factory, resident pin table or hidden shutdown ownership context.
        pins = list(dict.fromkeys(captured))
        mask = sum(1 << (ord(pin[1]) - ord("A")) for pin in
                   {name[:2] for name in pins})
        locals_ = [f"    uint32_t saved_ahb1 = RCC->AHB1ENR & {mask}u;",
                   f"    uint32_t saved_apb1 = RCC->APB1ENR & {clock_bits['APB1ENR']};",
                   f"    uint32_t saved_apb2 = RCC->APB2ENR & {clock_bits['APB2ENR']};"]
        for pin in pins:
            locals_ += [f"    uint16_t saved_{pin} = 0u;",
                        f"    bool captured_{pin} = false;"]
        if kind == "spi":
            locals_ += ["    uint32_t saved_spi_cr1 = 0u, saved_spi_cr2 = 0u;",
                        "    bool captured_spi = false;"]
        body[2:2] = locals_
        body += ["rollback:"] + rollback_extra
        for pin in reversed(pins):
            body += [f"    if (captured_{pin}) {{",
                     f"        nx_stm32_pin_restore(GPIO{pin[1]}, {pin[2:]}u, saved_{pin});",
                     "    }"]
        body += [f"    RCC->APB1ENR = (RCC->APB1ENR & ~(uint32_t)({clock_bits['APB1ENR']})) | saved_apb1;",
                 f"    RCC->APB2ENR = (RCC->APB2ENR & ~(uint32_t)({clock_bits['APB2ENR']})) | saved_apb2;",
                 f"    RCC->AHB1ENR = (RCC->AHB1ENR & ~{mask}u) | saved_ahb1;",
                 "    nx_arch_dsb();", "    return status;"]
    body.append("}")
    return {"definitions": declarations + body,
            "initialize": f"prepare_{name}()", "stop": stop,
            "extra_headers": []}


def shared_irq_definitions(selections):
    """Emit one bounded static dispatcher per selected hardware vector."""
    selections = [controller_ir(item, "stm32f407") for item in selections]
    groups = {}
    for item in selections:
        if item["kind"] != "exti":
            continue
        line = int(item["controller"][4:])
        vector = "EXTI0" if line == 0 else "EXTI9_5"
        groups.setdefault(vector, []).append(item)
    lines = []
    for vector, items in groups.items():
        if len({item.options.irq.priority for item in items}) != 1:
            raise ValueError("Shared EXTI vector priorities must agree")
        pointers = ", ".join("&s_nx_port_" + _symbol(item["id"]) for item in items)
        array = "s_nx_irq_" + vector.lower() + "_ports"
        mask = sum(1 << int(item["controller"][4:]) for item in items)
        lines.extend([f"static nx_stm32_exti_state_t* const {array}[] = {{ {pointers} }};",
                      f"void {vector}_IRQHandler(void);",
                      "/** \\brief Static bounded EXTI shared-vector dispatch. */",
                      f"void {vector}_IRQHandler(void) {{",
                      f"    nx_stm32_exti_dispatch({array}, {len(items)}u, {mask}u);",
                      "}"])
    return lines


def _gpio_constructor(item):
    name = _symbol(item.id)
    port = item["pins"][0]["pin"][1]
    index = ord(port) - ord("A")
    mask = item.options.mask or sum(1 << int(pin["pin"][2:])
                                   for pin in item["pins"])
    initial = item.options.initial
    definitions = []
    definitions += [f'/* Fixed {item["controller"]} authorization. */',
               f'static nx_result_t prepare_{name}(void) {{',
               f'    s_nx_port_{name}.registers = GPIO{port};',
               f'    s_nx_port_{name}.mask = {mask}u;',
               f'    s_nx_port_{name}.output = {"true" if item["mode"] == "output" else "false"};',
               f'    uint32_t saved_clock = RCC->AHB1ENR & {1 << index}u;',
               f'    nx_result_t clock = nx_stm32_gpio_clock_enable({index}u);',
               f'    if (clock != NX_SUCCESS) {{ RCC->AHB1ENR = (RCC->AHB1ENR & ~{1 << index}u) | saved_clock; return clock; }}']
    for pin in item["pins"]:
        number = pin["pin"][2:]
        definitions.append(f'    uint16_t saved_{number} = nx_stm32_pin_capture(GPIO{port}, {number}u);')
    definitions += [f'    nx_result_t status = nx_stm32_gpio_initialize(&s_nx_port_{name}, {initial}u);',
               '    if (status != NX_SUCCESS) {']
    for pin in item["pins"]:
        number = pin["pin"][2:]
        definitions.append(f'        nx_stm32_pin_restore(GPIO{port}, {number}u, saved_{number});')
    definitions += [f'        RCC->AHB1ENR = (RCC->AHB1ENR & ~{1 << index}u) | saved_clock;',
               '        nx_arch_dsb();', '    }', '    return status;', '}']
    init = f'prepare_{name}()'
    return {"definitions": definitions, "initialize": init,
            "stop": [f"nx_stm32_gpio_stop(&s_nx_port_{name}, {initial}u)"]}


def _uart_constructor(item, result):
    name = _symbol(item.id)
    dma_uart = item.mode == "dma-tx"
    block_uart = item.mode == "irq-blocks"
    implementation = ("uart_dma" if dma_uart else
                      "uart_stream" if block_uart else "uart")
    options = item.options
    profile = ("NX_UART_RX_BLOCKS" if block_uart else
               "NX_UART_RX_EVENTS" if options.rx_profile == "events" else
               "NX_UART_RX_BYTES")
    storage = ("nx_uart_rx_event_t" if options.rx_profile == "events" else
               "uint8_t")
    capacity = options.rx_capacity or 0
    definitions = []
    if not block_uart:
        definitions.append(f"static {storage} s_nx_rx_{name}[{capacity}];")
    uart_state = f's_nx_port_{name}' + ('.uart' if dma_uart or block_uart else '')
    rx_storage = 'NULL' if block_uart else f's_nx_rx_{name}'
    definitions += [f'static nx_result_t prepare_{name}(void) {{',
               f'    {uart_state}.registers = {item["controller"]};',
               f'    {uart_state}.rx_storage = {rx_storage};',
               f'    {uart_state}.rx_capacity = {capacity}u;',
               f'    {uart_state}.baud = {item.options.baud}u;',
               f'    {uart_state}.profile = {profile};',
               f'    {uart_state}.irq = {item["controller"]}_IRQn;',
               '    uint32_t saved_clock = RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN;',
               '    uint32_t saved_uart_clock = RCC->APB2ENR & RCC_APB2ENR_USART1EN;',
               '    nx_result_t clock = nx_stm32_gpio_clock_enable(0u);',
               '    if (clock != NX_SUCCESS) {',
               '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock;',
               '        return clock;', '    }',
               '    uint16_t saved_tx = nx_stm32_pin_capture(GPIOA, 9u);',
               '    uint16_t saved_rx = nx_stm32_pin_capture(GPIOA, 10u);',
               '    nx_result_t pins = nx_stm32_uart1_pins_prepare();',
               f'    nx_result_t uart = pins == NX_SUCCESS ? nx_stm32_{implementation}_initialize(&s_nx_port_{name}, 84000000u) : pins;',
               '    if (uart != NX_SUCCESS) {',
               '        nx_stm32_pin_restore(GPIOA, 9u, saved_tx);',
               '        nx_stm32_pin_restore(GPIOA, 10u, saved_rx);',
               '        RCC->APB2ENR = (RCC->APB2ENR & ~(uint32_t)RCC_APB2ENR_USART1EN) | saved_uart_clock;',
               '        RCC->AHB1ENR = (RCC->AHB1ENR & ~(uint32_t)RCC_AHB1ENR_GPIOAEN) | saved_clock;',
               '        nx_arch_dsb();', '        return uart;', '    }',
               f'    NVIC_SetPriority({uart_state}.irq, {item.options.irq.priority}u);',
               '    return NX_SUCCESS;', '}',
               f'static nx_result_t stop_{name}(void) {{',
               f'    nx_result_t status = nx_uart_port_stop(&s_nx_face_{name});',
               '    if (status != NX_SUCCESS) { return status; }',
               '    (void)nx_stm32_pin_configure(GPIOA, 9u, 0u, 0u, 0u, false);',
               '    (void)nx_stm32_pin_configure(GPIOA, 10u, 0u, 0u, 0u, false);',
               '    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_USART1EN;',
               '    nx_arch_dsb();', '    return NX_SUCCESS;', '}']
    init = f'prepare_{name}()'
    irq = item["controller"] + '_IRQHandler'
    irq_implementation = "uart_dma_uart" if dma_uart else "uart_stream" if block_uart else "uart"
    definitions += [f'void {irq}(void);', '/** \\brief Fixed single-controller IRQ dispatch. */',
               f'void {irq}(void) {{ nx_stm32_{irq_implementation}_irq(&s_nx_port_{name}); }}']
    if dma_uart:
        if item["controller"] != "USART1" or item.get("dma") != ("DMA2:stream7:channel4",) and item.get("dma") != ["DMA2:stream7:channel4"]:
            raise ValueError("UART DMA requires reviewed USART1 DMA2 stream7 channel4")
        regions = [region for region in result["memory"] if region["dma"]]
        region_lines = [f'static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{']
        for region in regions:
            region_lines.append(f'    {{ 0x{region["origin"]:08x}u, {region["size"]}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},')
        region_lines += [f'    {{ 0x{result["flash"]["origin"]:08x}u, {result["flash"]["size"]}u, NX_DMA_MEMORY_READ }}', '};']
        # Fixed DMA pointers and domains precede the ordinary UART
        # register preparation. The provider owns admission/drain.
        position = next(index for index, line in enumerate(definitions)
                        if line == f'static nx_result_t prepare_{name}(void) {{')
        definitions[position:position] = region_lines
        definitions[position + len(region_lines) + 1:position + len(region_lines) + 1] = [
            f'    s_nx_port_{name}.dma = DMA2;',
            f'    s_nx_port_{name}.tx = DMA2_Stream7;',
            f'    s_nx_port_{name}.regions = s_nx_dma_regions_{name};',
            f'    s_nx_port_{name}.region_count = {len(regions) + 1}u;',
            f'    s_nx_port_{name}.dma_irq = DMA2_Stream7_IRQn;',
            f'    s_nx_port_{name}.stream = 7u;',
            f'    s_nx_port_{name}.channel = 4u;']
        priority_line = next(index for index, line in enumerate(definitions)
                             if line == f'    NVIC_SetPriority({uart_state}.irq, {item.options.irq.priority}u);')
        definitions.insert(priority_line + 1, f'    NVIC_SetPriority(s_nx_port_{name}.dma_irq, {item.options.irq.priority}u);')
        definitions += ['void DMA2_Stream7_IRQHandler(void);',
                   '/** \\brief Fixed DMA stream terminal observation. */',
                   f'void DMA2_Stream7_IRQHandler(void) {{ nx_stm32_uart_dma_irq(&s_nx_port_{name}); }}']
    return {"definitions": definitions, "initialize": init,
            "stop": [f"stop_{name}()"]}


def constructor(item, result, board_bindings, routes, devices):
    """Emit one reviewed STM32 instance, including its rollback and stop."""
    item = controller_ir(item, "stm32f407")
    if item.kind == "gpio":
        return _gpio_constructor(item)
    if item.kind == "uart":
        return _uart_constructor(item, result)
    return _controller_constructor(item, result, board_bindings, routes, devices)
