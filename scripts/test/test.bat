@echo off
python "%~dp0..\ci\ci_build.py" --stage test %*
exit /b %ERRORLEVEL%
