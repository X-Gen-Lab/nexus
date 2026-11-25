"""Execute the single hook/CI path policy and required-job decisions."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.ci.check_required_jobs import check_required_jobs, required_jobs
from tools.dev.change_paths import classify as classify_paths, behavioral

WORKFLOW = Path(__file__).resolve().parents[2] / ".github/workflows/ci.yml"


def classify(changes):
    return {name: str(value).lower()
            for name, value in classify_paths(path for _, path in changes).items()}


class CodeqlScopeTests(unittest.TestCase):
    page = Path("docs/development/workflow.md")
    locale = Path("docs/locale/zh_CN/LC_MESSAGES/development/workflow.po")
    initial = "# Workflow\n\nKeep the current title.\n\n```sh\nold command\n```\n"

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="nexus CodeQL scope ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Nexus test")
        self.git("config", "user.email", "test@example.invalid")
        self.write(self.page, self.initial)
        self.write(self.locale, 'msgid "Keep the current title."\n'
                               'msgstr "保留当前标题。"\n')
        self.base = self.commit()

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.root, text=True,
                              capture_output=True, check=True,
                              timeout=10).stdout.strip()

    def write(self, path, value):
        output = self.root / path
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(value, encoding="utf-8")

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "test: record input")
        return self.git("rev-parse", "HEAD")

    def decision(self, head, *, event="push", base=None, checkout=None):
        from tools.dev.change_paths import codeql_scope

        return codeql_scope(self.root, event, base or self.base, head,
                            checkout or head)["skip_cpp"]

    def test_push_admits_reviewed_prose_and_locale_updates(self):
        self.write(self.page, self.initial.replace("current title", "default title"))
        self.write(self.locale, 'msgid "Keep the default title."\n'
                               'msgstr "保留默认标题。"\n')
        self.assertTrue(self.decision(self.commit()))

    def test_edited_code_block_generated_input_and_unknown_docs_require_analysis(self):
        self.write(self.page, self.initial.replace("old command", "new command"))
        self.assertFalse(self.decision(self.commit()))
        self.write(self.page, self.initial + "\nCheck the title.\n")
        for path in ("docs/demo/assembly.toml", "docs/guide.md",
                     "tools/configure/emit.py", "dependencies/new.lock.json",
                     ".github/workflows/security.yml"):
            with self.subTest(path=path):
                self.write(path, "another input\n")
                self.assertFalse(self.decision(self.commit()))
                self.git("reset", "--hard", self.base)
                self.write(self.page, self.initial + "\nCheck the title.\n")

    def test_renames_deletions_symlinks_and_new_files_require_analysis(self):
        self.git("mv", str(self.page), "docs/renamed.md")
        self.assertFalse(self.decision(self.commit()))
        self.git("reset", "--hard", self.base)
        (self.root / self.locale).unlink()
        self.assertFalse(self.decision(self.commit()))
        self.git("reset", "--hard", self.base)
        (self.root / self.page).unlink()
        (self.root / self.page).symlink_to("missing.md")
        self.assertFalse(self.decision(self.commit()))
        self.git("reset", "--hard", self.base)
        self.write("docs/new.md", "Check the title.\n")
        self.assertFalse(self.decision(self.commit()))

    def test_gitlink_and_locale_code_changes_require_analysis(self):
        self.git("config", "diff.ignoreSubmodules", "all")
        self.git("update-index", "--add", "--cacheinfo",
                 "160000," + self.base + ",ext/provider")
        self.git("commit", "-qm", "test: add gitlink")
        self.assertFalse(self.decision(self.git("rev-parse", "HEAD")))
        self.git("reset", "--hard", self.base)
        self.write(self.locale, 'msgid "Keep the current title."\n'
                               'msgstr "```sh\\nnew command\\n```"\n')
        self.assertFalse(self.decision(self.commit()))

    def test_pr_checks_exact_merge_parents_and_keeps_ambiguous_events_full(self):
        self.write(self.page, self.initial + "\nCheck the title.\n")
        head = self.commit()
        tree = self.git("rev-parse", "HEAD^{tree}")
        merge = self.git("commit-tree", tree, "-p", self.base, "-p", head,
                         "-m", "test: merge input")
        self.git("checkout", "-q", "--detach", merge)
        self.assertTrue(self.decision(head, event="pull_request", checkout=merge))
        self.assertFalse(self.decision(head, event="pull_request"))
        self.assertFalse(self.decision(merge, base="0" * 40))
        self.assertFalse(self.decision(merge, base="b" * 40))
        self.assertFalse(self.decision(merge, base=head, checkout=self.base))
        self.assertFalse(self.decision(merge, base=merge))
        for event in ("schedule", "workflow_dispatch", "unknown"):
            self.assertFalse(self.decision(merge, event=event))


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
            "soc/stm32/stm32f4/controllers/spi/stm32_spi_sync.c",
            "runtime/src/nx_runtime.c", "runtime/src/nx_platform_info.c",
            "arch/arm/cortex_m/interrupt.c", "soc/stm32/stm32f4/flash.c",
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
            "soc/stm32/stm32f4/linker/stm32f407.ld",
            "Kconfig", "runtime/Kconfig", "platforms/native/Kconfig.platform",
            "configs/stm32f407_freertos_defconfig", ".config", ".config.product",
            "runtime/contracts/.config.board", "runtime/contracts/minimal_defconfig",
            "scripts/kconfig/generate_config.py", ".gitmodules", "nexus.lock",
            "dependencies/actions.lock.json", "deps/sdk.lock", "third_party/sdk/source.c",
            "requirements.txt", "runtime/contracts/requirements-build.txt",
            "runtime/contracts/pyproject.toml", "runtime/contracts/poetry.lock",
            "runtime/contracts/uv.lock", ".clang-tidy", ".clang-format",
            ".clang-format-dirs", ".editorconfig", ".pre-commit-config.yaml",
            "docs/development/coding-standards.md",
            "CONTRIBUTING.md", "CONTRIBUTING_CN.md",
        )
        for path in paths:
            with self.subTest(path=path):
                self.assertTrue(self.decisions([("modified", path)])["build-test"])

    def test_comment_policy_uses_repository_standards(self):
        from scripts.ci import style_gate
        from tools.dev import change_paths

        standard = "docs/development/coding-standards.md"
        self.assertIn(standard, change_paths.BEHAVIOR_DOCUMENTS)
        self.assertIn(standard, style_gate.CONTROL_FILES)
        retired = "docs/development/coding_standards.rst"
        self.assertNotIn(retired, change_paths.BEHAVIOR_DOCUMENTS)
        self.assertNotIn(retired, style_gate.CONTROL_FILES)
        for paths in (change_paths.BEHAVIOR_DOCUMENTS,
                      style_gate.CONTROL_FILES):
            self.assertFalse(any(path.startswith(".kiro/") for path in paths))
        self.assertTrue(self.decisions([("modified", standard)])["build-test"])

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

    def test_workflow_documentation_requires_docs_without_code_execution(self):
        for path in (".github/workflows/README.md",
                     ".github/actions/setup-build/README.md"):
            with self.subTest(path=path):
                self.assertEqual(classify_paths([path]), {
                    "code": False, "docs": True, "workflows": False})
                self.assertFalse(behavioral(path))
                self.assertFalse(self.decisions([("modified", path)])["build-test"])
                self.assertTrue(self.decisions([("modified", path)])["docs"])

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
