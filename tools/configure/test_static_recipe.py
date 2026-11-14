"""Finite provider recipes project immutable IR into compiler-checked C."""
from dataclasses import FrozenInstanceError, dataclass, replace
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import MappingProxyType
import unittest

from tools.configure.static_recipe import StaticRecipe
from tools.configure.calls import Symbol


@dataclass(frozen=True)
class Options:
    mask: int = 3
    initial: int = 1
    pull: str = "up"
    missing: int | None = None


@dataclass(frozen=True)
class Integration:
    pins: tuple = (MappingProxyType({"number": 2, "alternate": 7}),
                   MappingProxyType({"number": 3, "alternate": 7}))
    handler: str = "UART1_IRQHandler"
    vectors: tuple = (MappingProxyType({"handler": "UART1_IRQHandler"}),)


@dataclass(frozen=True)
class Item:
    id: str = "test-port"
    controller: str = "GPIOA"
    mode: str = "output"
    options: Options = Options()
    integration: Integration = Integration()


def gpio_recipe():
    return {
        "state_type": "gpio_state_t", "ops": "gpio_ops",
        "retain_on_failed_init": False,
        "declarations": [{"role": "gpio_config", "type": "gpio_config_t",
                          "const": True, "value": {"record": [
            ["registers", {"source": "item.controller", "as": "symbol"}],
            ["mask", {"source": "item.options.mask"}],
            ["initial", {"source": "item.options.initial"}],
            ["output", {"source": "item.mode", "values": {
                "input": False, "output": True}}],
            ["pull", {"source": "item.options.pull", "values": {
                "none": 0, "up": 1, "down": 2}}]]}}],
        "calls": {
            "admit": [{"function": "gpio_admit", "args": [
                {"ref": "gpio_config", "as": "address"}]}],
            "prepare": {"function": "gpio_prepare", "args": [
                {"ref": "port", "as": "address"},
                {"ref": "gpio_config", "as": "address"}]},
            "close": [{"function": "gpio_stop", "args": [
                {"ref": "port", "as": "address"},
                {"source": "item.options.initial"}]}]},
        "irq": [{"name": {"source": "selected.handler", "as": "symbol"},
                 "calls": [{"function": "gpio_irq", "args": [
                     {"ref": "port", "as": "address"}]}],
                 "barrier": "nx_arch_dsb"}]}


class StaticRecipeTests(unittest.TestCase):
    def construct(self, author=None, item=None):
        return StaticRecipe.from_validated(
            author if author is not None else gpio_recipe()).construct(
                item if item is not None else Item(), None,
                MappingProxyType({}), MappingProxyType({}), ())

    def test_gpio_is_only_static_config_and_fixed_connections(self):
        result = self.construct()
        self.assertEqual(result.definitions, ())
        self.assertEqual(result.state_type, "gpio_state_t")
        self.assertFalse(result.retain_on_failed_init)
        declaration, = result.static_declarations
        self.assertEqual(declaration.name, "s_nx_gpio_config_test_port")
        self.assertEqual(declaration.initializer.render(),
            "{ .registers = GPIOA, .mask = 3u, .initial = 1u, "
            ".output = true, .pull = 1u }")
        self.assertEqual(result.initialize.render(),
            "gpio_prepare(&s_nx_port_test_port, &s_nx_gpio_config_test_port)")
        self.assertEqual(result.stop[0].render(),
                         "gpio_stop(&s_nx_port_test_port, 1u)")
        self.assertEqual(result.irq_connections[0].name, "UART1_IRQHandler")

    def test_author_mutation_cannot_change_a_validated_recipe(self):
        author = gpio_recipe()
        recipe = StaticRecipe.from_validated(author)
        author["declarations"][0]["value"]["record"][0][1]["source"] = "bad"
        author["calls"]["close"].clear()
        result = recipe.construct(Item(), None, {}, {}, ())
        self.assertEqual(len(result.stop), 1)
        self.assertIn(".registers = GPIOA", result.static_declarations[0].render()[0])
        with self.assertRaises(FrozenInstanceError):
            recipe.state_type = "mutated_t"

    def test_operations_have_one_static_symbol_authority(self):
        self.assertEqual(StaticRecipe.from_validated(gpio_recipe()).operations(Item()), "gpio_ops")
        author = gpio_recipe()
        author["ops"] = {"source": "item.controller", "as": "symbol",
                         "values": {"GPIOA": "gpio_ops"}}
        recipe = StaticRecipe.from_validated(author)
        self.assertEqual(recipe.operations(Item()), "gpio_ops")
        with self.assertRaises(ValueError):
            recipe.operations(replace(Item(), controller="GPIOB"))

    def test_unknown_fields_unordered_and_executable_values_are_rejected(self):
        for mutate in (
                lambda value: value.update(eval="write_register()"),
                lambda value: value.update(declarations=set()),
                lambda value: value["calls"].update(prepare={
                    "function": "GPIOA->ODR = 1", "args": []}),
                lambda value: value["declarations"][0].update(
                    value="GPIOA->ODR |= 1"),
                lambda value: value["declarations"][0].update(
                    value={"record": {"mask": 3}}),
                lambda value: value["declarations"][0].update(
                    value={"source": "item.__class__"}),
                lambda value: value["declarations"][0].update(
                    value={"source": "result.memory"})):
            author = gpio_recipe()
            mutate(author)
            with self.subTest(author=author), self.assertRaises(ValueError):
                StaticRecipe.from_validated(author)

    def test_refs_have_declared_roles_and_only_fixed_member_or_index(self):
        for reference in (
                {"ref": "missing", "as": "address"},
                {"ref": "port", "as": "address", "member": "x->y"},
                {"ref": "port", "as": "address", "index": -1},
                {"ref": "port", "as": "address", "index": True},
                {"ref": "port", "as": "address", "index": "item.options.mask"},
                {"ref": "port", "as": "address", "member": "x", "index": 1}):
            author = gpio_recipe()
            author["calls"]["prepare"]["args"][0] = reference
            with self.subTest(reference=reference), self.assertRaises(ValueError):
                self.construct(author)

    def test_required_enum_and_source_values_fail_without_silent_fallback(self):
        for item in (replace(Item(), mode="alternate"),
                     replace(Item(), options=replace(Options(), mask=True)),
                     replace(Item(), options=replace(Options(), pull="other"))):
            author = gpio_recipe()
            author["declarations"][0]["value"]["record"][1][1]["as"] = "u32"
            with self.subTest(item=item), self.assertRaises(ValueError):
                self.construct(author, item)
        author = gpio_recipe()
        author["declarations"][0]["value"]["record"][1][1]["source"] = "item.unknown"
        with self.assertRaises(ValueError):
            self.construct(author)

    def test_mutable_or_unordered_typed_inputs_are_not_production_ir(self):
        for pins in ([{"number": 2}], ({"number": 2},), frozenset({2})):
            author = gpio_recipe()
            author["declarations"] = [{
                "role": "pins", "type": "pin_t", "const": True,
                "count": {"length": "selected.pins", "limit": 8},
                "value": {"map": {"source": "selected.pins", "limit": 8,
                    "value": {"record": [["number", {
                        "source": "entry.number", "as": "u32"}]]}}}}]
            author["calls"]["admit"] = []
            author["calls"]["prepare"]["args"] = [{"ref": "port", "as": "address"}]
            with self.subTest(pins=pins), self.assertRaises(ValueError):
                self.construct(author, replace(Item(), integration=replace(
                    Integration(), pins=pins)))

    def test_bounded_mapping_declares_only_existing_ordered_ir_entries(self):
        author = gpio_recipe()
        author["declarations"].insert(0, {
            "role": "pins", "type": "pin_t", "const": True,
            "count": {"length": "selected.pins", "limit": 8},
            "value": {"map": {"source": "selected.pins", "limit": 8,
                "value": {"record": [
                    ["number", {"source": "entry.number", "as": "u32"}],
                    ["saved", {"ref": "saved", "as": "address", "index": 0}]
                ]}}}})
        author["declarations"].insert(0, {
            "role": "saved", "type": "uint16_t", "const": False, "count": 2})
        result = self.construct(author)
        declaration = result.static_declarations[1]
        self.assertEqual(declaration.count, 2)
        self.assertIn(".number = 2u", declaration.initializer.render())
        self.assertIn(".number = 3u", declaration.initializer.render())
        self.assertIn("&s_nx_saved_test_port[0u]", declaration.initializer.render())
        author["declarations"][1]["count"]["limit"] = 1
        with self.assertRaises(ValueError):
            self.construct(author)

    def test_numeric_sources_are_fixed_tuple_indices_and_snapshots_are_typed(self):
        author = gpio_recipe()
        author["irq"][0]["name"]["source"] = "selected.vectors.0.handler"
        author["declarations"].insert(0, {
            "role": "saved", "type": "uint16_t", "const": False, "count": 2})
        author["pin_snapshots"] = {
            "source": "selected.pins", "key": "entry.identity",
            "array_role": "saved", "limit": 4}
        item = replace(Item(), integration=replace(Integration(), pins=(
            MappingProxyType({"identity": "PA2", "number": 2}),
            MappingProxyType({"identity": "PA3", "number": 3}))))
        result = self.construct(author, item)
        self.assertEqual(result.irq_connections[0].name, "UART1_IRQHandler")
        self.assertEqual(dict(result.pin_snapshots), {
            "PA2": "s_nx_saved_test_port[0u]", "PA3": "s_nx_saved_test_port[1u]"})
        for path in ("selected.vectors.1.handler", "selected.vectors.-1.handler",
                     "selected.vectors.item.options.mask", "selected.vectors[0].handler"):
            invalid = gpio_recipe()
            invalid["irq"][0]["name"]["source"] = path
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.construct(invalid)

    def test_fixed_array_references_must_fit_the_declared_storage(self):
        for count, index in ((2, 2), (None, 0)):
            author = gpio_recipe()
            saved = {"role": "saved", "type": "uint16_t", "const": False}
            if count is not None:
                saved["count"] = count
            author["declarations"].insert(0, saved)
            author["declarations"][1]["value"]["record"].append([
                "saved", {"ref": "saved", "as": "address", "index": index}])
            with self.subTest(count=count, index=index), self.assertRaises(ValueError):
                self.construct(author)

    def test_irq_mapping_expands_only_the_selected_vector_tuple(self):
        author = gpio_recipe()
        original, = author["irq"]
        original["name"]["source"] = "entry.handler"
        author["irq"] = [{"map": {"source": "selected.vectors", "limit": 4,
                                  "value": original}}]
        actual = self.construct(author)
        self.assertEqual([irq.name for irq in actual.irq_connections], ["UART1_IRQHandler"])
        author["irq"][0]["map"]["limit"] = 0
        with self.assertRaises(ValueError):
            self.construct(author)

    def test_typed_symbol_selection_and_pointer_members_generate_fixed_c_nodes(self):
        author = gpio_recipe()
        author["calls"]["prepare"]["function"] = {
            "source": "item.controller", "as": "symbol", "values": {"GPIOA": "gpio_prepare"}}
        author["declarations"][0]["value"]["record"].append([
            "input", {"pointer_member": {"base": {"source": "item.controller", "as": "symbol"},
                                         "member": "IDR"}}])
        actual = self.construct(author)
        self.assertEqual(actual.initialize.function, "gpio_prepare")
        self.assertIn(".input = &GPIOA->IDR", actual.static_declarations[0].render()[0])
        author["declarations"][0]["value"]["record"][-1][1]["pointer_member"]["member"] = "IDR; write()"
        with self.assertRaises(ValueError):
            self.construct(author)

    def test_optional_source_and_pure_constant_macro_keep_layout_in_c(self):
        author = gpio_recipe()
        fields = author["declarations"][0]["value"]["record"]
        fields.append(["alternate", {"source": "item.options.missing", "default": 4}])
        fields.append(["encoding", {"macro": "GPIO_ENCODING", "args": [
            {"source": "item.options.mask"}, {"symbol": "GPIO_SPEED"}]}])
        value = self.construct(author).static_declarations[0].initializer.render()
        self.assertIn(".alternate = 4u", value)
        self.assertIn(".encoding = GPIO_ENCODING(3u, GPIO_SPEED)", value)

    def test_static_values_owner_connections_and_prepare_sequences_are_finite(self):
        from tools.configure.static_ir import Flags

        @dataclass(frozen=True)
        class Assembly:
            controllers: tuple

        author = gpio_recipe()
        author["declarations"][0]["value"]["record"].extend([
            ["domain", {"address": "shared_domain"}],
            ["permissions", {"source": "selected.permissions", "as": "static"}],
            ["source", {"owner": {"source": "selected.source", "role": "port"}}]])
        prepare = author["calls"]["prepare"]
        author["calls"]["prepare"] = {"role": "gpio_start", "sequence": [prepare,
            {"function": "gpio_finish", "args": [{"ref": "port", "as": "address"}]}]}
        integration = MappingProxyType({"permissions": Flags((Symbol("READ"), Symbol("WRITE"))),
            "source": "trigger", "handler": "UART1_IRQHandler"})
        item = replace(Item(), integration=integration)
        recipe = StaticRecipe.from_validated(author)
        actual = recipe.construct(item, Assembly((item, replace(Item(), id="trigger"))), {}, {}, ())
        value = actual.static_declarations[0].initializer.render()
        self.assertIn(".domain = &shared_domain", value)
        self.assertIn(".permissions = READ | WRITE", value)
        self.assertIn(".source = &s_nx_port_trigger", value)
        sequence, = actual.initialize_sequences
        self.assertEqual([call.function for call in sequence.calls],
                         ["gpio_prepare", "gpio_finish"])
        self.assertEqual(actual.initialize.render(), "nx_gpio_start_test_port()")
        with self.assertRaises(ValueError):
            recipe.construct(item, Assembly((item,)), {}, {}, ())
        with self.assertRaises(ValueError):
            recipe.construct(replace(item, integration=MappingProxyType({
                **integration, "permissions": "READ | WRITE"})),
                Assembly((item, replace(Item(), id="trigger"))), {}, {}, ())

    def test_generated_recipe_is_real_c11_static_layout_and_irq(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "C11 recipe contract requires a host compiler")
        result = self.construct()
        lines = ["#include <stdbool.h>", "#include <stdint.h>",
                 "typedef int gpio_state_t;", "static int register_bank;",
                 "#define GPIOA (&register_bank)",
                 "typedef struct { int* registers; uint32_t mask, initial; "
                 "bool output; unsigned pull; } gpio_config_t;",
                 "static gpio_state_t s_nx_port_test_port;",
                 "static int gpio_admit(const gpio_config_t* c) { return c->mask != 0; }",
                 "static int gpio_prepare(gpio_state_t* s, const gpio_config_t* c) "
                 "{ *s = (int)c->initial; return 0; }",
                 "static void gpio_stop(gpio_state_t* s, unsigned initial) "
                 "{ *s = (int)initial; }",
                 "static void gpio_irq(gpio_state_t* s) { ++*s; }",
                 "static void nx_arch_dsb(void) {}"]
        for declaration in result.static_declarations:
            lines.extend(declaration.render())
        for irq in result.irq_connections:
            lines.extend(irq.render())
        lines += ["int main(void) {", "    " + result.initialize.render() + ";",
                  "    UART1_IRQHandler();", "    " + result.stop[0].render() + ";",
                  "    return !(" + result.admission[0].render() +
                  " && s_nx_port_test_port == 1);", "}"]
        with tempfile.TemporaryDirectory(prefix="nexus-static-recipe-") as directory:
            source, binary = Path(directory) / "recipe.c", Path(directory) / "recipe"
            source.write_text("\n".join(lines) + "\n", encoding="utf-8")
            completed = subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                str(source), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(subprocess.run([str(binary)], check=False).returncode, 0)

    def test_f407_gpio_data_preserves_resolved_configuration_and_lifecycle(self):
        from tools.configure import configure
        from tools.configure.data_provider import DataProvider
        from tools.configure.providers import for_selection
        source = (configure.ROOT /
            "tools/configure/assemblies/qiming-baremetal-gpio.toml").read_text(encoding="utf-8")
        for mode in ("input", "output"):
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "assembly.toml"
                path.write_text(source.replace('mode = "output"', f'mode = "{mode}"'),
                                encoding="utf-8")
                resolved = configure.resolve_ir(path)
            item, = resolved.controllers
            provider = for_selection(resolved)
            self.assertIsInstance(provider, DataProvider)
            selected = provider.constructor(item, resolved, resolved.board_bindings,
                                            resolved.routes, resolved.devices)
            self.assertEqual(selected.state_type, "nx_stm32_gpio_state_t")
            self.assertEqual(selected.static_declarations[0].initializer.render(),
                "{ .registers = GPIOE, .mask = 8u, .initial = 8u, .output = " +
                ("true" if mode == "output" else "false") +
                ", .pull = 0u, .output_type = 0u, .speed = NX_STM32_GPIO_SPEED_PRESERVE }")
            self.assertEqual(selected.initialize.render(),
                "nx_stm32_gpio_prepare(&s_nx_port_led0, &s_nx_gpio_config_led0)")
            self.assertEqual(selected.stop[0].render(),
                             "nx_stm32_gpio_stop(&s_nx_port_led0, 8u)")
            self.assertEqual(selected.definitions, ())

    def test_dac_constant_projection_stays_in_a_compiler_checked_private_header(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler)
        root = Path(__file__).resolve().parents[2]
        source = '''#include "stm32f4_dac_constants.h"
_Static_assert(NX_STM32_DAC_CHANNEL_MASK(1u, 0u) == 1u, "one");
_Static_assert(NX_STM32_DAC_CHANNEL_MASK(2u, 0u) == 2u, "two");
_Static_assert(NX_STM32_DAC_CHANNEL_MASK(1u, 2u) == 3u, "both");
_Static_assert(NX_STM32_DAC_IDLE_CODE(1u, 2u, 0u, 31u, 0u) == 0u, "absent");
_Static_assert(NX_STM32_DAC_IDLE_CODE(2u, 2u, 0u, 31u, 0u) == 31u, "single");
_Static_assert(NX_STM32_DAC_IDLE_CODE(1u, 2u, 1u, 31u, 47u) == 47u, "order");
int main(void) { return 0; }
'''
        with tempfile.TemporaryDirectory() as directory:
            path, binary = Path(directory) / "constants.c", Path(directory) / "constants"
            path.write_text(source, encoding="utf-8")
            completed = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-pedantic", "-I", str(root / "soc/stm32/stm32f4/private"),
                str(path), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_dac_recipes_cover_existing_modes_and_channel_order_without_algorithms(self):
        from tools.configure.data_provider import DataProvider
        from tools.configure.providers import for_selection
        from tools.configure.test_f407_dac_configuration import F407DacConfigurationTests
        fixture = F407DacConfigurationTests()
        modes = ("direct", "triggered", "noise", "triangle", "trigger-dma", "trigger-dma-circular")
        for mode in modes:
            for channels, idle in (("[1,2]", "[31,47]"), ("[2]", "[47]")):
                with self.subTest(mode=mode, channels=channels), tempfile.TemporaryDirectory() as directory:
                    extra = "amplitude=255\n" if mode in {"noise", "triangle"} else ""
                    if "dma" in mode:
                        extra += "irq.priority=5\nirq.calls_os=false\n"
                    resolved = fixture.resolve(Path(directory), mode=mode,
                        channels=channels, idle=idle, extra=extra,
                        trigger="tim6-trgo" if "dma" in mode else None)
                    item, = resolved.controllers
                    provider = for_selection(resolved)
                    self.assertIsInstance(provider, DataProvider)
                    actual = provider.constructor(item, resolved, resolved.board_bindings,
                                                  resolved.routes, resolved.devices)
                    self.assertEqual(actual.definitions, ())
                    self.assertEqual(actual.initialize.function,
                        "nx_stm32_dac_dma_prepare" if "dma" in mode else "nx_stm32_dac_prepare")
                    self.assertEqual(actual.stop[0].function,
                        "nx_stm32_dac_dma_close" if "dma" in mode else "nx_stm32_dac_close")
                    config = next(value for value in actual.static_declarations
                                  if value.type_name in {"nx_stm32_dac_config_t",
                                                         "nx_stm32_dac_dma_config_t"})
                    self.assertIn("NX_STM32_DAC_CHANNEL_MASK(", config.render()[0])
                    self.compile_dac_projection(actual, item)

    def compile_dac_projection(self, construction, item):
        """C11 evaluates actual recipe macros and layout, without a device SDK."""
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler)
        root = Path(__file__).resolve().parents[2]
        preamble = '''#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "stm32f4_dac_constants.h"
static int registers, s_nx_port_output;
#define DAC (&registers)
enum { NX_DAC_FORMAT_U8, NX_DAC_FORMAT_U12_RIGHT, NX_DAC_FORMAT_U12_LEFT };
enum { NX_STM32_DAC_DIRECT, NX_STM32_DAC_TRIGGERED, NX_STM32_DAC_NOISE, NX_STM32_DAC_TRIANGLE };
enum { NX_STM32_DAC_TRIGGER_NONE, NX_STM32_DAC_TRIGGER_SOFTWARE, NX_STM32_DAC_TRIGGER_TIM6,
       NX_STM32_DAC_TRIGGER_TIM7, NX_STM32_DAC_TRIGGER_EXTI9 };
enum { NX_DMA_MEMORY_READ = 1u, NX_DMA_MEMORY_WRITE = 2u };
typedef struct { uint32_t begin, size, permissions; } nx_dma_memory_region_t;
typedef struct { void* registers; unsigned channels, format, mode, trigger;
    uint16_t idle_codes[2]; bool output_buffer; uint16_t amplitude;
    const void* external; } nx_stm32_dac_config_t;
typedef struct { nx_stm32_dac_config_t dac; const nx_dma_memory_region_t* regions;
    unsigned region_count, priority; bool kernel_bound, circular;
    unsigned recovery_trigger; } nx_stm32_dac_dma_config_t;
typedef struct { const int* ops; void* context; } nx_dac_tx_port_t;
static const int nx_stm32_dac_dma_tx_ops = 0;
'''
        lines = preamble.splitlines()
        for declaration in construction.static_declarations:
            lines.extend(declaration.render())
        config = "s_nx_dac_config_output" + (".dac" if "dma" in item.mode else "")
        idle = dict(zip(item.options.channels, item.options.idle_codes))
        mask = sum(1 << (channel - 1) for channel in item.options.channels)
        lines += ["int main(void) {", "    (void)&nx_stm32_dac_dma_tx_ops;",
                  "    (void)&s_nx_port_output;"]
        lines += ["    (void)&" + declaration.name + ";"
                  for declaration in construction.static_declarations]
        lines += [f"    return !({config}.channels == {mask}u &&",
                  f"        {config}.idle_codes[0] == {idle.get(1, 0)}u &&",
                  f"        {config}.idle_codes[1] == {idle.get(2, 0)}u);", "}"]
        with tempfile.TemporaryDirectory() as directory:
            path, binary = Path(directory) / "dac.c", Path(directory) / "dac"
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            completed = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-I", str(root / "soc/stm32/stm32f4/private"),
                str(path), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(subprocess.run([str(binary)], check=False).returncode, 0)

    def test_timer_constant_encoders_are_c11_constant_expressions(self):
        from tools.configure import configure
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler)
        source = '''#include "stm32f4_timer_encoding.h"
_Static_assert(NX_STM32_TIMER_CHANNEL_MASK(1u, 0u, 0u, 0u) == 1u, "single");
_Static_assert(NX_STM32_TIMER_CHANNEL_MASK(1u, 2u, 3u, 4u) == 15u, "all");
_Static_assert(NX_STM32_TIMER_CHANNEL_MASK(4u, 1u, 0u, 0u) == 9u, "sparse");
_Static_assert(NX_STM32_TIMER_PRESCALER(84000000u, 1000000u) == 83u, "divider");
_Static_assert(NX_STM32_TIMER_PIN_MASK(15u) == 32768u, "pad");
_Static_assert(!NX_STM32_TIMER_IDLE_HIGH(0u), "low");
_Static_assert(NX_STM32_TIMER_IDLE_HIGH(32768u), "high");
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "encodings.c"
            path.write_text(source, encoding="utf-8")
            completed = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-pedantic", "-fsyntax-only", "-I",
                str(configure.ROOT / "soc/stm32/stm32f4/private"), str(path)],
                capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
