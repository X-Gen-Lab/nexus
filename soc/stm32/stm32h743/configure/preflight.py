"""Exact H743 selected C preflight calls; no generated hardware algorithm."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import fail

PREFLIGHT_CALL = 'nx_stm32h743_system_preflight(&g_nx_stm32h743_system)'


def _checked(call):
    return [f"    result.primary = {call.render()};",
            "    if (result.primary != NX_SUCCESS) { return result; }"]


def selected_preflight(result, *, dma_conditions=None):
    """Order read-only C admissions before any System clock effect."""
    del dma_conditions
    from .emission import _ports, dma_admission
    source = []
    lanes, _ = dma_admission(result)
    if lanes:
        spi_lanes = sum(1 << lane.lane for lane in result.dma_topology.lanes
                        if lane.controller == "SPI4")
        source += _checked(DirectCall("nx_stm32h743_dma_preflight", (lanes, spi_lanes)))
    for item in result.controllers:
        if item.kind in {"gpio", "watchdog"}:
            continue
        if item.kind not in {"uart", "spi", "pwm", "i2c", "adc", "flash", "exti"}:
            fail("No maintained H743 selected admission")
        arguments = (int(item.controller.removeprefix("EXTI")),) if item.kind == "exti" else ()
        source += _checked(DirectCall(f"nx_stm32h743_{item.kind}_preflight", arguments))
        if item.kind == "spi" and item.mode == "dma":
            source += _checked(DirectCall("nx_stm32h743_spi_dma_preflight"))
    if _ports(result):
        source += _checked(DirectCall("nx_stm32h743_platform_preflight",
                                      (Address("s_nx_platform_config"),)))
    return source
