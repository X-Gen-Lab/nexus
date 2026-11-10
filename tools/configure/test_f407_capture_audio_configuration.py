"""Exact capture/audio assembly, typed storage and resource rejection."""
from dataclasses import FrozenInstanceError
import importlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, factory
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
