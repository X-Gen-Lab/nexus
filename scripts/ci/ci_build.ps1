#!/usr/bin/env pwsh
# One implementation owns preset resolution, exit codes and CTest selection.
[CmdletBinding()]
param(
    [string]$Preset,
    [ValidateSet("configure", "build", "test", "lint", "docs", "all")]
    [string]$Stage = "all",
    [ValidateRange(1, 256)]
    [int]$Jobs = 4
)
$ErrorActionPreference = "Stop"
if (-not $Preset) {
    Write-Error "Select an explicit -Preset from CMakePresets.json"
    exit 2
}
$runner = Join-Path $PSScriptRoot "ci_build.py"
python $runner --preset $Preset --stage $Stage --jobs $Jobs
exit $LASTEXITCODE
