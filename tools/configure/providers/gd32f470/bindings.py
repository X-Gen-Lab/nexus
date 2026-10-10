"""Reviewed GD32 fixed construction with no dynamic controller discovery."""
try:
    from ...emission import dma_regions
except ImportError:
    from emission import dma_regions
from ..common import controller_ir


def _symbol(value):
    return value.replace("-", "_")


def _integer(value, minimum, maximum, description):
    if (not isinstance(value, int) or isinstance(value, bool)
            or not minimum <= value <= maximum):
        raise ValueError(description)
    return value


def _pins(item, expected):
    actual = [(pin["pin"], pin["function"], pin["af"])
              for pin in item["pins"]]
    if actual != expected:
        raise ValueError("GD32 fixed provider requires its exact reviewed route")


def _endpoint_face(kind, name, state, implementation=None):
    implementation = implementation or kind
    return [f"static const nx_{kind}_endpoint_t s_nx_device_face_{name} = {{",
            f"    &nx_gd32_{implementation}_endpoint_ops, &{state}", "};",
            f"const nx_{kind}_endpoint_t* const nx_device_{name} = &s_nx_device_face_{name};"]


def prepared(name, pins, initialize, after=()):
    """Wire cold pin helpers and rollback around one validated constructor."""
    unique_pins = {pin["pin"]: (pin, mode, pull, drain, initial)
                   for pin, mode, pull, drain, initial in pins}
    mask = sum(1 << (ord(port) - ord("A"))
               for port in {pin[1] for pin in unique_pins})
    lines = [f"static nx_result_t prepare_{name}(void) {{",
             "    nx_result_t status = NX_SUCCESS;",
             f"    uint32_t saved_clocks = RCU_AHB1EN & {mask}u;"]
    for pin in unique_pins:
        lines += [f"    uint16_t saved_{pin} = 0u;", f"    bool captured_{pin} = false;"]
    for pin, mode, pull, drain, initial in unique_pins.values():
        wire = pin["pin"]
        lines += [f"    status = nx_gd32_gpio_clock_enable({ord(wire[1]) - ord('A')}u);",
                  "    if (status != NX_SUCCESS) { goto rollback; }",
                  f"    saved_{wire} = nx_gd32_pin_capture(GPIO{wire[1]}, {wire[2:]}u);",
                  f"    captured_{wire} = true;"]
        if initial is not None:
            bit = 1 << int(wire[2:])
            value = bit if initial else bit << 16
            lines.append(f"    GPIO_BOP(GPIO{wire[1]}) = {value}u;")
        lines += [f"    status = nx_gd32_pin_configure(GPIO{wire[1]}, {wire[2:]}u, {mode}u, {pin['af']}u, {pull}u, {'true' if drain else 'false'});",
                  "    if (status != NX_SUCCESS) { goto rollback; }"]
    lines += [f"    status = {initialize};", "    if (status != NX_SUCCESS) { goto rollback; }"]
    lines += list(after)
    lines += ["    return NX_SUCCESS;", "rollback:"]
    for wire in reversed(unique_pins):
        lines += [f"    if (captured_{wire}) {{ nx_gd32_pin_restore(GPIO{wire[1]}, {wire[2:]}u, saved_{wire}); }}"]
    lines += [f"    RCU_AHB1EN = (RCU_AHB1EN & ~{mask}u) | saved_clocks;",
              "    nx_arch_dsb();", "    return status;", "}"]
    return lines


def released(name, pins, stop, cs_pins=()):
    lines = [f"static nx_result_t stop_{name}(void) {{",
             f"    nx_result_t status = {stop};",
             "    if (status != NX_SUCCESS) { return status; }"]
    for pin in cs_pins:
        wire = pin["pin"]
        lines.append(f"    GPIO_BOP(GPIO{wire[1]}) = {1 << int(wire[2:])}u;")
    for pin in pins:
        wire = pin["pin"]
        lines.append(f"    (void)nx_gd32_pin_configure(GPIO{wire[1]}, {wire[2:]}u, 0u, 0u, 0u, false);")
    return lines + ["    return NX_SUCCESS;", "}"]


def _controller_constructor(item, result, board_bindings, routes, devices):
    """Emit only maintained controllers and explicit package-reviewed routes.

    The Liangshan SPI provider owns one built-in PF6 chip select. I2C children
    are fixed address facts sharing one controller. These silicon routes do not
    qualify any unmeasured PCB, external pull-up or peripheral device.
    """
    item = controller_ir(item, 'gd32f470')
    name = _symbol(item["id"])
    kind = item["kind"]
    declarations = []
    children = [child for child in devices
                if child["controller"] == item["id"]]
    extra_headers = []
    if kind == "uart":
        if item["controller"] not in {"USART0", "USART1"}:
            raise ValueError("GD32 maintains USART0/USART1 IRQ UART")
        name = _symbol(item["id"])
        blocks = item["mode"] == "irq-blocks"
        profile = 'NX_UART_RX_EVENTS' if item.options.rx_profile == "events" else 'NX_UART_RX_BYTES'
        storage = 'nx_uart_rx_event_t' if item.options.rx_profile == "events" else 'uint8_t'
        capacity = (item.options.rx_capacity or 0)
        if not blocks:
            declarations.append(f"static {storage} s_nx_rx_{name}[{capacity}];")
        descriptor = item["controller"].lower()
        dma = item["mode"] == "dma-tx"
        prefix = []
        if blocks:
            call = (f"nx_gd32_uart_stream_initialize_at(&s_nx_port_{name}, &nx_gd32_{descriptor}_controller, "
                    f"{item.options.baud}u, {item.options.irq.priority}u)")
        elif dma:
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            prefix = [f"    s_nx_port_{name}.regions = s_nx_dma_regions_{name};",
                      f"    s_nx_port_{name}.region_count = {region_count}u;"]
            call = (f"nx_gd32_uart_dma_initialize(&s_nx_port_{name}, "
                    f"{item.options.baud}u, {profile}, s_nx_rx_{name}, {capacity}u, {item.options.irq.priority}u)")
        else:
            call = (f"nx_gd32_uart_initialize_at(&s_nx_port_{name}, &nx_gd32_{descriptor}_controller, "
                    f"{item.options.baud}u, {profile}, s_nx_rx_{name}, {capacity}u, {item.options.irq.priority}u)")
        prepare = prepared(name, [(pin, 2, 1, False, None) for pin in item["pins"]], call)
        position = prepare.index(f"    status = {call};")
        prepare[position:position] = prefix
        declarations += prepare
        declarations += released(name, item["pins"], f"nx_uart_port_stop(&s_nx_face_{name})")
        vector = item["controller"] + "_IRQHandler"
        irq_implementation = "uart_dma_uart" if dma else "uart_stream" if blocks else "uart"
        declarations += [f"void {vector}(void);",
                         "/** \\brief Fixed generated UART controller dispatch. */",
                         f"void {vector}(void) {{ nx_gd32_{irq_implementation}_irq(&s_nx_port_{name}); }}"]
        if dma:
            declarations += ["void DMA1_Channel7_IRQHandler(void);",
                             "/** \\brief Exact GD USART0 TX DMA terminal observation. */",
                             f"void DMA1_Channel7_IRQHandler(void) {{ nx_gd32_uart_dma_irq(&s_nx_port_{name}); }}"]
        initialize = f"prepare_{name}()"
        stop = [f"stop_{name}()"]
    elif kind == "spi":
        dma = item["mode"] == "dma-full-duplex"
        implementation = "spi_dma" if dma else "spi"
        bus = f"s_nx_port_{name}" + (".spi" if dma else "")
        if item["controller"] not in {"SPI0", "SPI4"} or not children:
            raise ValueError("GD32 SPI requires maintained controller and explicit child")
        descriptor = item["controller"].lower()
        pin_settings = [(pin, 2, 0, False, None) for pin in item["pins"] if pin["function"] != "cs"]
        cs_pins = []
        after = []
        first_call = None
        prefix = []
        if dma:
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            prefix = [f"    s_nx_port_{name}.regions = s_nx_dma_regions_{name};",
                      f"    s_nx_port_{name}.region_count = {region_count}u;"]
        for index, child in enumerate(children):
            child_name = _symbol(child["id"])
            if child.get("cs_binding"):
                cs_route = routes[board_bindings[child["cs_binding"]]["route"]]
                cs_pin, = cs_route["pins"]
            else:
                cs_pin, = [pin for pin in item["pins"] if pin["function"] == "cs"]
            cs_pins.append(cs_pin)
            pin_settings.append((cs_pin, 1, 0, False, True))
            wire = cs_pin["pin"]
            cs_gpio = f"GPIO{wire[1]}"
            cs_mask = 1 << int(wire[2:])
            maximum = child.get("max_hz", item.options.max_hz)
            mode = child.get("mode", 0)
            endpoint = f"s_nx_endpoint_{name}" if index == 0 else f"s_nx_endpoint_{child_name}"
            if index:
                declarations.append(f"static nx_gd32_{implementation}_endpoint_state_t {endpoint};")
            declarations += _endpoint_face("spi", child_name, endpoint, implementation)
            base = endpoint + (".endpoint" if dma else "")
            # Every endpoint retains its own reviewed CS. No first-child bus alias.
            after += [f"    {base}.cs_gpio = {cs_gpio};", f"    {base}.cs_mask = {cs_mask}u;"]
            if dma:
                after.append(f"    {endpoint}.dma = &s_nx_port_{name};")
            if index == 0:
                if dma:
                    first_call = (f"nx_gd32_spi_dma_initialize(&s_nx_port_{name}, &{endpoint}, "
                                  f"{cs_gpio}, {cs_mask}u, {maximum}u, {mode}u, {item.options.irq.priority}u)")
                else:
                    first_call = (f"nx_gd32_spi_initialize_at(&s_nx_port_{name}, &{endpoint}, "
                                  f"&nx_gd32_{descriptor}_controller, {cs_gpio}, {cs_mask}u, {maximum}u, {mode}u)")
            else:
                after += [f"    status = nx_gd32_spi_endpoint_initialize(&{bus}, &{base}, {cs_gpio}, {cs_mask}u, {maximum}u, {mode}u);",
                          "    if (status != NX_SUCCESS) { return status; }"]
        prepare = prepared(name, pin_settings, first_call, after)
        position = prepare.index(f"    status = {first_call};")
        prepare[position:position] = prefix
        declarations += prepare
        pins = [pin for pin, *_ in pin_settings]
        stop_expression = (f"nx_spi_port_stop(nx_binding_{name})" if dma else
                           f"nx_gd32_spi_stop(&s_nx_port_{name})")
        declarations += released(name, pins, stop_expression, cs_pins)
        if dma:
            for channel, receive in ((3, "true"), (4, "false")):
                declarations += [f"void DMA1_Channel{channel}_IRQHandler(void);",
                                 "/** \\brief Exact selected GD SPI DMA terminal observation. */",
                                 f"void DMA1_Channel{channel}_IRQHandler(void) {{ nx_gd32_spi_dma_irq(&s_nx_port_{name}, {receive}); }}"]
        initialize = f"prepare_{name}()"
        stop = [f"stop_{name}()"]
    elif kind == "i2c":
        if item["controller"] not in {"I2C0", "I2C1"} or not children:
            raise ValueError("GD32 I2C requires maintained controller and explicit address")
        descriptor = item["controller"].lower()
        after = []
        for index, child in enumerate(children):
            child_name = _symbol(child["id"])
            endpoint = f"s_nx_endpoint_{name}" if index == 0 else f"s_nx_endpoint_{child_name}"
            if index:
                declarations.append(f"static nx_gd32_i2c_endpoint_state_t {endpoint};")
            declarations += _endpoint_face("i2c", child_name, endpoint)
            after += [f"    {endpoint}.port = &s_nx_port_{name};",
                      f"    {endpoint}.address = {child['address']}u;"]
        line_gpio = item["pins"][0]["pin"][1]
        line_mask = sum(1 << int(pin["pin"][2:]) for pin in item["pins"])
        call = (f"nx_gd32_i2c_initialize_at(&s_nx_port_{name}, &s_nx_endpoint_{name}, "
                f"&nx_gd32_{descriptor}_controller, {children[0]['address']}u, 100000u, GPIO{line_gpio}, {line_mask}u)")
        declarations += prepared(name, [(pin, 2, 1, True, None) for pin in item["pins"]], call, after)
        declarations += released(name, item["pins"], f"nx_gd32_i2c_stop(&s_nx_port_{name})")
        initialize = f"prepare_{name}()"
        stop = [f"stop_{name}()"]
    elif kind == "flash":
        if item["controller"] != "FMC" or item["pins"]:
            raise ValueError("GD32 maintains only its complete internal FMC")
        initialize = f"nx_gd32_flash_initialize(&s_nx_port_{name})"
        stop = [f"nx_gd32_flash_stop(&s_nx_port_{name})"]
    elif kind == "watchdog":
        if item["controller"] != "FWDGT" or item["pins"]:
            raise ValueError("GD32 maintains only independent FWDGT")
        initialize = f"nx_gd32_watchdog_initialize(&s_nx_port_{name})"
        stop = [f"nx_gd32_watchdog_stop(&s_nx_port_{name})"]
    elif kind == "exti":
        if item["controller"] != "EXTI3":
            raise ValueError("GD32 first-release route is EXTI3 PE3 only")
        _pins(item, [("PE3", "input", 0)])
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}.get(item.options.edge)
        if edge is None:
            raise ValueError("EXTI requires explicit rising/falling/both edge")
        capacity = _integer(item.options.event_capacity, 1, 4096,
                            "EXTI requires bounded event_capacity")
        priority = _integer(item.options.irq.priority, 0, 15,
                            "EXTI requires explicit IRQ priority")
        declarations.append(f"static nx_exti_event_t s_nx_events_{name}[{capacity}];")
        initialize = (f"nx_gd32_exti_initialize(&s_nx_port_{name}, 3u, 4u, {edge}, "
                      f"s_nx_events_{name}, {capacity}u, {priority}u)")
        stop = [f"nx_exti_port_stop(&s_nx_face_{name})"]
    elif kind == "pwm":
        if item["controller"] != "TIMER2":
            raise ValueError("GD32 maintains TIMER2 channel 0 only")
        _pins(item, [("PA6", "ch0", 2)])
        period = _integer(item.options.period_ticks, 1, 65535,
                          "PWM requires a positive 16-bit period")
        duty = _integer(item.options.duty_ticks, 0, period,
                        "PWM duty must be within the fixed period")
        tick = _integer(item.options.tick_hz, 1, 100000000,
                        "PWM requires an explicit tick_hz")
        if 100000000 % tick != 0 or not 1 <= 100000000 // tick <= 65536:
            raise ValueError("PWM tick_hz must exactly divide the 100 MHz timer")
        if item["initial"] != 0:
            raise ValueError("GD32 fixed PWM route supports inactive low only")
        declarations += [f"static nx_result_t prepare_{name}(void) {{",
                         f"    nx_result_t status = nx_gd32_pwm_initialize(&s_nx_port_{name}, {period}u, {tick}u);",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    return nx_pwm_port_set(&s_nx_face_{name}, {period}u, {duty}u);", "}"]
        initialize = f"prepare_{name}()"
        stop = [f"nx_gd32_pwm_release(&s_nx_port_{name})"]
    elif kind == "adc":
        stream = item["mode"] == "trigger-dma"
        if item["controller"] != "ADC0":
            raise ValueError("GD32 maintains ADC0 polling only")
        channels = list(item.options.channels)
        if channels not in ([0], [0, 1]):
            raise ValueError("GD32 maintained ADC routes use PA0 or PA0/PA1")
        _pins(item, [(f"PA{channel}", f"channel{channel}", 0)
                     for channel in channels])
        expected_sample = 1 if stream else 7
        if tuple(item.options.sample_times) != (expected_sample,) * len(channels):
            raise ValueError("GD32 ADC sampling differs from the selected provider contract")
        reference = _integer(item.options.reference_mv, 1, 3600,
                             "ADC requires explicit nominal reference_mv")
        timeout = _integer(item.options.timeout_ms, 1, 60000,
                           "ADC requires bounded startup timeout_ms")
        values = ", ".join(f"{channel}u" for channel in channels)
        declarations.append(f"static const uint8_t s_nx_adc_channels_{name}[] = {{ {values} }};")
        arguments = (f"&s_nx_port_{name}, s_nx_adc_channels_{name}, {len(channels)}u, "
                     f"{reference}u, nx_deadline_after(nx_time_now_us(), {timeout * 1000}u)")
        if stream:
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            call = f"nx_gd32_adc_stream_initialize({arguments}, {item.options.irq.priority}u)"
            declarations += [f"static nx_result_t prepare_{name}(void) {{",
                             f"    s_nx_port_{name}.regions = s_nx_dma_regions_{name};",
                             f"    s_nx_port_{name}.region_count = {region_count}u;",
                             f"    return {call};", "}",
                             "void DMA1_Channel0_IRQHandler(void);",
                             "/** \\brief Exact GD ADC DMA block terminal observation. */",
                             f"void DMA1_Channel0_IRQHandler(void) {{ nx_gd32_adc_stream_irq(&s_nx_port_{name}); }}"]
            initialize = f"prepare_{name}()"
            stop = [f"nx_gd32_adc_stream_stop(&s_nx_port_{name})"]
        else:
            initialize = f"nx_gd32_adc_initialize({arguments})"
            stop = [f"nx_gd32_adc_stop(&s_nx_port_{name})"]
    else:
        raise ValueError(f"No maintained GD32 constructor for {kind}")
    return {"definitions": declarations, "initialize": initialize,
            "stop": stop, "extra_headers": extra_headers}


def constructor(item, result, board_bindings, routes, devices):
    """Emit one reviewed GD32 instance, including explicit GPIO construction."""
    item = controller_ir(item, "gd32f470")
    if item.kind != "gpio":
        return _controller_constructor(item, result, board_bindings, routes,
                                       devices)
    name = _symbol(item.id)
    index = ord(item["pins"][0]["pin"][1]) - ord("A")
    mask = item.options.mask or sum(1 << int(pin["pin"][2:])
                                   for pin in item["pins"])
    initial = item.options.initial
    output = "true" if item.mode == "output" else "false"
    return {"definitions": [],
            "initialize": (f"nx_gd32_gpio_initialize(&s_nx_port_{name}, "
                           f"{index}u, {mask}u, {initial}u, {output})"),
            "stop": [f"nx_gd32_gpio_stop(&s_nx_port_{name}, {initial}u)"]}
