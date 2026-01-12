"""
脚本功能完整性属性测试

Feature: script-delivery-validation, Property 4: 脚本功能完整性
验证：需求 2.1, 2.2, 2.3, 2.4, 2.5, 2.6
"""

import pytest
import tempfile
import os
from pathlib import Path
from hypothesis import given, strategies as st, assume, settings
from hypothesis import HealthCheck

from script_validation.models import (
    Script, ScriptType, Platform, ScriptMetadata, ScriptCategory, Parameter
)
from script_validation.validators.functional_validator import FunctionalValidator
from script_validation.managers.platform_manager import PlatformManager


# 策略定义
@st.composite
def script_categories_with_content(draw):
    """生成脚本分类和对应的内容"""
    category = draw(st.sampled_from(list(ScriptCategory)))
    script_type = draw(st.sampled_from(list(ScriptType)))

    # 根据分类生成相应的脚本内容
    content_templates = {
        ScriptCategory.BUILD: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import os
import subprocess
import sys

def build():
    """Build the project"""
    print("Starting build process...")
    # Simulate build steps
    print("Compiling sources...")
    print("Linking...")
    print("Build completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(build())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Starting build process..."
echo "Compiling sources..."
echo "Linking..."
echo "Build completed successfully"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Starting build process...
echo Compiling sources...
echo Linking...
echo Build completed successfully
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Build script
Write-Host "Starting build process..."
Write-Host "Compiling sources..."
Write-Host "Linking..."
Write-Host "Build completed successfully"
exit 0
'''
        },
        ScriptCategory.TEST: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys
import unittest

def run_tests():
    """Run test suite"""
    print("Running tests...")
    print("Test 1: PASSED")
    print("Test 2: PASSED")
    print("All tests passed")
    return 0

if __name__ == "__main__":
    sys.exit(run_tests())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Running tests..."
echo "Test 1: PASSED"
echo "Test 2: PASSED"
echo "All tests passed"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Running tests...
echo Test 1: PASSED
echo Test 2: PASSED
echo All tests passed
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Test script
Write-Host "Running tests..."
Write-Host "Test 1: PASSED"
Write-Host "Test 2: PASSED"
Write-Host "All tests passed"
exit 0
'''
        },
        ScriptCategory.FORMAT: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys

def format_code():
    """Format code files"""
    print("Formatting code...")
    print("Processing file 1...")
    print("Processing file 2...")
    print("Code formatting completed")
    return 0

if __name__ == "__main__":
    sys.exit(format_code())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Formatting code..."
echo "Processing file 1..."
echo "Processing file 2..."
echo "Code formatting completed"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Formatting code...
echo Processing file 1...
echo Processing file 2...
echo Code formatting completed
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Format script
Write-Host "Formatting code..."
Write-Host "Processing file 1..."
Write-Host "Processing file 2..."
Write-Host "Code formatting completed"
exit 0
'''
        },
        ScriptCategory.CLEAN: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys
import os

def clean():
    """Clean build artifacts"""
    print("Cleaning build artifacts...")
    print("Removing temporary files...")
    print("Removing build directory...")
    print("Clean completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(clean())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Cleaning build artifacts..."
echo "Removing temporary files..."
echo "Removing build directory..."
echo "Clean completed successfully"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Cleaning build artifacts...
echo Removing temporary files...
echo Removing build directory...
echo Clean completed successfully
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Clean script
Write-Host "Cleaning build artifacts..."
Write-Host "Removing temporary files..."
Write-Host "Removing build directory..."
Write-Host "Clean completed successfully"
exit 0
'''
        },
        ScriptCategory.DOCS: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys

def generate_docs():
    """Generate documentation"""
    print("Generating documentation...")
    print("Processing source files...")
    print("Creating HTML output...")
    print("Documentation generated successfully")
    return 0

if __name__ == "__main__":
    sys.exit(generate_docs())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Generating documentation..."
echo "Processing source files..."
echo "Creating HTML output..."
echo "Documentation generated successfully"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Generating documentation...
echo Processing source files...
echo Creating HTML output...
echo Documentation generated successfully
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Documentation script
Write-Host "Generating documentation..."
Write-Host "Processing source files..."
Write-Host "Creating HTML output..."
Write-Host "Documentation generated successfully"
exit 0
'''
        },
        ScriptCategory.SETUP: {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys

def setup_environment():
    """Setup development environment"""
    print("Setting up development environment...")
    print("Installing dependencies...")
    print("Configuring environment...")
    print("Setup completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(setup_environment())
''',
            ScriptType.SHELL: '''#!/bin/bash
set -e

echo "Setting up development environment..."
echo "Installing dependencies..."
echo "Configuring environment..."
echo "Setup completed successfully"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Setting up development environment...
echo Installing dependencies...
echo Configuring environment...
echo Setup completed successfully
exit /b 0
''',
            ScriptType.POWERSHELL: '''# Setup script
Write-Host "Setting up development environment..."
Write-Host "Installing dependencies..."
Write-Host "Configuring environment..."
Write-Host "Setup completed successfully"
exit 0
'''
        }
    }

    # 获取默认内容模板
    default_content = {
        ScriptType.PYTHON: '#!/usr/bin/env python3\nprint("Hello World")\n',
        ScriptType.SHELL: '#!/bin/bash\necho "Hello World"\n',
        ScriptType.BATCH: '@echo off\necho Hello World\n',
        ScriptType.POWERSHELL: 'Write-Host "Hello World"\n'
    }

    # 选择内容
    if category in content_templates and script_type in content_templates[category]:
        content = content_templates[category][script_type]
    else:
        content = default_content.get(script_type, 'echo "Hello World"\n')

    return category, script_type, content


@st.composite
def valid_script_names(draw):
    """生成有效的脚本名称"""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd'), min_codepoint=32, max_codepoint=126),
        min_size=1,
        max_size=20
    ).filter(lambda x: x.replace('_', '').replace('-', '').isalnum()))
    return name.replace(' ', '_')


class TestFunctionalCompletenessProperties:
    """脚本功能完整性属性测试类"""

    def setup_method(self):
        """设置测试方法"""
        self.platform_manager = PlatformManager()
        self.validator = FunctionalValidator(self.platform_manager)

    @given(
        category_content=script_categories_with_content(),
        script_name=valid_script_names()
    )
    @settings(
        max_examples=20,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_script_functional_completeness_property(self, category_content, script_name):
        """
        Feature: script-delivery-validation, Property 4: 脚本功能完整性

        对于任何脚本，如果验证通过，则该脚本应该能够成功完成其预期功能
        （构建、测试、格式化、清理、文档生成、环境设置）而不产生错误
        验证：需求 2.1, 2.2, 2.3, 2.4, 2.5, 2.6
        """
        category, script_type, content = category_content

        # 获取当前平台
        current_platform = self.platform_manager.detect_current_platform()

        # 检查平台是否支持该脚本类型
        if not self._is_script_type_supported(script_type, current_platform):
            pytest.skip(f"Script type {script_type} not supported on {current_platform}")

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix=self._get_script_extension(script_type),
            delete=False,
            encoding='utf-8'
        ) as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            # 创建脚本对象
            script = Script(
                path=temp_path,
                name=script_name,
                type=script_type,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description=f"{category.value} script for testing",
                    usage=f"./{script_name}{self._get_script_extension(script_type)}"
                ),
                category=category
            )

            # 执行功能验证
            result = self.validator.validate(script, current_platform)

            # 验证脚本功能完整性
            assert result is not None, "验证结果不能为空"
            assert hasattr(result, 'status'), "验证结果应该包含状态"
            assert hasattr(result, 'execution_time'), "验证结果应该包含执行时间"
            assert hasattr(result, 'output'), "验证结果应该包含输出"

            # 如果验证通过，脚本应该能够成功完成其预期功能
            if result.status.value == "passed":
                # 验证输出包含预期的功能指示
                self._verify_functional_output(category, result.output)

                # 验证执行时间合理
                assert result.execution_time >= 0, "执行时间应该非负"
                assert result.execution_time < 30, "执行时间应该在合理范围内"

            # 验证错误处理
            if result.status.value == "failed":
                # 检查是否有错误信息或详细信息解释失败原因
                has_error_info = (
                    result.error is not None or
                    "error" in result.output.lower() or
                    (result.details and any("failed" in str(v).lower() or "error" in str(v).lower()
                                          for v in result.details.values() if isinstance(v, (str, bool))))
                )
                assert has_error_info, \
                    f"失败的验证应该包含错误信息。状态: {result.status}, 错误: {result.error}, 输出: {result.output}, 详情: {result.details}"

        finally:
            # 清理临时文件
            if temp_path.exists():
                temp_path.unlink()

    @given(
        script_type=st.sampled_from(list(ScriptType)),
        script_name=valid_script_names()
    )
    @settings(
        max_examples=15,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_build_script_functionality(self, script_type, script_name):
        """
        测试构建脚本功能完整性
        验证：需求 2.1 - 构建脚本验证
        """
        current_platform = self.platform_manager.detect_current_platform()

        if not self._is_script_type_supported(script_type, current_platform):
            pytest.skip(f"Script type {script_type} not supported on {current_platform}")

        # 创建构建脚本内容
        build_contents = {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys
print("Build started")
print("Compiling sources...")
print("Build artifacts created")
print("Build completed successfully")
sys.exit(0)
''',
            ScriptType.SHELL: '''#!/bin/bash
echo "Build started"
echo "Compiling sources..."
echo "Build artifacts created"
echo "Build completed successfully"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Build started
echo Compiling sources...
echo Build artifacts created
echo Build completed successfully
exit /b 0
''',
            ScriptType.POWERSHELL: '''Write-Host "Build started"
Write-Host "Compiling sources..."
Write-Host "Build artifacts created"
Write-Host "Build completed successfully"
exit 0
'''
        }

        content = build_contents.get(script_type, 'echo "Build completed"\n')

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix=self._get_script_extension(script_type),
            delete=False,
            encoding='utf-8'
        ) as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name=script_name,
                type=script_type,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description="Build script for testing",
                    usage=f"./{script_name}{self._get_script_extension(script_type)}"
                ),
                category=ScriptCategory.BUILD
            )

            result = self.validator.validate(script, current_platform)

            # 验证构建脚本的功能完整性
            assert result is not None

            if result.status.value == "passed":
                # 验证构建过程的关键步骤
                output_lower = result.output.lower()
                assert any(keyword in output_lower for keyword in [
                    "build", "compiling", "completed", "success"
                ]), f"构建脚本输出应包含构建相关信息: {result.output}"

        finally:
            if temp_path.exists():
                temp_path.unlink()

    @given(
        script_type=st.sampled_from(list(ScriptType)),
        script_name=valid_script_names()
    )
    @settings(
        max_examples=15,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_test_script_functionality(self, script_type, script_name):
        """
        测试测试脚本功能完整性
        验证：需求 2.2 - 测试脚本验证
        """
        current_platform = self.platform_manager.detect_current_platform()

        if not self._is_script_type_supported(script_type, current_platform):
            pytest.skip(f"Script type {script_type} not supported on {current_platform}")

        # 创建测试脚本内容
        test_contents = {
            ScriptType.PYTHON: '''#!/usr/bin/env python3
import sys
print("Running tests...")
print("Test suite: unit tests")
print("Test 1: PASSED")
print("Test 2: PASSED")
print("All tests passed")
sys.exit(0)
''',
            ScriptType.SHELL: '''#!/bin/bash
echo "Running tests..."
echo "Test suite: unit tests"
echo "Test 1: PASSED"
echo "Test 2: PASSED"
echo "All tests passed"
exit 0
''',
            ScriptType.BATCH: '''@echo off
echo Running tests...
echo Test suite: unit tests
echo Test 1: PASSED
echo Test 2: PASSED
echo All tests passed
exit /b 0
''',
            ScriptType.POWERSHELL: '''Write-Host "Running tests..."
Write-Host "Test suite: unit tests"
Write-Host "Test 1: PASSED"
Write-Host "Test 2: PASSED"
Write-Host "All tests passed"
exit 0
'''
        }

        content = test_contents.get(script_type, 'echo "Tests completed"\n')

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix=self._get_script_extension(script_type),
            delete=False,
            encoding='utf-8'
        ) as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name=script_name,
                type=script_type,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description="Test script for testing",
                    usage=f"./{script_name}{self._get_script_extension(script_type)}"
                ),
                category=ScriptCategory.TEST
            )

            result = self.validator.validate(script, current_platform)

            # 验证测试脚本的功能完整性
            assert result is not None

            if result.status.value == "passed":
                # 验证测试执行和结果报告
                output_lower = result.output.lower()
                assert any(keyword in output_lower for keyword in [
                    "test", "passed", "running", "suite"
                ]), f"测试脚本输出应包含测试相关信息: {result.output}"

        finally:
            if temp_path.exists():
                temp_path.unlink()

    def _is_script_type_supported(self, script_type: ScriptType, platform: Platform) -> bool:
        """检查脚本类型是否在平台上受支持"""
        support_matrix = {
            Platform.WINDOWS: [ScriptType.BATCH, ScriptType.POWERSHELL, ScriptType.PYTHON],
            Platform.LINUX: [ScriptType.SHELL, ScriptType.PYTHON, ScriptType.POWERSHELL],
            Platform.WSL: [ScriptType.SHELL, ScriptType.PYTHON]
        }
        return script_type in support_matrix.get(platform, [])

    def _get_script_extension(self, script_type: ScriptType) -> str:
        """获取脚本文件扩展名"""
        extensions = {
            ScriptType.BATCH: '.bat',
            ScriptType.POWERSHELL: '.ps1',
            ScriptType.SHELL: '.sh',
            ScriptType.PYTHON: '.py'
        }
        return extensions.get(script_type, '.txt')

    def _verify_functional_output(self, category: ScriptCategory, output: str):
        """验证功能输出包含预期内容"""
        output_lower = output.lower()

        expected_keywords = {
            ScriptCategory.BUILD: ["build", "compiling", "completed"],
            ScriptCategory.TEST: ["test", "running", "passed"],
            ScriptCategory.FORMAT: ["format", "processing", "completed"],
            ScriptCategory.CLEAN: ["clean", "removing", "completed"],
            ScriptCategory.DOCS: ["documentation", "generating", "created"],
            ScriptCategory.SETUP: ["setup", "installing", "completed"],
            ScriptCategory.CI: ["ci", "pipeline", "completed"],
            ScriptCategory.UTILITY: ["utility", "processing", "completed"]
        }

        keywords = expected_keywords.get(category, ["completed", "success"])
        assert any(keyword in output_lower for keyword in keywords), \
            f"输出应包含 {category.value} 相关的关键词: {keywords}, 实际输出: {output}"
