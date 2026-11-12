"""Instrument only FIFO/digest leaves of exact pinned Host SDK inputs."""
from pathlib import Path
import hashlib
import json
import re
import sys

PINS = {
    "stm32f4xx_hal_cryp.c": "7f685430ec5e2df8198ec27f3251fa95b59f913bdacf6716deafffd19962afe7",
    "stm32f4xx_hal_hash.c": "bb6a64009355f7a0ebc5f4849fcf51d1df91c18f6282068e7bc2affe21376eb4",
}
COUNTS = {
    "stm32f4xx_hal_cryp.c": {"cryp-input": 105, "cryp-output": 9, "cryp-key": 54},
    "stm32f4xx_hal_hash.c": {"hash-input": 13, "hash-digest": 24},
}

def instrument(root, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    report = {"scope": "Host external MMIO only; no cryptographic algorithm model", "files": []}
    for name, expected in PINS.items():
        source = root / "vendors/st/stm32f4_hal/Src" / name
        original = source.read_bytes()
        digest = hashlib.sha256(original).hexdigest()
        if digest != expected:
            raise ValueError("pinned SDK source changed: " + name)
        text = original.decode("utf-8")
        if name.endswith("cryp.c"):
            rules = [
                ("cryp-key", r"(?m)^(\s*)hcryp->Instance->(K[0-3](?:LR|RR))\s*=\s*(.+?);",
                 lambda m: m[1] + "nx_crypto_model_cryp_key(&hcryp->Instance->" + m[2] + ", (uint32_t)(" + m[3] + "));"),
                ("cryp-input", r"(?m)^(\s*)hcryp->Instance->DIN\s*=\s*(.+?);",
                 lambda m: m[1] + "nx_crypto_model_cryp_input(hcryp->Instance, (uint32_t)(" + m[2] + "));"),
                ("cryp-output", r"(?m)^(\s*(?:temp\[i\]|intermediate_data\[index\])\s*=\s*)hcryp->Instance->DOUT;",
                 lambda m: m[1] + "nx_crypto_model_cryp_output(hcryp->Instance);"),
            ]
        else:
            rules = [
                ("hash-input", r"(?m)^(\s*)HASH->DIN\s*=\s*(.+?);",
                 lambda m: m[1] + "nx_crypto_model_hash_input((uint32_t)(" + m[2] + "));"),
                ("hash-digest", r"\b(?:HASH|HASH_DIGEST)->HR\[([0-9]+)\]",
                 lambda m: "nx_crypto_model_hash_digest(" + m[1] + "U)"),
            ]
        mapping = []
        for kind, pattern, replacement in rules:
            found = list(re.finditer(pattern, text))
            if len(found) != COUNTS[name][kind]:
                raise ValueError("unexpected MMIO match count: " + name + ":" + kind)
            for match in found:
                mapping.append({"kind": kind, "line_before_rule": text[:match.start()].count("\n") + 1,
                                "original_leaf": match[0], "replacement_leaf": replacement(match)})
            text = re.sub(pattern, replacement, text)
        # No branch, loop or RAM input expression is rewritten. In particular,
        # SDK partial-word RAM overreads stay intact for real gap RED probes.
        generated = text.encode("utf-8")
        destination = output / name
        if not destination.exists() or destination.read_bytes() != generated:
            destination.write_bytes(generated)
        report["files"].append({"original_path": str(source), "original_sha256": digest,
                                "generated_path": str(output / name),
                                "generated_sha256": hashlib.sha256(generated).hexdigest(),
                                "mapping": mapping})
    (output / "instrumentation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report

if __name__ == "__main__":
    instrument(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).absolute())
