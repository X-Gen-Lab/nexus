"""Execute the single hook/CI path policy and required-job decisions."""

import unittest
from pathlib import Path

from scripts.ci.check_required_jobs import check_required_jobs, required_jobs
from tools.dev.change_paths import classify as classify_paths, behavioral

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"


def classify(changes):
    return {name: str(value).lower()
            for name, value in classify_paths(path for _, path in changes).items()}


class ChangePathsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        if "tools/dev/change_paths.py --github-output" not in workflow:
            raise AssertionError("CI must execute the shared path policy")
        if "dorny/paths-filter" in workflow:
            raise AssertionError("CI cannot retain a competing path policy")

    def decisions(self, changes):
        return required_jobs(classify(changes), "pull_request")

    def test_production_sources_and_runtime_boundaries_require_builds(self):
        paths = (
            "hal/src/nx_hal.c", "osal/adapters/baremetal/osal_baremetal.c",
            "framework/config/src/config.c", "services/storage/src/storage.c",
            "soc/stm32/stm32f407/controllers/spi/stm32_spi_sync.c",
            "runtime/src/nx_runtime.c", "runtime/src/nx_platform_info.c",
            "arch/arm/cortex_m/interrupt.c", "soc/stm32/stm32f407/flash.c",
            "boards/stm32f4discovery/spi.c", "runtime/support-matrix.json",
            "runtime/contracts/manifest.json", "runtime/contracts/src/main.c",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(self.decisions([("modified", path)]),
                                 {"build-test": True, "code-quality": True, "tool-contracts": True, "docs": True})

    def test_build_configuration_and_dependency_identity_require_builds(self):
        paths = (
            "CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json",
            "runtime/contracts/CMakeLists.txt", "cmake/toolchains/arm-gcc.cmake",
            "soc/stm32/stm32f407/linker/stm32f407.ld",
            "Kconfig", "runtime/Kconfig", "platforms/native/Kconfig.platform",
            "configs/stm32f407_freertos_defconfig", ".config", ".config.product",
            "runtime/contracts/.config.board", "runtime/contracts/minimal_defconfig",
            "scripts/kconfig/generate_config.py", ".gitmodules", "nexus.lock",
            "dependencies/actions.lock.json", "deps/sdk.lock", "third_party/sdk/source.c",
            "requirements.txt", "runtime/contracts/requirements-build.txt",
            "runtime/contracts/pyproject.toml", "runtime/contracts/poetry.lock",
            "runtime/contracts/uv.lock", ".clang-tidy", ".clang-format",
            ".clang-format-dirs", ".editorconfig", ".pre-commit-config.yaml",
            ".kiro/steering/comment-standards.md",
            "docs/sphinx/development/coding_standards.rst",
            "CONTRIBUTING.md", "CONTRIBUTING_CN.md",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertTrue(self.decisions([("modified", path)])["build-test"])

    def test_sdk_and_rtos_gitlink_updates_require_builds(self):
        for path in ("vendors/arm/CMSIS_6", "vendors/st/cmsis_device_f4",
                     "vendors/st/stm32f4xx_hal_driver", "ext/freertos", "ext/googletest"):
            with self.subTest(path=path):
                self.assertTrue(self.decisions([("modified", path)])["build-test"])

    def test_deletions_and_mixed_changes_cannot_justify_build_skips(self):
        for path in ("runtime/src/nx_platform_info.c", "vendors/st/cmsis_device_f4",
                     "runtime/contracts/profile.json", "cmake/toolchains/arm-gcc.cmake"):
            for changes in ([("deleted", path)],
                            [("modified", "README.md"), ("deleted", path)]):
                with self.subTest(changes=changes):
                    outputs = classify(changes)
                    decisions = required_jobs(outputs, "pull_request")
                    self.assertTrue(decisions["build-test"])
                    needs = {"changes": {"result": "success", "outputs": outputs}}
                    needs.update({job: {"result": "success"} for job in decisions})
                    self.assertTrue(check_required_jobs(needs, "pull_request").passed)
                    needs["build-test"]["result"] = "skipped"
                    self.assertFalse(check_required_jobs(needs, "pull_request").passed)

    def test_docs_only_changes_allow_build_skips_but_require_documentation(self):
        for changes in ([("modified", "README.md")],
                        [("deleted", "docs/guide/old.rst")],
                        [("modified", "docs/guide/new.rst"), ("modified", "CHANGELOG.md")]):
            with self.subTest(changes=changes):
                outputs = classify(changes)
                self.assertEqual(required_jobs(outputs, "pull_request"),
                                 {"build-test": False, "code-quality": True, "tool-contracts": False, "docs": True})
                needs = {"changes": {"result": "success", "outputs": outputs},
                         "build-test": {"result": "skipped"}, "code-quality": {"result": "success"},
                         "tool-contracts": {"result": "skipped"},
                         "docs": {"result": "success"}}
                self.assertTrue(check_required_jobs(needs, "pull_request").passed)
                needs["docs"]["result"] = "skipped"
                self.assertFalse(check_required_jobs(needs, "pull_request").passed)

    def test_workflow_and_action_changes_require_build_and_quality(self):
        for path in (".github/workflows/ci.yml", ".github/actions/setup-build/action.yml"):
            with self.subTest(path=path):
                decisions = self.decisions([("modified", path)])
                self.assertTrue(decisions["build-test"])
                self.assertTrue(decisions["code-quality"])

    def test_invalid_repository_paths_fail_instead_of_approximating(self):
        for path in ("", "../src/file.c", "/src/file.c", "src/../file.c",
                     "src//file.c", "src/./file.c", "src/file\0.c"):
            with self.subTest(path=path):
                with self.assertRaises(ValueError):
                    classify_paths([path])

    def test_hook_matches_ci_even_for_unknown_inputs_and_documentation(self):
        for path in ("new-submodule", "ext/freertos", "vendors/new_sdk",
                     "dependencies/new.lock.json", "tools/configure/README.md",
                     ".github/workflows/ci.yml", "docs/guide/new.rst"):
            with self.subTest(path=path):
                outputs = classify_paths([path])
                self.assertEqual(behavioral(path),
                                 outputs["code"] or outputs["workflows"])


if __name__ == "__main__":
    unittest.main()
