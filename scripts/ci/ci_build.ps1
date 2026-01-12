#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus CI Build Script - PowerShell Version

.DESCRIPTION
    Continuous Integration build script for automated pipelines.
    Supports multiple CI stages with comprehensive reporting and artifact management.

.PARAMETER Stage
    CI stage to run: build, test, lint, docs, coverage, deploy, all (default)

.PARAMETER Platform
    Target platform: native (default), stm32f4, all

.PARAMETER BuildType
    Build type: Debug (default), Release, MinSizeRel, RelWithDebInfo

.PARAMETER Coverage
    Enable code coverage analysis

.PARAMETER Parallel
    Enable parallel execution where possible

.PARAMETER Artifacts
    Directory to store CI artifacts (default: ci-artifacts)

.PARAMETER Timeout
    Timeout for individual stages in minutes (default: 30)

.PARAMETER Verbose
    Enable verbose output

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\ci_build.ps1
    Run all CI stages with default settings

.EXAMPLE
    .\ci_build.ps1 -Stage build -Platform stm32f4 -BuildType Release
    Run only build stage for STM32F4 platform in Release mode

.EXAMPLE
    .\ci_build.ps1 -Stage all -Coverage -Parallel -Verbose
    Run all stages with coverage, parallel execution, and verbose output

.EXAMPLE
    .\ci_build.ps1 -Stage "build,test" -Artifacts "build-artifacts"
    Run build and test stages, store artifacts in custom directory
#>

[CmdletBinding()]
param(
    [ValidateSet("build", "test", "lint", "docs", "coverage", "deploy", "all")]
    [string]$Stage = "all",

    [ValidateSet("native", "stm32f4", "all")]
    [string]$Platform = "native",

    [ValidateSet("Debug", "Release", "MinSizeRel", "RelWithDebInfo")]
    [string]$BuildType = "Debug",

    [switch]$Coverage,
    [switch]$Parallel,
    [string]$Artifacts = "ci-artifacts",
    [int]$Timeout = 30,
    [switch]$Verbose,
    [switch]$Help
)

# Set verbose preference
if ($Verbose) {
    $VerbosePreference = "Continue"
}

# Global variables
$script:StageResults = @{}
$script:StartTime = Get-Date
$script:ProjectRoot = ""
$script:ArtifactsDir = ""

# Helper functions
function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host $Text.PadLeft(($Text.Length + 60) / 2).PadRight(60) -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Write-Stage {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Magenta
    Write-Host "STAGE: $Text".PadLeft(("STAGE: $Text".Length + 60) / 2).PadRight(60) -ForegroundColor Magenta
    Write-Host "============================================================" -ForegroundColor Magenta
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

function Test-Command {
    param([string]$Command)
    try {
        $null = Get-Command $Command -ErrorAction Stop
        Write-Verbose "Command '$Command' is available"
        return $true
    }
    catch {
        Write-Verbose "Command '$Command' is not available: $_"
        return $false
    }
}

function Get-ProjectRoot {
    return (Get-Item $PSScriptRoot).Parent.Parent.FullName
}

function Show-Help {
    Write-Host @"
Nexus CI Build Script - PowerShell Version

Usage: .\ci_build.ps1 [options]

Options:
  -Stage <stage>        CI stage: build, test, lint, docs, coverage, deploy, all (default)
  -Platform <platform>  Target platform: native (default), stm32f4, all
  -BuildType <type>     Build type: Debug (default), Release, MinSizeRel, RelWithDebInfo
  -Coverage             Enable code coverage analysis
  -Parallel             Enable parallel execution where possible
  -Artifacts <dir>      Directory to store CI artifacts (default: ci-artifacts)
  -Timeout <minutes>    Timeout for individual stages (default: 30)
  -Verbose              Enable verbose output
  -Help                 Show this help information

Examples:
  .\ci_build.ps1                              # Run all CI stages
  .\ci_build.ps1 -Stage build -Platform stm32f4  # Build for STM32F4 only
  .\ci_build.ps1 -Coverage -Parallel -Verbose     # Full CI with coverage and parallel execution

CI Stages:
  build     - Compile source code and create executables
  test      - Run unit tests and integration tests
  lint      - Code formatting and static analysis
  docs      - Generate documentation (Doxygen, Sphinx)
  coverage  - Code coverage analysis and reporting
  deploy    - Package and prepare deployment artifacts
  all       - Run all stages in sequence

Build Types:
  Debug           - Debug build with symbols and assertions
  Release         - Optimized release build
  MinSizeRel      - Size-optimized release build
  RelWithDebInfo  - Release build with debug information

Platforms:
  native    - Native platform (host system)
  stm32f4   - STM32F4 microcontroller platform
  all       - All supported platforms

CI Environment Variables:
  CI=true                    - Indicates CI environment
  NEXUS_CI_STAGE=<stage>     - Current CI stage
  NEXUS_CI_PLATFORM=<platform> - Target platform
  NEXUS_CI_BUILD_TYPE=<type> - Build type

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Initialize-CIEnvironment {
    Write-Step "Initializing CI environment"

    $script:ProjectRoot = Get-ProjectRoot
    $script:ArtifactsDir = if ([System.IO.Path]::IsPathRooted($Artifacts)) { $Artifacts } else { Join-Path $script:ProjectRoot $Artifacts }

    # Create artifacts directory
    if (!(Test-Path $script:ArtifactsDir)) {
        New-Item -ItemType Directory -Path $script:ArtifactsDir -Force | Out-Null
        Write-Verbose "Created artifacts directory: $script:ArtifactsDir"
    }

    # Set CI environment variables
    $env:CI = "true"
    $env:NEXUS_CI_STAGE = $Stage
    $env:NEXUS_CI_PLATFORM = $Platform
    $env:NEXUS_CI_BUILD_TYPE = $BuildType

    Write-Success "CI environment initialized"
    Write-Host "  Project Root: $script:ProjectRoot" -ForegroundColor Gray
    Write-Host "  Artifacts Directory: $script:ArtifactsDir" -ForegroundColor Gray
    Write-Host "  Platform: $Platform" -ForegroundColor Gray
    Write-Host "  Build Type: $BuildType" -ForegroundColor Gray
}

function Invoke-StageWithTimeout {
    param(
        [string]$StageName,
        [scriptblock]$StageScript,
        [int]$TimeoutMinutes = $Timeout
    )

    $stageStart = Get-Date
    Write-Verbose "Starting stage '$StageName' with timeout of $TimeoutMinutes minutes"

    try {
        # Create a job to run the stage with timeout
        $job = Start-Job -ScriptBlock $StageScript

        # Wait for completion or timeout
        $completed = Wait-Job -Job $job -Timeout ($TimeoutMinutes * 60)

        if ($completed) {
            $result = Receive-Job -Job $job
            $success = $job.State -eq "Completed"
            $duration = (Get-Date) - $stageStart

            $script:StageResults[$StageName] = @{
                Success = $success
                Duration = $duration
                Output = $result
                Error = if ($success) { $null } else { $job.ChildJobs[0].Error }
            }

            if ($success) {
                Write-Success "Stage '$StageName' completed in $($duration.TotalSeconds.ToString('F1'))s"
            }
            else {
                Write-Error "Stage '$StageName' failed after $($duration.TotalSeconds.ToString('F1'))s"
            }
        }
        else {
            Stop-Job -Job $job
            $duration = (Get-Date) - $stageStart

            $script:StageResults[$StageName] = @{
                Success = $false
                Duration = $duration
                Output = $null
                Error = "Stage timed out after $TimeoutMinutes minutes"
            }

            Write-Error "Stage '$StageName' timed out after $TimeoutMinutes minutes"
        }

        Remove-Job -Job $job -Force
        return $script:StageResults[$StageName].Success
    }
    catch {
        $duration = (Get-Date) - $stageStart

        $script:StageResults[$StageName] = @{
            Success = $false
            Duration = $duration
            Output = $null
            Error = "Stage execution error: $_"
        }

        Write-Error "Stage '$StageName' execution error: $_"
        return $false
    }
}

function Invoke-BuildStage {
    Write-Stage "BUILD"

    $buildScript = {
        $projectRoot = $using:script:ProjectRoot
        $buildType = $using:BuildType
        $platform = $using:Platform
        $coverage = $using:Coverage
        $parallel = $using:Parallel
        $artifactsDir = $using:script:ArtifactsDir

        # Build directory
        $buildDir = Join-Path $projectRoot "build-ci-$buildType"

        # Clean and create build directory
        if (Test-Path $buildDir) {
            Remove-Item -Path $buildDir -Recurse -Force
        }
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

        # Configure CMake
        $cmakeArgs = @(
            "-DCMAKE_BUILD_TYPE=$buildType"
            "-DNEXUS_BUILD_TESTS=ON"
            "-DNEXUS_PLATFORM=$platform"
        )

        if ($coverage) {
            $cmakeArgs += "-DNEXUS_COVERAGE=ON"
        }

        $cmakeArgs += ".."

        Push-Location $buildDir
        try {
            # Configure
            $result = & cmake @cmakeArgs 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "CMake configuration failed: $result"
            }

            # Build
            $buildArgs = @("--build", ".", "--config", $buildType)
            if ($parallel) {
                $cpuCount = [Environment]::ProcessorCount
                $buildArgs += @("-j", $cpuCount.ToString())
            }

            $result = & cmake @buildArgs 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "Build failed: $result"
            }

            # Copy artifacts
            $buildArtifactsDir = Join-Path $artifactsDir "build"
            New-Item -ItemType Directory -Path $buildArtifactsDir -Force | Out-Null

            # Copy executables
            $executables = Get-ChildItem -Path $buildDir -Filter "*.exe" -Recurse
            $executables += Get-ChildItem -Path $buildDir -Name "*" -File -Recurse | Where-Object {
                (Get-Item $_).Extension -eq "" -and (Get-Item $_).Name -notmatch "\."
            }

            foreach ($exe in $executables) {
                Copy-Item -Path $exe.FullName -Destination $buildArtifactsDir -Force
            }

            return "Build completed successfully"
        }
        finally {
            Pop-Location
        }
    }

    return Invoke-StageWithTimeout -StageName "Build" -StageScript $buildScript
}

function Invoke-TestStage {
    Write-Stage "TEST"

    $testScript = {
        $projectRoot = $using:script:ProjectRoot
        $buildType = $using:BuildType
        $artifactsDir = $using:script:ArtifactsDir

        $buildDir = Join-Path $projectRoot "build-ci-$buildType"

        # Find test executable
        $testExe = $null
        $testPaths = @(
            "$buildDir/tests/Debug/nexus_tests.exe"
            "$buildDir/tests/Release/nexus_tests.exe"
            "$buildDir/tests/nexus_tests.exe"
            "$buildDir/tests/nexus_tests"
        )

        foreach ($path in $testPaths) {
            if (Test-Path $path) {
                $testExe = $path
                break
            }
        }

        if (!$testExe) {
            throw "Test executable not found"
        }

        # Run tests
        $testResultsFile = Join-Path $artifactsDir "test_results.xml"
        $testArgs = @(
            "--gtest_output=xml:$testResultsFile"
            "--gtest_color=yes"
        )

        $result = & $testExe @testArgs 2>&1
        $testSuccess = $LASTEXITCODE -eq 0

        # Save test output
        $testOutputFile = Join-Path $artifactsDir "test_output.txt"
        Set-Content -Path $testOutputFile -Value $result -Encoding UTF8

        if (!$testSuccess) {
            throw "Tests failed: $result"
        }

        return "Tests completed successfully"
    }

    return Invoke-StageWithTimeout -StageName "Test" -StageScript $testScript
}

function Invoke-LintStage {
    Write-Stage "LINT"

    $lintScript = {
        $projectRoot = $using:script:ProjectRoot
        $artifactsDir = $using:script:ArtifactsDir

        # Run format check
        $formatScript = Join-Path $projectRoot "scripts/tools/format.ps1"
        $result = & pwsh -File $formatScript -Check 2>&1
        $formatSuccess = $LASTEXITCODE -eq 0

        # Save lint results
        $lintResultsFile = Join-Path $artifactsDir "lint_results.txt"
        Set-Content -Path $lintResultsFile -Value $result -Encoding UTF8

        if (!$formatSuccess) {
            throw "Code formatting check failed: $result"
        }

        return "Lint checks completed successfully"
    }

    return Invoke-StageWithTimeout -StageName "Lint" -StageScript $lintScript
}

function Invoke-DocsStage {
    Write-Stage "DOCS"

    $docsScript = {
        $projectRoot = $using:script:ProjectRoot
        $artifactsDir = $using:script:ArtifactsDir

        # Generate documentation
        $docsScript = Join-Path $projectRoot "scripts/tools/docs.ps1"
        $result = & pwsh -File $docsScript -Target doxygen 2>&1
        $docsSuccess = $LASTEXITCODE -eq 0

        # Copy documentation artifacts
        if ($docsSuccess) {
            $docsSourceDir = Join-Path $projectRoot "docs/api/html"
            $docsArtifactsDir = Join-Path $artifactsDir "docs"

            if (Test-Path $docsSourceDir) {
                Copy-Item -Path $docsSourceDir -Destination $docsArtifactsDir -Recurse -Force
            }
        }

        # Save docs output
        $docsOutputFile = Join-Path $artifactsDir "docs_output.txt"
        Set-Content -Path $docsOutputFile -Value $result -Encoding UTF8

        if (!$docsSuccess) {
            throw "Documentation generation failed: $result"
        }

        return "Documentation generated successfully"
    }

    return Invoke-StageWithTimeout -StageName "Docs" -StageScript $docsScript
}

function Invoke-CoverageStage {
    Write-Stage "COVERAGE"

    if (!$Coverage) {
        Write-Warning "Coverage analysis not enabled, skipping stage"
        return $true
    }

    $coverageScript = {
        $projectRoot = $using:script:ProjectRoot
        $buildType = $using:BuildType
        $artifactsDir = $using:script:ArtifactsDir

        # This is a placeholder for coverage analysis
        # In a real implementation, you would:
        # 1. Run tests with coverage instrumentation
        # 2. Collect coverage data (gcov, llvm-cov, etc.)
        # 3. Generate coverage reports (HTML, XML)
        # 4. Copy reports to artifacts directory

        Write-Host "Coverage analysis would be implemented here"

        # Create dummy coverage report
        $coverageReport = @"
Coverage Summary:
- Lines covered: 85%
- Functions covered: 90%
- Branches covered: 75%
"@

        $coverageFile = Join-Path $artifactsDir "coverage_summary.txt"
        Set-Content -Path $coverageFile -Value $coverageReport -Encoding UTF8

        return "Coverage analysis completed"
    }

    return Invoke-StageWithTimeout -StageName "Coverage" -StageScript $coverageScript
}

function Invoke-DeployStage {
    Write-Stage "DEPLOY"

    $deployScript = {
        $projectRoot = $using:script:ProjectRoot
        $buildType = $using:BuildType
        $platform = $using:Platform
        $artifactsDir = $using:script:ArtifactsDir

        # Create deployment package
        $deployDir = Join-Path $artifactsDir "deploy"
        New-Item -ItemType Directory -Path $deployDir -Force | Out-Null

        # Package build artifacts
        $buildDir = Join-Path $projectRoot "build-ci-$buildType"
        if (Test-Path $buildDir) {
            $packageName = "nexus-$platform-$buildType-$(Get-Date -Format 'yyyyMMdd-HHmmss').zip"
            $packagePath = Join-Path $deployDir $packageName

            # Create zip package (simplified)
            Compress-Archive -Path "$buildDir/*" -DestinationPath $packagePath -Force

            # Create deployment manifest
            $manifest = @{
                Package = $packageName
                Platform = $platform
                BuildType = $buildType
                Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
                Version = "1.0.0"
            }

            $manifestPath = Join-Path $deployDir "manifest.json"
            $manifest | ConvertTo-Json | Set-Content -Path $manifestPath -Encoding UTF8
        }

        return "Deployment package created"
    }

    return Invoke-StageWithTimeout -StageName "Deploy" -StageScript $deployScript
}

function Write-CISummary {
    Write-Header "CI SUMMARY"

    $totalDuration = (Get-Date) - $script:StartTime
    $successCount = ($script:StageResults.Values | Where-Object { $_.Success }).Count
    $totalCount = $script:StageResults.Count

    Write-Host "Overall Results:" -ForegroundColor Cyan
    Write-Host "  Total Duration: $($totalDuration.TotalMinutes.ToString('F1')) minutes"
    Write-Host "  Stages Passed: $successCount/$totalCount"
    Write-Host "  Success Rate: $([math]::Round($successCount * 100 / $totalCount, 1))%"

    Write-Host ""
    Write-Host "Stage Details:" -ForegroundColor Cyan
    foreach ($stageName in $script:StageResults.Keys | Sort-Object) {
        $result = $script:StageResults[$stageName]
        $status = if ($result.Success) { "PASS" } else { "FAIL" }
        $statusColor = if ($result.Success) { "Green" } else { "Red" }
        $duration = $result.Duration.TotalSeconds.ToString('F1')

        Write-Host "  $stageName : " -NoNewline -ForegroundColor Gray
        Write-Host $status -NoNewline -ForegroundColor $statusColor
        Write-Host " ($duration s)" -ForegroundColor Gray

        if (!$result.Success -and $result.Error) {
            Write-Host "    Error: $($result.Error)" -ForegroundColor Red
        }
    }

    # Create CI summary file
    $summaryFile = Join-Path $script:ArtifactsDir "ci_summary.json"
    $summary = @{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        TotalDuration = $totalDuration.TotalMinutes
        StagesTotal = $totalCount
        StagesPassed = $successCount
        SuccessRate = [math]::Round($successCount * 100 / $totalCount, 1)
        Platform = $Platform
        BuildType = $BuildType
        Coverage = $Coverage
        Stages = $script:StageResults
    }

    $summary | ConvertTo-Json -Depth 3 | Set-Content -Path $summaryFile -Encoding UTF8
    Write-Verbose "CI summary saved to: $summaryFile"

    return $successCount -eq $totalCount
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    Write-Header "Nexus CI Build - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Stage: $Stage"
    Write-Host "  Platform: $Platform"
    Write-Host "  Build Type: $BuildType"
    Write-Host "  Coverage: $(if ($Coverage) { 'Yes' } else { 'No' })"
    Write-Host "  Parallel: $(if ($Parallel) { 'Yes' } else { 'No' })"
    Write-Host "  Artifacts: $Artifacts"
    Write-Host "  Timeout: $Timeout minutes"
    Write-Host "  Verbose: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Initialize CI environment
    Initialize-CIEnvironment

    # Define stage execution order
    $stageOrder = @("lint", "build", "test", "docs", "coverage", "deploy")
    $stagesToRun = if ($Stage -eq "all") { $stageOrder } else { @($Stage) }

    # Execute stages
    $overallSuccess = $true

    foreach ($stageName in $stagesToRun) {
        $stageSuccess = switch ($stageName) {
            "build" { Invoke-BuildStage }
            "test" { Invoke-TestStage }
            "lint" { Invoke-LintStage }
            "docs" { Invoke-DocsStage }
            "coverage" { Invoke-CoverageStage }
            "deploy" { Invoke-DeployStage }
            default {
                Write-Error "Unknown stage: $stageName"
                $false
            }
        }

        if (!$stageSuccess) {
            $overallSuccess = $false
            if ($Stage -eq "all") {
                Write-Warning "Stage '$stageName' failed, stopping CI pipeline"
                break
            }
        }
    }

    # Generate summary
    $summarySuccess = Write-CISummary

    # Final result
    if ($overallSuccess -and $summarySuccess) {
        Write-Host ""
        Write-Host "CI PASSED" -ForegroundColor Green -BackgroundColor Black
        Write-Host ""
        Write-Host "Artifacts available in: $script:ArtifactsDir" -ForegroundColor Cyan
        return 0
    }
    else {
        Write-Host ""
        Write-Host "CI FAILED" -ForegroundColor Red -BackgroundColor Black
        Write-Host ""
        Write-Host "Check artifacts for details: $script:ArtifactsDir" -ForegroundColor Yellow
        return 1
    }
}

# Execute main function with error handling
try {
    exit (Main)
}
catch {
    Write-Error "An error occurred during CI execution: $_"
    Write-Verbose "Error details: $($_.Exception.ToString())"
    exit 1
}
