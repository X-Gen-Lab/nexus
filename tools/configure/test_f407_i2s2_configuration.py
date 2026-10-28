"""Normal authored F407 I2S2 resolution owns one finite SPI2 personality."""

import dataclasses
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure


ROOT = Path(__file__).resolve().parents[2]
PROFILE = "slave-tx-stereo16-pb12-pb13-pb15"
SELECTED = "NEXUS_F407_I2S2_SLAVE_TX_SELECTED"
PROFILE_VARIABLE = "NEXUS_F407_I2S2_PROFILE"
FIXTURES = (
    ("sky-baremetal-empty.toml", "STM32F407VET6"),
    ("qiming-baremetal-empty.toml", "STM32F407ZGT6"),
)
CLAIMS = frozenset((
    "controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
    "dma:DMA1:stream4", "irq:DMA1_Stream4", "irq:SPI2",
))
INPUT_IDENTITIES = {}


def record_inputs(paths):
    """Retain the exact read-only facts and temporary authored input bytes."""
    for actual in paths.values():
        path = Path(actual)
        INPUT_IDENTITIES[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()


class F407I2s2ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.serial = 0

    def assembly(self, name="sky-baremetal-empty.toml", *, selected=True,
                 backend=None, clock=None, extra="", fixture=None):
        source = (ROOT / "tools/configure/assemblies" / name
                  if fixture is None else fixture)
        contents = source.read_text(encoding="utf-8")
        # Preserve a real external Board package when moving its authored
        # fixture into this test's isolated output directory.
        match = re.search(r'^board_package\s*=\s*"([^"]+)"', contents, re.M)
        if match is not None:
            package = (source.parent / match[1]).resolve()
            contents = contents[:match.start()] + (
                "board_package = " + json.dumps(str(package))) + contents[match.end():]
        for field, value in (("backend", backend), ("clock", clock)):
            if value is not None:
                contents, count = re.subn(
                    rf'^{field}\s*=\s*"[^"]+"',
                    field + " = " + json.dumps(value), contents, count=1,
                    flags=re.M)
                self.assertEqual(count, 1)
        if selected:
            contents += '\n[i2s2]\nprofile = "' + PROFILE + '"\n'
        contents += extra
        self.serial += 1
        path = self.work / f"assembly-{self.serial}.toml"
        path.write_text(contents, encoding="utf-8")
        return path

    def resolve(self, assembly):
        inputs = {}
        result = configure.resolve_ir(assembly, ROOT, input_paths=inputs)
        record_inputs(inputs)
        return result

    def generate(self, assembly, output):
        result = configure.configure(assembly, output, ROOT)
        record_inputs(json.loads((output / "input_paths.json").read_text()))
        return result

    def cli(self, assembly, output):
        # Execute the real CLI main with its ordinary argv. This sparse overlay
        # has no complete generator-code tree: bind the unchanged dependency
        # snapshot reader to primary before the script's absolute imports.
        # Owned changed modules are independently hash-bound by the runner;
        # complete normal-CMake/SDK identity closure is a separate integration.
        environment = dict(os.environ)
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        environment["PYTHONPATH"] = os.pathsep.join((
            str(Path(configure.__file__).parent),
            str(ROOT / "tools/configure"), str(ROOT)))
        bootstrap = (
            "import runpy,sys;"
            "sys.path.insert(0,sys.argv.pop(1));"
            "import dependencies;"
            "sys.path.insert(0,sys.argv[1]);"
            "sys.argv.pop(1);"
            "sys.argv=sys.argv[1:];"
            "runpy.run_path(sys.argv[0],run_name='__main__')")
        result = subprocess.run(
            [sys.executable, "-B", "-c", bootstrap,
             str(ROOT / "tools/configure"), str(Path(configure.__file__).parent),
             str(Path(configure.__file__)), "generate",
             "--source-root", str(ROOT), "--assembly", str(assembly),
             "--output", str(output)],
            env=environment, cwd=self.work, capture_output=True, text=True,
            check=False, timeout=60)
        if result.returncode == 0:
            record_inputs(json.loads((output / "input_paths.json").read_text()))
        return result

    def assert_selection(self, output, selected):
        header = (output / "nexus_config.h").read_text(encoding="utf-8")
        selection = (output / "selection.cmake").read_text(encoding="utf-8")
        self.assertIn(f"#define {SELECTED} {int(selected)}u\n", header)
        self.assertEqual(len(re.findall(rf"^#define {SELECTED}\s", header, re.M)), 1)
        state = "ON" if selected else "OFF"
        self.assertIn(f"set({SELECTED} {state})\n", selection)
        profile = PROFILE if selected else ""
        self.assertIn(f'set({PROFILE_VARIABLE} "{profile}")\n', selection)

    def test_default_empty_assemblies_keep_role_and_hardware_claims_absent(self):
        for name, part in FIXTURES:
            with self.subTest(part=part):
                result = self.resolve(self.assembly(name, selected=False))
                self.assertIsNone(result.i2s2)
                self.assertEqual(result.part, part)
                self.assertEqual(result.controllers, ())
                self.assertEqual(result.devices, ())
                self.assertFalse(any(claim.owner == "i2s2"
                                     for claim in result.resource_plan.claims))
                self.assertEqual(result.components, ())

    def test_selected_exact_parts_have_immutable_role_and_physical_claims(self):
        for name, part in FIXTURES:
            with self.subTest(part=part):
                result = self.resolve(self.assembly(name))
                role = result.i2s2
                self.assertTrue(dataclasses.is_dataclass(role))
                self.assertEqual(role.profile, PROFILE)
                self.assertEqual(role.part, part)
                self.assertIsInstance(role.claims, tuple)
                with self.assertRaises((AttributeError, TypeError)):
                    role.profile = "unreviewed"
                with self.assertRaises(dataclasses.FrozenInstanceError):
                    result.i2s2 = None
                claims = [claim for claim in result.resource_plan.claims
                          if claim.owner == "i2s2"]
                self.assertEqual({claim.key for claim in claims}, CLAIMS)
                self.assertEqual(len(claims), len(CLAIMS))
                self.assertTrue(all(claim.mode == PROFILE and
                                    claim.policy == "exclusive"
                                    for claim in claims))
                self.assertNotIn("dma:DMA1:stream4:channel0",
                                 {claim.key for claim in claims})
                self.assertEqual(result.controllers, ())
                self.assertEqual(result.devices, ())
                self.assertEqual(result.components, ())
                self.assertFalse(result.physical_qualified)

    def test_role_adds_only_its_seven_normalized_resource_reservations(self):
        base = self.resolve(self.assembly(selected=False))
        selected = self.resolve(self.assembly())
        before = {claim.key for claim in base.resource_plan.claims}
        after = {claim.key for claim in selected.resource_plan.claims}
        self.assertEqual(after - before, CLAIMS)
        self.assertEqual(before - after, set())
        self.assertEqual(selected.controllers, base.controllers)
        self.assertEqual(selected.devices, base.devices)

    def test_existing_spi2_board_binding_conflicts_in_the_same_normal_ir(self):
        fixture = (ROOT / "tests/contracts/multi_controller_assembly" /
                   "stm32-ve.toml")
        ordinary = self.resolve(self.assembly(selected=False, fixture=fixture))
        self.assertTrue(any(item.controller == "SPI2"
                            for item in ordinary.controllers))
        with self.assertRaisesRegex(ValueError, r"(?i)conflict.*(?:SPI2|PB13|PB15|DMA1)"):
            self.resolve(self.assembly(fixture=fixture))

    def test_unreviewed_part_backend_and_clock_are_rejected(self):
        foreign = self.assembly(name="nucleo-u575-baremetal-empty.toml",
                                selected=False)
        self.assertEqual(self.resolve(foreign).part, "STM32U575ZIT6Q")
        with self.assertRaisesRegex(ValueError, "I2S2"):
            self.resolve(self.assembly(name="nucleo-u575-baremetal-empty.toml"))
        with self.assertRaisesRegex(ValueError, "I2S2"):
            self.resolve(self.assembly(name="sky-freertos-empty.toml"))
        with self.assertRaisesRegex(ValueError, r"(?i)clock"):
            self.resolve(self.assembly(clock="hse8-pll120"))

    def test_authored_role_rejects_unknown_profile_or_mutable_hardware_keys(self):
        assembly = self.assembly()
        assembly.write_text(assembly.read_text().replace(PROFILE, "continuous"))
        with self.assertRaisesRegex(ValueError, "I2S2"):
            self.resolve(assembly)
        for field in ("pins", "dma", "irq", "binding"):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.resolve(self.assembly(extra=f'{field} = "unreviewed"\n'))

    def test_generated_bundle_selection_clears_the_role_when_omitted_again(self):
        for name, part in FIXTURES:
            with self.subTest(part=part):
                output = self.work / part
                identities = []
                for selected in (False, True, False):
                    result = self.generate(self.assembly(name, selected=selected),
                                           output)
                    self.assert_selection(output, selected)
                    projected = json.loads((output / "resolved.json").read_text())
                    self.assertEqual(projected, result)
                    self.assertEqual(projected["part"], part)
                    self.assertEqual(projected["controllers"], [])
                    self.assertEqual(projected["devices"], [])
                    identities.append(projected["configuration_sha256"])
                self.assertNotEqual(identities[0], identities[1])
                self.assertNotEqual(identities[1], identities[2])

    def test_actual_cli_emits_selected_and_default_cmake_inputs(self):
        for selected in (False, True):
            with self.subTest(selected=selected):
                output = self.work / ("cli-selected" if selected else "cli-default")
                result = self.cli(self.assembly(selected=selected), output)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assert_selection(output, selected)
                projected = json.loads((output / "resolved.json").read_text())
                self.assertEqual(projected["part"], "STM32F407VET6")
                self.assertEqual(projected["controllers"], [])
                self.assertEqual((output / ".nexus-config-bundle").read_text().strip(),
                                 projected["configuration_sha256"])

    def test_actual_cli_rejects_invalid_role_without_publishing_a_bundle(self):
        assembly = self.assembly()
        assembly.write_text(assembly.read_text().replace(PROFILE, "continuous"))
        output = self.work / "cli-rejected"
        result = self.cli(assembly, output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("I2S2", result.stderr)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
