"""Exact indirect serial endpoint; hardware transport/lifecycle remains C."""

from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer
from tools.configure import board_hardware
from tools.configure.contracts import SERIAL_FLASH_FIELDS
from tools.configure.memory import dma_regions
from types import MappingProxyType

PARAMETERS = frozenset(("part", "prescaler", "cs_high_cycles",
                        "read_budget_us", "prepare_budget_us", "stop_budget_us"))
PINS = (("PB6", 10), ("PF10", 9), ("PF8", 10), ("PF9", 10))
QUAD_PINS = PINS + (("PF7", 9), ("PF6", 9))
READ_PROFILES = MappingProxyType({
    "nor-indirect": ("W25Q128JV", "quadspi-bank1-sdr111", PINS),
    "nor-indirect-quad-read": ("W25Q128JVSIQ", "quadspi-bank1-sdr114", QUAD_PINS),
    "nor-mapped-cpu": ("W25Q128JVSIQ", "quadspi-bank1-mapped114", QUAD_PINS),
})


def read_profile(mode):
    """Select exact immutable protocol facts without a family-wide fallback."""
    if mode not in READ_PROFILES:
        fail("QSPI requires a reviewed exact read profile")
    return READ_PROFILES[mode]


def _route(device, bindings, routes):
    binding = bindings.get(device["binding"])
    if binding is None:
        fail("QSPI requires an explicit Board endpoint binding")
    route = routes[binding["route"]]
    _, expected_route, pins = read_profile(device["mode"])
    if (route["id"] != expected_route or
            route["controller"] != "QUADSPI" or route["kind"] !=
            ("mapped_storage" if device["mode"] == "nor-mapped-cpu" else "flash_async") or
            device["mode"] not in route["modes"] or len(route["pins"]) != len(pins) or
            {(pin["pin"], pin["af"]) for pin in route["pins"]} != set(pins)):
        fail("QSPI requires the exact pad route of its selected read profile")
    return binding, route


def resolve_endpoint(device, controller, *, authored_kind, bindings, routes,
                     claim, board_conditions, clock):
    """One serial endpoint selects physical geometry through its exact part."""
    mapped = device["mode"] == "nor-mapped-cpu"
    if (set(device) != SERIAL_FLASH_FIELDS | {"id", "driver"} or
            authored_kind != ("mapped_storage" if mapped else "flash_async") or
            device["driver"] != ("mapped_storage-endpoint" if mapped else "flash_async-endpoint") or
            (mapped and controller["mode"] != "qspi-indirect-sdr") or
            device["mode"] not in READ_PROFILES or controller["controller"] != "QUADSPI"):
        fail("QSPI requires one explicit indirect Flash endpoint")
    binding, route = _route(device, bindings, routes)
    board_hardware.require_binding(binding, board_conditions)
    validate(device["parameters"], hclk_hz=clock["core_hz"], mode=device["mode"])
    claim("ip:QUADSPI/flash1", device["id"], controller["mode"], route["id"])


def complete_controller(controller, children, *, claim, bindings, routes):
    """One controller retains all selected pads until physical withdrawal."""
    if len(children) != 1:
        fail("QSPI requires exactly one selected Flash1 endpoint")
    _, route = _route(children[0], bindings, routes)
    controller["pins"] = sorted(route["pins"],
        key=lambda pin: (pin["pin"][1], int(pin["pin"][2:])))
    for pin in controller["pins"]:
        claim("pin:" + pin["pin"], controller["id"], controller["mode"], route["id"])


def validate(parameters, *, hclk_hz, mode="nor-indirect"):
    """Keep the exact part, physical clock bound and explicit finite budgets."""
    expected = PARAMETERS | ({"read_slots"} if mode == "nor-mapped-cpu" else set())
    if not isinstance(parameters, dict) or set(parameters) != expected:
        fail("QSPI requires every explicit exact-part endpoint parameter")
    if mode == "nor-mapped-cpu":
        integer(parameters["read_slots"], 1, 8, "QSPI map read slots")
    expected_part, _, _ = read_profile(mode)
    if parameters["part"] != expected_part:
        fail("QSPI part must match the explicit reviewed read profile")
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
    children = [child for child in result.devices if child.controller == item.id]
    if (len(children) != 1 or children[0].driver not in {"flash_async-endpoint", "mapped_storage-endpoint"}):
        fail("QSPI requires one explicitly selected indirect Flash endpoint")
    child = children[0]
    expected_part, _, selected_pins = read_profile(child.options.mode)
    if (result.part != "STM32F479NIH6" or item.controller != "QUADSPI" or
            item.mode not in {"qspi-indirect-sdr", "qspi-indirect-dma"} or
            item.dma != (("DMA2:stream7:channel3",)
                         if item.mode == "qspi-indirect-dma" else ()) or
            len(item.pins) != len(selected_pins) or
            {(pin["pin"], pin["af"]) for pin in item.pins} != set(selected_pins)):
        fail("QSPI requires its exact F479 read-profile serial route")
    p = child.options.parameters
    if p.part != expected_part:
        fail("QSPI constructor requires the same reviewed exact part")
    name = item.id.replace("-", "_")
    endpoint = child.id.replace("-", "_")
    owner = "s_nx_port_" + name
    config = "s_nx_qspi_config_" + name
    pins = "s_nx_qspi_pins_" + name
    saved = "s_nx_qspi_saved_" + name
    mapped = child.driver == "mapped_storage-endpoint"
    composite = owner
    if mapped:
        owner += ".controller"
    definitions = []
    if mapped:
        map_owner = "s_nx_mapped_owner_" + name
        slots = "s_nx_mapped_slots_" + name
        map_config = "s_nx_mapped_config_" + name
        definitions += [f"static nx_mapped_storage_owner_t {map_owner};",
            f"static nx_mapped_storage_slot_t {slots}[{p.read_slots}u];",
            f"static const nx_stm32_qspi_mapped_config_t {map_config} = {{",
            f"    &{map_owner}, {slots}, {p.read_slots}u", "};"]
    dma_config = "NULL"
    if item.mode == "qspi-indirect-dma":
        regions, count = dma_regions(name, result, include_flash=True,
                                      engine="DMA2")
        definitions += regions
        dma_state = "s_nx_qspi_dma_" + name
        dma_config = "s_nx_qspi_dma_config_" + name
        definitions += [f"static nx_stm32_qspi_dma_t {dma_state};",
            f"static const nx_stm32_qspi_dma_config_t {dma_config} = {{",
            f"    &{dma_state}, s_nx_dma_regions_{name}, {count}u", "};"]
        dma_config = "&" + dma_config
    definitions += [f"static uint16_t {saved}[{len(selected_pins)}u];",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, (wire, _) in enumerate(selected_pins):
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    definitions += ["};", f"static const nx_stm32_qspi_config_t {config} = {{",
                    f"    .pins = {pins}, .pin_count = {len(selected_pins)}u,",
                    "    .system = &g_nx_stm32_system,",
                    f"    .dma = {dma_config},",
                    f"    .hclk_hz = {result.clock['core_hz']}u,"]
    if mapped:
        definitions.append(f"    .mapping = &{map_config},")
    for field in ("read_budget_us", "prepare_budget_us", "stop_budget_us",
                  "prescaler", "cs_high_cycles"):
        definitions.append(f"    .{field} = {getattr(p, field)}u,")
    if mapped:
        definitions += ["};",
            f"static const nx_mapped_storage_port_t s_nx_device_face_{endpoint} = {{",
            f"    &nx_stm32_qspi_mapped_ops, &{composite}", "};",
            f"const nx_mapped_storage_port_t* const nx_device_{endpoint} = "
            f"&s_nx_device_face_{endpoint};"]
    else:
        definitions += ["};",
                        f"static const nx_flash_async_port_t s_nx_device_face_{endpoint} = {{",
                        f"    &nx_stm32_qspi_async_ops, &{owner}", "};",
                        f"const nx_flash_async_port_t* const nx_device_{endpoint} = "
                        f"&s_nx_device_face_{endpoint};",
                        f"static const nx_flash_port_t s_nx_flash_readonly_{endpoint} = {{",
                        f"    &nx_stm32_qspi_flash_ops, &{owner}", "};"]
    arguments = (Address(owner), Address(config))
    return Construction(state_type="nx_stm32_qspi_mapped_t" if mapped else "nx_stm32_qspi_t",
        face_context="&" + composite if mapped else None, definitions=definitions,
        admission=[DirectCall("nx_stm32_qspi_admit", arguments)],
        initialize=DirectCall("nx_stm32_qspi_start", arguments),
        stop=[DirectCall("nx_stm32_qspi_stop", (Address(owner),))],
        retain_on_failed_init=True, pin_snapshots=snapshots)
