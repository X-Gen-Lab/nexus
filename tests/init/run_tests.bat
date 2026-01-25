@echo off
REM ###########################################################################
REM Init Framework Test Runner (Windows)
REM ###########################################################################

setlocal enabledelayedexpansion

REM Script directory
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..\..
set BUILD_DIR=%PROJECT_ROOT%\build

REM Test options
set RUN_UNIT_TESTS=1
set RUN_INTEGRATION_TESTS=1
set RUN_PERFORMANCE_TESTS=1
set VERBOSE=0

REM ###########################################################################
REM Parse Arguments
REM ###########################################################################

:parse_args
if "%~1"=="" goto end_parse_args
if /i "%~1"=="-u" (
    set RUN_INTEGRATION_TESTS=0
    set RUN_PERFORMANCE_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="--unit" (
    set RUN_INTEGRATION_TESTS=0
    set RUN_PERFORMANCE_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="-i" (
    set RUN_UNIT_TESTS=0
    set RUN_PERFORMANCE_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="--integration" (
    set RUN_UNIT_TESTS=0
    set RUN_PERFORMANCE_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="-p" (
    set RUN_UNIT_TESTS=0
    set RUN_INTEGRATION_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="--performance" (
    set RUN_UNIT_TESTS=0
    set RUN_INTEGRATION_TESTS=0
    shift
    goto parse_args
)
if /i "%~1"=="-v" (
    set VERBOSE=1
    shift
    goto parse_args
)
if /i "%~1"=="--verbose" (
    set VERBOSE=1
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="--help" goto show_help

echo [ERROR] Unknown option: %~1
goto show_help

:end_parse_args

REM ###########################################################################
REM Main
REM ###########################################################################

echo ========================================
echo Init Framework Test Runner
echo ========================================
echo.

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo [ERROR] Build directory not found: %BUILD_DIR%
    echo [INFO] Please run CMake to configure the project first
    exit /b 1
)

REM Find test executable
set TEST_EXECUTABLE=%BUILD_DIR%\tests\init\Debug\init_tests.exe
if not exist "%TEST_EXECUTABLE%" (
    set TEST_EXECUTABLE=%BUILD_DIR%\tests\init\Release\init_tests.exe
)
if not exist "%TEST_EXECUTABLE%" (
    set TEST_EXECUTABLE=%BUILD_DIR%\tests\init\init_tests.exe
)

if not exist "%TEST_EXECUTABLE%" (
    echo [ERROR] Test executable not found
    echo [INFO] Please build the project first
    exit /b 1
)

REM Prepare test arguments
set TEST_ARGS=
if %VERBOSE%==1 (
    set TEST_ARGS=--gtest_print_time=1
)

REM Run tests
echo [INFO] Running Init Framework tests...
echo.

if %RUN_UNIT_TESTS%==1 (
    echo ========================================
    echo Unit Tests
    echo ========================================
    "%TEST_EXECUTABLE%" --gtest_filter="NxInit*:NxStartup*:NxFirmwareInfo*" %TEST_ARGS%
    if errorlevel 1 (
        echo [ERROR] Unit tests failed
        exit /b 1
    )
    echo.
)

if %RUN_INTEGRATION_TESTS%==1 (
    echo ========================================
    echo Integration Tests
    echo ========================================
    "%TEST_EXECUTABLE%" --gtest_filter="InitIntegration*" %TEST_ARGS%
    if errorlevel 1 (
        echo [ERROR] Integration tests failed
        exit /b 1
    )
    echo.
)

if %RUN_PERFORMANCE_TESTS%==1 (
    echo ========================================
    echo Performance Tests
    echo ========================================
    "%TEST_EXECUTABLE%" --gtest_filter="InitPerformance*" %TEST_ARGS%
    if errorlevel 1 (
        echo [ERROR] Performance tests failed
        exit /b 1
    )
    echo.
)

echo ========================================
echo Test Run Complete
echo ========================================
echo [INFO] All tests passed successfully!
echo.

exit /b 0

REM ###########################################################################
REM Help
REM ###########################################################################

:show_help
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo     -u, --unit              Run unit tests only
echo     -i, --integration       Run integration tests only
echo     -p, --performance       Run performance tests only
echo     -v, --verbose           Verbose output
echo     -h, --help              Show this help message
echo.
echo Examples:
echo     %~nx0                   Run all tests
echo     %~nx0 -u                Run unit tests only
echo     %~nx0 -p                Run performance tests only
echo.
exit /b 0
