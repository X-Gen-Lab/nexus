"""Package protocol changes reject old declarations instead of guessing formats."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import packages
from tools.configure.tests.f4_fixture import copy_package


class PackageProtocolGenerationTests(unittest.TestCase):
    def test_previous_package_protocol_is_explicitly_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = copy_package(Path(temporary))
            path = package / "package.json"
            value = json.loads(path.read_text())
            value["interface_version"] = 1
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ValueError, "interface version"):
                packages.load(path)


if __name__ == "__main__":
    unittest.main()
