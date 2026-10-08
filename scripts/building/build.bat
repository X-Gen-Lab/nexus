@echo off
python "%~dp0..\ci\ci_build.py" --stage build %*
exit /b %ERRORLEVEL%
