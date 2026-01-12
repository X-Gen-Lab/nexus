"""
脚本发现系统

实现脚本发现、解析和分类功能。
"""

import os
import glob
import re
from pathlib import Path
from typing import List, Dict, Any, Optional, Set

from .models import (
    Script, ScriptType, ScriptMetadata, Platform, ScriptCategory, Parameter
)
from .interfaces import ScriptDiscovery as IScriptDiscovery, ScriptParser as IScriptParser, ScriptClassifier as IScriptClassifier


class ScriptDiscovery(IScriptDiscovery):
    """脚本发现类 - 扫描项目目录查找所有脚本文件"""

    def __init__(self, exclude_dirs: Optional[List[str]] = None):
        """
        初始化脚本发现器

        Args:
            exclude_dirs: 要排除的目录列表
        """
        self.exclude_dirs = exclude_dirs or [
            '.git', '.vscode', '__pycache__', 'node_modules',
            'build', '_build', '.pytest_cache', '.hypothesis'
        ]

    def discover_scripts(self, root_path: str, patterns: List[str]) -> List[Script]:
        """
        发现项目中的所有脚本文件

        Args:
            root_path: 项目根目录路径
            patterns: 文件匹配模式列表 (如 ['*.py', '*.sh', '*.bat', '*.ps1'])

        Returns:
            发现的脚本列表
        """
        scripts = []
        root_path_obj = Path(root_path).resolve()

        if not root_path_obj.exists():
            return scripts

        # 递归遍历目录
        for script_path in self._recursive_find_scripts(root_path_obj, patterns):
            try:
                script = self._create_script_object(script_path)
                if script:
                    scripts.append(script)
            except Exception as e:
                # 记录错误但继续处理其他脚本
                print(f"Warning: Failed to process script {script_path}: {e}")
                continue

        return scripts

    def _recursive_find_scripts(self, root_path: Path, patterns: List[str]) -> List[Path]:
        """
        递归查找匹配模式的脚本文件

        Args:
            root_path: 根目录
            patterns: 文件匹配模式

        Returns:
            找到的脚本文件路径列表
        """
        found_files = []

        for pattern in patterns:
            # 使用glob进行递归搜索
            search_pattern = str(root_path / "**" / pattern)
            matched_files = glob.glob(search_pattern, recursive=True)

            for file_path in matched_files:
                path_obj = Path(file_path)

                # 检查是否在排除目录中
                if self._should_exclude_path(path_obj, root_path):
                    continue

                # 检查文件是否可读
                if path_obj.is_file() and os.access(path_obj, os.R_OK):
                    found_files.append(path_obj)

        # 去重并排序
        unique_files = list(set(found_files))
        unique_files.sort()

        return unique_files

    def _should_exclude_path(self, file_path: Path, root_path: Path) -> bool:
        """
        检查路径是否应该被排除

        Args:
            file_path: 文件路径
            root_path: 根目录路径

        Returns:
            是否应该排除
        """
        try:
            # 获取相对路径
            relative_path = file_path.relative_to(root_path)
            path_parts = relative_path.parts

            # 检查路径中是否包含排除的目录
            for exclude_dir in self.exclude_dirs:
                if exclude_dir in path_parts:
                    return True

            return False
        except ValueError:
            # 如果无法计算相对路径，则排除
            return True

    def _create_script_object(self, script_path: Path) -> Optional[Script]:
        """
        创建脚本对象

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本对象或None
        """
        try:
            # 使用分类器确定脚本类型和分类
            classifier = ScriptClassifier()
            script_type = classifier.classify_script_type(script_path)
            script_category = classifier.classify_script_category(script_path)
            platform = classifier.determine_platform(script_type)

            # 使用解析器获取元数据
            parser = ScriptParser()
            metadata = parser.parse_basic_metadata(script_path)
            dependencies = parser.extract_dependencies_from_file(script_path)

            # 创建脚本对象
            script = Script(
                path=script_path,
                name=script_path.stem,
                type=script_type,
                platform=platform,
                metadata=metadata,
                dependencies=dependencies,
                category=script_category
            )

            return script

        except Exception as e:
            print(f"Error creating script object for {script_path}: {e}")
            return None

    def get_script_count_by_type(self, scripts: List[Script]) -> Dict[ScriptType, int]:
        """
        按类型统计脚本数量

        Args:
            scripts: 脚本列表

        Returns:
            按类型分组的脚本数量
        """
        counts = {}
        for script in scripts:
            counts[script.type] = counts.get(script.type, 0) + 1
        return counts

    def get_script_count_by_category(self, scripts: List[Script]) -> Dict[ScriptCategory, int]:
        """
        按分类统计脚本数量

        Args:
            scripts: 脚本列表

        Returns:
            按分类分组的脚本数量
        """
        counts = {}
        for script in scripts:
            counts[script.category] = counts.get(script.category, 0) + 1
        return counts

    def filter_scripts_by_type(self, scripts: List[Script], script_type: ScriptType) -> List[Script]:
        """
        按类型过滤脚本

        Args:
            scripts: 脚本列表
            script_type: 要过滤的脚本类型

        Returns:
            过滤后的脚本列表
        """
        return [script for script in scripts if script.type == script_type]

    def filter_scripts_by_category(self, scripts: List[Script], category: ScriptCategory) -> List[Script]:
        """
        按分类过滤脚本

        Args:
            scripts: 脚本列表
            category: 要过滤的脚本分类

        Returns:
            过滤后的脚本列表
        """
        return [script for script in scripts if script.category == category]


class ScriptParser(IScriptParser):
    """脚本解析器 - 解析脚本元数据和依赖信息"""

    def parse_metadata(self, script: Script) -> Dict[str, Any]:
        """
        解析脚本元数据

        Args:
            script: 脚本对象

        Returns:
            元数据字典
        """
        try:
            with open(script.path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            metadata = {
                'description': self._extract_description(content, script.type),
                'usage': self._extract_usage(content, script.type),
                'parameters': self._extract_parameters(content, script.type),
                'examples': self._extract_examples(content, script.type),
                'author': self._extract_author(content, script.type),
                'version': self._extract_version(content, script.type)
            }

            return metadata

        except Exception as e:
            # 返回默认元数据
            return {
                'description': f"Script: {script.name}",
                'usage': f"Usage: {script.name} [options]",
                'parameters': [],
                'examples': [],
                'author': '',
                'version': '1.0.0'
            }

    def extract_dependencies(self, script: Script) -> List[str]:
        """
        提取脚本依赖

        Args:
            script: 脚本对象

        Returns:
            依赖列表
        """
        return self.extract_dependencies_from_file(script.path)

    def extract_dependencies_from_file(self, script_path: Path) -> List[str]:
        """
        从文件中提取脚本依赖

        Args:
            script_path: 脚本文件路径

        Returns:
            依赖列表
        """
        try:
            with open(script_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            script_type = ScriptClassifier().classify_script_type(script_path)

            if script_type == ScriptType.PYTHON:
                return self._extract_python_dependencies(content)
            elif script_type == ScriptType.SHELL:
                return self._extract_shell_dependencies(content)
            elif script_type == ScriptType.BATCH:
                return self._extract_batch_dependencies(content)
            elif script_type == ScriptType.POWERSHELL:
                return self._extract_powershell_dependencies(content)
            else:
                return []

        except Exception as e:
            return []

    def parse_basic_metadata(self, script_path: Path) -> ScriptMetadata:
        """
        解析基本元数据

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本元数据对象
        """
        try:
            with open(script_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            script_type = ScriptClassifier().classify_script_type(script_path)

            description = self._extract_description(content, script_type)
            usage = self._extract_usage(content, script_type)
            parameters = self._extract_parameters(content, script_type)
            examples = self._extract_examples(content, script_type)
            author = self._extract_author(content, script_type)
            version = self._extract_version(content, script_type)

            return ScriptMetadata(
                description=description,
                usage=usage,
                parameters=parameters,
                examples=examples,
                author=author,
                version=version
            )

        except Exception as e:
            return ScriptMetadata(
                description=f"Script: {script_path.name}",
                usage=f"Usage: {script_path.name} [options]"
            )

    def _extract_description(self, content: str, script_type: ScriptType) -> str:
        """提取脚本描述"""
        lines = content.split('\n')

        # 根据脚本类型使用不同的注释符号
        comment_chars = self._get_comment_chars(script_type)

        for line in lines[:20]:  # 检查前20行
            line = line.strip()
            for comment_char in comment_chars:
                if line.startswith(comment_char) and len(line) > len(comment_char) + 1:
                    desc = line[len(comment_char):].strip()
                    # 跳过usage行和其他特殊行
                    if (desc and not desc.lower().startswith('usage') and
                        not desc.lower().startswith('author') and
                        not desc.lower().startswith('version') and
                        not desc.startswith('!') and len(desc) > 3):
                        return desc

        return "Script description not available"

    def _extract_usage(self, content: str, script_type: ScriptType) -> str:
        """提取使用说明"""
        lines = content.split('\n')
        comment_chars = self._get_comment_chars(script_type)

        for line in lines:
            line_lower = line.lower().strip()
            for comment_char in comment_chars:
                if line_lower.startswith(comment_char.lower()) and 'usage' in line_lower:
                    return line.strip()

        return "Usage information not available"

    def _extract_parameters(self, content: str, script_type: ScriptType) -> List[Parameter]:
        """提取参数信息"""
        parameters = []

        if script_type == ScriptType.PYTHON:
            # 查找argparse参数定义
            import_argparse = 'import argparse' in content or 'from argparse' in content
            if import_argparse:
                # 简单的参数提取逻辑
                lines = content.split('\n')
                for line in lines:
                    if 'add_argument' in line and '--' in line:
                        param_match = re.search(r'--(\w+)', line)
                        if param_match:
                            param_name = param_match.group(1)
                            help_match = re.search(r'help=["\']([^"\']+)["\']', line)
                            help_text = help_match.group(1) if help_match else ""
                            required = 'required=True' in line

                            parameters.append(Parameter(
                                name=param_name,
                                description=help_text,
                                required=required
                            ))

        return parameters

    def _extract_examples(self, content: str, script_type: ScriptType) -> List[str]:
        """提取使用示例"""
        examples = []
        lines = content.split('\n')
        comment_chars = self._get_comment_chars(script_type)

        in_example_section = False
        for line in lines:
            line_stripped = line.strip()
            line_lower = line_stripped.lower()

            # 检查是否进入示例部分
            for comment_char in comment_chars:
                if line_lower.startswith(comment_char.lower()) and ('example' in line_lower or '示例' in line_lower):
                    in_example_section = True
                    continue

                if in_example_section and line_stripped.startswith(comment_char):
                    example_text = line_stripped[len(comment_char):].strip()
                    if example_text and not example_text.lower().startswith('example'):
                        examples.append(example_text)
                elif in_example_section and not line_stripped.startswith(comment_char) and line_stripped:
                    # 退出示例部分
                    break

        return examples

    def _extract_author(self, content: str, script_type: ScriptType) -> str:
        """提取作者信息"""
        lines = content.split('\n')
        comment_chars = self._get_comment_chars(script_type)

        for line in lines[:30]:  # 检查前30行
            line_lower = line.lower().strip()
            for comment_char in comment_chars:
                if line_lower.startswith(comment_char.lower()) and ('author' in line_lower or '作者' in line_lower):
                    author_match = re.search(r'author[:\s]+([^,\n]+)', line, re.IGNORECASE)
                    if author_match:
                        return author_match.group(1).strip()

        return ""

    def _extract_version(self, content: str, script_type: ScriptType) -> str:
        """提取版本信息"""
        lines = content.split('\n')
        comment_chars = self._get_comment_chars(script_type)

        for line in lines[:30]:  # 检查前30行
            line_lower = line.lower().strip()
            for comment_char in comment_chars:
                if line_lower.startswith(comment_char.lower()) and ('version' in line_lower or '版本' in line_lower):
                    version_match = re.search(r'version[:\s]+([0-9.]+)', line, re.IGNORECASE)
                    if version_match:
                        return version_match.group(1).strip()

        return "1.0.0"

    def _get_comment_chars(self, script_type: ScriptType) -> List[str]:
        """获取脚本类型对应的注释符号"""
        if script_type == ScriptType.PYTHON:
            return ['#']
        elif script_type == ScriptType.SHELL:
            return ['#']
        elif script_type == ScriptType.BATCH:
            return ['REM', 'rem', '::']
        elif script_type == ScriptType.POWERSHELL:
            return ['#', '<#']
        else:
            return ['#']

    def _extract_python_dependencies(self, content: str) -> List[str]:
        """提取Python脚本依赖"""
        dependencies = set()
        lines = content.split('\n')

        for line in lines:
            line = line.strip()

            # 处理import语句
            if line.startswith('import ') and not line.startswith('import os') and not line.startswith('import sys'):
                parts = line.split('import ')
                if len(parts) > 1:
                    module = parts[1].split()[0].split('.')[0].split(',')[0]
                    if not self._is_standard_library(module):
                        dependencies.add(module)

            # 处理from import语句
            elif line.startswith('from ') and ' import ' in line:
                parts = line.split('from ')[1].split(' import ')[0].strip()
                module = parts.split('.')[0]
                if not self._is_standard_library(module):
                    dependencies.add(module)

        return list(dependencies)

    def _extract_shell_dependencies(self, content: str) -> List[str]:
        """提取Shell脚本依赖"""
        dependencies = set()

        # 常见的外部命令
        common_commands = [
            'git', 'cmake', 'make', 'gcc', 'g++', 'clang', 'python', 'python3',
            'node', 'npm', 'yarn', 'docker', 'curl', 'wget', 'jq', 'sed', 'awk'
        ]

        content_lower = content.lower()
        for cmd in common_commands:
            if f' {cmd} ' in content_lower or f'\n{cmd} ' in content_lower or content_lower.startswith(f'{cmd} '):
                dependencies.add(cmd)

        return list(dependencies)

    def _extract_batch_dependencies(self, content: str) -> List[str]:
        """提取Batch脚本依赖"""
        dependencies = set()

        # 常见的Windows命令
        common_commands = [
            'git', 'cmake', 'msbuild', 'python', 'node', 'npm', 'docker', 'curl'
        ]

        content_lower = content.lower()
        for cmd in common_commands:
            if f' {cmd} ' in content_lower or f'\n{cmd} ' in content_lower or content_lower.startswith(f'{cmd} '):
                dependencies.add(cmd)

        return list(dependencies)

    def _extract_powershell_dependencies(self, content: str) -> List[str]:
        """提取PowerShell脚本依赖"""
        dependencies = set()
        lines = content.split('\n')

        for line in lines:
            line = line.strip()

            # 处理Import-Module语句
            if line.startswith('Import-Module'):
                parts = line.split()
                if len(parts) > 1:
                    module = parts[1].strip('"\'')
                    dependencies.add(module)

            # 处理常见命令
            common_commands = ['git', 'cmake', 'msbuild', 'python', 'node', 'npm', 'docker']
            line_lower = line.lower()
            for cmd in common_commands:
                if f' {cmd} ' in line_lower or line_lower.startswith(f'{cmd} '):
                    dependencies.add(cmd)

        return list(dependencies)

    def _is_standard_library(self, module: str) -> bool:
        """检查是否为Python标准库模块"""
        standard_modules = {
            'os', 'sys', 'pathlib', 'typing', 'collections', 'itertools', 'functools',
            'json', 'csv', 're', 'datetime', 'time', 'math', 'random', 'urllib',
            'http', 'email', 'html', 'xml', 'sqlite3', 'logging', 'unittest',
            'argparse', 'configparser', 'subprocess', 'threading', 'multiprocessing',
            'asyncio', 'socket', 'ssl', 'hashlib', 'base64', 'pickle', 'copy',
            'enum', 'dataclasses', 'abc', 'contextlib', 'warnings', 'traceback'
        }
        return module in standard_modules


class ScriptClassifier(IScriptClassifier):
    """脚本分类器 - 根据文件扩展名和内容分类脚本类型和功能"""

    def classify_script(self, script: Script) -> str:
        """
        分类脚本

        Args:
            script: 脚本对象

        Returns:
            脚本分类字符串
        """
        return script.category.value

    def classify_script_type(self, script_path: Path) -> ScriptType:
        """
        根据文件扩展名分类脚本类型

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本类型
        """
        suffix = script_path.suffix.lower()

        if suffix == '.bat':
            return ScriptType.BATCH
        elif suffix == '.ps1':
            return ScriptType.POWERSHELL
        elif suffix == '.sh':
            return ScriptType.SHELL
        elif suffix == '.py':
            return ScriptType.PYTHON
        else:
            # 检查文件内容来确定类型
            return self._classify_by_content(script_path)

    def classify_script_category(self, script_path: Path) -> ScriptCategory:
        """
        根据文件名和内容分类脚本功能类别

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本功能分类
        """
        name_lower = script_path.name.lower()

        # 基于文件名的分类
        if any(keyword in name_lower for keyword in ['build', 'compile', 'make']):
            return ScriptCategory.BUILD
        elif any(keyword in name_lower for keyword in ['test', 'check', 'verify']):
            return ScriptCategory.TEST
        elif any(keyword in name_lower for keyword in ['format', 'lint', 'style']):
            return ScriptCategory.FORMAT
        elif any(keyword in name_lower for keyword in ['clean', 'clear', 'remove']):
            return ScriptCategory.CLEAN
        elif any(keyword in name_lower for keyword in ['doc', 'docs', 'documentation']):
            return ScriptCategory.DOCS
        elif any(keyword in name_lower for keyword in ['setup', 'install', 'init', 'configure']):
            return ScriptCategory.SETUP
        elif any(keyword in name_lower for keyword in ['ci', 'deploy', 'release']):
            return ScriptCategory.CI
        else:
            # 尝试基于内容分类
            return self._classify_by_content_analysis(script_path)

    def determine_platform(self, script_type: ScriptType) -> Platform:
        """
        根据脚本类型确定主要平台

        Args:
            script_type: 脚本类型

        Returns:
            主要平台
        """
        if script_type == ScriptType.BATCH:
            return Platform.WINDOWS
        elif script_type == ScriptType.POWERSHELL:
            return Platform.WINDOWS
        elif script_type == ScriptType.SHELL:
            return Platform.LINUX
        elif script_type == ScriptType.PYTHON:
            # Python脚本可以跨平台，默认为Linux
            return Platform.LINUX
        else:
            return Platform.LINUX

    def _classify_by_content(self, script_path: Path) -> ScriptType:
        """
        通过文件内容分类脚本类型

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本类型
        """
        try:
            with open(script_path, 'r', encoding='utf-8', errors='ignore') as f:
                first_line = f.readline().strip()

            # 检查shebang
            if first_line.startswith('#!'):
                if 'python' in first_line:
                    return ScriptType.PYTHON
                elif 'bash' in first_line or 'sh' in first_line:
                    return ScriptType.SHELL
                elif 'powershell' in first_line or 'pwsh' in first_line:
                    return ScriptType.POWERSHELL

            # 默认为shell脚本
            return ScriptType.SHELL

        except Exception:
            return ScriptType.SHELL

    def _classify_by_content_analysis(self, script_path: Path) -> ScriptCategory:
        """
        通过内容分析分类脚本功能

        Args:
            script_path: 脚本文件路径

        Returns:
            脚本功能分类
        """
        try:
            with open(script_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read().lower()

            # 基于内容关键词的分类
            if any(keyword in content for keyword in ['cmake', 'make', 'gcc', 'g++', 'compile', 'build']):
                return ScriptCategory.BUILD
            elif any(keyword in content for keyword in ['pytest', 'unittest', 'test', 'assert']):
                return ScriptCategory.TEST
            elif any(keyword in content for keyword in ['black', 'flake8', 'pylint', 'format']):
                return ScriptCategory.FORMAT
            elif any(keyword in content for keyword in ['rm -rf', 'del', 'clean', 'remove']):
                return ScriptCategory.CLEAN
            elif any(keyword in content for keyword in ['doxygen', 'sphinx', 'documentation']):
                return ScriptCategory.DOCS
            elif any(keyword in content for keyword in ['pip install', 'apt-get', 'yum install', 'setup']):
                return ScriptCategory.SETUP
            elif any(keyword in content for keyword in ['deploy', 'release', 'ci', 'cd']):
                return ScriptCategory.CI
            else:
                return ScriptCategory.UTILITY

        except Exception:
            return ScriptCategory.UTILITY

    def get_supported_platforms(self, script: Script) -> List[Platform]:
        """
        获取脚本支持的平台列表

        Args:
            script: 脚本对象

        Returns:
            支持的平台列表
        """
        if script.type == ScriptType.PYTHON:
            # Python脚本通常支持所有平台
            return [Platform.WINDOWS, Platform.WSL, Platform.LINUX]
        elif script.type == ScriptType.BATCH:
            # Batch脚本只支持Windows
            return [Platform.WINDOWS]
        elif script.type == ScriptType.POWERSHELL:
            # PowerShell脚本主要支持Windows，但也可能支持Linux
            return [Platform.WINDOWS, Platform.WSL]
        elif script.type == ScriptType.SHELL:
            # Shell脚本支持WSL和Linux
            return [Platform.WSL, Platform.LINUX]
        else:
            return [Platform.LINUX]
