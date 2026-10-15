@echo off
setlocal
REM All options, including the legacy check argument, are handled by Python.
where py >nul 2>&1
if not errorlevel 1 goto with_py
where python >nul 2>&1
if not errorlevel 1 goto with_python
echo ERROR: Python 3 was not found. 1>&2
exit /b 2

:with_py
py -3 "%~dp0format.py" %*
exit /b %errorlevel%

:with_python
python "%~dp0format.py" %*
exit /b %errorlevel%
