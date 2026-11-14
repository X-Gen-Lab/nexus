"""Stable provider hooks over the already selected immutable domain snapshot.

No family module is imported here. Public protocol grammar remains in its
existing domain; package data narrows controller and electrical support.
"""
from tools.configure import clocks, dac
from tools.configure.domain_policies import DomainPolicies
from tools.configure.providers import common


_EMPTY = DomainPolicies()
_DOMAIN_ATTRIBUTES = frozenset(DomainPolicies.__dataclass_fields__) - {"clock"}
_HARDWARE_ATTRIBUTES = frozenset(("ADC_PIN_CHANNELS", "ADC_INTERNAL_CHANNELS",
    "DAC_PIN_CHANNELS", "PWM_TIMERS", "PWM_PIN_CHANNELS", "ADC_TRIGGERS",
    "DAC_DMA_ROUTES", "CONTROLLER_BINDINGS", "DMA_IRQS", "SECONDARY_FACTS",
    "FACT_SHA256", "FIXED_DMA"))


class DomainBridge:
    """Explicit narrow contracts, without a second configuration authority."""

    def __getattr__(self, name):
        if name in _DOMAIN_ATTRIBUTES:
            return getattr(self._domains(), name)
        if name in _HARDWARE_ATTRIBUTES:
            facts = object.__getattribute__(self, "hardware_facts")
            return getattr(facts, name)
        if name == "PWM_SHARED":
            return None
        raise AttributeError(name)

    def _domains(self):
        selected = object.__getattribute__(self, "domain_policies")
        return selected if selected is not None else _EMPTY

    def _clock(self):
        return self._domains().clock

    def normalize_clock(self, profile):
        selected = self._clock()
        return selected.normalize(profile) if selected is not None else clocks.normalize(profile)

    def clock_plan(self, name, profile):
        selected = self._clock()
        return selected.plan(name, profile) if selected is not None else clocks.plan(name, profile)

    def validate_clock_policy(self, policy, selected=None):
        clock = self._clock()
        if clock is None:
            if policy is not None:
                common.fail("Selected package declares no reviewed clock policy")
            return
        clock.validate_clock_policy(policy, selected)

    def timer_clock_domain(self, controller):
        clock = self._clock()
        if clock is None:
            common.fail("Selected package declares no timer clock domain")
        return clock.timer_domain(controller)

    def controller_clock_hz(self, result, controller):
        clock = self._clock()
        if clock is not None:
            return clock.controller_hz(result, controller)
        return self.soc["controllers"][controller].get("clock_hz", result.clock["core_hz"])

    def validate_clock_consumers(self, plan, selected):
        clock = self._clock()
        if clock is not None:
            clock.validate_consumers(plan, selected)

    def validate_selection(self, selection, route):
        kind, mode = route["kind"], selection["mode"]
        binding = self.binding(kind, mode, route["controller"])
        for name, bounds in binding.limits.items():
            if name in selection:
                common.integer(selection[name], *bounds, context="Binding " + name)
        self.mode_implementation(kind, mode, route["controller"])
        domains = self._domains()
        if domains.GPIO_PHASE is not None:
            domains.GPIO_PHASE.validate_output(selection, route)
        if kind in {"display", "image"}:
            if domains.EXTERNAL_BUS is None:
                common.fail("Selected package has no graphics policy")
            domains.EXTERNAL_BUS.validate_graphics(selection, route)
        elif kind == "adc_group":
            if domains.ADC_GROUP is None:
                common.fail("Selected package has no ADC sequencer policy")
            domains.ADC_GROUP.validate(selection, route)
        elif kind in {"capture", "audio"}:
            policy = (domains.SAI if domains.SAI is not None and
                      domains.SAI.handles(route["controller"]) else domains.CAPTURE_AUDIO)
            if policy is None:
                common.fail("Selected package has no capture/audio policy")
            policy.validate(selection, route)
        elif kind in {"pwm_group", "timer_pulse", "timer_sequence"}:
            if domains.WAVEFORM is None:
                common.fail("Selected package has no waveform policy")
            domains.WAVEFORM.validate(selection, route)
        if domains.SELECTION is not None:
            domains.SELECTION.validate(selection, route, self.hardware_facts)

    def validate_mode_context(self, selection, route, *, backend, clock_profile,
                              part=None, clock_plan=None):
        if backend not in self.software["backends"]:
            common.fail("Backend is outside the selected package binding scope")
        external = self._domains().EXTERNAL_BUS
        if external is not None:
            external.validate_context(selection, route, backend=backend,
                clock_profile=clock_profile, part=part, clock_plan=clock_plan)
        policy = self._domains().SELECTION
        if policy is not None:
            policy.validate_context(selection, route, backend=backend,
                clock_profile=clock_profile, part=part, clock_plan=clock_plan)

    def validate_dma_resource(self, resource):
        return common.validate_dma_resource(resource, self.DMA_TOPOLOGY)

    def dma_irq(self, resource):
        return common.dma_irq(resource, self.DMA_VECTOR, self.DMA_TOPOLOGY)

    def validate_dma(self, route, mode, normalize):
        del normalize
        kind = route["kind"]
        paths = tuple(common.route_dma(route, mode))
        traits = common.mode_traits(kind, mode)
        expected_count = (int(traits.tx_dma) + int(traits.rx_dma)
                          if kind in {"uart", "spi", "i2c"} else None)
        if expected_count is not None and len(paths) != expected_count:
            common.fail("DMA route must provide exactly its selected directions")
        physical = []
        for path in paths:
            engine, lane, request = self.validate_dma_resource("dma:" + path)
            if request is None:
                common.fail("DMA route requires its explicit request selector")
            physical.append((engine, lane))
        if len(set(physical)) != len(physical):
            common.fail("Selected DMA directions cannot alias one physical lane")
        if expected_count is not None:
            return
        domains = self._domains()
        if kind in {"capture", "audio"}:
            policy = (domains.SAI if domains.SAI is not None and
                      domains.SAI.handles(route["controller"]) else domains.CAPTURE_AUDIO)
            expected = policy.dma_paths(route, mode) if policy is not None else None
        elif kind == "timer_sequence":
            expected = (domains.WAVEFORM.dma_paths(route, mode)
                        if domains.WAVEFORM is not None else None)
        elif domains.SELECTION is not None:
            expected = domains.SELECTION.dma_paths(route, mode, self.hardware_facts)
        else:
            expected = None
        common.validate_dma(route, mode, expected, self.package.family)

    def shares_irq(self, kind, key):
        return self._domains().shares_irq(kind, key)

    def mode_fields(self, selection, route, *, clock_profile):
        del selection, route, clock_profile
        return {}

    def validate_controller_set(self, selected, *, clock_profile):
        del clock_profile
        domains = self._domains()
        if domains.WAVEFORM is not None:
            domains.WAVEFORM.resolve_edges(selected)
        dac.resolve_sources(selected, triggers=domains.DAC_TRIGGERS)
        policy = domains.CAPTURE_AUDIO
        if policy is not None and hasattr(policy, "validate_controller_set"):
            policy.validate_controller_set(selected)

    @staticmethod
    def validate_endpoint_clock(speed, *, clock_plan):
        common.validate_endpoint_clock(speed, clock_plan=clock_plan)

    @staticmethod
    def builtin_cs(controller):
        del controller
        return False

    def capture_audio_fields(self, selection, route, clock, clock_plan):
        domains = self._domains()
        policy = (domains.SAI if domains.SAI is not None and
                  domains.SAI.handles(route["controller"]) else domains.CAPTURE_AUDIO)
        if policy is None:
            common.fail("Selected package has no capture/audio policy")
        return policy.resolve_fields(selection, route, clock, clock_plan)

    def silicon_policy(self, result, item=None):
        from tools.configure.selected_contracts import SiliconPolicy
        if item is not None or result.clock_plan is None:
            return None
        return SiliconPolicy(part=result.part, provider=result.soc_family,
            profile=result.clock_plan.profile,
            reviewed_revision_ids=result.clock_plan.reviewed_revision_ids,
            review_scope=result.clock_plan.provenance +
                "; selected electrical and mode constraints remain unqualified physically.")

    def selected_format(self, item, shape):
        policy = self._domains().SELECTION
        return policy.selected_format(item, shape) if policy is not None else {}

    def configuration_header(self, result):
        header = self._domains().HEADER
        return header.render(result, self) if header is not None else []
