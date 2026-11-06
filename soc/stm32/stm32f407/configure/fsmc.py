"""One static F407 FSMC owner; concrete C owns all MMIO and bank behavior."""
from types import MappingProxyType

from tools.configure import board_hardware
from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol
from tools.configure.providers.common import Construction, fail, integer


BANKS = MappingProxyType({
    "memory_window": ("fsmc-ne3-is62wv51216", "cpu-window", 3),
    "parallel16": ("fsmc-ne4-ili9341", "8080-16", 4),
})
TIMING_FIELDS = frozenset(("address_setup", "address_hold", "data_setup",
                          "bus_turn"))


def _timing(value):
    if not isinstance(value, dict) or set(value) != TIMING_FIELDS:
        fail("FSMC timing requires all four encoded LL fields")
    for field, low, high in (("address_setup", 0, 15),
                             ("address_hold", 1, 15),
                             ("data_setup", 1, 255), ("bus_turn", 0, 15)):
        integer(value[field], low, high, "FSMC " + field)


def resolve_endpoint(device, controller, *, authored_kind, bindings, routes,
                     claim, board_conditions):
    """Admit an exact bank without making a second shared GPIO owner."""
    if (authored_kind not in BANKS or
            device["driver"] != authored_kind + "-endpoint"):
        fail("FSMC requires a reviewed memory_window or parallel16 endpoint")
    expected_route, mode, bank = BANKS[authored_kind]
    if authored_kind == "memory_window":
        mode = device["mode"]
        if mode not in {"cpu-window", "startup-memory"}:
            fail("Memory bank requires explicit cpu-window or "
                 "startup-memory mode")
    binding = bindings.get(device["binding"])
    if binding is None:
        fail("Unknown external bank Board binding")
    board_hardware.require_binding(binding, board_conditions)
    route = routes[binding["route"]]
    if (controller["controller"] != "FSMC" or
            route["controller"] != controller["controller"] or
            route["kind"] != authored_kind or route["id"] != expected_route or
            mode not in route["modes"] or
            any(pin["af"] != 12 for pin in route["pins"])):
        fail("External bank differs from its exact reviewed FSMC route")
    _timing(device["read_timing"])
    _timing(device["write_timing"])
    claim("ip:FSMC/bank" + str(bank), device["id"], controller["mode"],
          route["id"])
    if authored_kind == "memory_window":
        device.update(address=0x68000000, bytes=0x100000)
    else:
        device.update(command_address=0x6c000000, data_address=0x6c002000)


def complete_controller(controller, children, *, claim, bindings, routes):
    """Publish exactly one bus pin union after all banks have been admitted."""
    if not children:
        fail("External bus requires at least one explicit bank endpoint")
    selected = {}
    for child in children:
        route = routes[bindings[child["binding"]]["route"]]
        for pin in route["pins"]:
            previous = selected.get(pin["pin"])
            if previous is not None and previous != pin:
                fail("FSMC shared pad has inconsistent selected route facts")
            selected[pin["pin"]] = pin
    pins = [selected[name] for name in sorted(
        selected, key=lambda name: (name[1], int(name[2:])))]
    controller["pins"] = pins
    for pin in pins:
        claim("pin:" + pin["pin"], controller["id"], controller["mode"],
              controller["route"])


def _encoded(timing):
    return "{ " + ", ".join(str(getattr(timing, key)) + "u" for key in
                            ("address_setup", "address_hold", "data_setup",
                             "bus_turn")) + " }"


def _startup_endpoint(result):
    children = [child for child in result.devices
                if child.driver == "memory_window-endpoint" and
                child.options.mode == "startup-memory"]
    if len(children) > 1:
        fail("Only one exact SRAM startup bank is maintained")
    return children[0] if children else None


def boot_declarations(result):
    """Declare the same readonly bank array used by reset and runtime."""
    child = _startup_endpoint(result)
    if child is None:
        return []
    name = child.controller.replace("-", "_")
    return ["extern const nx_stm32_fsmc_bank_config_t "
            f"g_nx_fsmc_bank_config_{name}[];",
            "extern const nx_stm32_sram_boot_plan_t "
            "g_nx_stm32_sram_boot_plan;"]


def boot_bank_reference(result):
    """Return the exact immutable descriptor member, never a copied bank."""
    child = _startup_endpoint(result)
    if child is None:
        fail("External SRAM startup plan requires an explicit selected bank")
    children = [item for item in result.devices
                if item.controller == child.controller]
    index = children.index(child)
    return ("&g_nx_fsmc_bank_config_" + child.controller.replace("-", "_") +
            f"[{index}u]")


def excluded_pins(result):
    """Keep reset-owned SRAM pads outside the ordinary cold restore lease."""
    child = _startup_endpoint(result)
    if child is None:
        return ()
    route = result.routes[result.board_bindings[child.options.binding]["route"]]
    return tuple(pin["pin"] for pin in route["pins"])


def constructor(item, result):
    """Emit readonly faces and fixed C call order, with exact bank storage."""
    if (item.controller != "FSMC" or item.mode != "async16" or item.dma or
            result.part != "STM32F407ZGT6"):
        fail("FSMC requires the reviewed exact F407ZG assembly")
    name = item.id.replace("-", "_")
    children = [child for child in result.devices if child.controller == item.id]
    boot = any(child.driver == "memory_window-endpoint" and
               child.options.mode == "startup-memory" for child in children)
    retained = set(excluded_pins(result)) if boot else set()
    pins = "s_nx_fsmc_pins_" + name
    saved = "s_nx_fsmc_pin_state_" + name
    banks = ("g_nx_" if boot else "s_nx_") + "fsmc_bank_config_" + name
    runtime = "s_nx_fsmc_banks_" + name
    config = "s_nx_fsmc_config_" + name
    bus = "s_nx_port_" + name
    count = len(item.pins) - len(retained)
    definitions = ([f"static uint16_t {saved}[{count}u];"] if count else []) + [
        f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for pin in item.pins:
        wire = pin["pin"]
        slot = f"{saved}[{len(snapshots)}u]"
        address = "NULL" if wire in retained else "&" + slot
        if wire not in retained:
            snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, {address}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    storage = "" if boot else "static "
    definitions += ["};", f"{storage}const nx_stm32_fsmc_bank_config_t {banks}[] = {{"]
    for child in children:
        kind = ("NX_STM32_FSMC_SRAM_NE3" if child.driver == "memory_window-endpoint"
                else "NX_STM32_FSMC_LCD_NE4")
        definitions.append(f"    {{ {kind}, {_encoded(child.options.read_timing)}, "
                           f"{_encoded(child.options.write_timing)} }},")
    definitions += ["};", "static nx_stm32_fsmc_bank_state_t "
                    f"{runtime}[{len(children)}u];",
                    f"static const nx_stm32_fsmc_config_t {config} = {{",
                    f"    .pins = {pins}, .pin_count = {len(item.pins)}u,",
                    f"    .banks = {banks}, .bank_count = {len(children)}u,",
                    "    .clock = { nx_stm32_system_clock_read, &g_nx_stm32_system },",
                    f"    .boot_backed = {str(boot).lower()}", "};"]
    if boot:
        definitions += ["extern const nx_stm32_sram_boot_plan_t "
                        "g_nx_stm32_sram_boot_plan;"]
    arguments = (Address(bus), Address(config), Symbol(runtime), len(children))
    prepare = "nx_stm32_fsmc_adopt_boot" if boot else "nx_stm32_fsmc_prepare"
    if boot:
        arguments += (Address("g_nx_stm32_sram_boot_plan"),)
    start = [DirectCall(prepare, arguments)]
    stop = []
    for child in children:
        endpoint = child.id.replace("-", "_")
        if child.driver == "memory_window-endpoint":
            typename, context = "nx_memory_window_port_t", bus
            operations = "nx_stm32_fsmc_memory_window_ops"
        else:
            typename = "nx_parallel16_endpoint_t"
            context = "s_nx_fsmc_lcd_" + endpoint
            operations = "nx_stm32_fsmc_parallel16_ops"
            definitions.append(f"static nx_stm32_fsmc_lcd_t {context};")
            start.append(DirectCall("nx_stm32_fsmc_lcd_prepare", (
                Address(bus), Address(context))))
            stop.append(DirectCall("nx_stm32_fsmc_lcd_close", (Address(context),)))
        definitions += [f"static const {typename} s_nx_device_face_{endpoint} = {{",
                        f"    &{operations}, &{context}", "};",
                        f"const {typename}* const nx_device_{endpoint} = "
                        f"&s_nx_device_face_{endpoint};"]
    stop.append(DirectCall("nx_stm32_fsmc_stop", (Address(bus),)))
    sequence = InitializeSequence("prepare_fsmc_" + name, tuple(start))
    return Construction(state_type="nx_stm32_fsmc_t", definitions=definitions,
                        initialize_sequences=(sequence,),
                        initialize=DirectCall(sequence.function), stop=stop,
                        retain_on_failed_init=True, pin_snapshots=snapshots,
                        initial_state=(Symbol("NX_STM32_FSMC_BOOT_OWNER")
                                       if boot else None),
                        pre_c_effects=boot)
