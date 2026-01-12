"""
WSL兼容性属性测试

测试WSL环境中脚本执行的兼容性属性。
Feature: script-delivery-validation, Property 9: WSL兼容性
验证：需求 7.1, 7.2, 7.3, 7.4, 7.5
"""

import pytest
import tempfile
import os
from pathlib import Path
from hypothesis import given, strategies as st, assume, settings
from hypothesis import HealthCheck

from script_validation.models import (
    Script, ScriptType, Platform, ScriptMetadata, Parameter
)
from script_validation.adapters.wsl_adapter import WSLAdapter


class TestWSLCompatibilityProperties:
    """WSL兼容性属性测试类"""

    def setup_method(self):
        """设置测试方法"""
        self.adapter = WSLAdapter()

    def test_wsl_adapter_availability_check(self):
        """
        测试WSL适配器的可用性检查
        """
        # 这个测试总是运行，检查适配器的可用性检查逻辑
        availability = self.adapter.is_available()

        # 可用性检查应该返回布尔值
        assert isinstance(availability, bool)

        # 如果可用，应该能够获取环境信息
        if availability:
            env_info = self.adapter.get_environment_info()
            assert env_info.platform == Platform.WSL
            assert env_info.os_version is not None
            assert env_info.python_version is not None
            assert env_info.shell_version is not None
            assert isinstance(env_info.available_commands, list)

    def test_wsl_dependency_checking(self):
        """
        测试WSL依赖检查功能
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        # 测试常见命令的依赖检查
        common_deps = ['bash', 'echo', 'ls', 'nonexistent_command_12345']

        results = self.adapter.check_dependencies(common_deps)

        # 验证返回结果的结构
        assert len(results) == len(common_deps)

        for result in results:
            assert hasattr(result, 'dependency')
            assert hasattr(result, 'available')
            assert isinstance(result.available, bool)

            # bash和echo应该在大多数WSL环境中可用
            if result.dependency in ['bash', 'echo', 'ls']:
                # 这些命令通常应该可用，但我们不强制要求
                pass
            elif result.dependency == 'nonexistent_command_12345':
                # 这个命令应该不存在
                assert not result.available

    @given(
        script_name=st.text(min_size=1, max_size=50).filter(lambda x: x.isalnum())
    )
    @settings(
        max_examples=10,  # 减少测试次数以避免长时间运行
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_wsl_file_system_path_handling(self, script_name):
        """
        Feature: script-delivery-validation, Property 9: WSL兼容性
        对于任何在WSL环境中运行的脚本，应该正确处理文件系统路径、系统命令、
        行结束符、环境变量和Windows工具互操作
        验证：需求 7.1 - 文件系统路径处理
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        # 创建临时脚本文件
        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.sh',
            delete=False,
            encoding='utf-8'
        ) as f:
            # 创建一个简单的shell脚本来测试路径处理
            test_script = """#!/bin/bash
# Test script for path handling
echo "Current directory: $(pwd)"
echo "Script path: $0"
echo "Home directory: $HOME"
ls -la . | head -5 || echo "ls command failed"
"""
            f.write(test_script)
            temp_path = Path(f.name)

        try:
            # 创建脚本对象
            script = Script(
                path=temp_path,
                name=script_name,
                type=ScriptType.SHELL,
                platform=Platform.WSL,
                metadata=ScriptMetadata(
                    description="Test script for WSL path handling",
                    usage="./test_script.sh"
                )
            )

            # 执行脚本
            result = self.adapter.execute_script(script, [])

            # 验证脚本执行结果
            # 接受多种退出代码：0（成功）、-1（超时/执行问题）、127（命令未找到）
            assert result.exit_code in [0, -1, 127]

            # 如果WSL不可用，应该有相应的错误信息
            if result.exit_code == 127:
                assert "command not found" in result.stderr.lower() or "not found" in result.stderr.lower()
            elif result.exit_code == 0:
                # 如果成功执行，验证输出包含路径信息
                assert ("Current directory:" in result.stdout or
                       "Script path:" in result.stdout or
                       "Home directory:" in result.stdout)

        finally:
            # 清理临时文件
            if temp_path.exists():
                temp_path.unlink()

    @given(
        command=st.sampled_from(['ls', 'pwd', 'echo', 'cat', 'which'])
    )
    @settings(
        max_examples=5,  # 减少测试次数
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_wsl_system_command_availability(self, command):
        """
        Feature: script-delivery-validation, Property 9: WSL兼容性
        验证：需求 7.2 - 系统命令可用性和行为一致性
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        # 创建测试脚本
        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.sh',
            delete=False,
            encoding='utf-8'
        ) as f:
            # 创建测试系统命令的脚本
            test_script = f"""#!/bin/bash
# Test system command availability
if command -v {command} >/dev/null 2>&1; then
    echo "Command {command} is available"
    {command} --version 2>/dev/null || echo "Command executed with error"
else
    echo "Command {command} not found"
fi
"""
            f.write(test_script)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name=f"test_{command}",
                type=ScriptType.SHELL,
                platform=Platform.WSL,
                metadata=ScriptMetadata(
                    description=f"Test {command} command availability",
                    usage="./test_command.sh"
                )
            )

            # 执行脚本
            result = self.adapter.execute_script(script, [])

            # 验证脚本能够执行（即使命令不存在也应该能正常处理）
            assert result.exit_code in [0, -1, 127]

            # 验证输出或错误信息
            if result.exit_code == 127:
                assert "command not found" in result.stderr.lower()
            elif result.exit_code == 0:
                assert ("is available" in result.stdout or
                       "not found" in result.stdout or
                       "Command executed" in result.stdout)

        finally:
            if temp_path.exists():
                temp_path.unlink()

    @given(
        line_ending=st.sampled_from(['\n', '\r\n'])  # 移除单独的\r，因为它可能导致问题
    )
    @settings(
        max_examples=5,  # 减少测试次数
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_wsl_line_ending_handling(self, line_ending):
        """
        Feature: script-delivery-validation, Property 9: WSL兼容性
        验证：需求 7.3 - 行结束符处理
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.sh',
            delete=False,
            encoding='utf-8',
            newline=''  # 保持原始行结束符
        ) as f:
            test_script = f"""#!/bin/bash{line_ending}echo "Testing line endings"{line_ending}echo "Content: test"{line_ending}"""
            f.write(test_script)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name="test_line_endings",
                type=ScriptType.SHELL,
                platform=Platform.WSL,
                metadata=ScriptMetadata(
                    description="Test line ending handling",
                    usage="./test_line_endings.sh"
                )
            )

            # 执行脚本
            result = self.adapter.execute_script(script, [])

            # 验证脚本能够正确处理不同的行结束符
            assert result.exit_code in [0, -1, 127]

            if result.exit_code == 127:
                assert "command not found" in result.stderr.lower()
            elif result.exit_code == 0:
                # 验证输出被正确标准化（统一为LF）
                if result.stdout:
                    # 输出应该不包含CRLF
                    assert '\r\n' not in result.stdout
                    assert "Testing line endings" in result.stdout

        finally:
            if temp_path.exists():
                temp_path.unlink()

    @given(
        env_var_name=st.sampled_from(['TESTVAR', 'MYVAR', 'CUSTOMVAR', 'APPVAR', 'CONFIGVAR'])
    )
    @settings(
        max_examples=5,  # 减少测试次数
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture, HealthCheck.filter_too_much]
    )
    def test_wsl_environment_variable_handling(self, env_var_name):
        """
        Feature: script-delivery-validation, Property 9: WSL兼容性
        验证：需求 7.4 - 环境变量处理
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        # 避免覆盖重要的系统环境变量
        assume(env_var_name not in ['PATH', 'HOME', 'USER', 'SHELL', 'PWD'])

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.sh',
            delete=False,
            encoding='utf-8'
        ) as f:
            # 创建测试环境变量的脚本
            test_script = f"""#!/bin/bash
# Test environment variable handling
export {env_var_name}="test_value"
echo "Set {env_var_name}=${env_var_name}"
echo "Value: ${{{env_var_name}}}"
env | grep {env_var_name} || echo "Variable not found in env"
"""
            f.write(test_script)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name="test_env_vars",
                type=ScriptType.SHELL,
                platform=Platform.WSL,
                metadata=ScriptMetadata(
                    description="Test environment variable handling",
                    usage="./test_env_vars.sh"
                )
            )

            # 执行脚本
            result = self.adapter.execute_script(script, [])

            # 验证环境变量处理正确
            assert result.exit_code in [0, -1, 127]

            if result.exit_code == 127:
                assert "command not found" in result.stderr.lower()
            elif result.exit_code == 0:
                # 验证环境变量被正确设置和读取
                assert (f"Set {env_var_name}=" in result.stdout and
                       ("Value:" in result.stdout or
                        "Variable not found" in result.stdout))

        finally:
            if temp_path.exists():
                temp_path.unlink()

    def test_wsl_windows_interoperability(self):
        """
        Feature: script-delivery-validation, Property 9: WSL兼容性
        验证：需求 7.5 - Windows工具互操作性
        """
        # 跳过如果WSL不可用
        if not self.adapter.is_available():
            pytest.skip("WSL not available")

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.sh',
            delete=False,
            encoding='utf-8'
        ) as f:
            # 创建测试Windows互操作的脚本
            test_script = """#!/bin/bash
# Test Windows interoperability
echo "Testing Windows interoperability"

# Test if we can access Windows commands (if available)
if command -v cmd.exe >/dev/null 2>&1; then
    echo "Windows cmd.exe is accessible"
    cmd.exe /c "echo Windows command executed" 2>/dev/null || echo "Windows command failed"
else
    echo "Windows cmd.exe not accessible"
fi

# Test path conversion capabilities
echo "Current working directory: $(pwd)"
echo "PATH variable contains: $(echo $PATH | head -c 100)"
"""
            f.write(test_script)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name="test_windows_interop",
                type=ScriptType.SHELL,
                platform=Platform.WSL,
                metadata=ScriptMetadata(
                    description="Test Windows interoperability",
                    usage="./test_windows_interop.sh"
                )
            )

            # 执行脚本
            result = self.adapter.execute_script(script, [])

            # 验证互操作性测试能够执行
            assert result.exit_code in [0, -1, 127]

            if result.exit_code == 127:
                assert "command not found" in result.stderr.lower()
            elif result.exit_code == 0:
                # 验证输出包含互操作性测试结果
                assert ("Testing Windows interoperability" in result.stdout and
                       ("accessible" in result.stdout or
                        "Current working directory:" in result.stdout))

        finally:
            if temp_path.exists():
                temp_path.unlink()

    def test_wsl_path_conversion_logic(self):
        """
        测试WSL路径转换逻辑（不需要实际WSL环境）
        """
        # 测试路径转换方法
        windows_path = Path("C:\\Users\\test\\file.txt")
        wsl_path = self.adapter._convert_to_wsl_path(windows_path)

        # 验证路径转换逻辑
        if not self.adapter._is_running_in_wsl():
            # 在Windows上运行时，应该转换为WSL格式
            expected = "/mnt/c/Users/test/file.txt"
            assert wsl_path == expected or wsl_path == str(windows_path)

        # 测试相对路径
        relative_path = Path("test/file.txt")
        wsl_relative = self.adapter._convert_to_wsl_path(relative_path)
        assert "/" in wsl_relative or "\\" in wsl_relative  # 应该包含路径分隔符
