"""Official LL closure rejects source, license and pairing substitutions."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest

try:
    from tools.maintenance.stm32_ll_lock import PROFILES
except ImportError:
    PROFILES = None

ROOT = Path(__file__).resolve().parents[2]


class MainstreamLLTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(PROFILES, "shared official LL closure verifier missing")

    def test_each_reviewed_pair_and_original_notice_is_verified(self):
        for name, profile in PROFILES.items():
            with self.subTest(family=name):
                result = profile.verify(ROOT / profile.directory,
                    ROOT / profile.lock_name, ROOT / profile.cmsis_directory)
                self.assertEqual(result["commit"], profile.commit)
                self.assertFalse(result["software_qualified"])
                self.assertEqual(result["source_kind"], "git_checkout")

    def test_lock_cannot_change_pairing_graph_license_or_revision(self):
        for name, profile in PROFILES.items():
            original = json.loads((ROOT / profile.lock_name).read_text())
            for mutate in (lambda x: x.update(commit="0" * 40),
                    lambda x: x["pairing"].update(cmsis_commit="0" * 40),
                    lambda x: x["license"].update(expression="MIT"),
                    lambda x: x["files"].pop("LICENSE.md"),
                    lambda x: x.update(unreviewed=True)):
                selected = copy.deepcopy(original)
                mutate(selected)
                with self.subTest(family=name), self.assertRaises(ValueError):
                    profile.contract(selected)

    def test_installed_pair_requires_exact_both_commits_and_original_bytes(self):
        for name, profile in PROFILES.items():
            with self.subTest(family=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, cmsis = root / "driver", root / "cmsis"
                source.mkdir()
                cmsis.mkdir()
                lock = json.loads((ROOT / profile.lock_name).read_text())
                for filename in lock["files"]:
                    destination = source / filename
                    destination.parent.mkdir(exist_ok=True, parents=True)
                    shutil.copyfile(ROOT / profile.directory / filename, destination)
                selected = root / "lock.json"
                selected.write_text(json.dumps(lock))
                result = profile.verify(source, selected, cmsis,
                    packaged_commit=profile.commit,
                    packaged_cmsis_commit=profile.cmsis_commit)
                self.assertEqual(result["source_kind"], "installed_source_sdk")
                with self.assertRaises(ValueError):
                    profile.verify(source, selected, cmsis,
                                   packaged_commit=profile.commit)
                (source / "LICENSE.md").write_text("substituted notice\n")
                with self.assertRaises(ValueError):
                    profile.verify(source, selected, cmsis,
                        packaged_commit=profile.commit,
                        packaged_cmsis_commit=profile.cmsis_commit)

    def test_g4_records_reviewed_comment_only_child_instead_of_false_exact_pair(self):
        profile = PROFILES["g4"]
        lock = json.loads((ROOT / profile.lock_name).read_text())
        compatibility = lock["pairing"]["compatibility"]
        self.assertEqual(compatibility["kind"], "reviewed_comment_only_child")
        self.assertNotEqual(compatibility["paired_cmsis_commit"], profile.cmsis_commit)
        self.assertEqual(compatibility["files"], ["Include/stm32g4xx.h"])

    def test_h7_reference_transports_close_qspi_mdma_and_hsem_inputs(self):
        profile = PROFILES["h7"]
        required = {
            "Inc/stm32h7xx_ll_hsem.h", "Src/stm32h7xx_hal_qspi.c",
            "Src/stm32h7xx_hal_mdma.c", "Inc/stm32h7xx_hal.h",
            "Inc/stm32h7xx_hal_def.h", "Inc/stm32h7xx_hal_qspi.h",
            "Inc/stm32h7xx_hal_mdma.h", "Inc/stm32h7xx_ll_delayblock.h",
            "Inc/Legacy/stm32_hal_legacy.h",
        }
        lock = json.loads((ROOT / profile.lock_name).read_text())
        self.assertFalse(required - profile.contract(lock))
        for name in required:
            changed = copy.deepcopy(lock)
            changed["files"].pop(name)
            with self.subTest(missing=name), self.assertRaises(ValueError):
                profile.contract(changed)


if __name__ == "__main__":
    unittest.main()
