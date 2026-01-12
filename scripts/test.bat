@echo off
REM Nexus Test Script for Windows
REM Usage: test.bat [filter] [verbose]

setlocal enabledelayedexpansion

set BUILD_DIR=build-Debug
set FILTER=*
set VERBOSE=0

:parse_args
if "%1"=="" goto :run_tests
if /i "%1"=="verbose" (
    set VERBOSE=1
) else (
    set FILTER=%1
)
shift
goto :parse_args

:run_tests
set TEST_EXE=%BUILD_DIR%\tests\Debug\nexus_tests.exe

echo ========================================
echo Nexus Test Runner
echo Filter: %FILTER%
echo ========================================

if not exist %TEST_EXE% (
    echo Test executable not found!
    echo Please run build.bat first.
    exit /b 1
)

if %VERBOSE%==1 (
    %TEST_EXE% --gtest_filter=%FILTER% --gtest_color=yes
) else (
    %TEST_EXE% --gtest_filter=%FILTER% --gtest_color=yes --gtest_brief=1
)

if errorlevel 1 (
    echo ========================================
    echo Some tests failed!
    echo ========================================
    exit /b 1
)

echo ========================================
echo All tests passed!
echo ========================================
