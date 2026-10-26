"""Generate the actual reviewed constructors for deterministic register tests."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2])
sys.path.insert(0, str(root))
from tools.configure.configure import resolve_ir  # noqa: E402
from soc.stm32.stm32f407.configure.bindings import (  # noqa: E402
    constructor, shared_irq_definitions,
)

lines = ["#ifndef NX_PLATFORM_REGISTER_WRITE",
         "#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (value))",
         "#endif"]
for fixture in ("ve-exti.toml", "ve-pwm.toml", "ve-spi.toml", "ve-i2c.toml"):
    result = resolve_ir(root / "tests/contracts/stm32_assembly" / fixture, root)
    for item in result["controllers"]:
        name = item["id"].replace("-", "_")
        lines.append(f"static nx_stm32_{item['kind']}_state_t s_nx_port_{name};")
        if item["kind"] in {"spi", "i2c"}:
            lines.append(f"static nx_stm32_{item['kind']}_endpoint_state_t s_nx_endpoint_{name};")
        if item["kind"] in {"pwm", "exti"}:
            kind = item["kind"]
            lines.append(f"static const nx_{kind}_port_t s_nx_face_{name} = {{&nx_stm32_{kind}_ops, &s_nx_port_{name}}};")
        generated = constructor(item, result, result["board_bindings"],
                                result["routes"], result["devices"])
        lines += generated["definitions"]
    lines += shared_irq_definitions(result["controllers"])
output.write_text("\n".join(lines) + "\n", encoding="utf-8")
