"""Generate the actual reviewed constructors for deterministic register tests."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2])
sys.path.insert(0, str(root / "tools/configure"))
from configure import resolve  # noqa: E402
from stm32_bindings import constructor, shared_irq_definitions  # noqa: E402

lines = []
for fixture in ("ve-exti.json", "ve-pwm.json", "ve-spi.json"):
    result = resolve(root / "tests/contracts/stm32_assembly" / fixture, root)
    for item in result["controllers"]:
        name = item["id"].replace("-", "_")
        lines.append(f"static nx_{item['kind']}_port_t s_{name};")
        if item["kind"] == "spi":
            lines.append(f"static nx_spi_endpoint_t s_{name}_endpoint;")
        generated = constructor(item, result, result["board_bindings"],
                                result["routes"], result["devices"])
        lines += generated["definitions"]
    lines += shared_irq_definitions(result["controllers"])
output.write_text("\n".join(lines) + "\n", encoding="utf-8")
