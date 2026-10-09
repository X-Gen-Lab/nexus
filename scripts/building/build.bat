@echo off
python "%~dp0..\ci\ci_build.py" %*
exit /b %ERRORLEVEL%
