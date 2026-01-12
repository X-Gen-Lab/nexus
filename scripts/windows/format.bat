@echo off
REM Nexus Code Formatter Script for Windows
REM Usage: format.bat [check]

setlocal enabledelayedexpansion

set CHECK_ONLY=0

if /i "%1"=="check" set CHECK_ONLY=1

echo ========================================
echo Nexus Code Formatter
echo ========================================

where clang-format >nul 2>&1
if errorlevel 1 (
    echo clang-format not found! Please install LLVM.
    exit /b 1
)

set SOURCES=
for /r hal %%f in (*.c *.h) do set SOURCES=!SOURCES! "%%f"
for /r osal %%f in (*.c *.h) do set SOURCES=!SOURCES! "%%f"
for /r platforms %%f in (*.c *.h) do set SOURCES=!SOURCES! "%%f"
for /r tests %%f in (*.cpp *.hpp) do set SOURCES=!SOURCES! "%%f"
for /r applications %%f in (*.c *.h) do set SOURCES=!SOURCES! "%%f"

if %CHECK_ONLY%==1 (
    echo Checking code format...
    for %%f in (%SOURCES%) do (
        clang-format --dry-run --Werror "%%f"
        if errorlevel 1 (
            echo Format check failed for: %%f
            exit /b 1
        )
    )
    echo All files are properly formatted!
) else (
    echo Formatting code...
    for %%f in (%SOURCES%) do (
        echo Formatting: %%f
        clang-format -i "%%f"
    )
    echo Code formatting complete!
)

echo ========================================
