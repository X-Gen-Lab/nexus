@echo off
REM Nexus Clean Script for Windows
REM Usage: clean.bat [all]

setlocal

echo ========================================
echo Nexus Clean Script
echo ========================================

echo Removing build directories...
if exist build rmdir /s /q build
if exist build-Debug rmdir /s /q build-Debug
if exist build-Release rmdir /s /q build-Release
if exist build-test rmdir /s /q build-test
if exist build-check rmdir /s /q build-check
if exist build-verify rmdir /s /q build-verify
if exist out rmdir /s /q out

if /i "%1"=="all" (
    echo Removing generated documentation...
    if exist docs\api\html rmdir /s /q docs\api\html
    if exist docs\api\xml rmdir /s /q docs\api\xml
    if exist docs\sphinx\_build rmdir /s /q docs\sphinx\_build
    
    echo Removing test artifacts...
    if exist Testing rmdir /s /q Testing
    del /q test_results.xml 2>nul
)

echo ========================================
echo Clean complete!
echo ========================================
