#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Code Formatter - PowerShell Version

.DESCRIPTION
    Cross-platform code formatting script using clang-format.
    Supports checking and formatting C/C++ source files with customizable options.

.PARAMETER Check
    Check only mode - don't modify files, just report formatting issues

.PARAMETER Verbose
    Enable verbose output showing each file being processed

.PARAMETER Extensions
    File extensions to format (default: .c,.h,.cpp,.hpp)

.PARAMETER Directories
    Directories to scan for source files (default: hal,osal,platforms,tests,applications)

.PARAMETER Exclude
    Patterns to exclude from formatting (supports wildcards)

.PARAMETER Style
    Clang-format style: file (default), llvm, google, chromium, mozilla, webkit

.PARAMETER Parallel
    Enable parallel processing of files

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\format.ps1
    Format all source files using .clang-format configuration

.EXAMPLE
    .\format.ps1 -Check -Verbose
    Check formatting without modifying files, with verbose output

.EXAMPLE
    .\format.ps1 -Style google -Directories "hal,osal"
    Format only hal and osal directories using Google style

.EXAMPLE
    .\format.ps1 -Exclude "*test*,*example*" -Parallel
    Format files excluding test and example files, using parallel processing
#>

[CmdletBinding()]
param(
    [switch]$Check,
    [switch]$Verbose,
    [string]$Extensions = ".c,.h,.cpp,.hpp",
    [string]$Directories = "hal,osal,platforms,tests,applications",
    [string[]]$Exclude = @(),

    [ValidateSet("file", "llvm", "google", "chromium", "mozilla", "webkit")]
    [string]$Style = "file",

    [switch]$Parallel,
    [switch]$Help
)

# Set verbose preference
if ($Verbose) {
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

function Get-ProjectRoot {
    return (Get-Item $PSScriptRoot).Parent.Parent.FullName
}

function Show-Help {
    Write-Host @"
Nexus Code Formatter - PowerShell Version

Usage: .\format.ps1 [options]

Options:
  -Check                Check only mode (don't modify files)
  -Verbose              Enable verbose output
  -Extensions <list>    File extensions to format (default: .c,.h,.cpp,.hpp)
  -Directories <list>   Directories to scan (default: hal,osal,platforms,tests,applications)
  -Exclude <patterns>   Patterns to exclude from formatting
  -Style <style>        Clang-format style: file (default), llvm, google, chromium, mozilla, webkit
  -Parallel             Enable parallel processing
  -Help                 Show this help information

Examples:
  .\format.ps1                              # Format all source files
  .\format.ps1 -Check -Verbose              # Check formatting with verbose output
  .\format.ps1 -Style google                # Use Google coding style
  .\format.ps1 -Exclude "*test*" -Parallel  # Exclude test files, use parallel processing

Clang-Format Styles:
  file      - Use .clang-format file in project root (recommended)
  llvm      - LLVM coding standards
  google    - Google C++ Style Guide
  chromium  - Chromium coding style
  mozilla   - Mozilla coding style
  webkit    - WebKit coding style

File Extensions:
  The script processes C/C++ source files by default. You can customize
  the extensions using the -Extensions parameter.

Exclusion Patterns:
  Use wildcards to exclude files or directories:
  -Exclude "*test*,*example*,vendor/*"

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Test-ClangFormat {
    Write-Step "Checking clang-format availability"

    if (!(Test-Command "clang-format")) {
        Write-Error "clang-format not found!"
        Write-Host ""
        Write-Host "Installation options:" -ForegroundColor Yellow
        Write-Host "  Windows: winget install LLVM.LLVM" -ForegroundColor Cyan
        Write-Host "  Windows: scoop install llvm" -ForegroundColor Cyan
        Write-Host "  Linux:   sudo apt-get install clang-format" -ForegroundColor Cyan
        Write-Host "  macOS:   brew install clang-format" -ForegroundColor Cyan
        return $false
    }

    try {
        $version = & clang-format --version 2>$null
        Write-Success "clang-format available: $version"
        Write-Verbose "clang-format version: $version"
        return $true
    }
    catch {
        Write-Error "Failed to get clang-format version: $_"
        return $false
    }
}

function Find-SourceFiles {
    param(
        [string]$ProjectRoot,
        [string[]]$Extensions,
        [string[]]$Directories,
        [string[]]$ExcludePatterns
    )

    Write-Step "Scanning for source files"

    $allFiles = @()

    foreach ($dir in $Directories) {
        $dirPath = Join-Path $ProjectRoot $dir
        if (Test-Path $dirPath) {
            Write-Verbose "Scanning directory: $dirPath"

            foreach ($ext in $Extensions) {
                $pattern = "*$ext"
                $files = Get-ChildItem -Path $dirPath -Filter $pattern -Recurse -File
                $allFiles += $files
                Write-Verbose "Found $($files.Count) files with extension $ext in $dir"
            }
        }
        else {
            Write-Verbose "Directory not found: $dirPath"
        }
    }

    # Apply exclusion patterns
    if ($ExcludePatterns.Count -gt 0) {
        Write-Verbose "Applying exclusion patterns: $($ExcludePatterns -join ', ')"
        $filteredFiles = @()

        foreach ($file in $allFiles) {
            $excluded = $false
            $relativePath = $file.FullName.Substring($ProjectRoot.Length + 1)

            foreach ($pattern in $ExcludePatterns) {
                if ($relativePath -like $pattern) {
                    Write-Verbose "Excluding file: $relativePath (matches pattern: $pattern)"
                    $excluded = $true
                    break
                }
            }

            if (!$excluded) {
                $filteredFiles += $file
            }
        }

        $allFiles = $filteredFiles
        Write-Verbose "After exclusion: $($allFiles.Count) files remaining"
    }

    Write-Success "Found $($allFiles.Count) source files to process"

    if ($Verbose -and $allFiles.Count -le 20) {
        Write-Host "Files to process:" -ForegroundColor Gray
        foreach ($file in $allFiles) {
            $relativePath = $file.FullName.Substring($ProjectRoot.Length + 1)
            Write-Host "  - $relativePath" -ForegroundColor Gray
        }
    }
    elseif ($allFiles.Count -gt 20) {
        Write-Host "Files to process: $($allFiles.Count) (use -Verbose to see full list)" -ForegroundColor Gray
    }

    return $allFiles
}

function Format-File {
    param(
        [System.IO.FileInfo]$File,
        [bool]$CheckOnly,
        [string]$Style,
        [string]$ProjectRoot
    )

    $relativePath = $File.FullName.Substring($ProjectRoot.Length + 1)

    try {
        if ($CheckOnly) {
            # Check mode: use --dry-run and --Werror
            $result = & clang-format --dry-run --Werror "--style=$Style" $File.FullName 2>&1
            $success = $LASTEXITCODE -eq 0

            if ($success) {
                Write-Verbose "✓ $relativePath (properly formatted)"
                return @{ Success = $true; File = $relativePath; Message = "OK" }
            }
            else {
                Write-Verbose "✗ $relativePath (formatting issues)"
                return @{ Success = $false; File = $relativePath; Message = "Formatting issues" }
            }
        }
        else {
            # Format mode: modify files in place
            $result = & clang-format -i "--style=$Style" $File.FullName 2>&1
            $success = $LASTEXITCODE -eq 0

            if ($success) {
                Write-Verbose "✓ $relativePath (formatted)"
                return @{ Success = $true; File = $relativePath; Message = "Formatted" }
            }
            else {
                Write-Verbose "✗ $relativePath (format failed)"
                return @{ Success = $false; File = $relativePath; Message = "Format failed: $result" }
            }
        }
    }
    catch {
        Write-Verbose "✗ $relativePath (error: $_)"
        return @{ Success = $false; File = $relativePath; Message = "Error: $_" }
    }
}

function Process-Files {
    param(
        [System.IO.FileInfo[]]$Files,
        [bool]$CheckOnly,
        [string]$Style,
        [string]$ProjectRoot,
        [bool]$UseParallel
    )

    Write-Step "$(if ($CheckOnly) { 'Checking' } else { 'Formatting' }) source files"

    $results = @()
    $startTime = Get-Date

    if ($UseParallel -and $Files.Count -gt 10) {
        Write-Host "Processing $($Files.Count) files in parallel..." -ForegroundColor Gray

        # PowerShell parallel processing
        $results = $Files | ForEach-Object -Parallel {
            $file = $_
            $checkOnly = $using:CheckOnly
            $style = $using:Style
            $projectRoot = $using:ProjectRoot

            # Import the Format-File function in parallel context
            function Format-File {
                param(
                    [System.IO.FileInfo]$File,
                    [bool]$CheckOnly,
                    [string]$Style,
                    [string]$ProjectRoot
                )

                $relativePath = $File.FullName.Substring($ProjectRoot.Length + 1)

                try {
                    if ($CheckOnly) {
                        $result = & clang-format --dry-run --Werror "--style=$Style" $File.FullName 2>&1
                        $success = $LASTEXITCODE -eq 0

                        if ($success) {
                            return @{ Success = $true; File = $relativePath; Message = "OK" }
                        }
                        else {
                            return @{ Success = $false; File = $relativePath; Message = "Formatting issues" }
                        }
                    }
                    else {
                        $result = & clang-format -i "--style=$Style" $File.FullName 2>&1
                        $success = $LASTEXITCODE -eq 0

                        if ($success) {
                            return @{ Success = $true; File = $relativePath; Message = "Formatted" }
                        }
                        else {
                            return @{ Success = $false; File = $relativePath; Message = "Format failed: $result" }
                        }
                    }
                }
                catch {
                    return @{ Success = $false; File = $relativePath; Message = "Error: $_" }
                }
            }

            Format-File -File $file -CheckOnly $checkOnly -Style $style -ProjectRoot $projectRoot
        } -ThrottleLimit 8
    }
    else {
        Write-Host "Processing $($Files.Count) files sequentially..." -ForegroundColor Gray

        $processed = 0
        foreach ($file in $Files) {
            $processed++
            if ($processed % 10 -eq 0 -or $processed -eq $Files.Count) {
                Write-Host "Progress: $processed/$($Files.Count) files processed" -ForegroundColor Gray
            }

            $result = Format-File -File $file -CheckOnly $CheckOnly -Style $Style -ProjectRoot $ProjectRoot
            $results += $result
        }
    }

    $duration = (Get-Date) - $startTime

    # Analyze results
    $successCount = ($results | Where-Object { $_.Success }).Count
    $failureCount = $results.Count - $successCount

    Write-Host ""
    Write-Host "Processing Summary:" -ForegroundColor Cyan
    Write-Host "  Total files: $($results.Count)"
    Write-Host "  Successful: $successCount"
    Write-Host "  Failed: $failureCount"
    Write-Host "  Duration: $($duration.TotalSeconds.ToString('F1'))s"
    Write-Host "  Rate: $([math]::Round($results.Count / $duration.TotalSeconds, 1)) files/sec"

    # Show failures
    if ($failureCount -gt 0) {
        Write-Host ""
        Write-Host "Failed files:" -ForegroundColor Red
        $failures = $results | Where-Object { !$_.Success }
        foreach ($failure in $failures | Select-Object -First 10) {
            Write-Host "  ✗ $($failure.File): $($failure.Message)" -ForegroundColor Red
        }
        if ($failures.Count -gt 10) {
            Write-Host "  ... and $($failures.Count - 10) more failures" -ForegroundColor Red
        }
    }

    return $failureCount -eq 0
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    # Check clang-format availability
    if (!(Test-ClangFormat)) {
        return 1
    }

    $projectRoot = Get-ProjectRoot
    $extensionList = $Extensions -split ',' | ForEach-Object { $_.Trim() }
    $directoryList = $Directories -split ',' | ForEach-Object { $_.Trim() }

    Write-Header "Nexus Code Formatter - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Project Root: $projectRoot"
    Write-Host "  Mode: $(if ($Check) { 'Check Only' } else { 'Format' })"
    Write-Host "  Style: $Style"
    Write-Host "  Extensions: $($extensionList -join ', ')"
    Write-Host "  Directories: $($directoryList -join ', ')"
    Write-Host "  Exclude Patterns: $(if ($Exclude.Count -gt 0) { $Exclude -join ', ' } else { 'None' })"
    Write-Host "  Parallel Processing: $(if ($Parallel) { 'Yes' } else { 'No' })"
    Write-Host "  Verbose Output: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Check for .clang-format file if using 'file' style
    if ($Style -eq "file") {
        $clangFormatFile = Join-Path $projectRoot ".clang-format"
        if (Test-Path $clangFormatFile) {
            Write-Success "Using .clang-format configuration file"
            Write-Verbose ".clang-format file found: $clangFormatFile"
        }
        else {
            Write-Warning ".clang-format file not found, using default style"
            Write-Host "Consider creating a .clang-format file for consistent formatting" -ForegroundColor Yellow
        }
    }

    # Find source files
    $sourceFiles = Find-SourceFiles -ProjectRoot $projectRoot -Extensions $extensionList -Directories $directoryList -ExcludePatterns $Exclude

    if ($sourceFiles.Count -eq 0) {
        Write-Warning "No source files found to process"
        return 0
    }

    # Process files
    $success = Process-Files -Files $sourceFiles -CheckOnly $Check -Style $Style -ProjectRoot $projectRoot -UseParallel $Parallel

    Write-Header "Format Results"

    if ($success) {
        if ($Check) {
            Write-Success "All files are properly formatted!"
        }
        else {
            Write-Success "Code formatting completed successfully!"
        }

        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  Run tests: .\scripts\test\test.ps1"
        Write-Host "  Build project: .\scripts\building\build.ps1"
        Write-Host "  Check other tools: .\scripts\tools\clean.ps1"
    }
    else {
        if ($Check) {
            Write-Error "Some files have formatting issues!"
            Write-Host ""
            Write-Host "To fix formatting issues:" -ForegroundColor Yellow
            Write-Host "  .\format.ps1" -ForegroundColor Cyan
        }
        else {
            Write-Error "Some files failed to format!"
            Write-Host ""
            Write-Host "Troubleshooting:" -ForegroundColor Yellow
            Write-Host "  Check clang-format version: clang-format --version"
            Write-Host "  Verify .clang-format file syntax"
            Write-Host "  Run with verbose output: .\format.ps1 -Verbose"
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
