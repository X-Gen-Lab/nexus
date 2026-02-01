@echo off
REM ---------------------------------------------------------------------------
REM setup_deps.bat - Windows dependency initialization script
REM ---------------------------------------------------------------------------
REM Nexus Dependency Setup Script (Windows)
REM Author: Nexus Team
REM
REM This script automatically initializes all required dependencies for Nexus.
REM It detects the platform and only initializes necessary submodules.
REM
REM Usage:
REM   scripts\setup_deps.bat [options]
REM
REM Options:
REM   --all           Initialize all submodules (not recommended)
REM   --platform=X    Initialize for specific platform (stm32, native, etc.)
REM   --series=X      STM32 series (f4, h7, l4, etc.) - required for STM32
REM   --help          Show this help message
REM
REM ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

REM Default values
set INIT_ALL=false
set PLATFORM=
set SERIES=
set PARALLEL_JOBS=4

REM Script directory
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..

REM ---------------------------------------------------------------------------
REM Parse Arguments
REM ---------------------------------------------------------------------------

:parse_args
if "%~1"=="" goto check_args
if "%~1"=="--all" (
    set INIT_ALL=true
    shift
    goto parse_args
)
if "%~1"=="--help" (
    call :show_help
    exit /b 0
)
set ARG=%~1
if "!ARG:~0,11!"=="--platform=" (
    set PLATFORM=!ARG:~11!
    shift
    goto parse_args
)
if "!ARG:~0,9!"=="--series=" (
    set SERIES=!ARG:~9!
    shift
    goto parse_args
)
if "!ARG:~0,11!"=="--parallel=" (
    set PARALLEL_JOBS=!ARG:~11!
    shift
    goto parse_args
)
echo [ERROR] Unknown option: %~1
call :show_help
exit /b 1

:check_args

REM ---------------------------------------------------------------------------
REM Auto-detect Platform from .config
REM ---------------------------------------------------------------------------

if "%PLATFORM%"=="" if not "%INIT_ALL%"=="true" (
    set CONFIG_FILE=%PROJECT_ROOT%\.config

    if exist "!CONFIG_FILE!" (
        echo [INFO] Detecting platform from .config...

        findstr /C:"CONFIG_PLATFORM_NAME=\"stm32\"" "!CONFIG_FILE!" >nul
        if !errorlevel! equ 0 (
            set PLATFORM=stm32

            REM Try to detect series
            if "%SERIES%"=="" (
                for /f "tokens=2 delims==" %%a in ('findstr /C:"CONFIG_PLATFORM_STM32_SERIES=" "!CONFIG_FILE!"') do (
                    set SERIES=%%a
                    set SERIES=!SERIES:"=!
                )
            )
        )

        findstr /C:"CONFIG_PLATFORM_NAME=\"native\"" "!CONFIG_FILE!" >nul
        if !errorlevel! equ 0 (
            set PLATFORM=native
        )

        if not "!PLATFORM!"=="" (
            echo [SUCCESS] Detected platform: !PLATFORM!
            if not "!SERIES!"=="" echo [SUCCESS] Detected series: !SERIES!
        )
    )
)

REM ---------------------------------------------------------------------------
REM Validate Arguments
REM ---------------------------------------------------------------------------

if not "%INIT_ALL%"=="true" if "%PLATFORM%"=="" (
    echo [ERROR] No platform specified and auto-detection failed
    echo.
    call :show_help
    exit /b 1
)

if "%PLATFORM%"=="stm32" if "%SERIES%"=="" (
    echo [ERROR] STM32 series is required for STM32 platform
    echo.
    echo Available series: f0, f1, f2, f3, f4, f7, g0, g4, h5, h7, l0, l1, l4, l5, u0, u3, u5, c0
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM Check Git
REM ---------------------------------------------------------------------------

where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed
    echo.
    echo Please install Git from: https://git-scm.com/download/win
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM Initialize Submodules
REM ---------------------------------------------------------------------------

cd /d "%PROJECT_ROOT%"

echo [INFO] Initializing dependencies...
echo.

if "%INIT_ALL%"=="true" (
    echo [WARNING] Initializing ALL submodules ^(this may take a while and download ~2GB^)...
    git submodule update --init --recursive --depth 1 --jobs %PARALLEL_JOBS%
    echo [SUCCESS] All submodules initialized
    goto summary
)

if "%PLATFORM%"=="stm32" (
    echo [INFO] Initializing STM32%SERIES% dependencies...
    echo.

    REM CMSIS Core
    call :init_submodule "vendors/arm/CMSIS_5" "CMSIS Core (ARM)"

    REM CMSIS Device
    call :init_submodule "vendors/st/cmsis_device_%SERIES%" "CMSIS Device (STM32%SERIES%)"

    REM HAL Driver
    call :init_submodule "vendors/st/stm32%SERIES%xx_hal_driver" "HAL Driver (STM32%SERIES%)"

    REM FreeRTOS (if configured)
    if exist ".config" (
        findstr /C:"CONFIG_OSAL_BACKEND_FREERTOS=y" ".config" >nul
        if !errorlevel! equ 0 (
            call :init_submodule "ext/freertos" "FreeRTOS Kernel"
        )
    )

    echo.
    echo [SUCCESS] STM32%SERIES% dependencies initialized
    goto summary
)

if "%PLATFORM%"=="native" (
    echo [INFO] Initializing native platform dependencies...
    echo.

    REM GoogleTest
    call :init_submodule "ext/googletest" "GoogleTest"

    REM FreeRTOS (if configured)
    if exist ".config" (
        findstr /C:"CONFIG_OSAL_BACKEND_FREERTOS=y" ".config" >nul
        if !errorlevel! equ 0 (
            call :init_submodule "ext/freertos" "FreeRTOS Kernel"
        )
    )

    echo.
    echo [SUCCESS] Native platform dependencies initialized
    goto summary
)

echo [ERROR] Unknown platform: %PLATFORM%
exit /b 1

REM ---------------------------------------------------------------------------
REM Summary
REM ---------------------------------------------------------------------------

:summary
echo.
echo =========================================
echo   Dependency Setup Complete
echo =========================================
echo.
echo Next steps:
echo   1. Configure build:
if "%PLATFORM%"=="stm32" (
    echo        cmake --preset stm32%SERIES%
) else if "%PLATFORM%"=="native" (
    echo        cmake --preset native
)
echo   2. Build project:
echo        cmake --build build
echo.
echo [SUCCESS] Ready to build!
exit /b 0

REM ---------------------------------------------------------------------------
REM Helper Functions
REM ---------------------------------------------------------------------------

:init_submodule
set SUBMODULE_PATH=%~1
set SUBMODULE_NAME=%~2

if exist "%SUBMODULE_PATH%" (
    dir /b "%SUBMODULE_PATH%" | findstr "^" >nul
    if !errorlevel! equ 0 (
        echo [SUCCESS] Already initialized: %SUBMODULE_NAME%
        exit /b 0
    )
)

echo [INFO] Initializing: %SUBMODULE_NAME%
git submodule update --init --depth 1 --jobs %PARALLEL_JOBS% -- "%SUBMODULE_PATH%"
if %errorlevel% equ 0 (
    echo [SUCCESS] Initialized: %SUBMODULE_NAME%
) else (
    echo [ERROR] Failed to initialize: %SUBMODULE_NAME%
    exit /b 1
)
exit /b 0

:show_help
echo Nexus Dependency Setup Script (Windows)
echo.
echo Usage: %~nx0 [options]
echo.
echo Options:
echo   --all              Initialize all submodules (not recommended, ~2GB)
echo   --platform=X       Initialize for specific platform:
echo                        - stm32: STM32 microcontrollers
echo                        - native: Native host builds (tests)
echo   --series=X         STM32 series (required for --platform=stm32):
echo                        - f4, f7, h7, l4, g4, etc.
echo   --parallel=N       Use N parallel jobs (default: 4)
echo   --help             Show this help message
echo.
echo Examples:
echo   # Initialize for STM32F4 development
echo   %~nx0 --platform=stm32 --series=f4
echo.
echo   # Initialize for native testing
echo   %~nx0 --platform=native
echo.
echo   # Initialize everything (not recommended)
echo   %~nx0 --all
echo.
echo Auto-detection:
echo   If no options are provided, the script will try to detect the platform
echo   from .config file (if exists).
echo.
exit /b 0
