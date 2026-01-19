"""
\\file            conftest.py
\\brief           pytest根配置文件
\\author          Nexus Team
"""

import sys
from pathlib import Path

# 添加scripts目录到Python路径（在导入之前）
scripts_dir = Path(__file__).parent.parent / "scripts"
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))
    print(f"Added {scripts_dir} to sys.path")
