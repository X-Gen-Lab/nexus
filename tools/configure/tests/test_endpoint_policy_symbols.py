"""Endpoint assembly consumes explicit symbols and validated GPIO masks."""
from dataclasses import FrozenInstanceError
from types import SimpleNamespace
from unittest.mock import patch
import unittest

from tools.configure import configure, enhanced_endpoints
from tools.configure.calls import Symbol
from tools.configure.domain_policies import _endpoints
from tools.configure.providers.common import EndpointPolicy


class EndpointPolicySymbolTests(unittest.TestCase):
    def test_owner_factory_uses_both_selected_symbols_without_prefix_inference(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/io_evolution_assembly/f407-spi-dma-transaction.toml")
        policy = SimpleNamespace(spi_dma_owner=(Symbol("selected_membership"),
                                              Symbol("selected_notification")))
        provider = SimpleNamespace(ENDPOINT_POLICY=policy, PREFIX="incorrect")
        with patch.object(enhanced_endpoints, "_spi_provider", return_value=provider):
            source = "\n".join(enhanced_endpoints.emit_owner_factory(result)[1])
        self.assertIn(".contains = selected_membership", source)
        self.assertIn(".attach_wake = selected_notification", source)
        self.assertNotIn("incorrect", source)

    def test_explicit_symbols_are_immutable_and_independent_of_vendor_names(self):
        selected = _endpoints({
            "spi_transaction": True, "spi_transaction_ops": "polling_table",
            "spi_dma": {"controllers": ["BUS_A"], "mode": "stream"},
            "spi_dma_owner": {"contains": "membership_proof",
                              "attach_wake": "notification_binding"}})
        self.assertEqual(selected.spi_transaction_ops, Symbol("polling_table"))
        self.assertEqual(selected.spi_dma_owner,
                         (Symbol("membership_proof"), Symbol("notification_binding")))
        with self.assertRaises(FrozenInstanceError):
            selected.spi_dma_owner = None

    def test_owner_pair_rejects_missing_extra_and_executable_symbols(self):
        dma = {"controllers": ["BUS_A"], "mode": "stream"}
        for owner in ({"contains": "proof"},
                      {"contains": "proof", "attach_wake": "wake", "other": "hook"},
                      {"contains": "proof()", "attach_wake": "wake"}):
            with self.subTest(owner=owner), self.assertRaises(ValueError):
                _endpoints({"spi_dma": dma, "spi_dma_owner": owner})
        with self.assertRaises(ValueError):
            _endpoints({"spi_dma": dma})
        with self.assertRaises(ValueError):
            _endpoints({"spi_dma_owner": {"contains": "proof", "attach_wake": "wake"}})
        with self.assertRaises(ValueError):
            EndpointPolicy(spi_dma=(None, "stream"), spi_dma_owner=("proof", "wake"))

    def test_native_face_uses_explicit_operations_and_rejects_production_use(self):
        endpoint = SimpleNamespace(id="sensor", kind="spi", driver="spi-transaction")
        policy = EndpointPolicy(spi_transaction=True,
                                spi_transaction_ops=Symbol("reviewed_polling_ops"))
        provider = SimpleNamespace(MODEL=True, ENDPOINT_POLICY=policy,
                                   PREFIX="not_an_operations_namespace")
        with patch.object(enhanced_endpoints, "for_selection", return_value=provider):
            source = "\n".join(enhanced_endpoints.face(endpoint, "state", "unused", object()))
            self.assertIn("&reviewed_polling_ops, &state", source)
            provider.MODEL = False
            with self.assertRaisesRegex(ValueError, "Native|model"):
                enhanced_endpoints.face(endpoint, "state", "unused", object())

    def test_cs_uses_the_validated_mask_without_a_pin_naming_convention(self):
        provider = SimpleNamespace(MODEL=True, ENDPOINT_POLICY=EndpointPolicy(),
            builtin_cs=lambda controller: False,
            validate_endpoint_clock=lambda speed, clock_plan: None)
        controller = {"id": "bus", "kind": "spi", "mode": "short-poll",
                      "controller": "BUS_A", "max_hz": 1000000}
        binding = {"route": "select", "mask": 8, "initial": 8}
        routes = {"select": {"kind": "gpio", "pins": [{"pin": "select-pad"}]}}
        claims = []

        def resolve():
            enhanced_endpoints.resolve_device(
                {"id": "child", "controller": "bus", "driver": "spi-endpoint",
                 "cs_binding": "select"}, controller, provider,
                soc={"controllers": {"BUS_A": {}}}, clock_plan=None,
                bindings={"select": binding}, routes=routes, addresses=set(),
                chip_selects=set(), claim=lambda *args: claims.append(args),
                obj=lambda *args: None)

        resolve()
        self.assertEqual(claims, [("pin:select-pad", "child", "cs", "select")])
        for mask, initial in ((0, 0), (3, 3), (8, 0), (8, 16)):
            binding.update(mask=mask, initial=initial)
            with self.subTest(mask=mask, initial=initial), self.assertRaisesRegex(
                    ValueError, "one reviewed pin initially high"):
                resolve()


if __name__ == "__main__":
    unittest.main()
