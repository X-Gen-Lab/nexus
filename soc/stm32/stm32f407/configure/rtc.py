"""Static F407 LSE calendar assembly; backup hardware behavior lives in C."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail


def validate(selection, route):
    """The two explicit policies share one exact oscillator and Alarm A."""
    if route["controller"] != "RTC" or route["pins"]:
        fail("F407 RTC requires its exact LSE oscillator ownership route")


def validate_context(selection, route, *, backend, clock_profile):
    """Only the reviewed RUN system owns the finite monotonic deadline."""
    if route["kind"] == "rtc" and (
            backend != "baremetal" or clock_profile != "hse8-pll168"):
        fail("F407 RTC requires baremetal hse8-pll168 RUN ownership")


def constructor(item, result):
    """Describe immutable configuration and retained C lifecycle calls."""
    if item.controller != "RTC" or item.pins or item.dma:
        fail("F407 RTC requires its exact pinless C oscillator owner")
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
