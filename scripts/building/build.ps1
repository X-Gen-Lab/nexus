#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Build Script - PowerShell Version

.DESCRIPTION
    Cross-platform build script for Nexus embedded development platform.
    Supports Windows, Linux, and macOS with automatic toolchain detection.

.PARAMETER Type
    Build type: Debug (default) or Release

.PARAMETER Clean
    Clean build directory before building

.PARAMETER Jobs
    Number of parallel jobs (default: auto-detect CPU count)

.PARAMETER Platform
    Target platform: native (default), stm32f4, all

.PARAMETER Verbose
    Enable verbose output

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\build.ps1
    Build with default settings (Debug, native platform)

.EXAMPLE
    .\build.ps1 -Type Release -Platform stm32f4 -Clean
    Clean build for STM32F4 platform in Release mode

.EXAMPLE
    .\build.ps1 -Jobs 8 -Verbose
    Build with 8 parallel jobs and verbose output
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Type = "Debug",

    [switch]$Clean,
    [int]$Jobs = 0,

    [ValidateSet("native", "stm32f4", "all")]
    [string]$Platform = "native",

    [switch]$VerboseOutput,
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

function Get-CPUCount {
    try {
        if ($env:NUMBER_OF_PROCESSORS) {
            return [int]$env:NUMBER_OF_PROCESSORS
        }
        elseif (Test-Command "nproc") {
            $result = & nproc 2>$null
            if ($LASTEXITCODE -eq 0) {
                return [int]$result
            }
        }
        elseif (Test-Command "sysctl") {
            $result = & sysctl -n hw.ncpu 2>$null
            if ($LASTEXITCODE -eq 0) {
                return [int]$result
            }
        }
        return 4  # Default fallback
    }
    catch {
        Write-Verbose "Failed to detect CPU count: $_"
        return 4
    }
}

function Get-ProjectRoot {
    return (Get-Item $PSScriptRoot).Parent.Parent.FullName
}

function Show-Help {
    Write-Host @"
Nexus Build Script - PowerShell Version

Usage: .\build.ps1 [options]

Options:
  -Type <type>        Build type: Debug (default), Release
  -Clean              Clean build directory before building
  -Jobs <number>      Number of parallel jobs (default: auto-detect)
  -Platform <platform> Target platform: native (default), stm32f4, all
  -Verbose            Enable verbose output
  -Help               Show this help information

Examples:
  .\build.ps1                           # Build with default settings
  .\build.ps1 -Type Release -Clean      # Clean Release build
  .\build.ps1 -Platform stm32f4 -Jobs 8 # STM32F4 build with 8 jobs
  .\build.ps1 -Verbose                  # Build with verbose output

Build Types:
  Debug    - Debug build with symbols and assertions
  Release  - Optimized release build

Platforms:
  native   - Native platform (host system)
  stm32f4  - STM32F4 microcontroller platform
  all      - All supported platforms

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Invoke-CMakeConfigure {
    param(
        [string]$BuildDir,
        [string]$BuildType,
        [string]$Platform
    )

    Write-Step "Configuring CMake"

    $cmakeArgs = @(
        "-DCMAKE_BUILD_TYPE=$BuildType"
        "-DNEXUS_BUILD_TESTS=ON"
        "-DNEXUS_PLATFORM=$Platform"
        ".."
    )

    Write-Verbose "CMake configure arguments: $($cmakeArgs -join ' ')"

    try {
        $startTime = Get-Date
        Push-Location $BuildDir
        $result = & cmake @cmakeArgs 2>&1
        $success = $LASTEXITCODE -eq 0
        $duration = (Get-Date) - $startTime

        if ($success) {
            Write-Success "CMake configuration completed in $($duration.TotalSeconds.ToString('F1'))s"
            Write-Verbose "CMake output: $result"
            return $true
        }
        else {
            Write-Error "CMake configuration failed"
            Write-Host "CMake output:" -ForegroundColor Red
            Write-Host $result -ForegroundColor Red
            return $false
        }
    }
    catch {
        Write-Error "CMake configuration error: $_"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Invoke-CMakeBuild {
    param(
        [string]$BuildDir,
        [string]$BuildType,
        [int]$Jobs
    )

    Write-Step "Building project"

    $cmakeArgs = @(
        "--build", "."
        "--config", $BuildType
    )

    if ($Jobs -gt 0) {
        $cmakeArgs += @("-j", $Jobs.ToString())
    }

    Write-Verbose "CMake build arguments: $($cmakeArgs -join ' ')"

    try {
        $startTime = Get-Date
        Push-Location $BuildDir
        $result = & cmake @cmakeArgs 2>&1
        $success = $LASTEXITCODE -eq 0
        $duration = (Get-Date) - $startTime

        if ($success) {
            Write-Success "Build completed in $($duration.TotalSeconds.ToString('F1'))s"
            Write-Verbose "Build output: $result"
            return $true
        }
        else {
            Write-Error "Build failed"
            Write-Host "Build output:" -ForegroundColor Red
            Write-Host $result -ForegroundColor Red
            return $false
        }
    }
    catch {
        Write-Error "Build error: $_"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Test-BuildArtifacts {
    param([string]$BuildDir, [string]$Platform)

    Write-Step "Verifying build artifacts"

    $artifacts = @()

    # Check for main application
    $appPaths = @(
        "$BuildDir/applications/blinky/Debug/blinky.exe"
        "$BuildDir/applications/blinky/Release/blinky.exe"
        "$BuildDir/applications/blinky/blinky.exe"
        "$BuildDir/applications/blinky/blinky"
    )

    $foundApp = $false
    foreach ($path in $appPaths) {
        if (Test-Path $path) {
            $artifacts += $path
            $foundApp = $true
            Write-Verbose "Found application: $path"
            break
        }
    }

    # Check for tests
    $testPaths = @(
        "$BuildDir/tests/Debug/nexus_tests.exe"
        "$BuildDir/tests/Release/nexus_tests.exe"
        "$BuildDir/tests/nexus_tests.exe"
        "$BuildDir/tests/nexus_tests"
    )

    $foundTests = $false
    foreach ($path in $testPaths) {
        if (Test-Path $path) {
            $artifacts += $path
            $foundTests = $true
            Write-Verbose "Found tests: $path"
            break
        }
    }

    if ($artifacts.Count -gt 0) {
        Write-Success "Build artifacts verified ($($artifacts.Count) files found)"
        foreach ($artifact in $artifacts) {
            $size = (Get-Item $artifact).Length
            Write-Host "  - $artifact ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Gray
        }
        return $true
    }
    else {
        Write-Warning "No build artifacts found - build may have failed"
        return $false
    }
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    # Validate prerequisites
    if (!(Test-Command "cmake")) {
        Write-Error "CMake not found! Please install CMake and add it to PATH."
        Write-Host "Download from: https://cmake.org/download/" -ForegroundColor Yellow
        return 1
    }

    # Auto-detect CPU count if not specified
    if ($Jobs -eq 0) {
        $Jobs = Get-CPUCount
        Write-Verbose "Auto-detected CPU count: $Jobs"
    }

    $projectRoot = Get-ProjectRoot
    $buildDir = Join-Path $projectRoot "build-$Type"

    Write-Header "Nexus Build Script - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Project Root: $projectRoot"
    Write-Host "  Build Type: $Type"
    Write-Host "  Platform: $Platform"
    Write-Host "  Build Directory: $buildDir"
    Write-Host "  Parallel Jobs: $Jobs"
    Write-Host "  Clean Build: $(if ($Clean) { 'Yes' } else { 'No' })"
    Write-Host "  Verbose Output: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Clean if requested
    if ($Clean -and (Test-Path $buildDir)) {
        Write-Step "Cleaning build directory"
        try {
            Remove-Item -Path $buildDir -Recurse -Force
            Write-Success "Build directory cleaned"
        }
        catch {
            Write-Error "Failed to clean build directory: $_"
            return 1
        }
    }

    # Create build directory
    if (!(Test-Path $buildDir)) {
        Write-Step "Creating build directory"
        try {
            New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
            Write-Success "Build directory created"
        }
        catch {
            Write-Error "Failed to create build directory: $_"
            return 1
        }
    }

    # Configure
    if (!(Invoke-CMakeConfigure -BuildDir $buildDir -BuildType $Type -Platform $Platform)) {
        Write-Error "Configuration failed!"
        return 1
    }

    # Build
    if (!(Invoke-CMakeBuild -BuildDir $buildDir -BuildType $Type -Jobs $Jobs)) {
        Write-Error "Build failed!"
        return 1
    }

    # Verify artifacts
    Test-BuildArtifacts -BuildDir $buildDir -Platform $Platform | Out-Null

    Write-Header "Build Complete"
    Write-Success "Nexus build completed successfully!"

    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  Run tests: .\scripts\test\test.ps1"
    Write-Host "  Format code: .\scripts\tools\format.ps1"
    Write-Host "  Generate docs: .\scripts\tools\docs.ps1"

    if ($Platform -eq "stm32f4") {
        Write-Host ""
        Write-Host "STM32F4 artifacts:" -ForegroundColor Magenta
        Write-Host "  Firmware: $buildDir\applications\blinky\"
        Write-Host "  Flash with: st-flash write firmware.bin 0x8000000"
    }

    return 0
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
