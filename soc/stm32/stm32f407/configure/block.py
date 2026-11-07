"""Static SDIO socket construction; concrete C owns discovery and transfers."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer


def validate(selection):
    """Require bounded CPU-polling limits without implicit media admission."""
    required = {"bus_width", "max_blocks", "poll_budget", "stop_timeout_us"}
    if not required.issubset(selection):
        fail("SDIO requires explicit bus_width and finite transfer/stop bounds")
    width = selection["bus_width"]
    if type(width) is not int or width not in {1, 4}:
        fail("SDIO bus_width must be 1 or 4")
    for field, maximum in (("max_blocks", 32), ("poll_budget", 1000000),
                            ("stop_timeout_us", 0xffffffff)):
        integer(selection[field], 1, maximum, "SDIO " + field)


def constructor(item, result):
    """Emit selected storage and fixed C calls, never an SD command recipe."""
    expected = ("PC8", "PC9", "PC10", "PC11", "PC12", "PD2")
    if (item.controller != "SDIO" or item.mode != "sdio-poll" or
            tuple(pin["pin"] for pin in item.pins) != expected or
            any(pin["af"] != 12 for pin in item.pins) or item.dma or
            result.clock_profile != "hse8-pll168"):
        fail("SDIO requires the reviewed six-pad F407 PLLQ polling socket")
    name = item.id.replace("-", "_")
    saved = f"s_nx_sdio_pin_state_{name}"
    receipt = f"s_nx_sdio_cold_{name}"
    config = f"s_nx_sdio_config_{name}"
    pins = f"s_nx_sdio_pins_{name}"
    options = item.options
    definitions = [f"static uint16_t {saved}[6];",
                   f"static nx_stm32_sdio_receipt_t {receipt};",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(
            f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, "
            f"{int(wire[2:])}u }},")
    definitions += ["};", f"static const nx_stm32_sdio_config_t {config} = {{",
                    f"    .registers = SDIO, .pins = {pins},",
                    f"    .max_blocks = {options.max_blocks}u,",
                    f"    .poll_budget = {options.poll_budget}u,",
                    f"    .stop_timeout_us = {options.stop_timeout_us}u,",
                    f"    .bus_width = {options.bus_width}u", "};"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config), Address(receipt))
    return Construction(
        state_type="nx_stm32_sdio_state_t", definitions=definitions,
        initialize=DirectCall("nx_stm32_sdio_prepare", arguments),
        stop=[DirectCall("nx_stm32_sdio_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)
