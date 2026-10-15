# Legacy forwarding entry. Generation and validation use the maintained CLI.
[CmdletBinding()]
param(
    [ValidateSet('all', 'doxygen', 'sphinx')][string]$Target = 'all',
    [ValidateSet('en', 'zh_CN', 'all')][string]$Language = 'all',
    [string]$Output,
    [switch]$Help
)
$ErrorActionPreference = 'Stop'
$python = if ($env:NEXUS_PYTHON) { $env:NEXUS_PYTHON } else { 'python' }
$arguments = @((Join-Path $PSScriptRoot 'docs.py'))
if ($Help) { $arguments += '--help' }
else {
    $arguments += @('--target', $Target, '--language', $Language)
    if ($Output) { $arguments += @('--output', $Output) }
}
& $python @arguments
exit $LASTEXITCODE
