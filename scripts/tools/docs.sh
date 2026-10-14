#!/bin/sh
# Legacy forwarding entry. Configuration and generation belong to the docs CLI.
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "${NEXUS_PYTHON:-python3}" "$script_dir/docs.py" "$@"
