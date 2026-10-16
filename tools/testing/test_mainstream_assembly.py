"""Explicit mainstream software fixtures use the ordinary platform build graph."""
from pathlib import Path
import unittest

import yaml

from scripts.ci import provider_assemblies
from tools.evidence.common import EvidenceError


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/contracts/mainstream_assembly"
EXPECTED = {
    "f103-io.toml", "f103-io-freertos.toml",
    "f103-spi.toml", "f103-spi-freertos.toml",
    "f103-watchdog.toml", "f103-exti.toml",
    "gd303-io.toml", "gd303-io-freertos.toml",
    "gd303-spi-watchdog.toml", "gd303-spi-watchdog-freertos.toml"}


class MainstreamAssemblyTests(unittest.TestCase):
    def test_workflow_admits_one_mainstream_job_without_duplicate_existing_families(self):
        workflow = yaml.safe_load((ROOT / ".github/workflows/build-matrix.yml").read_text())
        fixtures = workflow["jobs"]["controller-routes"]["strategy"]["matrix"]["fixture"]
        for name in ("mainstream_assembly", "g474_assembly", "h743_assembly", "u575_assembly"):
            with self.subTest(fixture=name):
                self.assertEqual(fixtures.count(name), 1)

    def test_every_explicit_reference_exists_and_resolves_without_mutation(self):
        self.assertEqual({path.name for path in FIXTURE.glob("*.toml")}, EXPECTED)
        paths = [FIXTURE / name for name in sorted(EXPECTED)]
        original = {path: path.read_bytes() for path in paths}
        rows = provider_assemblies.validate_fixtures(ROOT, paths)
        self.assertEqual(len(rows), 10)
        self.assertEqual({row["resolved"]["soc_family"] for row in rows},
                         {"stm32f103", "gd32f303"})
        self.assertEqual({row["resolved"]["backend"] for row in rows},
                         {"baremetal", "freertos"})
        for row in rows:
            resolved = row["resolved"]
            expected = "soft" if resolved["soc_family"] == "stm32f103" else "hard"
            self.assertEqual(resolved["cpu"]["float_abi"], expected)
        self.assertEqual({path: path.read_bytes() for path in paths}, original)

    def test_missing_authored_fixture_fails_closed_in_the_existing_validator(self):
        with self.assertRaises(EvidenceError):
            provider_assemblies.validate_fixtures(ROOT, [FIXTURE / "missing.toml"])


if __name__ == "__main__":
    unittest.main()
