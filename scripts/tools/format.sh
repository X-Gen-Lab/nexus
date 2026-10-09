#!/usr/bin/env bash
# All options, including the legacy "check" argument, are handled by Python.
set -euo pipefail
FORMAT_SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$FORMAT_SCRIPT_DIR/format.py" "$@"
