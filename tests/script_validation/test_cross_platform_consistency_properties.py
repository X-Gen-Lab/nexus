"""
跨平台一致性属性测试

Feature: script-delivery-validation, Property 2: 跨平台脚本一致性
验证：需求 1.4
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
from script_validation.validators.compatibility_validator import CompatibilityValidator
from script_validation.managers.platform_manager import PlatformManager


# 策略定义
@st.composite
def cross_platform_python_scripts(draw):
    """生成跨平台Python脚本"""
    script_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
        min_size=1,
        max_size=15
    ).filter(lambda x: x.isalnum()))

    # 生成跨平台兼容的Python脚本内容
    script_templates = [
        '''#!/usr/bin/env python3
import sys
import os

def main():
    """Cross-platform main function"""
    print("Hello from cross-platform script")
    print(f"Platform: {sys.platform}")
    print(f"Python version: {sys.version}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
        '''#!/usr/bin/env python3
import sys
import platform

def main():
    """Cross-platform system info"""
    print("System Information:")
    print(f"OS: {platform.system()}")
    print(f"Architecture: {platform.architecture()[0]}")
    print(f"Python: {platform.python_version()}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
        '''#!/usr/bin/env python3
import sys
import json

def main():
    """Cross-platform JSON processing"""
    data = {"message": "Hello World", "platform": sys.platform}
    print(json.dumps(data, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
        '''#!/usr/bin/env python3
import sys
import hashlib

def main():
    """Cross-platform hash calculation"""
    text = "Hello World"
    hash_obj = hashlib.sha256(text.encode())
    print(f"Text: {text}")
    print(f"SHA256: {hash_obj.hexdigest()}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
    ]

    content = draw(st.sampled_from(script_templates))
    return script_name, content


@st.composite
def script_input_data(draw):
    """生成脚本输入数据"""
    input_type = draw(st.sampled_from(['text', 'number', 'json']))

    if input_type == 'text':
        return draw(st.text(min_size=1, max_size=50))
    elif input_type == 'number':
        return str(draw(st.integers(min_value=1, max_value=1000)))
    else:  # json
        data = {
            'name': draw(st.text(min_size=1, max_size=20)),
            'value': draw(st.integers(min_value=1, max_value=100))
        }
        return str(data)


class TestCrossPlatformConsistencyProperties:
    """跨平台一致性属性测试类"""

    def setup_method(self):
        """设置测试方法"""
        self.platform_manager = PlatformManager()
        self.validator = CompatibilityValidator(self.platform_manager)

    @given(
        script_data=cross_platform_python_scripts(),
        input_data=script_input_data()
    )
    @settings(
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_cross_platform_script_consistency_property(self, script_data, input_data):
        """
        Feature: script-delivery-validation, Property 2: 跨平台脚本一致性

        对于任何跨平台Python脚本，在所有支持的平台上执行相同的输入应该产生等效的输出结果
        验证：需求 1.4
        """
        script_name, content = script_data

        # 获取当前平台
        current_platform = self.platform_manager.detect_current_platform()

        # 只测试Python脚本的跨平台一致性
        script_type = ScriptType.PYTHON

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.py',
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
                    description="Cross-platform Python script for testing",
                    usage=f"python {script_name}.py"
                )
            )

            # 执行跨平台一致性验证
            result = self.validator.validate(script, current_platform)

            # 验证跨平台一致性
            assert result is not None, "验证结果不能为空"
            assert hasattr(result, 'status'), "验证结果应该包含状态"
            assert hasattr(result, 'details'), "验证结果应该包含详细信息"

            # 验证跨平台一致性检查结果
            if result.details and 'cross_platform_consistency' in result.details:
                consistency_result = result.details['cross_platform_consistency']

                # 验证一致性检查的结构
                assert isinstance(consistency_result, dict), "一致性结果应该是字典"

                # 如果有多个平台的结果，验证它们的一致性
                if 'platform_results' in consistency_result:
                    platform_results = consistency_result['platform_results']

                    if len(platform_results) > 1:
                        # 验证输出的一致性（允许平台特定的差异）
                        self._verify_output_consistency(platform_results)

                        # 验证退出代码的一致性
                        self._verify_exit_code_consistency(platform_results)

            # 验证脚本在当前平台上的基本功能
            if result.status.value == "passed":
                assert result.execution_time >= 0, "执行时间应该非负"
                assert result.output is not None, "应该有输出信息"

        finally:
            # 清理临时文件
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
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_python_standard_library_consistency(self, script_name):
        """
        测试Python标准库在不同平台上的一致性
        验证：需求 1.4 - 跨平台脚本一致性
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 创建使用Python标准库的脚本
        content = '''#!/usr/bin/env python3
import sys
import os
import json
import hashlib
import datetime

def main():
    """Test standard library consistency"""
    # Test basic operations
    print("Testing standard library consistency")

    # Test JSON
    data = {"test": "value", "number": 42}
    json_str = json.dumps(data, sort_keys=True)
    parsed = json.loads(json_str)
    print(f"JSON test: {parsed['test']}")

    # Test hash
    text = "consistency_test"
    hash_obj = hashlib.md5(text.encode())
    print(f"Hash: {hash_obj.hexdigest()}")

    # Test datetime
    now = datetime.datetime.now()
    print(f"Year: {now.year}")

    return 0

if __name__ == "__main__":
    sys.exit(main())
'''

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.py',
            delete=False,
            encoding='utf-8'
        ) as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name=script_name,
                type=ScriptType.PYTHON,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description="Standard library consistency test",
                    usage=f"python {script_name}.py"
                )
            )

            result = self.validator.validate(script, current_platform)

            # 验证标准库一致性
            assert result is not None

            if result.status.value == "passed":
                # 验证输出包含预期的标准库操作结果
                output_lower = result.output.lower()
                assert any(keyword in output_lower for keyword in [
                    "json test", "hash:", "year:", "consistency"
                ]), f"输出应包含标准库操作结果: {result.output}"

        finally:
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
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_file_operations_consistency(self, script_name):
        """
        测试文件操作在不同平台上的一致性
        验证：需求 1.4 - 跨平台脚本一致性
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 创建执行文件操作的脚本
        content = '''#!/usr/bin/env python3
import sys
import os
import tempfile
from pathlib import Path

def main():
    """Test file operations consistency"""
    print("Testing file operations consistency")

    # Create temporary file
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
        f.write("Test content for consistency")
        temp_file = f.name

    try:
        # Read file
        with open(temp_file, 'r') as f:
            content = f.read()
        print(f"File content length: {len(content)}")

        # Check file exists
        if os.path.exists(temp_file):
            print("File exists: True")

        # Get file size
        size = os.path.getsize(temp_file)
        print(f"File size: {size}")

    finally:
        # Clean up
        if os.path.exists(temp_file):
            os.unlink(temp_file)
            print("File cleaned up")

    return 0

if __name__ == "__main__":
    sys.exit(main())
'''

        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.py',
            delete=False,
            encoding='utf-8'
        ) as f:
            f.write(content)
            temp_path = Path(f.name)

        try:
            script = Script(
                path=temp_path,
                name=script_name,
                type=ScriptType.PYTHON,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description="File operations consistency test",
                    usage=f"python {script_name}.py"
                )
            )

            result = self.validator.validate(script, current_platform)

            # 验证文件操作一致性
            assert result is not None

            if result.status.value == "passed":
                # 验证输出包含预期的文件操作结果
                output_lower = result.output.lower()
                assert any(keyword in output_lower for keyword in [
                    "file content length", "file exists", "file size", "cleaned up"
                ]), f"输出应包含文件操作结果: {result.output}"

        finally:
            if temp_path.exists():
                temp_path.unlink()

    def test_platform_detection_consistency(self):
        """
        测试平台检测的一致性
        验证：需求 1.4 - 跨平台脚本一致性
        """
        # 测试平台管理器的一致性
        current_platform = self.platform_manager.detect_current_platform()

        # 验证平台检测结果
        assert isinstance(current_platform, Platform), "平台检测应返回Platform枚举"
        assert current_platform in [Platform.WINDOWS, Platform.LINUX, Platform.WSL], \
            f"检测到的平台应该是支持的平台之一: {current_platform}"

        # 多次检测应该返回相同结果
        for _ in range(3):
            detected = self.platform_manager.detect_current_platform()
            assert detected == current_platform, "多次平台检测应该返回相同结果"

    def _verify_output_consistency(self, platform_results):
        """验证不同平台输出的一致性"""
        if len(platform_results) < 2:
            return

        outputs = []
        for platform, result in platform_results.items():
            if 'output' in result:
                # 标准化输出（移除平台特定信息）
                normalized_output = self._normalize_output(result['output'])
                outputs.append(normalized_output)

        if len(outputs) >= 2:
            # 检查输出的核心内容是否一致
            # 允许平台特定的差异，但核心功能输出应该相似
            first_output = outputs[0]
            for other_output in outputs[1:]:
                similarity = self._calculate_output_similarity(first_output, other_output)
                assert similarity > 0.5, f"跨平台输出一致性不足: {similarity}"

    def _verify_exit_code_consistency(self, platform_results):
        """验证不同平台退出代码的一致性"""
        exit_codes = []
        for platform, result in platform_results.items():
            if 'exit_code' in result:
                exit_codes.append(result['exit_code'])

        if len(exit_codes) >= 2:
            # 所有平台的退出代码应该相同
            first_code = exit_codes[0]
            for code in exit_codes[1:]:
                assert code == first_code, f"跨平台退出代码不一致: {exit_codes}"

    def _normalize_output(self, output):
        """标准化输出，移除平台特定信息"""
        if not output:
            return ""

        # 移除平台特定的路径分隔符差异
        normalized = output.replace('\\', '/')

        # 移除平台特定的换行符差异
        normalized = normalized.replace('\r\n', '\n').replace('\r', '\n')

        # 移除平台特定的版本信息
        lines = normalized.split('\n')
        filtered_lines = []
        for line in lines:
            # 跳过包含平台特定信息的行
            if not any(platform_info in line.lower() for platform_info in [
                'windows', 'linux', 'darwin', 'win32', 'platform:', 'python version:'
            ]):
                filtered_lines.append(line)

        return '\n'.join(filtered_lines)

    def _calculate_output_similarity(self, output1, output2):
        """计算两个输出的相似度"""
        if not output1 and not output2:
            return 1.0
        if not output1 or not output2:
            return 0.0

        # 简单的相似度计算：基于共同的单词数量
        words1 = set(output1.lower().split())
        words2 = set(output2.lower().split())

        if not words1 and not words2:
            return 1.0
        if not words1 or not words2:
            return 0.0

        intersection = words1.intersection(words2)
        union = words1.union(words2)

        return len(intersection) / len(union) if union else 0.0
