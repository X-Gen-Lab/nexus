"""Raw DAC admission; static facts do not establish analog measurements."""
from tools.configure.providers.common import fail, integer, sequence


FORMATS = ("u8", "u12-right", "u12-left")
WAVE_MODES = frozenset(("noise", "triangle"))
DMA_MODES = frozenset(("trigger-dma", "trigger-dma-circular"))
TRIGGERS = ("none", "software", "tim6-trgo", "tim7-trgo", "exti9")
AMPLITUDES = tuple((1 << bits) - 1 for bits in range(1, 13))


def validate_selection(selection, route, provider):
    """Validate all raw values before the resolver acquires any resources."""
    required = {"channels", "format", "idle_codes", "output_buffer", "trigger"}
    if not required.issubset(selection):
        fail("DAC requires channels, format, idle_codes, output_buffer and trigger")
    channels = sequence(selection["channels"], "DAC channels")
    if (not channels or len(channels) > 2 or
            any(type(channel) is not int or channel not in (1, 2)
                for channel in channels) or len(set(channels)) != len(channels)):
        fail("DAC channels must select unique physical channel 1 and/or 2")
    mapping = getattr(provider, "DAC_PIN_CHANNELS", None)
    if mapping is None:
        fail("DAC provider lacks a reviewed channel/pad map")
    authorized = {mapping.get((route["controller"], pin["pin"]))
                  for pin in route["pins"]}
    if None in authorized or not set(channels).issubset(authorized):
        fail("DAC channels exceed the Board-authorized analog pads")
    data_format = selection["format"]
    if data_format not in FORMATS:
        fail("DAC format must be u8, u12-right or u12-left")
    idle = sequence(selection["idle_codes"], "DAC idle codes")
    maximum = 255 if data_format == "u8" else 4095
    if len(idle) != len(channels):
        fail("DAC idle codes require one raw code per selected channel")
    for code in idle:
        integer(code, 0, maximum, "DAC idle code")
    if type(selection["output_buffer"]) is not bool:
        fail("DAC output_buffer must be boolean")
    trigger = selection["trigger"]
    mode = selection["mode"]
    if trigger not in TRIGGERS:
        fail("DAC trigger lacks a maintained source owner")
    if (mode == "direct") != (trigger == "none"):
        fail("DAC direct mode requires no trigger; triggered modes require one")
    external = trigger == "exti9"
    if external:
        source = selection.get("trigger_input")
        if not isinstance(source, str) or not source:
            fail("DAC EXTI9 requires an explicit trigger_input hardware-event owner")
        if mode == "trigger-dma":
            fail("DAC external finite DMA has no proven final-trigger tail")
        if mode == "trigger-dma-circular":
            if selection.get("recovery_trigger") not in ("tim6-trgo", "tim7-trgo"):
                fail("DAC external DMA requires a recovery timer TIM6 or TIM7")
        elif "recovery_trigger" in selection:
            fail("DAC recovery timer requires circular DMA")
    elif {"trigger_input", "recovery_trigger"} & selection.keys():
        fail("DAC external fields trigger_input/recovery_trigger require EXTI9")
    if mode in DMA_MODES:
        if "irq_priority" not in selection:
            fail("DAC DMA requires an explicit IRQ priority")
        if not external and trigger not in {"tim6-trgo", "tim7-trgo"}:
            fail("DAC DMA requires an exclusively owned TIM6 or TIM7 timer")
    elif {"irq_priority", "calls_os"} & selection.keys():
        fail("DAC CPU modes have no IRQ options")
    if mode in WAVE_MODES:
        amplitude = selection.get("amplitude")
        if (type(amplitude) is not int or amplitude not in AMPLITUDES or
                data_format == "u8" or
                any(code + amplitude > maximum for code in idle)):
            fail("DAC wave amplitude must be 2^n-1 without raw output overflow")
    elif "amplitude" in selection:
        fail("DAC amplitude requires noise or triangle mode")


def selected_route(selection, route, provider):
    """Project one Board authorization onto exactly the selected analog pads."""
    if route["kind"] != "dac":
        return route
    chosen = frozenset(selection["channels"])
    pins = [pin for pin in route["pins"]
            if provider.DAC_PIN_CHANNELS[(route["controller"], pin["pin"])]
            in chosen]
    resources = list(route.get("resources", ()))
    if selection["trigger"].startswith("tim"):
        timer = selection["trigger"].split("-", 1)[0].upper()
        resources += ["controller:" + timer, "timer:" + timer]
    elif selection["trigger"] == "exti9" and selection["mode"] in DMA_MODES:
        timer = selection["recovery_trigger"].split("-", 1)[0].upper()
        resources += ["controller:" + timer, "timer:" + timer]
    selected = {**route, "pins": pins, "resources": resources}
    if selection["mode"] in DMA_MODES:
        mask = sum(1 << (channel - 1) for channel in chosen)
        selected["mode_dma"] = {
            selection["mode"]: list(provider.DAC_DMA_ROUTES[mask])}
    return selected


def resolve_sources(selected):
    """Validate static EXTI9 references and order the same owner before DAC.

    The graph-local list is still a draft. No second runtime dependency graph
    or executable hardware sequence is produced; ordinary reverse close owns
    the corresponding consumer-before-source withdrawal.
    """
    sources = {}
    for item in selected:
        if item["kind"] != "dac" or item["trigger"] != "exti9":
            continue
        matches = [source for source in selected
                   if source["id"] == item["trigger_input"]]
        if len(matches) != 1:
            fail("DAC trigger_input requires one existing EXTI9 hardware-event owner")
        source, = matches
        if (source["kind"] != "exti" or source["controller"] != "EXTI9" or
                source["mode"] != "hardware-event"):
            fail("DAC trigger_input requires the exact EXTI9 hardware-event owner")
        sources[item["id"]] = source
    if not sources:
        return
    moved = {source["id"] for source in sources.values()}
    ordered, appended = [], set()
    for item in selected:
        if item["id"] in moved:
            continue
        source = sources.get(item["id"])
        if source is not None and source["id"] not in appended:
            ordered.append(source)
            appended.add(source["id"])
        ordered.append(item)
    selected[:] = ordered


def editor_schema(properties):
    """Reuse the same raw representation vocabulary in editor completion."""
    properties["format"] = {"type": "string", "enum": list(FORMATS)}
    properties["channels"] = {"type": "array", "minItems": 1, "maxItems": 2,
                              "uniqueItems": True,
                              "items": {"type": "integer", "enum": [1, 2]}}
    properties["idle_codes"] = {"type": "array", "minItems": 1, "maxItems": 2,
                                "items": {"type": "integer", "minimum": 0,
                                          "maximum": 4095}}
    properties["output_buffer"] = {"type": "boolean"}
    properties["trigger"] = {"type": "string", "enum": list(TRIGGERS)}
    properties["trigger_input"] = {"type": "string", "minLength": 1}
    properties["recovery_trigger"] = {
        "type": "string", "enum": ["tim6-trgo", "tim7-trgo"]}
    properties["amplitude"] = {"type": "integer", "enum": list(AMPLITUDES)}
