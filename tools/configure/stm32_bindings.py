"""Reviewed STM32 fixed construction, never a runtime resource allocator."""


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


def constructor(item, result, board_bindings, routes, devices):
    """Emit static contexts after resolver resource and mode validation.

    No pin is inferred from a Board name. SPI/I2C child identities and CS/address
    facts must already be reviewed and claimed by the resolver. Any unsupported
    controller, channel or incomplete electrical/resource description fails.
    """
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
        if item["controller"] != "SPI1":
            raise ValueError("Only reviewed SPI1 is maintained")
        children = [child for child in devices
                    if child["controller"] == item["id"]]
        if not children:
            raise ValueError("STM32 SPI requires a reviewed CS endpoint")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_SPI1EN"
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;",
                 "    nx_arch_dsb();",
                 f"    s_{name}.registers = SPI1;",
                 f"    s_{name}.rcc = RCC;",
                 f"    s_{name}.clock_hz = 84000000u;",
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
            endpoint = f"s_{name}_endpoint" if index == 0 else f"s_{child_name}_endpoint"
            if index != 0:
                declarations.append(f"static nx_spi_endpoint_t {endpoint};")
            declarations += [f"static nx_gpio_port_t s_{child_name}_cs;",
                             f"const nx_spi_endpoint_t* const nx_device_{child_name} = &{endpoint};"]
            _check(body, f"nx_stm32_gpio_clock_enable({ord(cs_pin[1]) - ord('A')}u)")
            _capture(body, cs_pin)
            rollback_extra.append(f"    s_{child_name}_cs.initialized = false;")
            body += [f"    s_{child_name}_cs.registers = GPIO{cs_pin[1]};",
                     f"    s_{child_name}_cs.mask = {bit}u;",
                     f"    s_{child_name}_cs.output = true;"]
            _check(body, f"nx_stm32_gpio_initialize(&s_{child_name}_cs, {bit}u)")
            mode = child.get("mode", 0)
            maximum = child.get("max_hz", item.get("max_hz", 1000000))
            if not isinstance(mode, int) or isinstance(mode, bool) or not 0 <= mode <= 3:
                raise ValueError("SPI mode must be 0 through 3")
            if not isinstance(maximum, int) or maximum < 328125 or maximum > 42000000:
                raise ValueError("SPI1 maintained frequency is 328125 through 42000000 Hz")
            body += [f"    {endpoint}.port = &s_{name};",
                     f"    {endpoint}.cs = &s_{child_name}_cs;",
                     f"    {endpoint}.cs_mask = {bit}u;",
                     f"    {endpoint}.mode = {mode}u;",
                     f"    {endpoint}.frequency_hz = {maximum}u;"]
        stop_name = f"stop_{name}"
        stop_body = [f"static nx_result_t {stop_name}(void) {{",
                     f"    if (s_{name}.active) {{ return NX_ERROR_BUSY; }}",
                     "    SPI1->CR1 = 0u;"]
        for child in children:
            child_name = _symbol(child["id"])
            binding = board_bindings[child["cs_binding"]]
            stop_body += [f"    nx_result_t result_{child_name} = nx_stm32_gpio_stop(",
                          f"        &s_{child_name}_cs, {binding['initial']}u);",
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
        body += [f"    s_{name}.registers = I2C1;",
                 f"    s_{name}.peripheral_mhz = 42u;",
                 f"    s_{name}.rate_hz = 100000u;"]
        for index, child in enumerate(children):
            child_name = _symbol(child["id"])
            address = child["address"]
            if not 8 <= address <= 119:
                raise ValueError("Reserved I2C addresses are not maintained")
            endpoint = f"s_{name}_endpoint" if index == 0 else f"s_{child_name}_endpoint"
            if index != 0:
                declarations.append(f"static nx_i2c_endpoint_t {endpoint};")
            declarations += [f"const nx_i2c_endpoint_t* const nx_device_{child_name} = &{endpoint};"]
            body += [f"    {endpoint}.port = &s_{name};",
                     f"    {endpoint}.address = {address}u;"]
        _check(body, f"nx_stm32_i2c_initialize(&s_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    if (s_{name}.active) {{ return NX_ERROR_BUSY; }}",
                         "    I2C1->CR1 = 0u;",
                         f"    s_{name}.initialized = false;"] + _release(item["pins"]) + [
                         "    RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_I2C1EN;",
                         "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "flash":
        if item["controller"] != "FLASH":
            raise ValueError("Only the full internal Flash provider is maintained")
        geometry = "ve" if result["part"] == "STM32F407VET6" else "zg"
        body += [f"    s_{name}.registers = FLASH;",
                 f"    s_{name}.geometry = &g_nx_stm32_flash_{geometry};",
                 f"    s_{name}.memory = (volatile uint8_t*)FLASH_BASE;",
                 f"    s_{name}.supply_mv = 3300u;"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    return s_{name}.active || (FLASH->SR & FLASH_SR_BSY) != 0u",
                         "        ? NX_ERROR_BUSY : NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "watchdog":
        if item["controller"] != "IWDG":
            raise ValueError("Only independent IWDG is maintained")
        body += [f"    s_{name}.registers = IWDG;",
                 f"    s_{name}.flash = FLASH;", f"    s_{name}.debug = DBGMCU;",
                 f"    s_{name}.rcc = RCC;", f"    s_{name}.poll_limit = 1000000u;"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         "    nx_watchdog_state_t state;",
                         f"    nx_result_t result = nx_watchdog_port_state(&s_{name}, &state);",
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
                "both": "NX_EXTI_BOTH"}.get(item.get("edge"))
        capacity = item.get("event_capacity")
        priority = item.get("irq_priority")
        if edge is None or not isinstance(capacity, int) or not 1 <= capacity <= 4096:
            raise ValueError("EXTI requires an edge and bounded event_capacity")
        if not isinstance(priority, int) or not 0 <= priority <= 15:
            raise ValueError("EXTI requires a reviewed IRQ priority")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_SYSCFGEN"
        declarations.append(f"static nx_exti_event_t s_{name}_events[{capacity}];")
        body += [f"    if ((EXTI->IMR & {1 << line}u) != 0u) {{",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        _pin(body, pin, 0)
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;", "    nx_arch_dsb();",
                 f"    s_{name}.registers = EXTI;",
                 f"    s_{name}.gpio = GPIO{pin['pin'][1]};",
                 f"    s_{name}.storage = s_{name}_events;",
                 f"    s_{name}.capacity = {capacity}u;", f"    s_{name}.line = {line}u;",
                 f"    s_{name}.edge = {edge};"]
        _check(body, f"nx_stm32_exti_initialize(&s_{name}, SYSCFG, {ord(pin['pin'][1]) - ord('A')}u)")
        body += [f"    NVIC_SetPriority({irq}, {priority}u);",
                 f"    NVIC_ClearPendingIRQ({irq});", f"    NVIC_EnableIRQ({irq});"]
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    nx_result_t status = nx_exti_port_stop(&s_{name});",
                         "    if (status != NX_SUCCESS) { return status; }"] + _release(item["pins"]) + [
                         "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "pwm":
        if item["controller"] != "TIM3" or [pin["pin"] for pin in item["pins"]] != ["PA6"]:
            raise ValueError("Only reviewed TIM3 CH1 PA6 PWM route is maintained")
        period, duty, tick = (item.get(field) for field in
                              ("period_ticks", "duty_ticks", "tick_hz"))
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
        declarations.append(f"static nx_gpio_port_t s_{name}_idle;")
        body += ["    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;",
                 "    nx_arch_dsb();",
                 f"    if ((TIM3->CCER & 1u) != 0u || ((TIM3->CR1 & TIM_CR1_CEN) != 0u &&",
                 f"        (TIM3->PSC != {84000000 // tick - 1}u || TIM3->ARR != {period - 1}u))) {{",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        _check(body, "nx_stm32_gpio_clock_enable(0u)")
        _capture(body, "PA6")
        rollback_extra.append(f"    s_{name}_idle.initialized = false;")
        body += [f"    s_{name}_idle.registers = GPIOA;",
                 f"    s_{name}_idle.mask = 64u;", f"    s_{name}_idle.output = true;"]
        _check(body, f"nx_stm32_gpio_initialize(&s_{name}_idle, {inactive}u)")
        body += [f"    s_{name}.registers = TIM3;",
                 f"    s_{name}.inactive_gpio = &s_{name}_idle;",
                 f"    s_{name}.inactive_mask = 64u;",
                 f"    s_{name}.inactive_high = {'true' if inactive else 'false'};",
                 f"    s_{name}.channel = 1u;", f"    s_{name}.period_ticks = {period}u;",
                 f"    s_{name}.duty_ticks = {duty}u;", f"    s_{name}.tick_hz = {tick}u;",
                 f"    s_{name}.prescaler = {84000000 // tick - 1}u;"]
        _check(body, f"nx_stm32_pwm_initialize(&s_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    nx_result_t status = nx_pwm_port_stop(&s_{name});",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    s_{name}.initialized = false;",
                         f"    s_{name}_idle.initialized = false;",
                         "    if ((TIM3->CCER & 0x1111u) == 0u) {",
                         "        RCC->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM3EN;",
                         "    }", "    nx_arch_dsb();", "    return NX_SUCCESS;", "}"]
        stop = [f"stop_{name}()"]
    elif kind == "adc":
        channels = item.get("channels")
        reviewed = [int(pin["pin"][2:]) for pin in item["pins"]]
        if item["controller"] != "ADC1" or channels != reviewed or channels not in ([0], [0, 1]):
            raise ValueError("ADC channel sequence must match the reviewed PA0/PA1 route")
        sampling = item.get("sample_times")
        reference = item.get("reference_mv")
        if not isinstance(sampling, list) or len(sampling) != len(channels) or not all(
                isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 7
                for value in sampling):
            raise ValueError("ADC requires one encoded sample_time (0..7) per channel")
        if not isinstance(reference, int) or not 1 <= reference <= 3600:
            raise ValueError("ADC requires a declared nominal reference voltage")
        clock_bits["APB2ENR"] = "RCC_APB2ENR_ADC1EN"
        channel_values = ", ".join(f"{value}u" for value in channels)
        sampling_values = ", ".join(f"{value}u" for value in sampling)
        declarations += [f"static const uint8_t s_{name}_channels[] = {{ {channel_values} }};",
                         f"static const uint8_t s_{name}_sampling[] = {{ {sampling_values} }};"]
        body += ["    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;", "    nx_arch_dsb();",
                 "    if ((ADC1->CR2 & ADC_CR2_ADON) != 0u) {",
                 "        status = NX_ERROR_BUSY; goto rollback;", "    }"]
        for pin in item["pins"]:
            _pin(body, pin, 3)
        body += [
                 f"    s_{name}.registers = ADC1;", f"    s_{name}.common = ADC;",
                 f"    s_{name}.channels = s_{name}_channels;",
                 f"    s_{name}.sample_times = s_{name}_sampling;",
                 f"    s_{name}.channel_count = {len(channels)}u;", f"    s_{name}.reference_mv = {reference}u;"]
        _check(body, f"nx_stm32_adc_initialize(&s_{name})")
        declarations += [f"static nx_result_t stop_{name}(void) {{",
                         f"    if (s_{name}.active) {{ return NX_ERROR_BUSY; }}",
                         "    ADC1->CR2 = 0u;",
                         f"    s_{name}.initialized = false;"] + _release(item["pins"]) + [
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
    groups = {}
    for item in selections:
        if item["kind"] != "exti":
            continue
        line = int(item["controller"][4:])
        vector = "EXTI0" if line == 0 else "EXTI9_5"
        groups.setdefault(vector, []).append(item)
    lines = []
    for vector, items in groups.items():
        if len({item["irq_priority"] for item in items}) != 1:
            raise ValueError("Shared EXTI vector priorities must agree")
        pointers = ", ".join("&s_" + _symbol(item["id"]) for item in items)
        array = "s_" + vector.lower() + "_lines"
        mask = sum(1 << int(item["controller"][4:]) for item in items)
        lines.extend([f"static nx_exti_port_t* const {array}[] = {{ {pointers} }};",
                      f"void {vector}_IRQHandler(void);",
                      "/** \\brief Static bounded EXTI shared-vector dispatch. */",
                      f"void {vector}_IRQHandler(void) {{",
                      f"    nx_stm32_exti_dispatch({array}, {len(items)}u, {mask}u);",
                      "}"])
    return lines
