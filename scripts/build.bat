@echo off
REM ---------------------------------------------------------------------------
REM Nexus Build Script for Windows
REM ---------------------------------------------------------------------------
REM This script automatically configures and builds the project based on
REM Kconfig configuration.
REM
REM Usage:
REM   scripts\build.bat [options]
REM
REM Options:
REM   --config        Run menuconfig before building
REM   --clean         Clean build directory before building
REM   --preset NAME   Use specific CMake preset (overrides auto-detection)
REM   --help          Show this help message
REM
REM Author: Nexus Team
REM ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

REM Default options
set RUN_CONFIG=0
set CLEAN_BUILD=0
set PRESET_NAME=

REM ---------------------------------------------------------------------------
REM Parse arguments
REM ---------------------------------------------------------------------------

:parse_args
if "%~1"=="" goto end_parse_args
if /i "%~1"=="--config" (
    set RUN_CONFIG=1
    shift
    goto parse_args
)
if /i "%~1"=="--clean" (
    set CLEAN_BUILD=1
    shift
    goto parse_args
)
if /i "%~1"=="--preset" (
    set PRESET_NAME=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" (
    goto show_help
)
echo [ERROR] Unknown option: %~1
goto show_help

:end_parse_args

REM ---------------------------------------------------------------------------
REM Main script
REM ---------------------------------------------------------------------------

echo ========================================
echo Nexus Build Script
echo ========================================
echo.

REM Step 1: Run menuconfig if requested
if %RUN_CONFIG%==1 (
    echo ========================================
    echo Step 1: Configuration
    echo ========================================
    echo [INFO] Running menuconfig...

    where menuconfig >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] menuconfig not found. Please install it first.
        exit /b 1
    )

    call menuconfig
    if errorlevel 1 (
        echo [ERROR] menuconfig failed
        exit /b 1
    )

    echo.
)

REM Step 2: Detect preset from Kconfig
if "%PRESET_NAME%"=="" (
    echo ========================================
    echo Step 2: Detect CMake Preset
    echo ========================================

    if not exist ".config" (
        echo [ERROR] .config file not found. Please run 'menuconfig' first.
        exit /b 1
    )

    echo [INFO] Detecting preset from Kconfig...

    if exist "scripts\generate_cmake_preset.py" (
        python scripts\generate_cmake_preset.py --config .config --output .cmake_preset
        if errorlevel 1 (
            echo [ERROR] Failed to detect preset from Kconfig
            exit /b 1
        )

        REM Read preset name from file
        if exist ".cmake_preset" (
            set /p PRESET_NAME=<.cmake_preset
            del .cmake_preset
        )
    ) else (
        echo [ERROR] generate_cmake_preset.py not found
        exit /b 1
    )

    echo.
)

echo [INFO] Using preset: !PRESET_NAME!

REM Step 3: Clean if requested
if %CLEAN_BUILD%==1 (
    echo ========================================
    echo Step 3: Clean Build
    echo ========================================

    set BUILD_DIR=build\!PRESET_NAME!

    if exist "!BUILD_DIR!" (
        echo [INFO] Cleaning build directory: !BUILD_DIR!
        rmdir /s /q "!BUILD_DIR!"
    ) else (
        echo [INFO] Build directory does not exist, skipping clean
    )

    echo.
)

REM Step 4: Configure with CMake
echo ========================================
echo Step 4: CMake Configuration
echo ========================================
echo [INFO] Configuring with preset: !PRESET_NAME!

cmake --preset !PRESET_NAME!
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo.

REM Step 5: Build
echo ========================================
echo Step 5: Build
echo ========================================
echo [INFO] Building project...

cmake --build build\!PRESET_NAME!
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.

REM Step 6: Success
echo ========================================
echo Build Complete
echo ========================================
echo [INFO] Build artifacts are in: build\!PRESET_NAME!

REM Show binary files
if exist "build\!PRESET_NAME!" (
    echo [INFO] Generated files:
    dir /s /b "build\!PRESET_NAME!\*.elf" "build\!PRESET_NAME!\*.bin" "build\!PRESET_NAME!\*.hex" "build\!PRESET_NAME!\*.exe" 2>nul | findstr /v /c:"File Not Found"
)

echo.
echo [INFO] Build completed successfully!

exit /b 0

REM ---------------------------------------------------------------------------
REM Help
REM ---------------------------------------------------------------------------

:show_help
echo Nexus Build Script for Windows
echo.
echo Usage: %~nx0 [options]
echo.
echo Options:
echo     --config        Run menuconfig before building
echo     --clean         Clean build directory before building
echo     --preset NAME   Use specific CMake preset (overrides auto-detection)
echo     --help          Show this help message
echo.
echo Examples:
echo     REM Configure and build
echo     %~nx0 --config
echo.
echo     REM Clean and build
echo     %~nx0 --clean
echo.
echo     REM Use specific preset
echo     %~nx0 --preset windows-arm-debug
echo.
echo     REM Configure, clean, and build
echo     %~nx0 --config --clean
echo.
exit /b 0
