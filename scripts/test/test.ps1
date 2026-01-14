#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Test Script - PowerShell Version

.DESCRIPTION
    Cross-platform test runner for Nexus embedded development platform.
    Supports Google Test framework with XML reporting and filtering.

.PARAMETER Filter
    Test filter pattern (default: * for all tests)

.PARAMETER Verbose
    Enable verbose test output

.PARAMETER Xml
    Generate XML test report

.PARAMETER BuildDir
    Build directory containing test executables (default: build-Debug)

.PARAMETER BuildType
    Build type: Debug (default) or Release

.PARAMETER Repeat
    Number of times to repeat tests (default: 1)

.PARAMETER Parallel
    Run tests in parallel (if supported)

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\test.ps1
    Run all tests with default settings

.EXAMPLE
    .\test.ps1 -Filter "*Math*" -Verbose
    Run math-related tests with verbose output

.EXAMPLE
    .\test.ps1 -Xml "test_results.xml" -BuildType Release
    Run tests and generate XML report from Release build

.EXAMPLE
    .\test.ps1 -Repeat 5 -Filter "*Stress*"
    Run stress tests 5 times
#>

[CmdletBinding()]
param(
    [string]$Filter = "*",
    [switch]$VerboseOutput,
    [string]$Xml = "",
    [string]$BuildDir = "",

    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [int]$Repeat = 1,
    [switch]$Parallel,
    [switch]$Help
)

# Set verbose preference
if ($VerboseOutput) {
    $VerbosePreference = "Continue"
}

# Helper functions
function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host $Text.PadLeft(($Text.Length + 60) / 2).PadRight(60) -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "[Step] $Text" -ForegroundColor Blue
    Write-Verbose "Starting step: $Text"
}

function Write-Success {
    param([string]$Text)
    Write-Host "√ $Text" -ForegroundColor Green
    Write-Verbose "Success: $Text"
}

function Write-Warning {
    param([string]$Text)
    Write-Host "! $Text" -ForegroundColor Yellow
    Write-Verbose "Warning: $Text"
}

function Write-Error {
    param([string]$Text)
    Write-Host "X $Text" -ForegroundColor Red
    Write-Verbose "Error: $Text"
}

function Get-ProjectRoot {
    return (Get-Item $PSScriptRoot).Parent.Parent.FullName
}

function Show-Help {
    Write-Host @"
Nexus Test Script - PowerShell Version

Usage: .\test.ps1 [options]

Options:
  -Filter <pattern>     Test filter pattern (default: * for all tests)
  -Verbose              Enable verbose test output
  -Xml <file>           Generate XML test report
  -BuildDir <dir>       Build directory (default: build-Debug)
  -BuildType <type>     Build type: Debug (default), Release
  -Repeat <count>       Number of times to repeat tests (default: 1)
  -Parallel             Run tests in parallel (if supported)
  -Help                 Show this help information

Examples:
  .\test.ps1                              # Run all tests
  .\test.ps1 -Filter "*Math*" -Verbose    # Run math tests with verbose output
  .\test.ps1 -Xml "results.xml"           # Generate XML report
  .\test.ps1 -Repeat 3 -Filter "*Unit*"   # Repeat unit tests 3 times

Filter Patterns:
  *                     All tests
  *Math*                Tests containing "Math"
  TestSuite.*           All tests in TestSuite
  TestSuite.TestName    Specific test

Google Test Options:
  The script uses Google Test framework which supports advanced filtering
  and reporting options. XML reports are compatible with CI/CD systems.

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Find-TestExecutable {
    param([string]$BuildDir)

    Write-Step "Locating test executable"

    # Possible test executable paths
    $testPaths = @(
        "$BuildDir\tests\Debug\nexus_tests.exe"
        "$BuildDir\tests\Release\nexus_tests.exe"
        "$BuildDir\tests\nexus_tests.exe"
        "$BuildDir\tests\nexus_tests"
        "$BuildDir\Debug\tests\nexus_tests.exe"
        "$BuildDir\Release\tests\nexus_tests.exe"
    )

    foreach ($path in $testPaths) {
        Write-Verbose "Checking for test executable: $path"
        if (Test-Path $path) {
            Write-Success "Found test executable: $path"
            return $path
        }
    }

    Write-Error "Test executable not found in $BuildDir"
    Write-Host "Searched paths:" -ForegroundColor Yellow
    foreach ($path in $testPaths) {
        Write-Host "  - $path" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "Please run the build script first:" -ForegroundColor Yellow
    Write-Host "  .\scripts\building\build.ps1" -ForegroundColor Cyan

    return $null
}

function Get-TestExecutableInfo {
    param([string]$TestExe)

    Write-Step "Getting test executable information"

    try {
        # Get file info
        $fileInfo = Get-Item $TestExe
        $size = [math]::Round($fileInfo.Length / 1KB, 1)
        Write-Host "  Executable: $($fileInfo.Name)"
        Write-Host "  Size: $size KB"
        Write-Host "  Modified: $($fileInfo.LastWriteTime)"

        # Try to get test list
        Write-Verbose "Attempting to get test list from executable"
        $result = & $TestExe --gtest_list_tests 2>$null
        if ($LASTEXITCODE -eq 0) {
            $testCount = ($result | Where-Object { $_ -match '^\s+\w+' }).Count
            Write-Host "  Available tests: $testCount"
            Write-Verbose "Test list: $result"
        }
        else {
            Write-Verbose "Could not retrieve test list (exit code: $LASTEXITCODE)"
        }
    }
    catch {
        Write-Verbose "Error getting test executable info: $_"
    }
}

function Invoke-Tests {
    param(
        [string]$TestExe,
        [string]$Filter,
        [bool]$VerboseOutput,
        [string]$XmlOutput,
        [int]$RepeatCount,
        [bool]$ParallelExecution
    )

    Write-Step "Running tests"

    # Build test command arguments
    $testArgs = @()

    # Filter
    if ($Filter -and $Filter -ne "*") {
        $testArgs += "--gtest_filter=$Filter"
        Write-Host "  Filter: $Filter" -ForegroundColor Gray
    }

    # Color output
    $testArgs += "--gtest_color=yes"

    # Verbose output
    if (!$VerboseOutput) {
        $testArgs += "--gtest_brief=1"
    }

    # XML output
    if ($XmlOutput) {
        $testArgs += "--gtest_output=xml:$XmlOutput"
        Write-Host "  XML Report: $XmlOutput" -ForegroundColor Gray
    }

    # Repeat
    if ($RepeatCount -gt 1) {
        $testArgs += "--gtest_repeat=$RepeatCount"
        Write-Host "  Repeat Count: $RepeatCount" -ForegroundColor Gray
    }

    # Parallel execution (if supported by the test framework)
    if ($ParallelExecution) {
        $testArgs += "--gtest_parallel=1"
        Write-Host "  Parallel Execution: Enabled" -ForegroundColor Gray
    }

    Write-Verbose "Test command: $TestExe $($testArgs -join ' ')"

    # Run tests
    $allPassed = $true
    $totalDuration = 0

    for ($i = 1; $i -le $RepeatCount; $i++) {
        if ($RepeatCount -gt 1) {
            Write-Host ""
            Write-Host "Test Run $i of $RepeatCount" -ForegroundColor Magenta
            Write-Host "=" * 40 -ForegroundColor Magenta
        }

        try {
            $startTime = Get-Date
            $result = & $TestExe @testArgs 2>&1
            $exitCode = $LASTEXITCODE
            $duration = (Get-Date) - $startTime
            $totalDuration += $duration.TotalSeconds

            Write-Verbose "Test run $i completed with exit code: $exitCode"
            Write-Verbose "Test run $i duration: $($duration.TotalSeconds.ToString('F1'))s"

            if ($exitCode -ne 0) {
                $allPassed = $false
                Write-Warning "Test run $i failed (exit code: $exitCode)"
            }
            else {
                Write-Success "Test run $i passed ($($duration.TotalSeconds.ToString('F1'))s)"
            }

            # Show test output
            if ($result) {
                Write-Host $result
            }
        }
        catch {
            Write-Error "Error running tests: $_"
            $allPassed = $false
            break
        }
    }

    # Summary
    Write-Host ""
    Write-Host "Test Execution Summary:" -ForegroundColor Cyan
    Write-Host "  Total Runs: $RepeatCount"
    Write-Host "  Total Duration: $($totalDuration.ToString('F1'))s"
    Write-Host "  Average Duration: $([math]::Round($totalDuration / $RepeatCount, 1))s"

    if ($XmlOutput -and (Test-Path $XmlOutput)) {
        $xmlSize = [math]::Round((Get-Item $XmlOutput).Length / 1KB, 1)
        Write-Host "  XML Report: $XmlOutput ($xmlSize KB)"
    }

    return $allPassed
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    $projectRoot = Get-ProjectRoot

    # Determine build directory
    if (!$BuildDir) {
        $BuildDir = Join-Path $projectRoot "build-$BuildType"
    }
    elseif (![System.IO.Path]::IsPathRooted($BuildDir)) {
        $BuildDir = Join-Path $projectRoot $BuildDir
    }

    Write-Header "Nexus Test Runner - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Project Root: $projectRoot"
    Write-Host "  Build Directory: $BuildDir"
    Write-Host "  Build Type: $BuildType"
    Write-Host "  Test Filter: $Filter"
    Write-Host "  Verbose Output: $(if ($Verbose) { 'Yes' } else { 'No' })"
    Write-Host "  XML Report: $(if ($Xml) { $Xml } else { 'None' })"
    Write-Host "  Repeat Count: $Repeat"
    Write-Host "  Parallel Execution: $(if ($Parallel) { 'Yes' } else { 'No' })"

    # Check if build directory exists
    if (!(Test-Path $BuildDir)) {
        Write-Error "Build directory not found: $BuildDir"
        Write-Host "Please run the build script first:" -ForegroundColor Yellow
        Write-Host "  .\scripts\building\build.ps1" -ForegroundColor Cyan
        return 1
    }

    # Find test executable
    $testExe = Find-TestExecutable -BuildDir $BuildDir
    if (!$testExe) {
        return 1
    }

    # Get test executable info
    Get-TestExecutableInfo -TestExe $testExe

    # Run tests
    $success = Invoke-Tests -TestExe $testExe -Filter $Filter -VerboseOutput $Verbose -XmlOutput $Xml -RepeatCount $Repeat -ParallelExecution $Parallel

    Write-Header "Test Results"

    if ($success) {
        Write-Success "All tests passed!"

        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  Check code coverage: .\scripts\tools\coverage.ps1"
        Write-Host "  Run performance tests: .\test.ps1 -Filter '*Performance*'"
        Write-Host "  Generate test report: .\scripts\tools\report.ps1"
    }
    else {
        Write-Error "Some tests failed!"

        Write-Host ""
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  Run with verbose output: .\test.ps1 -Verbose"
        Write-Host "  Run specific test: .\test.ps1 -Filter 'TestSuite.TestName'"
        Write-Host "  Check build logs: .\scripts\building\build.ps1 -Verbose"

        if ($Xml -and (Test-Path $Xml)) {
            Write-Host "  Review XML report: $Xml"
        }
    }

    return $(if ($success) { 0 } else { 1 })
}

# Execute main function with error handling
try {
    exit (Main)
}
catch {
    Write-Error "An error occurred during script execution: $_"
    Write-Verbose "Error details: $($_.Exception.ToString())"
    exit 1
}
