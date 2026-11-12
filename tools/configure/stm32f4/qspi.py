"""Exact indirect serial endpoint; hardware transport/lifecycle remains C."""

from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer
from tools.configure import board_hardware
from tools.configure.contracts import SERIAL_FLASH_FIELDS

PARAMETERS = frozenset(("part", "prescaler", "cs_high_cycles",
                        "read_budget_us", "prepare_budget_us", "stop_budget_us"))
PINS = (("PB6", 10), ("PF10", 9), ("PF8", 10), ("PF9", 10))


def _route(device, bindings, routes):
    binding = bindings.get(device["binding"])
    if binding is None:
        fail("QSPI requires an explicit Board endpoint binding")
    route = routes[binding["route"]]
    if (route["id"] != "quadspi-bank1-sdr111" or
            route["controller"] != "QUADSPI" or route["kind"] != "flash_async" or
            "nor-indirect" not in route["modes"] or len(route["pins"]) != 4 or
            {(pin["pin"], pin["af"]) for pin in route["pins"]} != set(PINS)):
        fail("QSPI requires the reviewed four-pad Flash1 route")
    return binding, route


def resolve_endpoint(device, controller, *, authored_kind, bindings, routes,
                     claim, board_conditions, clock):
    """One serial endpoint selects physical geometry through its exact part."""
    if (set(device) != SERIAL_FLASH_FIELDS | {"id", "driver"} or
            authored_kind != "flash_async" or device["driver"] != "flash_async-endpoint" or
            device["mode"] != "nor-indirect" or controller["controller"] != "QUADSPI"):
        fail("QSPI requires one explicit indirect Flash endpoint")
    binding, route = _route(device, bindings, routes)
    board_hardware.require_binding(binding, board_conditions)
    validate(device["parameters"], hclk_hz=clock["core_hz"])
    claim("ip:QUADSPI/flash1", device["id"], controller["mode"], route["id"])


def complete_controller(controller, children, *, claim, bindings, routes):
    """One controller retains all four pins until its physical withdrawal."""
    if len(children) != 1:
        fail("QSPI requires exactly one selected Flash1 endpoint")
    _, route = _route(children[0], bindings, routes)
    controller["pins"] = sorted(route["pins"],
        key=lambda pin: (pin["pin"][1], int(pin["pin"][2:])))
    for pin in controller["pins"]:
        claim("pin:" + pin["pin"], controller["id"], controller["mode"], route["id"])


def validate(parameters, *, hclk_hz):
    """Keep the exact part, physical clock bound and explicit finite budgets."""
    if not isinstance(parameters, dict) or set(parameters) != PARAMETERS:
        fail("QSPI requires every explicit exact-part endpoint parameter")
    if parameters["part"] != "W25Q128JV":
        fail("QSPI supports only the reviewed W25Q128JV serial reference")
    integer(hclk_hz, 1, 180000000, "QSPI selected HCLK")
    prescaler = integer(parameters["prescaler"], 0, 255, "QSPI prescaler")
    cycles = integer(parameters["cs_high_cycles"], 1, 8, "QSPI NCS high cycles")
    if hclk_hz > 50000000 * (prescaler + 1):
        fail("QSPI READ03 exceeds the exact endpoint's 50 MHz maximum")
    if cycles * (prescaler + 1) * 1000000000 < hclk_hz * 50:
        fail("QSPI NCS high period is shorter than the required 50 ns")
    for field in ("read_budget_us", "prepare_budget_us", "stop_budget_us"):
        integer(parameters[field], 1, 1000000, "QSPI " + field)


def constructor(item, result):
    """Emit only static connections and explicit C admission/start/stop calls."""
    if (result.part != "STM32F479NIH6" or item.controller != "QUADSPI" or
            item.mode != "qspi-indirect-sdr" or item.dma or
            len(item.pins) != 4 or
            {(pin["pin"], pin["af"]) for pin in item.pins} != set(PINS)):
        fail("QSPI requires its exact F479 four-pad Flash1 serial route")
    children = [child for child in result.devices if child.controller == item.id]
    if (len(children) != 1 or children[0].driver != "flash_async-endpoint" or
            children[0].options.mode != "nor-indirect"):
        fail("QSPI requires one explicitly selected indirect Flash endpoint")
    child = children[0]
    p = child.options.parameters
    if p.part != "W25Q128JV":
        fail("QSPI constructor requires the same reviewed exact part")
    name = item.id.replace("-", "_")
    endpoint = child.id.replace("-", "_")
    owner = "s_nx_port_" + name
    config = "s_nx_qspi_config_" + name
    pins = "s_nx_qspi_pins_" + name
    saved = "s_nx_qspi_saved_" + name
    definitions = [f"static uint16_t {saved}[4u];",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, (wire, _) in enumerate(PINS):
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    definitions += ["};", f"static const nx_stm32_qspi_config_t {config} = {{",
                    f"    .pins = {pins}, .pin_count = 4u,",
                    "    .system = &g_nx_stm32_system,",
                    f"    .hclk_hz = {result.clock['core_hz']}u,"]
    for field in ("read_budget_us", "prepare_budget_us", "stop_budget_us",
                  "prescaler", "cs_high_cycles"):
        definitions.append(f"    .{field} = {getattr(p, field)}u,")
    definitions += ["};",
                    f"static const nx_flash_async_port_t s_nx_device_face_{endpoint} = {{",
                    f"    &nx_stm32_qspi_async_ops, &{owner}", "};",
                    f"const nx_flash_async_port_t* const nx_device_{endpoint} = "
                    f"&s_nx_device_face_{endpoint};",
                    f"static const nx_flash_port_t s_nx_flash_readonly_{endpoint} = {{",
                    f"    &nx_stm32_qspi_flash_ops, &{owner}", "};"]
    arguments = (Address(owner), Address(config))
    return Construction(state_type="nx_stm32_qspi_t", definitions=definitions,
        admission=[DirectCall("nx_stm32_qspi_admit", arguments)],
        initialize=DirectCall("nx_stm32_qspi_start", arguments),
        stop=[DirectCall("nx_stm32_qspi_stop", (Address(owner),))],
        retain_on_failed_init=True, pin_snapshots=snapshots)
