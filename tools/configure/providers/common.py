"""Shared input checks; provider packages own silicon-specific limits."""
import re

UINT32_MAX = (1 << 32) - 1


class ConfigurationError(ValueError):
    """A rejected input has no usable generated configuration."""


def fail(message):
    raise ConfigurationError(message)


def integer(value, minimum=0, maximum=UINT32_MAX, context="integer"):
    if type(value) is not int or not minimum <= value <= maximum:
        fail(f"{context}: integer required in [{minimum}, {maximum}]")
    return value


def sequence(value, context):
    if not isinstance(value, list):
        fail(f"{context}: expected array")
    return value


def cpu_abi(cpu):
    """Capabilities are chip facts, never additional authored ABI choices."""
    return {key: cpu[key] for key in ("arch", "fpu", "float_abi")}


def resolve_interrupts(cpu, irq, backend, provider):
    """Bind logical IRQ range and syscall policy to a maintained kernel port."""
    try:
        from .. import cpu as profiles
    except ImportError:
        import cpu as profiles
    if cpu_abi(cpu) != provider.CPU_ABI:
        fail("FreeRTOS kernel CPU ABI differs from the selected SoC")
    # Direct provider-contract tests may supply ABI assertions alone. Their
    # missing capabilities come from the exact maintained provider, never an
    # architecture-name guess; public CPU resolution requires every fact.
    facts = {**provider.CPU_FEATURES, **cpu}
    irq = {"external_count": provider.EXTERNAL_IRQ_COUNT, **irq}
    profile = profiles.resolve(facts, irq, backend, enum_abi=provider.ENUM_ABI)
    result = profile.to_dict()["irq"]
    if backend == "freertos" and result["kernel_port"] != provider.FREERTOS_PORT:
        fail(f"Unmaintained FreeRTOS port: {provider.FREERTOS_PORT}")
    return result


def dma_irq(dma, vector):
    """Preserve physical stream identity while naming the family IRQ vector."""
    match = re.fullmatch(r"DMA([12]):stream([0-7]):channel([0-7])", dma)
    if match is None or vector is None:
        fail(f"DMA route requires an explicit request selector: {dma}")
    return f"irq:DMA{match[1]}_{vector}{match[2]}"


def validate_dma(route, mode, expected, family, normalize, trigger=None):
    actual = tuple(sequence(route["dma"], "DMA route"))
    if expected is None or actual != expected:
        fail(f"DMA route differs from fixed provider: {family}/{route['controller']}/{mode}")
    if mode == "trigger-dma":
        resources = tuple(normalize(resource) for resource in
                          route.get("mode_resources", {}).get(mode, []))
        if trigger is None or resources != ("controller:" + trigger,):
            fail(f"Trigger resource differs from fixed provider: {family}/{route['controller']}")


def validate_selection(selection, route, provider, ram_size):
    """One maintained mode has one bounded, explicit configuration contract."""
    kind = route["kind"]
    fields = {
        "gpio": set(), "uart": {"baud", "rx_capacity", "rx_profile", "irq_priority", "calls_os"},
        "spi": {"max_hz", "irq_priority", "calls_os"}, "i2c": {"max_hz"}, "flash": set(), "watchdog": set(),
        "exti": {"edge", "event_capacity", "irq_priority", "calls_os"},
        "pwm": {"period_ticks", "duty_ticks", "tick_hz"},
        "adc": {"channels", "sample_times", "reference_mv", "timeout_ms", "irq_priority", "calls_os"},
    }
    unknown = selection.keys() - {"id", "binding", "mode"} - fields.get(kind, set())
    if kind not in fields or unknown:
        fail(f"Unsupported {kind} configuration fields: {sorted(unknown)}")
    if kind == "uart":
        if not {"baud", "rx_profile", "irq_priority"}.issubset(selection):
            fail("UART requires baud, RX profile and IRQ priority")
        integer(selection["baud"], 1, 1000000, "UART baud")
        if selection["rx_profile"] == "blocks":
            if selection["mode"] != "irq-blocks":
                fail("UART RX blocks requires the implemented irq-blocks provider")
            if "rx_capacity" in selection:
                fail("UART RX blocks has no generated ring capacity; storage is caller-owned")
        elif selection["mode"] == "irq-blocks":
            fail("UART irq-blocks requires RX blocks format")
        elif selection["rx_profile"] in {"bytes", "events"}:
            integer(selection.get("rx_capacity"), 1, min(4096, ram_size // 16), "UART RX capacity")
        else:
            fail("Unsupported UART receive profile")
    elif kind == "exti":
        if not {"edge", "event_capacity", "irq_priority"}.issubset(selection):
            fail("EXTI requires edge, event capacity and IRQ priority")
        if selection["edge"] not in {"rising", "falling", "both"}:
            fail("Unsupported EXTI edge")
        integer(selection["event_capacity"], 1, min(4096, ram_size // 24), "EXTI event capacity")
    elif kind == "pwm":
        if not {"period_ticks", "duty_ticks", "tick_hz"}.issubset(selection):
            fail("PWM requires explicit period, duty and tick_hz")
        integer(selection["period_ticks"], 1, 65535, "PWM period")
        integer(selection["duty_ticks"], 0, selection["period_ticks"], "PWM duty")
        integer(selection["tick_hz"], 1, context="PWM tick_hz")
    elif kind == "adc":
        if not {"channels", "sample_times", "reference_mv"}.issubset(selection):
            fail("ADC requires channels, sample_times and reference_mv")
        channels = sequence(selection["channels"], "ADC channels")
        expected = [int(pin["pin"][2:]) for pin in route["pins"]]
        if channels != expected or not channels or len(channels) > 16:
            fail("ADC channel sequence differs from reviewed analog pins")
        for channel in channels:
            integer(channel, 0, 15, "ADC channel")
        samples = sequence(selection["sample_times"], "ADC sample_times")
        if len(samples) != len(channels):
            fail("ADC sample_times must match the channel sequence")
        for sample in samples:
            integer(sample, 0, 7, "ADC sample encoding")
        integer(selection["reference_mv"], 1, 3600, "ADC reference_mv")
        if selection["mode"] in {"single-shot", "scan"} and (selection["mode"] == "single-shot") != (len(channels) == 1):
            fail("ADC mode contradicts its reviewed channel count")
        if selection["mode"] == "trigger-dma" and "irq_priority" not in selection:
            fail("ADC trigger-dma requires an explicit IRQ priority")
        if selection["mode"] != "trigger-dma" and ({"irq_priority", "calls_os"} & selection.keys()):
            fail("Polling ADC has no IRQ options")
    elif kind == "i2c" and selection.get("max_hz", 100000) != 100000:
        fail("Only reviewed 100 kHz I2C mode is maintained")
    provider.validate_selection(selection, route)


def controller_ir(item, family):
    """Accept a validated projection at direct model-constructor boundaries."""
    try:
        from ..ir import ControllerIR
    except ImportError:
        from ir import ControllerIR
    return (item if isinstance(item, ControllerIR) else
            ControllerIR.create(item, family))
