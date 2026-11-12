"""Instrument five byte-wide QSPI FIFO leaves of the exact locked SDK."""
from pathlib import Path
import hashlib
import json
import sys

PIN = "22264aa1dee8525b5881c243f45b08feeb65255672623c5df19bc7a93bb1241e"
RULES = (
    ("fifo-write8", "*((__IO uint8_t *)data_reg) = *hqspi->pTxBuffPtr;",
     "nx_qspi_model_write8(data_reg, *hqspi->pTxBuffPtr);", 2),
    ("fifo-read8", "*hqspi->pRxBuffPtr = *((__IO uint8_t *)data_reg);",
     "*hqspi->pRxBuffPtr = nx_qspi_model_read8(data_reg);", 3),
)


def instrument(root, output):
    """Keep every branch, loop, count and caller-pointer expression intact."""
    source = Path(root) / "vendors/st/stm32f4_hal/Src/stm32f4xx_hal_qspi.c"
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != PIN:
        raise ValueError("pinned QSPI SDK source changed")
    text = original.decode("utf-8")
    mapping = []
    for kind, before, after, count in RULES:
        if text.count(before) != count:
            raise ValueError("unexpected QSPI MMIO match count: " + kind)
        start = 0
        while (position := text.find(before, start)) >= 0:
            mapping.append({"kind": kind,
                            "line_before_rule": text[:position].count("\n") + 1,
                            "original_leaf": before, "replacement_leaf": after})
            start = position + len(before)
        text = text.replace(before, after)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    destination = output / source.name
    generated = text.encode("utf-8")
    if not destination.exists() or destination.read_bytes() != generated:
        destination.write_bytes(generated)
    report = {"scope": "Host byte FIFO MMIO only; official QSPI transport unchanged",
              "original_path": str(source), "original_sha256": PIN,
              "generated_path": str(destination),
              "generated_sha256": hashlib.sha256(generated).hexdigest(),
              "mapping": mapping}
    report_path = output / "instrumentation.json"
    contents = json.dumps(report, indent=2) + "\n"
    if not report_path.exists() or report_path.read_text() != contents:
        report_path.write_text(contents)
    return report


if __name__ == "__main__":
    instrument(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).absolute())
