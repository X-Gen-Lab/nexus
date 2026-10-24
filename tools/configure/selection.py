"""Cross-domain selection checks above the leaf provider contracts."""

try:
    from .providers.common import (
        UINT32_MAX, cpu_abi, fail, integer, mode_traits, sequence)
except ImportError:
    from providers.common import (
        UINT32_MAX, cpu_abi, fail, integer, mode_traits, sequence)


def validate_clock_profiles(soc, provider):
    """Fixed cold implementations accept only their exact reviewed clock tuple."""
    if provider.MODEL:
        return
    supported = provider.CLOCK_PROFILES
    actual = soc["clock_profiles"]
    if set(actual) != set(supported):
        fail("Clock profile identity differs from the fixed cold implementation")
    try:
        from . import clocks
    except ImportError:
        import clocks
    for name, expected in supported.items():
        clocks.plan(name, actual[name])
        if clocks.normalize(actual[name]) != clocks.normalize(expected):
            fail(f"Clock profile differs from the fixed cold implementation: {name}")



def resolve_interrupts(cpu, irq, backend, provider):
    """Bind logical IRQ range and syscall policy to a maintained kernel port."""
    try:
        from . import cpu as profiles
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



def validate_selection(selection, route, provider, ram_size):
    """One maintained mode has one bounded, explicit configuration contract."""
    kind = route["kind"]
    fields = {
        "rtc": set(), "lptim": {"tick_hz"},
        "adc_group": {"channels", "sample_times", "reference_mv", "exclusive",
                      "silicon_profile", "trigger", "period_us", "watchdog",
                      "irq_priority", "calls_os"},
        "pwm_adc": {"tick_hz", "period_ticks", "trigger_ticks", "deadtime_ticks",
                    "channels", "sample_times", "reference_mv", "exclusive",
                    "irq_priority", "calls_os"},
        "can_fd": {"bitrate", "data_bitrate", "event_capacity", "filters",
                   "drain_timeout_us", "poll_limit", "exclusive",
                   "irq_priority", "calls_os", "message_ram_words"},
        "encoder": {"counting", "filter_a", "filter_b", "invert_a", "invert_b", "exclusive"},
        "pwm_group": {"tick_hz", "period_ticks", "deadtime_ticks", "exclusive", "irq_priority", "calls_os"},
        "timer_capture": {"tick_hz", "edge", "filter", "exclusive", "irq_priority", "calls_os"},
        "advanced_pwm": {"tick_hz", "period_ticks", "deadtime_ticks", "break_active_high",
                         "idle_high", "idle_complement_high", "exclusive", "irq_priority", "calls_os"},
        "eth": {"tx_count", "rx_count", "rx_storage_bytes", "address",
                "init_timeout_us", "drain_timeout_us", "irq_priority", "calls_os"},
        "eth_frames": {"tx_count", "rx_count", "rx_storage_bytes", "address",
                       "init_timeout_us", "drain_timeout_us", "irq_priority",
                       "calls_os"},
        "usb_device": {"event_capacity", "rx_fifo_words", "vbus_sensing",
                       "poll_limit", "drain_timeout_us", "clock_timeout_us",
                       "irq_priority", "calls_os"},
        "can": {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
                "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"},
        "gpio": set(), "uart": {"baud", "rx_capacity", "rx_profile", "irq_priority", "calls_os", "frame", "stream", "irq_probe", "fifo"},
        "spi": {"max_hz", "irq_priority", "calls_os"}, "i2c": {"max_hz", "irq_priority", "calls_os", "timing"}, "flash": set(), "watchdog": set(),
        "exti": {"edge", "event_capacity", "irq_priority", "calls_os"},
        "pwm": {"period_ticks", "duty_ticks", "tick_hz"},
        "adc": {"channels", "sample_times", "reference_mv", "timeout_ms", "irq_priority", "calls_os", "stream"},
    }
    if kind == "lptim" and selection["mode"] == "irq-counter16-oneshot":
        fields["lptim"] |= {"irq_priority", "calls_os"}
    unknown = selection.keys() - {"id", "binding", "mode"} - fields.get(kind, set())
    if kind not in fields or unknown:
        fail(f"Unsupported {kind} configuration fields: {sorted(unknown)}")
    if kind == "lptim":
        integer(selection.get("tick_hz"), 1, UINT32_MAX, "LPTIM tick_hz")
    if kind in {"timer_capture", "advanced_pwm"}:
        try:
            from . import timer_advanced
        except ImportError:
            import timer_advanced
        timer_advanced.validate_selection(selection, route, provider)
    elif kind in {"eth", "eth_frames"}:
        try:
            from . import ethernet
        except ImportError:
            import ethernet
        ethernet.validate_selection(selection, route, provider, ram_size)
    elif kind == "usb_device":
        try:
            from . import usb_device
        except ImportError:
            import usb_device
        usb_device.validate_selection(selection, route, provider, ram_size)
    elif kind == "can":
        try:
            from . import can
        except ImportError:
            import can
        can.validate_selection(selection, provider, ram_size)
    elif kind == "uart":
        if not {"baud", "rx_profile", "irq_priority"}.issubset(selection):
            fail("UART requires baud, RX profile and IRQ priority")
        integer(selection["baud"], *provider.LIMITS["uart_baud"], "UART baud")
        try:
            from . import uart_formats
        except ImportError:
            import uart_formats
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
