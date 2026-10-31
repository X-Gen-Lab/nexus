"""Exact ADC1 sequence layouts and their cold observed-silicon policy.

The source record owns reviewed cuts. Provider execution, routes and clock
checks remain mandatory; a policy record does not implement a workaround.
"""
import re

from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer, sequence
from tools.configure.memory import dma_regions


MODES = ("regular-sequence-dma", "injected-sequence")
SAMPLE_HALFCYCLES = (5, 13, 25, 49, 95, 185, 495, 1281)


def trigger_resources(selection):
    """Return a variable-length resource tuple: zero or one exact timer.

    Callers iterate these homogeneous resource identities, not fixed fields.
    """
    return ("controller:TIM6",) if selection["trigger"] == "tim6-trgo" else ()


def validate_selection(selection, route):
    """Limit selected channels to documented pads and internal sources."""
    required = {"channels", "sample_times", "reference_mv", "exclusive",
                "silicon_profile", "trigger", "irq_priority"}
    if not required.issubset(selection):
        fail("ADC group requires channels, profile, trigger and IRQ policy")
    pins = tuple((pin["pin"], pin["function"], pin["af"])
                 for pin in route["pins"])
    if (route["controller"] != "ADC1" or selection["mode"] not in MODES or
            pins != (("PA0", "adc1-in1", 0), ("PA1", "adc1-in2", 0))):
        fail("ADC group requires the reviewed ADC1 PA0/PA1 analog route")
    channels = sequence(selection["channels"], "ADC group channels")
    samples = sequence(selection["sample_times"], "ADC group sample_times")
    if not channels or len(channels) != len(samples):
        fail("ADC group sample_times must match nonempty logical ranks")
    previous = {}
    for channel, sample in zip(channels, samples):
        integer(channel, 1, 18, "ADC group channel")
        integer(sample, 0, 7, "ADC group sample encoding")
        if channel not in {1, 2, 16, 17, 18}:
            fail("ADC group channel has no reviewed analog/internal route")
        if sample < {16: 6, 17: 7, 18: 5}.get(channel, 0):
            fail("ADC internal source requires its minimum sampling time")
        if channel in previous and previous[channel] != sample:
            fail("One ADC channel has one hardware sampling-time register")
        previous[channel] = sample
    integer(selection["reference_mv"], 1620, 3600, "ADC reference_mv")
    if (selection["exclusive"] is not True or
            type(selection["irq_priority"]) is not int or
            selection["irq_priority"] != 5 or
            selection.get("calls_os", False) is not False):
        fail("ADC group requires exclusive ownership and non-OS IRQ priority5")
    trigger = selection["trigger"]
    if trigger not in {"software", "tim6-trgo"}:
        fail("ADC group trigger is unmaintained")
    period = selection.get("period_us", 0)
    integer(period, 0, (1 << 32) - 1, "ADC group period_us")
    if (trigger == "software") != (period == 0):
        fail("ADC group software has no period; TIM6 requires a finite period")
    if "watchdog" in selection:
        value = selection["watchdog"]
        if not isinstance(value, dict) or set(value) != {"lower", "upper"}:
            fail("ADC group watchdog requires lower and upper bounds")
        lower = integer(value["lower"], 0, 4095, "ADC watchdog lower")
        integer(value["upper"], lower, 4095, "ADC watchdog upper")


def resolve(selection, record, digest, *, part, clock):
    """Normalize one named expectation after source and provider validation."""
    if (part != "STM32G474RET6" or clock["core_hz"] != 170000000 or
            clock["source"] != "hse" or clock["input_hz"] != 24000000):
        fail("ADC group requires the reviewed G474RE HSE24/PLL170 profile")
    if (not isinstance(record, dict) or record.get("schema_version") != 1 or
            record.get("provider") != "stm32g474" or
            record.get("part") != part or record.get("kind") != "adc_group" or
            tuple(record.get("modes", ())) != MODES or
            record.get("device_id") != 0x469 or
            record.get("flash_bytes") != 524288 or
            not re.fullmatch(r"[0-9a-f]{64}", digest)):
        fail("ADC group review record differs from its exact provider scope")
    profile = selection["silicon_profile"]
    if not isinstance(profile, str):
        fail("ADC group silicon_profile requires a reviewed named profile")
    facts = record.get("profiles", {}).get(profile)
    if not isinstance(facts, dict) or set(facts) != {
            "revision_id", "rank_repeat", "required_workarounds"}:
        fail("ADC group silicon_profile is not reviewed: " + profile)
    repeat = integer(facts["rank_repeat"], 1, 2, "ADC rank repeat")
    revision = integer(facts["revision_id"], 0, 65535, "ADC expected revision")
    required = integer(facts["required_workarounds"], 0, 7,
                       "ADC required workarounds")
    # These flags describe the actual fixed provider algorithms: every selected
    # group primes and disables/re-enables; repeat2 alone executes duplication.
    applied = 6 | (1 if repeat == 2 else 0)
    if required & applied != required:
        fail("ADC group requires an unimplemented workaround")
    physical = len(selection["channels"]) * repeat
    regular = selection["mode"] == MODES[0]
    if physical > (16 if regular else 4):
        fail("ADC group physical ranks exceed its selected sequencer")
    halfcycles = sum((SAMPLE_HALFCYCLES[code] + 25) * repeat
                     for code in selection["sample_times"])
    period = selection.get("period_us", 0)
    if period and period * 170 <= (halfcycles + 5) * 4 + 170:
        fail("ADC timer period does not cover the complete physical scan")
    prime_us = (halfcycles * 4 + 169) // 170
    if period and period + prime_us >= 1000:
        fail("ADC timer period leaves no bounded priming freshness window")
    policy = {"profile": profile, "provider": "stm32g474", "part": part,
              "device_id": record["device_id"],
              "flash_bytes": record["flash_bytes"],
              "rules": [{"revision_id": revision,
                         "required_workarounds": required}],
              "applied_workarounds": applied, "review_sha256": digest,
              "review_scope": "selected_adc_group_modes"}
    return {"rank_repeat": repeat, "sample_stride": 2 * repeat,
            "scan_stride": physical * 2,
            "max_frames": 65535 // physical if regular else 1,
            "adc_clock_hz": 21250000, "hclk_hz": 170000000,
            "trigger_identity": 0, "admission": policy}


def symbols(item):
    name = item.id.replace("-", "_")
    return ("s_nx_silicon_source_" + name, "s_nx_silicon_policy_" + name,
            "s_nx_adc_group_profile_" + name)


def declarations(item):
    """Selected readonly facts; no admitted state or retained output object."""
    source, policy, profile = symbols(item)
    expectation = item.options.admission
    rule, = expectation.rules
    digest = ", ".join("0x" + expectation.review_sha256[index:index + 2] + "u"
                       for index in range(0, 64, 2))
    return [
        f"static const nx_stm32_identity_registers_t {source}_registers = {{",
        "    &DBGMCU->IDCODE, (const volatile uint16_t*)FLASHSIZE_BASE",
        "};",
        f"static const nx_silicon_source_t {source} = {{",
        f"    &{source}_registers, nx_stm32_identity_read", "};",
        f"static const nx_silicon_revision_rule_t {policy}_rule = {{",
        f"    {rule.revision_id}u, NX_SILICON_REVISION_REVIEWED,",
        f"    {rule.required_workarounds}u", "};",
        f"static const nx_silicon_admission_policy_t {policy} = {{",
        f"    .device_id = {expectation.device_id}u,",
        f"    .flash_bytes = {expectation.flash_bytes}u,",
        f"    .rules = &{policy}_rule, .rule_count = 1u,",
        f"    .applied_workarounds = {expectation.applied_workarounds}u,",
        f"    .review_sha256 = {{ {digest} }},",
        "    .allow_unreviewed = false", "};",
        f"static const nx_stm32g474_adc_group_profile_t {profile} = {{",
        f"    &{policy}, {item.options.rank_repeat}u", "};"]


def preflight(result):
    """Reobserve on each cold start before Platform acquires any resources."""
    lines = []
    for item in result.controllers:
        if item.kind != "adc_group":
            continue
        source, policy, _ = symbols(item)
        lines += ["    {", "        nx_silicon_admission_t observed;",
                  f"        result.primary = nx_silicon_preflight(&{source},",
                  f"                                               &{policy},",
                  "                                               &observed);",
                  "        if (result.primary != NX_SUCCESS)" +
                  " { return result; }",
                  "    }"]
    return lines


def constructor(item, result, binding):
    """One physical owner; no ordinary independently startable ADC face."""
    options = item.options
    if (type(binding.get("supply_mv")) is not int or
            not 1620 <= options.reference_mv <= binding["supply_mv"] <= 3600):
        fail("ADC group requires explicit valid supply/reference facts")
    name = item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_config_" + name
    source, _, profile = symbols(item)
    regular = item.mode == MODES[0]
    regions, count = dma_regions(name, result, engine="DMA1") if regular else (
        [], 0)
    if regular and count == 0:
        fail("Regular ADC group requires selected DMA1-visible SRAM")
    channel_text = ", ".join(str(value) + "u" for value in options.channels)
    sample_text = ", ".join(str(value) + "u" for value in options.sample_times)
    lower = options.watchdog.lower if options.watchdog else 0
    upper = options.watchdog.upper if options.watchdog else 0
    lines = declarations(item) + regions + [
        f"static const uint8_t s_nx_channels_{name}[] = {{ {channel_text} }};",
        f"static const uint8_t s_nx_samples_{name}[] = {{ {sample_text} }};",
        f"static const nx_stm32g474_adc_group_config_t {config} = {{",
        (f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,"
         if regular else "    .regions = NULL, .region_count = 0u,"),
        f"    .channels = s_nx_channels_{name},",
        f"    .sample_times = s_nx_samples_{name},",
        f"    .channel_count = {len(options.channels)}u,",
        f"    .silicon_source = &{source}, .profile = &{profile},",
        f"    .period_us = {options.period_us}u,",
        f"    .trigger_identity = {options.trigger_identity}u,",
        f"    .reference_mv = {options.reference_mv}u, .poll_limit = 1000000u,",
        "    .group = " + ("NX_ADC_REGULAR," if regular else
                              "NX_ADC_INJECTED,"),
        "    .trigger = " + ("NX_ADC_TRIGGER_SOFTWARE," if options.trigger ==
                                "software" else "NX_ADC_TRIGGER_TIMER,"),
        f"    .watchdog = {'true' if options.watchdog else 'false'},",
        f"    .watchdog_low = {lower}u, .watchdog_high = {upper}u,",
        "    .priority = 5u, .exclusive = true", "};",
        "void ADC1_2_IRQHandler(void);",
        r"/** \brief Observe selected ADC group OVR/AWD/JEOS facts. */",
        "void ADC1_2_IRQHandler(void) { "
        f"nx_stm32g474_adc_group_adc_irq(&{state}); }}"]
    if regular:
        lines += ["void DMA1_Channel1_IRQHandler(void);",
                  r"/** \brief Observe the finite regular sequence DMA. */",
                  "void DMA1_Channel1_IRQHandler(void) { "
                  f"nx_stm32g474_adc_group_dma_irq(&{state}); }}"]
    return Construction("nx_stm32g474_adc_group_state_t", tuple(lines),
                        DirectCall("nx_stm32g474_adc_group_initialize",
                                   (Address(state), Address(config))),
                        (DirectCall("nx_stm32g474_adc_group_close", (Address(state),)),), True)
