"""
平台等效性属性测试

Feature: script-delivery-validation, Property 3: 平台特定脚本等效性
验证：需求 1.5
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
from script_validation.validators.compatibility_validator import CompatibilityValidator
from script_validation.managers.platform_manager import PlatformManager


# 策略定义
@st.composite
def equivalent_script_sets(draw):
    """生成功能等效的平台特定脚本组"""
    script_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
        min_size=1,
        max_size=15
    ).filter(lambda x: x.isalnum()))

    category = draw(st.sampled_from([
        ScriptCategory.BUILD,
        ScriptCategory.TEST,
        ScriptCategory.CLEAN,
        ScriptCategory.FORMAT
    ]))

    # 生成功能等效的脚本内容
    script_contents = {}

    if category == ScriptCategory.BUILD:
        script_contents = {
            ScriptType.BATCH: f'''@echo off
echo Starting {script_name} build process...
echo Compiling sources...
echo Creating build artifacts...
echo Build completed successfully
exit /b 0
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Starting {script_name} build process..."
echo "Compiling sources..."
echo "Creating build artifacts..."
echo "Build completed successfully"
exit 0
''',
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys

def main():
    """Build script for {script_name}"""
    print("Starting {script_name} build process...")
    print("Compiling sources...")
    print("Creating build artifacts...")
    print("Build completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        }
    elif category == ScriptCategory.TEST:
        script_contents = {
            ScriptType.BATCH: f'''@echo off
echo Running {script_name} tests...
echo Test suite: unit tests
echo Test 1: PASSED
echo Test 2: PASSED
echo All tests completed successfully
exit /b 0
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Running {script_name} tests..."
echo "Test suite: unit tests"
echo "Test 1: PASSED"
echo "Test 2: PASSED"
echo "All tests completed successfully"
exit 0
''',
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys

def main():
    """Test script for {script_name}"""
    print("Running {script_name} tests...")
    print("Test suite: unit tests")
    print("Test 1: PASSED")
    print("Test 2: PASSED")
    print("All tests completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        }
    elif category == ScriptCategory.CLEAN:
        script_contents = {
            ScriptType.BATCH: f'''@echo off
echo Cleaning {script_name} artifacts...
echo Removing temporary files...
echo Removing build directory...
echo Clean completed successfully
exit /b 0
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Cleaning {script_name} artifacts..."
echo "Removing temporary files..."
echo "Removing build directory..."
echo "Clean completed successfully"
exit 0
''',
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys

def main():
    """Clean script for {script_name}"""
    print("Cleaning {script_name} artifacts...")
    print("Removing temporary files...")
    print("Removing build directory...")
    print("Clean completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        }
    else:  # FORMAT
        script_contents = {
            ScriptType.BATCH: f'''@echo off
echo Formatting {script_name} code...
echo Processing source files...
echo Applying code style...
echo Formatting completed successfully
exit /b 0
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Formatting {script_name} code..."
echo "Processing source files..."
echo "Applying code style..."
echo "Formatting completed successfully"
exit 0
''',
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys

def main():
    """Format script for {script_name}"""
    print("Formatting {script_name} code...")
    print("Processing source files...")
    print("Applying code style...")
    print("Formatting completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        }

    return script_name, category, script_contents


@st.composite
def build_script_variants(draw):
    """生成构建脚本的不同变体"""
    script_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
        min_size=1,
        max_size=15
    ).filter(lambda x: x.isalnum()))

    # 不同的构建步骤组合
    build_steps = draw(st.lists(
        st.sampled_from([
            "configure", "compile", "link", "package", "test", "install"
        ]),
        min_size=2,
        max_size=4,
        unique=True
    ))

    script_contents = {}

    # Windows批处理版本
    batch_steps = '\n'.join([f'echo {step.capitalize()}...' for step in build_steps])
    script_contents[ScriptType.BATCH] = f'''@echo off
echo Starting {script_name} build...
{batch_steps}
echo Build completed successfully
exit /b 0
'''

    # Linux Shell版本
    shell_steps = '\n'.join([f'echo "{step.capitalize()}..."' for step in build_steps])
    script_contents[ScriptType.SHELL] = f'''#!/bin/bash
set -e
echo "Starting {script_name} build..."
{shell_steps}
echo "Build completed successfully"
exit 0
'''

    # Python版本
    python_steps = '\n    '.join([f'print("{step.capitalize()}...")' for step in build_steps])
    script_contents[ScriptType.PYTHON] = f'''#!/usr/bin/env python3
import sys

def main():
    """Build script for {script_name}"""
    print("Starting {script_name} build...")
    {python_steps}
    print("Build completed successfully")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''

    return script_name, build_steps, script_contents


class TestPlatformEquivalenceProperties:
    """平台等效性属性测试类"""

    def setup_method(self):
        """设置测试方法"""
        self.platform_manager = PlatformManager()
        self.validator = CompatibilityValidator(self.platform_manager)

    @given(
        script_set=equivalent_script_sets()
    )
    @settings(
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_platform_specific_script_equivalence_property(self, script_set):
        """
        Feature: script-delivery-validation, Property 3: 平台特定脚本等效性

        对于任何功能等效的平台特定脚本组（如build.bat、build.sh、build.py），
        在各自平台上执行应该产生等效的最终结果
        验证：需求 1.5
        """
        script_name, category, script_contents = script_set

        current_platform = self.platform_manager.detect_current_platform()

        # 创建临时脚本文件
        temp_scripts = {}
        script_objects = {}

        try:
            for script_type, content in script_contents.items():
                # 检查脚本类型是否在当前平台支持
                if not self._is_script_type_supported(script_type, current_platform):
                    continue

                extension = self._get_script_extension(script_type)

                with tempfile.NamedTemporaryFile(
                    mode='w',
                    suffix=extension,
                    delete=False,
                    encoding='utf-8'
                ) as f:
                    f.write(content)
                    temp_path = Path(f.name)
                    temp_scripts[script_type] = temp_path

                # 创建脚本对象
                script_objects[script_type] = Script(
                    path=temp_path,
                    name=f"{script_name}_{script_type.value}",
                    type=script_type,
                    platform=current_platform,
                    metadata=ScriptMetadata(
                        description=f"{category.value} script for {script_name}",
                        usage=f"./{script_name}{extension}"
                    ),
                    category=category
                )

            # 如果没有支持的脚本类型，跳过测试
            if not script_objects:
                pytest.skip(f"No supported script types for platform {current_platform}")

            # 执行等效性验证
            validation_results = {}
            for script_type, script in script_objects.items():
                result = self.validator.validate(script, current_platform)
                validation_results[script_type] = result

            # 验证平台特定脚本的等效性
            self._verify_script_equivalence(validation_results, category)

        finally:
            # 清理临时文件
            for temp_path in temp_scripts.values():
                if temp_path.exists():
                    temp_path.unlink()

    @given(
        build_variants=build_script_variants()
    )
    @settings(
        max_examples=8,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_build_script_equivalence(self, build_variants):
        """
        测试构建脚本的等效性
        验证：需求 1.5 - 平台特定脚本等效性
        """
        script_name, build_steps, script_contents = build_variants

        current_platform = self.platform_manager.detect_current_platform()

        temp_scripts = {}
        validation_results = {}

        try:
            for script_type, content in script_contents.items():
                if not self._is_script_type_supported(script_type, current_platform):
                    continue

                extension = self._get_script_extension(script_type)

                with tempfile.NamedTemporaryFile(
                    mode='w',
                    suffix=extension,
                    delete=False,
                    encoding='utf-8'
                ) as f:
                    f.write(content)
                    temp_path = Path(f.name)
                    temp_scripts[script_type] = temp_path

                script = Script(
                    path=temp_path,
                    name=f"build_{script_name}_{script_type.value}",
                    type=script_type,
                    platform=current_platform,
                    metadata=ScriptMetadata(
                        description=f"Build script for {script_name}",
                        usage=f"./build_{script_name}{extension}"
                    ),
                    category=ScriptCategory.BUILD
                )

                result = self.validator.validate(script, current_platform)
                validation_results[script_type] = result

            if not validation_results:
                pytest.skip(f"No supported script types for platform {current_platform}")

            # 验证构建脚本的等效性
            self._verify_build_script_equivalence(validation_results, build_steps)

        finally:
            for temp_path in temp_scripts.values():
                if temp_path.exists():
                    temp_path.unlink()

    @given(
        script_name=st.text(
            alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
            min_size=1,
            max_size=15
        ).filter(lambda x: x.isalnum())
    )
    @settings(
        max_examples=8,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_test_script_equivalence(self, script_name):
        """
        测试测试脚本的等效性
        验证：需求 1.5 - 平台特定脚本等效性
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 创建等效的测试脚本
        test_scripts = {
            ScriptType.BATCH: f'''@echo off
echo Running {script_name} test suite...
echo Unit tests: 5 passed
echo Integration tests: 3 passed
echo Total: 8 tests passed
exit /b 0
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Running {script_name} test suite..."
echo "Unit tests: 5 passed"
echo "Integration tests: 3 passed"
echo "Total: 8 tests passed"
exit 0
''',
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys

def main():
    """Test script for {script_name}"""
    print("Running {script_name} test suite...")
    print("Unit tests: 5 passed")
    print("Integration tests: 3 passed")
    print("Total: 8 tests passed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        }

        temp_scripts = {}
        validation_results = {}

        try:
            for script_type, content in test_scripts.items():
                if not self._is_script_type_supported(script_type, current_platform):
                    continue

                extension = self._get_script_extension(script_type)

                with tempfile.NamedTemporaryFile(
                    mode='w',
                    suffix=extension,
                    delete=False,
                    encoding='utf-8'
                ) as f:
                    f.write(content)
                    temp_path = Path(f.name)
                    temp_scripts[script_type] = temp_path

                script = Script(
                    path=temp_path,
                    name=f"test_{script_name}_{script_type.value}",
                    type=script_type,
                    platform=current_platform,
                    metadata=ScriptMetadata(
                        description=f"Test script for {script_name}",
                        usage=f"./test_{script_name}{extension}"
                    ),
                    category=ScriptCategory.TEST
                )

                result = self.validator.validate(script, current_platform)
                validation_results[script_type] = result

            if not validation_results:
                pytest.skip(f"No supported script types for platform {current_platform}")

            # 验证测试脚本的等效性
            self._verify_test_script_equivalence(validation_results)

        finally:
            for temp_path in temp_scripts.values():
                if temp_path.exists():
                    temp_path.unlink()

    def test_platform_adapter_equivalence(self):
        """
        测试平台适配器的等效性
        验证：需求 1.5 - 平台特定脚本等效性
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 获取当前平台的适配器
        adapter = self.platform_manager.get_platform_adapter(current_platform)
        assert adapter is not None, f"应该能够获取 {current_platform} 的适配器"

        # 验证适配器的基本功能
        env_info = adapter.get_environment_info()
        assert env_info is not None, "适配器应该能够提供环境信息"
        assert env_info.platform == current_platform, "环境信息应该匹配当前平台"

        # 验证依赖检查功能
        common_deps = ['python', 'nonexistent_command_12345']
        dep_results = adapter.check_dependencies(common_deps)
        assert len(dep_results) == len(common_deps), "依赖检查结果数量应该匹配"

        # 验证不存在的命令被正确识别
        nonexistent_result = None
        for result in dep_results:
            if result.dependency == 'nonexistent_command_12345':
                nonexistent_result = result
                break

        assert nonexistent_result is not None, "应该包含不存在命令的检查结果"
        assert not nonexistent_result.available, "不存在的命令应该被标记为不可用"

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

    def _verify_script_equivalence(self, validation_results, category):
        """验证脚本等效性"""
        if len(validation_results) < 2:
            return  # 需要至少两个脚本来比较等效性

        # 收集所有成功的验证结果
        successful_results = []
        for script_type, result in validation_results.items():
            if result.status.value == "passed":
                successful_results.append((script_type, result))

        if len(successful_results) < 2:
            return  # 需要至少两个成功的结果来比较

        # 验证输出的等效性
        outputs = []
        for script_type, result in successful_results:
            normalized_output = self._normalize_script_output(result.output, category)
            outputs.append(normalized_output)

        # 检查输出的相似性
        if len(outputs) >= 2:
            first_output = outputs[0]
            for other_output in outputs[1:]:
                similarity = self._calculate_output_similarity(first_output, other_output)
                assert similarity > 0.7, f"脚本输出等效性不足: {similarity}"

    def _verify_build_script_equivalence(self, validation_results, build_steps):
        """验证构建脚本的等效性"""
        successful_results = []
        for script_type, result in validation_results.items():
            if result.status.value == "passed":
                successful_results.append((script_type, result))

        if len(successful_results) < 2:
            return

        # 验证所有构建步骤都在输出中出现
        for script_type, result in successful_results:
            output_lower = result.output.lower()
            for step in build_steps:
                assert step.lower() in output_lower, \
                    f"构建步骤 '{step}' 应该在 {script_type} 脚本输出中: {result.output}"

    def _verify_test_script_equivalence(self, validation_results):
        """验证测试脚本的等效性"""
        successful_results = []
        for script_type, result in validation_results.items():
            if result.status.value == "passed":
                successful_results.append((script_type, result))

        if len(successful_results) < 2:
            return

        # 验证测试结果的一致性
        for script_type, result in successful_results:
            output_lower = result.output.lower()
            # 所有测试脚本都应该报告相同的测试数量
            assert "5 passed" in output_lower, f"应该报告5个单元测试通过: {result.output}"
            assert "3 passed" in output_lower, f"应该报告3个集成测试通过: {result.output}"
            assert "8 tests passed" in output_lower, f"应该报告总共8个测试通过: {result.output}"

    def _normalize_script_output(self, output, category):
        """标准化脚本输出"""
        if not output:
            return ""

        # 转换为小写并标准化换行符
        normalized = output.lower().replace('\r\n', '\n').replace('\r', '\n')

        # 移除时间戳和路径等变化的信息
        lines = normalized.split('\n')
        filtered_lines = []

        for line in lines:
            # 保留包含功能关键词的行
            if any(keyword in line for keyword in [
                category.value.lower(), 'completed', 'success', 'passed', 'started'
            ]):
                filtered_lines.append(line.strip())

        return '\n'.join(filtered_lines)

    def _calculate_output_similarity(self, output1, output2):
        """计算两个输出的相似度"""
        if not output1 and not output2:
            return 1.0
        if not output1 or not output2:
            return 0.0

        words1 = set(output1.split())
        words2 = set(output2.split())

        if not words1 and not words2:
            return 1.0
        if not words1 or not words2:
            return 0.0

        intersection = words1.intersection(words2)
        union = words1.union(words2)

        return len(intersection) / len(union) if union else 0.0
