@echo off
REM ##########################################################################
REM Log Framework Test Runner (Windows)
REM ##########################################################################

setlocal enabledelayedexpansion

REM Default values
set "BUILD_DIR=..\..\build"
set "TEST_FILTER=*"
set "COVERAGE=0"
set "VERBOSE=0"
set "HELP=0"

REM Parse command line arguments
:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="-h" set "HELP=1"
if /i "%~1"=="--help" set "HELP=1"
if /i "%~1"=="-f" (
    set "TEST_FILTER=%~2"
    shift
)
if /i "%~1"=="--filter" (
    set "TEST_FILTER=%~2"
    shift
)
if /i "%~1"=="-c" set "COVERAGE=1"
if /i "%~1"=="--coverage" set "COVERAGE=1"
if /i "%~1"=="-V" set "VERBOSE=1"
if /i "%~1"=="--verbose" set "VERBOSE=1"
shift
goto parse_args
:end_parse

REM Show help
if "%HELP%"=="1" (
    echo Usage: run_tests.bat [OPTIONS]
    echo.
    echo Options:
    echo   -h, --help              Show this help message
    echo   -f, --filter PATTERN    Run only tests matching PATTERN
    echo   -c, --coverage          Generate coverage report
    echo   -V, --verbose           Enable verbose output
    echo.
    echo Examples:
    echo   run_tests.bat                          Run all tests
    echo   run_tests.bat -f "LogBasic*"           Run tests matching pattern
    echo   run_tests.bat -c                       Run with coverage
    echo   run_tests.bat -V                       Run with verbose output
    exit /b 0
)

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo Error: Build directory not found: %BUILD_DIR%
    echo Please build the project first.
    exit /b 1
)

REM Find test executable
set "TEST_EXE="
if exist "%BUILD_DIR%\tests\log\Debug\log_tests.exe" (
    set "TEST_EXE=%BUILD_DIR%\tests\log\Debug\log_tests.exe"
) else if exist "%BUILD_DIR%\tests\log\Release\log_tests.exe" (
    set "TEST_EXE=%BUILD_DIR%\tests\log\Release\log_tests.exe"
) else if exist "%BUILD_DIR%\tests\log\log_tests.exe" (
    set "TEST_EXE=%BUILD_DIR%\tests\log\log_tests.exe"
)

if "%TEST_EXE%"=="" (
    echo Error: Test executable not found
    echo Searched in:
    echo   - %BUILD_DIR%\tests\log\Debug\log_tests.exe
    echo   - %BUILD_DIR%\tests\log\Release\log_tests.exe
    echo   - %BUILD_DIR%\tests\log\log_tests.exe
    exit /b 1
)

echo ========================================
echo Log Framework Test Runner
echo ========================================
echo Test executable: %TEST_EXE%
echo Test filter: %TEST_FILTER%
echo Coverage: %COVERAGE%
echo Verbose: %VERBOSE%
echo ========================================
echo.

REM Build test arguments
set "TEST_ARGS=--gtest_filter=%TEST_FILTER%"
if "%VERBOSE%"=="1" (
    set "TEST_ARGS=!TEST_ARGS! --gtest_brief=0"
)

REM Run tests with coverage if requested
if "%COVERAGE%"=="1" (
    echo Running tests with coverage...
    echo.

    REM Check if OpenCppCoverage is available
    where OpenCppCoverage >nul 2>&1
    if errorlevel 1 (
        echo Warning: OpenCppCoverage not found. Running tests without coverage.
        echo Install from: https://github.com/OpenCppCoverage/OpenCppCoverage
        echo.
        "%TEST_EXE%" !TEST_ARGS!
    ) else (
        REM Run with coverage
        OpenCppCoverage --sources framework\log --export_type html:coverage_html -- "%TEST_EXE%" !TEST_ARGS!

        if errorlevel 0 (
            echo.
            echo Coverage report generated in: coverage_html\index.html
        )
    )
) else (
    REM Run tests normally
    "%TEST_EXE%" !TEST_ARGS!
)

set "EXIT_CODE=%errorlevel%"

echo.
echo ========================================
if "%EXIT_CODE%"=="0" (
    echo All tests passed!
) else (
    echo Some tests failed. Exit code: %EXIT_CODE%
)
echo ========================================

exit /b %EXIT_CODE%
