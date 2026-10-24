"""Readonly capability evidence follows actual C template specializations."""
from tools.configure.providers.descriptor import source_directory
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure
from tools.maintenance import capabilities


class TimerCapabilityObserverTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "io/private").mkdir(parents=True)
        self.source = self.root / "wrapper.c"

    def test_real_four_timer_tables_are_derived_from_the_selected_source(self):
        for family, prefix in (("stm32f407", "stm32"), ("gd32f470", "gd32")):
            for kind in ("timer_capture", "advanced_pwm"):
                with self.subTest(family=family, kind=kind):
                    source = configure.ROOT / f"{source_directory(family)}/drivers/{kind}.c"
                    self.assertEqual(capabilities.source_ops(source, configure.ROOT),
                                     {f"nx_{prefix}_{kind}_ops"})

    def test_macro_mentions_without_actual_table_definition_are_rejected(self):
        self.source.write_text('#define NX_TABLE_OPS nx_unused_ops\n#include "template.h"\n')
        (self.root / "io/private/template.h").write_text(
            'extern const nx_gpio_ops_t NX_TABLE_OPS;\nconst nx_gpio_ops_t* mention = &NX_TABLE_OPS;\n')
        with self.assertRaises(ValueError):
            capabilities.source_ops(self.source, self.root)

    def test_literal_specialization_binds_actual_const_table_only(self):
        self.source.write_text('#define NX_TABLE_OPS nx_gpio_ops\n#include "template.h"\n')
        (self.root / "io/private/template.h").write_text(
            'const nx_gpio_ops_t NX_TABLE_OPS = {.read = implementation};\n')
        self.assertEqual(capabilities.source_ops(self.source, self.root), {"nx_gpio_ops"})

    def test_conflicting_macro_identity_and_escaped_template_fail_closed(self):
        for source in ('#define NX_TABLE_OPS nx_a_ops\n#define NX_TABLE_OPS nx_b_ops\n#include "template.h"\n',
                       '#define NX_TABLE_OPS nx_a_ops\n#include "../template.h"\n'):
            with self.subTest(source=source):
                self.source.write_text(source)
                (self.root / "io/private/template.h").write_text(
                    'const nx_gpio_ops_t NX_TABLE_OPS = {0};\n')
                with self.assertRaises(ValueError):
                    capabilities.source_ops(self.source, self.root)

    def test_conditional_duplicate_and_ambiguous_projection_are_not_guessed(self):
        template = self.root / "io/private/template.h"
        template.write_text('const nx_gpio_ops_t NX_TABLE_OPS = {0};\n')
        cases = (
            '#if FEATURE\n#define NX_TABLE_OPS nx_a_ops\n#endif\n#include "template.h"\n',
            '#define NX_TABLE_OPS nx_a_ops\n#define NX_TABLE_OPS nx_a_ops\n#include "template.h"\n',
            '#define NX_TABLE_OPS nx_a_ops\n#include "template.h"\n#include "second.h"\n')
        (self.root / "io/private/second.h").write_text(template.read_text())
        for source in cases:
            with self.subTest(source=source):
                self.source.write_text(source)
                with self.assertRaises(ValueError):
                    capabilities.source_ops(self.source, self.root)

    def test_commented_specialization_does_not_claim_a_definition(self):
        self.source.write_text('/*\n#define NX_TABLE_OPS nx_a_ops\n#include "template.h"\n*/\n')
        (self.root / "io/private/template.h").write_text(
            'const nx_gpio_ops_t NX_TABLE_OPS = {0};\n')
        self.assertEqual(capabilities.source_ops(self.source, self.root), set())

    def test_actual_selected_timer_faces_use_real_public_types(self):
        rows = capabilities.collect(configure.ROOT)
        selected = [row for row in rows if row["kind"] in {"timer_capture", "advanced_pwm"}]
        expected = {
            ("stm32f407", "TIM4", "timer_capture", "edge-capture"),
            ("stm32f407", "TIM1", "advanced_pwm", "complementary-pwm"),
            ("gd32f470", "TIMER3", "timer_capture", "edge-capture"),
            ("gd32f470", "TIMER0", "advanced_pwm", "complementary-pwm"),
            ("stm32g474", "TIM5", "timer_capture", "edge-capture"),
        }
        self.assertEqual({(row["family"], row["controller"], row["kind"],
                           row["mode"]) for row in selected}, expected)
        for row in selected:
            self.assertTrue(row["fixtures"])
            self.assertEqual(row["exports"], [f"{source_directory(row['family'])}/drivers/{row['kind']}.c"])
            self.assertFalse(row["boards"])


if __name__ == "__main__":
    unittest.main()
