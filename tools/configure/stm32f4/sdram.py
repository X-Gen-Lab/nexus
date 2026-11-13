"""One exact SDRAM endpoint: static assembly only; hardware algorithms stay C."""

from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer
from tools.configure import board_hardware


TIMING_NAMES = ("mrd", "xsr", "ras", "rc", "wr", "rp", "rcd")
PARAMETERS = frozenset((
    "bytes", "row_bits", "column_bits", "internal_banks", "width_bits",
    "cas", "sdclk_div", "read_pipe", "initial_refreshes", "max_sdclk_hz",
    "refresh_window_us", "powerup_us", "prepare_budget_us", "stop_budget_us",
    "timing",
))
PIN_MASKS = {"C": 0x000d, "D": 0xc703, "E": 0xff83,
             "F": 0xf83f, "G": 0x8133}
PINS = frozenset(f"P{port}{pin}" for port, bits in PIN_MASKS.items()
                 for pin in range(16) if bits & (1 << pin))


def _route(device, bindings, routes):
    binding = bindings.get(device["binding"])
    if binding is None:
        fail("SDRAM requires an explicit Board endpoint binding")
    route = routes[binding["route"]]
    if (route["id"] != "fmc-sdram-bank1-16" or
            route["controller"] != "FMC" or route["kind"] != "memory_window" or
            device["mode"] not in route["modes"] or len(route["pins"]) != 38 or
            {pin["pin"] for pin in route["pins"]} != PINS or
            any(pin["af"] != 12 for pin in route["pins"])):
        fail("SDRAM requires the exact reviewed Bank1 AF12 route")
    return binding, route


def resolve_endpoint(device, controller, *, authored_kind, bindings, routes,
                     claim, board_conditions, clock):
    """Admit one exact bank window without an independent bus owner."""
    if (set(device) != {"id", "controller", "driver", "binding", "mode", "parameters"} or
            authored_kind != "memory_window" or
            device["driver"] != "memory_window-endpoint" or
            device["mode"] not in {"cpu-window", "graphics-owned"} or
            controller["controller"] != "FMC" or
            controller["mode"] != "sdram-controller"):
        fail("SDRAM requires one explicit parameter-based bank window")
    binding, route = _route(device, bindings, routes)
    board_hardware.require_binding(binding, board_conditions)
    validate(device["parameters"], hclk_hz=clock["core_hz"])
    claim("ip:FMC/bank1/data-window", device["id"], controller["mode"], route["id"])


def complete_controller(controller, children, *, claim, bindings, routes):
    """The single physical controller owns all endpoint pads through stop."""
    if len(children) != 1:
        fail("SDRAM requires exactly one explicit Bank1 endpoint")
    _, route = _route(children[0], bindings, routes)
    controller["pins"] = sorted(route["pins"],
        key=lambda pin: (pin["pin"][1], int(pin["pin"][2:])))
    for pin in controller["pins"]:
        claim("pin:" + pin["pin"], controller["id"], controller["mode"], route["id"])


def validate(parameters, *, hclk_hz):
    """Reject unrepresentable minima; preserve original explicit units in IR."""
    if not isinstance(parameters, dict) or set(parameters) != PARAMETERS:
        fail("SDRAM requires every explicit endpoint geometry and timing field")
    p = parameters
    integer(hclk_hz, 1, 180000000, "SDRAM selected HCLK")
    for name, expected in (("bytes", 0x800000), ("row_bits", 12),
                           ("column_bits", 8), ("internal_banks", 4),
                           ("width_bits", 16)):
        integer(p[name], expected, expected, "SDRAM " + name)
    integer(p["cas"], 1, 3, "SDRAM CAS")
    integer(p["sdclk_div"], 2, 3, "SDRAM clock divider")
    integer(p["read_pipe"], 0, 2, "SDRAM read pipe")
    integer(p["initial_refreshes"], 1, 16, "SDRAM initial refreshes")
    integer(p["max_sdclk_hz"], 1, 180000000, "SDRAM endpoint clock limit")
    if hclk_hz > p["max_sdclk_hz"] * p["sdclk_div"]:
        fail("SDRAM clock exceeds the explicit external endpoint maximum")
    for field in ("powerup_us", "prepare_budget_us", "stop_budget_us"):
        integer(p[field], 1, 1000000, "SDRAM " + field)
    integer(p["refresh_window_us"], 1, 0xffffffff, "SDRAM refresh window")
    if p["prepare_budget_us"] <= p["powerup_us"]:
        fail("SDRAM prepare budget must exceed the mandatory powerup dwell")
    timing = p["timing"]
    if not isinstance(timing, dict) or set(timing) != set(TIMING_NAMES):
        fail("SDRAM requires all seven explicit datasheet timing minima")
    cycles = {}
    denominator = p["sdclk_div"] * 1000000000
    for name in TIMING_NAMES:
        minimum = timing[name]
        if (not isinstance(minimum, dict) or
                set(minimum) != {"nanoseconds", "cycles"}):
            fail("SDRAM timing requires explicit nanoseconds and cycles")
        ns = integer(minimum["nanoseconds"], 0, 0xffffffff,
                     "SDRAM " + name + " nanoseconds")
        count = integer(minimum["cycles"], 0, 16,
                        "SDRAM " + name + " cycles")
        cycles[name] = max(count, (ns * hclk_hz + denominator - 1) // denominator)
        if not 1 <= cycles[name] <= 16:
            fail("SDRAM minimum cannot be represented by its controller field")
    cycles["rc"] = max(cycles["rc"], cycles["ras"] + cycles["rp"])
    cycles["wr"] = max(cycles["wr"], cycles["ras"] - cycles["rcd"],
                        cycles["rc"] - cycles["rcd"] - cycles["rp"])
    if cycles["rc"] > 16 or cycles["wr"] > 16:
        fail("SDRAM relational RC/WR timing cannot fit the controller fields")
    denominator = (1 << p["row_bits"]) * p["sdclk_div"] * 1000000
    refresh = p["refresh_window_us"] * hclk_hz // denominator - 20
    forbidden = sum(cycles[key] for key in ("wr", "rp", "rc", "rcd")) + 4
    if not 41 <= refresh <= 8191 or refresh == forbidden == 41:
        fail("SDRAM refresh cannot satisfy the minimum and forbidden COUNT")


def constructor(item, result):
    """Connect one exact window to the same exclusive FMC cold owner."""
    if (result.part != "STM32F479NIH6" or item.controller != "FMC" or
            item.mode != "sdram-controller" or item.dma or
            len(item.pins) != 38 or
            {pin["pin"] for pin in item.pins} != PINS or
            any(pin["af"] != 12 for pin in item.pins)):
        fail("SDRAM requires the reviewed exact F479 Bank1 software route")
    children = [child for child in result.devices if child.controller == item.id]
    if (len(children) != 1 or children[0].driver != "memory_window-endpoint" or
            children[0].options.mode not in {"cpu-window", "graphics-owned"}):
        fail("SDRAM requires one explicit endpoint on its owner")
    child = children[0]
    p = child.options.parameters
    graphics = child.options.mode == "graphics-owned"
    name = item.id.replace("-", "_")
    endpoint = child.id.replace("-", "_")
    owner = f"s_nx_port_{name}"
    config = f"s_nx_sdram_config_{name}"
    pins = f"s_nx_sdram_pins_{name}"
    saved = f"s_nx_sdram_saved_{name}"
    definitions = [f"static uint16_t {saved}[38u];",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    definitions += ["};", f"static const nx_stm32_sdram_config_t {config} = {{",
                    f"    .pins = {pins}, .pin_count = 38u,",
                    "    .system = &g_nx_stm32_system,", "    .timing = {"]
    for key in TIMING_NAMES:
        minimum = getattr(p.timing, key)
        definitions.append(f"        .{key} = {{ {minimum.nanoseconds}u, "
                           f"{minimum.cycles}u }},")
    definitions += ["    },", f"    .hclk_hz = {result.clock['core_hz']}u,"]
    if graphics:
        definitions.append("    .graphics_only = true,")
    for field in ("bytes", "refresh_window_us", "powerup_us", "prepare_budget_us",
                  "stop_budget_us", "row_bits", "column_bits", "internal_banks",
                  "width_bits", "cas", "sdclk_div", "read_pipe",
                  "initial_refreshes"):
        definitions.append(f"    .{field} = {getattr(p, field)}u,")
    definitions += ["};"]
    if graphics:
        definitions += [f"static nx_stm32_sdram_graphics_t "
                        f"s_nx_sdram_graphics_{name} = {{",
                        f"    .owner = &{owner}", "};"]
    definitions += [f"static const nx_memory_window_port_t s_nx_sdram_{endpoint} = {{",
                    f"    &nx_stm32_sdram_window_ops, &{owner}", "};",
                    f"const nx_memory_window_port_t* const nx_device_{endpoint} = "
                    f"&s_nx_sdram_{endpoint};"]
    arguments = (Address(owner), Address(config))
    return Construction(
        state_type="nx_stm32_sdram_t", definitions=definitions,
        admission=[DirectCall("nx_stm32_sdram_admit", arguments)],
        initialize=DirectCall("nx_stm32_sdram_start", arguments),
        stop=[DirectCall("nx_stm32_sdram_stop", (Address(owner),))],
        retain_on_failed_init=True, pin_snapshots=snapshots)
