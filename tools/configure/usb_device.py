"""Exact PIO FS directions, FIFO plan and explicit cold clock/pin leases.

This mechanism does not infer a Board connector, powered role, class protocol or
physical qualification. Selected endpoints are separate static typed identities.
"""
try:
    from .gpio_phase import withdraw_pin
    from .providers.common import (Construction, fail, integer,
                                   kernel_policy_constant)
except ImportError:
    from gpio_phase import withdraw_pin
    from providers.common import (Construction, fail, integer,
                                  kernel_policy_constant)


FIELDS = {"event_capacity", "rx_fifo_words", "vbus_sensing", "poll_limit",
          "drain_timeout_us", "clock_timeout_us", "irq_priority", "calls_os"}
ENDPOINT_FIELDS = {"id", "controller", "driver", "number", "direction",
                   "transfer", "max_packet", "fifo_words"}


def validate_soc(soc, provider):
    """Compare declared resources with the actual maintained SDK FS binding."""
    actual = {name for name, item in soc["controllers"].items()
              if item["kind"] == "usb_device"}
    if not actual:
        return
    expected = getattr(provider, "USB_CONTROLLER", None)
    if actual != {expected}:
        fail("USB controller differs from the maintained internal FS PHY")
    item = soc["controllers"][expected]
    if (item["modes"] != ["device-fs"] or item["irq"] != [expected] or
            item.get("irq_numbers") != [67] or item["pins"] != ["PA11", "PA12"] or
            any(type(item.get(key)) is not int or item[key] != value
                for key, value in (("clock_hz", 48000000),
                                   ("fifo_words", 320), ("endpoint_numbers", 4)))):
        fail("USB FS IRQ, endpoint, FIFO or clock facts differ from the SDK binding")
    required = ["clock:USBFS48"]
    if provider.PREFIX == "gd32":
        required += ["clock:CK48", "clock:PLLSAIP"]
    if item.get("resources") != required:
        fail("USB FS clock ownership differs from the maintained cold lease")


def validate_selection(selection, route, provider, ram_size):
    if not (FIELDS - {"calls_os"}).issubset(selection):
        fail("USB requires an explicit ring, FIFO, VBUS policy, finite budgets and IRQ")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 64),
            "USB event capacity")
    integer(selection["rx_fifo_words"], 64, 320, "USB RX FIFO words")
    for key in ("poll_limit", "drain_timeout_us", "clock_timeout_us"):
        integer(selection[key], 1, context="USB " + key)
    if type(selection["vbus_sensing"]) is not bool:
        fail("USB vbus_sensing requires an explicit boolean")
    if selection["vbus_sensing"]:
        fail("USB VBUS electrical route is not reviewed for this binding")
    if route["controller"] != getattr(provider, "USB_CONTROLLER", None) or [
            (pin["pin"], pin["function"], pin["af"]) for pin in route["pins"]] != [
                ("PA11", "dm", 10), ("PA12", "dp", 10)]:
        fail("USB FS requires its reviewed internal PHY PA11/PA12 AF10 route")


def endpoint(device):
    """Validate one direction before assembling the complete selected plan."""
    required = ENDPOINT_FIELDS - {"fifo_words"}
    if (not isinstance(device, dict) or not required.issubset(device) or
            set(device) - ENDPOINT_FIELDS):
        fail("USB endpoint requires exact number/direction/type/packet fields")
    if device["driver"] != "usb-fs-endpoint":
        fail("USB endpoint has no maintained FS packet provider")
    number = integer(device["number"], 0, 3, "USB endpoint number")
    direction = device["direction"]
    if not isinstance(direction, str) or direction not in {"in", "out"}:
        fail("USB endpoint direction must be explicitly in or out")
    transfer = device["transfer"]
    if not isinstance(transfer, str):
        fail("USB endpoint transfer requires an explicit type")
    maximum = integer(device["max_packet"], 1, 64, "USB packet bytes")
    if number == 0:
        valid = transfer == "control" and maximum == 64
    elif transfer == "bulk":
        valid = maximum in {8, 16, 32, 64}
    else:
        valid = transfer == "interrupt" and direction == "in"
    if not valid:
        fail("USB requires EP0 control/64, bulk 8/16/32/64 or "
             "IN interrupt 1..64")
    if direction == "in":
        integer(device.get("fifo_words"), 16, 320, "USB IN FIFO words")
    elif "fifo_words" in device:
        fail("USB OUT directions share RX FIFO and have no TX FIFO option")
    return {**device, "fifo_words": device.get("fifo_words", 0), "fifo_start": 0}


def complete_plan(selected, devices, claim):
    """Require both EP0 directions and resolve one nonoverlapping FIFO plan."""
    for controller in selected:
        if controller["kind"] != "usb_device":
            continue
        children = [item for item in devices if item["controller"] == controller["id"]]
        if not 2 <= len(children) <= 8:
            fail("USB requires exactly selected directions, including EP0 IN/OUT")
        keys = {(item["number"], item["direction"]) for item in children}
        if len(keys) != len(children) or not {(0, "in"), (0, "out")}.issubset(keys):
            fail("USB endpoint directions must be unique and include EP0 IN/OUT")
        cursor = controller["rx_fifo_words"]
        for item in sorted(children, key=lambda value: (value["number"], value["direction"])):
            if item["direction"] == "in":
                item["fifo_start"] = cursor
                cursor += item["fifo_words"]
        if cursor > 320:
            fail("USB RX and selected IN FIFOs exhaust the physical 320-word domain")
        for index, item in enumerate(children):
            item["endpoint_index"] = index
            claim("usb:" + controller["controller"] + ":endpoint:" +
                  str(item["number"]) + ":" + item["direction"], item["id"],
                  "device-fs", controller["route"])


def constructor(item, result, prefix):
    """Failed cold ownership remains counted until drain and reverse release."""
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    children = [child for child in result.devices if child.controller == item.id]
    options = item.options
    pins = item["pins"]
    lines = [f"static nx_{prefix}_usb_clock_state_t s_nx_usb_clock_{name};",
             f"static nx_usb_endpoint_t s_nx_usb_endpoints_{name}[{len(children)}];",
             f"static nx_usb_event_t s_nx_usb_events_{name}[{options.event_capacity}];",
             f"static uint16_t s_nx_usb_pin_snapshots_{name}[{len(pins)}];",
             f"static uint8_t s_nx_usb_pin_leases_{name};"]
    for child in children:
        identity = child.id.replace("-", "_")
        lines += [f"static const nx_usb_endpoint_port_t s_nx_device_face_{identity} = {{",
                  f"    &nx_usb_fs_endpoint_ops, &s_nx_usb_endpoints_{name}[{child.options.index}]",
                  "};", f"const nx_usb_endpoint_port_t* const nx_device_{identity} = &s_nx_device_face_{identity};"]
    lines += [f"static nx_result_t stop_{name}(void) {{",
              f"    nx_result_t status = nx_usb_device_stop(&s_nx_face_{name});",
              "    if (status != NX_SUCCESS) { return status; }",
              f"    status = nx_{prefix}_usb_clock_stop(&s_nx_usb_clock_{name},",
              f"        nx_deadline_after(nx_time_now_us(), {options.clock_timeout_us}u));",
              "    if (status != NX_SUCCESS) { return status; }"]
    for index, pin in reversed(list(enumerate(pins))):
        wire = pin["pin"]
        lines += [f"    if ((s_nx_usb_pin_leases_{name} & {1 << index}u) != 0u) {{"]
        lines += withdraw_pin(prefix, wire)
        lines += [f"        nx_{prefix}_pin_restore(GPIO{wire[1]}, {wire[2:]}u, s_nx_usb_pin_snapshots_{name}[{index}]);",
                  f"        if (nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u) != s_nx_usb_pin_snapshots_{name}[{index}]) {{",
                  "            return NX_ERROR_IO;", "        }",
                  f"        s_nx_usb_pin_leases_{name} &= (uint8_t)~{1 << index}u;", "    }"]
    lines += ["    return NX_SUCCESS;", "}", f"static nx_result_t prepare_{name}(void) {{",
              "    nx_result_t status;"]
    for child in children:
        option = child.options
        transfer = {
            "control": "NX_USB_PROFILE_CONTROL",
            "bulk": "NX_USB_PROFILE_BULK",
            "interrupt": "NX_USB_PROFILE_INTERRUPT",
        }[option.transfer]
        lines += [f"    s_nx_usb_endpoints_{name}[{option.index}] = (nx_usb_endpoint_t){{",
                  f"        .number = {option.number}u, .in = {str(option.direction == 'in').lower()},",
                  f"        .transfer = {transfer}, "
                  f".max_packet = {option.max_packet}u,",
                  f"        .fifo_start = {option.fifo_start}u, .fifo_words = {option.fifo_words}u", "    };"]
    policy = result["irq"]
    kernel = kernel_policy_constant(result)
    ceiling = policy["syscall_priority"] or 0
    lines += [f"    {state} = (nx_usb_controller_t){{",
              f"        .endpoints = s_nx_usb_endpoints_{name}, .endpoint_count = {len(children)}u,",
              f"        .events = s_nx_usb_events_{name}, .capacity = {options.event_capacity}u,",
              f"        .snapshot = {{ .epoch = {state}.snapshot.epoch }},",
              f"        .irq_policy = {{ {kernel}, {policy['external_count']}u, {policy['priority_bits']}u, {ceiling}u }},",
              f"        .irq = {{ .priority = {options.irq.priority}u }},",
              f"        .poll_limit = {options.poll_limit}u, .drain_timeout_us = {options.drain_timeout_us}u,",
              f"        .rx_fifo_words = {options.rx_fifo_words}u, .vbus_sensing = false", "    };"]
    for index, pin in enumerate(pins):
        wire = pin["pin"]
        lines += [f"    status = nx_{prefix}_gpio_clock_enable({ord(wire[1])-ord('A')}u);",
                  "    if (status != NX_SUCCESS) { return status; }",
                  f"    s_nx_usb_pin_snapshots_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u);",
                  f"    s_nx_usb_pin_leases_{name} |= {1 << index}u;",
                  f"    status = nx_{prefix}_pin_configure(GPIO{wire[1]}, {wire[2:]}u, 2u, {pin['af']}u, 0u, false);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += [f"    status = nx_{prefix}_usb_clock_start(&s_nx_usb_clock_{name}, true,",
              f"        nx_deadline_after(nx_time_now_us(), {options.clock_timeout_us}u));",
              "    if (status != NX_SUCCESS) { return status; }",
              f"    return nx_{prefix}_usb_device_initialize(&{state}, &s_nx_usb_clock_{name});", "}"]
    vector = "OTG_FS" if prefix == "stm32" else "USBFS"
    lines += [f"void {vector}_IRQHandler(void);",
              "/** \\brief Dispatch the selected internal-PHY FS PIO controller. */",
              f"void {vector}_IRQHandler(void) {{ nx_usb_controller_irq(&{state}); }}"]
    return Construction(
        state_type="nx_usb_controller_t", definitions=lines,
        initialize=f"prepare_{name}()", stop=[f"stop_{name}()"],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]:
                       f"s_nx_usb_pin_snapshots_{name}[{index}]"
                       for index, pin in enumerate(pins)})


def gpio_clock_mask(result):
    ports = {pin["pin"][1] for item in result.controllers
             if item.kind == "usb_device" for pin in item["pins"]}
    return sum(1 << (ord(port) - ord('A')) for port in ports)


def state_declarations(result, prefix):
    return (["static uint32_t s_nx_usb_gpio_clocks_added;"] if
            prefix == "gd32" and gpio_clock_mask(result) else [])


def start_resources(result, prefix):
    mask = gpio_clock_mask(result)
    # Capture before any other selected controller enables a shared GPIO gate.
    return ([f"    s_nx_usb_gpio_clocks_added = ~RCU_AHB1EN & {mask}u;"] if
            prefix == "gd32" and mask else [])


def stop_system(result, prefix):
    if prefix != "gd32" or not gpio_clock_mask(result):
        return []
    # Every selected port has drained before releasing this shared GPIO gate.
    return ["    if (s_nx_platform_started) {",
            "        RCU_AHB1EN &= ~s_nx_usb_gpio_clocks_added;",
            "        nx_arch_dsb();",
            "        if ((RCU_AHB1EN & s_nx_usb_gpio_clocks_added) != 0u) {",
            "            return NX_ERROR_IO;", "        }",
            "        s_nx_usb_gpio_clocks_added = 0u;", "    }"]
