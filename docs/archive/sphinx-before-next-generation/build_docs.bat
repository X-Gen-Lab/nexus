@echo off
chcp 65001 >nul
REM Build Nexus Documentation with i18n Support
REM Windows Batch version using Sphinx official gettext mechanism

setlocal enabledelayedexpansion

echo.
echo ========================================
echo   Nexus Documentation Builder (i18n)
echo ========================================
echo.

REM Parse arguments
set "BUILD_LANG="
set "CLEAN=0"
set "UPDATE_PO=0"
set "RUN_DOXYGEN=0"

:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="-l" (
    set "BUILD_LANG=%~2"
    shift
    shift
    goto :parse_args
)
if /i "%~1"=="--lang" (
    set "BUILD_LANG=%~2"
    shift
    shift
    goto :parse_args
)
if /i "%~1"=="--clean" (
    set "CLEAN=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--update-po" (
    set "UPDATE_PO=1"
    shift
    goto :parse_args
)
if /i "%~1"=="-d" (
    set "RUN_DOXYGEN=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--doxygen" (
    set "RUN_DOXYGEN=1"
    shift
    goto :parse_args
)
if /i "%~1"=="-h" goto :show_help
if /i "%~1"=="--help" goto :show_help
echo Unknown option: %~1
exit /b 1

:show_help
echo Usage: %~nx0 [options]
echo.
echo Options:
echo   -l, --lang LANG  Build specific language only (en, zh_CN)
echo   --clean          Clean build directory before building
echo   -d, --doxygen    Run Doxygen first
echo   --update-po      Update translation .po files
echo   -h, --help       Show this help message
echo.
echo Examples:
echo   %~nx0                    # Build all languages
echo   %~nx0 --lang zh_CN       # Build Chinese only
echo   %~nx0 --clean            # Clean and rebuild
echo   %~nx0 --update-po        # Update translation files
exit /b 0

:done_args

REM Handle --update-po
if "%UPDATE_PO%"=="1" (
    echo [Update] Extracting translatable messages...
    python -m sphinx -b gettext . _build/gettext
    if errorlevel 1 goto :error
    echo [Update] Updating zh_CN translation files...
    python -m sphinx_intl update -p _build/gettext -l zh_CN
    if errorlevel 1 goto :error
    echo.
    echo Translation files updated!
    echo Edit files in: locale\zh_CN\LC_MESSAGES\*.po
    exit /b 0
)

REM Run Doxygen if requested
if "%RUN_DOXYGEN%"=="1" (
    echo [Doxygen] Generating API documentation...
    pushd ..\..
    doxygen Doxyfile
    popd
    echo [Doxygen] Completed
)

REM Clean if requested
if "%CLEAN%"=="1" (
    echo [Clean] Removing _build directory...
    if exist "_build" rmdir /s /q "_build"
    echo [Clean] Completed
)

REM Create output directories
if not exist "_build\html\en" mkdir "_build\html\en"
if not exist "_build\html\zh_CN" mkdir "_build\html\zh_CN"

REM Determine what to build
if not "%BUILD_LANG%"=="" (
    if "%BUILD_LANG%"=="en" goto :build_en_only
    if "%BUILD_LANG%"=="zh_CN" goto :build_zh_only
    echo Unknown language: %BUILD_LANG%
    exit /b 1
)

REM Build all languages
:build_all
echo.
echo [1/3] Building English documentation...
python -m sphinx -b html . _build/html/en
if errorlevel 1 goto :error

echo.
echo [2/3] Building Chinese documentation...
python -m sphinx -b html -D language=zh_CN . _build/html/zh_CN
if errorlevel 1 goto :error

goto :create_index

:build_en_only
echo.
echo Building English documentation...
python -m sphinx -b html . _build/html/en
if errorlevel 1 goto :error
goto :done

:build_zh_only
echo.
echo Building Chinese documentation...
python -m sphinx -b html -D language=zh_CN . _build/html/zh_CN
if errorlevel 1 goto :error
goto :done

:create_index
echo.
echo [3/3] Creating language selection page...
(
echo ^<!DOCTYPE html^>
echo ^<html lang="en"^>
echo ^<head^>
echo     ^<meta charset="UTF-8"^>
echo     ^<meta name="viewport" content="width=device-width, initial-scale=1.0"^>
echo     ^<meta http-equiv="refresh" content="3; url=en/index.html"^>
echo     ^<title^>Nexus Documentation - Language Selection^</title^>
echo     ^<style^>
echo         * { box-sizing: border-box; margin: 0; padding: 0; }
echo         body {
echo             font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
echo             display: flex; justify-content: center; align-items: center;
echo             min-height: 100vh;
echo             background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%);
echo         }
echo         .container {
echo             text-align: center; padding: 60px 40px;
echo             background: white; border-radius: 16px;
echo             box-shadow: 0 20px 60px rgba(0,0,0,0.3);
echo             max-width: 500px; width: 90%%;
echo         }
echo         .logo { font-size: 48px; margin-bottom: 20px; }
echo         h1 { color: #333; margin-bottom: 10px; font-size: 28px; font-weight: 600; }
echo         .subtitle { color: #666; margin-bottom: 30px; font-size: 16px; }
echo         .languages { display: flex; justify-content: center; gap: 20px; flex-wrap: wrap; }
echo         a {
echo             display: flex; flex-direction: column; align-items: center;
echo             padding: 20px 30px; background: #f8f9fa; color: #333;
echo             text-decoration: none; border-radius: 12px;
echo             transition: all 0.3s ease; border: 2px solid transparent; min-width: 140px;
echo         }
echo         a:hover {
echo             background: #667eea; color: white;
echo             transform: translateY(-4px);
echo             box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4);
echo         }
echo         .flag { font-size: 32px; margin-bottom: 8px; }
echo         .lang-name { font-weight: 500; font-size: 16px; }
echo         .redirect-notice { margin-top: 30px; color: #999; font-size: 14px; }
echo     ^</style^>
echo ^</head^>
echo ^<body^>
echo     ^<div class="container"^>
echo         ^<div class="logo"^>📚^</div^>
echo         ^<h1^>Nexus Embedded Platform^</h1^>
echo         ^<p class="subtitle"^>Select your preferred language^</p^>
echo         ^<div class="languages"^>
echo             ^<a href="en/index.html"^>
echo                 ^<span class="flag"^>🇺🇸^</span^>
echo                 ^<span class="lang-name"^>English^</span^>
echo             ^</a^>
echo             ^<a href="zh_CN/index.html"^>
echo                 ^<span class="flag"^>🇨🇳^</span^>
echo                 ^<span class="lang-name"^>中文^</span^>
echo             ^</a^>
echo         ^</div^>
echo         ^<p class="redirect-notice"^>Redirecting to English in 3 seconds...^</p^>
echo     ^</div^>
echo ^</body^>
echo ^</html^>
) > _build\html\index.html

:done
echo.
echo ========================================
echo   Build completed successfully!
echo ========================================
echo.
echo Output locations:
if "%BUILD_LANG%"=="" (
    echo   Language selector: _build/html/index.html
    echo   English docs:      _build/html/en/index.html
    echo   Chinese docs:      _build/html/zh_CN/index.html
) else (
    echo   %BUILD_LANG% docs: _build/html/%BUILD_LANG%/index.html
)
echo.
exit /b 0

:error
echo.
echo Build failed with errors.
exit /b 1
