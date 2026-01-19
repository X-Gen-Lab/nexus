"""
\\file            conftest.py
\\brief           pytest配置文件
\\author          Nexus Team
"""

import sys
from pathlib import Path

# 添加scripts目录到Python路径
scripts_dir = Path(__file__).parent.parent.parent / "scripts"
sys.path.insert(0, str(scripts_dir))
