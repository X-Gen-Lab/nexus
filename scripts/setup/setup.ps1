#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "setup.py") @args
exit $LASTEXITCODE
