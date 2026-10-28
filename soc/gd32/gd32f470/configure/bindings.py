"""Reviewed GD32 fixed construction with no dynamic controller discovery."""
from tools.configure.ir import controller_ir
from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.memory import dma_regions
from tools.configure import enhanced_endpoints, uart_formats
from tools.configure.providers.common import Construction, mode_traits
from tools.configure.providers import hardware
from tools.configure import can, usb_device, ethernet, timer_advanced, secondary_controllers
from .contract import ADC_CHANNEL_SEQUENCES, LIMITS


def _symbol(value):
    return value.replace("-", "_")


def _integer(value, minimum, maximum, description):
    if (not isinstance(value, int) or isinstance(value, bool)
            or not minimum <= value <= maximum):
        raise ValueError(description)
    return value


def _pins(item, expected):
    actual = [(pin["pin"], pin["function"], pin["af"])
              for pin in item.pins]
    if actual != expected:
        raise ValueError("GD32 fixed provider requires its exact reviewed route")


def _endpoint_face(kind, name, state, implementation=None):
    implementation = implementation or kind
    operations = ("nx_i2c_v1_async_endpoint_ops"
                  if implementation == "i2c_async" else
                  f"nx_gd32_{implementation}_endpoint_ops")
    return [f"static const nx_{kind}_endpoint_t s_nx_device_face_{name} = {{",
            f"    &{operations}, &{state}", "};",
            f"const nx_{kind}_endpoint_t* const nx_device_{name} = &s_nx_device_face_{name};"]


def prepared(name, pins, initialize, after=(), before=()):
    """Connect a concrete constructor to the C-owned pin transaction."""
    unique_pins = {pin["pin"]: (pin, mode, pull, drain, initial)
                   for pin, mode, pull, drain, initial in pins}
    if not 1 <= len(unique_pins) <= 16:
        raise ValueError("GD470 constructor requires one to sixteen selected pins")
    config = f"s_nx_cold_pins_{name}"
    lines = [f"static const nx_gd32_cold_pin_config_t {config}[] = {{"]
    for pin, mode, pull, drain, initial in unique_pins.values():
        if initial is not None:
            raise ValueError("GD470 UART/I2C pins have no output-latch policy")
        wire = pin["pin"]
        lines.append(
            f"    {{ GPIO{wire[1]}, {wire[2:]}u, {pin['af']}u, {mode}u, "
            f"{pull}u, {'true' if drain else 'false'} }},")
    lines += ["};", f"static nx_result_t prepare_{name}(void) {{",
             "    nx_result_t status = NX_SUCCESS;",
              f"    uint16_t saved_pins[{len(unique_pins)}] = {{0u}};",
              "    nx_gd32_cold_pins_receipt_t pins = {0u};",
              f"    status = nx_gd32_cold_pins_prepare({config}, "
              f"{len(unique_pins)}u, saved_pins, &pins);",
              "    if (status != NX_SUCCESS) { goto rollback; }"]
    lines += list(before)
    lines += [f"    status = {initialize};", "    if (status != NX_SUCCESS) { goto rollback; }"]
    lines += list(after)
    lines += ["    return NX_SUCCESS;", "rollback:",
              f"    (void)nx_gd32_cold_pins_rollback({config}, "
              f"{len(unique_pins)}u, saved_pins, &pins);",
              "    return status;", "}"]
    return lines


def released(name, pins, stop, cs_pins=()):
    if cs_pins:
        raise ValueError("GD470 SPI CS lifecycle belongs to its concrete adapter")
    lines = [f"static nx_result_t stop_{name}(void) {{",
             f"    nx_result_t status = {stop};",
             "    if (status != NX_SUCCESS) { return status; }"]
    return lines + [f"    return nx_gd32_cold_pins_release(s_nx_cold_pins_{name}, "
                    f"{len({pin['pin'] for pin in pins})}u);", "}"]


def _spi_constructor(item, result, board_bindings, routes, devices):
    """Emit static exact connections; GD470 C owns every SPI cold transition."""
    selected = item.integration
    if selected is None or selected.clock_hz != 100000000:
        raise ValueError("SPI requires its exact resolved APB2 integration")
    if item.controller not in {"SPI0", "SPI4"}:
        raise ValueError("GD470 SPI requires its reviewed SPI0/SPI4 identity")
    name = _symbol(item.id)
    dma = item.mode == "dma-full-duplex"
    implementation = "spi_dma" if dma else "spi"
    children = [child for child in devices if child.controller == item.id]
    pins = [pin for pin in item.pins if pin["function"] != "cs"]
    if len(pins) != 3 or not children:
        raise ValueError("SPI requires three signals and explicit CS children")
    state, config = f"s_nx_port_{name}", f"s_nx_spi_config_{name}"
    saved, cold = f"s_nx_spi_pin_state_{name}", f"s_nx_spi_cold_{name}"
    definitions = [f"static nx_gd32_{implementation}_cold_t {cold};"]
    endpoints = []
    for index, child in enumerate(children):
        child_name = _symbol(child.id)
        if child.options.cs_binding is None:
            cs_pin, = [pin for pin in item.pins if pin["function"] == "cs"]
        else:
            binding = board_bindings[child.options.cs_binding]
            cs_pin, = routes[binding["route"]]["pins"]
            if binding.get("initial") != 1 << int(cs_pin["pin"][2:]):
                raise ValueError("SPI active-low CS requires initial high")
        pins.append(cs_pin)
        endpoint = (f"s_nx_endpoint_{name}" if index == 0 else
                    f"s_nx_endpoint_{child_name}")
        if index:
            definitions.append(
                f"static nx_gd32_{implementation}_endpoint_state_t {endpoint};")
        definitions += enhanced_endpoints.face(
            child, endpoint, f"nx_gd32_{implementation}_endpoint_ops", "gd32f470")
        base = endpoint + (".endpoint" if dma else "")
        endpoints.append(
            f"    {{ &{base}, {child.options.max_hz}u, {child.options.mode}u }}")
    if len(pins) > 16:
        raise ValueError("GD470 static SPI lease supports at most sixteen pads")
    definitions += [f"static uint16_t {saved}[{len(pins)}];",
                    f"static const nx_gd32_spi_pin_config_t s_nx_spi_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(
            f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, "
            f"{int(wire[2:])}u, {pin['af']}u }},")
    definitions += ["};",
                    f"static const nx_gd32_spi_endpoint_config_t s_nx_spi_endpoints_{name}[] = {{",
                    ",\n".join(endpoints), "};"]
    if dma:
        if selected.irq is None or len(selected.dma) != 2:
            raise ValueError("SPI DMA requires both exact lanes and IRQ policy")
        secondary_controllers.dma_paths(item)
        regions, count = dma_regions(name, result, include_flash=True)
        definitions += regions
        definitions += [f"static const nx_gd32_spi_dma_config_t {config} = {{",
                        "    .spi = {"]
    else:
        definitions += [f"static const nx_gd32_spi_config_t {config} = {{"]
    definitions += [f"    .controller = &nx_gd32_{item.controller.lower()}_controller,",
                    f"    .pins = s_nx_spi_pins_{name}, .pin_count = {len(pins)}u,",
                    f"    .endpoints = s_nx_spi_endpoints_{name}, .endpoint_count = {len(children)}u"]
    if dma:
        definitions += ["    },",
                        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
                        f"    .priority = {selected.irq.priority}u,",
                        f"    .kernel_bound = {'true' if result.kernel is not None else 'false'}"]
    definitions += ["};"]
    arguments = (Address(state), Address(config), Address(cold))
    stop = []
    if dma:
        stop.append(DirectCall("nx_spi_port_stop", (Symbol(f"nx_binding_{name}"),)))
        for index, path in enumerate(hardware.maintained("gd32f470", item.controller).dma(item.mode)):
            vector = hardware.dma_vector("gd32f470", path)
            receive = "true" if index == 1 else "false"
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Exact selected GD SPI DMA terminal observation. */",
                            f"void {vector}_IRQHandler(void) {{ nx_gd32_spi_dma_irq(&{state}, {receive}); }}"]
    stop.append(DirectCall(f"nx_gd32_{implementation}_close", arguments))
    return Construction(
        state_type=f"nx_gd32_{implementation}_state_t", definitions=definitions,
        initialize=DirectCall(f"nx_gd32_{implementation}_prepare", arguments),
        stop=stop, retain_on_failed_init=True, pin_snapshots=snapshots)


def _i2c_constructor(item, result, devices):
    """Describe exact resources; concrete C owns every I2C cold transition."""
    selected = item.integration
    if (selected is None or selected.hardware is None or
            selected.hardware.kind != "i2c" or
            selected.hardware.controller != item.controller or
            selected.clock_hz != 50000000 or len(selected.pins) != 2):
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
                   f"static nx_gd32_i2c_cold_t {receipt};",
                   f"static const nx_gd32_cold_pin_config_t s_nx_i2c_pins_{name}[] = {{"]
    snapshots = {}
    pull = 0 if secondary_controllers.applies(item, result, "gd32") else 1
    for index, pin in enumerate(selected.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, {int(wire[2:])}u, {pin['af']}u, 2u, {pull}u, true }},")
    definitions += ["};"]
    if dma:
        regions, count = dma_regions(name, result, include_flash=True)
        definitions += regions
        definitions += [f"static const nx_i2c_v1_dma_t s_nx_i2c_dma_{name} =",
                        f"    NX_GD32_{item.controller}_DMA(s_nx_dma_regions_{name}, {count}u);"]
    children = [child for child in devices if child.controller == item.id]
    if not children:
        raise ValueError("I2C requires at least one reviewed address endpoint")
    definitions += [f"static const nx_gd32_i2c_config_t {config} = {{",
                    f"    .registers = {item.controller}, .pins = s_nx_i2c_pins_{name},",
                    f"    .dma = {'&s_nx_i2c_dma_' + name if dma else 'NULL'},",
                    f"    .snapshots = {saved}, .address = {children[0].options.address}u,",
                    f"    .priority = {selected.irq.priority if asynchronous else 0}u,",
                    f"    .asynchronous = {'true' if asynchronous else 'false'}", "};"]
    implementation = "i2c_async" if asynchronous else "i2c"
    for index, child in enumerate(children):
        child_name = _symbol(child.id)
        endpoint = (f"s_nx_endpoint_{name}" if index == 0 else
                    f"s_nx_endpoint_{child_name}")
        if index:
            definitions += [f"static nx_gd32_{implementation}_endpoint_state_t {endpoint} = {{",
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
            vector = f"{lane.engine}_Channel{lane.lane}"
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Fixed selected I2C DMA publisher. */",
                            f"void {vector}_IRQHandler(void) {{ nx_i2c_v1_dma_irq(&s_nx_port_{name}, {receive}); }}"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config),
                 Address(receipt))
    initialize_arguments = arguments if asynchronous else (
        Address(f"s_nx_port_{name}"), Address(f"s_nx_endpoint_{name}"),
        Address(config), Address(receipt))
    return Construction(
        state_type=f"nx_gd32_{implementation}_state_t",
        definitions=definitions,
        initialize=DirectCall(f"nx_gd32_{implementation}_prepare", initialize_arguments),
        stop=[DirectCall(f"nx_gd32_{implementation}_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)


def _controller_constructor(item, result, board_bindings, routes, devices):
    """Emit only maintained controllers and explicit package-reviewed routes.

    The Liangshan SPI provider owns one built-in PF6 chip select. I2C children
    are fixed address facts sharing one controller. These silicon routes do not
    qualify any unmeasured PCB, external pull-up or peripheral device.
    """
    item = controller_ir(item, 'gd32f470')
    name = _symbol(item.id)
    kind = item.kind
    declarations = []
    children = [child for child in devices
                if child.controller == item.id]
    if kind == "uart":
        if item.controller not in {"USART0", "USART1"}:
            raise ValueError("GD32 maintains USART0/USART1 IRQ UART")
        name = _symbol(item.id)
        traits = mode_traits(kind, item.mode)
        blocks, receive = traits.block_stream, traits.rx_dma
        words = item.mode == "irq-words"
        profile = 'NX_UART_RX_EVENTS' if item.options.rx_profile == "events" else 'NX_UART_RX_BYTES'
        storage = ('uint16_t' if words else 'nx_uart_rx_event_t'
                   if item.options.rx_profile == "events" else 'uint8_t')
        capacity = (item.options.rx_capacity or 0)
        if not blocks:
            declarations.append(f"static {storage} s_nx_rx_{name}[{capacity}];")
        descriptor = item.controller.lower()
        dma = traits.tx_dma
        prefix = []
        if words:
            call = (f"nx_gd32_uart_word_initialize(&s_nx_port_{name}, "
                    f"&nx_gd32_{descriptor}_controller, {item.options.baud}u, "
                    f"s_nx_rx_{name}, {capacity}u, {item.options.irq.priority}u)")
        elif receive:
            from .contract import _FIXED_DMA
            if tuple(item.dma) != _FIXED_DMA.get((item.controller, item.mode)):
                raise ValueError("UART DMA differs from its reviewed fixed route")
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            receive_state = f"s_nx_port_{name}" + (".receive" if dma else "")
            prefix = [f"    {receive_state}.resources.regions = s_nx_dma_regions_{name};",
                      f"    {receive_state}.resources.region_count = {region_count}u;",
                      f"    {receive_state}.continuous = {'true' if traits.continuous else 'false'};"]
            receive_impl = "uart_duplex" if dma else "uart_receive"
            call = (f"nx_gd32_{receive_impl}_initialize(&s_nx_port_{name}, "
                    f"{item.options.baud}u, {item.options.irq.priority}u)")
        elif blocks:
            call = (f"nx_gd32_uart_stream_initialize_at(&s_nx_port_{name}, &nx_gd32_{descriptor}_controller, "
                    f"{item.options.baud}u, {item.options.irq.priority}u)")
        elif dma:
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            prefix = [f"    s_nx_port_{name}.resources.regions = s_nx_dma_regions_{name};",
                      f"    s_nx_port_{name}.resources.region_count = {region_count}u;"]
            call = (f"nx_gd32_uart_dma_initialize(&s_nx_port_{name}, "
                    f"{item.options.baud}u, {profile}, s_nx_rx_{name}, {capacity}u, {item.options.irq.priority}u)")
        else:
            call = (f"nx_gd32_uart_initialize_at(&s_nx_port_{name}, &nx_gd32_{descriptor}_controller, "
                    f"{item.options.baud}u, {profile}, s_nx_rx_{name}, {capacity}u, {item.options.irq.priority}u)")
            if uart_formats.changed(item):
                call = (f"nx_gd32_uart_initialize_format_at(&s_nx_port_{name}, "
                        f"&nx_gd32_{descriptor}_controller, "
                        f"{item.options.baud}u, {profile}, s_nx_rx_{name}, "
                        f"{capacity}u, {item.options.irq.priority}u, "
                        f"{uart_formats.expression(item.options.frame)})")
        prepare = prepared(name, [(pin, 2, 1, False, None) for pin in item.pins], call)
        position = prepare.index(f"    status = {call};")
        prepare[position:position] = prefix
        if words or uart_formats.changed(item):
            rejected = prepare.index(f"    status = {call};") + 1
            prepare[rejected:rejected + 1] = [
                "    if (status != NX_SUCCESS) {",
                f"        if (s_nx_port_{name}.initialized) {{ return status; }}",
                "        goto rollback;", "    }"]
        declarations += prepare
        stop_function = "nx_uart_word_port_stop" if words else "nx_uart_port_stop"
        declarations += released(name, item.pins,
                                 f"{stop_function}(&s_nx_face_{name})")
        vector = item.controller + "_IRQHandler"
        irq_implementation = ("uart_word" if words else
                              "uart_format" if uart_formats.changed(item) else
                              "uart_duplex" if receive and dma else
                              "uart_receive" if receive else
                              "uart_dma_uart" if dma else
                              "uart_stream" if blocks else "uart")
        irq_call = uart_formats.irq_call(item, f"nx_gd32_{irq_implementation}_irq(&s_nx_port_{name});")
        declarations += [f"void {vector}(void);",
                         "/** \\brief Fixed generated UART controller dispatch. */",
                         f"void {vector}(void) {{ {irq_call} }}"]
        if receive:
            port_binding = hardware.maintained("gd32f470", item.controller)
            vector = hardware.dma_vector("gd32f470", port_binding.dma("rx-dma-idle")[0])
            declarations += [f"void {vector}_IRQHandler(void);",
                             "/** \\brief Exact selected GD DMA RX publisher. */",
                             f"void {vector}_IRQHandler(void) {{ nx_gd32_uart_receive_dma_irq(&{receive_state}); }}"]
        if dma:
            port_binding = hardware.maintained("gd32f470", item.controller)
            vector = hardware.dma_vector("gd32f470", port_binding.dma("dma-tx")[0])
            declarations += [f"void {vector}_IRQHandler(void);",
                             "/** \\brief Exact GD USART0 TX DMA terminal observation. */",
                             f"void {vector}_IRQHandler(void) {{ nx_gd32_uart_{'duplex_dma' if receive else 'dma'}_irq(&s_nx_port_{name}); }}"]
        initialize = DirectCall(f"prepare_{name}")
        stop = [DirectCall(f"stop_{name}")]
    elif kind == "flash":
        if item.controller != "FMC" or item.pins:
            raise ValueError("GD32 maintains only its complete internal FMC")
        initialize = DirectCall("nx_gd32_flash_initialize", (Address(f"s_nx_port_{name}"),))
        stop = [DirectCall("nx_gd32_flash_stop", (Address(f"s_nx_port_{name}"),))]
    elif kind == "watchdog":
        if item.controller != "FWDGT" or item.pins:
            raise ValueError("GD32 maintains only independent FWDGT")
        initialize = DirectCall("nx_gd32_watchdog_initialize", (Address(f"s_nx_port_{name}"),))
        stop = [DirectCall("nx_gd32_watchdog_stop", (Address(f"s_nx_port_{name}"),))]
    elif kind == "exti":
        if item.controller != "EXTI3":
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
        initialize = DirectCall("nx_gd32_exti_initialize", (
            Address(f"s_nx_port_{name}"), Address("s_nx_exti_domain"), 3, 4,
            Symbol(edge), Symbol(f"s_nx_events_{name}"), capacity, priority))
        # Admission can publish ownership before a terminal enable observation
        # fails. Cold rejection owns nothing; retained failure needs real stop.
        stop = [DirectCall("nx_gd32_exti_close", (Address(f"s_nx_port_{name}"),))]
    elif kind == "pwm":
        if item.controller != "TIMER2":
            raise ValueError("GD32 maintains TIMER2 channel 0 only")
        _pins(item, [("PA6", "ch0", 2)])
        period = _integer(item.options.period_ticks, 1, LIMITS["pwm_period"],
                          "PWM requires a positive 16-bit period")
        duty = _integer(item.options.duty_ticks, 0, period,
                        "PWM duty must be within the fixed period")
        timer_hz = LIMITS["timer_hz"]
        tick = _integer(item.options.tick_hz, 1, timer_hz,
                        "PWM requires an explicit tick_hz")
        if timer_hz % tick != 0 or not 1 <= timer_hz // tick <= 65536:
            raise ValueError(f"PWM tick_hz must exactly divide the {timer_hz} Hz timer")
        if item.initial != 0:
            raise ValueError("GD32 fixed PWM route supports inactive low only")
        declarations += [f"static nx_result_t prepare_{name}(void) {{",
                         f"    nx_result_t status = nx_gd32_pwm_initialize(&s_nx_port_{name}, {period}u, {tick}u);",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    return nx_pwm_port_set(&s_nx_face_{name}, {period}u, {duty}u);", "}"]
        initialize = DirectCall(f"prepare_{name}")
        stop = [DirectCall("nx_gd32_pwm_release", (Address(f"s_nx_port_{name}"),))]
    elif kind == "adc":
        traits = mode_traits(kind, item.mode)
        stream, continuous = traits.dma, traits.continuous
        stream_state = f"s_nx_port_{name}" + (".base" if continuous else "")
        if item.controller != "ADC0":
            raise ValueError("ADC0 is the maintained GD32 controller")
        channels = list(item.options.channels)
        if tuple(channels) not in ADC_CHANNEL_SEQUENCES:
            raise ValueError("GD32 maintained ADC routes use PA0 or PA0/PA1")
        _pins(item, [(f"PA{channel}", f"channel{channel}", 0)
                     for channel in channels])
        expected_sample = 1 if stream else 7
        if not continuous and tuple(item.options.sample_times) != (expected_sample,) * len(channels):
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
            path = hardware.maintained("gd32f470", item.controller).dma(item.mode)[0]
            vector = hardware.dma_vector("gd32f470", path)
            region_lines, region_count = dma_regions(name, result)
            declarations += region_lines
            if continuous:
                samples = ", ".join(f"{value}u" for value in item.options.sample_times)
                declarations.append(f"static const uint8_t s_nx_adc_samples_{name}[] = {{ {samples} }};")
            call = f"nx_gd32_adc_{'continuous' if continuous else 'stream'}_initialize({arguments}, {item.options.irq.priority}u)"
            declarations += [f"static nx_result_t prepare_{name}(void) {{",
                             f"    {stream_state}.regions = s_nx_dma_regions_{name};",
                             f"    {stream_state}.region_count = {region_count}u;",
                             f"    return {call};", "}",
                             f"void {vector}_IRQHandler(void);",
                             "/** \\brief Exact GD ADC DMA block terminal observation. */",
                             f"void {vector}_IRQHandler(void) {{ nx_gd32_adc_{'continuous' if continuous else 'stream'}_irq(&s_nx_port_{name}); }}"]
            if continuous:
                prepare = declarations.index(f"static nx_result_t prepare_{name}(void) {{")
                declarations.insert(prepare + 1, f"    s_nx_port_{name}.sample_times = s_nx_adc_samples_{name};")
            initialize = DirectCall(f"prepare_{name}")
            stop = [DirectCall(f"nx_gd32_adc_{'continuous' if continuous else 'stream'}_stop", (Address(f"s_nx_port_{name}"),))]
        else:
            initialize = DirectCall("nx_gd32_adc_initialize", (
                Address(f"s_nx_port_{name}"), Symbol(f"s_nx_adc_channels_{name}"),
                len(channels), reference, DirectCall("nx_deadline_after", (
                    DirectCall("nx_time_now_us"), timeout * 1000))))
            stop = [DirectCall("nx_gd32_adc_stop", (Address(f"s_nx_port_{name}"),))]
    else:
        raise ValueError(f"No maintained GD32 constructor for {kind}")
    from .contract import mode_implementation
    implementation = mode_implementation(item.kind, item.mode)
    return Construction(
        state_type=f"nx_gd32_{implementation}_state_t",
        definitions=declarations, initialize=initialize, stop=stop,
        retain_on_failed_init=(kind == "exti" or kind == "uart" and
                               (item.mode == "irq-words" or
                                uart_formats.changed(item))))


def constructor(item, result, board_bindings, routes, devices):
    """Emit one reviewed GD32 instance, including explicit GPIO construction."""
    item = controller_ir(item, "gd32f470")
    if item.kind == "i2c":
        return _i2c_constructor(item, result, devices)
    if item.kind == "spi":
        construction = _spi_constructor(item, result, board_bindings, routes, devices)
        return enhanced_endpoints.dma_construction(item, construction, "gd32")
    if secondary_controllers.applies(item, result, "gd32"):
        construction = secondary_controllers.constructor(item, result, "gd32")
        return enhanced_endpoints.dma_construction(item, construction, "gd32")
    if item.kind in timer_advanced.KINDS:
        return timer_advanced.constructor(item, result, "gd32")
    if item.kind == "can":
        return can.constructor(item, result, "gd32")
    if item.kind in {"eth", "eth_frames"}:
        return ethernet.constructor(item, result, "gd32")
    if item.kind == "usb_device":
        return usb_device.constructor(item, result, "gd32")
    if item.kind != "gpio":
        construction = _controller_constructor(
            item, result, board_bindings, routes, devices)
        return enhanced_endpoints.dma_construction(item, construction, "gd32")
    name = _symbol(item.id)
    index = ord(item.pins[0]["pin"][1]) - ord("A")
    mask = item.options.mask or sum(1 << int(pin["pin"][2:])
                                   for pin in item.pins)
    initial = item.options.initial
    return Construction(
        state_type="nx_gd32_gpio_state_t", definitions=[],
        initialize=DirectCall("nx_gd32_gpio_initialize", (
            Address(f"s_nx_port_{name}"), index, mask, initial,
            item.mode == "output")),
        stop=[DirectCall("nx_gd32_gpio_stop", (Address(f"s_nx_port_{name}"), initial))],
        retain_on_failed_init=False)
