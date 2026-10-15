#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Delegate formatting to format.py and the repository's root .clang-format.
.DESCRIPTION
    Keeps -Check, -VerboseOutput/-Verbose, -ConfigFile and -ShowConfig.
    Use -Files for explicit owned files and -Tool for a clang-format executable.
    Directory/style/parallel overrides are replaced by the shared Python policy.
#>
[CmdletBinding()]
param(
    [switch]$Check,
    [switch]$VerboseOutput,
    [Alias("Config")][string]$ConfigFile = "",
    [switch]$ShowConfig,
    [string[]]$Files = @(),
    [string]$Tool = "",
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

$ErrorActionPreference = "Stop"
$formatArguments = @()
if ($Check) { $formatArguments += "--check" }
if ($VerboseOutput -or $PSBoundParameters["Verbose"]) { $formatArguments += "--verbose" }
if ($ConfigFile) { $formatArguments += @("--config", $ConfigFile) }
if ($ShowConfig) { $formatArguments += "--show-config" }
if ($Help) { $formatArguments += "--help" }
if ($Tool) { $formatArguments += @("--tool", $Tool) }
if ($Files.Count) { $formatArguments += "--files"; $formatArguments += $Files }
$formatArguments += $Arguments

try {
    $python = Get-Command python3, python -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $launcherArguments = @()
    if (-not $python) {
        $python = Get-Command py -CommandType Application -ErrorAction SilentlyContinue
        $launcherArguments = @("-3")
    }
    if (-not $python) { throw "Python 3 was not found." }
    & $python.Source @launcherArguments (Join-Path $PSScriptRoot "format.py") @formatArguments
    exit $LASTEXITCODE
}
catch {
    [Console]::Error.WriteLine("Cannot execute formatter: $_")
    exit 2
}
