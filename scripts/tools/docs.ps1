#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Documentation Generator - PowerShell Version

.DESCRIPTION
    Cross-platform documentation generation script for Nexus embedded development platform.
    Supports Doxygen API documentation and Sphinx user documentation.

.PARAMETER Target
    Documentation target: doxygen, sphinx, or all (default)

.PARAMETER Verbose
    Enable verbose output showing detailed generation process

.PARAMETER Clean
    Clean existing documentation before generating new content

.PARAMETER Open
    Open generated documentation in default browser after completion

.PARAMETER Format
    Output format for Doxygen: html (default), latex, xml, man

.PARAMETER Theme
    Sphinx theme: rtd (default), alabaster, classic, nature

.PARAMETER Language
    Documentation language: en (default), zh, fr, de, es

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\docs.ps1
    Generate all documentation with default settings

.EXAMPLE
    .\docs.ps1 -Target doxygen -Format html -Open
    Generate Doxygen HTML documentation and open in browser

.EXAMPLE
    .\docs.ps1 -Target sphinx -Theme rtd -Clean -Verbose
    Generate Sphinx documentation with RTD theme, clean first, verbose output

.EXAMPLE
    .\docs.ps1 -All -Language zh
    Generate all documentation in Chinese
#>

[CmdletBinding()]
param(
    [ValidateSet("doxygen", "sphinx", "all")]
    [string]$Target = "all",

    [switch]$Verbose,
    [switch]$Clean,
    [switch]$Open,

    [ValidateSet("html", "latex", "xml", "man")]
    [string]$Format = "html",

    [ValidateSet("rtd", "alabaster", "classic", "nature")]
    [string]$Theme = "rtd",

    [ValidateSet("en", "zh", "fr", "de", "es")]
    [string]$Language = "en",

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
Nexus Documentation Generator - PowerShell Version

Usage: .\docs.ps1 [options]

Options:
  -Target <target>      Documentation target: doxygen, sphinx, all (default)
  -Verbose              Enable verbose output
  -Clean                Clean existing documentation before generating
  -Open                 Open generated documentation in browser
  -Format <format>      Doxygen output format: html (default), latex, xml, man
  -Theme <theme>        Sphinx theme: rtd (default), alabaster, classic, nature
  -Language <lang>      Documentation language: en (default), zh, fr, de, es
  -Help                 Show this help information

Examples:
  .\docs.ps1                              # Generate all documentation
  .\docs.ps1 -Target doxygen -Open        # Generate Doxygen docs and open
  .\docs.ps1 -Target sphinx -Theme rtd    # Generate Sphinx with RTD theme
  .\docs.ps1 -Clean -Verbose              # Clean and generate with verbose output

Documentation Types:
  doxygen   - API documentation from source code comments
  sphinx    - User documentation from reStructuredText files
  all       - Both Doxygen and Sphinx documentation

Output Formats (Doxygen):
  html      - HTML documentation (default, recommended)
  latex     - LaTeX documentation for PDF generation
  xml       - XML output for further processing
  man       - Unix manual pages

Themes (Sphinx):
  rtd       - Read the Docs theme (default, recommended)
  alabaster - Clean, minimal theme
  classic   - Traditional Sphinx theme
  nature    - Nature-inspired theme

Prerequisites:
  - Doxygen: Install from https://doxygen.nl/
  - Sphinx: pip install sphinx sphinx-rtd-theme
  - LaTeX: For PDF generation (optional)

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Test-Prerequisites {
    Write-Step "Checking documentation prerequisites"

    $missing = @()

    # Check Doxygen
    if ($Target -in @("doxygen", "all")) {
        if (!(Test-Command "doxygen")) {
            $missing += "doxygen"
            Write-Warning "Doxygen not found"
        }
        else {
            $version = & doxygen --version 2>$null
            Write-Success "Doxygen available: $version"
        }
    }

    # Check Sphinx
    if ($Target -in @("sphinx", "all")) {
        if (!(Test-Command "sphinx-build")) {
            $missing += "sphinx-build"
            Write-Warning "Sphinx not found"
        }
        else {
            $version = & sphinx-build --version 2>$null
            Write-Success "Sphinx available: $version"
        }

        # Check Python and pip
        if (Test-Command "python") {
            try {
                $result = & python -c "import sphinx, sphinx_rtd_theme; print('Sphinx modules OK')" 2>$null
                if ($LASTEXITCODE -eq 0) {
                    Write-Success "Sphinx Python modules available"
                }
                else {
                    Write-Warning "Sphinx Python modules missing"
                    $missing += "sphinx-python-modules"
                }
            }
            catch {
                Write-Warning "Failed to check Sphinx Python modules"
            }
        }
    }

    if ($missing.Count -gt 0) {
        Write-Error "Missing prerequisites: $($missing -join ', ')"
        Write-Host ""
        Write-Host "Installation instructions:" -ForegroundColor Yellow

        if ("doxygen" -in $missing) {
            Write-Host "  Doxygen:" -ForegroundColor Cyan
            Write-Host "    Windows: winget install doxygen.doxygen" -ForegroundColor Gray
            Write-Host "    Windows: scoop install doxygen" -ForegroundColor Gray
            Write-Host "    Linux:   sudo apt-get install doxygen" -ForegroundColor Gray
            Write-Host "    macOS:   brew install doxygen" -ForegroundColor Gray
        }

        if ("sphinx-build" -in $missing -or "sphinx-python-modules" -in $missing) {
            Write-Host "  Sphinx:" -ForegroundColor Cyan
            Write-Host "    pip install sphinx sphinx-rtd-theme breathe" -ForegroundColor Gray
        }

        return $false
    }

    return $true
}

function New-DoxygenConfig {
    param([string]$ProjectRoot, [string]$OutputFormat)

    $doxyfilePath = Join-Path $ProjectRoot "Doxyfile"

    if (!(Test-Path $doxyfilePath)) {
        Write-Warning "Doxyfile not found, creating default configuration"

        # Create basic Doxyfile
        $doxyfileContent = @"
# Doxyfile for Nexus Project
PROJECT_NAME           = "Nexus Embedded Platform"
PROJECT_NUMBER         = "1.0.0"
PROJECT_BRIEF          = "Cross-platform embedded development framework"

OUTPUT_DIRECTORY       = docs/api
CREATE_SUBDIRS         = NO

INPUT                  = hal osal platforms applications
RECURSIVE              = YES
FILE_PATTERNS          = *.c *.h *.cpp *.hpp

GENERATE_HTML          = $(if ($OutputFormat -eq "html") { "YES" } else { "NO" })
GENERATE_LATEX         = $(if ($OutputFormat -eq "latex") { "YES" } else { "NO" })
GENERATE_XML           = $(if ($OutputFormat -eq "xml") { "YES" } else { "NO" })
GENERATE_MAN           = $(if ($OutputFormat -eq "man") { "YES" } else { "NO" })

HTML_OUTPUT            = html
LATEX_OUTPUT           = latex
XML_OUTPUT             = xml
MAN_OUTPUT             = man

EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES

SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO

GENERATE_TREEVIEW      = YES
USE_MATHJAX            = YES

QUIET                  = $(if ($Verbose) { "NO" } else { "YES" })
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_DOC_ERROR      = YES
"@

        Set-Content -Path $doxyfilePath -Value $doxyfileContent -Encoding UTF8
        Write-Success "Created default Doxyfile"
    }

    return $doxyfilePath
}

function Invoke-DoxygenGeneration {
    param([string]$ProjectRoot, [string]$OutputFormat)

    Write-Step "Generating Doxygen documentation"

    # Ensure Doxyfile exists
    $doxyfilePath = New-DoxygenConfig -ProjectRoot $ProjectRoot -OutputFormat $OutputFormat

    # Clean output directory if requested
    if ($Clean) {
        $outputDir = Join-Path $ProjectRoot "docs/api"
        if (Test-Path $outputDir) {
            Write-Verbose "Cleaning Doxygen output directory: $outputDir"
            Remove-Item -Path $outputDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    try {
        $startTime = Get-Date
        Push-Location $ProjectRoot

        if ($Verbose) {
            $result = & doxygen $doxyfilePath 2>&1
            Write-Verbose "Doxygen output: $result"
        }
        else {
            $result = & doxygen $doxyfilePath 2>&1 | Out-Null
        }

        $success = $LASTEXITCODE -eq 0
        $duration = (Get-Date) - $startTime

        if ($success) {
            Write-Success "Doxygen documentation generated in $($duration.TotalSeconds.ToString('F1'))s"

            # Check output
            $outputPath = Join-Path $ProjectRoot "docs/api/$OutputFormat"
            if (Test-Path $outputPath) {
                $fileCount = (Get-ChildItem -Path $outputPath -Recurse -File).Count
                Write-Host "  Output: $outputPath ($fileCount files)" -ForegroundColor Gray

                if ($Open -and $OutputFormat -eq "html") {
                    $indexPath = Join-Path $outputPath "index.html"
                    if (Test-Path $indexPath) {
                        Write-Host "  Opening: $indexPath" -ForegroundColor Gray
                        Start-Process $indexPath
                    }
                }

                return $outputPath
            }
            else {
                Write-Warning "Doxygen output directory not found: $outputPath"
                return $null
            }
        }
        else {
            Write-Error "Doxygen generation failed"
            if ($result) {
                Write-Host "Doxygen error output:" -ForegroundColor Red
                Write-Host $result -ForegroundColor Red
            }
            return $null
        }
    }
    catch {
        Write-Error "Doxygen generation error: $_"
        return $null
    }
    finally {
        Pop-Location
    }
}

function Invoke-SphinxGeneration {
    param([string]$ProjectRoot, [string]$Theme)

    Write-Step "Generating Sphinx documentation"

    $sphinxDir = Join-Path $ProjectRoot "docs/sphinx"
    $outputDir = Join-Path $sphinxDir "_build/html"

    # Check if Sphinx source directory exists
    if (!(Test-Path $sphinxDir)) {
        Write-Warning "Sphinx source directory not found: $sphinxDir"
        Write-Host "Creating basic Sphinx configuration..." -ForegroundColor Yellow

        # Create basic Sphinx structure
        New-Item -ItemType Directory -Path $sphinxDir -Force | Out-Null

        # Create conf.py
        $confPy = @"
# Configuration file for Sphinx documentation builder
import os
import sys

# Project information
project = 'Nexus Embedded Platform'
copyright = '2024, Nexus Team'
author = 'Nexus Team'
version = '1.0.0'
release = '1.0.0'

# Extensions
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'sphinx_rtd_theme',
]

# Theme
html_theme = '$Theme'
if html_theme == 'rtd':
    html_theme = 'sphinx_rtd_theme'

# HTML options
html_static_path = ['_static']
html_css_files = []

# Language
language = '$Language'

# Master document
master_doc = 'index'
"@
        Set-Content -Path (Join-Path $sphinxDir "conf.py") -Value $confPy -Encoding UTF8

        # Create index.rst
        $indexRst = @"
Nexus Embedded Platform Documentation
====================================

Welcome to the Nexus Embedded Platform documentation.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   getting-started
   api-reference
   examples

Getting Started
===============

This is the main documentation for the Nexus embedded development platform.

API Reference
=============

For detailed API documentation, see the Doxygen-generated documentation.

Examples
========

Example code and tutorials will be added here.

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
"@
        Set-Content -Path (Join-Path $sphinxDir "index.rst") -Value $indexRst -Encoding UTF8

        Write-Success "Created basic Sphinx configuration"
    }

    # Clean output directory if requested
    if ($Clean -and (Test-Path $outputDir)) {
        Write-Verbose "Cleaning Sphinx output directory: $outputDir"
        Remove-Item -Path $outputDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    try {
        $startTime = Get-Date

        $sphinxArgs = @(
            "-b", "html"
            "-D", "html_theme=$Theme"
            "-D", "language=$Language"
            ".", "_build/html"
        )

        if (!$Verbose) {
            $sphinxArgs = @("-q") + $sphinxArgs
        }

        Write-Verbose "Sphinx command: sphinx-build $($sphinxArgs -join ' ')"

        Push-Location $sphinxDir

        if ($Verbose) {
            $result = & sphinx-build @sphinxArgs 2>&1
            Write-Verbose "Sphinx output: $result"
        }
        else {
            $result = & sphinx-build @sphinxArgs 2>&1 | Out-Null
        }

        $success = $LASTEXITCODE -eq 0
        $duration = (Get-Date) - $startTime

        if ($success) {
            Write-Success "Sphinx documentation generated in $($duration.TotalSeconds.ToString('F1'))s"

            # Check output
            if (Test-Path $outputDir) {
                $fileCount = (Get-ChildItem -Path $outputDir -Recurse -File).Count
                Write-Host "  Output: $outputDir ($fileCount files)" -ForegroundColor Gray

                if ($Open) {
                    $indexPath = Join-Path $outputDir "index.html"
                    if (Test-Path $indexPath) {
                        Write-Host "  Opening: $indexPath" -ForegroundColor Gray
                        Start-Process $indexPath
                    }
                }

                return $outputDir
            }
            else {
                Write-Warning "Sphinx output directory not found: $outputDir"
                return $null
            }
        }
        else {
            Write-Error "Sphinx generation failed"
            if ($result) {
                Write-Host "Sphinx error output:" -ForegroundColor Red
                Write-Host $result -ForegroundColor Red
            }
            return $null
        }
    }
    catch {
        Write-Error "Sphinx generation error: $_"
        return $null
    }
    finally {
        Pop-Location
    }
}

function Get-DocumentationStats {
    param([string[]]$OutputPaths)

    Write-Step "Analyzing documentation statistics"

    $totalFiles = 0
    $totalSize = 0

    foreach ($path in $OutputPaths) {
        if ($path -and (Test-Path $path)) {
            $files = Get-ChildItem -Path $path -Recurse -File
            $size = ($files | Measure-Object -Property Length -Sum).Sum

            $pathName = Split-Path $path -Leaf
            $fileCount = $files.Count
            $sizeFormatted = if ($size -lt 1MB) { "$([math]::Round($size/1KB, 1)) KB" } else { "$([math]::Round($size/1MB, 1)) MB" }

            Write-Host "  $pathName : $fileCount files, $sizeFormatted" -ForegroundColor Gray

            $totalFiles += $fileCount
            $totalSize += $size
        }
    }

    $totalSizeFormatted = if ($totalSize -lt 1MB) { "$([math]::Round($totalSize/1KB, 1)) KB" } else { "$([math]::Round($totalSize/1MB, 1)) MB" }
    Write-Success "Total documentation: $totalFiles files, $totalSizeFormatted"
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    # Check prerequisites
    if (!(Test-Prerequisites)) {
        return 1
    }

    $projectRoot = Get-ProjectRoot

    Write-Header "Nexus Documentation Generator - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Project Root: $projectRoot"
    Write-Host "  Target: $Target"
    Write-Host "  Format: $Format"
    Write-Host "  Theme: $Theme"
    Write-Host "  Language: $Language"
    Write-Host "  Clean: $(if ($Clean) { 'Yes' } else { 'No' })"
    Write-Host "  Open: $(if ($Open) { 'Yes' } else { 'No' })"
    Write-Host "  Verbose Output: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Generate documentation
    $outputPaths = @()
    $success = $true

    if ($Target -in @("doxygen", "all")) {
        $doxygenOutput = Invoke-DoxygenGeneration -ProjectRoot $projectRoot -OutputFormat $Format
        if ($doxygenOutput) {
            $outputPaths += $doxygenOutput
        }
        else {
            $success = $false
        }
    }

    if ($Target -in @("sphinx", "all")) {
        $sphinxOutput = Invoke-SphinxGeneration -ProjectRoot $projectRoot -Theme $Theme
        if ($sphinxOutput) {
            $outputPaths += $sphinxOutput
        }
        else {
            $success = $false
        }
    }

    # Show statistics
    if ($outputPaths.Count -gt 0) {
        Get-DocumentationStats -OutputPaths $outputPaths
    }

    Write-Header "Documentation Generation Complete"

    if ($success) {
        Write-Success "Documentation generated successfully!"

        Write-Host ""
        Write-Host "Generated documentation:" -ForegroundColor Cyan
        foreach ($path in $outputPaths) {
            if ($path) {
                Write-Host "  - $path" -ForegroundColor Gray
            }
        }

        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  View documentation in browser"
        Write-Host "  Deploy to web server or documentation hosting"
        Write-Host "  Update documentation content and regenerate"
    }
    else {
        Write-Error "Documentation generation had errors!"

        Write-Host ""
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  Check prerequisites: .\docs.ps1 -Help"
        Write-Host "  Run with verbose output: .\docs.ps1 -Verbose"
        Write-Host "  Clean and retry: .\docs.ps1 -Clean"
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
