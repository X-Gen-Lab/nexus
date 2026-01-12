@echo off
REM Nexus Documentation Generator for Windows
REM Usage: docs.bat [doxygen|sphinx|all]

setlocal

set TARGET=%1
if "%TARGET%"=="" set TARGET=all

echo ========================================
echo Nexus Documentation Generator
echo Target: %TARGET%
echo ========================================

if /i "%TARGET%"=="doxygen" goto :doxygen
if /i "%TARGET%"=="sphinx" goto :sphinx
if /i "%TARGET%"=="all" goto :all
echo Unknown target: %TARGET%
exit /b 1

:doxygen
echo Generating Doxygen documentation...
where doxygen >nul 2>&1
if errorlevel 1 (
    echo Doxygen not found! Please install Doxygen.
    exit /b 1
)
doxygen Doxyfile
if errorlevel 1 (
    echo Doxygen generation failed!
    exit /b 1
)
echo Doxygen documentation generated in docs/api/html
goto :done

:sphinx
echo Generating Sphinx documentation...
where sphinx-build >nul 2>&1
if errorlevel 1 (
    echo Sphinx not found! Please install Sphinx.
    exit /b 1
)
cd docs\sphinx
call build_docs.bat
cd ..\..
echo Sphinx documentation generated in docs/sphinx/_build
goto :done

:all
call :doxygen
call :sphinx
goto :done

:done
echo ========================================
echo Documentation generation complete!
echo ========================================
