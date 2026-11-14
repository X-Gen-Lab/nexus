"""Finite selected C constants and connection tables from package facts.

The projection contains no hardware operations or register encodings. GPIO bank
identities come from reviewed PadBindingIR, not parsed pin spelling. Concrete C
headers continue to own each IP's encoding and compile-time assertions.
"""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Header projection requires exact named facts")


def _table(macro, rows):
    prefix = "#define " + macro + "(X)"
    if not rows:
        return [prefix]
    return [prefix + " \\", *("    X(" + ", ".join(row) + ")" +
        (" \\" if index + 1 < len(rows) else "") for index, row in enumerate(rows))]


def _unsigned(value):
    return str(integer(value, context="Header constant")) + "u"


@dataclass(frozen=True)
class HeaderProjection:
    data: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "timebase_prefix", "pin_tables", "dma_memory"},
                {"clock"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported header projection schema")
        Symbol(value["timebase_prefix"])
        _fields(value["pin_tables"], {"capture", "advanced", "adc"})
        for name in value["pin_tables"].values():
            Symbol(name)
        dma = value["dma_memory"]
        _fields(dma, {"region", "origin_macro", "size_macro", "block_mode", "lane_macro"})
        for name in ("region", "origin_macro", "size_macro", "lane_macro"):
            Symbol(dma[name])
        clock = value.get("clock")
        if clock is not None:
            _fields(clock, {"domain_macros", "parameter_macros", "profile_flag",
                "system_source_flag", "input_source_flag", "block_protocol_flags",
                "auxiliary_prefix", "auxiliary_controller_flags"})
            allowed = {"pll_m", "pll_n", "pll_p", "pll_q", "ahb_div", "apb1_div",
                       "apb2_div", "adc_div", "css", "pll_enabled", "flash_wait_states"}
            if (not isinstance(clock["parameter_macros"], Mapping) or
                    set(clock["parameter_macros"]) - allowed):
                fail("Header clock parameters require named semantic fields")
            for group in ("domain_macros", "parameter_macros", "block_protocol_flags",
                          "auxiliary_controller_flags"):
                if not isinstance(clock[group], Mapping):
                    fail("Header clock macros require named fields")
                for macro in clock[group].values():
                    Symbol(macro)
            for name in ("profile_flag", "system_source_flag", "input_source_flag"):
                _fields(clock[name], {"macro", "equals"})
                Symbol(clock[name]["macro"])
                if not isinstance(clock[name]["equals"], str):
                    fail("Header clock selector requires one scalar name")
            Symbol(clock["auxiliary_prefix"])
        return cls(freeze(value))

    def render(self, result, provider):
        lines = []
        if self.data.get("clock") is not None:
            lines.extend(self._clock(result))
        policy = result.provider_context.binding_policy
        if policy is not None:
            lines.extend(policy.header_lines(self.data["timebase_prefix"]))
        lines.extend(self._timer_pins(result, provider))
        lines.extend(self._adc_pins(result, provider))
        memory = self.data["dma_memory"]
        region = next((region for region in result.physical_part.memory
            if region["id"] == memory["region"] and region.get("dma") is True), None)
        lines.extend(("#define " + memory["origin_macro"] + " 0x" +
            format(region["origin"] if region is not None else 0, "08x") + "u",
            "#define " + memory["size_macro"] + " " +
            _unsigned(region["size"] if region is not None else 0)))
        selected = tuple(item for item in result.controllers
                         if item.kind == "block" and item.mode == memory["block_mode"])
        if len(selected) > 1:
            fail("One block controller cannot own multiple selected DMA profiles")
        if selected and (selected[0].integration is None or len(selected[0].integration.dma) != 1):
            fail("Block DMA header requires one selected physical lane")
        lane = selected[0].integration.dma[0].lane if selected else 0
        lines.append("#define " + memory["lane_macro"] + " " + _unsigned(lane))
        return lines

    def _clock(self, result):
        config, selected = self.data["clock"], result.clock_plan
        if selected is None or selected.parameters is None:
            fail("Selected header clock requires its reviewed semantic plan")
        p = selected.parameters
        flag = config["profile_flag"]
        lines = ["#define " + flag["macro"] + " " +
                 _unsigned(int(selected.profile == flag["equals"]))]
        for protocol, macro in config["block_protocol_flags"].items():
            enabled = any(item.kind == "block" and item.options.protocol == protocol and
                          item.options.speed == "high-speed" for item in result.controllers)
            lines.append("#define " + macro + " " + _unsigned(int(enabled)))
        for domain, macro in config["domain_macros"].items():
            if domain not in selected.domains:
                fail("Header macro requires a reviewed clock domain: " + domain)
            lines.append("#define " + macro + " " +
                         _unsigned(selected.domains[domain].nominal_hz))
        for field, macro in config["parameter_macros"].items():
            lines.append("#define " + macro + " " + _unsigned(int(p[field])))
        source = config["system_source_flag"]
        lines.append("#define " + source["macro"] + " " +
                     _unsigned(int(p["sysclk_source"] == source["equals"])))
        auxiliary, prefix = selected.pll_sai, config["auxiliary_prefix"]
        lines.append("#define " + prefix + "_ENABLED " + _unsigned(int(auxiliary is not None)))
        for name in ("sai1", "ltdc"):
            enabled = auxiliary is not None and getattr(auxiliary, name) is not None
            lines.append("#define " + prefix + "_" + name.upper() + "_ENABLED " + _unsigned(int(enabled)))
        for controller, macro in config["auxiliary_controller_flags"].items():
            enabled = any(item.kind == "audio" and item.controller == controller and
                          item.mode.startswith("master-") for item in result.controllers)
            lines.append("#define " + macro + " " + _unsigned(int(enabled)))
        if auxiliary is not None:
            lines.extend("#define " + prefix + "_" + name + " " + _unsigned(value)
                         for name, value in (("M", auxiliary.m), ("N", auxiliary.n)))
            if auxiliary.sai1 is not None:
                output = auxiliary.sai1
                lines.extend("#define " + prefix + "_" + name + " " + _unsigned(value)
                    for name, value in (("Q", output.q), ("DIVQ", output.post_divider),
                                         ("SAI1_HZ", output.kernel_hz)))
            if auxiliary.ltdc is not None:
                output = auxiliary.ltdc
                lines.extend("#define " + prefix + "_" + name + " " + _unsigned(value)
                    for name, value in (("R", output.r), ("DIVR", output.post_divider),
                                         ("LTDC_HZ", output.pixel_hz)))
        source = config["input_source_flag"]
        lines.extend(("#ifndef " + source["macro"], "#define " + source["macro"] + " " +
            _unsigned(int(p["source"] == source["equals"])), "#endif"))
        return lines

    def _timer_pins(self, result, provider):
        rows = {"capture": [], "advanced": []}
        owners = set()
        for item in result.controllers:
            if item.kind not in {"timer_capture", "advanced_pwm"}:
                continue
            identity = item.kind, item.controller
            if identity in owners:
                fail("Duplicate selected timer pin owner")
            owners.add(identity)
            binding = provider.TIMER_ADVANCED.binding(item.kind, item.controller)
            route = binding.route(tuple((pin["pin"], pin["function"], pin["af"])
                                  for pin in item.pins)) if binding is not None else None
            if route is None or item.integration is None:
                fail("Selected timer route differs from its reviewed pin facts")
            capture = item.kind == "timer_capture"
            expected = ("capture",) if capture else ("output", "complement", "break")
            if tuple(pin[1] for pin in route.pins) != expected:
                fail("Selected timer route has invalid logical roles")
            pads = {pad.identity: pad for pad in item.integration.pads}
            for index, (identity, _, _) in enumerate(route.pins):
                pad = pads.get(identity)
                if pad is None:
                    fail("Selected timer pad lacks its canonical private binding")
                row = (item.controller, str(route.channel) + "U")
                if not capture:
                    row += (str(index) + "U",)
                row += (pad.registers, str(pad.port_index) + "U", str(pad.number) + "U")
                rows["capture" if capture else "advanced"].append(row)
        return [line for name in ("capture", "advanced")
                for line in _table(self.data["pin_tables"][name], rows[name])]

    def _adc_pins(self, result, provider):
        rows, selected = [], set()
        facts = getattr(provider, "ADC_PIN_CHANNELS", {})
        internals = getattr(provider, "ADC_INTERNAL_CHANNELS", {})
        for item in result.controllers:
            if item.kind not in {"adc", "adc_group"}:
                continue
            if item.integration is None:
                fail("Selected ADC pads require physical integration")
            options = item.options
            if item.kind == "adc":
                inputs = tuple((item.controller, channel) for channel in options.channels)
            else:
                policy = provider.ADC_GROUP
                names = policy.converter_names
                sequences = [(options.channels, options.converters)]
                if options.injected is not None:
                    sequences.append((options.injected.channels, options.injected.converters))
                inputs = []
                for channels, converters in sequences:
                    if len(converters) < len(channels):
                        fail("ADC selected pad sequence lacks converter identities")
                    for channel, converter in zip(channels, converters):
                        integer(converter, 1, len(names), "ADC selected converter")
                        inputs.append((names[converter - 1], channel))
            pads = {pad.identity: pad for pad in item.integration.pads if pad.function == "analog"}
            for converter, channel in inputs:
                if channel in internals.get(converter, ()):
                    continue
                identity = converter, channel
                if identity in selected:
                    continue
                candidates = [pin for (owner, pin), number in facts.items()
                              if owner == converter and number == channel and pin in pads]
                if len(candidates) != 1:
                    fail("ADC selected pad is missing or ambiguous")
                pad = pads[candidates[0]]
                rows.append((converter, str(channel) + "U", pad.registers, str(pad.number) + "U"))
                selected.add(identity)
        return _table(self.data["pin_tables"]["adc"], rows)
