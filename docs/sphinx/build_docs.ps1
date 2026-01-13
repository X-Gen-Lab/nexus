# Build Nexus Documentation (English and Chinese)
# PowerShell version

param(
    [switch]$English,
    [switch]$Chinese,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Building Nexus Documentation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Clean build directory if requested
if ($Clean) {
    Write-Host "`n[Clean] Removing _build directory..." -ForegroundColor Yellow
    if (Test-Path "_build") {
        Remove-Item -Recurse -Force "_build"
    }
}

# Create output directories
New-Item -ItemType Directory -Force -Path "_build/html/en" | Out-Null
New-Item -ItemType Directory -Force -Path "_build/html/cn" | Out-Null

$buildEnglish = $true
$buildChinese = $true

if ($English -or $Chinese) {
    $buildEnglish = $English
    $buildChinese = $Chinese
}

# Build English documentation
if ($buildEnglish) {
    Write-Host "`n[1/3] Building English documentation..." -ForegroundColor Green
    python -m sphinx -b html . _build/html/en
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: English build failed!" -ForegroundColor Red
        exit 1
    }
}

# Build Chinese documentation
if ($buildChinese) {
    Write-Host "`n[2/3] Building Chinese documentation..." -ForegroundColor Green
    $excludePatterns = "['_build','Thumbs.db','.DS_Store','index.rst','getting_started/introduction.rst','getting_started/installation.rst','getting_started/quickstart.rst','user_guide/architecture.rst','user_guide/hal.rst','user_guide/osal.rst','user_guide/log.rst','user_guide/porting.rst','development/contributing.rst','development/coding_standards.rst','development/testing.rst','conf_cn.py']"
    python -m sphinx -b html -c . . _build/html/cn -D master_doc=index_cn -D language=zh_CN -D "exclude_patterns=$excludePatterns"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Chinese build failed!" -ForegroundColor Red
        exit 1
    }
}

# Create language selection page
Write-Host "`n[3/3] Creating language selection page..." -ForegroundColor Green
$indexHtml = @'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="0; url=en/index.html">
    <title>Nexus Documentation</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
        body { display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: #f5f5f5; }
        .container { text-align: center; padding: 40px; background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; margin-bottom: 20px; }
        p { color: #666; margin-bottom: 20px; }
        a { display: inline-block; margin: 10px; padding: 12px 24px; background: #0066cc; color: white; text-decoration: none; border-radius: 4px; }
        a:hover { background: #0052a3; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Nexus Embedded Platform</h1>
        <p>Select language:</p>
        <a href="en/index.html">English</a>
        <a href="cn/index_cn.html">Chinese</a>
    </div>
</body>
</html>
'@
$indexHtml | Out-File -FilePath "_build/html/index.html" -Encoding utf8

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Build completed successfully!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output:"
Write-Host "  Language selection: _build/html/index.html"
Write-Host "  English docs:       _build/html/en/index.html"
Write-Host "  Chinese docs:       _build/html/cn/index_cn.html"
Write-Host ""
Write-Host "Usage:"
Write-Host "  .\build_docs.ps1           # Build all"
Write-Host "  .\build_docs.ps1 -English  # Build English only"
Write-Host "  .\build_docs.ps1 -Chinese  # Build Chinese only"
Write-Host "  .\build_docs.ps1 -Clean    # Clean and rebuild"
