#!/usr/bin/env pwsh
# Accept exactly the maintained Python runner arguments; preserve its exit code.
$ErrorActionPreference = "Stop"
$runner = Join-Path $PSScriptRoot "../ci/ci_build.py"
& python $runner @args
exit $LASTEXITCODE
