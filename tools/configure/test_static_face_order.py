"""A valid auxiliary face context must be declared before its C initializer."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.configure import bindings, configure
from tools.configure.providers import for_selection
from tools.configure.static_recipe import StaticRecipe


class StaticFaceOrderTests(unittest.TestCase):
    def test_auxiliary_context_compiles_through_the_real_bindings_emitter(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/uart_rx_compact_assembly/f407-dma-duplex.toml")
        item, = result.controllers
        provider = for_selection(result)
        recipe = StaticRecipe.from_validated({
            "state_type": "review_state_t", "ops": "review_ops",
            "declarations": [{"role": "context", "type": "uint32_t",
                              "const": False}],
            "face_context": {"ref": "context", "as": "address"},
            "calls": {"prepare": {"function": "review_prepare", "args": []},
                      "close": []}})
        construction = recipe.construct(item, result, result.board_bindings,
                                        result.routes, result.devices)
        with patch.object(type(provider), "constructor", return_value=construction), \
                patch.object(type(provider), "operations", return_value="review_ops"):
            _, generated = bindings.emit(result)

        # Compile the exact declaration span from the actual emitter with the
        # public interface type; the private driver implementation is irrelevant
        # to whether this otherwise valid static connection is legal C11.
        names = ("s_nx_port_" + item.id, "s_nx_face_" + item.id,
                 "nx_binding_" + item.id, construction.face_context.name)
        lines = generated.splitlines()
        relevant = [index for index, line in enumerate(lines)
                    if any(name in line for name in names)]
        fragment = "\n".join(lines[min(relevant):max(relevant) + 1])
        source = ("#include <stdint.h>\n#include \"nexus/io/uart.h\"\n"
                  "typedef uint32_t review_state_t;\n"
                  "static const nx_uart_ops_t review_ops = {0};\n" +
                  fragment + "\n")
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "A maintained C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bindings.c"
            path.write_text(source, encoding="utf-8")
            command = [compiler, "-std=c11", "-pedantic-errors", "-fsyntax-only"]
            for root in ("io", "core", "arch", "os"):
                command.extend(("-I", str(configure.ROOT / root / "include")))
            compiled = subprocess.run(command + [str(path)], capture_output=True,
                                      text=True, check=False)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)


if __name__ == "__main__":
    unittest.main()
