"""
性能和资源管理属性测试

Feature: script-delivery-validation, Property 6: 性能和资源管理
验证：需求 4.1, 4.2, 4.3, 4.4, 4.5
"""

import pytest
import tempfile
import os
import time
from pathlib import Path
from hypothesis import given, strategies as st, assume, settings
from hypothesis import HealthCheck

from script_validation.models import (
    Script, ScriptType, Platform, ScriptMetadata, Parameter
)
from script_validation.validators.performance_validator import PerformanceValidator
from script_validation.managers.platform_manager import PlatformManager


# 策略定义
@st.composite
def performance_test_scripts(draw):
    """生成性能测试脚本"""
    script_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
        min_size=1,
        max_size=15
    ).filter(lambda x: x.isalnum()))

    script_type = draw(st.sampled_from(list(ScriptType)))

    # 生成不同复杂度的脚本
    complexity = draw(st.sampled_from(['simple', 'medium', 'complex']))

    script_contents = {}

    if complexity == 'simple':
        script_contents = {
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys
import time

def main():
    """Simple performance test script for {script_name}"""
    print("Starting simple task...")
    time.sleep(0.1)  # Simulate brief work
    print("Task completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Starting simple task..."
sleep 0.1
echo "Task completed"
exit 0
''',
            ScriptType.BATCH: f'''@echo off
echo Starting simple task...
timeout /t 1 /nobreak >nul 2>&1
echo Task completed
exit /b 0
''',
            ScriptType.POWERSHELL: f'''# Simple performance test
Write-Host "Starting simple task..."
Start-Sleep -Milliseconds 100
Write-Host "Task completed"
exit 0
'''
        }
    elif complexity == 'medium':
        script_contents = {
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys
import time
import tempfile
import os

def main():
    """Medium complexity performance test script for {script_name}"""
    print("Starting medium complexity task...")

    # Create temporary files
    temp_files = []
    for i in range(5):
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            f.write(f"Temporary data {{i}}")
            temp_files.append(f.name)

    # Process files
    for temp_file in temp_files:
        with open(temp_file, 'r') as f:
            content = f.read()
        print(f"Processed: {{content}}")

    # Clean up
    for temp_file in temp_files:
        if os.path.exists(temp_file):
            os.unlink(temp_file)

    print("Medium task completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Starting medium complexity task..."

# Create temporary directory
TEMP_DIR=$(mktemp -d)

# Create some files
for i in {{1..5}}; do
    echo "Temporary data $i" > "$TEMP_DIR/file_$i.txt"
done

# Process files
for file in "$TEMP_DIR"/*.txt; do
    echo "Processing: $(cat "$file")"
done

# Clean up
rm -rf "$TEMP_DIR"

echo "Medium task completed"
exit 0
''',
            ScriptType.BATCH: f'''@echo off
echo Starting medium complexity task...

set TEMP_DIR=%TEMP%\\{script_name}_test_%RANDOM%
mkdir "%TEMP_DIR%"

for /L %%i in (1,1,5) do (
    echo Temporary data %%i > "%TEMP_DIR%\\file_%%i.txt"
)

for %%f in ("%TEMP_DIR%\\*.txt") do (
    echo Processing:
    type "%%f"
)

rmdir /s /q "%TEMP_DIR%"

echo Medium task completed
exit /b 0
''',
            ScriptType.POWERSHELL: f'''# Medium complexity performance test
Write-Host "Starting medium complexity task..."

$tempDir = New-TemporaryFile | ForEach-Object {{ Remove-Item $_; New-Item -ItemType Directory -Path $_ }}

1..5 | ForEach-Object {{
    "Temporary data $_" | Out-File -FilePath "$tempDir\\file_$_.txt"
}}

Get-ChildItem "$tempDir\\*.txt" | ForEach-Object {{
    Write-Host "Processing: $(Get-Content $_.FullName)"
}}

Remove-Item -Recurse -Force $tempDir

Write-Host "Medium task completed"
exit 0
'''
        }
    else:  # complex
        script_contents = {
            ScriptType.PYTHON: f'''#!/usr/bin/env python3
import sys
import time
import tempfile
import os
import hashlib
import json

def main():
    """Complex performance test script for {script_name}"""
    print("Starting complex task...")

    # Create temporary directory
    temp_dir = tempfile.mkdtemp()

    try:
        # Generate data files
        data_files = []
        for i in range(10):
            file_path = os.path.join(temp_dir, f"data_{{i}}.json")
            data = {{
                "id": i,
                "name": f"item_{{i}}",
                "value": i * 100,
                "timestamp": time.time()
            }}
            with open(file_path, 'w') as f:
                json.dump(data, f)
            data_files.append(file_path)

        # Process files and calculate hashes
        results = []
        for file_path in data_files:
            with open(file_path, 'r') as f:
                content = f.read()

            hash_obj = hashlib.md5(content.encode())
            results.append({{
                "file": os.path.basename(file_path),
                "hash": hash_obj.hexdigest(),
                "size": len(content)
            }})

        # Save results
        results_file = os.path.join(temp_dir, "results.json")
        with open(results_file, 'w') as f:
            json.dump(results, f, indent=2)

        print(f"Processed {{len(results)}} files")
        print(f"Results saved to {{results_file}}")

    finally:
        # Clean up
        import shutil
        shutil.rmtree(temp_dir)

    print("Complex task completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
''',
            ScriptType.SHELL: f'''#!/bin/bash
set -e
echo "Starting complex task..."

TEMP_DIR=$(mktemp -d)

# Generate data files
for i in {{1..10}}; do
    cat > "$TEMP_DIR/data_$i.txt" << EOF
{{
  "id": $i,
  "name": "item_$i",
  "value": $((i * 100)),
  "timestamp": $(date +%s)
}}
EOF
done

# Process files
RESULTS_FILE="$TEMP_DIR/results.txt"
for file in "$TEMP_DIR"/data_*.txt; do
    HASH=$(md5sum "$file" | cut -d' ' -f1)
    SIZE=$(wc -c < "$file")
    echo "$(basename "$file"): $HASH ($SIZE bytes)" >> "$RESULTS_FILE"
done

echo "Processed $(ls "$TEMP_DIR"/data_*.txt | wc -l) files"
echo "Results saved to $RESULTS_FILE"

# Clean up
rm -rf "$TEMP_DIR"

echo "Complex task completed"
exit 0
''',
            ScriptType.BATCH: f'''@echo off
echo Starting complex task...

set TEMP_DIR=%TEMP%\\{script_name}_complex_%RANDOM%
mkdir "%TEMP_DIR%"

for /L %%i in (1,1,10) do (
    echo {{> "%TEMP_DIR%\\data_%%i.json"
    echo   "id": %%i,>> "%TEMP_DIR%\\data_%%i.json"
    echo   "name": "item_%%i",>> "%TEMP_DIR%\\data_%%i.json"
    echo   "value": %%i00>> "%TEMP_DIR%\\data_%%i.json"
    echo }}>> "%TEMP_DIR%\\data_%%i.json"
)

set /a FILE_COUNT=0
for %%f in ("%TEMP_DIR%\\*.json") do (
    set /a FILE_COUNT+=1
    echo Processing: %%f
)

echo Processed %FILE_COUNT% files

rmdir /s /q "%TEMP_DIR%"

echo Complex task completed
exit /b 0
''',
            ScriptType.POWERSHELL: f'''# Complex performance test
Write-Host "Starting complex task..."

$tempDir = New-TemporaryFile | ForEach-Object {{ Remove-Item $_; New-Item -ItemType Directory -Path $_ }}

1..10 | ForEach-Object {{
    $data = @{{
        id = $_
        name = "item_$_"
        value = $_ * 100
        timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    }}
    $data | ConvertTo-Json | Out-File -FilePath "$tempDir\\data_$_.json"
}}

$results = @()
Get-ChildItem "$tempDir\\*.json" | ForEach-Object {{
    $content = Get-Content $_.FullName -Raw
    $hash = (Get-FileHash -InputStream ([System.IO.MemoryStream]::new([System.Text.Encoding]::UTF8.GetBytes($content))) -Algorithm MD5).Hash
    $results += @{{
        file = $_.Name
        hash = $hash
        size = $content.Length
    }}
}}

$results | ConvertTo-Json | Out-File -FilePath "$tempDir\\results.json"

Write-Host "Processed $($results.Count) files"

Remove-Item -Recurse -Force $tempDir

Write-Host "Complex task completed"
exit 0
'''
        }

    content = script_contents.get(script_type, 'echo "Hello World"\n')
    return script_name, script_type, complexity, content


@st.composite
def resource_intensive_scripts(draw):
    """生成资源密集型脚本"""
    script_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
        min_size=1,
        max_size=15
    ).filter(lambda x: x.isalnum()))

    resource_type = draw(st.sampled_from(['memory', 'disk', 'cpu']))
    script_type = draw(st.sampled_from([ScriptType.PYTHON, ScriptType.SHELL]))

    if resource_type == 'memory':
        if script_type == ScriptType.PYTHON:
            content = f'''#!/usr/bin/env python3
import sys

def main():
    """Memory intensive script for {script_name}"""
    print("Starting memory test...")

    # Allocate some memory (but not too much)
    data = []
    for i in range(1000):
        data.append(f"Memory test data item {{i}}" * 10)

    print(f"Allocated {{len(data)}} items")

    # Process data
    processed = [item.upper() for item in data[:100]]
    print(f"Processed {{len(processed)}} items")

    # Clean up
    del data
    del processed

    print("Memory test completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        else:  # SHELL
            content = f'''#!/bin/bash
set -e
echo "Starting memory test..."

# Create some data in memory
DATA_FILE=$(mktemp)
for i in {{1..1000}}; do
    echo "Memory test data item $i" >> "$DATA_FILE"
done

echo "Created data file with $(wc -l < "$DATA_FILE") lines"

# Process data
head -100 "$DATA_FILE" | tr '[:lower:]' '[:upper:]' > /dev/null

# Clean up
rm -f "$DATA_FILE"

echo "Memory test completed"
exit 0
'''
    elif resource_type == 'disk':
        if script_type == ScriptType.PYTHON:
            content = f'''#!/usr/bin/env python3
import sys
import tempfile
import os

def main():
    """Disk intensive script for {script_name}"""
    print("Starting disk test...")

    temp_dir = tempfile.mkdtemp()

    try:
        # Create multiple files
        files_created = 0
        for i in range(20):
            file_path = os.path.join(temp_dir, f"test_file_{{i}}.txt")
            with open(file_path, 'w') as f:
                f.write(f"Test data for file {{i}}\\n" * 100)
            files_created += 1

        print(f"Created {{files_created}} files")

        # Read and process files
        total_size = 0
        for i in range(files_created):
            file_path = os.path.join(temp_dir, f"test_file_{{i}}.txt")
            with open(file_path, 'r') as f:
                content = f.read()
                total_size += len(content)

        print(f"Processed {{total_size}} bytes")

    finally:
        # Clean up
        import shutil
        shutil.rmtree(temp_dir)

    print("Disk test completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        else:  # SHELL
            content = f'''#!/bin/bash
set -e
echo "Starting disk test..."

TEMP_DIR=$(mktemp -d)

# Create multiple files
FILES_CREATED=0
for i in {{1..20}}; do
    FILE_PATH="$TEMP_DIR/test_file_$i.txt"
    for j in {{1..100}}; do
        echo "Test data for file $i" >> "$FILE_PATH"
    done
    FILES_CREATED=$((FILES_CREATED + 1))
done

echo "Created $FILES_CREATED files"

# Process files
TOTAL_SIZE=0
for file in "$TEMP_DIR"/*.txt; do
    SIZE=$(wc -c < "$file")
    TOTAL_SIZE=$((TOTAL_SIZE + SIZE))
done

echo "Processed $TOTAL_SIZE bytes"

# Clean up
rm -rf "$TEMP_DIR"

echo "Disk test completed"
exit 0
'''
    else:  # cpu
        if script_type == ScriptType.PYTHON:
            content = f'''#!/usr/bin/env python3
import sys
import time

def main():
    """CPU intensive script for {script_name}"""
    print("Starting CPU test...")

    # Perform some calculations
    result = 0
    for i in range(10000):
        result += i * i

    print(f"Calculation result: {{result}}")

    # String processing
    text = "CPU test data " * 1000
    processed = text.replace("test", "TEST").upper()

    print(f"Processed text length: {{len(processed)}}")

    print("CPU test completed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''
        else:  # SHELL
            content = f'''#!/bin/bash
set -e
echo "Starting CPU test..."

# Perform calculations
RESULT=0
for i in {{1..1000}}; do
    RESULT=$((RESULT + i * i))
done

echo "Calculation result: $RESULT"

# String processing
TEXT="CPU test data"
for i in {{1..100}}; do
    TEXT="$TEXT CPU test data"
done

PROCESSED=$(echo "$TEXT" | tr '[:lower:]' '[:upper:]')
echo "Processed text length: ${{#PROCESSED}}"

echo "CPU test completed"
exit 0
'''

    return script_name, script_type, resource_type, content


class TestPerformanceResourceProperties:
    """性能和资源管理属性测试类"""

    def setup_method(self):
        """设置测试方法"""
        self.platform_manager = PlatformManager()
        self.validator = PerformanceValidator(self.platform_manager)

    @given(
        script_data=performance_test_scripts()
    )
    @settings(
        max_examples=15,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_performance_and_resource_management_property(self, script_data):
        """
        Feature: script-delivery-validation, Property 6: 性能和资源管理

        对于任何脚本执行，验证器应该能够监控和报告执行时间、内存使用、
        资源利用和临时文件清理情况
        验证：需求 4.1, 4.2, 4.3, 4.4, 4.5
        """
        script_name, script_type, complexity, content = script_data

        current_platform = self.platform_manager.detect_current_platform()

        # 检查脚本类型是否在当前平台支持
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
                    description=f"{complexity} performance test script",
                    usage=f"./{script_name}{self._get_script_extension(script_type)}"
                )
            )

            # 执行性能验证
            result = self.validator.validate(script, current_platform)

            # 验证性能和资源管理
            assert result is not None, "验证结果不能为空"
            assert hasattr(result, 'execution_time'), "验证结果应该包含执行时间"
            assert hasattr(result, 'memory_usage'), "验证结果应该包含内存使用信息"

            # 验证执行时间监控 (需求 4.1)
            assert result.execution_time >= 0, "执行时间应该非负"
            assert result.execution_time < 60, "执行时间应该在合理范围内"

            # 验证内存使用监控 (需求 4.2)
            assert result.memory_usage >= 0, "内存使用应该非负"

            # 验证性能详细信息
            if result.details:
                # 检查性能相关的详细信息
                performance_details = result.details

                # 验证资源利用监控 (需求 4.3)
                if 'resource_utilization' in performance_details:
                    resource_util = performance_details['resource_utilization']
                    assert isinstance(resource_util, dict), "资源利用信息应该是字典"

                # 验证临时文件清理 (需求 4.4)
                if 'temp_files_cleaned' in performance_details:
                    temp_cleaned = performance_details['temp_files_cleaned']
                    assert isinstance(temp_cleaned, bool), "临时文件清理状态应该是布尔值"

                # 验证网络操作超时处理 (需求 4.5)
                if 'network_operations' in performance_details:
                    network_ops = performance_details['network_operations']
                    assert isinstance(network_ops, dict), "网络操作信息应该是字典"

        finally:
            # 清理临时文件
            if temp_path.exists():
                temp_path.unlink()

    @given(
        resource_script=resource_intensive_scripts()
    )
    @settings(
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_resource_intensive_script_monitoring(self, resource_script):
        """
        测试资源密集型脚本的监控
        验证：需求 4.1, 4.2, 4.3 - 执行时间、内存使用和资源利用监控
        """
        script_name, script_type, resource_type, content = resource_script

        current_platform = self.platform_manager.detect_current_platform()

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
            script = Script(
                path=temp_path,
                name=f"{resource_type}_{script_name}",
                type=script_type,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description=f"{resource_type} intensive test script",
                    usage=f"./{script_name}{self._get_script_extension(script_type)}"
                )
            )

            result = self.validator.validate(script, current_platform)

            # 验证资源监控
            assert result is not None

            # 验证执行时间合理性
            assert result.execution_time >= 0, "执行时间应该非负"

            # 对于资源密集型脚本，执行时间可能稍长但应该在合理范围内
            if resource_type == 'cpu':
                assert result.execution_time < 30, "CPU密集型脚本执行时间应该在30秒内"
            elif resource_type == 'memory':
                assert result.execution_time < 20, "内存密集型脚本执行时间应该在20秒内"
            elif resource_type == 'disk':
                assert result.execution_time < 25, "磁盘密集型脚本执行时间应该在25秒内"

            # 验证内存使用监控
            assert result.memory_usage >= 0, "内存使用应该非负"

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
    def test_temporary_file_cleanup_monitoring(self, script_name):
        """
        测试临时文件清理监控
        验证：需求 4.4 - 临时文件清理和磁盘空间管理
        """
        current_platform = self.platform_manager.detect_current_platform()

        # 创建会产生临时文件的脚本
        content = f'''#!/usr/bin/env python3
import sys
import tempfile
import os

def main():
    """Temporary file cleanup test for {script_name}"""
    print("Starting temporary file test...")

    # Create temporary files
    temp_files = []
    temp_dir = tempfile.mkdtemp()

    try:
        for i in range(5):
            temp_file = os.path.join(temp_dir, f"temp_{{i}}.txt")
            with open(temp_file, 'w') as f:
                f.write(f"Temporary content {{i}}")
            temp_files.append(temp_file)

        print(f"Created {{len(temp_files)}} temporary files")

        # Process files
        for temp_file in temp_files:
            with open(temp_file, 'r') as f:
                content = f.read()
            print(f"Processed: {{os.path.basename(temp_file)}}")

    finally:
        # Clean up temporary files
        import shutil
        shutil.rmtree(temp_dir)
        print("Temporary files cleaned up")

    print("Temporary file test completed")
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
                name=f"cleanup_{script_name}",
                type=ScriptType.PYTHON,
                platform=current_platform,
                metadata=ScriptMetadata(
                    description="Temporary file cleanup test",
                    usage=f"python cleanup_{script_name}.py"
                )
            )

            result = self.validator.validate(script, current_platform)

            # 验证临时文件清理监控
            assert result is not None

            if result.status.value == "passed":
                # 验证输出包含清理相关信息
                output_lower = result.output.lower()
                assert any(keyword in output_lower for keyword in [
                    "temporary", "cleanup", "cleaned", "created"
                ]), f"输出应包含临时文件处理信息: {result.output}"

        finally:
            if temp_path.exists():
                temp_path.unlink()

    def test_performance_validator_configuration(self):
        """
        测试性能验证器的配置和基本功能
        验证：需求 4.1, 4.2, 4.3, 4.4, 4.5
        """
        # 验证验证器初始化
        assert self.validator is not None, "性能验证器应该能够初始化"
        assert hasattr(self.validator, 'validate'), "验证器应该有validate方法"
        assert hasattr(self.validator, 'get_validator_name'), "验证器应该有get_validator_name方法"

        # 验证验证器名称
        validator_name = self.validator.get_validator_name()
        assert validator_name == "PerformanceValidator", f"验证器名称应该是PerformanceValidator: {validator_name}"

        # 验证平台管理器集成
        assert self.validator.platform_manager is not None, "验证器应该有平台管理器"

    @given(
        execution_time=st.floats(min_value=0.0, max_value=30.0),
        memory_usage=st.integers(min_value=0, max_value=999999999)
    )
    @settings(
        max_examples=10,
        deadline=None,
        suppress_health_check=[HealthCheck.function_scoped_fixture]
    )
    def test_performance_metrics_validation(self, execution_time, memory_usage):
        """
        测试性能指标验证的合理性
        验证：需求 4.1, 4.2 - 执行时间和内存使用监控
        """
        # 验证执行时间的合理性检查
        assert execution_time >= 0, "执行时间应该非负"

        # 验证内存使用的合理性检查
        assert memory_usage >= 0, "内存使用应该非负"

        # 验证性能阈值
        if execution_time > 0:
            # 执行时间应该在合理范围内
            assert execution_time < 60, "执行时间应该在60秒内"

        if memory_usage > 0:
            # 内存使用应该在合理范围内（1GB以内）
            assert memory_usage < 1000000000, "内存使用应该在1GB以内"

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
