"""Expose only exact locked USB MMIO leaves to an external Native model."""
from pathlib import Path
import hashlib
import json
import re
import sys

PINS = {
    "stm32f4xx_hal_hcd.c": "f9dcf87244d2d5127d42afe2debb8098df56559fd51e72b26e65c6ee4e16218e",
    "stm32f4xx_ll_usb.c": "a4a3827fba7ad9d0df9457a87e1bf31ede88eeeb9dc750b3fff639f59c8dff5c",
}
COUNTS = {
    "stm32f4xx_hal_hcd.c": {"register-read": 8, "register-write": 8, "rx-status-pop": 1},
    "stm32f4xx_ll_usb.c": {"register-read": 19, "register-write": 27,
                           "fifo-write": 1, "fifo-read": 2},
}
REGISTER = (r"(?:USBx->(?:GRSTCTL|GINTSTS)|USBx_HOST->HAINT|"
            r"USBx_HC\((?:\(uint32_t\))?\w+\)->(?:HCINT|HCCHAR))\b")
WRITE = re.compile(r"(?m)^([ \t]*)(" + REGISTER + r")\s*([|&]?=)\s*([\s\S]*?);")


def instrument(root, output):
    output.mkdir(parents=True, exist_ok=True)
    report = {"scope": "External Host MMIO only; no control-flow, timeout, return, RAM or transfer algorithm rewrite", "files": []}
    for name, expected in PINS.items():
        source = root / "vendors/st/stm32f4_hal/Src" / name
        original = source.read_bytes()
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError("Locked USB source identity changed: " + name)
        text = original.decode("utf-8")
        mapping = []
        reads = []
        for match in re.finditer(REGISTER, text):
            # Match complete field names only; never rewrite an lvalue or an
            # address operand as a read (in particular, never HCINTMSK).
            if (re.match(r"\s*(?:[|&]?=)", text[match.end():]) or
                    text[:match.start()].rstrip().endswith("&")):
                continue
            reads.append(match)
        if len(reads) != COUNTS[name]["register-read"]:
            raise ValueError("USB register read match count changed: " + name)
        for match in reversed(reads):
            replacement = "nx_usb_transport_read(&(" + match[0] + "))"
            mapping.append({"kind": "register-read", "line": text[:match.start()].count("\n") + 1,
                            "old": match[0], "new": replacement})
            text = text[:match.start()] + replacement + text[match.end():]
        writes = list(WRITE.finditer(text))
        if len(writes) != COUNTS[name]["register-write"]:
            raise ValueError("USB register write match count changed: " + name)
        for match in reversed(writes):
            value = "(uint32_t)(" + match[4] + ")"
            if match[3] != "=":
                value = "nx_usb_transport_read(&(" + match[2] + ")) " + match[3][0] + " " + value
            replacement = match[1] + "nx_usb_transport_write(&(" + match[2] + "), " + value + ");"
            mapping.append({"kind": "register-write", "line": text[:match.start()].count("\n") + 1,
                            "old": match[0], "new": replacement})
            text = text[:match.start()] + replacement + text[match.end():]
        if name.endswith("ll_usb.c"):
            leaves = [
                ("fifo-write", "USBx_DFIFO((uint32_t)ch_ep_num) = __UNALIGNED_UINT32_READ(pSrc);",
                 "nx_usb_transport_write(&(USBx_DFIFO((uint32_t)ch_ep_num)), __UNALIGNED_UINT32_READ(pSrc));"),
                ("fifo-read", "USBx_DFIFO(0U)", "nx_usb_transport_read(&(USBx_DFIFO(0U)))"),
            ]
        else:
            leaves = [("rx-status-pop", "hhcd->Instance->GRXSTSP", "nx_usb_transport_pop_rx(hhcd->Instance)")]
        for kind, old, new in leaves:
            if text.count(old) != COUNTS[name][kind]:
                raise ValueError("USB FIFO match count changed: " + name + ":" + kind)
            for match in re.finditer(re.escape(old), text):
                mapping.append({"kind": kind, "line": text[:match.start()].count("\n") + 1,
                                "old": old, "new": new})
            text = text.replace(old, new)
        control = r"\b(?:if|else|for|while|do|switch|case|default|return|break|continue|goto)\b"
        if re.findall(control, original.decode("utf-8")) != re.findall(control, text):
            raise ValueError("USB control tokens changed")
        generated = text.encode("utf-8")
        (output / name).write_bytes(generated)
        (output / (name + ".original")).write_bytes(original)
        report["files"].append({"original_path": str(source), "original_sha256": expected,
                                "generated_path": str(output / name),
                                "generated_sha256": hashlib.sha256(generated).hexdigest(),
                                "counts": COUNTS[name], "mapping": mapping})
    (output / "instrumentation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    instrument(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).absolute())
