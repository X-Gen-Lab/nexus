"""Generate the actual reviewed constructors for deterministic register tests."""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2])
sys.path.insert(0, str(root))
from tools.configure.configure import resolve_ir  # noqa: E402
from tools.configure.dependencies import source_names  # noqa: E402
from tools.configure.providers import for_selection  # noqa: E402
from tools.configure.static_ir import Declaration, IrqConnection  # noqa: E402

inputs = {str(root / name) for name in source_names("platform", None)}
inputs.add(str(Path(__file__).resolve()))
lines = ["#ifndef NX_PLATFORM_REGISTER_WRITE",
         "#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (value))",
         "#endif"]
for fixture in ("ve-exti.toml", "ve-pwm.toml", "ve-spi.toml", "ve-i2c.toml"):
    selected_inputs = {}
    result = resolve_ir(root / "tests/contracts/stm32_assembly" / fixture, root,
                        input_paths=selected_inputs)
    inputs.update(selected_inputs.values())
    provider = for_selection(result)
    # These constructor fixtures select no ADC. Retain the production empty
    # projection required by the shared cold-type header, not a private pad map.
    lines += [line for line in provider.configuration_header(result)
              if line.startswith("#define NEXUS_STM32_ADC_PINS(")]
    # Include exactly the private types selected by the production emitter;
    # runtime hooks belong to the firmware and are not part of this harness.
    lines += [line for line in provider.includes(result)
              if line.startswith('#include ')]
    for item in result.controllers:
        generated = provider.constructor(item, result, result.board_bindings,
                                         result.routes, result.devices)
        name = item.id.replace("-", "_")
        initial = (" = " + generated.initial_state.render()
                   if generated.initial_state is not None else "")
        lines.append(f"static {generated.state_type} s_nx_port_{name}{initial};")
        if item.kind in {"pwm", "exti"}:
            kind = item.kind
            lines.append(f"static const nx_{kind}_port_t s_nx_face_{name} = {{&{provider.operations(item)}, &s_nx_port_{name}}};")
        for declaration in generated.static_declarations:
            lines.extend(declaration.render())
        for connection in generated.irq_connections:
            lines.append(f"void {connection.name}(void);")
            lines.extend(connection.render())
        for sequence in generated.initialize_sequences:
            lines.extend(sequence.render())
        lines += generated.definitions
        # This test-only facade follows the actual typed call slots, including
        # ordered multi-call cleanup. It is not a production compatibility API.
        lines += [f"static nx_result_t s_nx_test_cleanup_{name};",
                  f"static nx_result_t nx_test_prepare_{name}(void) {{"]
        for index, call in enumerate(generated.admission):
            lines += [f"    nx_result_t admitted_{index} = {call.render()};",
                      f"    if (admitted_{index} != NX_SUCCESS) {{ return admitted_{index}; }}"]
        lines += [f"    s_nx_test_cleanup_{name} = NX_SUCCESS;",
                  f"    nx_result_t primary = {generated.initialize.render()};"]
        if generated.retain_on_failed_init and generated.stop:
            # Production Platform marks this constructor before prepare, then
            # runs typed stop slots on failure. Preserve primary and the first
            # cleanup error separately; never continue releasing dependencies.
            lines.append("    if (primary != NX_SUCCESS) {")
            for call in generated.stop:
                lines += [f"        s_nx_test_cleanup_{name} = {call.render()};",
                          f"        if (s_nx_test_cleanup_{name} != NX_SUCCESS) {{ return primary; }}"]
            lines.append("    }")
        lines += ["    return primary;", "}",
                  f"static nx_result_t nx_test_stop_{name}(void) {{"]
        if generated.stop:
            lines.append("    nx_result_t status;")
            for call in generated.stop:
                lines += [f"    status = {call.render()};",
                          "    if (status != NX_SUCCESS) { return status; }"]
        lines += ["    return NX_SUCCESS;", "}"]
    for connection in provider.shared_irqs(result):
        if isinstance(connection, IrqConnection):
            lines.append(f"void {connection.name}(void);")
        if not isinstance(connection, (Declaration, IrqConnection)):
            raise TypeError("Register harness requires production typed assembly")
        lines.extend(connection.render())
output.write_text("\n".join(lines) + "\n", encoding="utf-8")
output.with_suffix(".inputs.json").write_text(
    json.dumps(sorted(inputs), indent=2) + "\n", encoding="utf-8")
