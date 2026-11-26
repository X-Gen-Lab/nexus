"""Exercise workspace ownership, build isolation and content cache boundaries."""

import json
from pathlib import Path
import re
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.dev.build_lock import build_lock
from tools.documentation import build, cli, install


PAGE = ("---\nid: home\ntype: reference\naudience: consumer\n"
        "source_language: en\n---\n\n# Start\n")


class BuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="docs build contract ")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root, self.output, self.prefix = (self.base / n for n in
                                             ("source", "output", "tools"))
        (self.root / "docs/assets/diagrams").mkdir(parents=True)
        (self.root / "docs/index.md").write_text(PAGE)
        (self.root / "core/include").mkdir(parents=True)
        (self.root / "core/include/api.h").write_text("void api(void);\n")
        (self.root / "Doxyfile").write_text("INPUT = core/include\n")
        (self.root / "LICENSE").write_text("source license\n")
        (self.root / "dependencies").mkdir()
        (self.root / "dependencies/platform-compatibility.json").write_text(
            json.dumps({"semantic_version": "0.2.1"}))
        (self.prefix / "licenses").mkdir(parents=True)
        (self.prefix / "licenses/tool.txt").write_text("tool license\n")
        self.context = install.ToolContext(json.dumps({"prefix": str(self.prefix),
            "installation_sha256": "a" * 64}), (("NEXUS_BUILD_JOBS", "2"),))
        self.enterContext(patch.object(install, "context", return_value=self.context))
        self.source = {"commit": "1" * 40, "tree": "2" * 40, "dirty": False}
        self.enterContext(patch.object(build, "observe_git_source", return_value=self.source))
        self.source_scan = build.check.scan
        self.enterContext(patch.object(build.check, "scan", return_value={"errors": []}))
        self.enterContext(patch.object(build.check, "validate_translations",
            return_value={"status": "passed", "errors": [], "records": []}))
        self.enterContext(patch.object(build.diagrams, "check", return_value={"errors": []}))
        self.graph = self.enterContext(patch.object(build.diagram_localization, "write",
                                                  side_effect=self.write_graph))
        self.commands, self.environments = [], {}
        self.port = self.enterContext(patch.object(build.process, "run",
                                                  side_effect=self.command))

    def write_graph(self, _folder, *, output, languages, **_kwargs):
        for language in languages:
            target = output / language
            target.mkdir(parents=True)
            (target / "flow.svg").write_text("<svg/>\n")

    def command(self, command, **kwargs):
        stage = kwargs["stage"]
        self.commands.append((stage, list(command)))
        self.environments[stage] = dict(kwargs["env"])
        log = kwargs["log"]
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text("actual external port fixture\n")
        receipt = log.with_suffix(".receipt.json")
        receipt.write_text(json.dumps({"exit_code": 0}))
        if stage == "documentation-doxygen":
            config = Path(command[-1]).read_text()
            self.assertIn("GENERATE_HTML = NO", config)
            folder = Path(re.search(r'^OUTPUT_DIRECTORY = "(.*)"$', config,
                                    re.MULTILINE)[1]) / "xml"
            folder.mkdir(parents=True)
            (folder / "index.xml").write_text("<doxygenindex/>\n")
        elif stage == "documentation-gettext":
            folder = Path(command[-1])
            folder.mkdir(parents=True, exist_ok=True)
            (folder / "index.pot").write_text('msgid "Start"\nmsgstr ""\n')
        elif stage.startswith("documentation-html-"):
            folder = Path(command[-1])
            folder.mkdir(parents=True, exist_ok=True)
            (folder / "index.html").write_text(kwargs["env"]["NEXUS_DOCS_LANGUAGE"])
        return SimpleNamespace(returncode=0, stdout="", stderr="", receipt_path=receipt)

    def invoke(self, **kwargs):
        return build.build(self.root, self.output, **kwargs)

    def test_build_shares_include_discovery_and_keeps_one_graph_validation(self):
        folder = self.root / 'parts'
        folder.mkdir()
        (folder / 'fragment.txt').write_text(
            '```{literalinclude} nested.c\n```\n')
        (folder / 'nested.c').write_text('int included_input;\n')
        with (self.root / 'docs/index.md').open('a') as page:
            page.write('\n```{include} ../parts/fragment.txt\n```\n')
        (self.root / 'docs/assets/diagrams/diagram-index.csv').write_text(
            'diagram_id,source_dot,rendered_svg,render_status\n'
            'FIG-01,flow.dot,flow.svg,rendered\n')
        with patch.object(build.check, 'scan', wraps=self.source_scan), \
                patch.object(build.check, 'dependencies',
                             wraps=build.check.dependencies) as discover:
            self.invoke()
            # Output protection, admission and the final observation each
            # independently discover inputs; admission must not parse twice.
            self.assertEqual(discover.call_count, 3)
        graph = build.diagrams.check
        graph.assert_called_once_with(self.root,
            env=self.context.environment(), full=True)
        self.assertIn('parts/nested.c', build.inputs(self.root))
        graph.reset_mock()
        graph.return_value = {'errors': ['standalone diagram rejection']}
        result = self.source_scan(self.root)
        self.assertIn('standalone diagram rejection', result['errors'])
        graph.assert_called_once_with(self.root, full=False)

    def test_build_still_rejects_a_diagram_declared_unrendered(self):
        (self.root / 'docs/assets/diagrams/diagram-index.csv').write_text(
            'diagram_id,source_dot,rendered_svg,render_status\n'
            'FIG-01,flow.dot,flow.svg,pending\n')
        with patch.object(build.check, 'scan', wraps=self.source_scan):
            with self.assertRaisesRegex(ValueError, 'required diagram not rendered'):
                self.invoke()

    def test_verify_publishes_only_site_with_explicit_full_scope(self):
        result = self.invoke()
        self.assertEqual((result["mode"], result["scope"], result["status"]),
                         ("verify", "all", "passed"))
        self.assertTrue((self.output / "site/en/index.html").is_file())
        self.assertFalse((self.output / "site/api").exists())
        self.assertEqual(cli.serve_output(self.root, self.output), self.output / "site")

    def test_single_locale_verify_does_not_claim_full_bilingual_scope(self):
        result = self.invoke(language="zh_CN")
        self.assertEqual(result["scope"], "locale:zh_CN")
        self.assertEqual(result["languages"], ["zh_CN"])
        self.assertFalse((self.output / "site/en").exists())

    def test_locale_environments_and_doctrees_are_independent(self):
        self.invoke()
        en = self.environments["documentation-html-en"]
        zh = self.environments["documentation-html-zh_CN"]
        self.assertEqual(en["NEXUS_DOCS_LANGUAGE"], "en")
        self.assertEqual(zh["NEXUS_DOCS_LANGUAGE"], "zh_CN")
        paths = [args[args.index("-d") + 1] for stage, args in self.commands
                 if stage.startswith("documentation-html-")]
        self.assertEqual(len(set(paths)), 2)
        self.graph.assert_called_once()

    def test_host_translation_override_cannot_escape_the_observed_source_closure(self):
        hostile = install.ToolContext(self.context.identity_json,
            self.context.variables + (("NEXUS_DOCS_TRANSLATION_ROOT", "/other-catalogs"),))
        with patch.object(install, "context", return_value=hostile):
            self.invoke()
        for environment in self.environments.values():
            self.assertEqual(environment["NEXUS_DOCS_TRANSLATION_ROOT"],
                             str(self.root / "docs/locale"))

    def test_observed_dirty_state_overrides_the_host_flag(self):
        hostile = install.ToolContext(self.context.identity_json,
            self.context.variables + (("NEXUS_DOCS_SOURCE_DIRTY", "1"),))
        with patch.object(install, "context", return_value=hostile):
            self.invoke()
        for environment in self.environments.values():
            self.assertEqual(environment["NEXUS_DOCS_SOURCE_DIRTY"], "0")

    def test_preview_overlaps_tools_with_a_snapshot_before_source_checks(self):
        tools_started, snapshot_done, checked = (threading.Event() for _ in range(3))
        def prepare(_prefix):
            tools_started.set()
            self.assertTrue(checked.wait(timeout=3))
            return self.context
        def observe(_root):
            snapshot_done.set()
            return self.source
        def scan(_root, **_options):
            self.assertTrue(snapshot_done.is_set())
            self.assertTrue(tools_started.wait(timeout=3))
            checked.set()
            return {"errors": []}
        import os
        with patch.dict(os.environ, {"NEXUS_BUILD_JOBS": "2"}), \
                patch.object(install, "context", side_effect=prepare), \
                patch.object(build, "observe_git_source", side_effect=observe), \
                patch.object(build.check, "scan", side_effect=scan):
            result = self.invoke(mode="preview")
        self.assertEqual(result["status"], "ready")
        self.assertTrue(checked.is_set())

    def test_preview_tool_failure_waits_for_source_check_and_preserves_view(self):
        self.invoke(mode="preview")
        previous = (self.output / "site/en/index.html").read_bytes()
        checked = threading.Event()
        def prepare(_prefix):
            self.assertTrue(checked.wait(timeout=3))
            raise ValueError("tool tuple failed")
        def scan(_root, **_options):
            checked.set()
            return {"errors": []}
        import os
        with patch.dict(os.environ, {"NEXUS_BUILD_JOBS": "2"}), \
                patch.object(install, "context", side_effect=prepare), \
                patch.object(build.check, "scan", side_effect=scan):
            with self.assertRaisesRegex(ValueError, "tool tuple failed"):
                self.invoke(mode="preview")
        self.assertTrue(checked.is_set())
        self.assertEqual((self.output / "site/en/index.html").read_bytes(), previous)
        self.assertEqual(json.loads((self.output / build.REPORT).read_text())["status"],
                         "failed")

    def test_parallel_admission_preserves_control_after_a_tool_failure(self):
        for control in (KeyboardInterrupt(), SystemExit(23)):
            with self.subTest(control=type(control).__name__), \
                    patch.object(install, "context", side_effect=ValueError("tools failed")), \
                    patch.object(build, "observe_git_source", side_effect=control):
                with self.assertRaises(type(control)) as caught:
                    build._admit(self.root, self.output, self.prefix, parallel=True)
                self.assertIs(caught.exception, control)

    def test_verify_and_single_job_preview_keep_serial_admission_order(self):
        import os
        for mode in ("verify", "preview"):
            order = []
            def prepare(_prefix):
                order.append("tools")
                return self.context
            def observe(_root):
                order.append("snapshot")
                return self.source
            def scan(_root, **_options):
                order.append("source-check")
                return {"errors": []}
            with self.subTest(mode=mode), \
                    patch.dict(os.environ, {"NEXUS_BUILD_JOBS": "1"}), \
                    patch.object(install, "context", side_effect=prepare), \
                    patch.object(build, "observe_git_source", side_effect=observe), \
                    patch.object(build.check, "scan", side_effect=scan):
                self.invoke(mode=mode)
                self.assertEqual(order[:3], ["tools", "snapshot", "source-check"])

    def test_preview_warm_reuses_inputs_but_executes_html_again(self):
        self.invoke(mode="preview")
        self.commands.clear()
        result = self.invoke(mode="preview")
        self.assertEqual(result["status"], "ready")
        self.assertTrue(result["api_cache_hit"])
        self.assertTrue(result["gettext_cache_hit"])
        self.assertTrue(result["diagram_cache_hit"])
        self.assertEqual(len(self.commands), 2)
        self.assertTrue(all(stage.startswith("documentation-html-")
                            for stage, _args in self.commands))

    def test_success_report_contains_actual_stage_timings(self):
        result = self.invoke()
        timings = result["timings_seconds"]
        self.assertLessEqual({"tools_verification", "source_snapshot", "source_checks",
                              "admission_wall", "messages", "translations",
                              "diagrams", "html", "publication_inputs",
                              "prepublication_total"}, timings.keys())
        self.assertTrue(all(isinstance(value, float) and value >= 0
                            for value in timings.values()))

    def test_preview_assets_catalogs_and_unrendered_facts_keep_message_inputs(self):
        inputs = ("docs/assets/styles/site.css",
                  "docs/locale/zh_CN/LC_MESSAGES/index.po",
                  "soc/stm32/stm32f4/facts/unrendered.json")
        for name in inputs:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("first input\n")
        self.invoke(mode="preview")
        for name in inputs:
            with self.subTest(name=name):
                (self.root / name).write_text("changed input\n")
                self.commands.clear()
                result = self.invoke(mode="preview")
                self.assertTrue(result.get("api_cache_hit", False))
                self.assertTrue(result.get("gettext_cache_hit", False))
                self.assertEqual(self.environments["documentation-html-en"][
                    "NEXUS_DOCS_REBUILD_ALL"], "0")
                self.assertTrue(all(stage.startswith("documentation-html-")
                                    for stage, _ in self.commands))

    def test_image_only_edit_does_not_regenerate_messages(self):
        (self.root / "docs/index.md").write_text(
            PAGE + "\n![Flow](assets/diagrams/flow.svg)\n")
        svg = self.root / "docs/assets/diagrams/flow.svg"
        svg.write_text("<svg>first</svg>\n")
        self.invoke(mode="preview")
        svg.write_text("<svg>changed</svg>\n")
        self.commands.clear()
        result = self.invoke(mode="preview")
        self.assertTrue(result.get("api_cache_hit", False))
        self.assertTrue(result.get("gettext_cache_hit", False))
        self.assertFalse(result.get("diagram_cache_hit", False))
        self.assertEqual(self.environments["documentation-html-en"][
            "NEXUS_DOCS_REBUILD_ALL"], "0")

    def test_image_also_used_as_literal_input_still_invalidates_messages(self):
        (self.root / "docs/index.md").write_text(PAGE
            + "\n![Flow](assets/diagrams/flow.svg)\n\n"
            + "```{literalinclude} assets/diagrams/flow.svg\n:language: xml\n```\n")
        svg = self.root / "docs/assets/diagrams/flow.svg"
        svg.write_text("<svg>first</svg>\n")
        self.invoke(mode="preview")
        svg.write_text("<svg>changed</svg>\n")
        result = self.invoke(mode="preview")
        self.assertFalse(result.get("gettext_cache_hit", False))

    def test_orchestration_edit_keeps_gettext_and_html_parsing_state(self):
        wrapper = self.root / "tools/documentation/cli.py"
        wrapper.parent.mkdir(parents=True)
        wrapper.write_text("# invocation only\n")
        self.invoke(mode="preview")
        before = next(args[args.index("-d") + 1] for stage, args in self.commands
                      if stage == "documentation-html-en")
        wrapper.write_text("# changed invocation only\n")
        self.commands.clear()
        result = self.invoke(mode="preview")
        self.assertTrue(result.get("api_cache_hit", False))
        self.assertTrue(result.get("gettext_cache_hit", False))
        after = next(args[args.index("-d") + 1] for stage, args in self.commands
                     if stage == "documentation-html-en")
        self.assertEqual(after, before)

    def test_rendering_implementation_edit_invalidates_messages_not_xml(self):
        renderer = self.root / "tools/documentation/sphinx_ext.py"
        renderer.parent.mkdir(parents=True)
        renderer.write_text("# first renderer\n")
        self.invoke(mode="preview")
        renderer.write_text("# changed renderer\n")
        self.commands.clear()
        result = self.invoke(mode="preview")
        self.assertTrue(result.get("api_cache_hit", False))
        self.assertIn("documentation-gettext", [stage for stage, _ in self.commands])
        self.assertEqual(self.environments["documentation-html-en"][
            "NEXUS_DOCS_REBUILD_ALL"], "1")

    def test_actual_doxygen_overrides_participate_in_the_api_cache_key(self):
        self.invoke(mode="preview")
        options = ("GENERATE_HTML = NO", "GENERATE_XML = YES",
                   "OPTIMIZE_OUTPUT_FOR_C = NO")
        configs = []
        def inspect_config(command, **kwargs):
            if kwargs["stage"] == "documentation-doxygen":
                configs.append(Path(command[-1]).read_text())
            return self.command(command, **kwargs)
        self.port.side_effect = inspect_config
        with patch.object(build, "_DOXYGEN_OPTIONS", options, create=True):
            result = self.invoke(mode="preview")
        self.assertFalse(result.get("api_cache_hit", False))
        self.assertEqual(len(configs), 1)
        self.assertIn("OPTIMIZE_OUTPUT_FOR_C = NO", configs[0])

    def test_render_environment_changes_invalidate_cached_inputs(self):
        self.invoke(mode="preview")
        altered = install.ToolContext(self.context.identity_json,
            self.context.variables + (("SOURCE_DATE_EPOCH", "0"),))
        with patch.object(install, "context", return_value=altered):
            result = self.invoke(mode="preview")
        self.assertFalse(result.get("api_cache_hit", False))
        self.assertFalse(result.get("gettext_cache_hit", False))

    def test_job_budget_changes_do_not_invalidate_cached_inputs(self):
        self.invoke(mode="preview")
        altered = install.ToolContext(self.context.identity_json,
            (("NEXUS_BUILD_JOBS", "1"),))
        with patch.object(install, "context", return_value=altered):
            result = self.invoke(mode="preview")
        self.assertTrue(result.get("api_cache_hit", False))
        self.assertTrue(result.get("gettext_cache_hit", False))

    def test_unknown_rendering_implementation_still_requests_full_reparse(self):
        renderer = self.root / "tools/documentation/new_renderer.py"
        renderer.parent.mkdir(parents=True)
        renderer.write_text("# first unknown implementation\n")
        self.invoke(mode="preview")
        renderer.write_text("# changed unknown implementation\n")
        self.invoke(mode="preview")
        self.assertEqual(self.environments["documentation-html-en"][
            "NEXUS_DOCS_REBUILD_ALL"], "1")

    def test_new_commit_changes_source_links_without_discarding_html_parsing(self):
        self.invoke(mode="preview")
        before = next(args[args.index("-d") + 1] for stage, args in self.commands
                      if stage == "documentation-html-en")
        self.source = {**self.source, "commit": "3" * 40}
        with patch.object(build, "observe_git_source", return_value=self.source):
            self.commands.clear()
            result = self.invoke(mode="preview")
        self.assertTrue(result["api_cache_hit"])
        self.assertTrue(result["gettext_cache_hit"])
        after = next(args[args.index("-d") + 1] for stage, args in self.commands
                     if stage == "documentation-html-en")
        self.assertEqual(after, before)
        self.assertEqual(self.environments["documentation-html-en"][
            "NEXUS_DOCS_REVISION"], "3" * 40)

    def test_new_api_generation_preserves_unchanged_xml_mtimes(self):
        self.invoke(mode="preview")
        old = self.output / "cache/api/payload/xml/index.xml"
        before = old.stat().st_mtime_ns
        (self.root / "core/include/api.h").write_text("void changed_api(void);\n")
        observed = []
        def inspect_generation(command, **kwargs):
            if kwargs["stage"] == "documentation-gettext":
                xml = self.output / "work/api/xml/index.xml"
                observed.append(xml.stat().st_mtime_ns)
            return self.command(command, **kwargs)
        self.port.side_effect = inspect_generation
        self.commands.clear()
        result = self.invoke(mode="preview")
        self.assertFalse(result.get("api_cache_hit", False))
        self.assertIn("documentation-doxygen", [stage for stage, _ in self.commands])
        self.assertEqual(observed, [before])
        self.assertEqual(self.environments["documentation-html-en"][
            "NEXUS_DOCS_REBUILD_ALL"], "0")

    def test_changed_xml_file_set_gets_fresh_breathe_environments(self):
        header = self.root / "core/include/api.h"
        def different_compound(command, **kwargs):
            result = self.command(command, **kwargs)
            if kwargs["stage"] == "documentation-doxygen":
                name = "after.xml" if "changed" in header.read_text() else "before.xml"
                folder = Path(kwargs["env"]["NEXUS_DOCS_DOXYGEN_XML"])
                (folder / name).write_text("<compound/>\n")
            return result
        self.port.side_effect = different_compound
        self.invoke(mode="preview")
        before = {stage: args[args.index("-d") + 1] for stage, args in self.commands
                  if stage in {"documentation-gettext", "documentation-html-en"}}
        header.write_text("void changed_api(void);\n")
        self.commands.clear()
        self.invoke(mode="preview")
        after = {stage: args[args.index("-d") + 1] for stage, args in self.commands
                 if stage in {"documentation-gettext", "documentation-html-en"}}
        self.assertEqual(before.keys(), after.keys())
        for stage in before:
            self.assertNotEqual(before[stage], after[stage])

    def test_changed_index_gets_fresh_breathe_environments_with_the_same_xml_set(self):
        header = self.root / "core/include/api.h"
        def different_index(command, **kwargs):
            result = self.command(command, **kwargs)
            if kwargs["stage"] == "documentation-doxygen":
                index = Path(kwargs["env"]["NEXUS_DOCS_DOXYGEN_XML"]) / "index.xml"
                name = "changed_api" if "changed" in header.read_text() else "api"
                index.write_text('<doxygenindex><name>' + name + '</name></doxygenindex>\n')
            return result
        self.port.side_effect = different_index
        self.invoke(mode="preview")
        before = {stage: args[args.index("-d") + 1] for stage, args in self.commands
                  if stage in {"documentation-gettext", "documentation-html-en"}}
        header.write_text("void changed_api(void);\n")
        self.commands.clear()
        self.invoke(mode="preview")
        after = {stage: args[args.index("-d") + 1] for stage, args in self.commands
                 if stage in {"documentation-gettext", "documentation-html-en"}}
        self.assertEqual(before.keys(), after.keys())
        for stage in before:
            self.assertNotEqual(before[stage], after[stage])

    def test_same_mtime_changed_content_invalidates_preview(self):
        self.invoke(mode="preview")
        source = self.root / "docs/index.md"
        before = source.stat()
        source.write_text(PAGE.replace("Start", "Other"))
        import os
        os.utime(source, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.commands.clear()
        self.invoke(mode="preview")
        changed = json.loads(self.environments["documentation-html-en"][
            "NEXUS_DOCS_CHANGED_INPUTS"])
        self.assertIn("docs/index.md", changed)
        self.assertIn("documentation-gettext", [stage for stage, _ in self.commands])

    def test_changed_page_keeps_the_gettext_environment(self):
        self.invoke(mode="preview")
        command, = [args for stage, args in self.commands
                    if stage == "documentation-gettext"]
        environment = Path(command[command.index("-d") + 1])
        environment.mkdir(parents=True, exist_ok=True)
        sentinel = environment / "observed-environment"
        sentinel.write_text("preserved input state\n")
        (self.root / "docs/index.md").write_text(PAGE + "new message\n")
        self.invoke(mode="preview")
        self.assertTrue(sentinel.is_file())

    def test_removed_page_cannot_leave_an_unknown_gettext_output(self):
        removed = self.root / "docs/removed.md"
        removed.write_text(PAGE.replace("id: home", "id: removed"))
        self.invoke(mode="preview")
        command, = [args for stage, args in self.commands
                    if stage == "documentation-gettext"]
        stale = Path(command[-1]) / "removed.pot"
        stale.write_text('msgid "Removed"\nmsgstr ""\n')
        removed.unlink()
        self.invoke(mode="preview")
        self.assertFalse((self.output / "work/gettext/removed.pot").exists())
        self.assertFalse(stale.exists())

    def test_external_workspace_keeps_commands_and_failure_receipts_outside_source(self):
        self.port.side_effect = lambda *args, **kwargs: SimpleNamespace(
            returncode=7, receipt_path=kwargs["log"].with_suffix(".receipt.json"))
        with self.assertRaisesRegex(ValueError, "doxygen failed"):
            self.invoke()
        self.assertFalse((self.root / "build").exists())
        result = json.loads((self.output / build.REPORT).read_text())
        self.assertIn(str(self.output / "work/doxygen.receipt.json"), result["error"])

    def test_owned_workspace_cannot_clear_a_new_doxygen_header_input(self):
        self.output = self.root / "build/docs"
        self.invoke()
        header = self.output / "site/published-api.h"
        header.write_text("void retained_api(void);\n")
        (self.root / "Doxyfile").write_text('INPUT = "build/docs/site/published-api.h"\n')
        self.commands.clear()
        with self.assertRaisesRegex(ValueError, "input"):
            self.invoke()
        self.assertEqual(self.commands, [])
        self.assertEqual(header.read_text(), "void retained_api(void);\n")

    def test_corrupt_api_cache_cannot_borrow_a_valid_message_cache(self):
        self.invoke(mode="preview")
        (self.output / "cache/api/payload/xml/index.xml").unlink()
        self.commands.clear()
        self.invoke(mode="preview")
        self.assertIn("documentation-doxygen", [stage for stage, _ in self.commands])

    def test_corrupt_incremental_manifest_rebuilds_instead_of_reusing_an_environment(self):
        self.invoke(mode="preview")
        (self.output / "cache/inputs.json").write_text("{incomplete")
        result = self.invoke(mode="preview")
        self.assertEqual(result["status"], "ready")
        self.assertEqual(self.environments["documentation-html-en"]["NEXUS_DOCS_REBUILD_ALL"], "1")

    def test_verify_never_uses_a_preview_pass_or_input_cache(self):
        self.invoke(mode="preview")
        self.commands.clear()
        result = self.invoke(mode="verify")
        self.assertNotIn("api_cache_hit", result)
        self.assertIn("documentation-doxygen", [stage for stage, _ in self.commands])
        self.assertEqual(result["status"], "passed")

    def test_failed_preview_discards_mutable_environments_before_restoring_inputs(self):
        self.invoke(mode="preview")
        mutable = [self.output / "cache/gettext-work", self.output / "cache/html"]
        for folder in mutable:
            (folder / "environment-read-newer-input").write_text("newer state\n")
        page = self.root / "docs/index.md"
        before = page.stat()
        page.write_text(PAGE + "newer message\n")
        import os
        os.utime(page, ns=(before.st_atime_ns, before.st_mtime_ns))
        original = self.port.side_effect
        def fails_after_read(command, **kwargs):
            result = self.command(command, **kwargs)
            if kwargs["stage"] == "documentation-html-zh_CN":
                result.returncode = 7
            return result
        self.port.side_effect = fails_after_read
        with self.assertRaisesRegex(ValueError, "html-zh_CN failed"):
            self.invoke(mode="preview")
        for folder in mutable:
            self.assertFalse(folder.exists())
        page.write_text(PAGE)
        os.utime(page, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.port.side_effect = original
        self.invoke(mode="preview")

    def test_failed_command_preserves_published_view_and_records_failure(self):
        self.invoke()
        previous = (self.output / "site/en/index.html").read_bytes()
        self.port.side_effect = lambda *args, **kwargs: SimpleNamespace(
            returncode=7, stdout="", stderr="failed", receipt_path="actual-receipt")
        with self.assertRaisesRegex(ValueError, "doxygen failed with exit 7"):
            self.invoke()
        self.assertEqual((self.output / "site/en/index.html").read_bytes(), previous)
        result = json.loads((self.output / build.REPORT).read_text())
        self.assertEqual(result["status"], "failed")
        self.assertEqual(cli.serve_output(self.root, self.output), self.output / "site")

    def test_html_generation_keeps_previous_view_until_publication(self):
        self.invoke()
        previous = (self.output / "site/en/index.html").read_bytes()
        observed = []
        def while_generating(command, **kwargs):
            if kwargs["stage"].startswith("documentation-html-"):
                observed.append((self.output / "site/en/index.html").read_bytes())
            return self.command(command, **kwargs)
        self.port.side_effect = while_generating
        self.invoke()
        self.assertEqual(observed, [previous, previous])

    def test_serve_rejects_a_site_root_replaced_by_a_symlink(self):
        self.invoke()
        site, moved = self.output / "site", self.output / "original-site"
        site.rename(moved)
        site.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            cli.serve_output(self.root, self.output)

    def test_serve_rejects_a_nonobject_report(self):
        self.invoke()
        (self.output / build.REPORT).write_text("[]\n")
        with self.assertRaisesRegex(ValueError, "report"):
            cli.serve_output(self.root, self.output)

    def test_source_edit_during_build_rejects_publication(self):
        def changing(command, **kwargs):
            result = self.command(command, **kwargs)
            if kwargs["stage"] == "documentation-gettext":
                (self.root / "docs/index.md").write_text(PAGE + "changed\n")
            return result
        self.port.side_effect = changing
        with self.assertRaisesRegex(ValueError, "source identity changed"):
            self.invoke()
        self.assertEqual(json.loads((self.output / build.REPORT).read_text())["status"],
                         "failed")

    def test_competing_writer_is_rejected_without_replacing_view(self):
        self.invoke()
        before = (self.output / build.REPORT).read_bytes()
        failures = []
        def writer():
            try:
                self.invoke()
            except ValueError as error:
                failures.append(str(error))
        with build_lock(self.output):
            thread = threading.Thread(target=writer)
            thread.start()
            thread.join(timeout=5)
        self.assertFalse(thread.is_alive())
        self.assertEqual(len(failures), 1)
        self.assertIn("already owned", failures[0])
        self.assertEqual((self.output / build.REPORT).read_bytes(), before)

    def test_preview_rejects_linked_incremental_outputs_before_any_command(self):
        self.invoke(mode="preview")
        target = self.root / "README.md"
        target.write_text("source bytes must survive\n")
        command, = [args for stage, args in self.commands
                    if stage == "documentation-html-en"]
        linked = Path(command[-1]) / "index.html"
        for kind in ("hardlink", "symlink"):
            with self.subTest(kind=kind):
                linked.unlink(missing_ok=True)
                if kind == "hardlink":
                    import os
                    os.link(target, linked)
                else:
                    linked.symlink_to(target)
                self.commands.clear()
                try:
                    with self.assertRaisesRegex(ValueError, "link"):
                        self.invoke(mode="preview")
                    self.assertEqual(self.commands, [])
                    self.assertEqual(target.read_text(), "source bytes must survive\n")
                finally:
                    linked.unlink(missing_ok=True)

    def test_first_workspace_creator_cannot_be_adopted_before_owner_is_written(self):
        entered, release = threading.Event(), threading.Event()
        original = Path.open
        errors = []
        def opening(path, *args, **kwargs):
            if path == self.output / build.OWNER and args == ("x",):
                entered.set()
                self.assertTrue(release.wait(timeout=5))
            return original(path, *args, **kwargs)
        def first():
            try:
                build.prepare_output(self.root, self.output)
            except Exception as error:
                errors.append(error)
        with patch.object(Path, "open", opening):
            thread = threading.Thread(target=first)
            thread.start()
            self.assertTrue(entered.wait(timeout=5))
            try:
                with self.assertRaisesRegex(ValueError, "ownership"):
                    build.prepare_output(self.root, self.output)
            finally:
                release.set()
                thread.join(timeout=5)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors)
        self.assertEqual(json.loads((self.output / build.OWNER).read_text())["schema_version"],
                         build.SCHEMA)


if __name__ == "__main__":
    unittest.main()
