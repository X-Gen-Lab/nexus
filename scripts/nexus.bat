@echo off
chcp 65001 >nul 2>&1
REM Nexus Project Manager - Batch Version
REM
REM Unified entry point for all Nexus project operations.
REM Provides easy access to build, test, format, clean, docs, and CI operations.
REM
REM Usage: nexus.bat <command> [arguments...]
REM
REM Commands:
REM   setup     - Environment setup and configuration
REM   build     - Build the project
REM   test      - Run tests
REM   format    - Format source code
REM   clean     - Clean build artifacts
REM   docs      - Generate documentation
REM   ci        - Run CI pipeline
REM   help      - Show help information
REM
REM Examples:
REM   nexus.bat setup --dev --docs
REM   nexus.bat build --type release --platform stm32f4
REM   nexus.bat test --filter "*Math*" --verbose
REM   nexus.bat ci --stage all --coverage

setlocal enabledelayedexpansion

REM Project information
set PROJECT_NAME=Nexus Embedded Platform
set PROJECT_VERSION=1.0.0
set PROJECT_DESCRIPTION=Cross-platform embedded development framework
set PROJECT_REPOSITORY=https://github.com/nexus-platform/nexus
set BATCH_VERSION=1.0.0

REM Parse command line arguments
set COMMAND=%1
set SHOW_LIST=false
set SHOW_VERSION=false
set SHOW_HELP=false

REM Check for special flags
if "%1"=="--list" (
    set SHOW_LIST=true
    goto :handle_flags
)
if "%1"=="--version" (
    set SHOW_VERSION=true
    goto :handle_flags
)
if "%1"=="--help" (
    set SHOW_HELP=true
    goto :handle_flags
)
if "%1"=="" (
    set SHOW_HELP=true
    goto :handle_flags
)
if "%1"=="help" (
    set SHOW_HELP=true
    goto :handle_flags
)

goto :validate_environment

:handle_flags
if "%SHOW_VERSION%"=="true" (
    call :show_version
    exit /b 0
)
if "%SHOW_LIST%"=="true" (
    call :show_command_list
    exit /b 0
)
if "%SHOW_HELP%"=="true" (
    call :show_help
    exit /b 0
)

:validate_environment
call :test_project_structure
if errorlevel 1 (
    echo ERROR: Project structure validation failed
    exit /b 1
)

REM Execute command
call :invoke_command %*
exit /b %errorlevel%

:show_version
echo.
echo ============================================================
echo                 Nexus Project Manager
echo ============================================================
echo.
echo Project Information:
echo   Name: %PROJECT_NAME%
echo   Version: %PROJECT_VERSION%
echo   Description: %PROJECT_DESCRIPTION%
echo   Repository: %PROJECT_REPOSITORY%
echo.
echo Batch Scripts Version: %BATCH_VERSION%
echo Windows Version: %OS%
echo Platform: %PROCESSOR_ARCHITECTURE%
goto :eof

:show_help
echo.
echo ============================================================
echo             Nexus Project Manager - Help
echo ============================================================
echo.
echo Usage: nexus.bat ^<command^> [arguments...]
echo.
echo Commands:
echo   setup     - Environment setup and configuration
echo   build     - Build the project
echo   test      - Run tests
echo   format    - Format source code
echo   clean     - Clean build artifacts
echo   docs      - Generate documentation
echo   ci        - Run CI pipeline
echo   help      - Show this help information
echo.
echo Options:
echo   --list    - List all available commands
echo   --version - Show version information
echo   --help    - Show this help information
echo.
echo Examples:
echo   nexus.bat setup --dev --docs         # Setup development environment
echo   nexus.bat build --type release       # Build in Release mode
echo   nexus.bat test --verbose             # Run tests with verbose output
echo   nexus.bat format --check             # Check code formatting
echo   nexus.bat clean --all                # Clean all artifacts
echo   nexus.bat docs --target doxygen      # Generate API docs
echo   nexus.bat ci --stage all --coverage  # Run full CI with coverage
echo.
echo For detailed help on a specific command:
echo   nexus.bat ^<command^> --help
echo.
echo For more information, visit: %PROJECT_REPOSITORY%
goto :eof

:show_command_list
echo.
echo ============================================================
echo                 Available Commands
echo ============================================================
echo.
echo Command     Description                             Script Location
echo --------------------------------------------------------------------------------

call :check_script_exists setup "Environment setup and configuration" scripts\setup\setup.bat
call :check_script_exists build "Build the project" scripts\building\build.bat
call :check_script_exists test "Run tests" scripts\test\test.bat
call :check_script_exists format "Format source code" scripts\tools\format.bat
call :check_script_exists clean "Clean build artifacts" scripts\tools\clean.bat
call :check_script_exists docs "Generate documentation" scripts\tools\docs.bat
call :check_script_exists ci "Run CI pipeline" scripts\ci\ci_build.bat

echo.
echo Usage: nexus.bat ^<command^> [arguments...]
echo For command-specific help: nexus.bat ^<command^> --help
goto :eof

:check_script_exists
set cmd_name=%1
set cmd_desc=%2
set script_path=%3

if exist "%script_path%" (
    echo %cmd_name%       %cmd_desc%             √ %script_path%
) else (
    echo %cmd_name%       %cmd_desc%             X %script_path%
)
goto :eof

:test_project_structure
REM Check for essential directories
if not exist "scripts" (
    echo WARNING: Missing project directory: scripts
    echo Please ensure you're running this script from the project root
    exit /b 1
)
if not exist "hal" (
    echo WARNING: Missing project directory: hal
    echo Please ensure you're running this script from the project root
    exit /b 1
)
if not exist "osal" (
    echo WARNING: Missing project directory: osal
    echo Please ensure you're running this script from the project root
    exit /b 1
)
if not exist "platforms" (
    echo WARNING: Missing project directory: platforms
    echo Please ensure you're running this script from the project root
    exit /b 1
)

REM Check for batch scripts (optional warnings)
set missing_scripts=
if not exist "scripts\setup\setup.bat" set missing_scripts=!missing_scripts! setup
if not exist "scripts\building\build.bat" set missing_scripts=!missing_scripts! build
if not exist "scripts\test\test.bat" set missing_scripts=!missing_scripts! test

if not "!missing_scripts!"=="" (
    echo WARNING: Missing batch scripts:!missing_scripts!
    echo Some commands may not be available
    echo Consider using Python scripts instead: python nexus.py ^<command^>
)

goto :eof

:invoke_command
set command_name=%1
shift

REM Map commands to script paths
if "%command_name%"=="setup" set script_path=scripts\setup\setup.bat
if "%command_name%"=="build" set script_path=scripts\building\build.bat
if "%command_name%"=="test" set script_path=scripts\test\test.bat
if "%command_name%"=="format" set script_path=scripts\tools\format.bat
if "%command_name%"=="clean" set script_path=scripts\tools\clean.bat
if "%command_name%"=="docs" set script_path=scripts\tools\docs.bat
if "%command_name%"=="ci" set script_path=scripts\ci\ci_build.bat

if "%script_path%"=="" (
    echo ERROR: Unknown command: %command_name%
    echo Use 'nexus.bat --list' to see available commands
    exit /b 1
)

if not exist "%script_path%" (
    echo ERROR: Script not found: %script_path%
    echo.
    echo Alternative options:
    echo   1. Use Python version: python nexus.py %command_name% %*
    echo   2. Use PowerShell version: powershell -File nexus.ps1 %command_name% %*
    echo   3. Install missing batch scripts
    exit /b 1
)

echo Executing: %command_name%
echo Script: %script_path%
if not "%1"=="" echo Arguments: %*
echo.

REM Execute the script with remaining arguments
call "%script_path%" %1 %2 %3 %4 %5 %6 %7 %8 %9
exit /b %errorlevel%
