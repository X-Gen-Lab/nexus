"""Exact PLL M/N/P/Q arithmetic over explicit numeric and consumer facts.

This clock model computes frequencies and budgets, never register encodings,
hardware operations or pin ownership. A package explicitly selects this model;
new parts sharing it provide data, while a genuinely different clock model
requires its own reviewed arithmetic contract.
"""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure import clocks, usb_otg
from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


_PARAMETERS = frozenset(("source", "input_hz", "pll_m", "pll_n", "pll_p", "pll_q",
                         "ahb_div", "apb1_div", "apb2_div", "adc_div", "supply_mv", "css"))
_OPTIONAL = frozenset(("sysclk_source", "pll48", "pll_sai"))
_RANGES = frozenset(("input_hz", "pll_m", "pll_n", "pll_q", "supply_mv",
                     "pll_input_hz", "pll_vco_hz"))
_DIVIDERS = frozenset(("pll_p", "ahb_div", "apb1_div", "apb2_div", "adc_div"))
_MAXIMUM = frozenset(("pll_sysclk", "core", "apb1", "apb2", "adc"))
_CONSUMERS = frozenset(("usb", "ethernet_min_hz", "entropy_hz", "adc_min_hz",
    "uart_divisor", "timer_prescaler_max", "i2c_quantum_hz", "i2c_standard_min_hz",
    "i2c_fast_min_hz", "i2c_fast_threshold_hz", "auxiliary_audio_controllers",
    "auxiliary_display_controller"))


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("PLL domain requires exact named facts")


def _range(value, label):
    if not isinstance(value, (tuple, list)) or len(value) != 2:
        fail("PLL " + label + " requires minimum/maximum")
    low, high = (integer(item, 1, context="PLL " + label) for item in value)
    if low > high:
        fail("PLL " + label + " has inverted bounds")
    return low, high


def _numbers(value, label):
    if not isinstance(value, (tuple, list)) or not value:
        fail("PLL " + label + " requires explicit positive values")
    values = tuple(integer(item, 1, context="PLL " + label) for item in value)
    if len(set(values)) != len(values):
        fail("PLL " + label + " requires distinct values")
    return values


def _divide(numerator, denominator, label):
    value, remainder = divmod(numerator, denominator)
    if remainder or value == 0:
        fail("Clock must have exact integer Hz: " + label)
    return value


def _bounded_product(numerator, denominator, bounds, label):
    if not bounds[0] * denominator <= numerator <= bounds[1] * denominator:
        fail("PLL " + label + " is outside its reviewed range")


def _boolean(value, label):
    if type(value) is not bool:
        fail("PLL " + label + " requires a boolean")
    return value


def _consumers(value):
    _fields(value, _CONSUMERS)
    for name in _CONSUMERS - {"usb", "uart_divisor", "auxiliary_audio_controllers",
                              "auxiliary_display_controller"}:
        integer(value[name], 1, context="PLL consumer " + name)
    _range(value["uart_divisor"], "UART divisor")
    if not isinstance(value["usb"], (tuple, list)):
        fail("USB clock consumers require explicit records")
    keys = set()
    for row in value["usb"]:
        _fields(row, {"kind", "mode", "controller", "pll48",
                      "minimum_core_hz", "external_source"})
        if row["kind"] not in {"usb_device", "usb_host"} or not isinstance(row["mode"], str):
            fail("USB clock consumer requires a protocol kind and mode")
        Symbol(row["controller"])
        key = row["kind"], row["mode"]
        if key in keys:
            fail("Duplicate USB clock consumer")
        keys.add(key)
        _boolean(row["pll48"], "USB PLL48")
        _boolean(row["external_source"], "USB external source")
        integer(row["minimum_core_hz"], 1, context="USB minimum core clock")
    audio = value["auxiliary_audio_controllers"]
    if not isinstance(audio, (tuple, list)) or len(set(audio)) != len(audio):
        fail("Auxiliary audio controllers require distinct names")
    for controller in audio:
        Symbol(controller)
    if value["auxiliary_display_controller"] is not None:
        Symbol(value["auxiliary_display_controller"])


def _reviews(value, domains):
    if not isinstance(value, Mapping):
        fail("Clock reviews require named explicit contracts")
    for name, review in value.items():
        if not isinstance(name, str) or not name:
            fail("Device clock review requires a name")
        _fields(review, {"profiles", "auxiliary_allowed", "maximum_hz"},
                {"input", "css", "ahb_div", "direct_input", "pll_tuples", "auxiliary"})
        if (not isinstance(review["profiles"], (tuple, list)) or not review["profiles"] or
                any(not isinstance(profile, str) or not profile for profile in review["profiles"])):
            fail("Clock review requires explicit allowed profile names")
        _boolean(review["auxiliary_allowed"], "review auxiliary capability")
        if not isinstance(review["maximum_hz"], Mapping) or set(review["maximum_hz"]) - domains:
            fail("Clock review maximum selects an unknown domain")
        for maximum in review["maximum_hz"].values():
            integer(maximum, 1, context="Device maximum clock")
        if "input" in review:
            _fields(review["input"], {"source", "input_hz"})
            Symbol(review["input"]["source"])
            integer(review["input"]["input_hz"], 1, context="Device clock source")
            if not {"css", "ahb_div", "direct_input", "pll_tuples"}.issubset(review):
                fail("Fixed input review requires its complete direct/PLL choices")
            _boolean(review["css"], "review CSS")
            _boolean(review["direct_input"], "review direct input")
            integer(review["ahb_div"], 1, context="Review AHB divider")
            if not isinstance(review["pll_tuples"], (tuple, list)):
                fail("Clock review PLL choices require exact tuples")
            for row in review["pll_tuples"]:
                _fields(row, {"pll_m", "pll_n", "pll_p", "pll_q", "apb1_div", "apb2_div"})
                for item in row.values():
                    integer(item, 1, context="Review PLL tuple")
        if review["auxiliary_allowed"]:
            auxiliary = review.get("auxiliary")
            _fields(auxiliary, {"shared_m", "vco_min_hz", "outputs"})
            _boolean(auxiliary["shared_m"], "review shared M")
            integer(auxiliary["vco_min_hz"], context="Review auxiliary VCO minimum")
            if (not isinstance(auxiliary["outputs"], (tuple, list)) or
                    set(auxiliary["outputs"]) - {"sai1", "ltdc"}):
                fail("Clock review requires known auxiliary outputs")


@dataclass(frozen=True)
class PllMnpqPolicy:
    """One immutable reviewed model configuration; no vendor-name dispatch."""

    data: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"model", "custom_profile", "default_parameters", "profiles",
            "ranges", "dividers", "internal_sources", "external_sources", "maximum_hz",
            "timer_domains", "timebase", "flash_latency_step_hz", "auxiliary",
            "controller_domains", "timer_clock_domains", "external_clock_modes",
            "consumers", "reviews"}, {"sdio_high_speed"})
        if value["model"] != "pll-mnpq":
            fail("Unmaintained explicit PLL clock model")
        Symbol(value["custom_profile"])
        _fields(value["default_parameters"], _PARAMETERS, _OPTIONAL)
        _fields(value["ranges"], _RANGES)
        for name, bounds in value["ranges"].items():
            _range(bounds, name)
        _fields(value["dividers"], _DIVIDERS)
        for name, values in value["dividers"].items():
            _numbers(values, name)
        _fields(value["maximum_hz"], _MAXIMUM)
        for name, maximum in value["maximum_hz"].items():
            integer(maximum, 1, context="PLL maximum " + name)
        integer(value["flash_latency_step_hz"], 1, context="PLL Flash latency step")
        if not isinstance(value["internal_sources"], Mapping):
            fail("PLL internal sources require exact nominal frequencies")
        for source, frequency in value["internal_sources"].items():
            Symbol(source)
            integer(frequency, 1, context="PLL internal source")
        external = value["external_sources"]
        if (not isinstance(external, (tuple, list)) or
                any(not isinstance(item, str) or not item for item in external) or
                len(set(external)) != len(external)):
            fail("PLL external sources require explicit unique identities")
        if set(external) & set(value["internal_sources"]):
            fail("PLL source cannot be both internal and external")
        if not isinstance(value["profiles"], Mapping):
            fail("PLL profiles require named records")
        for name, profile in value["profiles"].items():
            if not isinstance(name, str) or not name:
                fail("PLL profile requires an explicit name")
            _fields(profile, {"clock", "parameters"})
            clocks.normalize(profile["clock"])
            _fields(profile["parameters"], _PARAMETERS, _OPTIONAL)
        for name, domain in value["timer_domains"].items():
            Symbol(name)
            _fields(domain, {"parent", "divider", "multiplier"})
            if (domain["parent"] not in {"apb1", "apb2"} or
                    domain["divider"] != domain["parent"] + "_div"):
                fail("PLL timer domain requires a reviewed bus/divider")
            integer(domain["multiplier"], 1, context="PLL timer multiplier")
        _fields(value["timebase"], {"domain", "tick_hz"})
        if value["timebase"]["domain"] not in value["timer_domains"]:
            fail("PLL timebase must select one declared timer domain")
        integer(value["timebase"]["tick_hz"], 1, context="PLL timebase frequency")
        for field in ("controller_domains", "timer_clock_domains"):
            if not isinstance(value[field], Mapping):
                fail("PLL controller clock domains require explicit bindings")
            for controller, domain in value[field].items():
                Symbol(controller)
                if domain not in {*_MAXIMUM, "pll48", *value["timer_domains"]}:
                    fail("PLL controller selects an unknown clock domain")
        _consumers(value["consumers"])
        _reviews(value["reviews"], {*_MAXIMUM, "pll48", "sysclk", *value["timer_domains"]})
        if not isinstance(value["external_clock_modes"], Mapping):
            fail("External clock consumers require explicit controller/mode names")
        for controller, modes in value["external_clock_modes"].items():
            Symbol(controller)
            if (not isinstance(modes, (tuple, list)) or
                    any(not isinstance(mode, str) or not mode for mode in modes)):
                fail("External clock modes require explicit mode names")
        if "sdio_high_speed" in value:
            sdio = value["sdio_high_speed"]
            _fields(sdio, {"profile", "peripheral_domain", "kernel_hz", "normal_kernel_hz",
                "minimum_divisor", "normal_minimum_divisor", "bus_width", "protocols",
                "forbidden_kinds"})
            if (not isinstance(sdio["profile"], str) or not sdio["profile"] or
                    sdio["peripheral_domain"] not in {"apb1", "apb2"} or
                    not isinstance(sdio["protocols"], (tuple, list)) or
                    set(sdio["protocols"]) - {"sd-memory", "mmc"} or
                    not isinstance(sdio["forbidden_kinds"], (tuple, list)) or
                    any(not isinstance(kind, str) for kind in sdio["forbidden_kinds"])):
                fail("SDIO workaround requires explicit protocol/clock ownership")
            for name in ("kernel_hz", "normal_kernel_hz", "minimum_divisor",
                         "normal_minimum_divisor", "bus_width"):
                integer(sdio[name], 1, context="SDIO " + name)
        auxiliary = value["auxiliary"]
        _fields(auxiliary, {"m", "n", "input_hz", "vco_hz", "output_max_hz", "outputs"})
        for name in ("m", "n", "input_hz", "vco_hz"):
            _range(auxiliary[name], "auxiliary " + name)
        integer(auxiliary["output_max_hz"], 1, context="Auxiliary output maximum")
        if set(auxiliary["outputs"]) - {"sai1", "ltdc"}:
            fail("Auxiliary output has no maintained typed domain")
        for name, output in auxiliary["outputs"].items():
            _fields(output, {"divider", "range", "post_dividers", "frequency"})
            expected = ("q", "kernel_hz") if name == "sai1" else ("r", "pixel_hz")
            if (output["divider"], output["frequency"]) != expected:
                fail("Auxiliary output requires a reviewed divider/frequency field")
            _range(output["range"], "auxiliary divider")
            _numbers(output["post_dividers"], "auxiliary post divider")
        return cls(freeze(value))

    def _parameters(self, name, profile):
        facts = self.data
        if name in facts["profiles"]:
            reviewed = facts["profiles"][name]
            if clocks.normalize(profile) != clocks.normalize(reviewed["clock"]):
                fail("Clock profile differs from its exact reviewed tuple")
            return dict(reviewed["parameters"])
        if name != facts["custom_profile"]:
            fail("Clock requires a reviewed profile or explicit custom tuple")
        _fields(profile, _PARAMETERS, _OPTIONAL)
        return dict(profile)

    def _auxiliary(self, source, parameters):
        facts = self.data["auxiliary"]
        _fields(parameters, {"m", "n"}, facts["outputs"])
        if not set(parameters).intersection(facts["outputs"]):
            fail("Auxiliary PLL requires at least one selected output")
        m = integer(parameters["m"], *facts["m"], "Auxiliary m")
        n = integer(parameters["n"], *facts["n"], "Auxiliary n")
        _bounded_product(source, m, facts["input_hz"], "auxiliary input")
        _bounded_product(source * n, m, facts["vco_hz"], "auxiliary VCO")
        result = dict(m=m, n=n, sai1=None, ltdc=None)
        for name, output in facts["outputs"].items():
            if name not in parameters:
                continue
            selected = parameters[name]
            divider = output["divider"]
            _fields(selected, {divider, "post_divider"})
            factor = integer(selected[divider], *output["range"], "Auxiliary divider")
            post = selected["post_divider"]
            if type(post) is not int or post not in output["post_dividers"]:
                fail("Auxiliary output has an unsupported post_divider")
            if source * n > facts["output_max_hz"] * m * factor:
                fail("Auxiliary output exceeds its reviewed maximum")
            frequency = _divide(source * n, m * factor * post, "auxiliary output")
            result[name] = {**selected, output["frequency"]: frequency}
        return result

    def plan(self, profile_name, profile):
        """Resolve one exact semantic plan; register encoding belongs to C."""
        facts = self.data
        p = self._parameters(profile_name, profile)
        source_name = p["source"]
        sources = set(facts["internal_sources"]) | set(facts["external_sources"])
        if not isinstance(source_name, str) or source_name not in sources:
            fail("Clock input source is outside the selected model")
        source = integer(p["input_hz"], *facts["ranges"]["input_hz"], "Clock input Hz")
        if source_name in facts["internal_sources"] and source != facts["internal_sources"][source_name]:
            fail("Internal clock source differs from its exact nominal frequency")
        for name in ("pll_m", "pll_n", "pll_q", "supply_mv"):
            integer(p[name], *facts["ranges"][name], "Clock " + name)
        for name, allowed in facts["dividers"].items():
            if type(p[name]) is not int or p[name] not in allowed:
                fail("Unsupported clock divider: " + name)
        if _boolean(p["css"], "CSS") and source_name not in facts["external_sources"]:
            fail("Clock security requires an explicitly external source")
        p.setdefault("sysclk_source", "pll")
        p.setdefault("pll48", False)
        if type(p["sysclk_source"]) is not str or p["sysclk_source"] not in {"pll", "input"}:
            fail("SYSCLK source must select input or PLL")
        _boolean(p["pll48"], "PLL48 ownership")
        p["pll_enabled"] = p["sysclk_source"] == "pll" or p["pll48"]
        auxiliary = self._auxiliary(source, p.pop("pll_sai")) if "pll_sai" in p else None
        m, n = p["pll_m"], p["pll_n"]
        _bounded_product(source, m, facts["ranges"]["pll_input_hz"], "input")
        _bounded_product(source * n, m, facts["ranges"]["pll_vco_hz"], "VCO")
        pll_sysclk = _divide(source * n, m * p["pll_p"], "PLL P")
        sysclk = pll_sysclk if p["sysclk_source"] == "pll" else source
        core = _divide(sysclk, p["ahb_div"], "core")
        apb1 = _divide(core, p["apb1_div"], "APB1")
        apb2 = _divide(core, p["apb2_div"], "APB2")
        adc = _divide(apb2, p["adc_div"], "ADC")
        values = dict(pll_sysclk=pll_sysclk, core=core, apb1=apb1, apb2=apb2, adc=adc)
        for name, maximum in facts["maximum_hz"].items():
            if values[name] > maximum:
                fail("Clock exceeds the reviewed " + name + " calculation limit")
        for name, domain in facts["timer_domains"].items():
            values[name] = values[domain["parent"]] * (
                1 if p[domain["divider"]] == 1 else domain["multiplier"])
        timebase = facts["timebase"]
        if values[timebase["domain"]] % timebase["tick_hz"]:
            fail("Timebase requires an exact integer prescaler")
        pll48 = _divide(source * n, m * p["pll_q"], "PLL Q")
        if p["pll48"] and pll48 != 48000000:
            fail("Explicit PLL48 owner requires exactly 48 MHz")
        values.update(sysclk=sysclk, pll_sysclk=pll_sysclk if p["pll_enabled"] else 0,
                      pll48=pll48 if p["pll_enabled"] else 0)
        p["flash_wait_states"] = (core - 1) // facts["flash_latency_step_hz"]
        domain = lambda label, hz: dict(source=label, nominal_hz=hz,
                                       minimum_hz=0, maximum_hz=0)
        return dict(profile=profile_name, input=domain(source_name, source),
            domains={name: domain(name, hz) for name, hz in values.items()},
            parameters=p, pll_sai=auxiliary, reviewed_revision_ids=(),
            provenance="Selected explicit PLL M/N/P/Q clock-domain facts; physical accuracy unmeasured",
            physical_qualified=False)

    def normalize(self, profile):
        if "pll_m" not in profile:
            return clocks.normalize(profile)
        selected = self.plan(self.data["custom_profile"], profile)
        return self.projection(selected)

    @staticmethod
    def projection(selected):
        domains = selected["domains"]
        return dict(source=selected["input"]["source"], input_hz=selected["input"]["nominal_hz"],
                    **{name + "_hz": domains[name]["nominal_hz"]
                       for name in ("core", "apb1", "apb2")})

    def timer_domain(self, controller):
        domain = self.data["timer_clock_domains"].get(controller)
        if domain is None:
            fail("Timer has no declared clock domain: " + controller)
        return domain

    def controller_hz(self, result, controller):
        modes = self.data["external_clock_modes"].get(controller, ())
        if modes:
            item = next((item for item in result.controllers if item.controller == controller), None)
            if item is not None and usb_otg.device_mode(item.mode) in modes:
                return item.options.ulpi_clock_hz
        domain = self.data["controller_domains"].get(controller)
        if domain is None:
            return None
        if result.clock_plan is not None:
            return result.clock_plan.domains[domain].nominal_hz
        return result.clock.get(domain + "_hz")

    def _sdio_divider(self, selected, width, minimum_divisor):
        facts = self.data["sdio_high_speed"]
        kernel = selected["domains"]["pll48"]["nominal_hz"]
        peripheral = selected["domains"][facts["peripheral_domain"]]["nominal_hz"]
        high = selected["profile"] == facts["profile"]
        expected = facts["kernel_hz"] if high else facts["normal_kernel_hz"]
        if kernel != expected or peripheral <= 0 or type(width) is not int or width not in {1, 4, 8}:
            fail("SDIO requires its exact kernel/peripheral clock and bus width")
        # The reviewed FIFO IP has two independent bus/erratum constraints.
        bus_minimum = (3 * kernel + 8 * peripheral - 1) // (8 * peripheral)
        fifo_minimum = 3 * width * (kernel + peripheral) // (32 * peripheral) + 1
        divisor = max(facts["minimum_divisor"] if high else facts["normal_minimum_divisor"],
                      minimum_divisor, bus_minimum, fifo_minimum)
        divisor = (divisor + 1) & ~1
        if divisor > 256:
            fail("Selected SDIO FIFO clock model has no legal divider")
        return divisor - 2

    def validate_consumers(self, selected, controllers):
        """Check explicit I/O clock contracts against the one resolved plan."""
        facts = self.data["consumers"]
        domain = lambda name: selected["domains"][name]["nominal_hz"]
        auxiliary = selected.get("pll_sai")
        audio = any(item.kind == "audio" and item.controller in facts["auxiliary_audio_controllers"]
                    and item.mode.startswith("master-") for item in controllers)
        display = any(item.kind == "display" and item.controller == facts["auxiliary_display_controller"]
                      and item.mode == "rgb-scanout" for item in controllers)
        if auxiliary is not None and auxiliary["sai1"] is not None and not audio:
            fail("Auxiliary Q requires its explicitly selected audio master consumer")
        if audio and (auxiliary is None or auxiliary["sai1"] is None):
            fail("Audio master requires the one declared System auxiliary Q plan")
        if auxiliary is not None and auxiliary["ltdc"] is not None and not display:
            fail("Auxiliary R requires its explicitly selected display consumer")
        if display and (auxiliary is None or auxiliary["ltdc"] is None):
            fail("Display requires the one declared System auxiliary R plan")
        sdio = self.data.get("sdio_high_speed")
        high = sdio is not None and selected["profile"] == sdio["profile"]
        if high and not any(item.kind == "block" for item in controllers):
            fail("High-speed SDIO clock profile requires its selected memory host")
        for item in controllers:
            if high and item.kind in sdio["forbidden_kinds"]:
                fail("High-speed SDIO workaround forbids the selected shared clock owner")
            if item.kind in {"eth", "eth_frames"} and domain("core") < facts["ethernet_min_hz"]:
                fail("Ethernet requires an AHB clock of at least " +
                     str(facts["ethernet_min_hz"]) + " Hz")
            if item.kind == "entropy" and domain("pll48") != facts["entropy_hz"]:
                fail("Entropy requires its exact selected nominal kernel clock")
            kernel48 = item.kind == "block" and not high
            if item.kind in {"usb_device", "usb_host"}:
                mode = usb_otg.device_mode(item.mode)
                policy = next((row for row in facts["usb"] if
                               row["kind"] == item.kind and row["mode"] == mode), None)
                if policy is None or item.controller != policy["controller"]:
                    fail("USB clock requires its exact engine/PHY mode")
                if domain("core") <= policy["minimum_core_hz"]:
                    fail("USB AHB frequency is below its engine limit")
                kernel48 = policy["pll48"]
                if policy["external_source"] and selected["input"]["source"] not in self.data["external_sources"]:
                    names = "/".join(self.data["external_sources"]).upper()
                    fail("Internal USB PHY requires an external " + names + " crystal")
            if kernel48 and domain("pll48") != 48000000:
                fail("Selected USB/SDIO requires exact nominal PLL48")
            if item.kind == "block":
                if sdio is None:
                    fail("Memory host requires an explicit SDIO FIFO clock contract")
                selected_high = getattr(item.options, "speed", "default") == "high-speed"
                if selected_high != high:
                    fail("SDIO high-speed requires its exact reviewed clock profile")
                if high and (item.options.bus_width != sdio["bus_width"] or
                             item.options.protocol not in sdio["protocols"]):
                    fail("High-speed SDIO admits only its reviewed SDR memory shape")
                self._sdio_divider(selected, item.options.bus_width,
                    sdio["minimum_divisor"] if high else sdio["normal_minimum_divisor"])
                self._sdio_divider(selected, 1, domain("pll48") // 400000 + 1)
            if item.kind in {"adc", "adc_group", "pwm_adc"} and domain("adc") < facts["adc_min_hz"]:
                fail("Selected ADC clock is below its reviewed minimum")
            controller_domain = self.data["controller_domains"].get(item.controller)
            if item.kind in {"uart", "i2c", "i2c_target", "can"}:
                if controller_domain is None:
                    fail("Selected controller lacks an explicit consumer clock domain")
                hz = domain(controller_domain)
                if item.kind == "uart":
                    divisor = (hz + item.options.baud // 2) // item.options.baud
                    if not facts["uart_divisor"][0] <= divisor <= facts["uart_divisor"][1]:
                        fail("UART baud has no admitted selected clock divider")
                elif item.kind in {"i2c", "i2c_target"}:
                    minimum = facts["i2c_fast_min_hz"] if item.options.max_hz > facts["i2c_fast_threshold_hz"] else facts["i2c_standard_min_hz"]
                    if hz % facts["i2c_quantum_hz"] or hz < minimum:
                        fail("I2C requires an integer reviewed peripheral clock")
                else:
                    timing = item.options.timing
                    if item.options.bitrate * timing.prescaler * (1 + timing.segment1 + timing.segment2) != hz:
                        fail("CAN bitrate differs from the selected clock")
            elif item.kind in {"pwm", "timer_capture", "advanced_pwm"}:
                hz = domain(self.timer_domain(item.controller))
                if hz % item.options.tick_hz or not 1 <= hz // item.options.tick_hz <= facts["timer_prescaler_max"]:
                    fail("Timer tick must divide its selected bus clock")

    def validate_clock_policy(self, name, selected=None):
        """Apply an explicit device review, never a family-name fallback."""
        review = self.data["reviews"].get(name)
        if review is None:
            fail("Unknown selected device clock policy")
        if selected is None:
            return
        auxiliary = selected.get("pll_sai")
        if selected["profile"] not in review["profiles"]:
            fail("Clock profile is outside the selected device clock policy")
        if auxiliary is not None and not review["auxiliary_allowed"]:
            fail("Selected device clock policy forbids PLLSAI")
        for domain, maximum in review["maximum_hz"].items():
            if selected["domains"][domain]["nominal_hz"] > maximum:
                fail("Selected device clock policy exceeds its domain limit")
        p = selected["parameters"]
        if "input" in review:
            if any(p[key] != value for key, value in review["input"].items()):
                fail("Selected device clock policy requires its reviewed input source")
            if p["css"] != review["css"] or p["ahb_div"] != review["ahb_div"]:
                fail("Selected device clock policy requires its reviewed source/divider")
            direct = (review["direct_input"] and p["sysclk_source"] == "input" and
                      not p["pll_enabled"] and p["apb1_div"] == p["apb2_div"] == 1)
            high = (p["sysclk_source"] == "pll" and p["pll_enabled"] and any(
                all(p[key] == value for key, value in row.items()) for row in review["pll_tuples"]))
            if not (direct or high):
                fail("Selected device clock policy requires a reviewed direct or PLL tuple")
        if auxiliary is not None:
            limits = review["auxiliary"]
            if limits["shared_m"] and auxiliary["m"] != p["pll_m"]:
                fail("Auxiliary shared PLLM differs from the selected System input")
            if p["input_hz"] * auxiliary["n"] < limits["vco_min_hz"] * auxiliary["m"]:
                fail("Auxiliary VCO must be at least " +
                     str(limits["vco_min_hz"] // 1000000) + " MHz")
            for output in ("sai1", "ltdc"):
                if auxiliary[output] is not None and output not in limits["outputs"]:
                    divider = self.data["auxiliary"]["outputs"][output]["divider"].upper()
                    fail("Selected device has no reviewed PLLSAI " + divider + " output")
