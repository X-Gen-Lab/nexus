"""Generate the actual reviewed constructors for deterministic register tests."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2])
sys.path.insert(0, str(root / "tools/configure"))
from configure import resolve  # noqa: E402
from providers.stm32f407.bindings import constructor, shared_irq_definitions  # noqa: E402

lines = []
for fixture in ("ve-exti.toml", "ve-pwm.toml", "ve-spi.toml", "ve-i2c.toml"):
    result = resolve(root / "tests/contracts/stm32_assembly" / fixture, root)
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
