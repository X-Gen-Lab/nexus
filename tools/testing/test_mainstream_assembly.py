"""Missing authored fixtures remain failures in the provider validator."""
from pathlib import Path
import unittest
from scripts.ci import provider_assemblies
from tools.evidence.common import EvidenceError
ROOT = Path(__file__).resolve().parents[2]
class MainstreamAssemblyTests(unittest.TestCase):
    def test_missing_authored_fixture_fails_closed_in_the_existing_validator(self):
        with self.assertRaises(EvidenceError):
            provider_assemblies.validate_fixtures(ROOT, [
                ROOT / "tests/contracts/stm32_assembly/missing.toml"])
