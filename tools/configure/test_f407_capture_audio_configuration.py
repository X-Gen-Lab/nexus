"""Exact capture/audio assembly, typed storage and resource rejection."""
from dataclasses import FrozenInstanceError
import importlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, factory, selected_contracts
from tools.configure import configure as gate


class CaptureAudioConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="f407 capture audio ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def test_f407_owns_capture_and_audio_configuration(self):
        module = importlib.import_module("tools.configure.stm32f4.capture_audio")
        self.assertTrue(callable(module.constructor))
        self.assertTrue(callable(module.validate))

    def resolve(self, suffix, part="STM32F407ZGT6", routes=None):
        directory = self.directory / "board"
        directory.mkdir(exist_ok=True)
        board = {
            "schema_version": 1, "id": "f407-capture-audio-software-fixture",
            "soc": part, "soc_family": "stm32f4", "source_revision": "v1",
            "physical_pcb_revision": None, "clocks": {"hse_hz": 8000000},
            "bindings": [{"id": name, "controller": controller,
                          "route": route} for name, controller, route in
                         (routes or [("camera", "DCMI", "dcmi8-hardware-s1")])],
            "reserved_resources": ["PA13", "PA14"],
            "unknowns": ["Software fixture; no PCB or physical qualification"],
            "provenance": [{"url": "https://www.st.com/resource/en/datasheet/stm32f407vg.pdf",
                            "scope": "DS8626 Rev12 AF routes only"}]}
        (directory / "board.json").write_text(json.dumps(board))
        path = self.directory / "assembly.toml"
        path.write_text('schema = 2\nboard_package = "board"\n'
                        'backend = "baremetal"\nclock = "hse8-pll168"\n'
                        '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
                        + suffix)
        return gate.resolve_ir(path)

    @staticmethod
    def capture(mode="snapshot-raw", extra=""):
        return ('\n[capture.camera]\nbinding = "camera"\nmode = "' + mode +
                '"\npixel_clock_hz = 24000000\npoll_limit = 128\n'
                'frame_divisor = 1\npclk_falling = false\nhsync_high = false\n'
                'vsync_high = false\nirq = { priority = 5 }\n' + extra)

    @staticmethod
    def audio(mode="master-duplex", extra=""):
        return ('\n[audio.sound]\nbinding = "sound"\nmode = "' + mode +
                '"\nstandard = "philips"\nsample_bits = 24\nslot_bits = 32\n'
                'mclk = false\ncpol_high = false\npoll_limit = 4096\n'
                'irq = { priority = 5 }\n' + extra)

    def test_snapshot_is_typed_and_generated_factory_is_pure(self):
        result = self.resolve(self.capture())
        item, = result.controllers
        self.assertEqual("CaptureOptions", type(item.options).__name__)
        with self.assertRaises(FrozenInstanceError):
            item.options.pixel_clock_hz = 1
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32_capture_prepare(", source)
        self.assertIn("nx_stm32_capture_close(", source)
        self.assertIn("void DCMI_IRQHandler(void)", source)
        self.assertNotIn("HAL_DCMI_Start", source)
        header, lookup = factory.emit(result)
        self.assertIn("nx_factory_capture", header)
        self.assertNotIn("prepare(", lookup)
        self.assertEqual(1, len([claim for claim in result.claims
                                 if claim.key == "dma:DMA2:stream1"]))

    def test_embedded_sync_owns_only_clock_and_eight_data_pads(self):
        result = self.resolve(self.capture(extra="codes = [171, 128, 157, 182]\n"),
            routes=[("camera", "DCMI", "dcmi8-embedded-s1")])
        item, = result.controllers
        self.assertEqual(9, len(item.pins))
        self.assertNotIn("PA4", [pin["pin"] for pin in item.pins])
        self.assertNotIn("PB7", [pin["pin"] for pin in item.pins])

    def test_jpeg_rejects_embedded_crop_and_snapshot_division(self):
        for suffix in (self.capture("snapshot-jpeg", "crop = { x_clocks = 0, y_lines = 0, width_clocks = 1, height_lines = 1 }\n"),
                       self.capture().replace("frame_divisor = 1", "frame_divisor = 2")):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                self.resolve(suffix)
        with self.assertRaises(ValueError):
            self.resolve(self.capture("snapshot-jpeg", "codes = [1, 2, 3, 4]\n"),
                routes=[("camera", "DCMI", "dcmi8-embedded-s1")])

    def test_ve_does_not_select_unbonded_d12_d13(self):
        with self.assertRaises(ValueError):
            self.resolve(self.capture(), part="STM32F407VET6",
                routes=[("camera", "DCMI", "dcmi14-hardware-s1")])

    def test_pixel_clock_rejects_electrical_and_hclk_ratio_excess(self):
        with self.assertRaises(ValueError):
            self.resolve(self.capture().replace("24000000", "55000000"))

    def test_continuous_requires_explicit_service_bound_and_not_frames(self):
        with self.assertRaises(ValueError):
            self.resolve(self.capture("raw-blocks"))
        result = self.resolve(self.capture("raw-blocks",
                                            "max_service_gap_us = 10\n"))
        self.assertEqual(10, result.controllers[0].options.max_service_gap_us)
        self.assertNotIn("frames", result.controllers[0].options.__dict__)

    def test_audio_both_controllers_have_typed_pll_and_exact_dma(self):
        for number, route in ((2, "i2s2-duplex"), (3, "i2s3-duplex")):
            with self.subTest(number=number):
                result = self.resolve(self.audio(extra="sample_rate_hz = 48000\npll_n = 192\npll_r = 2\nmax_error_ppm = 10000\n"),
                    routes=[("sound", "I2S" + str(number), route)])
                item, = result.controllers
                self.assertEqual("AudioOptions", type(item.options).__name__)
                self.assertEqual(2, len(item.dma))
                self.assertGreater(item.options.rate_numerator, 0)
                self.assertGreater(item.options.rate_denominator, 0)
                source = bindings.emit(result)[1]
                self.assertIn("nx_stm32_audio_prepare(", source)
                self.assertIn("nx_stm32_audio_close(", source)
                self.assertIn("nx_stm32_audio_dma_irq(", source)
                self.assertIn("nx_factory_audio", factory.emit(result)[0])

    def test_slave_has_no_pll_or_fabricated_sample_rate(self):
        result = self.resolve(self.audio("slave-rx"),
            routes=[("sound", "I2S2", "i2s2-simplex")])
        options = result.controllers[0].options
        self.assertEqual((0, 0), (options.rate_numerator,
                                  options.rate_denominator))
        with self.assertRaises(ValueError):
            self.resolve(self.audio("slave-rx", "pll_n = 192\n"),
                routes=[("sound", "I2S2", "i2s2-simplex")])

    def test_spi_personality_and_adc_default_dma_conflicts_are_rejected(self):
        result = self.resolve(self.capture())
        self.assertIn("dma:DMA2:stream1", [claim.key for claim in result.claims])
        suffix = self.capture() + ('\n[adc.adc3]\nbinding = "analog"\n'
            'mode = "trigger-dma"\nchannels = [4]\nsample_times = [7]\n'
            'reference_mv = 3300\ntrigger = "tim3-trgo"\nirq = { priority = 5 }\n')
        with self.assertRaisesRegex(ValueError, "DMA2:stream1|conflict"):
            self.resolve(suffix, routes=[("camera", "DCMI", "dcmi8-hardware-s1"),
                ("analog", "ADC3", "adc3-ch4-pf6")])

    def test_audio_excludes_the_same_physical_spi_and_its_pads(self):
        suffix = self.audio("slave-tx") + (
            '\n[spi.bus]\nbinding="bus"\nmode="short-poll"\n'
            'max_hz=1000000\n')
        with self.assertRaisesRegex(ValueError, "SPI2|PB13|conflict"):
            self.resolve(suffix, routes=[
                ("sound", "I2S2", "i2s2-simplex"),
                ("bus", "SPI2", "spi2-pb13-pb14-pb15")])

    def test_continuous_audio_uses_separate_static_state_and_irq_owners(self):
        for number in (2, 3):
            for role in ("master", "slave"):
                for direction in ("tx", "rx", "duplex"):
                    with self.subTest(number=number, role=role,
                                      direction=direction):
                        mode = role + "-" + direction + "-blocks"
                        extra = ("max_sample_rate_hz = 96000\n"
                                 "max_service_gap_us = 20\n")
                        if role == "master":
                            extra += ("sample_rate_hz = 48000\npll_n = 192\n"
                                      "pll_r = 2\nmax_error_ppm = 10000\n")
                        result = self.resolve(self.audio(mode, extra), routes=[
                            ("sound", "I2S" + str(number),
                             "i2s" + str(number) +
                             ("-duplex" if direction == "duplex" else
                              "-simplex"))])
                        item, = result.controllers
                        self.assertTrue(item.options.continuous)
                        self.assertEqual(item.options.max_sample_rate_hz,
                                         96000)
                        self.assertEqual(item.options.max_service_gap_us, 20)
                        source = bindings.emit(result)[1]
                        self.assertIn("nx_stm32_audio_blocks_state_t", source)
                        self.assertIn("nx_stm32_audio_blocks_prepare(", source)
                        self.assertIn("nx_stm32_audio_blocks_close(", source)
                        self.assertIn("nx_stm32_audio_blocks_dma_irq(", source)
                        self.assertIn("nx_factory_audio", factory.emit(result)[0])
                        self.assertEqual(len(item.dma),
                                         2 if direction == "duplex" else 1)

    def test_continuous_audio_requires_both_explicit_observation_bounds(self):
        for extra in ("", "max_sample_rate_hz = 96000\n",
                      "max_service_gap_us = 20\n",
                      "max_sample_rate_hz = 0\nmax_service_gap_us = 20\n",
                      "max_sample_rate_hz = 96000\nmax_service_gap_us = 0\n"):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.resolve(self.audio("slave-rx-blocks", extra), routes=[
                    ("sound", "I2S2", "i2s2-simplex")])

    def test_finite_audio_does_not_carry_continuous_scheduling_fields(self):
        result = self.resolve(self.audio("slave-rx"), routes=[
            ("sound", "I2S2", "i2s2-simplex")])
        source = bindings.emit(result)[1]
        self.assertNotIn("nx_stm32_audio_blocks_state_t", source)
        self.assertNotIn(".max_sample_rate_hz", source)
        with self.assertRaises(ValueError):
            self.resolve(self.audio("slave-rx", "max_sample_rate_hz = 96000\n"
                                    "max_service_gap_us = 20\n"), routes=[
                ("sound", "I2S2", "i2s2-simplex")])

    def test_frame_sessions_select_cold_owner_and_explicit_frame_units(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6", "STM32F417ZGT6"):
            for mode in ("frames-raw", "frames-jpeg"):
                with self.subTest(part=part, mode=mode):
                    result = self.resolve(self.capture(mode), part=part)
                    item, = result.controllers
                    source = bindings.emit(result)[1]
                    self.assertIn("nx_stm32_capture_frames_state_t", source)
                    self.assertIn("nx_stm32_capture_config_t", source)
                    self.assertNotIn("nx_stm32_capture_frames_config_t", source)
                    for operation in ("admit", "prepare", "close"):
                        self.assertIn("nx_stm32_capture_frames_" + operation + "(", source)
                    self.assertIn('#include "stm32f407_capture_frames.h"', source)
                    self.assertIn("nx_stm32_capture_irq(&s_nx_port_camera.base)", source)
                    self.assertIn("nx_stm32_capture_dma_irq(&s_nx_port_camera.base)", source)
                    self.assertIn("&nx_stm32_capture_frames_ops", source)
                    self.assertIn(".jpeg = " + str(mode == "frames-jpeg").lower(), source)
                    self.assertIn(".continuous = false", source)
                    description = selected_contracts.describe(item)
                    self.assertEqual(description.unit, "frames")
                    self.assertIsNone(description.unit_bytes)
                    self.assertEqual(description.buffer_alignment, 4)
                    self.assertEqual(description.continuity, "rearm_gap")
                    self.assertEqual(description.abort_scope, "controller_pending")
                    self.assertIn("nx_factory_capture", factory.emit(result)[0])

    def test_frame_sessions_preserve_snapshot_and_jpeg_hardware_limits(self):
        for mode in ("frames-raw", "frames-jpeg"):
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, "frame_divisor"):
                self.resolve(self.capture(mode).replace("frame_divisor = 1", "frame_divisor = 2"))
        with self.assertRaisesRegex(ValueError, "JPEG requires|Unsupported mode frames-jpeg"):
            self.resolve(self.capture("frames-jpeg", "codes=[1,2,3,4]\n"),
                         routes=[("camera", "DCMI", "dcmi8-embedded-s1")])
        with self.assertRaisesRegex(ValueError, "JPEG requires|Unsupported mode frames-jpeg"):
            self.resolve(self.capture("frames-jpeg", "crop={x_clocks=0,y_lines=0,width_clocks=1,height_lines=1}\n"))
        with self.assertRaisesRegex(ValueError, "JPEG requires|Unsupported mode frames-jpeg"):
            self.resolve(self.capture("frames-jpeg"),
                         routes=[("camera", "DCMI", "dcmi10-hardware-s1")])
        with self.assertRaisesRegex(ValueError, "maximum service gap"):
            self.resolve(self.capture("frames-raw", "max_service_gap_us=10\n"))

    def test_jpeg_blocks_select_only_opaque_word_storage(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6", "STM32F417ZGT6"):
            for lane in (1, 7):
                with self.subTest(part=part, lane=lane):
                    result = self.resolve(self.capture("jpeg-blocks", "max_service_gap_us = 10\n").replace("frame_divisor = 1", "frame_divisor = 2"),
                        part=part, routes=[("camera", "DCMI", "dcmi8-hardware-s" + str(lane))])
                    item, = result.controllers
                    self.assertEqual("CaptureOptions", type(item.options).__name__)
                    self.assertEqual(8, item.options.bits)
                    self.assertEqual(10, item.options.max_service_gap_us)
                    source = bindings.emit(result)[1]
                    self.assertIn(".jpeg = true", source)
                    self.assertIn(".continuous = true", source)
                    self.assertIn("nx_stm32_capture_prepare(", source)
                    self.assertIn("&nx_stm32_capture_ops", source)
                    self.assertNotIn("nx_stm32_capture_frames", source)
                    contract = selected_contracts.describe(item)
                    self.assertEqual("words", contract.unit)
                    self.assertEqual(4, contract.unit_bytes)
                    self.assertEqual(4, contract.buffer_alignment)
                    self.assertEqual("double_buffer", contract.continuity)
                    self.assertEqual("controller_pending", contract.abort_scope)
                    self.assertIn("nx_factory_capture", factory.emit(result)[0])

    def test_jpeg_blocks_reject_unbounded_or_incompatible_hardware(self):
        for extra in ("", "max_service_gap_us = 0\n"):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.resolve(self.capture("jpeg-blocks", extra))
        for route in ("dcmi8-embedded-s1", "dcmi10-hardware-s1", "dcmi12-hardware-s7", "dcmi14-hardware-s7"):
            extra = "max_service_gap_us = 10\n"
            if "embedded" in route:
                extra += "codes = [1,2,3,4]\n"
            with self.subTest(route=route), self.assertRaises(ValueError):
                self.resolve(self.capture("jpeg-blocks", extra), routes=[("camera", "DCMI", route)])
        with self.assertRaises(ValueError):
            self.resolve(self.capture("jpeg-blocks", "max_service_gap_us=10\ncrop={x_clocks=0,y_lines=0,width_clocks=1,height_lines=1}\n"))
