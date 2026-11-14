"""Common hardware projection uses explicit facts and the sealed input reader."""
import hashlib
import importlib
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

from tools.configure import configure, packages


class SnapshotReader:
    def __init__(self, package):
        self.raw = {package.fact(name): package.fact(name).read_bytes()
                    for name in ("bindings", "soc", "routes", "device_catalog")}
        self.calls = []

    def __call__(self, path):
        self.calls.append(path)
        return json.loads(self.raw[path])

    def snapshot_bytes(self, path):
        return self.raw[path]


class CommonHardwareBindingTests(unittest.TestCase):
    def test_import_has_no_family_selection_or_hardware_file_loading(self):
        command = [sys.executable, "-c", (
            "import sys; import tools.configure.hardware_bindings; "
            "assert 'tools.configure.stm32f4' not in sys.modules")]
        result = subprocess.run(command, cwd=configure.ROOT,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_selected_facts_use_only_the_exact_reader_snapshot(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        reader = SnapshotReader(package)
        expected = {name: hashlib.sha256(reader.raw[package.fact(name)]).hexdigest()
                    for name in ("bindings", "soc", "routes", "device_catalog")}
        with patch.object(Path, "read_bytes", side_effect=AssertionError("unsealed read")):
            facts = module.load_hardware(package, read=reader)
        self.assertEqual(dict(facts.FACT_SHA256), expected)
        self.assertEqual(reader.calls, list(reader.raw))
        self.assertEqual(facts.FIXED_DMA["USART1", "dma-tx"],
                         ("DMA2:stream7:channel4",))

    def test_declared_protocol_accepts_author_owned_pads_engines_and_bounds(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        reader = SnapshotReader(package)
        documents = {path: json.loads(raw) for path, raw in reader.raw.items()}
        bindings = documents[package.fact("bindings")]["hardware"]
        documents[package.fact("bindings")]["pads"]["PAD_X"] = dict(
            documents[package.fact("bindings")]["pads"]["PA0"])
        channels = bindings["adc_channels"][0]["pins"]
        channels["PAD_X"] = 24
        del channels["PA0"]
        bindings["channel_bounds"]["adc"] = [0, 31]
        controllers = documents[package.fact("soc")]["controller_groups"][
            bindings["controller_group"]]["controllers"]
        for name in bindings["adc_channels"][0]["controllers"]:
            controllers[name]["pins"] = ["PAD_X" if pin == "PA0" else pin
                                         for pin in controllers[name]["pins"]]

        def rename(value):
            if isinstance(value, str):
                return value.replace("DMA1:stream", "DMA9:stream")
            if isinstance(value, dict):
                return {rename(key): rename(item) for key, item in value.items()}
            if isinstance(value, list):
                return [rename(item) for item in value]
            return value

        reader.raw = {path: json.dumps(rename(value)).encode()
                      for path, value in documents.items()}
        # A metadata codec fixture; it establishes no device, Board or SDK support.
        facts = module.load_hardware(package, read=reader)
        self.assertEqual(facts.ADC_PIN_CHANNELS["ADC1", "PAD_X"], 24)
        self.assertIn(9, dict(facts.DMA_IRQS))
        serial = next(item for item in facts.CONTROLLER_BINDINGS
                      if item.controller == "USART2")
        self.assertEqual(serial.dma("dma-tx"), ((9, 6, 4),))

    def test_unreviewed_protocol_and_invalid_channel_bounds_reject(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        for name, value in (("protocol", "arbitrary-register-program"),
                            ("channel_bounds", {"adc": [True, 15]})):
            with self.subTest(field=name):
                reader = SnapshotReader(package)
                path = package.fact("bindings")
                document = json.loads(reader.raw[path])
                document["hardware"][name] = value
                reader.raw[path] = json.dumps(document).encode()
                with self.assertRaises(ValueError):
                    module.load_hardware(package, read=reader)

    def test_absent_hardware_grants_no_tables_and_reads_no_other_facts(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        reader = SnapshotReader(package)
        path = package.fact("bindings")
        document = json.loads(reader.raw[path])
        del document["hardware"]
        reader.raw[path] = json.dumps(document).encode()
        facts = module.load_hardware(package, read=reader)
        self.assertEqual(reader.calls, [path])
        self.assertEqual(facts.CONTROLLER_BINDINGS, ())
        self.assertEqual(dict(facts.ADC_PIN_CHANNELS), {})
        self.assertEqual(dict(facts.FIXED_DMA), {})

    def test_numeric_lane_aliases_cannot_replace_an_existing_physical_owner(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        reader = SnapshotReader(package)
        path = package.fact("bindings")
        document = json.loads(reader.raw[path])
        lanes = document["hardware"]["dma_lanes"]
        lanes["dma:DMA01:stream0"] = dict(lanes["dma:DMA1:stream0"])
        reader.raw[path] = json.dumps(document).encode()
        with self.assertRaisesRegex(ValueError, "canonical|alias"):
            module.load_hardware(package, read=reader)

    def test_decoded_reader_without_original_bytes_is_rejected(self):
        module = importlib.import_module("tools.configure.hardware_bindings")
        package = packages.find(configure.ROOT, "stm32f4")
        with self.assertRaisesRegex(ValueError, "snapshot.*bytes|sealed.*reader"):
            module.load_hardware(package, read=lambda path: json.loads(path.read_bytes()))


if __name__ == "__main__":
    unittest.main()
