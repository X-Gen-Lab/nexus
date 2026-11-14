"""Serial finite recipes preserve C ownership, wiring and declaration order."""
from dataclasses import replace
from pathlib import Path
import shutil
import json
import subprocess
import tempfile
import unittest

from tools.configure import bindings, configure
from tools.configure.calls import DirectCall
from tools.configure.static_recipe import StaticRecipe


class FaceContextCompileTests(unittest.TestCase):
    def test_static_sequence_context_precedes_the_public_face_initializer(self):
        recipe = StaticRecipe.from_validated({
            "state_type": "nx_smoke_state_t", "ops": "nx_smoke_spi_ops",
            "declarations": [{"role": "sequence", "type": "nx_smoke_state_t", "const": False}],
            "face_context": {"ref": "sequence", "as": "address"},
            "calls": {"prepare": {"function": "nx_smoke_prepare", "args": [
                {"ref": "port", "as": "address"}, {"ref": "sequence", "as": "address"}]},
                "close": [{"function": "nx_smoke_close", "args": [{"ref": "port", "as": "address"}]}]}})
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/zg-spi.toml")

        class Provider:
            MODEL = True
            GPIO_PHASE = None
            FLASH_GEOMETRY = None
            state_declarations = staticmethod(lambda _result: ())
            instance_declarations = staticmethod(lambda _item, _result: ())
            shared_irqs = staticmethod(lambda _result: ())
            includes = staticmethod(lambda _result: ['#include "smoke.h"'])
            operations = staticmethod(lambda _item: "nx_smoke_spi_ops")
            constructor = staticmethod(recipe.construct)

        context = replace(result.provider_context, provider=Provider())
        result = replace(result, provider_context=context)
        header, source = bindings.emit(result)
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "nexus_config.h").write_text("")
            (path / "nexus_factory.h").write_text("")
            (path / "nexus_bindings.h").write_text(header)
            (path / "bindings.c").write_text(source)
            (path / "smoke.h").write_text('''#include "nexus/io/spi.h"
typedef struct { unsigned value; } nx_smoke_state_t;
static const nx_spi_ops_t nx_smoke_spi_ops = {0};
nx_result_t nx_smoke_prepare(nx_smoke_state_t*, nx_smoke_state_t*);
nx_result_t nx_smoke_close(nx_smoke_state_t*);
''')
            command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-fsyntax-only", "-I", str(path),
                *[part for name in ("core/include", "io/include", "arch/include", "core/private")
                  for part in ("-I", str(configure.ROOT / name))], str(path / "bindings.c")]
            completed = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)


class SpiConnectionProjectionTests(unittest.TestCase):
    def test_selected_chip_select_uses_the_same_authorized_pad_leaf(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/zg-spi.toml")
        item, = result.controllers
        self.assertIn("spi", item.integration.constants)
        connected = item.integration.constants["spi"]
        child, = connected.endpoints
        self.assertIs(child.child, result.devices[0])
        expected = result.provider_context.binding_policy.pad_facts[child.cs.identity]
        self.assertEqual((child.cs.registers, child.cs.number),
                         (expected["registers"], expected["number"]))
        self.assertEqual(connected.pads[-1], child.cs)
        self.assertEqual(tuple(connected.endpoint_bindings), (child.child.id,))

    def test_sequence_profile_follows_the_nominal_public_interface(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/io_evolution_assembly/f407-spi-dma-transaction.toml")
        item = next(item for item in result.controllers if item.kind == "spi")
        self.assertEqual(item.interface_kind, "spi_dma_transaction")
        self.assertEqual(item.integration.execution_profile, "sequence")


class SerialConstantEncodingTests(unittest.TestCase):
    def test_private_constant_encoders_are_c11_constant_initializers(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler)
        source = """#include <stdint.h>
#define DMA_SxCR_PL_0 (UINT32_C(1) << 16)
#define DMA_SxCR_MINC (UINT32_C(1) << 10)
#define DMA_SxCR_TCIE (UINT32_C(1) << 4)
#define DMA_SxCR_TEIE (UINT32_C(1) << 2)
#define DMA_SxCR_DMEIE (UINT32_C(1) << 1)
#define DMA_SxCR_DIR_0 (UINT32_C(1) << 6)
#define DMA_SxCR_CHSEL_Pos 25U
#include "stm32f4_io_encoding.h"
_Static_assert(NX_STM32_IO_PIN_MASK(15U) == UINT32_C(0x8000), "pin mask");
_Static_assert(NX_STM32_I2C_DMA_CONFIGURATION(0U, 0) == UINT32_C(0x10416), "rx");
_Static_assert(NX_STM32_I2C_DMA_CONFIGURATION(7U, 1) == UINT32_C(0xe010456), "tx");
_Static_assert(!NX_STM32_I2C_ADDRESS_TEN_BIT(7U), "7-bit address");
_Static_assert(NX_STM32_I2C_ADDRESS_TEN_BIT(10U), "10-bit address");
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "constants.c"
            path.write_text(source)
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-fsyntax-only", "-I",
                str(configure.ROOT / "soc/stm32/stm32f4/private"), str(path)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


class SerialRecipeProjectionTests(unittest.TestCase):
    def library(self):
        from tools.configure.data_provider import Binding
        from tools.configure.static_fragments import StaticFragments
        documents = [json.loads((configure.ROOT / "soc/stm32/stm32f4/bindings" /
            (name + ".json")).read_text()) for name in
            ("spi", "i2c", "spi_frames", "i2c_target")]
        library = {"declarations": {}, "values": {}}
        for document in documents:
            for kind, values in document.get("static_fragments", {}).items():
                self.assertFalse(set(library[kind]) & set(values))
                library[kind].update(values)
        fragments = StaticFragments.from_validated(library)
        return tuple(Binding.create(row, fragments=fragments)
                     for document in documents for row in document["bindings"])

    def test_every_selected_serial_layout_has_one_c_owner_and_typed_connections(self):
        from tools.configure.static_ir import Declaration
        library = self.library()
        fixtures = (
            ("stm32_assembly/zg-spi.toml", "spi"),
            ("stm32_assembly/zg-dma.toml", "spi"),
            ("io_evolution_assembly/f407-spi-transaction.toml", "spi"),
            ("io_evolution_assembly/f407-spi-dma-transaction.toml", "spi"),
            ("spi_frames_assembly/three-frames-ve.toml", "spi"),
            ("stm32_assembly/ve-i2c.toml", "i2c"),
            ("stm32_assembly/zg-i2c-irq.toml", "i2c"),
            ("stm32_assembly/zg-i2c-dma.toml", "i2c"),
            ("multi_controller_assembly/stm32-ve-i2c-pair-irq-freertos.toml", "i2c"),
            ("i2c_target/ve-target.toml", "i2c_target"),
            ("i2c_target/ve-smbus.toml", "i2c_target"))
        covered = set()
        for relative, kind in fixtures:
            with self.subTest(assembly=relative):
                result = configure.resolve_ir(configure.ROOT / "tests/contracts" / relative)
                for item in result.controllers:
                    if item.kind != kind:
                        continue
                    binding, = (entry for entry in library if entry.kind == item.kind and
                        item.mode in entry.modes and item.controller in entry.controllers)
                    built = binding.recipe.construct(item, result, result.board_bindings,
                                                     result.routes, result.devices)
                    covered.add((item.kind, item.mode, item.integration.execution_profile))
                    self.assertFalse(built.definitions)
                    self.assertFalse(built.face_definitions)
                    self.assertTrue(all(isinstance(node, Declaration)
                                        for node in built.static_declarations))
                    self.assertTrue(built.retain_on_failed_init)
                    self.assertEqual(len({node.name for node in built.static_declarations}),
                                     len(built.static_declarations))
                    expected = (item.integration.constants["spi"].pads
                        if item.kind == "spi" and item.mode != "irq-frames" else item.integration.pads)
                    self.assertEqual(set(built.pin_snapshots), {pin.identity for pin in expected})
                    if item.interface_kind == "spi_dma_transaction":
                        self.assertEqual(built.face_context.name, "s_nx_sequence_" + item.id)
                        self.assertEqual(built.stop[0].function, "nx_spi_dma_transaction_stop")
                        self.assertEqual(len(built.initialize_sequences), 1)
        self.assertIn(("spi", "dma", "sequence"), covered)
        self.assertIn(("i2c", "dma-messages", "default"), covered)


if __name__ == "__main__":
    unittest.main()
