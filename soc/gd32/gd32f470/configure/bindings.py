"""Reviewed GD32 fixed construction with no dynamic controller discovery."""
from tools.configure.ir import controller_ir
from tools.configure.calls import Address, DirectCall, Symbol
from tools.configure.memory import dma_regions
from tools.configure import enhanced_endpoints
from tools.configure.providers.common import Construction, mode_traits
from tools.configure.providers import hardware
from tools.configure import can, usb_device, ethernet, timer_advanced, secondary_controllers
from . import uart, control
from . import can as can_cold, timer as timer_cold, packet


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
    if kind == "flash":
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
    else:
        raise ValueError(f"No maintained GD32 constructor for {kind}")
    from .contract import mode_implementation
    implementation = mode_implementation(item.kind, item.mode)
    return Construction(
        state_type=f"nx_gd32_{implementation}_state_t",
        definitions=declarations, initialize=initialize, stop=stop,
        retain_on_failed_init=(kind == "exti"))


def constructor(item, result, board_bindings, routes, devices):
    """Emit one reviewed GD32 instance, including explicit GPIO construction."""
    item = controller_ir(item, "gd32f470")
    if item.kind == "uart":
        return uart.constructor(item, result)
    if item.kind in {"pwm", "adc"}:
        return control.constructor(item, result)
    if item.kind == "i2c":
        return _i2c_constructor(item, result, devices)
    if item.kind == "spi":
        construction = _spi_constructor(item, result, board_bindings, routes, devices)
        return enhanced_endpoints.dma_construction(item, construction, "gd32")
    if item.kind in timer_advanced.KINDS:
        return timer_cold.constructor(item, result)
    if item.kind == "can":
        return can_cold.constructor(item, result)
    if item.kind in {"eth", "eth_frames"}:
        return packet.constructor(item, result)
    if item.kind == "usb_device":
        return packet.constructor(item, result)
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
