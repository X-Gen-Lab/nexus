#!/usr/bin/env sh
# Run the maintained preset workflow from any working directory.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$SCRIPT_DIR/ci/ci_build.py" --stage build "$@"
