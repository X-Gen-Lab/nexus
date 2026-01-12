@echo off
chcp 65001 >nul 2>&1
REM Nexus Environment Setup Script - Windows Version
REM This is an enhanced wrapper script that calls the Python version

echo ============================================================
echo                Nexus Environment Setup Script
echo ============================================================
echo.
echo This Windows batch script is an enhanced wrapper for the Python version.
echo For better compatibility and features, we recommend using:
echo.
echo   python scripts\setup\setup.py %*
echo.
echo If you encounter any issues with this batch file, please use
echo the Python script directly.
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.7+ and try again
    echo Download from: https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Show Python version
for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
echo Using Python %PYTHON_VERSION%

REM Check if we're in the correct directory
if not exist "scripts\setup\setup.py" (
    echo ERROR: setup.py not found. Please run this script from the project root directory.
    pause
    exit /b 1
)

REM Call the Python script with all arguments
echo.
echo Calling Python setup script...
echo Command: python scripts\setup\setup.py %*
echo.
python "%~dp0setup.py" %*
set PYTHON_EXIT_CODE=%errorlevel%

echo.
if %PYTHON_EXIT_CODE% equ 0 (
    echo ✓ Setup completed successfully
) else (
    echo ✗ Setup failed with exit code %PYTHON_EXIT_CODE%
)

exit /b %PYTHON_EXIT_CODE%
