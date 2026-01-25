@echo off
REM ##########################################################################
REM Config Manager Test Runner Script (Windows)
REM ##########################################################################

setlocal enabledelayedexpansion

REM Script directory
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..\..
set BUILD_DIR=%PROJECT_ROOT%\build

REM Default options
set ENABLE_COVERAGE=0
set VERBOSE=0
set TEST_FILTER=
set PARALLEL_JOBS=4

REM ##########################################################################
REM Parse Arguments
REM ##########################################################################

:parse_args
if "%~1"=="" goto end_parse_args
if /i "%~1"=="-c" set ENABLE_COVERAGE=1
if /i "%~1"=="--coverage" set ENABLE_COVERAGE=1
if /i "%~1"=="-f" (
    set TEST_FILTER=%~2
    shift
)
if /i "%~1"=="--filter" (
    set TEST_FILTER=%~2
    shift
)
if /i "%~1"=="-j" (
    set PARALLEL_JOBS=%~2
    shift
)
if /i "%~1"=="--jobs" (
    set PARALLEL_JOBS=%~2
    shift
)
if /i "%~1"=="-V" set VERBOSE=1
if /i "%~1"=="--verbose" set VERBOSE=1
if /i "%~1"=="-h" goto show_usage
if /i "%~1"=="--help" goto show_usage
shift
goto parse_args

:end_parse_args

REM ##########################################################################
REM Build Tests
REM ##########################################################################

echo ========================================
echo Building Config Manager Tests
echo ========================================
echo.

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM CMake options
set CMAKE_OPTS=-DCMAKE_BUILD_TYPE=Debug

if %ENABLE_COVERAGE%==1 (
    set CMAKE_OPTS=%CMAKE_OPTS% -DENABLE_COVERAGE=ON
    echo [INFO] Coverage enabled
)

REM Configure
echo [INFO] Configuring...
cmake %CMAKE_OPTS% .. || (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

REM Build
echo [INFO] Building...
cmake --build . --target config_tests --config Debug -j %PARALLEL_JOBS% || (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] Build completed
echo.

REM ##########################################################################
REM Run Tests
REM ##########################################################################

echo ========================================
echo Running Config Manager Tests
echo ========================================
echo.

REM Test options
set TEST_OPTS=

if not "%TEST_FILTER%"=="" (
    set TEST_OPTS=%TEST_OPTS% --gtest_filter=%TEST_FILTER%
    echo [INFO] Filter: %TEST_FILTER%
)

if %VERBOSE%==1 (
    set TEST_OPTS=%TEST_OPTS% --gtest_print_time=1
)

REM Run tests
tests\Debug\config_tests.exe %TEST_OPTS% || (
    echo [ERROR] Tests failed
    exit /b 1
)

echo [SUCCESS] All tests passed
echo.

REM ##########################################################################
REM Generate Coverage Report (if enabled)
REM ##########################################################################

if %ENABLE_COVERAGE%==1 (
    echo ========================================
    echo Generating Coverage Report
    echo ========================================
    echo.

    REM Check for OpenCppCoverage
    where OpenCppCoverage >nul 2>&1
    if errorlevel 1 (
        echo [WARNING] OpenCppCoverage not found, skipping coverage report
        echo [INFO] Install from: https://github.com/OpenCppCoverage/OpenCppCoverage
    ) else (
        echo [INFO] Generating coverage report...

        OpenCppCoverage ^
            --sources "%PROJECT_ROOT%\framework\config" ^
            --excluded_sources "%PROJECT_ROOT%\tests" ^
            --export_type html:coverage_html ^
            --export_type cobertura:coverage.xml ^
            -- tests\Debug\config_tests.exe %TEST_OPTS%

        if errorlevel 1 (
            echo [ERROR] Failed to generate coverage report
            exit /b 1
        )

        echo [SUCCESS] Coverage report generated: %BUILD_DIR%\coverage_html\index.html
    )
    echo.
)

REM ##########################################################################
REM Summary
REM ##########################################################################

echo ========================================
echo Test Summary
echo ========================================
echo.
echo Build directory: %BUILD_DIR%
echo Test executable: %BUILD_DIR%\tests\Debug\config_tests.exe

if %ENABLE_COVERAGE%==1 (
    echo Coverage report: %BUILD_DIR%\coverage_html\index.html
)

echo.
echo [SUCCESS] All operations completed successfully
goto :eof

REM ##########################################################################
REM Show Usage
REM ##########################################################################

:show_usage
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo     -c, --coverage          Enable code coverage analysis
echo     -f, --filter PATTERN    Run only tests matching PATTERN
echo     -j, --jobs N            Number of parallel jobs (default: 4)
echo     -V, --verbose           Verbose output
echo     -h, --help              Show this help message
echo.
echo Examples:
echo     %~nx0                      # Run all tests
echo     %~nx0 -c                   # Run with coverage
echo     %~nx0 -f "ConfigStore*"    # Run only ConfigStore tests
echo.
exit /b 0
