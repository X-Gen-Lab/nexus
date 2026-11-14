"""Static reviewed F4 LSE calendar; backup hardware behavior lives in C."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail


def validate(selection, route):
    """The two explicit policies share one exact oscillator and Alarm A."""
    if route["controller"] != "RTC" or route["pins"]:
        fail("F4 RTC requires its exact LSE oscillator ownership route")
    expected = ("rtc-lse-bypass-pc14" if selection.get("lse_bypass", False)
                else "rtc-lse-pc14-pc15")
    if route["id"] != expected:
        fail("F4 RTC oscillator source differs from its reviewed pad route")


def validate_context(selection, route, *, backend, clock_profile,
                     part=None, clock_plan=None):
    """Admit only the exact System/RTC combination exercised together."""
    if route["kind"] != "rtc":
        return
    if part not in {"STM32F411RET6", "STM32F446ZET6", "STM32F479NIH6"}:
        if backend != "baremetal" or clock_profile != "hse8-pll168":
            fail("F407 RTC requires baremetal hse8-pll168 RUN ownership")
        return
    if (backend != "baremetal" or clock_profile != "custom" or
            clock_plan is None or clock_plan.parameters is None or
            clock_plan.pll_sai is not None or
            selection.get("lse_bypass", False) or
            selection.get("periodic_wakeup", False)):
        fail("RTC requires its reviewed exact-part RUN and crystal profile")
    expected = dict(source="hsi", input_hz=16000000, sysclk_source="pll",
                    pll_enabled=True, ahb_div=1, adc_div=4,
                    supply_mv=3300, css=False)
    m, n, p, q, apb1, apb2 = {
        "STM32F411RET6": (16, 384, 4, 8, 2, 1),
        "STM32F446ZET6": (16, 336, 2, 7, 4, 2),
        "STM32F479NIH6": (10, 210, 2, 7, 4, 2),
    }[part]
    expected.update(pll_m=m, pll_n=n, pll_p=p, pll_q=q,
                    apb1_div=apb1, apb2_div=apb2)
    if any(clock_plan.parameters[key] != value
           for key, value in expected.items()):
        fail("RTC requires its reviewed exact-part RUN tuple")


def constructor(item, result):
    """Describe immutable configuration and retained C lifecycle calls."""
    if item.controller != "RTC" or item.pins or item.dma:
        fail("F4 RTC requires its exact pinless C oscillator owner")
    name = item.id.replace("-", "_")
    config = "s_nx_rtc_config_" + name
    options = item.options
    provision = item.mode == "provision-empty"
    policy = ("NX_STM32_RTC_PROVISION_EMPTY" if provision else
              "NX_STM32_RTC_ADOPT_RETAINED")
    definitions = [f"static const nx_stm32_rtc_config_t {config} = {{",
        "    .clock = { nx_stm32_rtc_clock_read, &g_nx_stm32_system },",
        f"    .timeout_us = {options.timeout_us}u, .poll_limit = {options.poll_limit}u,",
        f"    .policy = {policy},",
        f"    .lse_bypass = {str(options.lse_bypass).lower()},",
        f"    .allow_set = {str(options.allow_set).lower()}, .has_initial = {str(provision).lower()}"]
    if provision:
        initial = options.initial
        values = (initial.year, initial.month, initial.day, initial.weekday,
                  initial.hour, initial.minute, initial.second)
        definitions[-1] += ","
        definitions.append("    .initial = { " +
                           ", ".join(str(value) + "u" for value in values) + " }")
    definitions += ["};"]
    state = "s_nx_port_" + name
    return Construction(state_type="nx_stm32_rtc_state_t",
        definitions=definitions,
        initialize=DirectCall("nx_stm32_rtc_prepare", (Address(state), Address(config))),
        stop=[DirectCall("nx_stm32_rtc_close", (Address(state),))],
        retain_on_failed_init=True)
