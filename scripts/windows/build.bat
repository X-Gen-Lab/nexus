@echo off
REM Nexus Build Script for Windows
REM Usage: build.bat [debug|release] [clean]

setlocal enabledelayedexpansion

set BUILD_TYPE=Debug
set CLEAN=0

:parse_args
if "%1"=="" goto :start_build
if /i "%1"=="debug" set BUILD_TYPE=Debug
if /i "%1"=="release" set BUILD_TYPE=Release
if /i "%1"=="clean" set CLEAN=1
shift
goto :parse_args

:start_build
set BUILD_DIR=build-%BUILD_TYPE%

echo ========================================
echo Nexus Build Script
echo Build Type: %BUILD_TYPE%
echo Build Dir:  %BUILD_DIR%
echo ========================================

if %CLEAN%==1 (
    echo Cleaning build directory...
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

if not exist %BUILD_DIR% (
    echo Creating build directory...
    mkdir %BUILD_DIR%
)

cd %BUILD_DIR%

echo Configuring CMake...
cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DNEXUS_BUILD_TESTS=ON ..
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building...
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo ========================================
echo Build completed successfully!
echo ========================================

cd ..
