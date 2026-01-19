# Nexus 系统验证框架

Nexus 系统验证框架是一个全面的自动化测试和覆盖率分析工具，用于验证 Nexus 嵌入式平台的配置系统、HAL框架、OSAL框架和平台实现的功能正确性。

## 目录

- [功能特性](#功能特性)
- [系统要求](#系统要求)
- [安装](#安装)
- [快速开始](#快速开始)
- [使用指南](#使用指南)
- [配置选项](#配置选项)
- [报告格式](#报告格式)
- [CI集成](#ci集成)
- [故障排除](#故障排除)
- [开发指南](#开发指南)

## 功能特性

### 测试执行
- **单元测试**: 使用 Google Test 框架执行所有单元测试
- **属性测试**: 使用 Hypothesis 库执行基于属性的测试
- **集成测试**: 验证各子系统之间的交互
- **并行执行**: 支持多核并行测试执行
- **失败快速模式**: 首次失败时立即停止

### 覆盖率分析
- **行覆盖率**: 统计代码行的执行覆盖率
- **分支覆盖率**: 统计分支条件的覆盖率
- **函数覆盖率**: 统计函数调用的覆盖率
- **阈值检查**: 强制执行最低覆盖率要求
- **未覆盖区域标记**: 自动识别未测试的代码区域

### 报告生成
- **汇总报告**: 测试结果统计和概览
- **失败详情**: 详细的错误信息和堆栈跟踪
- **性能报告**: 测试执行时间分析
- **JUnit XML**: 用于CI工具集成
- **HTML可视化**: 交互式网页报告

### CI/CD集成
- **GitHub Actions**: 预配置的工作流
- **多平台支持**: Ubuntu、Windows、macOS
- **覆盖率上传**: 自动上传到 Codecov
- **状态检查**: 自动化的合并门控

## 系统要求

### 必需工具
- **Python**: 3.8 或更高版本
- **CMake**: 3.15 或更高版本
- **Ninja**: 构建系统（推荐）
- **C/C++ 编译器**: GCC 9+, Clang 10+, 或 MSVC 2019+

### 覆盖率工具
- **Linux/macOS**: gcov + lcov
- **Windows**: OpenCppCoverage（可选）

### Python 依赖
```bash
pip install -r scripts/validation/requirements.txt
```

主要依赖：
- `hypothesis`: 属性测试框架
- `pytest`: Python 测试框架
- `jinja2`: 报告模板引擎
- `lxml`: XML 处理
- `pyyaml`: 配置文件解析

## 安装

### 1. 克隆仓库
```bash
git clone https://github.com/your-org/nexus.git
cd nexus
git submodule update --init --recursive
```

### 2. 安装系统依赖

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build lcov python3-pip
```

#### macOS
```bash
brew install cmake ninja lcov python3
```

#### Windows
```powershell
choco install cmake ninja python3
```

### 3. 安装 Python 依赖
```bash
pip install -r scripts/validation/requirements.txt
```

### 4. 验证安装
```bash
python scripts/validation/validate.py --help
```

## 快速开始

### 基本使用

1. **构建并运行所有测试**:
```bash
python scripts/validation/validate.py
```

2. **启用覆盖率分析**:
```bash
python scripts/validation/validate.py --coverage
```

3. **设置覆盖率阈值**:
```bash
python scripts/validation/validate.py --coverage --threshold 0.85
```

4. **并行执行测试**:
```bash
python scripts/validation/validate.py --parallel 4
```

### 典型工作流

```bash
# 1. 清理之前的构建
rm -rf build validation_reports

# 2. 运行完整验证（包括覆盖率）
python scripts/validation/validate.py \
    --coverage \
    --threshold 0.80 \
    --parallel 8 \
    --report-dir validation_reports

# 3. 查看报告
# HTML 报告: validation_reports/report.html
# 覆盖率报告: validation_reports/coverage/index.html
# JUnit XML: validation_reports/junit.xml
```

## 使用指南

### 命令行选项

```
用法: validate.py [选项]

选项:
  --build-dir DIR       构建目录（默认: build）
  --source-dir DIR      源代码目录（默认: .）
  --coverage            启用覆盖率分析
  --threshold FLOAT     覆盖率阈值 0.0-1.0（默认: 0.80）
  --fail-fast           首次测试失败时停止
  --parallel N          并行作业数（默认: CPU核心数）
  --report-dir DIR      报告输出目录（默认: validation_reports）
  --test-timeout SEC    测试超时时间（默认: 300秒）
  --verbose             详细输出模式
  --quiet               安静模式，只输出错误
  --help                显示帮助信息
```

### 运行特定测试套件

验证框架会自动发现并运行所有注册的测试。如果需要运行特定测试：

```bash
# 使用 CTest 直接运行
cd build
ctest -R "config_.*"  # 只运行配置系统测试
ctest -R "hal_.*"     # 只运行 HAL 测试
ctest -R "osal_.*"    # 只运行 OSAL 测试
```

### 覆盖率分析

#### 生成覆盖率报告
```bash
python scripts/validation/validate.py --coverage
```

覆盖率报告将生成在 `validation_reports/coverage/` 目录：
- `index.html`: 主覆盖率报告
- `coverage.info`: lcov 格式数据
- `coverage.xml`: Cobertura XML 格式

#### 检查覆盖率阈值
```bash
python scripts/validation/check_coverage.py \
    --coverage-file build/coverage.info \
    --threshold 0.80
```

#### 查看未覆盖区域
覆盖率报告会高亮显示：
- 🔴 红色：未执行的代码行
- 🟡 黄色：部分覆盖的分支
- 🟢 绿色：完全覆盖的代码

### 属性测试

属性测试使用 Hypothesis 库，每个测试至少运行 100 次迭代：

```python
from hypothesis import given, settings
import hypothesis.strategies as st

@settings(max_examples=100)
@given(value=st.integers())
def test_property(value):
    """
    Feature: system-validation, Property 1: 测试属性
    验证需求: 1.1
    """
    assert some_property_holds(value)
```

查看属性测试失败的反例：
```bash
# 反例会在测试输出中显示
# 例如: Falsifying example: test_property(value=42)
```

## 配置选项

### 配置文件

创建 `validation_config.yaml` 自定义配置：

```yaml
# 构建配置
build:
  directory: build
  generator: Ninja
  build_type: Debug
  
# 测试配置
testing:
  parallel_jobs: 8
  timeout: 300
  fail_fast: false
  
# 覆盖率配置
coverage:
  enabled: true
  threshold: 0.80
  exclude_patterns:
    - "*/tests/*"
    - "*/ext/*"
  
# 报告配置
reporting:
  output_dir: validation_reports
  formats:
    - html
    - junit
    - json
```

使用配置文件：
```bash
python scripts/validation/validate.py --config validation_config.yaml
```

### 环境变量

```bash
# 设置构建目录
export NEXUS_BUILD_DIR=build_coverage

# 设置覆盖率阈值
export NEXUS_COVERAGE_THRESHOLD=0.85

# 设置并行作业数
export NEXUS_PARALLEL_JOBS=16

# 运行验证
python scripts/validation/validate.py
```

## 报告格式

### HTML 报告

主报告 (`validation_reports/report.html`) 包含：
- 测试结果汇总表
- 覆盖率统计图表
- 失败测试详情
- 性能分析
- 交互式过滤和搜索

### JUnit XML 报告

JUnit 格式 (`validation_reports/junit.xml`) 用于 CI 集成：
```xml
<testsuites>
  <testsuite name="config_tests" tests="10" failures="0" time="1.23">
    <testcase name="test_config_set" time="0.12"/>
    ...
  </testsuite>
</testsuites>
```

### 覆盖率报告

覆盖率报告 (`validation_reports/coverage/index.html`) 提供：
- 文件级覆盖率统计
- 源代码行级覆盖率可视化
- 未覆盖代码区域列表
- 分支覆盖率详情

## CI集成

### GitHub Actions

项目已包含预配置的 GitHub Actions 工作流 (`.github/workflows/validation.yml`)：

```yaml
name: System Validation
on: [push, pull_request]
jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Run Validation
        run: python scripts/validation/validate.py --coverage
      - name: Upload Coverage
        uses: codecov/codecov-action@v3
```

### 本地 CI 模拟

模拟 CI 环境运行：
```bash
# 清理环境
rm -rf build validation_reports

# 运行验证（CI 模式）
python scripts/validation/validate.py \
    --coverage \
    --threshold 0.80 \
    --fail-fast \
    --report-dir validation_reports

# 检查退出码
echo $?  # 0 = 成功, 非0 = 失败
```

### 其他 CI 系统

#### GitLab CI
```yaml
test:
  script:
    - pip install -r scripts/validation/requirements.txt
    - python scripts/validation/validate.py --coverage
  artifacts:
    reports:
      junit: validation_reports/junit.xml
      coverage_report:
        coverage_format: cobertura
        path: validation_reports/coverage.xml
```

#### Jenkins
```groovy
stage('Validation') {
    steps {
        sh 'python scripts/validation/validate.py --coverage'
        junit 'validation_reports/junit.xml'
        publishHTML([
            reportDir: 'validation_reports',
            reportFiles: 'report.html',
            reportName: 'Validation Report'
        ])
    }
}
```

## 故障排除

详细的故障排除指南请参见 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)。

### 常见问题

#### 1. 构建失败
```
错误: CMake configuration failed
```
**解决方案**:
- 检查 CMake 版本 >= 3.15
- 确保所有子模块已初始化: `git submodule update --init --recursive`
- 清理构建目录: `rm -rf build`

#### 2. 测试超时
```
错误: Test timeout after 300 seconds
```
**解决方案**:
- 增加超时时间: `--test-timeout 600`
- 检查是否有死锁或无限循环
- 减少并行作业数: `--parallel 2`

#### 3. 覆盖率数据缺失
```
警告: No coverage data found
```
**解决方案**:
- 确保使用 `--coverage` 选项
- 检查编译器支持覆盖率: `gcc --version`
- 验证 lcov 已安装: `lcov --version`

#### 4. 属性测试失败
```
Falsifying example: test_property(value=42)
```
**解决方案**:
- 检查反例是否揭示了真实的 bug
- 如果是测试问题，调整测试策略
- 如果是代码问题，修复实现

### 调试技巧

#### 启用详细输出
```bash
python scripts/validation/validate.py --verbose
```

#### 查看 CMake 配置
```bash
cmake -B build -DNEXUS_BUILD_TESTS=ON -DNEXUS_ENABLE_COVERAGE=ON
cmake -L build  # 列出所有配置选项
```

#### 手动运行单个测试
```bash
cd build
./tests/config/config_tests --gtest_filter="ConfigTest.SetGet"
```

#### 检查覆盖率数据
```bash
# 查看原始覆盖率数据
lcov --list build/coverage.info

# 生成覆盖率报告
genhtml build/coverage.info --output-directory coverage_html
```

## 开发指南

### 添加新测试

1. 在 `tests/` 目录创建测试文件
2. 在 `tests/CMakeLists.txt` 注册测试
3. 运行验证确认测试被发现

### 扩展报告格式

1. 在 `report_generator.py` 添加生成方法
2. 在 `templates/` 添加模板文件
3. 更新配置支持新格式

### 贡献指南

1. Fork 仓库
2. 创建功能分支: `git checkout -b feature/new-validator`
3. 提交更改: `git commit -am 'Add new validator'`
4. 推送分支: `git push origin feature/new-validator`
5. 创建 Pull Request

### 代码规范

- Python 代码遵循 PEP 8
- C/C++ 代码遵循项目 `.clang-format`
- 所有公共函数需要文档字符串
- 测试覆盖率 >= 80%

## 相关文档

- [测试快速参考](../../docs/TESTING_QUICK_REFERENCE.md)
- [覆盖率设置指南](../../docs/COVERAGE_SETUP.md)
- [CI 集成文档](CI_INTEGRATION.md)
- [模板文档](TEMPLATES_README.md)
- [故障排除指南](TROUBLESHOOTING.md)

## 许可证

Copyright (c) 2026 Nexus Team. All rights reserved.

## 支持

如有问题或建议，请：
- 提交 Issue: https://github.com/your-org/nexus/issues
- 查看文档: https://nexus.readthedocs.io
- 联系团队: nexus-team@example.com
