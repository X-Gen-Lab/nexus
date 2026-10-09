#!/usr/bin/env pwsh
# Dispatch only; Python owns command help and the shared preset workflow.
$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "nexus.py") @args
exit $LASTEXITCODE
