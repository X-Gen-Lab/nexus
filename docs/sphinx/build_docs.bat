@echo off
chcp 65001 >nul
REM Build Nexus Documentation (English and Chinese)

echo ========================================
echo Building Nexus Documentation
echo ========================================

REM Create output directories
if not exist "_build\html\en" mkdir "_build\html\en"
if not exist "_build\html\cn" mkdir "_build\html\cn"

echo.
echo [1/3] Building English documentation...
python -m sphinx -b html . _build/html/en
if errorlevel 1 (
    echo ERROR: English build failed!
    goto :error
)

echo.
echo [2/3] Building Chinese documentation...
python -m sphinx -b html -c . . _build/html/cn -D master_doc=index_cn -D language=zh_CN -D exclude_patterns="['_build','Thumbs.db','.DS_Store','index.rst','getting_started/introduction.rst','getting_started/installation.rst','getting_started/quickstart.rst','user_guide/architecture.rst','user_guide/hal.rst','user_guide/osal.rst','user_guide/log.rst','user_guide/porting.rst','development/contributing.rst','development/coding_standards.rst','development/testing.rst','conf_cn.py']"
if errorlevel 1 (
    echo ERROR: Chinese build failed!
    goto :error
)

echo.
echo [3/3] Creating language selection page...
(
echo ^<!DOCTYPE html^>
echo ^<html^>
echo ^<head^>
echo     ^<meta charset="UTF-8"^>
echo     ^<meta http-equiv="refresh" content="0; url=en/index.html"^>
echo     ^<title^>Nexus Documentation^</title^>
echo     ^<style^>
echo         body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
echo         body { display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: #f5f5f5; }
echo         .container { text-align: center; padding: 40px; background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
echo         h1 { color: #333; margin-bottom: 20px; }
echo         p { color: #666; margin-bottom: 20px; }
echo         a { display: inline-block; margin: 10px; padding: 12px 24px; background: #0066cc; color: white; text-decoration: none; border-radius: 4px; }
echo         a:hover { background: #0052a3; }
echo     ^</style^>
echo ^</head^>
echo ^<body^>
echo     ^<div class="container"^>
echo         ^<h1^>Nexus Embedded Platform^</h1^>
echo         ^<p^>Select language:^</p^>
echo         ^<a href="en/index.html"^>English^</a^>
echo         ^<a href="cn/index_cn.html"^>Chinese^</a^>
echo     ^</div^>
echo ^</body^>
echo ^</html^>
) > _build\html\index.html

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Output:
echo   Language selection: _build/html/index.html
echo   English docs:       _build/html/en/index.html
echo   Chinese docs:       _build/html/cn/index_cn.html
echo.
goto :end

:error
echo.
echo Build failed with errors.
exit /b 1

:end
