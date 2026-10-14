@echo off
REM Legacy forwarding entry; no independent generation or default config.
setlocal
if not defined NEXUS_PYTHON set "NEXUS_PYTHON=python"
"%NEXUS_PYTHON%" "%~dp0docs.py" %*
exit /b %ERRORLEVEL%
