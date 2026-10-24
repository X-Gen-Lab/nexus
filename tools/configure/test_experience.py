"""Exercise developer decisions through the real configuration command."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import shutil
import tempfile
import tomllib
import unittest
from unittest.mock import patch

from tools.configure import authored, configure, experience


class ConfigurationExperienceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("dependencies/platform-compatibility.json",
                     "soc/native/soc.json", "soc/native/routes.json",
                     "boards/native_reference/board.json"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(configure.ROOT / name, target)
        self.assembly = self.root / "application.toml"
        self.base = ('schema = 2\nboard = "native_reference"\n'
                     'backend = "native"\nclock = "model"\n'
                     '[memory]\nmain_stack_bytes = 2048\n')
        self.assembly.write_text(self.base)

    def command(self, *arguments, source_root=None):
        output, errors = io.StringIO(), io.StringIO()
        with redirect_stdout(output), redirect_stderr(errors):
            try:
                status = configure.main([
                    *arguments, "--source-root", str(source_root or self.root)])
            except SystemExit as error:
                status = error.code
        return status, output.getvalue(), errors.getvalue()

    def test_explain_is_readable_and_does_not_qualify_native_hardware(self):
        status, output, _ = self.command(
            "explain", "--assembly", str(self.assembly))
        self.assertEqual(status, 0)
        self.assertIn("Platform: NATIVE_MODEL", output)
        self.assertIn("Clock: model", output)
        self.assertIn("Physical verification: not_applicable", output)
        self.assertIn("Declared budgets", output)
        self.assertFalse((self.root / "generated").exists())

    def test_json_explain_is_the_exact_resolved_projection(self):
        status, output, _ = self.command(
            "explain", "--assembly", str(self.assembly), "--json")
        self.assertEqual(status, 0)
        expected = configure.resolve_ir(self.assembly, self.root).to_dict()
        self.assertEqual(json.loads(output), expected)

    def test_board_bindings_do_not_require_an_authored_application(self):
        before = sorted(str(path) for path in self.root.rglob("*"))
        status, output, _ = self.command(
            "list-bindings", "--board", "native_reference", "--kind", "uart")
        self.assertEqual(status, 0)
        self.assertIn("uart0", output)
        self.assertIn("UART0", output)
        self.assertNotIn("led0:", output)
        self.assertIn("configuration-required", output)
        after = sorted(str(path) for path in self.root.rglob("*"))
        self.assertEqual(before, after)

    def test_binding_modes_project_implementation_and_execution_shape(self):
        status, output, errors = self.command(
            "list-bindings", "--board", "native_reference", "--kind", "uart",
            "--json")
        self.assertEqual(status, 0, errors)
        view = json.loads(output)
        self.assertEqual(view["physical_verification"], "not_applicable")
        self.assertIsNone(view["selection"]["backend"])
        self.assertIsNone(view["selection"]["clock_profile"])
        mode = view["bindings"][0]["mode_contracts"]["irq-byte-event"]
        self.assertTrue(mode["execution"]["irq"])
        self.assertTrue(mode["model"])
        self.assertTrue(mode["implementation"])
        self.assertTrue(view["fact_sources"])

    def test_explain_projects_selected_public_contract_and_borrow_limits(self):
        self.assembly.write_text(self.base +
            '[uart.link]\nbinding="uart0"\nmode="irq"\nbaud=115200\n'
            '[uart.link.rx]\nformat="events"\ncapacity=64\n'
            '[uart.link.irq]\npriority=5\ncalls_os=false\n')
        status, output, errors = self.command(
            "explain", "--assembly", str(self.assembly))
        self.assertEqual(status, 0, errors)
        self.assertIn("Public face: uart", output)
        self.assertIn("MODEL", output)
        self.assertIn("unit=bytes", output)
        self.assertIn("SETTLED", output)

    def test_board_query_rejects_unknown_kind_and_board_without_fallback(self):
        for extra in (("--kind", "invented"),
                      ("--board", "../native_reference")):
            with self.subTest(extra=extra):
                arguments = ["list-bindings", "--board", "native_reference"]
                if extra[0] == "--board":
                    arguments = ["list-bindings"]
                status, _, _ = self.command(*arguments, *extra)
                self.assertNotEqual(status, 0)

    def test_board_fact_error_identifies_its_file_and_key(self):
        board = self.root / "boards/native_reference/board.json"
        facts = json.loads(board.read_text())
        facts["unknowns"] = "not a list of reviewed unknowns"
        board.write_text(json.dumps(facts))
        status, _, errors = self.command(
            "list-bindings", "--board", "native_reference")
        self.assertEqual(status, 1)
        self.assertIn(str(board), errors)
        self.assertIn("unknowns", errors)

    def test_selected_binding_reports_conflict_and_keeps_both_owners(self):
        self.assembly.write_text(
            self.base + '[gpio.first]\nbinding="led0"\nmode="output"\n')
        status, output, _ = self.command(
            "list-bindings", "--assembly", str(self.assembly), "--kind", "gpio")
        self.assertEqual(status, 0)
        self.assertIn("first", output)
        self.assertIn("pin:PA0", output)
        self.assembly.write_text(
            self.assembly.read_text() +
            '[gpio.second]\nbinding="led0"\nmode="output"\n')
        status, _, errors = self.command(
            "check", "--assembly", str(self.assembly))
        self.assertEqual(status, 1)
        self.assertIn(str(self.assembly), errors)
        self.assertIn("first", errors)
        self.assertIn("second", errors)
        self.assertIn("gpio-pa0", errors)

    def test_invalid_authored_key_retains_file_and_key_context(self):
        self.assembly.write_text(self.base + "heap_dynamic = true\n")
        status, _, errors = self.command(
            "check", "--assembly", str(self.assembly))
        self.assertEqual(status, 1)
        self.assertIn(str(self.assembly), errors)
        self.assertIn("memory", errors)
        self.assertIn("heap_dynamic", errors)

    def test_init_uart_profile_validates_explicit_binding(self):
        output = self.root / "new.toml"
        arguments = ("init", "--board", "native_reference", "--backend",
                     "native", "--clock", "model", "--profile", "uart",
                     "--binding", "uart0", "--output", str(output))
        status, _, errors = self.command(*arguments)
        self.assertEqual(status, 0, errors)
        result = configure.resolve_ir(output, self.root)
        self.assertEqual(len(result.controllers), 1)
        self.assertEqual(result.controllers[0].kind, "uart")
        self.assertEqual(result.controllers[0].options.baud, 115200)
        previous = output.read_bytes()
        self.assertEqual(self.command(*arguments)[0], 1)
        self.assertEqual(output.read_bytes(), previous)

    def test_init_rejected_binding_does_not_leave_partial_file(self):
        output = self.root / "new.toml"
        status, _, _ = self.command(
            "init", "--board", "native_reference", "--backend", "native",
            "--clock", "model", "--profile", "uart", "--binding", "led0",
            "--output", str(output))
        self.assertEqual(status, 1)
        self.assertFalse(output.exists())

    def test_private_board_init_stays_relative_after_product_relocation(self):
        product = self.root / "product"
        package = product / "hardware" / "private-board"
        shutil.copytree(self.root / "boards/native_reference", package)
        output = product / "assembly.toml"
        status, _, errors = self.command(
            "init", "--board-package", str(package), "--backend", "native",
            "--clock", "model", "--profile", "uart", "--binding", "uart0",
            "--output", str(output))
        self.assertEqual(status, 0, errors)
        document = tomllib.loads(output.read_text())
        self.assertNotIn("board", document)
        self.assertEqual(document["board_package"], "hardware/private-board")
        moved = self.root / "relocated product"
        product.rename(moved)
        resolved = configure.resolve_ir(moved / "assembly.toml", self.root)
        self.assertEqual(resolved.controllers[0].controller, "UART0")

    def test_private_board_init_rejects_two_selectors_before_writing(self):
        output = self.root / "ambiguous.toml"
        status, _, errors = self.command(
            "init", "--board", "native_reference", "--board-package",
            str(self.root / "boards/native_reference"), "--backend", "native",
            "--clock", "model", "--output", str(output))
        self.assertNotEqual(status, 0)
        self.assertIn("exactly one", errors)
        self.assertFalse(output.exists())

    def test_mode_query_reports_dma_irq_and_actual_owner(self):
        assembly = configure.ROOT / (
            "tools/configure/assemblies/qiming-baremetal-uart-dma.toml")
        status, output, errors = self.command(
            "list-bindings", "--assembly", str(assembly), "--kind", "uart",
            "--json", source_root=configure.ROOT)
        self.assertEqual(status, 0, errors)
        row = next(item for item in json.loads(output)["bindings"]
                   if item["binding"] == "uart1")
        mode = row["mode_contracts"]["dma-tx"]
        self.assertIn("dma:DMA2:stream7", mode["resource_keys"])
        self.assertIn("irq:DMA2_Stream7", mode["resource_keys"])
        self.assertIn("irq:USART1", mode["resource_keys"])
        owners = {(item["key"], item["owner"])
                  for item in mode["occupied_resources"]}
        self.assertIn(("dma:DMA2:stream7", "uart1"), owners)
        self.assertEqual(mode["admission"], "configuration-required")

    def test_polling_mode_does_not_claim_its_alternative_dma_route(self):
        status, output, errors = self.command(
            "list-bindings", "--board", "gd32f470_liangshan",
            "--kind", "spi", "--json", source_root=configure.ROOT)
        self.assertEqual(status, 0, errors)
        row = json.loads(output)["bindings"][0]
        mode = row["mode_contracts"]["short-poll"]
        self.assertEqual(mode["dma"], [])
        self.assertFalse(any(key.startswith("dma:")
                             for key in mode["resource_keys"]))

    def test_bound_soc_view_rejects_changed_facts(self):
        resolved = configure.resolve_ir(self.assembly, self.root)
        path = self.root / "soc/native/soc.json"
        path.write_text(path.read_text() + "\n")
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    "differ from the resolved input"):
            experience.binding_facts(configure, resolved, self.root)

    def test_factory_order_impact_uses_selected_public_class(self):
        path = self.root / "boards/native_reference/board.json"
        board = json.loads(path.read_text())
        board["bindings"].append({"id": "uart1", "controller": "UART1",
                                  "route": "uart1"})
        path.write_text(json.dumps(board))
        def uart(name, binding):
            return (f'\n[uart.{name}]\nbinding="{binding}"\nmode="irq"\n'
                    f'baud=115200\n[uart.{name}.rx]\nformat="events"\n'
                    f'capacity=64\n[uart.{name}.irq]\npriority=5\n'
                    'calls_os=false\n')
        first, second = uart("first", "uart0"), uart("second", "uart1")
        self.assembly.write_text(self.base + first + second)
        other = self.root / "reordered.toml"
        other.write_text(self.base + second + first)
        status, output, errors = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other),
            "--json")
        self.assertEqual(status, 0, errors)
        impacts = json.loads(output)["impacts"]
        item = next(value for value in impacts
                    if value["path"] == "factory_ids.uart")
        self.assertEqual(item["before"], ["first", "second"])
        self.assertEqual(item["after"], ["second", "first"])

    def test_board_first_owns_silicon_and_board_reservations(self):
        path = self.root / "boards/native_reference/board.json"
        board = json.loads(path.read_text())
        board["reserved_resources"] = ["PA1", "controller:UART0"]
        path.write_text(json.dumps(board))
        path = self.root / "soc/native/soc.json"
        soc = json.loads(path.read_text())
        soc["reserved_resources"] = ["controller:UART0"]
        path.write_text(json.dumps(soc))
        status, output, errors = self.command(
            "list-bindings", "--board", "native_reference", "--kind", "uart",
            "--json")
        self.assertEqual(status, 0, errors)
        row = json.loads(output)["bindings"][0]
        mode = row["mode_contracts"]["irq-byte-event"]
        owners = {(item["key"], item["owner"])
                  for item in mode["occupied_resources"]}
        self.assertIn(("controller:UART0", "soc-timebase"), owners)
        self.assertIn(("pin:PA1", "board-reserved"), owners)
        self.assertEqual(mode["admission"], "configuration-required")


    def test_init_write_failure_does_not_leave_partial_file(self):
        output = self.root / "new.toml"
        real_open = Path.open

        class FailingWriter:
            def __init__(self, stream):
                self.stream = stream

            def write(self, text):
                self.stream.write(text[:20])
                raise OSError("simulated full disk")

            def __enter__(self):
                return self

            def __exit__(self, *unused):
                self.stream.close()

        def open_path(path, mode="r", *args, **kwargs):
            stream = real_open(path, mode, *args, **kwargs)
            return FailingWriter(stream) if mode == "x" else stream

        with patch.object(Path, "open", open_path):
            status, _, errors = self.command(
                "init", "--board", "native_reference", "--backend", "native",
                "--clock", "model", "--output", str(output))
        self.assertEqual(status, 1)
        self.assertIn("simulated full disk", errors)
        self.assertFalse(output.exists())

    def test_diff_describes_changed_budget_without_claiming_measurement(self):
        other = self.root / "other.toml"
        other.write_text(self.base.replace("2048", "4096"))
        status, output, _ = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other))
        self.assertEqual(status, 0)
        self.assertIn("memory_budgets.main_stack_bytes: 2048 -> 4096", output)
        self.assertIn("not measured", output)

    def test_diff_identifies_the_changed_instance_storage(self):
        self.assembly.write_text(self.base +
            '[uart.link]\nbinding="uart0"\nmode="irq"\nbaud=115200\n'
            '[uart.link.rx]\nformat="events"\ncapacity=64\n'
            '[uart.link.irq]\npriority=5\ncalls_os=false\n')
        other = self.root / "larger.toml"
        other.write_text(self.assembly.read_text().replace("capacity=64",
                                                          "capacity=128"))
        status, output, errors = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other))
        self.assertEqual(status, 0, errors)
        self.assertIn("controllers.link.rx_capacity: 64 -> 128", output)

    def test_diff_explains_public_word_face_and_transfer_unit_change(self):
        byte = (self.base +
            '[uart.link]\nbinding="uart0"\nmode="irq"\nbaud=115200\n'
            '[uart.link.rx]\nformat="events"\ncapacity=64\n'
            '[uart.link.irq]\npriority=5\ncalls_os=false\n')
        self.assembly.write_text(byte)
        other = self.root / "words.toml"
        other.write_text(byte.replace('mode="irq"', 'mode="irq-words"')
                         .replace('format="events"', 'format="words"') +
                         '[uart.link.frame]\ndata_bits=9\nparity="none"\n'
                         'stop_bits=1\nflow="none"\n')
        status, output, errors = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other),
            "--json")
        self.assertEqual(status, 0, errors)
        impacts = json.loads(output)["impacts"]
        face = next(item for item in impacts
                    if item["category"] == "public_interface")
        self.assertEqual((face["before"], face["after"]),
                         ("uart", "uart_word"))
        transfer = next(item for item in impacts
                        if item["category"] == "transfer_contract")
        self.assertEqual(transfer["before"]["unit"], "bytes")
        self.assertEqual(transfer["after"]["unit"], "words")
        self.assertIn("not measured", json.loads(output)["measurement"])

    def test_diff_explains_changed_declared_storage_without_savings_claim(self):
        other = self.root / "larger.toml"
        other.write_text(self.base.replace("2048", "4096"))
        status, output, errors = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other),
            "--json")
        self.assertEqual(status, 0, errors)
        impacts = json.loads(output)["impacts"]
        storage = next(item for item in impacts
                       if item["category"] == "storage")
        self.assertEqual(storage["path"], "memory_budgets.main_stack_bytes")
        self.assertIn("measure", storage["obligation"])

    def test_diff_keeps_instance_order_as_an_explicit_change(self):
        before = configure.resolve_ir(self.assembly, self.root).to_dict()
        before["controllers"] = [{"id": "first"}, {"id": "second"}]
        after = dict(before, controllers=list(reversed(before["controllers"])))
        changes = experience.differences(before, after)["changes"]
        self.assertEqual(len(changes), 1)
        self.assertEqual(changes[0]["path"], "controllers.$order")
        self.assertEqual(changes[0]["after"], ["second", "first"])

    def test_diff_json_excludes_input_identity_noise(self):
        other = self.root / "same.toml"
        other.write_text(self.base)
        status, output, _ = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other),
            "--json")
        self.assertEqual(status, 0)
        self.assertEqual(json.loads(output)["changes"], [])

    def test_diff_rejects_invalid_second_input(self):
        other = self.root / "invalid.toml"
        other.write_text(self.base.replace("model", "unknown-clock"))
        status, _, errors = self.command(
            "diff", "--assembly", str(self.assembly), "--against", str(other))
        self.assertEqual(status, 1)
        self.assertIn(str(other), errors)

    def test_diff_distinguishes_missing_from_explicit_null(self):
        before = configure.resolve_ir(self.assembly, self.root).to_dict()
        after = dict(before, runtime_power=None)
        changes = experience.differences(before, after)["changes"]
        self.assertEqual(len(changes), 1)
        self.assertEqual(changes[0]["path"], "runtime_power")
        self.assertFalse(changes[0]["before_present"])
        self.assertTrue(changes[0]["after_present"])

    def test_mcu_explanation_cannot_infer_a_station_result(self):
        data = configure.resolve_ir(self.assembly, self.root).to_dict()
        # Presentation cannot turn a model's or a caller's qualification flag
        # into physical evidence for a different chip.
        data["soc_family"] = "stm32f407"
        data["part"] = "STM32F407VET6"
        data["physical_qualified"] = True
        self.assertIn("not_established_by_configuration",
                      experience.describe(data))

    def test_schema_explains_units_and_lifecycle_responsibility(self):
        properties = authored.editor_schema()["properties"]
        memory = properties["memory"]["properties"]
        uart = properties["uart"]["additionalProperties"]["properties"]
        self.assertIn("bytes", memory["main_stack_bytes"]["description"])
        self.assertIn("bits per second", uart["baud"]["description"])
        self.assertIn("service", uart["mode"]["description"])


if __name__ == "__main__":
    unittest.main()
