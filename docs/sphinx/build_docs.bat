@echo off
REM Build Nexus Documentation (English and Chinese)
REM 构建 Nexus 文档（中英文）

echo Building English documentation...
python -m sphinx -b html -c . . _build/html/en

echo Building Chinese documentation...
python -m sphinx -b html -c . . _build/html/cn -D master_doc=index_cn -D "project=Nexus 嵌入式平台" -D language=zh_CN -D "exclude_patterns=['_build','Thumbs.db','.DS_Store','conf_cn.py','index.rst','getting_started/introduction.rst','getting_started/installation.rst','getting_started/quickstart.rst','user_guide/architecture.rst','user_guide/hal.rst','user_guide/osal.rst','user_guide/porting.rst','development/contributing.rst','development/coding_standards.rst','development/testing.rst']"

echo Creating language selection page...
(
echo ^<!DOCTYPE html^>
echo ^<html^>
echo ^<head^>
echo     ^<meta charset="UTF-8"^>
echo     ^<title^>Nexus Documentation^</title^>
echo     ^<style^>
echo         body { font-family: sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: #f5f5f5; }
echo         .container { text-align: center; padding: 40px; background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba^(0,0,0,0.1^); }
echo         a { display: inline-block; margin: 10px; padding: 12px 24px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; }
echo     ^</style^>
echo ^</head^>
echo ^<body^>
echo     ^<div class="container"^>
echo         ^<h1^>Nexus Embedded Platform^</h1^>
echo         ^<p^>Select language / 选择语言^</p^>
echo         ^<a href="en/index.html"^>English^</a^>
echo         ^<a href="cn/index_cn.html"^>中文^</a^>
echo     ^</div^>
echo ^</body^>
echo ^</html^>
) > _build/html/index.html

echo.
echo Done! Documentation built to _build/html/
echo   Language selection: _build/html/index.html
echo   English: _build/html/en/index.html
echo   Chinese: _build/html/cn/index_cn.html
