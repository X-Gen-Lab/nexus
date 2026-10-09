"""Reviewed GD32 fixed construction with no dynamic controller discovery."""


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


def constructor(item, result, board_bindings, routes, devices):
    """Emit only maintained controllers and explicit package-reviewed routes.

    The Liangshan SPI provider owns one built-in PF6 chip select. I2C children
    are fixed address facts sharing one controller. These silicon routes do not
    qualify any unmeasured PCB, external pull-up or peripheral device.
    """
    del result
    name = _symbol(item["id"])
    kind = item["kind"]
    declarations = []
    children = [child for child in devices
                if child["controller"] == item["id"]]
    extra_headers = []
    if kind == "spi":
        if item["controller"] != "SPI4" or len(children) > 1:
            raise ValueError("GD32 maintains one SPI4 built-in PF6 endpoint")
        _pins(item, [("PF6", "cs", 0), ("PF7", "sck", 5),
                     ("PF8", "miso", 5), ("PF9", "mosi", 5)])
        maximum = item.get("max_hz", 1000000)
        mode = 0
        if children:
            child = children[0]
            mode = child.get("mode", 0)
            maximum = child.get("max_hz", maximum)
            # The provider's CS is already part of the SPI route. A separate
            # GPIO CS binding would imply unsupported arbitrary wiring.
            if child.get("cs_binding"):
                binding = board_bindings[child["cs_binding"]]
                cs_route = routes[binding["route"]]
                if ([pin["pin"] for pin in cs_route["pins"]] != ["PF6"]
                        or binding.get("initial") != 64):
                    raise ValueError("GD32 SPI4 supports active-low PF6 CS only")
            child_name = _symbol(child["id"])
            declarations += [
                f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_{name}_endpoint;"]
        maximum = _integer(maximum, 390625, 50000000,
                           "SPI4 maximum must be 390625 through 50000000 Hz")
        mode = _integer(mode, 0, 3, "SPI mode must be 0 through 3")
        initialize = (f"nx_gd32_spi_initialize(&s_{name}, &s_{name}_endpoint, "
                      f"{maximum}u, {mode}u)")
        stop = [f"nx_gd32_spi_stop(&s_{name})"]
    elif kind == "i2c":
        if item["controller"] != "I2C0" or item.get("max_hz", 100000) != 100000:
            raise ValueError("GD32 maintains only I2C0 standard-mode 100 kHz")
        _pins(item, [("PB6", "scl", 4), ("PB7", "sda", 4)])
        if not children:
            raise ValueError("I2C requires explicit reviewed address endpoints")
        first_address = None
        for index, child in enumerate(children):
            address = _integer(child["address"], 8, 119,
                               "Reserved I2C addresses are unsupported")
            child_name = _symbol(child["id"])
            if index == 0:
                first_address = address
                endpoint = f"s_{name}_endpoint"
            else:
                endpoint = f"s_{child_name}_endpoint"
                declarations.append(
                    f"static nx_i2c_endpoint_t {endpoint} = {{ &s_{name}, {address}u }};")
            declarations.append(
                f"const nx_i2c_endpoint_t* const nx_device_{child_name} = &{endpoint};")
        initialize = (f"nx_gd32_i2c_initialize(&s_{name}, &s_{name}_endpoint, "
                      f"{first_address}u, 100000u)")
        stop = [f"nx_gd32_i2c_stop(&s_{name})"]
    elif kind == "flash":
        if item["controller"] != "FMC" or item["pins"]:
            raise ValueError("GD32 maintains only its complete internal FMC")
        initialize = f"nx_gd32_flash_initialize(&s_{name})"
        stop = [f"nx_gd32_flash_stop(&s_{name})"]
    elif kind == "watchdog":
        if item["controller"] != "FWDGT" or item["pins"]:
            raise ValueError("GD32 maintains only independent FWDGT")
        initialize = f"nx_gd32_watchdog_initialize(&s_{name})"
        stop = [f"nx_gd32_watchdog_stop(&s_{name})"]
    elif kind == "exti":
        if item["controller"] != "EXTI3":
            raise ValueError("GD32 first-release route is EXTI3 PE3 only")
        _pins(item, [("PE3", "input", 0)])
        edge = {"rising": "NX_EXTI_RISING", "falling": "NX_EXTI_FALLING",
                "both": "NX_EXTI_BOTH"}.get(item.get("edge"))
        if edge is None:
            raise ValueError("EXTI requires explicit rising/falling/both edge")
        capacity = _integer(item.get("event_capacity"), 1, 4096,
                            "EXTI requires bounded event_capacity")
        priority = _integer(item.get("irq_priority"), 0, 15,
                            "EXTI requires explicit IRQ priority")
        declarations.append(f"static nx_exti_event_t s_{name}_events[{capacity}];")
        initialize = (f"nx_gd32_exti_initialize(&s_{name}, 3u, 4u, {edge}, "
                      f"s_{name}_events, {capacity}u, {priority}u)")
        stop = [f"nx_exti_port_stop(&s_{name})"]
    elif kind == "pwm":
        if item["controller"] != "TIMER2":
            raise ValueError("GD32 maintains TIMER2 channel 0 only")
        _pins(item, [("PA6", "ch0", 2)])
        period = _integer(item.get("period_ticks"), 1, 65535,
                          "PWM requires a positive 16-bit period")
        duty = _integer(item.get("duty_ticks"), 0, period,
                        "PWM duty must be within the fixed period")
        tick = _integer(item.get("tick_hz"), 1, 100000000,
                        "PWM requires an explicit tick_hz")
        if 100000000 % tick != 0 or not 1 <= 100000000 // tick <= 65536:
            raise ValueError("PWM tick_hz must exactly divide the 100 MHz timer")
        if item["initial"] != 0:
            raise ValueError("GD32 fixed PWM route supports inactive low only")
        declarations += [f"static nx_result_t prepare_{name}(void) {{",
                         f"    nx_result_t status = nx_gd32_pwm_initialize(&s_{name}, {period}u, {tick}u);",
                         "    if (status != NX_SUCCESS) { return status; }",
                         f"    return nx_pwm_port_set(&s_{name}, {period}u, {duty}u);", "}"]
        initialize = f"prepare_{name}()"
        stop = [f"nx_gd32_pwm_release(&s_{name})"]
    elif kind == "adc":
        if item["controller"] != "ADC0":
            raise ValueError("GD32 maintains ADC0 polling only")
        channels = item.get("channels")
        if channels not in ([0], [0, 1]):
            raise ValueError("GD32 maintained ADC routes use PA0 or PA0/PA1")
        _pins(item, [(f"PA{channel}", f"channel{channel}", 0)
                     for channel in channels])
        if item.get("sample_times") != [7] * len(channels):
            raise ValueError("GD32 fixed ADC sampling is 480 cycles (encoding 7)")
        reference = _integer(item.get("reference_mv"), 1, 3600,
                             "ADC requires explicit nominal reference_mv")
        timeout = _integer(item.get("timeout_ms"), 1, 60000,
                           "ADC requires bounded startup timeout_ms")
        values = ", ".join(f"{channel}u" for channel in channels)
        declarations.append(f"static const uint8_t s_{name}_channels[] = {{ {values} }};")
        initialize = (f"nx_gd32_adc_initialize(&s_{name}, s_{name}_channels, {len(channels)}u, "
                      f"{reference}u, nx_deadline_after(nx_time_now_us(), "
                      f"{timeout * 1000}u))")
        stop = [f"nx_gd32_adc_stop(&s_{name})"]
    else:
        raise ValueError(f"No maintained GD32 constructor for {kind}")
    return {"definitions": declarations, "initialize": initialize,
            "stop": stop, "extra_headers": extra_headers}
