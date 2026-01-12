#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Project Manager - PowerShell Version

.DESCRIPTION
    Unified entry point for all Nexus project operations.
    Provides easy access to build, test, format, clean, docs, and CI operations.

.PARAMETER Command
    Command to execute: setup, build, test, format, clean, docs, ci, help

.PARAMETER Arguments
    Additional arguments to pass to the specific command

.PARAMETER List
    List all available commands and their descriptions

.PARAMETER Version
    Show version information

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\nexus.ps1 setup
    Run environment setup

.EXAMPLE
    .\nexus.ps1 build -Type Release -Platform stm32f4
    Build for STM32F4 platform in Release mode

.EXAMPLE
    .\nexus.ps1 test -Filter "*Math*" -Verbose
    Run math-related tests with verbose output

.EXAMPLE
    .\nexus.ps1 ci -Stage all -Coverage
    Run full CI pipeline with coverage
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet("setup", "build", "test", "format", "clean", "docs", "ci", "help")]
    [string]$Command = "help",

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @(),

    [switch]$List,
    [switch]$Version,
    [switch]$Help
)

# Project information
$script:ProjectInfo = @{
    Name = "Nexus Embedded Platform"
    Version = "1.0.0"
    Description = "Cross-platform embedded development framework"
    Repository = "https://github.com/nexus-platform/nexus"
    PowerShellVersion = "1.0.0"
}

# Helper functions
function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host $Text.PadLeft(($Text.Length + 60) / 2).PadRight(60) -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Text)
    Write-Host "√ $Text" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Text)
    Write-Host "! $Text" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Text)
    Write-Host "X $Text" -ForegroundColor Red
}

function Get-ProjectRoot {
    return (Get-Item $PSScriptRoot).Parent.FullName
}

function Show-Version {
    Write-Header "Nexus Project Manager"

    Write-Host "Project Information:" -ForegroundColor Cyan
    Write-Host "  Name: $($script:ProjectInfo.Name)"
    Write-Host "  Version: $($script:ProjectInfo.Version)"
    Write-Host "  Description: $($script:ProjectInfo.Description)"
    Write-Host "  Repository: $($script:ProjectInfo.Repository)"
    Write-Host ""
    Write-Host "PowerShell Scripts Version: $($script:ProjectInfo.PowerShellVersion)"
    Write-Host "PowerShell Version: $($PSVersionTable.PSVersion)"
    Write-Host "Platform: $([System.Runtime.InteropServices.RuntimeInformation]::OSDescription)"
}

function Show-Help {
    Write-Header "Nexus Project Manager - Help"

    Write-Host @"
Usage: .\nexus.ps1 <command> [arguments...]

Commands:
  setup     - Environment setup and configuration
  build     - Build the project
  test      - Run tests
  format    - Format source code
  clean     - Clean build artifacts
  docs      - Generate documentation
  ci        - Run CI pipeline
  help      - Show this help information

Options:
  -List     - List all available commands
  -Version  - Show version information
  -Help     - Show this help information

Examples:
  .\nexus.ps1 setup                           # Setup development environment
  .\nexus.ps1 build -Type Release             # Build in Release mode
  .\nexus.ps1 test -Verbose                   # Run tests with verbose output
  .\nexus.ps1 format -Check                   # Check code formatting
  .\nexus.ps1 clean -All                      # Clean all artifacts
  .\nexus.ps1 docs -Target doxygen -Open      # Generate and open API docs
  .\nexus.ps1 ci -Stage all -Coverage         # Run full CI with coverage

For detailed help on a specific command:
  .\nexus.ps1 <command> -Help

For more information, visit: $($script:ProjectInfo.Repository)
"@
}

function Show-CommandList {
    Write-Header "Available Commands"

    $commands = @(
        @{ Name = "setup"; Description = "Environment setup and configuration"; Script = "scripts/setup/setup.ps1" }
        @{ Name = "build"; Description = "Build the project"; Script = "scripts/building/build.ps1" }
        @{ Name = "test"; Description = "Run tests"; Script = "scripts/test/test.ps1" }
        @{ Name = "format"; Description = "Format source code"; Script = "scripts/tools/format.ps1" }
        @{ Name = "clean"; Description = "Clean build artifacts"; Script = "scripts/tools/clean.ps1" }
        @{ Name = "docs"; Description = "Generate documentation"; Script = "scripts/tools/docs.ps1" }
        @{ Name = "ci"; Description = "Run CI pipeline"; Script = "scripts/ci/ci_build.ps1" }
    )

    Write-Host "Command".PadRight(12) + "Description".PadRight(40) + "Script Location" -ForegroundColor Cyan
    Write-Host ("-" * 80) -ForegroundColor Gray

    foreach ($cmd in $commands) {
        $projectRoot = Get-ProjectRoot
        $scriptPath = Join-Path $projectRoot $cmd.Script
        $exists = Test-Path $scriptPath
        $status = if ($exists) { "√" } else { "X" }
        $statusColor = if ($exists) { "Green" } else { "Red" }

        Write-Host $cmd.Name.PadRight(12) -NoNewline -ForegroundColor White
        Write-Host $cmd.Description.PadRight(40) -NoNewline -ForegroundColor Gray
        Write-Host $status -NoNewline -ForegroundColor $statusColor
        Write-Host " $($cmd.Script)" -ForegroundColor Gray
    }

    Write-Host ""
    Write-Host "Usage: .\nexus.ps1 <command> [arguments...]" -ForegroundColor Cyan
    Write-Host "For command-specific help: .\nexus.ps1 <command> -Help" -ForegroundColor Cyan
}

function Invoke-Command {
    param(
        [string]$CommandName,
        [string[]]$CommandArguments
    )

    $projectRoot = Get-ProjectRoot

    # Map commands to script paths
    $scriptMap = @{
        "setup" = "scripts/setup/setup.ps1"
        "build" = "scripts/building/build.ps1"
        "test" = "scripts/test/test.ps1"
        "format" = "scripts/tools/format.ps1"
        "clean" = "scripts/tools/clean.ps1"
        "docs" = "scripts/tools/docs.ps1"
        "ci" = "scripts/ci/ci_build.ps1"
    }

    if (!$scriptMap.ContainsKey($CommandName)) {
        Write-Error "Unknown command: $CommandName"
        Write-Host "Use '.\nexus.ps1 -List' to see available commands" -ForegroundColor Yellow
        return 1
    }

    $scriptPath = Join-Path $projectRoot $scriptMap[$CommandName]

    if (!(Test-Path $scriptPath)) {
        Write-Error "Script not found: $scriptPath"
        Write-Host "Please ensure all PowerShell scripts are properly installed" -ForegroundColor Yellow
        return 1
    }

    Write-Host "Executing: $CommandName" -ForegroundColor Blue
    Write-Host "Script: $scriptPath" -ForegroundColor Gray
    if ($CommandArguments.Count -gt 0) {
        Write-Host "Arguments: $($CommandArguments -join ' ')" -ForegroundColor Gray
    }
    Write-Host ""

    try {
        # Execute the script with arguments
        $result = & $scriptPath @CommandArguments
        return $LASTEXITCODE
    }
    catch {
        Write-Error "Failed to execute command '$CommandName': $_"
        return 1
    }
}

function Test-PowerShellVersion {
    $requiredVersion = [Version]"5.1"
    $currentVersion = $PSVersionTable.PSVersion

    if ($currentVersion -lt $requiredVersion) {
        Write-Warning "PowerShell version $currentVersion detected"
        Write-Warning "PowerShell $requiredVersion or higher is recommended"
        Write-Host "Consider upgrading to PowerShell 7+ for best compatibility" -ForegroundColor Yellow
        return $false
    }

    return $true
}

function Test-ProjectStructure {
    $projectRoot = Get-ProjectRoot

    # Check for essential directories
    $requiredDirs = @("scripts", "hal", "osal", "platforms")
    $missingDirs = @()

    foreach ($dir in $requiredDirs) {
        $dirPath = Join-Path $projectRoot $dir
        if (!(Test-Path $dirPath)) {
            $missingDirs += $dir
        }
    }

    if ($missingDirs.Count -gt 0) {
        Write-Warning "Missing project directories: $($missingDirs -join ', ')"
        Write-Host "Please ensure you're running this script from the project root" -ForegroundColor Yellow
        return $false
    }

    # Check for PowerShell scripts
    $scriptMap = @{
        "setup" = "scripts/setup/setup.ps1"
        "build" = "scripts/building/build.ps1"
        "test" = "scripts/test/test.ps1"
        "format" = "scripts/tools/format.ps1"
        "clean" = "scripts/tools/clean.ps1"
        "docs" = "scripts/tools/docs.ps1"
        "ci" = "scripts/ci/ci_build.ps1"
    }

    $missingScripts = @()
    foreach ($scriptName in $scriptMap.Keys) {
        $scriptPath = Join-Path $projectRoot $scriptMap[$scriptName]
        if (!(Test-Path $scriptPath)) {
            $missingScripts += $scriptName
        }
    }

    if ($missingScripts.Count -gt 0) {
        Write-Warning "Missing PowerShell scripts: $($missingScripts -join ', ')"
        Write-Host "Some commands may not be available" -ForegroundColor Yellow
        return $false
    }

    return $true
}

# Main script
function Main {
    # Handle special flags first
    if ($Version) {
        Show-Version
        return 0
    }

    if ($List) {
        Show-CommandList
        return 0
    }

    if ($Help -or $Command -eq "help") {
        Show-Help
        return 0
    }

    # Validate environment
    Test-PowerShellVersion | Out-Null

    if (!(Test-ProjectStructure)) {
        Write-Error "Project structure validation failed"
        return 1
    }

    # Execute command
    return Invoke-Command -CommandName $Command -CommandArguments $Arguments
}

# Execute main function with error handling
try {
    exit (Main)
}
catch {
    Write-Error "An error occurred: $_"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}
