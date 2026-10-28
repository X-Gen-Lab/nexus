"""Emit exact host model storage and wiring, without runtime hardware policy."""
from tools.configure.ir import ControllerIR
from tools.configure.calls import Address, DirectCall, Symbol

from tools.configure.providers.common import Construction


def symbol(name):
    return name.replace("-", "_")


def constructor(item, result, board_bindings, routes, devices):
    """Construct one validated static instance using narrow provider calls."""
    del board_bindings, routes
    if not isinstance(item, ControllerIR) or item.provider != "native":
        raise ValueError("Native construction requires its resolved controller")
    name = symbol(item.id)
    kind = item.kind
    state = f"&s_nx_port_{name}"
    state_address = Address(f"s_nx_port_{name}")
    face_address = Address(f"s_nx_face_{name}")
    definitions = []
    stop = []
    if kind in {"rtc", "lptim"}:
        arguments = (state_address, Address("s_nx_native_time_clock"))
        if kind == "lptim":
            arguments += (item.options.tick_hz,)
        return Construction(
            state_type=f"nx_native_{kind}_state_t", definitions=[],
            initialize=DirectCall(f"nx_native_{kind}_prepare", arguments),
            stop=[DirectCall(f"nx_{kind}_port_stop", (face_address,))],
            retain_on_failed_init=False)
    if kind == "gpio":
        mask = item.mask or sum(1 << int(pin["pin"][2:])
                                   for pin in item.pins)
        initial = item.initial
        initialize = DirectCall("nx_native_gpio_configure_instance",
            (state_address, mask, initial, item.mode == "output"))
        stop = [DirectCall("nx_native_gpio_stop_instance",
                          (state_address, initial))]
    elif kind == "uart":
        capacity = item.options.rx_capacity
        if item.mode == "irq-words":
            return Construction(
                state_type="nx_native_uart_word_state_t",
                definitions=[f"static uint16_t s_nx_rx_{name}[{capacity}];"],
                initialize=DirectCall("nx_native_uart_word_initialize",
                    (state_address, Symbol(f"s_nx_rx_{name}"), capacity, None, 0)),
                stop=[DirectCall("nx_uart_word_port_stop", (face_address,))],
                retain_on_failed_init=False)
        events = item.options.rx_profile == "events"
        data_type = "nx_uart_rx_event_t" if events else "uint8_t"
        profile = "NX_UART_RX_EVENTS" if events else "NX_UART_RX_BYTES"
        definitions = [f"static {data_type} s_nx_rx_{name}[{capacity}];",
                       f"static const nx_native_uart_config_t s_nx_config_{name} = {{",
                       f"    .profile = {profile}, .rx_storage = s_nx_rx_{name},",
                       f"    .rx_capacity = {capacity}u, .automatic_irq = true",
                       "};"]
        initialize = DirectCall("nx_native_uart_configure_instance",
                                (state_address, Address(f"s_nx_config_{name}")))
        stop = [DirectCall("nx_uart_port_stop", (face_address,))]
    elif kind == "spi" and item.interface_kind == "spi_dma_transaction":
        sequence = f"s_nx_sequence_{name}"
        return Construction(
            state_type="nx_native_spi_dma_state_t",
            face_context="&" + sequence,
            face_definitions=[
                f"static nx_spi_dma_sequence_state_t {sequence} = {{",
                f"    .backend = &nx_native_spi_dma_sequence_backend, .context = {state}",
                "};"],
            definitions=[],
            initialize=DirectCall("nx_native_spi_dma_prepare",
                                  (state_address, Address(sequence))),
            stop=[DirectCall("nx_spi_dma_transaction_stop", (face_address,))],
            retain_on_failed_init=True)
    elif kind in {"spi", "i2c"}:
        endpoints = []
        for device in devices:
            if device.controller != item.id:
                continue
            child = symbol(device.id)
            memory = f"s_nx_model_{child}" if device.options.model_bytes is not None else "NULL"
            size = device.options.model_bytes or 0
            address = f", {device.options.address}u" if kind == "i2c" else ""
            endpoints.append(f"    {{ &s_nx_endpoint_{child}, {memory}, "
                             f"{size}u{address} }},")
        if endpoints:
            definitions = [
                f"static const nx_native_{kind}_endpoint_config_t "
                f"s_nx_endpoints_{name}[] = {{", *endpoints, "};"]
        initialize = DirectCall(f"nx_native_{kind}_prepare", (
            state_address, Symbol(f"s_nx_endpoints_{name}") if endpoints
            else None, len(endpoints)))
        stop = [DirectCall("nx_spi_port_stop", (face_address,)) if kind == "spi"
                else DirectCall("nx_native_i2c_stop_instance", (state_address,))]
    elif kind == "flash":
        flash = result.flash
        offset = 0
        definitions = [f"static uint8_t s_nx_model_flash_{name}[{flash['size']}];",
                       f"static const nx_flash_sector_t s_nx_sectors_{name}[] = {{"]
        for size in flash["erase_blocks"]:
            definitions.append(f"    {{ {offset}u, {size}u }},")
            offset += size
        definitions += ["};",
                        f"static const nx_flash_geometry_t s_nx_geometry_{name} = {{",
                        f"    {flash['origin']}u, {flash['size']}u, 1u,",
                        f"    s_nx_sectors_{name}, {len(flash['erase_blocks'])}u, 0u",
                        "};"]
        initialize = DirectCall("nx_native_flash_initialize_instance",
            (state_address, Symbol(f"s_nx_model_flash_{name}"),
             Address(f"s_nx_geometry_{name}")))
        stop = [DirectCall("nx_native_flash_stop_instance", (state_address,))]
    elif kind == "watchdog":
        initialize = DirectCall("nx_native_watchdog_initialize_instance",
                                (state_address,))
        # Independent watchdog effects intentionally survive platform stop.
    elif kind == "exti":
        capacity = item.options.event_capacity
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}[item.options.edge]
        line = int(item.pins[0]["pin"][2:])
        definitions = [f"static nx_exti_event_t s_nx_events_{name}[{capacity}];"]
        initialize = DirectCall("nx_native_exti_configure_instance",
            (state_address, line, Symbol(edge), Symbol(f"s_nx_events_{name}"),
             capacity))
        stop = [DirectCall("nx_exti_port_stop", (face_address,))]
    elif kind == "pwm":
        initialize = DirectCall("nx_native_pwm_prepare", (
            state_address, item.options.tick_hz, item.options.period_ticks,
            item.options.duty_ticks))
        stop = [DirectCall("nx_native_pwm_stop_instance", (state_address,))]
    elif kind == "adc":
        channels = item.options.channels
        definitions = [f"static const uint16_t s_nx_adc_model_{name}[{len(channels)}] = {{0}};"]
        initialize = DirectCall("nx_native_adc_configure_instance",
            (state_address, Symbol(f"s_nx_adc_model_{name}"), len(channels),
             12, item.options.reference_mv))
        stop = [DirectCall("nx_native_adc_stop_instance", (state_address,))]
    else:
        raise ValueError(f"Native construction is not maintained for {kind}")
    return Construction(
        state_type=f"nx_native_{kind}_state_t", definitions=definitions,
        initialize=initialize, stop=stop,
        retain_on_failed_init=kind in {"spi", "i2c"})
