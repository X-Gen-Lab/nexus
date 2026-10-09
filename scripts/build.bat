@echo off
python "%~dp0ci\ci_build.py" %*
exit /b %ERRORLEVEL%
