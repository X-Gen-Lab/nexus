# Build Nexus Documentation with i18n Support
# PowerShell version using Sphinx official gettext mechanism

param(
    [ValidateSet('en', 'zh_CN')]
    [string]$Lang,
    [switch]$Clean,
    [switch]$Serve,
    [int]$Port = 8000,
    [switch]$Doxygen,
    [switch]$UpdatePo,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# Supported languages
$Languages = @('en', 'zh_CN')

function Write-Header($msg) {
    Write-Host $msg -ForegroundColor Cyan
}

function Write-Step($msg) {
    Write-Host "  → $msg" -ForegroundColor Green
}

function Write-Warning($msg) {
    Write-Host "  ⚠ $msg" -ForegroundColor Yellow
}

function Write-Error($msg) {
    Write-Host "  ✗ $msg" -ForegroundColor Red
}

function Write-Success($msg) {
    Write-Host "  ✓ $msg" -ForegroundColor Green
}

# Show help
if ($Help) {
    Write-Host @"
Usage: .\build_docs.ps1 [options]

Options:
  -Lang LANG      Build specific language only (en, zh_CN)
  -Clean          Clean build directory before building
  -Serve          Serve documentation after building
  -Port PORT      Port for serving (default: 8000)
  -Doxygen        Run Doxygen first
  -UpdatePo       Update translation .po files
  -Help           Show this help message

Examples:
  .\build_docs.ps1                    # Build all languages
  .\build_docs.ps1 -Lang zh_CN        # Build Chinese only
  .\build_docs.ps1 -Clean -Serve      # Clean, rebuild, and serve
  .\build_docs.ps1 -UpdatePo          # Update translation files
"@
    exit 0
}

Write-Header ""
Write-Header "╔════════════════════════════════════════╗"
Write-Header "║   Nexus Documentation Builder (i18n)   ║"
Write-Header "╚════════════════════════════════════════╝"
Write-Header ""

# Handle -UpdatePo
if ($UpdatePo) {
    Write-Header "Updating Translation Files"
    Write-Step "Extracting translatable messages..."
    python -m sphinx -b gettext . _build/gettext
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Message extraction failed!"
        exit 1
    }
    Write-Success "Message extraction completed"

    Write-Step "Updating zh_CN translation files..."
    python -m sphinx_intl update -p _build/gettext -l zh_CN
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to update translation files!"
        exit 1
    }
    Write-Success "Translation files updated"

    Write-Host ""
    Write-Host "Edit files in: locale\zh_CN\LC_MESSAGES\*.po"
    exit 0
}

# Run Doxygen if requested
if ($Doxygen) {
    Write-Header "Running Doxygen"
    Write-Step "Generating API documentation..."
    Push-Location ..\..
    doxygen Doxyfile
    Pop-Location
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Doxygen failed, API docs may be incomplete"
    } else {
        Write-Success "Doxygen completed"
    }
}

# Clean build directory if requested
if ($Clean) {
    Write-Header "Cleaning Build Directory"
    Write-Step "Removing _build directory..."
    if (Test-Path "_build") {
        Remove-Item -Recurse -Force "_build"
    }
    Write-Success "Clean completed"
}

# Create output directories
New-Item -ItemType Directory -Force -Path "_build/html/en" | Out-Null
New-Item -ItemType Directory -Force -Path "_build/html/zh_CN" | Out-Null

# Determine languages to build
if ($Lang) {
    $LanguagesToBuild = @($Lang)
} else {
    $LanguagesToBuild = $Languages
}

# Build documentation
Write-Header "Building Documentation"

foreach ($language in $LanguagesToBuild) {
    Write-Step "Building $language documentation..."

    if ($language -eq 'en') {
        python -m sphinx -b html . "_build/html/$language"
    } else {
        python -m sphinx -b html -D "language=$language" . "_build/html/$language"
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "$language build failed!"
        exit 1
    }
    Write-Success "$language documentation built"
}

# Create language selection page (only if building all languages)
if (-not $Lang) {
    Write-Step "Creating language selection page..."
    $indexHtml = @'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="3; url=en/index.html">
    <title>Nexus Documentation - Language Selection</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        .container {
            text-align: center;
            padding: 60px 40px;
            background: white;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 500px;
            width: 90%;
        }
        .logo { font-size: 48px; margin-bottom: 20px; }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
            font-weight: 600;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 16px;
        }
        .languages {
            display: flex;
            justify-content: center;
            gap: 20px;
            flex-wrap: wrap;
        }
        a {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px 30px;
            background: #f8f9fa;
            color: #333;
            text-decoration: none;
            border-radius: 12px;
            transition: all 0.3s ease;
            border: 2px solid transparent;
            min-width: 140px;
        }
        a:hover {
            background: #667eea;
            color: white;
            transform: translateY(-4px);
            box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4);
        }
        .flag { font-size: 32px; margin-bottom: 8px; }
        .lang-name { font-weight: 500; font-size: 16px; }
        .redirect-notice {
            margin-top: 30px;
            color: #999;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">📚</div>
        <h1>Nexus Embedded Platform</h1>
        <p class="subtitle">Select your preferred language</p>
        <div class="languages">
            <a href="en/index.html">
                <span class="flag">🇺🇸</span>
                <span class="lang-name">English</span>
            </a>
            <a href="zh_CN/index.html">
                <span class="flag">🇨🇳</span>
                <span class="lang-name">中文</span>
            </a>
        </div>
        <p class="redirect-notice">Redirecting to English in 3 seconds...</p>
    </div>
</body>
</html>
'@
    $indexHtml | Out-File -FilePath "_build/html/index.html" -Encoding utf8
    Write-Success "Language selection page created"
}

# Summary
Write-Header ""
Write-Header "╔════════════════════════════════════════╗"
Write-Header "║         Build Completed!               ║"
Write-Header "╚════════════════════════════════════════╝"
Write-Host ""
Write-Host "Output locations:"
if (-not $Lang) {
    Write-Host "  Language selector: _build/html/index.html"
}
foreach ($language in $LanguagesToBuild) {
    Write-Host ("  {0,-15}: _build/html/{0}/index.html" -f $language)
}

# Serve if requested
if ($Serve) {
    Write-Host ""
    Write-Host "Serving documentation at http://localhost:$Port" -ForegroundColor Cyan
    Write-Host "Press Ctrl+C to stop."
    Write-Host ""
    Set-Location "_build/html"
    python -m http.server $Port
}
