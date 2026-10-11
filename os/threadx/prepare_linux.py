"""Derive the reviewed monotonic Linux port without modifying upstream files.

The selecting CMake target must compile/link a sem_clockwait capability probe.
This source derivation never claims runtime privilege, latency or clock-step HIL.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path

SOURCE_SHA256 = "760b2e8e341b7fcdd2b5984eb9724100c562ccd27b79a18673f9fe45dd93b1da"
PATCHES = (
    ("clock_gettime(CLOCK_REALTIME, &ts);",
     "clock_gettime(CLOCK_MONOTONIC, &ts);"),
    ("sem_timedwait(&_tx_linux_timer_semaphore, &ts)",
     "sem_clockwait(&_tx_linux_timer_semaphore, CLOCK_MONOTONIC, &ts)"),
    ("if (ts.tv_nsec > 1000000000)", "if (ts.tv_nsec >= 1000000000)"),
)
PROVENANCE = (
    "/* Nexus build-derived Linux timer: reviewed upstream source identity;\n"
    " * MONOTONIC clock/wait and normalized nanosecond carry.\n"
    " * Original ThreadX copyright and MIT license remain below. */\n"
)


def regular(path, *, output=False):
    path = Path(path).absolute()
    if ".." in path.parts or any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError("Canonical regular paths without symlinks required")
    if output and not path.exists():
        if not path.parent.is_dir():
            raise ValueError("Derived output parent must already exist")
    elif not path.is_file():
        raise ValueError("Regular input/output file required")
    return path


def replace_once(text, before, after):
    if text.count(before) != 1 or after in text:
        raise ValueError("Unreviewed Linux timer patch boundary")
    return text.replace(before, after)


def derive(raw):
    if hashlib.sha256(raw).hexdigest() != SOURCE_SHA256:
        raise ValueError("Unreviewed Linux ThreadX source identity")
    text = raw.decode("utf-8")
    for before, after in PATCHES:
        text = replace_once(text, before, after)
    if any(before in text or text.count(after) != 1 for before, after in PATCHES):
        raise ValueError("Linux timer derivation is incomplete")
    return (PROVENANCE + text).encode("utf-8")


def prepare(source, output):
    source, output = regular(source), regular(output, output=True)
    before = source.stat()
    raw = source.read_bytes()
    derived = derive(raw)
    after = regular(source).stat()
    if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino) or source.read_bytes() != raw:
        raise ValueError("Reviewed source changed during derivation")
    if output.exists():
        if regular(output).read_bytes() != derived:
            raise ValueError("Reserved derived output has different contents")
        return
    with output.open("xb") as stream:
        stream.write(derived)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        prepare(args.source, args.output)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, str(error) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
