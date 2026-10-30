"""Reviewed STM32 fixed construction, never a runtime resource allocator."""
from tools.configure.ir import controller_ir
from tools.configure.calls import DirectCall, Address, Symbol
from tools.configure.memory import dma_regions, flash_geometry
from tools.configure import enhanced_endpoints, uart_formats
from tools.configure.providers.common import Construction, exact_part, mode_traits
from tools.configure.providers import hardware
from tools.configure import timer_advanced, secondary_controllers
from .contract import (ADC_CHANNEL_SEQUENCES, FLASH_PROGRAM_UNIT, LIMITS,
                       PARTS, mode_implementation)


def _symbol(value):
    return value.replace("-", "_")


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
    """Select immutable resources; concrete C owns every cold transition."""
    del board_bindings, routes, devices
    item = controller_ir(item, "stm32f407")
    name, kind = _symbol(item.id), item.kind
    config = f"s_nx_control_config_{name}"
    state = Address(f"s_nx_port_{name}")
    definitions = []
    if kind == "exti":
        if item.controller not in {"EXTI0", "EXTI5", "EXTI6"} or len(item.pins) != 1:
            raise ValueError("EXTI line must use a reviewed package route")
        line = int(item.controller[4:])
        wire = item.pins[0]["pin"]
        if int(wire[2:]) != line:
            raise ValueError("EXTI pin and mux line mismatch")
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}.get(item.options.edge)
        capacity, priority = item.options.event_capacity, item.options.irq.priority
        if edge is None or not isinstance(capacity, int) or not 1 <= capacity <= 4096:
            raise ValueError("EXTI requires an edge and bounded event_capacity")
        if not isinstance(priority, int) or not 0 <= priority <= 15:
            raise ValueError("EXTI requires a reviewed IRQ priority")
        definitions += [f"static nx_exti_event_t s_nx_events_{name}[{capacity}];",
                        f"static const nx_stm32_exti_config_t {config} = {{",
                        f"    .gpio = GPIO{wire[1]}, .storage = s_nx_events_{name},",
                        f"    .capacity = {capacity}u, .edge = {edge},",
                        f"    .gpio_index = {ord(wire[1]) - ord('A')}u,",
                        f"    .line = {line}u, .priority = {priority}u", "};"]
        initialize = DirectCall("nx_stm32_exti_prepare", (state, Address(config)))
        stop = DirectCall("nx_stm32_exti_close", (state,))
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
        if item.initial not in (0, 64):
            raise ValueError("PWM idle must be explicit PA6 masked output level")
        definitions += [f"static nx_stm32_gpio_state_t s_nx_idle_{name};",
                        f"static const nx_stm32_pwm_config_t {config} = {{",
                        f"    .period_ticks = {period}u, .duty_ticks = {duty}u,",
                        f"    .tick_hz = {tick}u, .prescaler = {timer_hz // tick - 1}u,",
                        f"    .inactive_high = {str(bool(item.initial)).lower()}", "};"]
        inactive = Address(f"s_nx_idle_{name}")
        initialize = DirectCall("nx_stm32_pwm_prepare", (state, inactive, Address(config)))
        stop = DirectCall("nx_stm32_pwm_close", (state, inactive))
    elif kind == "adc":
        traits = mode_traits(kind, item.mode)
        channels, sampling = item.options.channels, item.options.sample_times
        reference = item.options.reference_mv
        reviewed = tuple(int(pin["pin"][2:]) for pin in item.pins)
        if item.controller != "ADC1" or channels != reviewed or channels not in ADC_CHANNEL_SEQUENCES:
            raise ValueError("ADC channel sequence must match the reviewed PA0/PA1 route")
        if len(sampling) != len(channels) or not all(
                isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 7
                for value in sampling):
            raise ValueError("ADC requires one encoded sample_time (0..7) per channel")
        if not isinstance(reference, int) or not 1 <= reference <= 3600:
            raise ValueError("ADC requires a declared nominal reference voltage")
        channel_values = ", ".join(f"{value}u" for value in channels)
        sampling_values = ", ".join(f"{value}u" for value in sampling)
        definitions += [f"static const uint8_t s_nx_adc_channels_{name}[] = {{ {channel_values} }};",
                        f"static const uint8_t s_nx_adc_samples_{name}[] = {{ {sampling_values} }};"]
        config_type = "nx_stm32_adc_stream_config_t" if traits.dma else "nx_stm32_adc_config_t"
        if traits.dma:
            regions, count = dma_regions(name, result)
            definitions += regions
        definitions += [f"static const {config_type} {config} = {{"]
        if traits.dma:
            definitions += ["    .adc = {"]
        definitions += [f"    .channels = s_nx_adc_channels_{name},",
                        f"    .sample_times = s_nx_adc_samples_{name},",
                        f"    .channel_count = {len(channels)}u, .reference_mv = {reference}u"]
        if traits.dma:
            definitions += ["    },", f"    .regions = s_nx_dma_regions_{name},",
                            f"    .region_count = {count}u, .priority = {item.options.irq.priority}u"]
        definitions += ["};"]
        selected = "adc_continuous" if traits.continuous else "adc_stream" if traits.dma else "adc"
        initialize = DirectCall(f"nx_stm32_{selected}_prepare", (state, Address(config)))
        stop = DirectCall(f"nx_stm32_{selected}_close", (state,))
        if traits.dma:
            path = hardware.maintained("stm32f407", item.controller).dma(item.mode)[0]
            vector = hardware.dma_vector("stm32f407", path)
            definitions += [f"void {vector}_IRQHandler(void);",
                            r"/** \brief Exact selected ADC DMA block publisher. */",
                            f"void {vector}_IRQHandler(void) {{ nx_stm32_{selected}_irq(&s_nx_port_{name}); }}"]
    else:
        raise ValueError(f"No maintained STM32 constructor for {kind}")
    from .contract import mode_implementation
    return Construction(
        state_type=f"nx_stm32_{mode_implementation(item.kind, item.mode)}_state_t",
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=True)


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
    """Connect selected static UART facts to concrete typed C cold owners."""
    selected = item.integration
    if (selected is None or selected.hardware is None or
            selected.hardware.kind != "uart" or
            selected.hardware.controller != item.controller or
            selected.clock_hz is None or selected.irq is None or
            len(selected.pins) != 2):
        raise ValueError("UART requires its reviewed controller integration")
    name = _symbol(item.id)
    traits = mode_traits("uart", item.mode)
    tx_dma, rx_dma = traits.tx_dma, traits.rx_dma
    blocks = traits.block_stream
    words = item.mode == "irq-words"
    implementation = mode_implementation(item.kind, item.mode)
    cold_kind = "byte" if implementation == "uart" else implementation[5:]
    saved = f"s_nx_uart_pin_state_{name}"
    config = f"s_nx_uart_config_{name}"
    receipt = f"s_nx_uart_cold_{name}"
    options = item.options
    profile = ("NX_UART_RX_BLOCKS" if blocks else
               "NX_UART_RX_EVENTS" if options.rx_profile == "events" else
               "NX_UART_RX_BYTES")
    storage_type = ("uint16_t" if words else
                    "nx_uart_rx_event_t" if options.rx_profile == "events"
                    else "uint8_t")
    capacity = options.rx_capacity or 0
    definitions = [f"static uint16_t {saved}[2];",
                   f"static nx_stm32_uart_cold_receipt_t {receipt};"]
    if not blocks:
        definitions += [f"static {storage_type} s_nx_rx_{name}[{capacity}];"]
    definitions += [f"static const nx_stm32_platform_pin_config_t s_nx_uart_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(selected.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions += [f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},"]
    definitions += ["};"]
    vectors = [item.controller]
    binding = selected.hardware
    rx = tx = None
    if traits.dma:
        paths = secondary_controllers.dma_paths(item)
        expected = tuple((f"DMA{engine}", lane, request)
                         for engine, lane, request in paths)
        observed = tuple((lane.engine, lane.lane, lane.request)
                         for lane in selected.dma)
        if observed != expected:
            raise ValueError("UART DMA differs from its reviewed integration")
        region_lines, region_count = dma_regions(name, result,
                                                  include_flash=tx_dma)
        definitions += region_lines
        if rx_dma:
            rx = binding.dma("rx-dma-idle")[0]
            vectors.append(hardware.dma_vector("stm32f407", rx))
        if tx_dma:
            tx = binding.dma("dma-tx")[0]
            vectors.append(hardware.dma_vector("stm32f407", tx))
    definitions += [f"static const IRQn_Type s_nx_uart_vectors_{name}[] = {{",
                    "    " + ", ".join(vector + "_IRQn" for vector in vectors), "};"]
    frame = "NULL"
    if not words and uart_formats.changed(item):
        frame = "&s_nx_uart_frame_" + name
        definitions += [f"static const nx_uart_format_t s_nx_uart_frame_{name} =",
                        "    " + uart_formats.expression(options.frame) + ";"]
    config_type = ("nx_stm32_uart_dma_cold_config_t" if traits.dma else
                   "nx_stm32_uart_cold_config_t")
    definitions += [f"static const {config_type} {config} = {{"]
    indent = "        " if traits.dma else "    "
    if traits.dma:
        definitions += ["    .uart = {"]
    definitions += [indent + f".registers = {item.controller}, .pins = s_nx_uart_pins_{name},",
                    indent + f".vectors = s_nx_uart_vectors_{name}, .vector_count = {len(vectors)}u,",
                    indent + f".rx_storage = {'NULL' if blocks else 's_nx_rx_' + name}, .rx_capacity = {capacity}u,",
                    indent + f".clock_hz = {selected.clock_hz}u, .baud = {options.baud}u,",
                    indent + f".profile = {profile}, .priority = {selected.irq.priority}u,",
                    indent + f".kernel_bound = {'true' if result.kernel is not None else 'false'},",
                    indent + f".preload_tx = {'true' if secondary_controllers.applies(item, result, 'stm32') else 'false'}, .format = {frame}"]
    if traits.dma:
        engine = (rx or tx)[0]
        definitions += ["    },",
                        f"    .resources = {{ DMA{engine}, s_nx_dma_regions_{name}, {region_count}u }},",
                        f"    .receive = {hardware.dma_vector('stm32f407', rx) if rx else 'NULL'},",
                        f"    .continuous = {'true' if traits.continuous else 'false'},"]
        if tx:
            vector = hardware.dma_vector("stm32f407", tx)
            definitions += [f"    .transmit = {{ {vector}, {vector}_IRQn, {tx[1]}u, {tx[2]}u }}"]
    definitions += ["};"]
    irq_implementation = ("uart_hal" if implementation == "uart_hal" else
                          "uart_word" if words else
                          "uart_format" if uart_formats.changed(item) else
                          "uart_duplex" if rx_dma and tx_dma else
                          "uart_receive" if rx_dma else
                          "uart_dma_uart" if tx_dma else
                          "uart_stream" if blocks else "uart")
    irq_call = uart_formats.irq_call(
        item, f"nx_stm32_{irq_implementation}_irq(&s_nx_port_{name});")
    definitions += [f"void {item.controller}_IRQHandler(void);",
                    r"/** \brief Fixed selected UART publisher. */",
                    f"void {item.controller}_IRQHandler(void) {{ {irq_call} }}"]
    if rx:
        vector = hardware.dma_vector("stm32f407", rx)
        state = f"s_nx_port_{name}" + (".receive" if tx_dma else "")
        definitions += [f"void {vector}_IRQHandler(void);",
                        r"/** \brief Fixed selected RX-DMA publisher. */",
                        f"void {vector}_IRQHandler(void) {{ nx_stm32_uart_receive_dma_irq(&{state}); }}"]
    if tx:
        vector = hardware.dma_vector("stm32f407", tx)
        method = ("hal_dma" if implementation == "uart_hal" else
                  "duplex_dma" if rx_dma else "dma")
        definitions += [f"void {vector}_IRQHandler(void);",
                        r"/** \brief Fixed selected TX-DMA publisher. */",
                        f"void {vector}_IRQHandler(void) {{ nx_stm32_uart_{method}_irq(&s_nx_port_{name}); }}"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config), Address(receipt))
    return Construction(
        state_type=f"nx_stm32_{implementation}_state_t", definitions=definitions,
        initialize=DirectCall(f"nx_stm32_uart_{cold_kind}_cold_prepare", arguments),
        stop=[DirectCall(f"nx_stm32_uart_{cold_kind}_cold_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)

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
    if item.kind == "uart":
        return _uart_constructor(item, result)
    if item.kind in timer_advanced.KINDS:
        from .timer import constructor as timer_constructor
        return timer_constructor(item, result)
    if item.kind == "can":
        from .can import constructor as can_constructor
        return can_constructor(item, result)
    if item.kind in {"eth", "eth_frames", "usb_device"}:
        from .packet import constructor as packet_constructor
        return packet_constructor(item, result)
    if item.kind == "gpio":
        return _gpio_constructor(item)
    if item.kind in {"flash", "watchdog"}:
        return _basic_constructor(item, result)
    construction = _controller_constructor(item, result, board_bindings,
                                            routes, devices)
    return enhanced_endpoints.dma_construction(item, construction, "stm32")
