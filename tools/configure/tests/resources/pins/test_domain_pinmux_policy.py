"""Domain policies consume the selected package's exact numeric pinmux set."""
from pathlib import Path
import copy
import json
import unittest

from tools.configure.coupled_io import CoupledIoPolicy
from tools.configure.pinmux import NumericPinMuxPolicy
from tools.configure.policies.external_bus import ExternalBusPolicy
from tools.configure.policies.packet import PacketPolicy
from tools.configure.policies.selection import SelectionPolicy
from tools.configure.policies.capture_audio import CaptureAudioPolicy


class DomainPinMuxPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        package = Path(__file__).resolve().parents[5] / "soc/stm32/stm32f4"
        manifest = json.loads((package / "package.json").read_text())
        names = {"coupled_io", "selection", "packet", "external_bus", "capture_audio"}
        cls.documents = {}
        for path in manifest["binding_files"]:
            document = json.loads((package / path).read_text())
            domains = document.get("domains", {})
            for name in names.intersection(domains):
                if name in cls.documents:
                    raise AssertionError("Domain fixture has duplicate owners")
                cls.documents[name] = domains[name]
        if set(cls.documents) != names:
            raise AssertionError("Maintained domain fixtures are incomplete")
        cls.f4 = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": list(range(16))})
        cls.host = NumericPinMuxPolicy.from_validated({
            "encoding": "numeric", "selectors": [*range(16), 31]})
        cls.types = {"coupled_io": CoupledIoPolicy,
                     "selection": SelectionPolicy,
                     "packet": PacketPolicy,
                     "external_bus": ExternalBusPolicy,
                     "capture_audio": CaptureAudioPolicy}

    def test_capture_and_i2s_accept_only_the_selected_numeric_alternates(self):
        for alternate in (0, 31):
            with self.subTest(alternate=alternate):
                facts = self.facts("capture_audio")
                facts["capture"]["alternates"] = [alternate]
                for roles in facts["audio"]["controllers"].values():
                    for role in roles:
                        roles[role] = [alternate]
                CaptureAudioPolicy.from_validated(facts, pinmux=self.host)
                if alternate == 31:
                    self.reject_undeclared(CaptureAudioPolicy, facts)
        facts = self.facts("capture_audio")
        facts["capture"]["alternates"] = [True]
        with self.assertRaises(ValueError):
            CaptureAudioPolicy.from_validated(facts, pinmux=self.host)

    def facts(self, name):
        return copy.deepcopy(self.documents[name])

    def reject_undeclared(self, policy, facts):
        with self.assertRaisesRegex(ValueError, "not declared.*pinmux policy"):
            policy.from_validated(facts, pinmux=self.f4)

    def test_maintained_domains_accept_explicit_f4_policy(self):
        for name, policy in self.types.items():
            with self.subTest(domain=name):
                self.assertIsInstance(policy.from_validated(
                    self.facts(name), pinmux=self.f4), policy)

    def test_coupled_pad_selectors_use_the_explicit_policy(self):
        facts = self.facts("coupled_io")
        facts["routes"][0]["pins"][0][2] = 31
        selected = CoupledIoPolicy.from_validated(facts, pinmux=self.host)
        actual = selected.routes[facts["routes"][0]["id"]]["pins"][0][2]
        self.assertEqual(actual, 31)
        self.reject_undeclared(CoupledIoPolicy, facts)

    def test_encoder_target_and_card_selectors_use_the_explicit_policy(self):
        for field in ("encoder", "i2c_target", "block"):
            with self.subTest(field=field):
                facts = self.facts("selection")
                if field == "encoder":
                    controller = next(iter(facts[field]["controllers"]))
                    facts[field]["controllers"][controller] = 31
                else:
                    facts[field]["alternate"] = 31
                selected = SelectionPolicy.from_validated(
                    facts, pinmux=self.host)
                actual = (selected.data[field]["controllers"][controller]
                          if field == "encoder" else
                          selected.data[field]["alternate"])
                self.assertEqual(actual, 31)
                self.reject_undeclared(SelectionPolicy, facts)

    def test_device_and_host_usb_selectors_use_the_explicit_policy(self):
        for kind in ("device", "host"):
            with self.subTest(kind=kind):
                facts = self.facts("packet")
                mode = next(iter(facts["admission"][kind]))
                facts["admission"][kind][mode]["pins"][0][2] = 31
                selected = PacketPolicy.from_validated(facts, pinmux=self.host)
                actual = selected.data["admission"][kind][mode]["pins"][0][2]
                self.assertEqual(actual, 31)
                self.reject_undeclared(PacketPolicy, facts)

    def test_banked_sdram_and_graphics_selectors_use_the_explicit_policy(self):
        for name in ("FSMC", "FMC", "graphics"):
            with self.subTest(binding=name):
                facts = self.facts("external_bus")
                row = (facts["graphics"] if name == "graphics" else
                       facts["controllers"][name])
                row["alternate"] = 31
                selected = ExternalBusPolicy.from_validated(
                    facts, pinmux=self.host)
                actual = (selected.graphics if name == "graphics" else
                          selected.controllers[name])
                self.assertEqual(actual["alternate"], 31)
                self.reject_undeclared(ExternalBusPolicy, facts)

    def test_domains_reject_an_unvalidated_pinmux_policy(self):
        for name, policy in self.types.items():
            with self.subTest(domain=name), self.assertRaisesRegex(
                    ValueError, "numeric pinmux policy"):
                policy.from_validated(self.facts(name), pinmux=object())


if __name__ == "__main__":
    unittest.main()
