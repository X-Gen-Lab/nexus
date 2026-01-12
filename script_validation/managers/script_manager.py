"""
脚本管理器

负责脚本发现、分类和管理。
"""

import os
import glob
from pathlib import Path
from typing import List

from ..models import Script, ScriptType, ScriptMetadata, Platform, ScriptCategory
from ..discovery import ScriptDiscovery, ScriptParser, ScriptClassifier


class ScriptManager:
    """脚本管理器"""

    def __init__(self):
        """初始化脚本管理器"""
        self.discovery = ScriptDiscovery()
        self.parser = ScriptParser()
        self.classifier = ScriptClassifier()

    def discover_scripts(self, root_path: Path, patterns: List[str]) -> List[Script]:
        """发现项目中的所有脚本"""
        return self.discovery.discover_scripts(str(root_path), patterns)

    def classify_script_type(self, script_path: Path) -> ScriptType:
        """分类脚本类型"""
        return self.classifier.classify_script_type(script_path)

    def determine_platform(self, script_type: ScriptType) -> Platform:
        """根据脚本类型确定主要平台"""
        return self.classifier.determine_platform(script_type)

    def classify_script_category(self, script_path: Path) -> ScriptCategory:
        """分类脚本功能类别"""
        return self.classifier.classify_script_category(script_path)

    def parse_script_metadata(self, script: Script) -> ScriptMetadata:
        """解析脚本元数据"""
        return self.parser.parse_basic_metadata(script.path)

    def get_script_dependencies(self, script: Script) -> List[str]:
        """获取脚本依赖"""
        return self.parser.extract_dependencies(script)


