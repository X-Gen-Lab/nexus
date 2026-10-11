"""Emit exact host model storage and wiring, without runtime hardware policy."""

from ..common import controller_ir


def symbol(name):
    return name.replace("-", "_")


def constructor(item, result, board_bindings, routes, devices):
    """Construct one validated static instance using narrow provider calls."""
    del board_bindings, routes
    item = controller_ir(item, 'native')
    name = symbol(item["id"])
    kind = item["kind"]
    state = f"&s_nx_port_{name}"
    face = f"&s_nx_face_{name}"
    definitions = []
    stop = []
    if kind == "gpio":
        mask = item["mask"] or sum(1 << int(pin["pin"][2:])
                                   for pin in item["pins"])
        initial = item["initial"]
        output = "true" if item["mode"] == "output" else "false"
        initialize = (f"nx_native_gpio_configure_instance({state}, {mask}u, "
                      f"{initial}u, {output})")
        stop = [f"nx_native_gpio_stop_instance({state}, {initial}u)"]
    elif kind == "uart":
        capacity = item.options.rx_capacity
        events = item.options.rx_profile == "events"
        data_type = "nx_uart_rx_event_t" if events else "uint8_t"
        profile = "NX_UART_RX_EVENTS" if events else "NX_UART_RX_BYTES"
        definitions = [f"static {data_type} s_nx_rx_{name}[{capacity}];",
                       f"static const nx_native_uart_config_t s_nx_config_{name} = {{",
                       f"    .profile = {profile}, .rx_storage = s_nx_rx_{name},",
                       f"    .rx_capacity = {capacity}u, .automatic_irq = true",
                       "};"]
        initialize = (f"nx_native_uart_configure_instance({state}, "
                      f"&s_nx_config_{name})")
        stop = [f"nx_uart_port_stop({face})"]
    elif kind in {"spi", "i2c"}:
        definitions = [f"static nx_result_t prepare_{name}(void) {{",
                       f"    nx_result_t status = nx_native_{kind}_port_configure_instance({state});",
                       "    if (status != NX_SUCCESS) { return status; }"]
        for device in devices:
            if device["controller"] != item["id"]:
                continue
            child = symbol(device["id"])
            memory = f"s_nx_model_{child}" if "model_bytes" in device else "NULL"
            size = device.get("model_bytes", 0)
            address = f", {device['address']}u" if kind == "i2c" else ""
            definitions += [f"    status = nx_native_{kind}_configure_instance(",
                            f"        &s_nx_endpoint_{child}, {state}, {memory}, {size}u{address});",
                            "    if (status != NX_SUCCESS) { return status; }"]
        definitions += ["    return NX_SUCCESS;", "}"]
        initialize = f"prepare_{name}()"
        stop = [f"nx_spi_port_stop({face})" if kind == "spi"
                else f"nx_native_i2c_stop_instance({state})"]
    elif kind == "flash":
        flash = result["flash"]
        offset = 0
        definitions = [f"static uint8_t s_nx_model_flash_{name}[{flash['size']}];",
                       f"static const nx_flash_sector_t s_nx_sectors_{name}[] = {{"]
        for size in flash["erase_blocks"]:
            definitions.append(f"    {{ {offset}u, {size}u }},")
            offset += size
        definitions += ["};",
                        f"static const nx_flash_geometry_t s_nx_geometry_{name} = {{",
                        f"    {flash['origin']}u, {flash['size']}u, 1u,",
                        f"    s_nx_sectors_{name}, {len(flash['erase_blocks'])}u",
                        "};"]
        initialize = (f"nx_native_flash_initialize_instance({state}, "
                      f"s_nx_model_flash_{name}, &s_nx_geometry_{name})")
        stop = [f"nx_native_flash_stop_instance({state})"]
    elif kind == "watchdog":
        initialize = f"nx_native_watchdog_initialize_instance({state})"
        # Independent watchdog effects intentionally survive platform stop.
    elif kind == "exti":
        capacity = item.options.event_capacity
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}[item.options.edge]
        line = int(item["pins"][0]["pin"][2:])
        definitions = [f"static nx_exti_event_t s_nx_events_{name}[{capacity}];"]
        initialize = (f"nx_native_exti_configure_instance({state}, {line}u, "
                      f"{edge}, s_nx_events_{name}, {capacity}u)")
        stop = [f"nx_exti_port_stop({face})"]
    elif kind == "pwm":
        definitions = [f"static nx_result_t prepare_{name}(void) {{",
                       f"    nx_result_t status = nx_native_pwm_configure_instance({state},",
                       f"        {item.options.tick_hz}u, {item.options.period_ticks}u);",
                       "    if (status != NX_SUCCESS) { return status; }",
                       f"    return nx_pwm_port_set({face}, {item.options.period_ticks}u, {item.options.duty_ticks}u);",
                       "}"]
        initialize = f"prepare_{name}()"
        stop = [f"nx_native_pwm_stop_instance({state})"]
    elif kind == "adc":
        channels = item.options.channels
        definitions = [f"static const uint16_t s_nx_adc_model_{name}[{len(channels)}] = {{0}};"]
        initialize = (f"nx_native_adc_configure_instance({state}, "
                      f"s_nx_adc_model_{name}, {len(channels)}u, 12u, "
                      f"{item.options.reference_mv}u)")
        stop = [f"nx_native_adc_stop_instance({state})"]
    else:
        raise ValueError(f"Native construction is not maintained for {kind}")
    return {"definitions": definitions, "initialize": initialize, "stop": stop}
