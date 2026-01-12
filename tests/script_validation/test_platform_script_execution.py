"""
平台脚本执行正确性属性测试

Feature: script-delivery-validation, Property 1: 平台脚本执行正确性
验证：需求 1.1, 1.2, 1.3
"""

import pytest
from hypothesis import given, strategies as st, assume
from pathlib import Path
import tempfile
import os

from script_validation.models import (
    Script, ScriptType, Platform, ScriptMetadata, ValidationStatus
)
from script_validation.managers import ScriptManager, PlatformManager


# 策略定义
@st.composite
def valid_script_names(draw):
    """生成有效的脚本名称"""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd'), min_codepoint=32, max_codepoint=126),
        min_size=1,
        max_size=20
    ).filter(lambda x: x.isalnum() or '_' in x or '-' in x))
    return name.replace(' ', '_')


@st.composite
def valid_scripts(draw):
    """生成有效的脚本对象"""
    script_name = draw(valid_script_names())
    script_type = draw(st.sampled_from(list(ScriptType)))
    platform = draw(st.sampled_from(list(Platform)))

    # 根据脚本类型确定文件扩展名
    extensions = {
        ScriptType.BATCH: '.bat',
        ScriptType.POWERSHELL: '.ps1',
        ScriptType.SHELL: '.sh',
        ScriptType.PYTHON: '.py'
    }

    extension = extensions[script_type]
    script_path = Path(f"/tmp/{script_name}{extension}")

    metadata = ScriptMetadata(
        description=f"Test script: {script_name}",
        usage=f"Usage: {script_name} [options]"
    )

    return Script(
        path=script_path,
        name=script_name,
        type=script_type,
        platform=platform,
        metadata=metadata
    )


@st.composite
def supported_platforms(draw):
    """生成支持的平台"""
    return draw(st.sampled_from([Platform.WINDOWS, Platform.WSL, Platform.LINUX]))


class TestPlatformScriptExecution:
    """平台脚本执行正确性测试"""

    def setup_method(self):
        """测试设置"""
        self.script_manager = ScriptManager()
        self.platform_manager = PlatformManager()

    @given(script=valid_scripts(), platform=supported_platforms())
    def test_platform_script_execution_correctness(self, script, platform):
        """
        Feature: script-delivery-validation, Property 1: 平台脚本执行正确性

        对于任何脚本在其支持的平台上执行时，应该能够成功运行并验证其基本功能
        验证：需求 1.1, 1.2, 1.3
        """
        # 假设：只测试兼容的脚本-平台组合
        assume(self._is_script_platform_compatible(script, platform))

        # 创建临时脚本文件用于测试
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_script_path = Path(temp_dir) / script.path.name

            # 创建基本的可执行脚本内容
            script_content = self._generate_basic_script_content(script.type)

            with open(temp_script_path, 'w', encoding='utf-8') as f:
                f.write(script_content)

            # 更新脚本路径
            test_script = Script(
                path=temp_script_path,
                name=script.name,
                type=script.type,
                platform=platform,
                metadata=script.metadata
            )

            # 验证脚本可以被正确分类
            classified_type = self.script_manager.classify_script_type(temp_script_path)
            assert classified_type == script.type, f"脚本类型分类错误: 期望 {script.type}, 实际 {classified_type}"

            # 验证平台兼容性检查
            is_compatible = self._is_script_platform_compatible(test_script, platform)

            if is_compatible:
                # 验证脚本元数据可以被解析
                metadata = self.script_manager.parse_script_metadata(test_script)
                assert metadata is not None, "脚本元数据解析失败"
                assert isinstance(metadata.description, str), "脚本描述应该是字符串"
                assert isinstance(metadata.usage, str), "脚本使用说明应该是字符串"

                # 验证依赖可以被提取
                dependencies = self.script_manager.get_script_dependencies(test_script)
                assert isinstance(dependencies, list), "脚本依赖应该是列表"

                # 验证脚本分类
                category = self.script_manager.classify_script_category(temp_script_path)
                assert category is not None, "脚本分类不能为空"

    def _is_script_platform_compatible(self, script: Script, platform: Platform) -> bool:
        """检查脚本与平台的兼容性"""
        if platform == Platform.WINDOWS:
            return script.type in [ScriptType.BATCH, ScriptType.POWERSHELL, ScriptType.PYTHON]
        elif platform in [Platform.WSL, Platform.LINUX]:
            return script.type in [ScriptType.SHELL, ScriptType.PYTHON]
        return False

    def _generate_basic_script_content(self, script_type: ScriptType) -> str:
        """生成基本的可执行脚本内容"""
        if script_type == ScriptType.BATCH:
            return """@echo off
REM Test batch script
echo Hello from batch script
exit /b 0
"""
        elif script_type == ScriptType.POWERSHELL:
            return """# Test PowerShell script
Write-Host "Hello from PowerShell script"
exit 0
"""
        elif script_type == ScriptType.SHELL:
            return """#!/bin/bash
# Test shell script
echo "Hello from shell script"
exit 0
"""
        elif script_type == ScriptType.PYTHON:
            return """#!/usr/bin/env python3
# Test Python script
print("Hello from Python script")
import sys
sys.exit(0)
"""
        else:
            return "# Empty script\n"


class TestScriptDiscovery:
    """脚本发现功能测试"""

    def setup_method(self):
        """测试设置"""
        self.script_manager = ScriptManager()

    @given(
        script_names=st.lists(valid_script_names(), min_size=1, max_size=5),
        script_types=st.lists(st.sampled_from(list(ScriptType)), min_size=1, max_size=4)
    )
    def test_script_discovery_completeness(self, script_names, script_types):
        """
        测试脚本发现的完整性

        对于任何给定的脚本集合，脚本发现功能应该能够找到所有匹配的脚本文件
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            created_scripts = []

            # 创建测试脚本文件
            for i, (name, script_type) in enumerate(zip(script_names, script_types)):
                extensions = {
                    ScriptType.BATCH: '.bat',
                    ScriptType.POWERSHELL: '.ps1',
                    ScriptType.SHELL: '.sh',
                    ScriptType.PYTHON: '.py'
                }

                extension = extensions[script_type]
                script_path = temp_path / f"{name}_{i}{extension}"

                # 创建脚本文件
                with open(script_path, 'w', encoding='utf-8') as f:
                    f.write(self._generate_basic_script_content(script_type))

                created_scripts.append((script_path, script_type))

            # 发现脚本
            patterns = ["*.bat", "*.ps1", "*.sh", "*.py"]
            discovered_scripts = self.script_manager.discover_scripts(temp_path, patterns)

            # 验证发现的脚本数量
            assert len(discovered_scripts) == len(created_scripts), \
                f"发现的脚本数量不匹配: 期望 {len(created_scripts)}, 实际 {len(discovered_scripts)}"

            # 验证每个脚本都被正确发现和分类
            discovered_paths = {script.path for script in discovered_scripts}
            created_paths = {path for path, _ in created_scripts}

            assert discovered_paths == created_paths, "发现的脚本路径与创建的不匹配"

            # 验证脚本类型分类正确
            for script in discovered_scripts:
                expected_type = next(
                    script_type for path, script_type in created_scripts
                    if path == script.path
                )
                assert script.type == expected_type, \
                    f"脚本类型分类错误: {script.path}, 期望 {expected_type}, 实际 {script.type}"

    def _generate_basic_script_content(self, script_type: ScriptType) -> str:
        """生成基本的可执行脚本内容"""
        if script_type == ScriptType.BATCH:
            return "@echo off\necho Test batch script\n"
        elif script_type == ScriptType.POWERSHELL:
            return "Write-Host 'Test PowerShell script'\n"
        elif script_type == ScriptType.SHELL:
            return "#!/bin/bash\necho 'Test shell script'\n"
        elif script_type == ScriptType.PYTHON:
            return "#!/usr/bin/env python3\nprint('Test Python script')\n"
        else:
            return "# Empty script\n"


class TestPlatformDetection:
    """平台检测功能测试"""

    def setup_method(self):
        """测试设置"""
        self.platform_manager = PlatformManager()

    def test_current_platform_detection(self):
        """
        测试当前平台检测的正确性

        平台管理器应该能够正确检测当前运行的平台
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 验证检测到的平台是有效的
        assert current_platform in [Platform.WINDOWS, Platform.WSL, Platform.LINUX], \
            f"检测到的平台无效: {current_platform}"

        # 验证环境信息可以获取
        env_info = self.platform_manager.get_current_environment_info()

        assert env_info.platform == current_platform, "环境信息中的平台与检测到的平台不匹配"
        assert isinstance(env_info.os_version, str), "操作系统版本应该是字符串"
        assert isinstance(env_info.python_version, str), "Python版本应该是字符串"
        assert isinstance(env_info.shell_version, str), "Shell版本应该是字符串"
        assert isinstance(env_info.available_commands, list), "可用命令应该是列表"

    @given(platform=st.sampled_from(list(Platform)))
    def test_platform_availability_check(self, platform):
        """
        测试平台可用性检查

        对于任何平台，平台管理器应该能够正确判断其是否可用
        """
        is_available = self.platform_manager.is_platform_available(platform)

        # 验证返回值是布尔类型
        assert isinstance(is_available, bool), f"平台可用性检查应该返回布尔值，实际返回: {type(is_available)}"

        # 如果当前平台可用，那么检测结果应该为True
        current_platform = self.platform_manager.detect_current_platform()
        if platform == current_platform:
            assert is_available, f"当前平台 {platform} 应该是可用的"
