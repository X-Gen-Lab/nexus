#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Nexus Environment Setup Script - PowerShell Version

.DESCRIPTION
    Cross-platform environment setup script for Nexus embedded development platform.
    Supports Windows (winget/scoop), Linux, and macOS with automatic package manager detection.

.PARAMETER Platform
    Target platform: native, stm32f4, all (default: native)

.PARAMETER Dev
    Install development tools (formatting, static analysis, etc.)

.PARAMETER Docs
    Install documentation generation tools

.PARAMETER Test
    Run environment verification tests

.PARAMETER PackageManager
    Windows package manager preference: auto, winget, scoop (default: auto)

.PARAMETER Verbose
    Enable verbose output

.PARAMETER Help
    Show this help information

.EXAMPLE
    .\setup.ps1
    Install basic environment

.EXAMPLE
    .\setup.ps1 -Platform stm32f4 -Dev
    Install STM32F4 development environment and dev tools

.EXAMPLE
    .\setup.ps1 -PackageManager scoop -Dev -Docs -Verbose
    Use Scoop package manager and install dev/docs tools with verbose output
#>

[CmdletBinding()]
param(
    [ValidateSet("native", "stm32f4", "all")]
    [string]$Platform = "native",

    [switch]$Dev,
    [switch]$Docs,
    [switch]$Test,

    [ValidateSet("auto", "winget", "scoop")]
    [string]$PackageManager = "auto",

    [switch]$Verbose,
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

function Show-Help {
    Write-Host @"
Nexus Environment Setup Script - PowerShell Version

Usage: .\setup.ps1 [options]

Options:
  -Platform <platform>      Target platform: native, stm32f4, all (default: native)
  -Dev                      Install development tools (formatting, static analysis, etc.)
  -Docs                     Install documentation generation tools
  -Test                     Run environment verification tests
  -PackageManager <manager> Windows package manager: auto, winget, scoop (default: auto)
  -Verbose                  Enable verbose output
  -Help                     Show this help information

Examples:
  .\setup.ps1                           # Install basic environment
  .\setup.ps1 -Platform stm32f4 -Dev    # Install STM32F4 development environment and dev tools
  .\setup.ps1 -PackageManager scoop -Dev -Docs -Verbose  # Use Scoop with verbose output

Package Manager Support:
  Windows: winget (default), scoop (alternative)
  Linux:   apt, yum, dnf, pacman (auto-detected)
  macOS:   homebrew (auto-installed if needed)

Scoop Installation:
  If you prefer Scoop over winget, install it first:
  Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
  irm get.scoop.sh | iex

For more information, visit: https://github.com/nexus-platform/nexus
"@
}

function Get-SystemInfo {
    Write-Step "Detecting system information"

    $osName = if ($IsWindows) { "Windows" } elseif ($IsLinux) { "Linux" } elseif ($IsMacOS) { "macOS" } else { "Unknown" }
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    $psVersion = $PSVersionTable.PSVersion

    Write-Host "Operating System: $osName"
    Write-Host "Architecture: $arch"
    Write-Host "PowerShell: $psVersion"

    if ($Verbose) {
        Write-Host "Platform Details: $([System.Runtime.InteropServices.RuntimeInformation]::OSDescription)"
        if ($env:NUMBER_OF_PROCESSORS) {
            Write-Host "Processor Count: $($env:NUMBER_OF_PROCESSORS)"
        }
        if ($env:USERNAME) {
            Write-Host "Username: $($env:USERNAME)"
        }
    }

    return @{
        OS = $osName
        Architecture = $arch
        PSVersion = $psVersion
    }
}

function Get-WindowsPackageManager {
    param([string]$Preference = "auto")

    Write-Step "Detecting Windows package managers"

    $available = @()

    # Check winget
    if (Test-Command "winget") {
        $available += "winget"
        try {
            $wingetVersion = (winget --version 2>$null) -replace 'v', ''
            Write-Host "√ winget available (version: $wingetVersion)" -ForegroundColor Green
        }
        catch {
            Write-Host "√ winget available" -ForegroundColor Green
        }
    }

    # Check scoop
    if (Test-Command "scoop") {
        $available += "scoop"
        try {
            $scoopVersion = (scoop --version 2>$null).Split()[2]
            Write-Host "√ scoop available (version: $scoopVersion)" -ForegroundColor Green
        }
        catch {
            Write-Host "√ scoop available" -ForegroundColor Green
        }
    }

    if ($available.Count -eq 0) {
        Write-Error "No supported package manager found (winget or scoop)"
        Write-Host ""
        Write-Host "Installation options:"
        Write-Host "1. winget: Install App Installer"
        Write-Host "   Download: https://www.microsoft.com/store/productId/9NBLGGH4NNS1"
        Write-Host "2. scoop: Run the following commands"
        Write-Host "   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser"
        Write-Host "   irm get.scoop.sh | iex"
        return $null
    }

    # Select package manager based on preference
    switch ($Preference) {
        "winget" {
            if ("winget" -in $available) {
                Write-Success "Selected winget as package manager"
                return "winget"
            }
            Write-Warning "winget not available, falling back to other options"
        }
        "scoop" {
            if ("scoop" -in $available) {
                Write-Success "Selected scoop as package manager"
                return "scoop"
            }
            Write-Warning "scoop not available, falling back to other options"
        }
        "auto" {
            # Prefer winget, fallback to scoop
            if ("winget" -in $available) {
                Write-Success "Auto-selected winget as package manager"
                return "winget"
            }
            elseif ("scoop" -in $available) {
                Write-Success "Auto-selected scoop as package manager"
                return "scoop"
            }
        }
    }

    # Fallback to first available
    $selected = $available[0]
    Write-Success "Using $selected as package manager"
    return $selected
}

function Install-WindowsPackage {
    param(
        [string]$PackageManager,
        [string]$PackageName,
        [string]$ScoopPackageName = "",
        [string]$DisplayName,
        [switch]$Silent = $true
    )

    Write-Host "Installing $DisplayName..." -NoNewline
    Write-Verbose "Installing $DisplayName using $PackageManager"

    try {
        $startTime = Get-Date
        switch ($PackageManager) {
            "winget" {
                $args = @("install", $PackageName)
                if ($Silent) {
                    $args += @("--silent", "--accept-package-agreements", "--accept-source-agreements")
                }
                $result = & winget @args 2>&1
                $success = $LASTEXITCODE -eq 0
            }
            "scoop" {
                $scoopPkg = if ($ScoopPackageName) { $ScoopPackageName } else { $PackageName }
                $result = & scoop install $scoopPkg 2>&1
                $success = $LASTEXITCODE -eq 0
            }
        }

        $duration = (Get-Date) - $startTime

        if ($success) {
            Write-Host " √" -ForegroundColor Green
            Write-Verbose "Successfully installed $DisplayName in $($duration.TotalSeconds.ToString('F1'))s"
            return $true
        }
        else {
            Write-Host " !" -ForegroundColor Yellow
            Write-Verbose "Installation failed or package already exists: $result"
            return $false
        }
    }
    catch {
        Write-Host " X" -ForegroundColor Red
        Write-Verbose "Installation error for $DisplayName : $_"
        return $false
    }
}

function Install-WindowsDependencies {
    param(
        [string]$TargetPlatform,
        [bool]$InstallDev,
        [bool]$InstallDocs,
        [string]$PackageManager
    )

    Write-Step "Installing Windows dependencies"

    # Basic tools with scoop alternatives
    $basicTools = @(
        @{ Winget = "Git.Git"; Scoop = "git"; Name = "Git" }
        @{ Winget = "Kitware.CMake"; Scoop = "cmake"; Name = "CMake" }
    )

    # Install basic tools
    Write-Host "Basic tools installation:"
    foreach ($tool in $basicTools) {
        Install-WindowsPackage -PackageManager $PackageManager -PackageName $tool.Winget -ScoopPackageName $tool.Scoop -DisplayName $tool.Name
    }

    # Compiler installation
    Write-Host "`nCompiler installation:"
    if ($PackageManager -eq "scoop") {
        Write-Host "Configuring Scoop buckets..."
        # Add extras bucket for additional tools
        & scoop bucket add extras 2>$null
        & scoop bucket add versions 2>$null

        # Install LLVM for clang
        Install-WindowsPackage -PackageManager "scoop" -PackageName "llvm" -DisplayName "LLVM/Clang"

        # For MSVC, we still need Visual Studio Build Tools via winget or manual installation
        if (Test-Command "winget") {
            Write-Host "Installing Visual Studio Build Tools (via winget)..."
            Install-WindowsPackage -PackageManager "winget" -PackageName "Microsoft.VisualStudio.2022.BuildTools" -DisplayName "Visual Studio Build Tools"
        }
        else {
            Write-Warning "Recommend manually installing Visual Studio Build Tools for MSVC compiler support"
            Write-Host "Download: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
        }
    }
    else {
        # Use winget for Visual Studio Build Tools
        Install-WindowsPackage -PackageManager $PackageManager -PackageName "Microsoft.VisualStudio.2022.BuildTools" -DisplayName "Visual Studio Build Tools"
    }

    # ARM toolchain
    if ($TargetPlatform -in @("stm32f4", "all")) {
        Install-ARMToolchain -PackageManager $PackageManager
    }

    # Development tools
    if ($InstallDev) {
        Install-WindowsDevTools -PackageManager $PackageManager
    }

    # Documentation tools
    if ($InstallDocs) {
        Install-WindowsDocsTools -PackageManager $PackageManager
    }

    return $true
}

function Install-ARMToolchain {
    param([string]$PackageManager)

    Write-Step "Installing ARM GCC toolchain"

    if ($PackageManager -eq "scoop") {
        # Try to install ARM toolchain via scoop
        Write-Host "Attempting to install ARM toolchain via Scoop..."
        & scoop bucket add versions 2>$null
        $result = & scoop install gcc-arm-none-eabi 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Success "ARM GCC toolchain installed successfully (via scoop)"
            return
        }
        Write-Warning "Scoop ARM toolchain installation failed, trying manual installation"
    }

    # Manual installation for both winget and scoop fallback
    Write-Host "Downloading ARM GCC toolchain..."

    $toolsDir = "$env:USERPROFILE\nexus-tools"
    if (!(Test-Path $toolsDir)) {
        New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
        Write-Verbose "Created tools directory: $toolsDir"
    }

    $armUrl = "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-win32.zip"
    $armZip = "$toolsDir\gcc-arm-none-eabi.zip"

    try {
        Write-Host "Downloading to: $armZip"
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($armUrl, $armZip)

        Write-Host "Extracting..."
        Expand-Archive -Path $armZip -DestinationPath $toolsDir -Force

        Write-Success "ARM GCC toolchain installation completed"
        Write-Host ""
        Write-Host "IMPORTANT: Please add the following path to your system PATH environment variable:" -ForegroundColor Yellow
        Write-Host "$toolsDir\gcc-arm-none-eabi-10.3-2021.10\bin" -ForegroundColor Cyan
        Write-Host ""

        # Optionally add to current session PATH
        $armBinPath = "$toolsDir\gcc-arm-none-eabi-10.3-2021.10\bin"
        if (Test-Path $armBinPath) {
            $env:PATH = "$armBinPath;$env:PATH"
            Write-Host "Added to current session PATH" -ForegroundColor Green
        }
    }
    catch {
        Write-Error "ARM GCC toolchain installation failed: $_"
    }
}

function Install-WindowsDevTools {
    param([string]$PackageManager)

    Write-Step "Installing development tools"

    $devTools = @(
        @{ Winget = "LLVM.LLVM"; Scoop = "llvm"; Name = "LLVM (includes clang-format)" }
    )

    foreach ($tool in $devTools) {
        Install-WindowsPackage -PackageManager $PackageManager -PackageName $tool.Winget -ScoopPackageName $tool.Scoop -DisplayName $tool.Name
    }
}

function Install-WindowsDocsTools {
    param([string]$PackageManager)

    Write-Step "Installing documentation tools"

    # Install Doxygen
    if ($PackageManager -eq "scoop") {
        Install-WindowsPackage -PackageManager "scoop" -PackageName "doxygen" -DisplayName "Doxygen"
    }
    else {
        Install-WindowsPackage -PackageManager "winget" -PackageName "doxygen.doxygen" -DisplayName "Doxygen"
    }

    # Install Python packages
    Write-Host "Installing Python documentation packages..."
    try {
        $result = & python -m pip install sphinx breathe sphinx-rtd-theme 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Python documentation packages installed successfully"
        }
        else {
            Write-Warning "Python documentation packages installation failed"
            Write-Verbose "pip install output: $result"
        }
    }
    catch {
        Write-Warning "Python documentation packages installation error: $_"
    }
}

function New-VSCodeConfig {
    Write-Step "Creating VS Code configuration"

    if (!(Test-Path ".vscode")) {
        New-Item -ItemType Directory -Path ".vscode" -Force | Out-Null
        Write-Verbose "Created .vscode directory"
    }

    $settings = @{
        "C_Cpp.default.configurationProvider" = "ms-vscode.cmake-tools"
        "cmake.buildDirectory" = "`${workspaceFolder}/build-`${buildType}"
        "files.associations" = @{
            "*.h" = "c"
            "*.c" = "c"
        }
        "editor.formatOnSave" = $true
        "C_Cpp.clang_format_style" = "file"
        "cmake.configureOnOpen" = $true
        "C_Cpp.intelliSenseEngine" = "Default"
    }

    $settingsJson = $settings | ConvertTo-Json -Depth 3
    $settingsJson | Out-File -FilePath ".vscode/settings.json" -Encoding UTF8

    Write-Success "VS Code configuration created successfully"
    Write-Verbose "VS Code settings written to .vscode/settings.json"
}

function Show-NextSteps {
    param([string]$Platform, [string]$PackageManager)

    Write-Host ""
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "1. Restart terminal to ensure environment variables take effect"
    Write-Host "2. Run build script: python scripts/building/build.py"
    Write-Host "3. Run tests: python scripts/test/test.py"

    if ($Platform -eq "stm32f4") {
        Write-Host ""
        Write-Host "STM32F4 Development:" -ForegroundColor Magenta
        Write-Host "  Build firmware: python scripts/building/build.py -p stm32f4"
        Write-Host "  Output location: build-stm32f4/applications/blinky/"
    }

    Write-Host ""
    Write-Host "Useful Commands:" -ForegroundColor Cyan
    Write-Host "  Check environment: python scripts/setup/check-env.py"
    Write-Host "  Format code: python scripts/tools/format.py"
    Write-Host "  Generate docs: python scripts/tools/docs.py"

    if ($PackageManager -eq "scoop") {
        Write-Host ""
        Write-Host "Scoop Management:" -ForegroundColor Yellow
        Write-Host "  Update packages: scoop update *"
        Write-Host "  List packages: scoop list"
        Write-Host "  Search packages: scoop search <name>"
    }
}

# Main script
function Main {
    # Handle help first
    if ($Help) {
        Show-Help
        return 0
    }

    Write-Header "Nexus Environment Setup - PowerShell Version"

    Write-Host "Configuration:"
    Write-Host "  Target platform: $Platform"
    Write-Host "  Install dev tools: $(if ($Dev) { 'Yes' } else { 'No' })"
    Write-Host "  Install docs tools: $(if ($Docs) { 'Yes' } else { 'No' })"
    Write-Host "  Run tests: $(if ($Test) { 'Yes' } else { 'No' })"
    Write-Host "  Package manager preference: $PackageManager"
    Write-Host "  Verbose output: $(if ($Verbose) { 'Yes' } else { 'No' })"

    # Get system information
    $sysInfo = Get-SystemInfo

    # Install dependencies based on OS
    $success = $false

    # Check if we're on Windows
    $isWindowsSystem = ($env:OS -eq "Windows_NT") -or ($PSVersionTable.PSVersion.Major -le 5) -or $IsWindows

    if ($isWindowsSystem) {
        $pkgMgr = Get-WindowsPackageManager -Preference $PackageManager
        if ($pkgMgr) {
            $success = Install-WindowsDependencies -TargetPlatform $Platform -InstallDev $Dev -InstallDocs $Docs -PackageManager $pkgMgr
        }
    }
    else {
        Write-Host "Non-Windows system detected. Calling Python script..." -ForegroundColor Yellow
        $pythonArgs = @("-p", $Platform)
        if ($Dev) { $pythonArgs += "--dev" }
        if ($Docs) { $pythonArgs += "--docs" }
        if ($Test) { $pythonArgs += "--test" }

        $result = & python "$PSScriptRoot\setup.py" @pythonArgs
        return $LASTEXITCODE
    }

    if (!$success) {
        Write-Error "Dependencies installation failed"
        return 1
    }

    # Create VS Code configuration
    New-VSCodeConfig

    # Run test build
    if ($Test) {
        Write-Host "Running environment verification..." -ForegroundColor Yellow
        $result = & python "$PSScriptRoot\check-env.py" --platform $Platform
    }

    Write-Header "Environment Setup Complete"
    Write-Success "Nexus development environment is ready!"

    # Show next steps
    Show-NextSteps -Platform $Platform -PackageManager $PackageManager

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
