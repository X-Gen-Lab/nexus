"""
错误处理一致性属性测试

属性5：错误处理一致性
*对于任何*脚本，当遇到错误条件（缺失依赖、无效参数、权限问题、中断、平台限制）时，
应该提供清晰的错误消息和适当的退出代码

验证：需求 3.1, 3.2, 3.3, 3.4, 3.5
"""

import pytest
from hypothesis import given, strategies as st, settings, assume
from pathlib import Path
import tempfile
import os
import shutil

from script_validation.handlers import (
    ErrorHandler,
    ResourceManager,
    ScriptError,
    DependencyMissingError,
    PermissionDeniedError,
    ScriptSyntaxError,
    TimeoutError,
    PlatformError,
    PlatformNotSupportedError,
    CommandNotFoundError,
    PathFormatError,
    EnvironmentVariableError,
    ValidationSystemError,
    ConfigurationError,
    ResourceExhaustedError,
    NetworkError,
    ErrorResponse,
    ErrorSeverity
)


# ============================================================================
# 测试策略（Generators）
# ============================================================================

# 平台策略
platforms = st.sampled_from(['windows', 'linux', 'wsl', 'unknown'])

# 依赖名称策略
dependency_names = st.sampled_from([
    'python', 'python3', 'git', 'cmake', 'make', 'gcc', 'node', 'npm',
    'unknown_dep', 'custom_tool', 'my_command'
])

# 错误消息策略
error_messages = st.text(
    alphabet=st.characters(whitelist_categories=('L', 'N', 'P', 'S')),
    min_size=1,
    max_size=200
)

# 路径策略
path_strings = st.text(
    alphabet=st.characters(whitelist_categories=('L', 'N'), whitelist_characters='/_\\.-'),
    min_size=1,
    max_size=100
)

# 环境变量名策略
env_var_names = st.text(
    alphabet=st.characters(whitelist_categories=('Lu', 'N'), whitelist_characters='_'),
    min_size=1,
    max_size=50
)

# 退出代码策略
exit_codes = st.integers(min_value=-128, max_value=255)

# 超时时间策略
timeout_seconds = st.integers(min_value=1, max_value=3600)

# 资源类型策略
resource_types = st.sampled_from(['memory', 'disk', 'cpu', 'network'])


# ============================================================================
# 属性测试：错误处理一致性
# Feature: script-delivery-validation, Property 5: 错误处理一致性
# ============================================================================

class TestErrorHandlingConsistency:
    """
    属性5：错误处理一致性

    *对于任何*脚本，当遇到错误条件时，应该提供清晰的错误消息和适当的退出代码

    **验证：需求 3.1, 3.2, 3.3, 3.4, 3.5**
    """

    @given(dependency=dependency_names, platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_dependency_missing_error_provides_clear_message(
        self, dependency: str, platform: str, message: str
    ):
        """
        属性5.1：缺失依赖错误提供清晰消息

        *对于任何*缺失依赖错误，ErrorHandler应该返回包含建议的ErrorResponse

        **验证：需求 3.1**
        """
        # Arrange
        handler = ErrorHandler()
        error = DependencyMissingError(
            message=message,
            dependency=dependency
        )

        # Act
        response = handler.handle_script_error(error, platform)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "DependencyMissing"
        assert dependency in response.message or dependency in str(response.details)
        assert len(response.suggestions) > 0, "应该提供安装建议"
        assert response.exit_code == 127, "缺失依赖应该返回退出代码127"
        assert response.can_recover is True, "缺失依赖错误应该可恢复"

    @given(platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_permission_denied_error_provides_actionable_message(
        self, platform: str, message: str
    ):
        """
        属性5.2：权限不足错误提供可操作的消息

        *对于任何*权限不足错误，ErrorHandler应该返回包含修复建议的ErrorResponse

        **验证：需求 3.3**
        """
        # Arrange
        handler = ErrorHandler()
        error = PermissionDeniedError(
            message=message,
            resource_path=Path("/some/path")
        )

        # Act
        response = handler.handle_script_error(error, platform)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "PermissionDenied"
        assert len(response.suggestions) > 0, "应该提供权限修复建议"
        assert response.exit_code == 126, "权限错误应该返回退出代码126"
        assert response.can_recover is True, "权限错误应该可恢复"
        assert len(response.recovery_actions) > 0, "应该提供恢复操作"

    @given(timeout=timeout_seconds, message=error_messages)
    @settings(max_examples=100)
    def test_timeout_error_suggests_recovery(self, timeout: int, message: str):
        """
        属性5.3：超时错误建议恢复操作

        *对于任何*超时错误，ErrorHandler应该返回包含恢复建议的ErrorResponse

        **验证：需求 3.4**
        """
        # Arrange
        handler = ErrorHandler()
        error = TimeoutError(
            message=message,
            timeout_seconds=timeout
        )

        # Act
        response = handler.handle_script_error(error, 'unknown')

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "Timeout"
        assert str(timeout) in response.message, "应该包含超时时间"
        assert response.exit_code == 124, "超时应该返回退出代码124"
        assert response.can_recover is True, "超时错误应该可恢复"
        assert any('清理' in action or 'clean' in action.lower()
                   for action in response.recovery_actions), "应该建议清理操作"

    @given(platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_platform_not_supported_provides_alternatives(
        self, platform: str, message: str
    ):
        """
        属性5.4：平台不支持错误提供替代方案

        *对于任何*平台不支持错误，ErrorHandler应该返回包含替代方案的ErrorResponse

        **验证：需求 3.5**
        """
        # Arrange
        handler = ErrorHandler()
        error = PlatformNotSupportedError(
            message=message,
            platform=platform
        )

        # Act
        response = handler.handle_platform_error(error)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "PlatformNotSupported"
        assert len(response.suggestions) > 0, "应该提供替代方案建议"
        assert response.can_recover is True, "平台不支持错误应该可恢复"


    @given(exit_code=exit_codes, platform=platforms)
    @settings(max_examples=100)
    def test_exit_code_analysis_provides_suggestions(
        self, exit_code: int, platform: str
    ):
        """
        属性5.5：退出代码分析提供建议

        *对于任何*非零退出代码，ErrorHandler应该能够分析并提供建议

        **验证：需求 3.2**
        """
        assume(exit_code != 0)  # 只测试非零退出代码

        # Arrange
        handler = ErrorHandler()
        stderr_samples = [
            "command not found",
            "permission denied",
            "no such file or directory",
            "syntax error",
            "out of memory",
            "connection timeout",
            "unknown error"
        ]

        for stderr in stderr_samples:
            # Act
            response = handler.create_error_from_exit_code(
                exit_code=exit_code,
                stderr=stderr,
                platform=platform
            )

            # Assert
            assert response is not None, "非零退出代码应该创建错误响应"
            assert isinstance(response, ErrorResponse)
            assert response.exit_code == exit_code
            assert len(response.suggestions) > 0, "应该提供建议"

    @given(command=dependency_names, platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_command_not_found_error_consistency(
        self, command: str, platform: str, message: str
    ):
        """
        属性5.6：命令不可用错误一致性

        *对于任何*命令不可用错误，ErrorHandler应该返回一致的ErrorResponse

        **验证：需求 3.1**
        """
        # Arrange
        handler = ErrorHandler()
        error = CommandNotFoundError(
            message=message,
            command=command,
            platform=platform
        )

        # Act
        response = handler.handle_platform_error(error)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "CommandNotFound"
        assert command in response.message or command in str(response.details)
        assert response.exit_code == 127
        assert response.can_recover is True

    @given(path=path_strings, platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_path_format_error_provides_platform_specific_suggestions(
        self, path: str, platform: str, message: str
    ):
        """
        属性5.7：路径格式错误提供平台特定建议

        *对于任何*路径格式错误，ErrorHandler应该返回平台特定的建议

        **验证：需求 3.5**
        """
        # Arrange
        handler = ErrorHandler()
        error = PathFormatError(
            message=message,
            path=path,
            platform=platform
        )

        # Act
        response = handler.handle_platform_error(error)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "PathFormatError"
        assert path in response.message or path in str(response.details)
        assert len(response.suggestions) > 0
        assert response.can_recover is True

    @given(var_name=env_var_names, platform=platforms, message=error_messages)
    @settings(max_examples=100)
    def test_env_variable_error_provides_setup_instructions(
        self, var_name: str, platform: str, message: str
    ):
        """
        属性5.8：环境变量错误提供设置说明

        *对于任何*环境变量错误，ErrorHandler应该返回设置说明

        **验证：需求 3.5**
        """
        assume(len(var_name) > 0)  # 确保变量名非空

        # Arrange
        handler = ErrorHandler()
        error = EnvironmentVariableError(
            message=message,
            variable_name=var_name,
            platform=platform
        )

        # Act
        response = handler.handle_platform_error(error)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "EnvironmentVariableError"
        assert var_name in response.message or var_name in str(response.details)
        assert len(response.suggestions) > 0
        assert response.can_recover is True

    @given(resource_type=resource_types, message=error_messages)
    @settings(max_examples=100)
    def test_resource_exhausted_error_provides_resource_specific_suggestions(
        self, resource_type: str, message: str
    ):
        """
        属性5.9：资源不足错误提供资源特定建议

        *对于任何*资源不足错误，ErrorHandler应该返回资源特定的建议

        **验证：需求 3.4**
        """
        # Arrange
        handler = ErrorHandler()
        error = ResourceExhaustedError(
            message=message,
            resource_type=resource_type
        )

        # Act
        response = handler.handle_validation_error(error)

        # Assert
        assert isinstance(response, ErrorResponse)
        assert response.error_type == "ResourceExhausted"
        assert resource_type in response.message or resource_type in str(response.details)
        assert len(response.suggestions) > 0
        assert response.severity == ErrorSeverity.CRITICAL
        assert response.can_recover is True


class TestErrorResponseFormatting:
    """测试错误响应格式化"""

    @given(message=error_messages)
    @settings(max_examples=100)
    def test_error_response_format_message_is_readable(self, message: str):
        """
        属性5.10：错误响应格式化消息可读

        *对于任何*ErrorResponse，format_message()应该返回可读的字符串

        **验证：需求 3.1, 3.2, 3.3**
        """
        # Arrange
        response = ErrorResponse(
            error_type="TestError",
            message=message,
            severity=ErrorSeverity.ERROR,
            suggestions=["建议1", "建议2"],
            recovery_actions=["操作1", "操作2"]
        )

        # Act
        formatted = response.format_message()

        # Assert
        assert isinstance(formatted, str)
        assert len(formatted) > 0
        assert "TestError" in formatted
        assert "ERROR" in formatted

    @given(message=error_messages)
    @settings(max_examples=100)
    def test_error_response_to_dict_is_serializable(self, message: str):
        """
        属性5.11：错误响应可序列化为字典

        *对于任何*ErrorResponse，to_dict()应该返回可序列化的字典

        **验证：需求 3.2**
        """
        # Arrange
        response = ErrorResponse(
            error_type="TestError",
            message=message,
            severity=ErrorSeverity.ERROR,
            suggestions=["建议"],
            recovery_actions=["操作"],
            details={"key": "value"}
        )

        # Act
        result = response.to_dict()

        # Assert
        assert isinstance(result, dict)
        assert result['error_type'] == "TestError"
        assert result['message'] == message
        assert result['severity'] == "error"
        assert 'suggestions' in result
        assert 'recovery_actions' in result
        assert 'details' in result


class TestErrorHistory:
    """测试错误历史记录"""

    @given(messages=st.lists(error_messages, min_size=1, max_size=10))
    @settings(max_examples=50)
    def test_error_history_tracks_all_errors(self, messages: list):
        """
        属性5.12：错误历史记录所有错误

        *对于任何*一系列错误，ErrorHandler应该记录所有错误

        **验证：需求 3.2**
        """
        # Arrange
        handler = ErrorHandler()
        handler.clear_error_history()

        # Act
        for msg in messages:
            error = ScriptError(message=msg)
            handler.handle_script_error(error, 'unknown')

        # Assert
        history = handler.get_error_history()
        assert len(history) == len(messages)

    def test_error_history_can_be_cleared(self):
        """
        属性5.13：错误历史可以清除

        **验证：需求 3.2**
        """
        # Arrange
        handler = ErrorHandler()
        error = ScriptError(message="test error")
        handler.handle_script_error(error, 'unknown')

        # Act
        handler.clear_error_history()

        # Assert
        assert len(handler.get_error_history()) == 0



class TestResourceManagerProperties:
    """
    资源管理器属性测试

    验证：需求 3.4, 4.4
    """

    @given(content=st.text(min_size=0, max_size=1000, alphabet=st.characters(
        whitelist_categories=('L', 'N', 'P', 'S'),
        blacklist_characters='\r'  # 排除回车符以避免行结束符问题
    )))
    @settings(max_examples=50)
    def test_temp_file_creation_and_cleanup(self, content: str):
        """
        属性5.14：临时文件创建和清理

        *对于任何*临时文件，创建后应该存在，清理后应该不存在

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()

        # Act - 创建临时文件
        temp_path = manager.create_temp_file(content=content)

        # Assert - 文件应该存在
        assert temp_path.exists()
        if content:
            actual_content = temp_path.read_text(encoding='utf-8')
            assert actual_content == content

        # Act - 清理
        resource_id = f"file_{temp_path.name}"
        result = manager.cleanup_resource(resource_id)

        # Assert - 文件应该不存在
        assert result is True
        assert not temp_path.exists()

    @given(num_files=st.integers(min_value=1, max_value=5))
    @settings(max_examples=30)
    def test_temp_directory_creation_and_cleanup(self, num_files: int):
        """
        属性5.15：临时目录创建和清理

        *对于任何*临时目录，创建后应该存在，清理后应该不存在

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()

        # Act - 创建临时目录
        temp_dir = manager.create_temp_directory()

        # 在目录中创建一些文件
        for i in range(num_files):
            (temp_dir / f"file_{i}.txt").write_text(f"content {i}")

        # Assert - 目录应该存在
        assert temp_dir.exists()
        assert temp_dir.is_dir()
        assert len(list(temp_dir.iterdir())) == num_files

        # Act - 清理
        resource_id = f"dir_{temp_dir.name}"
        result = manager.cleanup_resource(resource_id)

        # Assert - 目录应该不存在
        assert result is True
        assert not temp_dir.exists()

    def test_managed_temp_file_context_manager(self):
        """
        属性5.16：上下文管理器自动清理临时文件

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()
        temp_path = None

        # Act
        with manager.managed_temp_file(content="test content") as path:
            temp_path = path
            assert path.exists()
            assert path.read_text(encoding='utf-8') == "test content"

        # Assert - 退出上下文后文件应该被清理
        assert not temp_path.exists()

    def test_managed_temp_directory_context_manager(self):
        """
        属性5.17：上下文管理器自动清理临时目录

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()
        temp_dir = None

        # Act
        with manager.managed_temp_directory() as dir_path:
            temp_dir = dir_path
            assert dir_path.exists()
            # 创建一些文件
            (dir_path / "test.txt").write_text("test")

        # Assert - 退出上下文后目录应该被清理
        assert not temp_dir.exists()

    def test_execution_state_save_and_restore(self):
        """
        属性5.18：执行状态保存和恢复

        *对于任何*执行状态，保存后应该能够恢复

        **验证：需求 3.4**
        """
        # Arrange
        manager = ResourceManager()

        # 创建一个临时文件用于测试
        test_file = manager.create_temp_file(content="original content")

        # Act - 保存状态
        state_id = manager.save_execution_state(
            files_to_backup=[test_file],
            metadata={"test": "value"}
        )

        # 修改文件
        test_file.write_text("modified content")
        assert test_file.read_text() == "modified content"

        # Act - 恢复状态
        result = manager.restore_execution_state(state_id)

        # Assert
        assert result is True
        assert test_file.read_text() == "original content"

        # Cleanup
        manager.cleanup_all_resources()

    def test_cleanup_all_resources(self):
        """
        属性5.19：清理所有资源

        *对于任何*注册的资源集合，cleanup_all_resources应该清理所有资源

        **验证：需求 3.4, 4.4**
        """
        # Arrange
        manager = ResourceManager()

        # 创建多个资源
        file1 = manager.create_temp_file(content="file1")
        file2 = manager.create_temp_file(content="file2")
        dir1 = manager.create_temp_directory()

        # Assert - 资源应该存在
        assert file1.exists()
        assert file2.exists()
        assert dir1.exists()

        # Act
        results = manager.cleanup_all_resources()

        # Assert - 所有资源应该被清理
        assert not file1.exists()
        assert not file2.exists()
        assert not dir1.exists()
        assert all(results.values())

    def test_disk_space_management(self):
        """
        属性5.20：磁盘空间管理

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()

        # Act
        temp_size = manager.get_temp_directory_size()
        free_space = manager.get_disk_free_space()

        # Assert
        assert temp_size >= 0
        # free_space可能返回-1如果无法检测
        assert free_space >= -1

    def test_ensure_disk_space(self):
        """
        属性5.21：确保磁盘空间

        **验证：需求 4.4**
        """
        # Arrange
        manager = ResourceManager()

        # Act - 请求一个合理的空间量
        result = manager.ensure_disk_space(1024 * 1024)  # 1MB

        # Assert - 应该有足够的空间
        assert result is True

    def test_resource_registration_and_unregistration(self):
        """
        属性5.22：资源注册和取消注册

        **验证：需求 3.4**
        """
        # Arrange
        manager = ResourceManager()

        # Act - 注册资源
        resource_id = manager.register_resource(
            resource_id="test_resource",
            resource_type="custom",
            metadata={"key": "value"}
        )

        # Assert - 资源应该被注册
        resources = manager.get_registered_resources()
        assert any(r.resource_id == "test_resource" for r in resources)

        # Act - 取消注册
        result = manager.unregister_resource("test_resource")

        # Assert - 资源应该被取消注册
        assert result is True
        resources = manager.get_registered_resources()
        assert not any(r.resource_id == "test_resource" for r in resources)


class TestExceptionClassification:
    """测试异常分类"""

    def test_file_not_found_exception_classification(self):
        """
        属性5.23：FileNotFoundError分类

        **验证：需求 3.1**
        """
        # Arrange
        handler = ErrorHandler()
        exception = FileNotFoundError("test file not found")

        # Act
        response = handler.handle_exception(exception)

        # Assert
        assert response.error_type == "FileNotFound"
        assert response.can_recover is True

    def test_permission_error_exception_classification(self):
        """
        属性5.24：PermissionError分类

        **验证：需求 3.3**
        """
        # Arrange
        handler = ErrorHandler()
        exception = PermissionError("permission denied")

        # Act
        response = handler.handle_exception(exception)

        # Assert
        assert response.error_type == "PermissionDenied"
        assert response.exit_code == 126
        assert response.can_recover is True

    def test_os_error_exception_classification(self):
        """
        属性5.25：OSError分类

        **验证：需求 3.1**
        """
        # Arrange
        handler = ErrorHandler()
        exception = OSError("os error")

        # Act
        response = handler.handle_exception(exception)

        # Assert
        assert response.error_type == "OSError"

    def test_generic_exception_classification(self):
        """
        属性5.26：通用异常分类

        **验证：需求 3.2**
        """
        # Arrange
        handler = ErrorHandler()
        exception = ValueError("value error")

        # Act
        response = handler.handle_exception(exception, context={"test": "context"})

        # Assert
        assert response.error_type == "ValueError"
        assert "traceback" in response.details
        assert "context" in response.details


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
