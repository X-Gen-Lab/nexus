"""Observe actual linked Ethernet objects within the reviewed main SRAM domain.

This checks ARM allocation and initialized domain bytes. Host DMA-address mocks,
family-wide DMA flags, a PHY assumption or a source digest cannot establish this.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.measurement.elf import Elf32
from tools.measurement.retention import linked_object, object_words


def verify(elf, result):
    """Use resolved counts, concrete symbol sizes and actual LOAD permissions."""
    main = [item for item in result["memory"] if item["id"] == "sram"]
    selected = [item for item in result["controllers"] if item["kind"] == "eth"]
    if len(main) != 1 or not selected:
        raise EvidenceError("selected Ethernet and one reviewed main SRAM required")
    origin, size = main[0]["origin"], main[0]["size"]
    if type(origin) is not int or type(size) is not int or size <= 0:
        raise EvidenceError("invalid resolved main SRAM span")
    report = []
    for item in selected:
        name = item["id"].replace("-", "_")
        domain, section = linked_object(elf, "s_nx_eth_regions_" + name,
                                        "Ethernet domain")
        if object_words(elf, domain, section) != (origin, size, 3):
            raise EvidenceError("Ethernet domain must grant only reviewed main SRAM")
        tx, rx, capacity = (item[key] for key in
                            ("tx_count", "rx_count", "rx_storage_bytes"))
        if any(type(value) is not int or value <= 0 for value in (tx, rx, capacity)):
            raise EvidenceError("invalid selected Ethernet counts")
        spans = []
        objects = []
        for role, count, expected in (("tx", tx, tx * 16),
                                      ("rx", rx, rx * 16),
                                      ("tx_slots", tx, None),
                                      ("rx_slots", rx, None),
                                      ("storage", rx, rx * capacity)):
            symbol, _ = linked_object(elf, "s_nx_eth_" + role + "_" + name,
                                       "Ethernet " + role, writable=True)
            first, extent = symbol["value"], symbol["size"]
            if (first % 4 or not origin <= first < first + extent <= origin + size
                    or extent % count or (extent // count) % 4
                    or (expected is not None and extent != expected)):
                raise EvidenceError("Ethernet linked storage/count/domain mismatch: " + role)
            if any(first < end and begin < first + extent for begin, end in spans):
                raise EvidenceError("Ethernet linked storage overlaps")
            spans.append((first, first + extent))
            objects.append({"role": role, "address": first, "size": extent})
        report.append({"id": item["id"], "domain": {"origin": origin, "size": size},
                       "descriptor_bytes": (tx + rx) * 16,
                       "rx_storage_bytes": rx * capacity, "objects": objects})
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify(Elf32(args.elf.read_bytes()),
                        json.loads(args.resolved.read_text()))
        atomic_json(args.output, {"scope": "Actual ARM ELF storage; no physical PHY/HIL proof",
                                  "elf": file_identity(args.elf),
                                  "resolved": file_identity(args.resolved),
                                  "instances": result})
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
