"""Project selected typed C admission calls; no emitted register algorithms."""
from tools.configure.providers.common import fail

PREFLIGHT_CALL = 'nx_stm32u575_system_preflight(&g_nx_stm32u575_system)'


def selected_preflight(result, *, dma_conditions=None):
    del dma_conditions
    source = []
    for item in result.controllers:
        if item.kind in {"gpio", "watchdog"}:
            continue
        if item.kind not in {"uart", "spi", "pwm", "i2c", "adc", "flash", "exti", "rtc", "lptim"}:
            fail("No maintained U575 selected C preflight")
        arguments = ""
        if item.kind == "uart":
            if item.controller not in {"USART1", "USART2"}:
                fail("U575 requires an exact reviewed USART")
            dma = item.mode in {"dma-tx", "dma-chain-tx"}
            arguments = f"{item.controller[-1]}u, {'true' if dma else 'false'}"
        elif item.kind == "exti":
            arguments = f"{int(item.controller[-1])}u"
        source += [f"    result.primary = nx_stm32u575_{item.kind}_preflight({arguments});",
            "    if (result.primary != NX_SUCCESS) { return result; }"]
    from .emission import _leases
    if _leases(result):
        source += ["    result.primary = nx_stm32u575_platform_preflight(&s_nx_platform_cold);",
            "    if (result.primary != NX_SUCCESS) { return result; }"]
    return source
