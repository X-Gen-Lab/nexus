"""Actual linked ARM cost observations reject mixed or fabricated inputs."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from tests.contracts.test_evidence_support import link_fixture
from tools.evidence.common import EvidenceError, atomic_json
from tools.evidence.identity import canonical_digest
try:
    from tools.measurement import soc_costs
except ImportError:
    soc_costs = None


class SocCostsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.storage = tempfile.TemporaryDirectory()
        cls.fixture = Path(cls.storage.name) / "source"
        cls.fixture.mkdir()
        compiler = (os.environ.get("NEXUS_ARM_GCC") or
                    shutil.which("arm-none-eabi-gcc"))
        if not compiler:
            raise AssertionError("Actual maintained ARM compiler is required")
        cls.compiler = str(Path(compiler).resolve())
        cls.objdump = str(Path(cls.compiler).with_name("arm-none-eabi-objdump"))
        cls.elf, cls.resolved, cls.budget = link_fixture(cls.fixture)
        resolved = json.loads(cls.resolved.read_text())
        resolved.update(cpu={"arch": "cortex-m4", "fpu": "fpv4-sp-d16",
                             "float_abi": "hard"},
                        clock={"core_hz": 168000000}, optimization="Os")
        resolved["configuration_sha256"] = canonical_digest(
            {k: v for k, v in resolved.items() if k != "configuration_sha256"})
        atomic_json(cls.resolved, resolved)
        linker = cls.fixture / "memory.ld"
        content = re.sub(r"__nx_resolved_sha256_\d+ = 0x[0-9a-f]+;", "",
                         linker.read_text())
        content += "\n".join(f"__nx_resolved_sha256_{i} = 0x"
            f"{resolved['configuration_sha256'][i*8:i*8+8]};" for i in range(8))
        linker.write_text(content)
        argv = [cls.compiler, "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
                "-mfloat-abi=hard", "-Os", "-nostdlib", "-T", str(linker),
                str(cls.fixture / "image.c"), "-o", str(cls.elf)]
        subprocess.run(argv, capture_output=True, check=True, timeout=30)
        cls.commands = cls.fixture / "compile_commands.json"
        cls.commands.write_text(json.dumps([{"directory": str(cls.fixture),
            "file": str(cls.fixture / "image.c"), "arguments": argv}]))

    @classmethod
    def tearDownClass(cls):
        cls.storage.cleanup()

    def setUp(self):
        self.assertIsNotNone(soc_costs, "SoC cost observer is missing")
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.output = Path(self.storage.name)

    def observe(self, **kwargs):
        return soc_costs.observe(self.elf, self.resolved, self.budget,
            self.commands,
            output=self.output / str(len(list(self.output.iterdir()))),
            objdump=self.objdump, symbols=["Reset_Handler"], **kwargs)

    def modify_commands(self, mutate):
        path = self.output / "commands.json"
        entries = json.loads(self.commands.read_text())
        mutate(entries)
        path.write_text(json.dumps(entries))
        return path

    def bad_observe(self, **kwargs):
        values = dict(elf_path=self.elf, resolved_path=self.resolved,
                      budget_path=self.budget,
                      compile_commands_path=self.commands,
                      output=self.output / "bad", objdump=self.objdump,
                      symbols=["Reset_Handler"])
        values.update(kwargs)
        with self.assertRaises(EvidenceError):
            soc_costs.observe(**values)

    def candidate(self, *, change=None, code=None, optimization="Os", cpu=None):
        directory = self.output / "candidate-source"
        shutil.copytree(self.fixture, directory)
        resolved = json.loads((directory / "resolved.json").read_text())
        if change:
            change(resolved)
        if cpu is not None:
            resolved["cpu"] = dict(cpu)
        source = directory / "image.c"
        if code:
            source.write_text(code(source.read_text()))
        from tools.evidence.common import digest
        resolved["inputs"][0]["sha256"] = digest(source)
        resolved["configuration_sha256"] = canonical_digest(
            {k: v for k, v in resolved.items() if k != "configuration_sha256"})
        atomic_json(directory / "resolved.json", resolved)
        atomic_json(directory / "input_paths.json", {"image.c": str(source)})
        linker = directory / "memory.ld"
        content = re.sub(r"__nx_resolved_sha256_\d+ = 0x[0-9a-f]+;", "",
                         linker.read_text())
        content += "\n".join(f"__nx_resolved_sha256_{i} = 0x"
            f"{resolved['configuration_sha256'][i*8:i*8+8]};" for i in range(8))
        linker.write_text(content)
        cpu = resolved["cpu"]
        argv = [self.compiler, "-mcpu=" + cpu["arch"], "-mthumb",
                "-mfloat-abi=" + cpu["float_abi"], "-" + optimization,
                "-nostdlib", "-T", str(linker), str(source),
                "-o", str(directory / "image.elf")]
        if cpu["fpu"] != "none":
            argv.insert(3, "-mfpu=" + cpu["fpu"])
        subprocess.run(argv, capture_output=True, check=True, timeout=30)
        (directory / "compile_commands.json").write_text(json.dumps([
            {"directory": str(directory), "file": str(source),
             "arguments": argv}]))
        return soc_costs.observe(directory / "image.elf",
            directory / "resolved.json", directory / "budget.json",
            directory / "compile_commands.json",
            output=self.output / "candidate",
            objdump=self.objdump, symbols=["Reset_Handler"])

    def test_actual_elf_reports_resources_and_thumb_bytes(self):
        report = self.observe()
        self.assertEqual(report["status"], "observed")
        self.assertGreater(report["metrics"]["flash"]["loaded_bytes"], 0)
        self.assertGreater(report["functions"][0]["static_instructions"], 0)
        self.assertEqual(report["cycles_status"], "not_measured")

    def test_identical_actual_inputs_have_zero_delta(self):
        first, second = self.observe(), self.observe()
        growth = {key: 0 for key in soc_costs.cost_values(first["metrics"])}
        comparison = soc_costs.compare(first, second, growth)
        self.assertEqual(comparison["status"], "within_growth_budget")
        self.assertTrue(all(value == 0
                            for value in comparison["delta"].values()))

    def test_same_profile_private_state_subset_has_its_own_growth_ceiling(self):
        baseline = self.observe()
        candidate = self.candidate(code=lambda text: text.replace(
            "zeroed", "s_nx_port_fixture"))
        values = soc_costs.cost_values(baseline["metrics"])
        self.assertIn("static.provider_state_bytes", values)
        growth = {key: 4096 for key in values}
        growth["static.provider_state_bytes"] = 0
        result = soc_costs.compare(baseline, candidate, growth)
        self.assertEqual(result["status"], "growth_exceeded")
        self.assertEqual(result["delta"]["static.provider_state_bytes"], 16)
        self.assertEqual(result["delta"]["ram.sram"], 0)

    def test_static_storage_summary_is_recomputed_from_actual_elf(self):
        report = self.observe()
        self.assertIn("static_storage", report["metrics"])
        report["metrics"]["static_storage"]["bytes"]["provider_state"] += 1
        with self.assertRaises(EvidenceError):
            soc_costs.validate_observation(report)

    def test_unknown_function_is_rejected(self):
        self.bad_observe(symbols=["no_such_function"])

    def test_duplicate_function_selection_is_rejected(self):
        self.bad_observe(symbols=["Reset_Handler", "Reset_Handler"])

    def test_actual_duplicate_locals_require_validated_symbol_identity(self):
        from tools.measurement.elf import Elf32
        sources = []
        for index in range(2):
            source = self.output / f"local{index}.S"
            source.write_text(
                '.syntax unified\n.thumb\n.eabi_attribute 28,1\n.file "local' +
                str(index) + '.c"\n'
                '.section .text.local,"ax",%progbits\n'
                '.local repeated_local\n.type repeated_local,%function\n'
                '.thumb_func\nrepeated_local:\nmovs r0,#' + str(index) +
                '\nbx lr\n.size repeated_local,.-repeated_local\n')
            sources.append(source)
        image = self.output / "locals.elf"
        subprocess.run([
            self.compiler, "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16",
            "-mfloat-abi=hard", "-nostdlib", "-Wl,--build-id=none",
            "-T", str(self.fixture / "memory.ld"),
            str(self.fixture / "image.c"), *map(str, sources), "-o", str(image)
        ], check=True, capture_output=True, timeout=30)
        elf = Elf32(image.read_bytes())
        raw = subprocess.run([self.objdump, "-d", str(image)], check=True,
                             capture_output=True, timeout=30).stdout
        symbols = [(index, symbol) for index, symbol in
                   enumerate(elf.defined_symbols)
                   if symbol["name"] == "repeated_local"]
        self.assertEqual(2, len(symbols))
        self.assertNotEqual(symbols[0][1]["value"], symbols[1][1]["value"])
        self.assertTrue(callable(getattr(soc_costs, "decode_symbol", None)),
                        "actual ELF symbol identity decoder is missing")
        with self.assertRaises(EvidenceError):
            soc_costs.decode_function(elf, "repeated_local", raw)
        selected = []
        for index, symbol in symbols:
            start = symbol["value"] & ~1
            lines = []
            for line in raw.decode("ascii").splitlines(keepends=True):
                match = re.match(r"^\s*([0-9a-f]+):\s", line)
                if match and start <= int(match[1], 16) < start + symbol["size"]:
                    lines.append(line)
            decoded_raw = "".join(lines).encode("ascii")
            selected.append(decoded_raw)
            report = soc_costs.decode_symbol(elf, index, decoded_raw)
            self.assertEqual(start, report["address"])
            self.assertEqual("repeated_local", report["symbol"])
            self.assertEqual(2, report["static_instructions"])
        with self.assertRaises(EvidenceError):
            soc_costs.decode_symbol(elf, symbols[0][0], selected[1])
        for index in (True, -1, len(elf.defined_symbols)):
            with self.subTest(index=index), self.assertRaises(EvidenceError):
                soc_costs.decode_symbol(elf, index, selected[0])
        non_function = next(index for index, symbol in
                            enumerate(elf.defined_symbols)
                            if symbol["type"] != 2)
        with self.assertRaises(EvidenceError):
            soc_costs.decode_symbol(elf, non_function, selected[0])

    def test_symbol_cannot_inject_adapter_options(self):
        self.bad_observe(symbols=["--help"])

    def test_non_arm_elf_is_rejected(self):
        path = self.output / "host.elf"
        path.write_bytes(Path("/usr/bin/true").read_bytes())
        self.bad_observe(elf_path=path)

    def test_empty_elf_is_rejected(self):
        path = self.output / "empty.elf"
        path.touch()
        self.bad_observe(elf_path=path)

    def test_resolved_digest_tamper_is_rejected(self):
        path = self.output / "resolved.json"
        data = json.loads(self.resolved.read_text())
        data["part"] = "wrong"
        path.write_text(json.dumps(data))
        self.bad_observe(resolved_path=path)

    def test_budget_overrun_does_not_become_observed(self):
        path = self.output / "budget.json"
        data = json.loads(self.budget.read_text())
        data["flash_loaded_max"] = 0
        path.write_text(json.dumps(data))
        self.bad_observe(budget_path=path)

    def test_empty_compile_database_is_rejected(self):
        path = self.output / "empty.json"
        path.write_text("[]")
        self.bad_observe(compile_commands_path=path)

    def test_duplicate_compile_database_fields_are_rejected(self):
        path = self.output / "duplicate.json"
        path.write_text('[{"file":"a","file":"b"}]')
        self.bad_observe(compile_commands_path=path)

    def test_response_file_cannot_hide_actual_flags(self):
        path = self.modify_commands(
            lambda rows: rows[0]["arguments"].append("@flags"))
        self.bad_observe(compile_commands_path=path)

    def test_shell_operator_is_not_an_adapter_command(self):
        path = self.modify_commands(
            lambda rows: rows[0]["arguments"].append(";"))
        self.bad_observe(compile_commands_path=path)

    def test_cpu_flag_mismatch_is_rejected(self):
        path = self.modify_commands(
            lambda rows: rows[0]["arguments"].append("-mcpu=cortex-m7"))
        self.bad_observe(compile_commands_path=path)

    def test_non_arm_compiler_metadata_is_rejected(self):
        path = self.modify_commands(
            lambda rows: rows[0]["arguments"].__setitem__(0, "/usr/bin/cc"))
        self.bad_observe(compile_commands_path=path)

    def test_timeout_boolean_is_rejected(self):
        self.bad_observe(timeout_s=True)

    def test_output_cannot_overwrite_artifact_directory(self):
        self.bad_observe(output=self.fixture)

    def test_tool_failure_is_not_success(self):
        self.bad_observe(objdump="/usr/bin/false")

    def test_fabricated_cost_summary_is_rejected(self):
        first, second = self.observe(), self.observe()
        second["metrics"]["flash"]["loaded_bytes"] = 0
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_missing_explicit_growth_limit_is_rejected(self):
        first, second = self.observe(), self.observe()
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_negative_and_boolean_growth_limits_are_rejected(self):
        first, second = self.observe(), self.observe()
        for value in (-1, True):
            growth = {key: value
                      for key in soc_costs.cost_values(first["metrics"])}
            with self.assertRaises(EvidenceError):
                soc_costs.compare(first, second, growth)

    def test_fabricated_comparison_profile_is_rejected(self):
        first, second = self.observe(), self.observe()
        second["comparison_profile"]["part"] = "GD32F470ZGT6"
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_instruction_count_cannot_be_edited(self):
        first, second = self.observe(), self.observe()
        second["functions"][0]["static_instructions"] += 1
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_artifact_mutation_after_observation_is_rejected(self):
        first, second = self.observe(), self.observe()
        second["inputs"]["elf"]["sha256"] = "0" * 64
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_input_changed_during_adapter_is_rejected(self):
        original = self.commands.read_bytes()
        actual = soc_costs.run_adapter
        changed = False
        def adapter(*args, **kwargs):
            nonlocal changed
            output = actual(*args, **kwargs)
            if not changed:
                changed = True
                self.commands.write_bytes(original + b"\n")
            return output
        try:
            with mock.patch.object(soc_costs, "run_adapter",
                                   side_effect=adapter):
                self.bad_observe()
        finally:
            self.commands.write_bytes(original)

    def test_actual_ram_growth_exceeds_explicit_zero_ceiling(self):
        baseline = self.observe()
        candidate = self.candidate(code=lambda text: text.replace(
            "volatile u32 zeroed[4];", "volatile u32 zeroed[6];"))
        growth = {key: 4096
                  for key in soc_costs.cost_values(baseline["metrics"])}
        growth["ram.sram"] = 0
        report = soc_costs.compare(baseline, candidate, growth)
        self.assertEqual(report["status"], "growth_exceeded")
        self.assertEqual(report["delta"]["ram.sram"], 8)

    def test_explicit_growth_allowance_accepts_actual_eight_bytes(self):
        baseline = self.observe()
        candidate = self.candidate(code=lambda text: text.replace(
            "volatile u32 zeroed[4];", "volatile u32 zeroed[6];"))
        growth = {key: 4096
                  for key in soc_costs.cost_values(baseline["metrics"])}
        growth["ram.sram"] = 8
        report = soc_costs.compare(baseline, candidate, growth)
        self.assertEqual(report["status"],
                         "within_growth_budget")

    def test_different_actual_capacity_is_not_comparable(self):
        baseline = self.observe()
        candidate = self.candidate(change=lambda data:
            data["controllers"][0].update(rx_capacity=8))
        with self.assertRaises(EvidenceError):
            soc_costs.compare(baseline, candidate, {})

    def test_different_actual_clock_is_not_comparable(self):
        baseline = self.observe()
        candidate = self.candidate(change=lambda data:
            data["clock"].update(core_hz=84000000))
        with self.assertRaises(EvidenceError):
            soc_costs.compare(baseline, candidate, {})

    def test_different_actual_backend_is_not_comparable(self):
        baseline = self.observe()
        candidate = self.candidate(change=lambda data:
            data.update(backend="freertos"))
        with self.assertRaises(EvidenceError):
            soc_costs.compare(baseline, candidate, {})

    def test_different_actual_optimization_is_not_comparable(self):
        baseline = self.observe()
        candidate = self.candidate(change=lambda data:
            data.update(optimization="O2"), optimization="O2")
        with self.assertRaises(EvidenceError):
            soc_costs.compare(baseline, candidate, {})

    def test_edited_disassembly_encoding_is_not_a_cost_observation(self):
        from tools.evidence.common import file_identity
        first, second = self.observe(), self.observe()
        path = Path(second["functions"][0]["raw"]["path"])
        text = path.read_text()
        text = re.sub(r"(^\s*[0-9a-f]+:\s+)[0-9a-f]{4}", r"\g<1>1111",
                      text, count=1, flags=re.MULTILINE)
        path.write_text(text)
        second["functions"][0]["raw"] = file_identity(path)
        second["executions"][-1]["stdout"] = file_identity(path)
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_timing_claim_cannot_be_inserted_into_static_observation(self):
        first, second = self.observe(), self.observe()
        second["cycles_status"] = "measured"
        growth = {key: 0 for key in soc_costs.cost_values(first["metrics"])}
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, growth)

    def test_slow_tool_is_killed_by_bounded_adapter(self):
        path = self.output / "slow-tool"
        path.write_text("#!/usr/bin/python3\nimport time\ntime.sleep(2)\n")
        path.chmod(0o700)
        self.bad_observe(objdump=str(path), timeout_s=0.02)

    def test_truncated_disassembly_cannot_lower_instruction_count(self):
        from tools.measurement.elf import Elf32
        report = self.observe()
        raw = Path(report["functions"][0]["raw"]["path"]).read_text()
        raw = re.sub(r"^\s*[0-9a-f]+:\s+[0-9a-f]{4}[^\n]*\n", "", raw,
                     count=1, flags=re.MULTILINE)
        with self.assertRaises(EvidenceError):
            soc_costs.decode_function(Elf32(self.elf.read_bytes()),
                                     "Reset_Handler", raw.encode())

    def test_thumb_instruction_cannot_be_relabelled_as_literal_data(self):
        from tools.measurement.elf import Elf32
        report = self.observe()
        raw = Path(report["functions"][0]["raw"]["path"]).read_text()
        raw = re.sub(r"(^\s*[0-9a-f]+:\s+[0-9a-f]{4}\s+)\w+",
                     r"\g<1>.word", raw, count=1, flags=re.MULTILINE)
        with self.assertRaises(EvidenceError):
            soc_costs.decode_function(Elf32(self.elf.read_bytes()),
                                     "Reset_Handler", raw.encode())

    def test_actual_no_fpu_thumb_elf_uses_explicit_soft_abi(self):
        candidate = self.candidate(cpu={"arch": "cortex-m0", "fpu": "none",
                                       "float_abi": "soft"})
        self.assertEqual(candidate["status"], "observed")
        self.assertGreater(candidate["functions"][0]["static_instructions"], 0)

    def test_actual_softfp_fpu_observation_and_reverification_keep_compile_abi(self):
        candidate = self.candidate(
            cpu={"arch": "cortex-m4", "fpu": "fpv4-sp-d16",
                 "float_abi": "softfp"},
            code=lambda source: source +
                "\nfloat nx_abi_add(float a, float b) { return a + b; }\n")
        self.assertEqual(candidate["status"], "observed")
        self.assertGreater(candidate["functions"][0]["static_instructions"], 0)
        resolved = json.loads(
            (self.output / "candidate-source/resolved.json").read_text())
        self.assertEqual(resolved["cpu"]["float_abi"], "softfp")
        commands = json.loads(
            (self.output / "candidate-source/compile_commands.json").read_text())
        self.assertIn("-mfloat-abi=softfp", commands[0]["arguments"])
        soc_costs.validate_observation(candidate)

    def test_actual_no_fpu_object_rejects_hard_abi_interpretation(self):
        from tools.measurement.elf import Elf32
        self.candidate(cpu={"arch": "cortex-m0", "fpu": "none",
                            "float_abi": "soft"})
        data = (self.output / "candidate-source/image.elf").read_bytes()
        with self.assertRaises(EvidenceError):
            Elf32(data)
        self.assertGreater(Elf32(data, float_abi="soft").entry, 0)

    def test_hard_float_elf_rejects_soft_and_unknown_abi(self):
        from tools.measurement.elf import Elf32
        for abi in ("soft", "softfp", "unknown", True):
            with self.assertRaises(EvidenceError):
                Elf32(self.elf.read_bytes(), float_abi=abi)

    def test_explicit_soft_resource_scope_rejects_hard_ir(self):
        from tools.measurement.resources import measure
        with self.assertRaises(EvidenceError):
            measure(self.elf, self.resolved, self.budget, float_abi="soft")

    def test_two_thumb_halfwords_cannot_be_grouped_as_one_instruction(self):
        from tools.measurement.elf import Elf32
        report = self.observe()
        raw = Path(report["functions"][0]["raw"]["path"]).read_text()
        pattern = (r"(^\s*([0-9a-f]+):\s+([0-9a-f]{4})[^\n]*\n)"
                   r"\s*([0-9a-f]+):\s+([0-9a-f]{4})[^\n]*\n")
        match = re.search(pattern, raw, re.MULTILINE)
        self.assertIsNotNone(match, "actual fixture needs adjacent halfwords")
        self.assertLess(int(match[3], 16) >> 11, 29)
        self.assertEqual(int(match[4], 16), int(match[2], 16) + 2)
        raw = (raw[:match.start()] + f" {match[2]}: {match[3]} {match[5]}"
               "  mov.w r0, r0\n" + raw[match.end():])
        with self.assertRaises(EvidenceError):
            soc_costs.decode_function(Elf32(self.elf.read_bytes()),
                                     "Reset_Handler", raw.encode())

    def test_deleted_execution_receipts_cannot_be_compared(self):
        first, second = self.observe(), self.observe()
        second["executions"] = []
        growth = {key: 0 for key in soc_costs.cost_values(first["metrics"])}
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, growth)

    def test_function_receipt_cannot_name_another_symbol(self):
        first, second = self.observe(), self.observe()
        second["executions"][-1]["argv"][3] = "--disassemble=another_symbol"
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_compiler_target_receipt_cannot_be_relabelled_as_version(self):
        first, second = self.observe(), self.observe()
        second["executions"][2]["argv"][1] = "--version"
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_receipt_timeout_must_remain_bounded(self):
        first, second = self.observe(), self.observe()
        second["executions"][0]["timeout_s"] = 31
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_receipt_raw_output_must_match_function_raw_identity(self):
        first, second = self.observe(), self.observe()
        second["executions"][-1]["stdout"] = second["executions"][0]["stdout"]
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_receipt_compiler_target_must_be_actual_arm_target(self):
        from tools.evidence.common import file_identity
        first, second = self.observe(), self.observe()
        path = Path(second["executions"][2]["stdout"]["path"])
        path.write_bytes(b"x86_64-linux-gnu\n")
        second["executions"][2]["stdout"] = file_identity(path)
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_extra_receipt_cannot_be_inserted_into_execution_closure(self):
        first, second = self.observe(), self.observe()
        second["executions"].append(dict(second["executions"][0]))
        with self.assertRaises(EvidenceError):
            soc_costs.compare(first, second, {})

    def test_same_cs_binding_name_cannot_hide_different_physical_route(self):
        import copy
        data = json.loads(self.resolved.read_text())
        data.update(board_bindings={"primary-cs": {
            "controller": "GPIOE", "route": "gpio-pe4", "mask": 16,
            "initial": 16}},
            routes={"gpio-pe4": {"controller": "GPIOE", "kind": "gpio",
                "pins": [{"pin": "PE4", "function": "output", "af": 0}]}},
            claims=[{"key": "pin:PE4", "owner": "primary_sensor"}])
        data["controllers"] = [{"id": "primary_spi", "controller": "SPI1",
                                 "kind": "spi", "mode": "short-poll"}]
        data["devices"] = [{"id": "primary_sensor", "controller": "primary_spi",
            "driver": "spi-endpoint", "cs_binding": "primary-cs",
            "max_hz": 1000000, "mode": 0}]
        changed = copy.deepcopy(data)
        changed["board_bindings"]["primary-cs"] = {
            "controller": "GPIOG", "route": "gpio-pg9", "mask": 512,
            "initial": 512}
        changed["routes"] = {"gpio-pg9": {"controller": "GPIOG", "kind": "gpio",
            "pins": [{"pin": "PG9", "function": "output", "af": 0}]}}
        changed["claims"] = [{"key": "pin:PG9", "owner": "primary_sensor"}]
        self.assertNotEqual(soc_costs.comparison_profile(data, []),
                            soc_costs.comparison_profile(changed, []))


if __name__ == "__main__":
    unittest.main()
