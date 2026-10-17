"""DMA lane topology and descriptor ranges are checked before C emission."""

import json
from types import SimpleNamespace
import unittest

from tools.configure import dma_topology as dma


def region(name="sram", **options):
    return {"id": name, "origin": 0x20000000, "size": 0x1000,
            "engines": ["DMA1"], "security": "single", "alignment": 4,
            **options}


def descriptor(name="first", **options):
    return {"id": name, "address": 0x20000000, "size": 16,
            "payload_address": 0x20000100, "payload_bytes": 32,
            "engine": "DMA1", "security": "single", "next": None, **options}


class DmaTopologyTests(unittest.TestCase):
    def test_validated_lane_projection_round_trip(self):
        provider = SimpleNamespace(
            validate_dma_resource=lambda value: (1, 1, 5))
        selected = [{"id": "adc", "controller": "ADC1",
                     "dma": ["DMA1:stream1:channel5"]}]
        plan = dma.resolve_selected(selected, provider)
        values = json.loads(json.dumps(plan.to_dict()))
        self.assertEqual(plan.to_dict(), values)
        frozen = dma.DmaTopologyIR.from_validated(values)
        values["lanes"][0]["owner"] = "changed"
        self.assertEqual(frozen, plan)

    def test_existing_lane_separates_physical_channel_from_request(self):
        provider = SimpleNamespace(
            validate_dma_resource=lambda value: (1, 1, 5))
        selected = [{"id": "adc", "controller": "ADC1",
                     "dma": ["DMA1:stream1:channel5"]}]
        plan = dma.resolve_selected(selected, provider)
        lane = plan.lanes[0]
        self.assertEqual((lane.engine, lane.lane, lane.request), ("DMA1", 1, 5))
        self.assertEqual(lane.physical_key, "dma:DMA1:stream1")

    def test_shared_lane_with_different_request_conflicts(self):
        provider = SimpleNamespace(validate_dma_resource=lambda value:
                                   (1, 1, int(value.rsplit("channel", 1)[1])))
        with self.assertRaisesRegex(ValueError, "lane"):
            dma.resolve_selected([{"id": "a", "controller": "A",
                                   "dma": ["DMA1:stream1:channel5"]},
                                  {"id": "b", "controller": "B",
                                   "dma": ["DMA1:stream1:channel6"]}], provider)

    def test_missing_request_and_invalid_provider_tuple_reject(self):
        for facts in ((1, 1, None), (True, 1, 5), (1, -1, 5)):
            with self.subTest(facts=facts), self.assertRaises(ValueError):
                provider = SimpleNamespace(
                    validate_dma_resource=lambda value: facts)
                dma.resolve_selected([{"id": "a", "controller": "A",
                                       "dma": ["DMA1:stream1"]}],
                                     provider)

    def test_valid_descriptor_plan_is_immutable(self):
        plan = dma.resolve_descriptors([descriptor()], [region()])
        self.assertEqual(plan.descriptors[0].region, "sram")
        self.assertEqual(plan.to_dict()["descriptors"][0]["payload_bytes"], 32)
        with self.assertRaises(AttributeError):
            plan.descriptors[0].size = 32

    def test_descriptor_and_payload_require_accessible_regions(self):
        for change in ({"address": 0x10000000},
                       {"payload_address": 0x10000000},
                       {"engine": "DMA2"}, {"security": "secure"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                dma.resolve_descriptors([descriptor(**change)], [region()])

    def test_bounds_alignment_and_integer_domains(self):
        for change in ({"address": 0x20000001},
                       {"payload_address": 0x20000101},
                       {"payload_bytes": 0}, {"size": True},
                       {"payload_address": 0xfffffff0, "payload_bytes": 32},
                       {"address": 0x20000ffc, "size": 16}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                dma.resolve_descriptors([descriptor(**change)], [region()])

    def test_descriptor_payload_and_descriptor_ranges_cannot_overlap(self):
        examples = ([descriptor(payload_address=0x20000008)],
                    [descriptor(), descriptor("next", address=0x20000008)],
                    [descriptor(), descriptor("next", address=0x20000110)])
        for descriptors in examples:
            with self.subTest(descriptors=descriptors), self.assertRaises(
                    ValueError):
                dma.resolve_descriptors(descriptors, [region()])

    def test_cache_line_exclusivity_is_checked(self):
        cache = region(cache_line_bytes=32)
        dma.resolve_descriptors([descriptor(size=32, payload_bytes=32)],
                                [cache])
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([descriptor(size=16)], [cache])

    def test_next_references_and_finite_chain_are_validated(self):
        plan = dma.resolve_descriptors([
            descriptor(next="second"), descriptor("second", address=0x20000020,
                                                   payload_address=0x20000200)],
            [region()])
        self.assertEqual(plan.descriptors[0].next, "second")
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([descriptor(next="missing")], [region()])

    def test_loop_needs_explicit_stoppable_owner(self):
        with self.assertRaisesRegex(ValueError, "cycle"):
            dma.resolve_descriptors([descriptor(next="first")], [region()])
        plan = dma.resolve_descriptors([descriptor(next="first")], [region()],
                                       cyclic=True, stop_owner="acquisition")
        self.assertEqual(plan.stop_owner, "acquisition")
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([descriptor(next="first")], [region()],
                                    cyclic=True)

    def test_alias_regions_and_unknown_fields_reject(self):
        for regions in ([region(), region("alias")],
                        [region(cache_line_bytes=3)], [region(typo=True)],
                        [region(security=[])]):
            with self.subTest(regions=regions), self.assertRaises(ValueError):
                dma.resolve_descriptors([], regions)
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([descriptor(typo=True)], [region()])

    def test_chain_cannot_cross_engine_or_security_owner(self):
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([
                descriptor(next="second"),
                descriptor("second", address=0x20000020,
                    payload_address=0x20000200, engine="DMA2")],
                [region(engines=["DMA1", "DMA2"])])

    def test_malformed_engine_list_is_a_configuration_error(self):
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([], [region(engines=[[]])])

    def test_payload_size_respects_transfer_alignment(self):
        with self.assertRaises(ValueError):
            dma.resolve_descriptors([descriptor(payload_bytes=3)], [region()])


if __name__ == "__main__":
    unittest.main()
