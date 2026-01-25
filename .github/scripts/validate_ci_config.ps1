##############################################################################
# Validate CI/CD Configuration (PowerShell)
#
# This script validates that the CI/CD pipeline configuration is correct
# and all required files are present.
#
# Usage:
#   .\validate_ci_config.ps1
##############################################################################

$ErrorActionPreference = "Stop"

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

function Write-Success {
    param([string]$Message)
    Write-ColorOutput "✅ $Message" "Green"
}

function Write-Error-Custom {
    param([string]$Message)
    Write-ColorOutput "❌ ERROR: $Message" "Red"
}

function Write-Warning-Custom {
    param([string]$Message)
    Write-ColorOutput "⚠️  WARNING: $Message" "Yellow"
}

function Write-Info {
    param([string]$Message)
    Write-ColorOutput "ℹ️  $Message" "Cyan"
}

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Get-Item (Join-Path $ScriptDir "..\..")).FullName

Write-Host "=== Validating CI/CD Configuration ===" -ForegroundColor Cyan
Write-Host ""

# Check required files
Write-Info "Checking required files..."

$RequiredFiles = @(
    ".github\workflows\test.yml",
    "codecov.yml",
    ".github\CI_CD_GUIDE.md",
    "scripts\coverage\run_coverage_linux.sh",
    "scripts\coverage\run_coverage_windows.ps1"
)

$AllFilesPresent = $true
foreach ($file in $RequiredFiles) {
    $fullPath = Join-Path $ProjectRoot $file
    if (Test-Path $fullPath) {
        Write-Success "Found: $file"
    } else {
        Write-Error-Custom "Missing: $file"
        $AllFilesPresent = $false
    }
}

if (-not $AllFilesPresent) {
    Write-Error-Custom "Some required files are missing!"
    exit 1
}

Write-Host ""

# Check coverage thresholds
Write-Info "Checking coverage thresholds..."

$TestYml = Join-Path $ProjectRoot ".github\workflows\test.yml"
$content = Get-Content $TestYml -Raw

if ($content -match 'MIN_COVERAGE=(\d+\.?\d*)') {
    $MinCoverage = $Matches[1]
    Write-Success "Minimum coverage threshold: $MinCoverage%"
} else {
    Write-Error-Custom "Could not find MIN_COVERAGE in test.yml"
    exit 1
}

if ($content -match 'TARGET_COVERAGE=(\d+\.?\d*)') {
    $TargetCoverage = $Matches[1]
    Write-Success "Target coverage threshold: $TargetCoverage%"
} else {
    Write-Error-Custom "Could not find TARGET_COVERAGE in test.yml"
    exit 1
}

# Validate thresholds
if ([double]$MinCoverage -lt 90) {
    Write-Warning-Custom "Minimum coverage threshold is below 90%"
}

if ([double]$TargetCoverage -lt [double]$MinCoverage) {
    Write-Error-Custom "Target coverage is less than minimum coverage!"
    exit 1
}

Write-Host ""

# Check codecov configuration
Write-Info "Checking codecov configuration..."

$CodecovYml = Join-Path $ProjectRoot "codecov.yml"
$codecovContent = Get-Content $CodecovYml -Raw

if ($codecovContent -match 'target:\s*100%') {
    Write-Success "Codecov target is set to 100%"
} else {
    Write-Warning-Custom "Codecov target is not set to 100%"
}

if ($codecovContent -match 'native-hal') {
    Write-Success "Codecov flag 'native-hal' is configured"
} else {
    Write-Error-Custom "Codecov flag 'native-hal' is missing"
    exit 1
}

Write-Host ""

# Check for required tools
Write-Info "Checking required tools..."

$Tools = @{
    "cmake" = "CMake"
    "python" = "Python 3"
}

$AllToolsPresent = $true
foreach ($cmd in $Tools.Keys) {
    $name = $Tools[$cmd]
    try {
        $version = & $cmd --version 2>&1 | Select-Object -First 1
        Write-Success "$name found: $version"
    } catch {
        Write-Warning-Custom "$name not found (required for local testing)"
        $AllToolsPresent = $false
    }
}

if (-not $AllToolsPresent) {
    Write-Info "Some tools are missing but CI will still work"
}

Write-Host ""

# Summary
Write-Info "=== Validation Summary ==="
Write-Success "CI/CD configuration is valid!"
Write-Info "Coverage thresholds: Min=$MinCoverage%, Target=$TargetCoverage%"
Write-Info "All required files are present"
Write-Info "Configuration is ready for use"

Write-Host ""
Write-Info "Next steps:"
Write-Host "  1. Commit the CI/CD configuration files"
Write-Host "  2. Push to trigger the CI pipeline"
Write-Host "  3. Monitor the pipeline in GitHub Actions"
Write-Host "  4. Review coverage reports"

