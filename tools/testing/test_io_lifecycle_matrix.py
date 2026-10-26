"""Evidence-boundary unit tests; synthetic fixtures are not provider proof."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from scripts.ci import io_lifecycle_matrix as matrix

class IOLifecycleMatrixTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.execution = self.build / "tdd"
        self.execution.mkdir(parents=True)
        self.source = self.root / "tests/google/provider_test.cpp"
        self.source.parent.mkdir(parents=True)
        self.source.write_text("TEST_F(Provider, Retains) { run(); EXPECT_EQ(state, ACTIVE); EXPECT_EQ(count, 0U); }\n")
        self.production = self.root / "soc/example/provider.c"
        self.production.parent.mkdir(parents=True)
        self.production.write_text("void run(void) {}\n")
        self.binary = self.build / "bin/nexus_provider_test"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b"unit binary identity fixture")
        entries = []
        for source in (self.source, self.production):
            obj = self.build / "CMakeFiles/nexus_provider_test.dir" / (source.name + ".o")
            obj.parent.mkdir(parents=True, exist_ok=True)
            obj.write_bytes(b"unit compiled context fixture")
            entries.append({"directory": str(self.build), "file": str(source),
                            "arguments": ["cc", "-c", str(source), "-o", str(obj)]})
        (self.build / "compile_commands.json").write_text(json.dumps(entries))
        (self.build / "google-contracts.json").write_text(json.dumps({"schema": 1, "executables": [str(self.binary)]}))
        self.catalogue = self.root / "catalogue.json"
        self.catalogue.write_text(json.dumps({"schema": 1, "scope": "bounded unit fixture", "properties": [{
            "id": "borrow", "family": "example", "peripheral": "uart", "mode": "dma-tx",
            "target": self.binary.name, "case": "Provider.Retains", "test_source": str(self.source.relative_to(self.root)),
            "production_sources": [str(self.production.relative_to(self.root))], "model_sources": [],
            "assertions": ["EXPECT_EQ(state, ACTIVE)", "EXPECT_EQ(count, 0U)"], "calls": ["run()"],
            "invariant": "unproved drain retains loan", "limitations": "unit fixture only"}],
            "unproven": [{"scope": "physical", "reason": "equipment not connected"}]}))
        self.snapshot = self.root / "snapshot.json"
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()

    def write_execution(self):
        self.xml = self.execution / "google-0.xml"
        self.xml.write_text('<testsuites name="AllTests" tests="1" failures="0" errors="0" skipped="0"><testsuite name="Provider" tests="1"><testcase classname="Provider" name="Retains" status="run" result="completed"/></testsuite></testsuites>')
        (self.execution / "discovery-0.txt").write_text("Provider.\n  Retains\n")
        import hashlib
        sha = hashlib.sha256(self.xml.read_bytes()).hexdigest()
        (self.execution / "execution.json").write_text(json.dumps({"schema": 1, "kind": "development-contract-execution", "suites": [{"binary": self.binary.name, "passed": 1, "report_sha256": sha, "cases": [{"classname": "Provider", "name": "Retains", "status": "passed"}]}]}))
        for name, command in (("discovery", [str(self.binary), "--gtest_filter=*", "--gtest_list_tests"]),
                              ("execution", [str(self.binary), "--gtest_filter=*", "--gtest_repeat=1", "--gtest_also_run_disabled_tests", "--gtest_output=xml:" + str(self.xml)])):
            (self.execution / (name + "-0.command.json")).write_text(json.dumps({"command": command, "cwd": str(self.root), "exit_code": 0}))

    def run_audit(self):
        return matrix.audit(self.root, self.build, self.catalogue, self.snapshot, self.execution)

    def test_actual_complete_inputs_bind_reviewed_assertions(self):
        result = self.run_audit()
        self.assertEqual(result["properties"][0]["status"], "executed-bounded")
        self.assertEqual(result["hardware_status"], "not_executed")
        self.assertTrue(result["properties"][0]["compiled_inputs"])

    def test_changed_source_cannot_certify_prior_execution(self):
        self.production.write_text("void replaced(void) {}\n")
        with self.assertRaises(ValueError): self.run_audit()

    def test_changed_binary_rejected(self):
        self.binary.write_bytes(b"different binary")
        with self.assertRaises(ValueError): self.run_audit()

    def test_parent_components_cannot_borrow_an_external_binary(self):
        foreign = self.root / "foreign" / self.binary.name
        foreign.parent.mkdir()
        foreign.write_bytes(self.binary.read_bytes())
        escaped = self.build / ".." / "foreign" / self.binary.name
        self.assertTrue(escaped.is_file())
        self.assertFalse(escaped.is_symlink())
        self.assertFalse(escaped.resolve().is_relative_to(self.build))
        plan_path = self.build / "google-contracts.json"
        plan = json.loads(plan_path.read_text())
        plan["executables"] = [str(escaped)]
        plan_path.write_text(json.dumps(plan))
        with self.assertRaises(ValueError):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_different_case_with_same_count_rejected(self):
        (self.execution / "discovery-0.txt").write_text("Provider.\n  Other\n")
        with self.assertRaises(ValueError): self.run_audit()

    def test_failed_subprocess_rejected_even_with_success_xml(self):
        p = self.execution / "execution-0.command.json"
        j = json.loads(p.read_text()); j["exit_code"] = 1; p.write_text(json.dumps(j))
        with self.assertRaises(ValueError): self.run_audit()

    def test_filtered_execution_rejected(self):
        p = self.execution / "execution-0.command.json"
        j = json.loads(p.read_text()); j["command"][1] = "--gtest_filter=Provider.Retains"; p.write_text(json.dumps(j))
        with self.assertRaises(ValueError): self.run_audit()

    def test_skipped_case_rejected(self):
        self.xml.write_text(self.xml.read_text().replace('result="completed"', 'result="skipped"'))
        with self.assertRaises(ValueError): self.run_audit()

    def test_missing_compiled_production_context_rejected(self):
        p = self.build / "compile_commands.json"; j = json.loads(p.read_text()); p.write_text(json.dumps(j[:1]))
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_assertion_only_in_other_case_rejected(self):
        self.source.write_text("TEST_F(Provider, Retains) { run(); } TEST_F(Provider, Other) { EXPECT_EQ(state, ACTIVE); EXPECT_EQ(count, 0U); }\n")
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_commented_assertion_does_not_establish_review_anchor(self):
        self.source.write_text("TEST_F(Provider, Retains) { run(); /* EXPECT_EQ(state, ACTIVE); EXPECT_EQ(count, 0U); */ }\n")
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_assertions_inside_string_literal_are_not_review_predicates(self):
        self.source.write_text('TEST_F(Provider, Retains) { run(); const char* fake = "EXPECT_EQ(state, ACTIVE); EXPECT_EQ(count, 0U);"; }\n')
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_test_body_compiled_only_for_other_target_cannot_prove_this_binary(self):
        p = self.build / "compile_commands.json"
        entries = json.loads(p.read_text())
        old = Path(entries[0]["arguments"][-1])
        new = self.build / "CMakeFiles/another_target.dir" / old.name
        new.parent.mkdir(parents=True); new.write_bytes(old.read_bytes())
        entries[0]["arguments"][-1] = str(new)
        p.write_text(json.dumps(entries))
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def shared_native_model(self, owner="nexus_io_native"):
        source = self.root / "soc/native/drivers/uart.c"
        source.parent.mkdir(parents=True)
        self.production.rename(source)
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entry = entries[1]
        old_object = Path(entry["arguments"][-1])
        output = (self.build / "soc/native/CMakeFiles" / (owner + ".dir") /
                  "drivers/uart.c.o")
        output.parent.mkdir(parents=True)
        output.write_bytes(old_object.read_bytes())
        entry["file"] = str(source)
        entry["arguments"][2] = str(source)
        entry["arguments"][-1] = str(output)
        database.write_text(json.dumps(entries))
        catalogue = json.loads(self.catalogue.read_text())
        catalogue["properties"][0]["production_sources"] = [
            source.relative_to(self.root).as_posix()]
        self.catalogue.write_text(json.dumps(catalogue))
        return source, output

    def test_shared_native_model_has_one_owned_compiled_context(self):
        source, output = self.shared_native_model()
        snapshot = matrix.capture(self.root, self.build, self.catalogue,
                                  self.snapshot)
        context = snapshot["compiled"][source.relative_to(self.root).as_posix()]
        self.assertEqual(len(context), 1)
        self.assertEqual(context[0]["object"], str(output))
        self.assertEqual(context[0]["object_sha256"], matrix.digest(output))
        self.write_execution()
        self.assertEqual(self.run_audit()["properties"][0]["status"],
                         "executed-bounded")

    def test_shared_native_model_compiled_for_another_owner_rejects(self):
        self.shared_native_model(owner="another_target")
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_native_owner_marker_cannot_hide_another_compiled_target(self):
        _, output = self.shared_native_model()
        another = (self.build / "soc/native/CMakeFiles/another_target.dir" /
                   "drivers/uart.c.o")
        another.parent.mkdir(parents=True)
        another.write_bytes(output.read_bytes())
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entries[1]["arguments"][-1] = str(
            output.parent.parent / "../another_target.dir/drivers/uart.c.o")
        database.write_text(json.dumps(entries))
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_hardware_provider_compiled_for_shared_native_owner_rejects(self):
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        old_object = Path(entries[1]["arguments"][-1])
        output = (self.build / "soc/native/CMakeFiles/nexus_io_native.dir" /
                  old_object.name)
        output.parent.mkdir(parents=True)
        output.write_bytes(old_object.read_bytes())
        entries[1]["arguments"][-1] = str(output)
        database.write_text(json.dumps(entries))
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def hardware_source_alias(self, name):
        source = self.root / "soc/stm32/stm32f407/drivers/uart.c"
        source.parent.mkdir(parents=True)
        self.production.rename(source)
        alias = self.root / name
        alias.parent.mkdir(parents=True, exist_ok=True)
        self.assertEqual(alias.resolve(), source)
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entry = entries[1]
        old_object = Path(entry["arguments"][-1])
        output = (self.build / "soc/native/CMakeFiles/nexus_io_native.dir" /
                  "drivers/uart.c.o")
        output.parent.mkdir(parents=True)
        output.write_bytes(old_object.read_bytes())
        entry["file"] = str(alias)
        entry["arguments"][2] = str(alias)
        entry["arguments"][-1] = str(output)
        database.write_text(json.dumps(entries))
        catalogue = json.loads(self.catalogue.read_text())
        catalogue["properties"][0]["production_sources"] = [name]
        self.catalogue.write_text(json.dumps(catalogue))

    def test_hardware_source_alias_cannot_borrow_the_native_owner(self):
        self.hardware_source_alias(
            "soc/native/drivers/../../stm32/stm32f407/drivers/uart.c")
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_hardware_source_alias_cannot_skip_soc_ownership(self):
        self.hardware_source_alias("io/../soc/stm32/stm32f407/drivers/uart.c")
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def shared_ip_driver(self, owner, *, alias=False):
        source = self.root / "drivers/i2c/i2c_v1_async/i2c_v1_async.c"
        source.parent.mkdir(parents=True)
        self.production.rename(source)
        name = source.relative_to(self.root).as_posix()
        if alias:
            (self.root / "io").mkdir()
            name = "io/../" + name
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        old_object = Path(entries[1]["arguments"][-1])
        output = self.build / "CMakeFiles" / (owner + ".dir") / "ip.o"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(old_object.read_bytes())
        entries[1]["file"] = str(self.root / name)
        entries[1]["arguments"][2] = str(self.root / name)
        entries[1]["arguments"][-1] = str(output)
        database.write_text(json.dumps(entries))
        catalogue = json.loads(self.catalogue.read_text())
        catalogue["properties"][0]["production_sources"] = [name]
        self.catalogue.write_text(json.dumps(catalogue))

    def test_shared_ip_driver_retains_target_local_execution_evidence(self):
        self.shared_ip_driver(self.binary.name)
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()
        self.assertEqual(self.run_audit()["properties"][0]["status"],
                         "executed-bounded")

    def test_shared_ip_driver_cannot_borrow_another_target_object(self):
        self.shared_ip_driver("another_target")
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_shared_ip_driver_alias_cannot_borrow_the_native_owner(self):
        self.shared_ip_driver("nexus_io_native", alias=True)
        with self.assertRaisesRegex(ValueError, "compiled only for another target"):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_duplicate_property_rejected(self):
        j = json.loads(self.catalogue.read_text()); j["properties"].append(copy.deepcopy(j["properties"][0])); self.catalogue.write_text(json.dumps(j))
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_old_xml_timestamp_rejected(self):
        import os
        os.utime(self.xml, ns=(1, 1))
        with self.assertRaises(ValueError): self.run_audit()

    def test_symlink_source_cannot_expand_owned_scope(self):
        self.production.unlink(); self.production.symlink_to(self.source)
        with self.assertRaises(ValueError): matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_missing_execution_summary_rejected(self):
        (self.execution / "execution.json").unlink()
        with self.assertRaises((ValueError, OSError)): self.run_audit()

    def test_complete_gate_rejects_an_omitted_non_catalogue_target(self):
        path = self.build / "google-contracts.json"
        plan = json.loads(path.read_text())
        plan["executables"].append(str(self.build / "bin/nexus_other_test"))
        path.write_text(json.dumps(plan))
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()
        with self.assertRaises(ValueError): self.run_audit()

    def test_explicit_development_subset_is_never_complete_gate_evidence(self):
        path = self.build / "google-contracts.json"
        plan = json.loads(path.read_text())
        plan["executables"].append(str(self.build / "bin/nexus_other_test"))
        path.write_text(json.dumps(plan))
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()
        result = matrix.audit(self.root, self.build, self.catalogue, self.snapshot,
                              self.execution, allow_subset=True)
        self.assertEqual(result["execution_scope"], "development-subset")

    def generated_context(self):
        source = self.build / "generated/bindings.c"
        source.parent.mkdir(parents=True)
        source.write_text("void assembled(void) {}\n")
        output = (self.build / "CMakeFiles/nexus_provider_test.dir" /
                  "generated-bindings.c.o")
        output.write_bytes(b"actual generated context fixture")
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        entries.append({"directory": str(self.build), "file": str(source),
                        "arguments": ["cc", "-c", str(source),
                                      "-o", str(output)]})
        database.write_text(json.dumps(entries))
        return source, output

    def test_actual_generated_target_context_binds_source_and_object(self):
        source, output = self.generated_context()
        record = matrix.capture(self.root, self.build, self.catalogue,
                                self.snapshot)
        contexts = record["target_compiled"][self.binary.name]
        generated = next(item for item in contexts
                         if item["source"] == str(source))
        self.assertEqual(generated["source_sha256"], matrix.digest(source))
        self.assertEqual(generated["object_sha256"], matrix.digest(output))

    def test_generated_source_change_cannot_certify_prior_execution(self):
        source, _ = self.generated_context()
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()
        source.write_text("void different_assembly(void) {}\n")
        with self.assertRaises(ValueError):
            self.run_audit()

    def test_generated_object_change_cannot_certify_prior_execution(self):
        _, output = self.generated_context()
        matrix.capture(self.root, self.build, self.catalogue, self.snapshot)
        self.write_execution()
        output.write_bytes(b"replaced generated object")
        with self.assertRaises(ValueError):
            self.run_audit()

    def test_missing_generated_object_rejects_before_execution(self):
        _, output = self.generated_context()
        output.unlink()
        with self.assertRaises(ValueError):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_target_local_source_outside_checkout_and_build_rejected(self):
        source, _ = self.generated_context()
        with tempfile.TemporaryDirectory() as directory:
            foreign = Path(directory) / source.name
            foreign.write_bytes(source.read_bytes())
            database = self.build / "compile_commands.json"
            entries = json.loads(database.read_text())
            entries[-1]["file"] = str(foreign)
            entries[-1]["arguments"][2] = str(foreign)
            database.write_text(json.dumps(entries))
            with self.assertRaises(ValueError):
                matrix.capture(self.root, self.build, self.catalogue,
                               self.snapshot)

    def test_target_local_generated_symlink_cannot_expand_ownership(self):
        source, _ = self.generated_context()
        source.unlink()
        source.symlink_to(self.production)
        with self.assertRaises(ValueError):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

    def test_target_marker_cannot_hide_an_escaped_object_path(self):
        _, output = self.generated_context()
        database = self.build / "compile_commands.json"
        entries = json.loads(database.read_text())
        escaped = output.parent / "../../../external.o"
        escaped.write_bytes(output.read_bytes())
        entries[-1]["arguments"][-1] = str(escaped)
        database.write_text(json.dumps(entries))
        with self.assertRaises(ValueError):
            matrix.capture(self.root, self.build, self.catalogue, self.snapshot)

if __name__ == "__main__":
    unittest.main()
