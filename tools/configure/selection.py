"""Cross-domain selection checks above the leaf provider contracts."""

from tools.configure.contracts import CONTROLLER_CONTRACTS
from tools.configure.providers.common import (
    UINT32_MAX, cpu_abi, fail, integer, mode_traits, sequence)


def validate_clock_profiles(soc, provider):
    """Fixed cold implementations accept only their exact reviewed clock tuple."""
    if provider.MODEL:
        return
    supported = provider.CLOCK_PROFILES
    actual = soc["clock_profiles"]
    if set(actual) != set(supported):
        fail("Clock profile identity differs from the fixed cold implementation")
    for name, expected in supported.items():
        provider.clock_plan(name, actual[name])
        if provider.normalize_clock(actual[name]) != provider.normalize_clock(expected):
            fail(f"Clock profile differs from the fixed cold implementation: {name}")



def resolve_interrupts(cpu, irq, backend, provider):
    """Bind logical IRQ range and syscall policy to a maintained kernel port."""
    from tools.configure import cpu as profiles
    if cpu_abi(cpu) != provider.CPU_ABI:
        fail("FreeRTOS kernel CPU ABI differs from the selected SoC")
    # Direct provider-contract tests may supply ABI assertions alone. Their
    # missing capabilities come from the exact maintained provider, never an
    # architecture-name guess; public CPU resolution requires every fact.
    facts = {**provider.CPU_FEATURES, **cpu}
    irq = {"external_count": provider.EXTERNAL_IRQ_COUNT, **irq}
    profile = profiles.resolve(facts, irq, backend, enum_abi=provider.ENUM_ABI)
    return validate_interrupt_profile(profile, provider)


def validate_interrupt_profile(profile, provider):
    """Bind the resolved CPU once to the exact provider's maintained port."""
    if cpu_abi(profile.cpu) != provider.CPU_ABI:
        fail("FreeRTOS kernel CPU ABI differs from the selected SoC")
    if (profile.backend == "freertos" and
            profile.irq.kernel_port != provider.FREERTOS_PORT):
        fail(f"Unmaintained FreeRTOS port: {provider.FREERTOS_PORT}")
    return profile.irq



def validate_selection(selection, route, provider, ram_size):
    """One maintained mode has one bounded, explicit configuration contract."""
    kind = route["kind"]
    contract = CONTROLLER_CONTRACTS.get(kind)
    fields = contract.selected_fields if contract is not None else frozenset()
    if kind == "lptim" and selection["mode"] == "irq-counter16-oneshot":
        fields |= {"irq_priority", "calls_os"}
    unknown = selection.keys() - {"id", "binding", "mode"} - fields
    if contract is None or unknown:
        fail(f"Unsupported {kind} configuration fields: {sorted(unknown)}")
    if kind == "lptim":
        integer(selection.get("tick_hz"), 1, UINT32_MAX, "LPTIM tick_hz")
    if kind in {"timer_capture", "advanced_pwm"}:
        from tools.configure import timer_advanced
        timer_advanced.validate_selection(selection, route, provider)
    elif kind in {"eth", "eth_frames"}:
        from tools.configure import ethernet
        ethernet.validate_selection(selection, route, provider, ram_size)
    elif kind == "usb_device":
        from tools.configure import usb_device
        usb_device.validate_selection(selection, route, provider, ram_size)
    elif kind == "can":
        from tools.configure import can
        can.validate_selection(selection, provider, ram_size)
    elif kind == "uart":
        if not {"baud", "rx_profile", "irq_priority"}.issubset(selection):
            fail("UART requires baud, RX profile and IRQ priority")
        integer(selection["baud"], *provider.LIMITS["uart_baud"], "UART baud")
        from tools.configure import uart_formats
        uart_formats.validate(selection, route, provider)
        if selection["mode"] == "irq-words":
            if selection["rx_profile"] != "words":
                fail("Word UART requires RX words, never byte/event storage")
            integer(selection.get("rx_capacity"), 1,
                    min(4096, ram_size // 2), "UART RX word capacity")
        elif selection["rx_profile"] == "blocks":
            if not mode_traits(kind, selection["mode"]).block_stream:
                fail("UART RX blocks requires irq-blocks or a reviewed RX-DMA provider")
            if "rx_capacity" in selection:
                fail("UART RX blocks has no generated ring capacity; storage is caller-owned")
        elif mode_traits(kind, selection["mode"]).block_stream:
            fail("UART block provider requires RX blocks format")
        elif selection["rx_profile"] in {"bytes", "events"}:
            integer(selection.get("rx_capacity"), 1, min(4096, ram_size // 16), "UART RX capacity")
        else:
            fail("Unsupported UART receive profile")
    elif kind == "exti":
        if not {"edge", "event_capacity", "irq_priority"}.issubset(selection):
            fail("EXTI requires edge, event capacity and IRQ priority")
        if (type(selection["edge"]) is not str or
                selection["edge"] not in {"rising", "falling", "both"}):
            fail("Unsupported EXTI edge")
        integer(selection["event_capacity"], 1, min(4096, ram_size // 24), "EXTI event capacity")
    elif kind == "pwm":
        if not {"period_ticks", "duty_ticks", "tick_hz"}.issubset(selection):
            fail("PWM requires explicit period, duty and tick_hz")
        integer(selection["period_ticks"], 1, provider.LIMITS["pwm_period"], "PWM period")
        integer(selection["duty_ticks"], 0, selection["period_ticks"], "PWM duty")
        integer(selection["tick_hz"], 1, context="PWM tick_hz")
    elif kind == "adc":
        if not {"channels", "sample_times", "reference_mv"}.issubset(selection):
            fail("ADC requires channels, sample_times and reference_mv")
        channels = sequence(selection["channels"], "ADC channels")
        mapping = getattr(provider, "ADC_PIN_CHANNELS", None)
        if mapping is None:
            expected = [int(pin["pin"][2:]) for pin in route["pins"]]
        else:
            expected = []
            for pin in route["pins"]:
                key = (route["controller"], pin["pin"])
                if key not in mapping:
                    fail("ADC controller/pin lacks a reviewed channel mapping")
                expected.append(mapping[key])
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
        if selection["mode"] in {"single-shot", "low-rate-scan"} and (selection["mode"] == "single-shot") != (len(channels) == 1):
            fail("ADC mode contradicts its reviewed channel count")
        if mode_traits(kind, selection["mode"]).dma and "irq_priority" not in selection:
            fail("ADC trigger-dma requires an explicit IRQ priority")
        if not mode_traits(kind, selection["mode"]).dma and ({"irq_priority", "calls_os"} & selection.keys()):
            fail("Polling ADC has no IRQ options")
    elif kind == "i2c":
        limits = provider.LIMITS.get("i2c_hz", (100000, 100000))
        integer(selection.get("max_hz", 100000), *limits,
                "Reviewed I2C maximum frequency")
        if mode_traits(kind, selection["mode"]).irq:
            if "irq_priority" not in selection:
                fail("Asynchronous I2C requires an explicit IRQ priority")
        elif {"irq_priority", "calls_os"} & selection.keys():
            fail("Polling I2C has no IRQ options")
    provider.validate_selection(selection, route)
