#!/usr/bin/env python3
"""Generate peripheral schema drafts into an explicitly selected directory."""
import argparse
from copy import deepcopy
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from kconfig_tools import KconfigGenerator, templates


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--peripheral", choices=templates.list_templates())
    parser.add_argument("--instances", type=int)
    args = parser.parse_args(argv)
    platform = args.platform.upper()
    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", platform):
        parser.error("platform must be a Kconfig identifier")
    if args.instances is not None and (not args.peripheral or args.instances < 1):
        parser.error("--instances requires one --peripheral and a positive count")
    names = [args.peripheral] if args.peripheral else templates.list_templates()
    try:
        for name in names:
            template = deepcopy(templates.get_template(name))
            template.platform = platform
            if args.instances is not None:
                if args.instances > template.max_instances:
                    parser.error(f"{name} permits at most {template.max_instances} instances")
                template.max_instances = args.instances
            output = args.output_dir / name.lower() / "Kconfig"
            if output.exists():
                raise FileExistsError(f"refusing to overwrite {output}; use a fresh output directory")
            output.parent.mkdir(parents=True, exist_ok=True)
            KconfigGenerator(template).generate_file(str(output))
            print(output)
    except (OSError, ValueError, TypeError) as error:
        print(f"generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
