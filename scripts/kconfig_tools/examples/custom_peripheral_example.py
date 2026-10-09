#!/usr/bin/env python3
"""Generate a TIMER schema draft from Python data or a JSON template."""
import argparse
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from kconfig_tools import KconfigGenerator, PeripheralTemplate, ParameterConfig, ChoiceConfig


def code_template(platform):
    return PeripheralTemplate(
        name="TIMER", platform=platform, max_instances=4, instance_type="numeric",
        parameters=[ParameterConfig("PRESCALER", "int", 1, (1, 65536)),
                    ParameterConfig("PERIOD", "int", 1000, (1, 4294967295)),
                    ParameterConfig("AUTO_RELOAD", "bool", True)],
        choices=[ChoiceConfig("CLOCK_SOURCE", ["INTERNAL", "EXTERNAL"], "INTERNAL",
                              values={"INTERNAL": 0, "EXTERNAL": 1})],
        help_text="Schema draft; product driver and board qualification are separate.")


def json_template(path, platform):
    data = json.loads(path.read_text(encoding="utf-8"))
    parameters = []
    for source in data.get("parameters", []):
        item = dict(source)
        if item.get("range") is not None:
            item["range"] = tuple(item["range"])
        parameters.append(ParameterConfig(**item))
    return PeripheralTemplate(
        name=data["name"], platform=platform, max_instances=data["max_instances"],
        instance_type=data["instance_type"], parameters=parameters,
        choices=[ChoiceConfig(**item) for item in data.get("choices", [])],
        help_text=data.get("help_text", ""))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    code = modes.add_parser("code")
    code.add_argument("platform")
    code.add_argument("output_dir", type=Path)
    external = modes.add_parser("json")
    external.add_argument("template", type=Path)
    external.add_argument("platform")
    external.add_argument("output_dir", type=Path)
    args = parser.parse_args(argv)
    platform = args.platform.upper()
    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", platform):
        parser.error("platform must be a Kconfig identifier")
    try:
        template = code_template(platform) if args.mode == "code" else json_template(args.template, platform)
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", template.name) or template.max_instances < 1:
            raise ValueError("template requires a symbol name and positive instance count")
        output = args.output_dir / template.name.lower() / "Kconfig"
        if output.exists():
            raise FileExistsError(f"refusing to overwrite {output}; use a fresh output directory")
        output.parent.mkdir(parents=True, exist_ok=True)
        KconfigGenerator(template).generate_file(str(output))
        print(output)
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
