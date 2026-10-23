"""Audit directly specialized provider tables from their actual source files."""
from tools.configure.providers.descriptor import source_directory

from pathlib import Path
import tempfile
import unittest

from tools.evidence.common import EvidenceError, file_identity
from tools.measurement.retention import source_contracts


ROOT = Path(__file__).resolve().parents[2]
PUBLIC = """typedef struct { int (*start)(void*); int (*stop)(void*); } nx_probe_ops_t;
typedef struct { const nx_probe_ops_t* ops; void* context; } nx_probe_t;
"""
WRAPPER = '#define NX_PROBE_OPS nx_reviewed_probe_ops\n#include "probe_impl.h"\n'
TEMPLATE = 'const nx_probe_ops_t NX_PROBE_OPS = { .start = real_start, .stop = real_stop };\n'


class ProviderTemplateTests(unittest.TestCase):
    def tree(self, directory, wrapper=WRAPPER, template=TEMPLATE):
        root = Path(directory)
        public = root / "io/include/nexus/io/probe.h"
        source = root / "soc/reviewed/drivers/probe.c"
        header = root / "io/private/probe_impl.h"
        for path, contents in ((public, PUBLIC), (source, wrapper), (header, template)):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        return root, source, header

    def test_actual_timer_tables_are_projected_from_both_families(self):
        _, _, providers, _ = source_contracts(ROOT)
        for family, stem in (("stm32f407", "stm32"), ("gd32f470", "gd32")):
            for name, template in (("timer_capture", "timer_capture_impl.h"),
                                   ("advanced_pwm", "advanced_pwm_impl.h")):
                with self.subTest(family=family, name=name):
                    provider = providers[f"nx_{stem}_{name}_ops"]
                    self.assertEqual(provider["source"], file_identity(
                        ROOT / source_directory(family) / "drivers" / (name + ".c")))
                    self.assertEqual(provider["template_source"], file_identity(
                        ROOT / "io/private" / template))

    def test_direct_projection_keeps_actual_translation_unit_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, header = self.tree(directory)
            _, _, providers, _ = source_contracts(root)
            actual = providers["nx_reviewed_probe_ops"]
            self.assertEqual(actual["source"], file_identity(source))
            self.assertEqual(actual["template_source"], file_identity(header))
            self.assertEqual(actual["fields"], {"start": "real_start", "stop": "real_stop"})

    def test_conditional_wrapper_binding_is_not_guessed(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, '#if CHOICE\n' + WRAPPER + '#endif\n')
            with self.assertRaisesRegex(EvidenceError, "conditional"):
                source_contracts(root)

    def test_duplicate_binding_is_rejected_even_if_identical(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.splitlines()[0] + '\n' + WRAPPER)
            with self.assertRaisesRegex(EvidenceError, "duplicate"):
                source_contracts(root)

    def test_opaque_macro_expression_is_not_substituted(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.replace(
                "nx_reviewed_probe_ops", "SELECT(nx_reviewed_probe_ops)"))
            with self.assertRaisesRegex(EvidenceError, "direct"):
                source_contracts(root)

    def test_conditional_template_table_is_not_guessed(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template='#if CHOICE\n' + TEMPLATE + '#endif\n')
            with self.assertRaisesRegex(EvidenceError, "conditional"):
                source_contracts(root)

    def test_duplicate_template_include_is_not_assumed_guarded(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER + '#include "probe_impl.h"\n')
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                source_contracts(root)

    def test_template_table_must_be_the_actual_bound_macro(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template=TEMPLATE.replace("NX_PROBE_OPS", "NX_OTHER_OPS"))
            with self.assertRaisesRegex(EvidenceError, "missing"):
                source_contracts(root)

    def test_opaque_initializer_is_rejected_by_existing_method_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template=TEMPLATE.replace("real_start", "(nx_fn_t)real_start"))
            with self.assertRaisesRegex(EvidenceError, "initializer"):
                source_contracts(root)

    def test_template_cannot_escape_the_owned_private_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, WRAPPER.replace('"probe_impl.h"', '"../probe_impl.h"'))
            with self.assertRaisesRegex(EvidenceError, "private"):
                source_contracts(root)

    def test_table_macro_redefinition_inside_template_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, template='#undef NX_PROBE_OPS\n' + TEMPLATE)
            with self.assertRaisesRegex(EvidenceError, "redefined"):
                source_contracts(root)

    def test_quoted_template_name_cannot_shadow_another_owned_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, _ = self.tree(directory)
            (source.parent / "probe_impl.h").write_text(TEMPLATE)
            with self.assertRaisesRegex(EvidenceError, "ambiguous"):
                source_contracts(root)

    def test_binding_after_include_is_not_an_actual_template_binding(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory, "\n".join(
                reversed(WRAPPER.splitlines())) + "\n")
            with self.assertRaisesRegex(EvidenceError, "before"):
                source_contracts(root)


if __name__ == "__main__":
    unittest.main()
