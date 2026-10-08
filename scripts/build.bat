@echo off
python "%~dp0ci\ci_build.py" --stage build %*
exit /b %ERRORLEVEL%
