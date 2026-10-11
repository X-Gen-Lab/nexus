"""Fail-closed OS resource and retained-instruction evidence contracts."""
import importlib
from pathlib import Path
import tempfile
import unittest


class OSResourceGateTests(unittest.TestCase):
    def gate(self):
        return importlib.import_module("os_resources")

    def sizes(self):
        return {"tcb": 84, "semaphore": 72, "stack_word": 4,
                "wait_port": 16, "notify": 84, "direct_notify": 16,
                "raw_queue": 80, "closable_queue": 96,
                "queue_waiter": 84, "joinable_task": 172,
                "permanent_task": 88}

    def test_matrix_reuses_authored_profiles_for_two_abis_and_optimizations(self):
        variants = self.gate().matrix()
        self.assertEqual(len(variants), 8)
        self.assertEqual(len({item["name"] for item in variants}), 8)
        self.assertEqual({item["optimization"] for item in variants},
                         {"Os", "O2"})
        self.assertEqual({item["facts"]["cpu"]["arch"] for item in variants},
                         {"cortex-m0", "cortex-m4"})
        self.assertEqual({item["kernel"]["profile"] for item in variants},
                         {"minimal", "standard"})

    def test_optimization_has_one_toml_authority(self):
        gate = self.gate()
        variant = next(item for item in gate.matrix()
                       if item["optimization"] == "O2")
        with tempfile.TemporaryDirectory() as temporary:
            path = gate.write_inputs(Path(temporary), variant)
            text = path.read_text()
            self.assertEqual(text.count("optimization ="), 1)
            self.assertIn('optimization = "O2"', text)
            self.assertIn("[os]", text)
            self.assertNotIn("configMAX_PRIORITIES", text)

    def test_every_actual_object_has_exactly_the_authored_optimization(self):
        gate = self.gate()
        gate.check_optimization([{"argv": ["gcc", "-Os", "-c", "source.c"]}], "Os")
        for flags in (("-O2",), ("-Os", "-O2"), ("-Os", "-Os"), (),
                      ("-Os", "-flto")):
            with self.subTest(flags=flags), self.assertRaises(ValueError):
                gate.check_optimization([{"argv": ["gcc", *flags]}], "Os")

    def test_actual_nm_object_sizes_reject_missing_or_duplicate_storage(self):
        text = ("20000000 00000054 B nx_os_notify__bytes\n"
                "20000054 00000010 B nx_os_direct_notify__bytes\n")
        self.assertEqual(self.gate().storage_symbols(text),
                         {"notify": 84, "direct_notify": 16})
        for malformed in (text + text.splitlines()[0] + "\n",
                          text.replace("00000054 B", "00000000 B")):
            with self.assertRaises(ValueError):
                self.gate().storage_symbols(malformed)

    def test_resource_formula_matches_real_storage_and_kernel_idle(self):
        sizes = self.sizes()
        measured = {key: value for key, value in sizes.items()
                    if key not in {"stack_word", "semaphore"}}
        measured.update({"join_stack": 512, "permanent_stack": 512,
                         "raw_payload": 12, "closable_payload": 12})
        policy = {"idle_stack_words": 128}
        idle = {"tcb": 84, "stack": 512}
        result = self.gate().resource_accounting(sizes, measured, idle, policy)
        self.assertEqual(result["idle_total"], 596)
        self.assertEqual(result["direct_notify_saved"], 68)
        self.assertEqual(result["permanent_task_saved"], 84)
        self.assertEqual(result["closable_queue_increment"], 16)
        measured["queue_waiter"] = 80
        with self.assertRaisesRegex(ValueError, "storage"):
            self.gate().resource_accounting(sizes, measured, idle, policy)

    def test_disabled_direct_slot_does_not_claim_usable_resource_savings(self):
        sizes = self.sizes()
        measured = {key: value for key, value in sizes.items()
                    if key not in {"stack_word", "semaphore"}}
        measured.update({"join_stack": 512, "permanent_stack": 512,
                         "raw_payload": 12, "closable_payload": 12})
        result = self.gate().resource_accounting(
            sizes, measured, {"tcb": 84, "stack": 512},
            {"idle_stack_words": 128, "task_notifications": False})
        self.assertFalse(result["direct_notify_supported"])
        self.assertIsNone(result["direct_notify_saved"])

    def test_idle_requires_actual_tcb_and_exact_configured_stack_size(self):
        text = ("20000000 00000054 b xIdleTaskTCB.3\n"
                "20000054 00000200 b uxIdleTaskStack.2\n")
        self.assertEqual(self.gate().idle_symbols(text),
                         {"tcb": 84, "stack": 512})
        with self.assertRaises(ValueError):
            self.gate().idle_symbols(text.replace("xIdleTaskTCB", "other"))
        with self.assertRaises(ValueError):
            self.gate().idle_symbols(text + text)

    def test_hot_path_counts_instructions_and_resolves_external_calls(self):
        body = (" 8000000: b510 push {r4, lr}\n"
                " 8000002: f000 f801 bl 8000008 <nx_arch_irq_save>\n"
                " 8000006: 4801 ldr r0, [pc, #4]\n"
                " 8000008: f000 f801 bl 800000e <nx_arch_irq_restore>\n"
                " 800000c: f000 f801 bl 8000012 <xQueueGenericSend>\n"
                " 8000010: bd10 pop {r4, pc}\n"
                " 8000014: 20000000 .word 0x20000000\n")
        result = self.gate().instruction_summary(body)
        self.assertEqual(result["instructions"], 6)
        self.assertEqual(result["calls"], ["nx_arch_irq_save",
                                         "nx_arch_irq_restore",
                                         "xQueueGenericSend"])
        self.gate().check_baseline_restore(result)
        invalid = {"calls": ["nx_arch_irq_save", "xQueueGenericSend",
                             "nx_arch_irq_restore"]}
        with self.assertRaisesRegex(ValueError, "restore"):
            self.gate().check_baseline_restore(invalid)

    def test_branch_alternatives_each_restore_one_saved_mask(self):
        body = (
            " 8000000: f000 f801 bl 8000100 <nx_arch_irq_save>\n"
            " 8000004: 2800 cmp r0, #0\n"
            " 8000006: d003 beq.n 800000e <notify_wait+0xe>\n"
            " 8000008: f000 f801 bl 8000110 <nx_arch_irq_restore>\n"
            " 800000c: e003 b.n 8000016 <notify_wait+0x16>\n"
            " 800000e: f000 f801 bl 8000110 <nx_arch_irq_restore>\n"
            " 8000012: f000 f801 bl 8000120 <xQueueSemaphoreTake>\n"
            " 8000016: bd10 pop {r4, pc}\n")
        self.gate().check_baseline_restore(
            self.gate().instruction_summary(body))
        invalid = body.replace(
            "800000e: f000 f801 bl 8000110 <nx_arch_irq_restore>",
            "800000e: bf00 nop")
        with self.assertRaisesRegex(ValueError, "restore"):
            self.gate().check_baseline_restore(
                self.gate().instruction_summary(invalid))

    def test_heap_and_atomic_helpers_cannot_enter_the_os_image(self):
        self.gate().check_static_image("08000000 T xQueueGenericSend\n")
        for name in ("malloc", "pvPortMalloc", "__atomic_fetch_add_4"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.gate().check_static_image("08000000 T " + name + "\n")

    def test_minimal_resource_comparison_requires_matching_cpu_and_opt(self):
        variants = []
        for optimization in ("Os", "O2"):
            for profile, tcb, ram in (("standard", 84, 2000),
                                      ("minimal", 68, 1700)):
                variants.append({"cpu": "cortex-m4", "optimization": optimization,
                                 "profile": profile,
                                 "resources": {"sizes": {"tcb": tcb},
                                               "image": {"static_ram": ram}}})
        comparisons = self.gate().profile_comparisons(variants)
        self.assertEqual(len(comparisons), 2)
        self.assertEqual(comparisons[0]["per_tcb_saved"], 16)
        variants[-1]["optimization"] = "O3"
        with self.assertRaises(ValueError):
            self.gate().profile_comparisons(variants)


if __name__ == "__main__":
    unittest.main()
