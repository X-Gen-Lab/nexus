#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Clean Script - PowerShell Version

.DESCRIPTION
    Cross-platform clean script for removing build artifacts, temporary files,
    and generated content from the Nexus project.

.PARAMETER All
    Clean everything including documentation, test artifacts, and caches

.PARAMETER Build
    Clean only build directories (default if no specific option is given)

.PARAMETER Docs
    Clean only documentation artifacts

.PARAMETER Tests
    Clean only test artifacts and reports

.PARAMETER Cache
    Clean only cache files and temporary directories

.PARAMETER Verbose
    Enable verbose output showing each item being removed

.PARAMETER DryRun
    Show what would be removed without actually deleting anything

.PARAMETER Force
    Force removal without confirmation prompts

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\clean.ps1
    Clean build directories only

.EXAMPLE
    .\clean.ps1 -All -Verbose
    Clean everything with verbose output

.EXAMPLE
    .\clean.ps1 -DryRun -All
    Show what would be cleaned without actually removing anything

.EXAMPLE
    .\clean.ps1 -Docs -Tests
    Clean only documentation and test artifacts
#>

[CmdletBinding()]
param(
    [switch]$All,
    [switch]$Build,
    [switch]$Docs,
    [switch]$Tests,
    [switch]$Cache,
    [switch]$VerboseOutput,
    [switch]$DryRun,
    [switch]$Force,
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
Nexus Clean Script - PowerShell Version

Usage: .\clean.ps1 [options]

Options:
  -All                  Clean everything (build, docs, tests, cache)
  -Build                Clean only build directories (default)
  -Docs                 Clean only documentation artifacts
  -Tests                Clean only test artifacts and reports
  -Cache                Clean only cache files and temporary directories
  -Verbose              Enable verbose output
  -DryRun               Show what would be removed without deleting
  -Force                Force removal without confirmation
  -Help                 Show this help information

Examples:
  .\clean.ps1                     # Clean build directories only
  .\clean.ps1 -All -Verbose       # Clean everything with verbose output
  .\clean.ps1 -DryRun -All        # Preview what would be cleaned
  .\clean.ps1 -Docs -Tests        # Clean docs and test artifacts only

Clean Categories:
  Build     - CMake build directories, object files, executables
  Docs      - Generated documentation (Doxygen, Sphinx)
  Tests     - Test reports, coverage data, temporary test files
  Cache     - IDE caches, temporary files, system artifacts

Safety Features:
  - Dry run mode to preview changes
  - Confirmation prompts for destructive operations
  - Verbose logging of all operations
  - Selective cleaning options

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Get-DirectorySize {
    param([string]$Path)

    if (!(Test-Path $Path)) {
        return 0
    }

    try {
        $size = (Get-ChildItem -Path $Path -Recurse -File | Measure-Object -Property Length -Sum).Sum
        return $size
    }
    catch {
        Write-Verbose "Failed to calculate size for $Path : $_"
        return 0
    }
}

function Format-FileSize {
    param([long]$Bytes)

    if ($Bytes -lt 1KB) {
        return "$Bytes B"
    }
    elseif ($Bytes -lt 1MB) {
        return "$([math]::Round($Bytes / 1KB, 1)) KB"
    }
    elseif ($Bytes -lt 1GB) {
        return "$([math]::Round($Bytes / 1MB, 1)) MB"
    }
    else {
        return "$([math]::Round($Bytes / 1GB, 2)) GB"
    }
}

function Remove-ItemSafely {
    param(
        [string]$Path,
        [string]$Description,
        [bool]$IsDirectory = $true
    )

    if (!(Test-Path $Path)) {
        Write-Verbose "Path does not exist: $Path"
        return @{ Removed = $false; Size = 0; Reason = "Not found" }
    }

    $size = if ($IsDirectory) { Get-DirectorySize -Path $Path } else { (Get-Item $Path).Length }
    $formattedSize = Format-FileSize -Bytes $size

    if ($DryRun) {
        Write-Host "  [DRY RUN] Would remove: $Description ($formattedSize)" -ForegroundColor Yellow
        return @{ Removed = $false; Size = $size; Reason = "Dry run" }
    }

    try {
        if ($Verbose) {
            Write-Host "  Removing: $Description ($formattedSize)" -ForegroundColor Gray
        }

        if ($IsDirectory) {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
        }
        else {
            Remove-Item -Path $Path -Force -ErrorAction Stop
        }

        Write-Verbose "Successfully removed: $Path"
        return @{ Removed = $true; Size = $size; Reason = "Success" }
    }
    catch {
        Write-Warning "Failed to remove $Description : $_"
        Write-Verbose "Remove error for $Path : $_"
        return @{ Removed = $false; Size = 0; Reason = "Error: $_" }
    }
}

function Clean-BuildArtifacts {
    param([string]$ProjectRoot)

    Write-Step "Cleaning build artifacts"

    $buildDirs = @(
        "build", "build-Debug", "build-Release", "build-MinSizeRel", "build-RelWithDebInfo",
        "build-test", "build-check", "build-verify", "build-ci", "build-coverage",
        "out", "cmake-build-debug", "cmake-build-release", "_build",
        "Debug", "Release", "x64", "Win32"
    )

    $buildFiles = @(
        "CMakeCache.txt", "cmake_install.cmake", "Makefile",
        "*.vcxproj", "*.vcxproj.filters", "*.vcxproj.user", "*.sln"
    )

    $results = @()
    $totalSize = 0

    # Remove build directories
    foreach ($dir in $buildDirs) {
        $fullPath = Join-Path $ProjectRoot $dir
        $result = Remove-ItemSafely -Path $fullPath -Description "Build directory: $dir" -IsDirectory $true
        $results += $result
        $totalSize += $result.Size
    }

    # Remove build files
    foreach ($pattern in $buildFiles) {
        $files = Get-ChildItem -Path $ProjectRoot -Filter $pattern -File -ErrorAction SilentlyContinue
        foreach ($file in $files) {
            $result = Remove-ItemSafely -Path $file.FullName -Description "Build file: $($file.Name)" -IsDirectory $false
            $results += $result
            $totalSize += $result.Size
        }
    }

    # Remove CMakeFiles directories recursively
    $cmakeFilesDirs = Get-ChildItem -Path $ProjectRoot -Name "CMakeFiles" -Directory -Recurse -ErrorAction SilentlyContinue
    foreach ($dir in $cmakeFilesDirs) {
        $fullPath = Join-Path $ProjectRoot $dir
        $result = Remove-ItemSafely -Path $fullPath -Description "CMakeFiles directory: $dir" -IsDirectory $true
        $results += $result
        $totalSize += $result.Size
    }

    $removedCount = ($results | Where-Object { $_.Removed }).Count
    $totalCount = $results.Count

    if ($totalCount -gt 0) {
        Write-Success "Build cleanup: $removedCount/$totalCount items removed ($(Format-FileSize $totalSize) freed)"
    }
    else {
        Write-Success "Build cleanup: No build artifacts found"
    }

    return @{ Count = $removedCount; Size = $totalSize }
}

function Clean-Documentation {
    param([string]$ProjectRoot)

    Write-Step "Cleaning documentation artifacts"

    $docDirs = @(
        "docs/api/html", "docs/api/xml", "docs/api/latex", "docs/api/man",
        "docs/sphinx/_build", "docs/sphinx/_static", "docs/sphinx/_templates",
        "docs/doxygen/html", "docs/doxygen/xml", "docs/doxygen/latex",
        "html", "latex", "xml"
    )

    $docFiles = @(
        "docs/api/Doxyfile.bak", "docs/sphinx/conf.pyc",
        "*.tag", "doxygen_warnings.txt"
    )

    $results = @()
    $totalSize = 0

    # Remove documentation directories
    foreach ($dir in $docDirs) {
        $fullPath = Join-Path $ProjectRoot $dir
        $result = Remove-ItemSafely -Path $fullPath -Description "Documentation directory: $dir" -IsDirectory $true
        $results += $result
        $totalSize += $result.Size
    }

    # Remove documentation files
    foreach ($pattern in $docFiles) {
        $files = Get-ChildItem -Path $ProjectRoot -Filter $pattern -File -Recurse -ErrorAction SilentlyContinue
        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($ProjectRoot.Length + 1)
            $result = Remove-ItemSafely -Path $file.FullName -Description "Documentation file: $relativePath" -IsDirectory $false
            $results += $result
            $totalSize += $result.Size
        }
    }

    $removedCount = ($results | Where-Object { $_.Removed }).Count
    $totalCount = $results.Count

    if ($totalCount -gt 0) {
        Write-Success "Documentation cleanup: $removedCount/$totalCount items removed ($(Format-FileSize $totalSize) freed)"
    }
    else {
        Write-Success "Documentation cleanup: No documentation artifacts found"
    }

    return @{ Count = $removedCount; Size = $totalSize }
}

function Clean-TestArtifacts {
    param([string]$ProjectRoot)

    Write-Step "Cleaning test artifacts"

    $testDirs = @(
        "Testing", "test-results", "coverage", "gcov-reports",
        ".coverage", "htmlcov", "__pycache__"
    )

    $testFiles = @(
        "test_results.xml", "test_report.html", "coverage.xml", "coverage.info",
        "*.gcda", "*.gcno", "*.gcov", "*.profdata", "*.profraw",
        "gtest_output.xml", "ctest_output.xml", "junit.xml"
    )

    $results = @()
    $totalSize = 0

    # Remove test directories
    foreach ($dir in $testDirs) {
        $fullPath = Join-Path $ProjectRoot $dir
        $result = Remove-ItemSafely -Path $fullPath -Description "Test directory: $dir" -IsDirectory $true
        $results += $result
        $totalSize += $result.Size
    }

    # Remove test files
    foreach ($pattern in $testFiles) {
        $files = Get-ChildItem -Path $ProjectRoot -Filter $pattern -File -Recurse -ErrorAction SilentlyContinue
        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($ProjectRoot.Length + 1)
            $result = Remove-ItemSafely -Path $file.FullName -Description "Test file: $relativePath" -IsDirectory $false
            $results += $result
            $totalSize += $result.Size
        }
    }

    $removedCount = ($results | Where-Object { $_.Removed }).Count
    $totalCount = $results.Count

    if ($totalCount -gt 0) {
        Write-Success "Test cleanup: $removedCount/$totalCount items removed ($(Format-FileSize $totalSize) freed)"
    }
    else {
        Write-Success "Test cleanup: No test artifacts found"
    }

    return @{ Count = $removedCount; Size = $totalSize }
}

function Clean-CacheFiles {
    param([string]$ProjectRoot)

    Write-Step "Cleaning cache and temporary files"

    $cacheDirs = @(
        ".vscode/.browse.c_cpp.db*", ".vscode/ipch",
        ".vs", ".idea", "*.tmp", "*.temp",
        "node_modules", ".git/objects/tmp*"
    )

    $cacheFiles = @(
        "*.tmp", "*.temp", "*.bak", "*.swp", "*.swo", "*~",
        ".DS_Store", "Thumbs.db", "desktop.ini",
        "*.log", "*.pid", "*.lock"
    )

    $results = @()
    $totalSize = 0

    # Remove cache directories
    foreach ($pattern in $cacheDirs) {
        if ($pattern.Contains("*")) {
            # Handle wildcard patterns
            $dirs = Get-ChildItem -Path $ProjectRoot -Filter $pattern -Directory -Recurse -ErrorAction SilentlyContinue
            foreach ($dir in $dirs) {
                $relativePath = $dir.FullName.Substring($ProjectRoot.Length + 1)
                $result = Remove-ItemSafely -Path $dir.FullName -Description "Cache directory: $relativePath" -IsDirectory $true
                $results += $result
                $totalSize += $result.Size
            }
        }
        else {
            $fullPath = Join-Path $ProjectRoot $pattern
            $result = Remove-ItemSafely -Path $fullPath -Description "Cache directory: $pattern" -IsDirectory $true
            $results += $result
            $totalSize += $result.Size
        }
    }

    # Remove cache files
    foreach ($pattern in $cacheFiles) {
        $files = Get-ChildItem -Path $ProjectRoot -Filter $pattern -File -Recurse -ErrorAction SilentlyContinue
        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($ProjectRoot.Length + 1)
            $result = Remove-ItemSafely -Path $file.FullName -Description "Cache file: $relativePath" -IsDirectory $false
            $results += $result
            $totalSize += $result.Size
        }
    }

    $removedCount = ($results | Where-Object { $_.Removed }).Count
    $totalCount = $results.Count

    if ($totalCount -gt 0) {
        Write-Success "Cache cleanup: $removedCount/$totalCount items removed ($(Format-FileSize $totalSize) freed)"
    }
    else {
        Write-Success "Cache cleanup: No cache files found"
    }

    return @{ Count = $removedCount; Size = $totalSize }
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    $projectRoot = Get-ProjectRoot

    # Determine what to clean
    $cleanBuild = $Build -or (!$All -and !$Docs -and !$Tests -and !$Cache)
    $cleanDocs = $Docs -or $All
    $cleanTests = $Tests -or $All
    $cleanCache = $Cache -or $All

    Write-Header "Nexus Clean Script - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Project Root: $projectRoot"
    Write-Host "  Clean Build: $(if ($cleanBuild) { 'Yes' } else { 'No' })"
    Write-Host "  Clean Documentation: $(if ($cleanDocs) { 'Yes' } else { 'No' })"
    Write-Host "  Clean Tests: $(if ($cleanTests) { 'Yes' } else { 'No' })"
    Write-Host "  Clean Cache: $(if ($cleanCache) { 'Yes' } else { 'No' })"
    Write-Host "  Dry Run: $(if ($DryRun) { 'Yes' } else { 'No' })"
    Write-Host "  Verbose Output: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Confirmation for destructive operations
    if (!$DryRun -and !$Force -and ($All -or ($cleanBuild -and $cleanDocs -and $cleanTests))) {
        Write-Host ""
        Write-Warning "This will remove build artifacts, documentation, and test files."
        $response = Read-Host "Continue? (y/N)"
        if ($response -notmatch '^[Yy]') {
            Write-Host "Operation cancelled." -ForegroundColor Yellow
            return 0
        }
    }

    # Execute cleaning operations
    $totalResults = @{ Count = 0; Size = 0 }

    if ($cleanBuild) {
        $result = Clean-BuildArtifacts -ProjectRoot $projectRoot
        $totalResults.Count += $result.Count
        $totalResults.Size += $result.Size
    }

    if ($cleanDocs) {
        $result = Clean-Documentation -ProjectRoot $projectRoot
        $totalResults.Count += $result.Count
        $totalResults.Size += $result.Size
    }

    if ($cleanTests) {
        $result = Clean-TestArtifacts -ProjectRoot $projectRoot
        $totalResults.Count += $result.Count
        $totalResults.Size += $result.Size
    }

    if ($cleanCache) {
        $result = Clean-CacheFiles -ProjectRoot $projectRoot
        $totalResults.Count += $result.Count
        $totalResults.Size += $result.Size
    }

    Write-Header "Clean Complete"

    if ($DryRun) {
        Write-Success "Dry run completed - no files were actually removed"
        Write-Host "Would have removed $($totalResults.Count) items ($(Format-FileSize $totalResults.Size))" -ForegroundColor Cyan
    }
    else {
        Write-Success "Clean operation completed successfully!"
        Write-Host "Removed $($totalResults.Count) items, freed $(Format-FileSize $totalResults.Size)" -ForegroundColor Cyan
    }

    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  Build project: .\scripts\building\build.ps1"
    Write-Host "  Run tests: .\scripts\test\test.ps1"
    Write-Host "  Format code: .\scripts\tools\format.ps1"

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
