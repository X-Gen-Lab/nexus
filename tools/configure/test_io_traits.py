"""Selected metadata stays distinct from silicon and timing qualification."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import io_traits
from tools.configure import configure, factory
from tools.configure.ir import ControllerIR


def controller(kind, mode, provider="stm32f407", **options):
    values = {"id": "selected", "kind": kind, "controller": "SELECTED",
              "binding": "selected", "mode": mode,
              "pins": (), "route": "metadata-unit-fixture",
              "mask": 0, "initial": 0, **options}
    return ControllerIR.create(values, provider)


class SelectedTraitsTests(unittest.TestCase):
    def test_uart_records_requested_format_and_rearm_gap(self):
        item = controller("uart", "rx-dma-idle", "stm32h743", baud=115200,
                          rx_profile="blocks", irq_priority=6)
        selected = io_traits.describe(item)
        self.assertEqual(selected.fields["data_bits"], 8)
        self.assertEqual(selected.fields["requested_baud"], 115200)
        self.assertEqual(selected.fields["stop_half_bits"], 2)
        self.assertEqual(selected.continuity, "rearm_gap")
        self.assertEqual(selected.dma_policy, "domain_cache_line")
        self.assertEqual(selected.cache_line_bytes, 32)
        with self.assertRaises(TypeError):
            selected.fields["data_bits"] = 9
        with self.assertRaises(FrozenInstanceError):
            selected.continuity = "double_buffer"

    def test_continuous_mode_is_a_selected_mode_not_dma_inference(self):
        item = controller("uart", "rx-dma-continuous", baud=9600,
                          rx_profile="blocks", irq_priority=6)
        self.assertEqual(io_traits.describe(item).continuity, "double_buffer")
        self.assertEqual(io_traits.describe(item).cache_line_bytes, 0)

    def test_model_does_not_acquire_hardware_timing_or_dma_claims(self):
        item = controller("uart", "irq-byte-event", "native", baud=9600,
                          rx_profile="events", irq_priority=6)
        selected = io_traits.describe(item)
        self.assertEqual(selected.continuity, "model")
        self.assertEqual(selected.timestamp_source, "unknown")
        self.assertEqual(selected.dma_policy, "none")

    def test_spi_and_i2c_describe_configured_ceiling_not_measured_clock(self):
        spi = controller("spi", "short-poll", max_hz=1000000)
        i2c = controller("i2c", "poll-messages", max_hz=100000)
        self.assertEqual(io_traits.describe(spi).fields,
                         {"word_bits": 8, "configured_max_hz": 1000000})
        self.assertEqual(io_traits.describe(i2c).fields,
                         {"address_bits": 7, "configured_max_hz": 100000})
        self.assertNotIn("actual_hz", io_traits.describe(i2c).fields)

    def test_adc_storage_width_does_not_claim_effective_resolution(self):
        item = controller("adc", "trigger-dma", channels=[0, 1],
                          sample_times=[15, 15], reference_mv=3300,
                          irq_priority=6)
        selected = io_traits.describe(item)
        self.assertEqual(selected.fields,
                         {"storage_bits": 16, "signed_samples": 0,
                          "scan_channels": 2})
        self.assertEqual(selected.timestamp_source, "irq_observation")
        self.assertEqual(selected.continuity, "rearm_gap")

    def test_ethernet_cancel_scope_is_not_claimed_to_be_one_request(self):
        item = controller("eth", "raw-rmii", tx_count=2, rx_count=2,
                          rx_storage_bytes=1536, address=[2, 0, 0, 0, 0, 1],
                          init_timeout_us=1000, drain_timeout_us=1000,
                          irq_priority=6)
        selected = io_traits.describe(item)
        self.assertEqual(selected.abort_scope, "controller_pending")
        self.assertEqual(selected.fields["vlan"], 0)
        self.assertEqual(selected.timestamp_source, "acquire_observation")

    def test_unmaintained_provider_mode_is_rejected(self):
        item = controller("uart", "rx-dma-continuous", "native", baud=9600,
                          rx_profile="blocks", irq_priority=6)
        with self.assertRaises(ValueError):
            io_traits.describe(item)

    def test_non_enhanced_classes_do_not_create_metadata_objects(self):
        item = controller("gpio", "output", mask=1, initial=0)
        self.assertIsNone(io_traits.describe(item))

    def test_emitter_has_only_selected_typed_records_and_invalid_lookup(self):
        items = (controller("spi", "dma", irq_priority=6),)
        header, source = io_traits.emit_controllers(items)
        header, source = "\n".join(header), "\n".join(source)
        self.assertIn("nx_factory_spi_traits_t", header)
        self.assertNotIn("nx_factory_uart_traits_t", header)
        self.assertIn("nx_factory_spi_traits", source)
        self.assertIn("return NULL", source)
        self.assertIn("static const", source)
        self.assertNotIn("malloc", source)


class RevisionAdmissionTests(unittest.TestCase):
    def policy(self):
        return io_traits.RevisionPolicy(
            part="TEST123ABC", device_id=0x123, document_revision="Rev 2",
            document_sha256="1" * 64, reviewed=(0x1000,), blocked=(0x1001,))

    def test_known_revision_is_software_review_only(self):
        result = io_traits.admit_revision(self.policy(), "TEST123ABC",
                                          0x123, 0x1000)
        self.assertEqual(result.status, "software_reviewed")
        self.assertTrue(result.allowed)
        self.assertFalse(result.hardware_qualified)

    def test_unobserved_unknown_and_blocked_revisions_are_not_admitted(self):
        for observed, status in ((None, "not_observed"),
                                 (0x9999, "unreviewed"),
                                 (0x1001, "blocked")):
            with self.subTest(observed=observed):
                result = io_traits.admit_revision(self.policy(), "TEST123ABC",
                                                  0x123, observed)
                self.assertEqual(result.status, status)
                self.assertFalse(result.allowed)
                self.assertFalse(result.hardware_qualified)

    def test_wrong_part_or_device_rejects_without_revision_guess(self):
        for part, device in (("TEST123AB", 0x123), ("TEST123ABC", 0x124)):
            with self.subTest(part=part, device=device):
                with self.assertRaises(ValueError):
                    io_traits.admit_revision(self.policy(), part, device,
                                             0x1000)

    def test_policy_rejects_ambiguous_or_unidentified_review(self):
        fields = dict(part="TEST123ABC", device_id=0x123,
                      document_revision="Rev 2", document_sha256="1" * 64,
                      reviewed=(0x1000,), blocked=(0x1001,))
        for key, value in (("document_sha256", "unknown"),
                           ("document_revision", ""),
                           ("part", "TEST123ABC "),
                           ("reviewed", (0x1000, 0x1000)),
                           ("reviewed", ([],)),
                           ("blocked", (0x1000,)),
                           ("device_id", True)):
            with self.subTest(key=key):
                with self.assertRaises(ValueError):
                    io_traits.RevisionPolicy(**{**fields, key: value})

    def test_qualification_cannot_be_supplied_to_admission_record(self):
        with self.assertRaises(TypeError):
            io_traits.RevisionAdmission("software_reviewed", True, True)


class ResolvedTraitsTests(unittest.TestCase):
    def resolve(self, fixture):
        path = configure.ROOT / "tests/contracts" / fixture
        return configure.resolve_ir(path)

    def test_current_h743_factory_exposes_the_reviewed_selected_mode(self):
        result = self.resolve("h743_assembly/rx-cache.toml")
        console = next(item for item in result.controllers
                       if item.kind == "uart")
        selected = io_traits.describe(console)
        self.assertEqual(result.soc_family, "stm32h743")
        self.assertEqual(console.mode, "rx-dma-idle")
        self.assertEqual(selected.continuity, "rearm_gap")
        header, source = factory.emit(result)
        self.assertIn("nx_factory_uart_traits_t", header)
        self.assertIn("NX_IO_CONTINUITY_REARM_GAP", source)
        self.assertIn("NX_IO_DMA_POLICY_DOMAIN_CACHE_LINE", source)
        self.assertNotIn("NX_IO_CONTINUITY_DOUBLE_BUFFER", source)

    def test_current_can_and_ethernet_keep_distinct_abort_scope(self):
        for fixture, kind, scope in (
                ("can_assembly/stm32-first.toml", "can", "request"),
                ("ethernet_assembly/stm32-zg.toml", "eth",
                 "controller_pending")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture)
                item = next(item for item in result.controllers
                            if item.kind == kind)
                self.assertEqual(io_traits.describe(item).abort_scope, scope)
                header, source = factory.emit(result)
                self.assertIn(f"nx_factory_{kind}_traits_t", header)
                self.assertIn("NX_IO_ABORT_" + scope.upper(), source)

    def test_disabled_capabilities_generate_no_traits_or_dependencies(self):
        result = configure.resolve_ir(
            configure.ROOT / "tools/configure/assemblies/native-uart.toml")
        self.assertFalse(result.factory.capabilities)
        header, source = factory.emit(result)
        self.assertNotIn("nx_factory_uart_traits", header)
        self.assertNotIn("s_nx_traits_", source)
        self.assertNotIn("nexus/io/observation.h", header)


if __name__ == "__main__":
    unittest.main()
