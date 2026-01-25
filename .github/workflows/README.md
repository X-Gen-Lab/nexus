# Nexus CI/CD Workflows

本目录包含 Nexus 项目的所有 GitHub Actions 工作流配置。

## 工作流概览

### 核心工作流

#### 1. Build Workflow (`build.yml`)
**触发条件**: Push 到 main/develop 分支，Pull Request

**功能**:
- 多平台原生构建 (Ubuntu, Windows, macOS)
- 多编译器支持 (GCC, Clang, MSVC)
- ARM 交叉编译 (STM32F4, STM32H7)
- 静态代码分析 (cppcheck, clang-format, clang-tidy)
- 文档构建检查

**优化特性**:
- 使用缓存加速构建 (CMake 构建缓存, ARM 工具链缓存)
- 并行构建和测试
- 构建报告生成
- 失败时继续其他任务 (fail-fast: false)

#### 2. Test Workflow (`test.yml`)
**触发条件**: Push 到 main/develop 分支，Pull Request

**功能**:
- 单元测试执行
- 代码覆盖率分析 (lcov)
- 覆盖率阈值检查 (最低 95%, 目标 100%)
- 多种 Sanitizer 测试 (Address, Undefined, Thread)
- MISRA C 合规性检查
- 覆盖率对比 (PR vs 基础分支)

**覆盖率报告**:
- 自动上传到 Codecov
- PR 评论显示覆盖率变化
- 详细的 HTML 报告

#### 3. Documentation Workflow (`docs.yml`)
**触发条件**: Push 到 main 分支，手动触发

**功能**:
- Doxygen API 文档生成
- Sphinx 用户文档构建 (英文/中文)
- 自动部署到 GitHub Pages
- 多语言支持

**部署地址**:
- 主页: https://nexus-platform.github.io/nexus/
- 英文文档: https://nexus-platform.github.io/nexus/en/
- 中文文档: https://nexus-platform.github.io/nexus/zh_CN/
- API 文档: https://nexus-platform.github.io/nexus/api/

#### 4. Validation Workflow (`validation.yml`)
**触发条件**: Push 到 main/develop 分支，Pull Request，手动触发

**功能**:
- 综合验证测试 (单元测试 + 属性测试 + 集成测试)
- 多平台验证矩阵
- 覆盖率分析 (仅 Ubuntu Debug)
- 验证报告生成

### 增强工作流

#### 5. Release Workflow (`release.yml`)
**触发条件**: 推送版本标签 (v*.*.*), 手动触发

**功能**:
- 自动创建 GitHub Release
- 从 CHANGELOG.md 提取发布说明
- 构建多平台发布包 (STM32F4, STM32H7)
- 生成代码大小报告
- 打包文档
- 上传发布资产

**使用方法**:
```bash
# 创建版本标签
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0

# 或手动触发
# GitHub Actions -> Release -> Run workflow
```

#### 6. Security Workflow (`security.yml`)
**触发条件**: Push, Pull Request, 每周日定时, 手动触发

**功能**:
- Python 依赖安全扫描 (Safety)
- CodeQL 代码安全分析 (C/C++, Python)
- 密钥泄露扫描 (TruffleHog)
- 许可证合规性检查

**安全报告**:
- 自动创建安全问题
- 上传 SARIF 报告到 GitHub Security

#### 7. Performance Workflow (`performance.yml`)
**触发条件**: Push, Pull Request, 每周一定时, 手动触发

**功能**:
- 性能基准测试
- 内存分析 (Valgrind)
- 代码大小分析 (ARM 平台)
- 性能回归检测

**报告内容**:
- 性能指标对比
- 内存泄漏检测
- 代码大小变化

## 工作流依赖关系

```
build.yml
├── build-native (多平台)
├── build-arm (STM32F4, STM32H7)
├── static-analysis
├── docs-check
└── build-summary

test.yml
├── test (覆盖率)
├── sanitizer (Address, Undefined, Thread)
├── misra
└── test-summary

validation.yml
├── validate (多平台矩阵)
└── validation-summary

release.yml
├── create-release
├── build-release (多平台)
└── build-docs

security.yml
├── dependency-scan
├── codeql (C/C++, Python)
├── secret-scan
└── license-check

performance.yml
├── benchmark
├── memory-profile
└── size-analysis
```

## 缓存策略

### 构建缓存
- **CMake 构建**: `${{ runner.os }}-cmake-${{ hashFiles('**/CMakeLists.txt') }}`
- **ARM 工具链**: `arm-none-eabi-gcc-10.3`
- **Python 包**: 使用 `actions/setup-python` 的 `cache: 'pip'`

### 工具缓存
- **分析工具**: `${{ runner.os }}-analysis-tools-v1`
- **文档工具**: `${{ runner.os }}-docs-tools-v1`

## 环境变量

### 全局环境变量
```yaml
BUILD_TYPE: Release          # 构建类型
PYTHON_VERSION: '3.11'       # Python 版本
MIN_COVERAGE: 95.0           # 最低覆盖率
TARGET_COVERAGE: 100.0       # 目标覆盖率
```

### Sanitizer 选项
```yaml
ASAN_OPTIONS: detect_leaks=1:check_initialization_order=1
UBSAN_OPTIONS: print_stacktrace=1
TSAN_OPTIONS: second_deadlock_stack=1
```

## 工件保留

| 工件名称 | 保留时间 | 内容 |
|---------|---------|------|
| `*-binaries` | 30 天 | 编译后的二进制文件 (.bin, .hex, .elf, .map) |
| `coverage-report` | 30 天 | 覆盖率报告 (HTML, lcov) |
| `static-analysis-reports` | 30 天 | 静态分析报告 |
| `documentation-preview` | 7 天 | 文档预览 |
| `test-results-*` | 30 天 | 测试结果 |
| `performance-report` | 永久 | 性能报告 |
| `security-reports` | 永久 | 安全扫描报告 |

## 状态徽章

在 README.md 中使用以下徽章:

```markdown
[![Build Status](https://github.com/nexus-platform/nexus/workflows/Build/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![Test Status](https://github.com/nexus-platform/nexus/workflows/Test/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![Documentation](https://github.com/nexus-platform/nexus/workflows/Documentation/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![Security](https://github.com/nexus-platform/nexus/workflows/Security/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![codecov](https://codecov.io/gh/nexus-platform/nexus/branch/main/graph/badge.svg)](https://codecov.io/gh/nexus-platform/nexus)
```

## 配置要求

### GitHub Secrets
需要配置以下 secrets:

- `CODECOV_TOKEN`: Codecov 上传令牌 (可选，公开仓库不需要)
- `GITHUB_TOKEN`: 自动提供，用于创建 Release 和评论

### GitHub Pages
需要在仓库设置中启用 GitHub Pages:
1. Settings -> Pages
2. Source: GitHub Actions
3. 保存

### 权限设置
工作流需要以下权限:
```yaml
permissions:
  contents: read        # 读取代码
  pages: write          # 部署 Pages
  id-token: write       # Pages 部署认证
  security-events: write # 安全扫描
  pull-requests: write  # PR 评论
```

## 本地测试

### 使用 act 本地运行工作流
```bash
# 安装 act
# https://github.com/nektos/act

# 运行构建工作流
act -j build-native

# 运行测试工作流
act -j test

# 使用特定平台
act -P ubuntu-latest=ghcr.io/catthehacker/ubuntu:act-latest
```

### 手动运行脚本
```bash
# 构建
python scripts/building/build.py

# 测试
python scripts/test/test.py

# 覆盖率
cd scripts/coverage
./run_coverage_linux.sh  # Linux
.\run_coverage_windows.ps1  # Windows

# 验证
python scripts/validation/validate.py --build-dir build --coverage
```

## 故障排查

### 常见问题

#### 1. 覆盖率生成失败
**原因**: lcov 版本不兼容或 gcov 数据不匹配

**解决方案**:
```bash
# 清理构建目录
rm -rf build
# 重新配置并构建
cmake -B build -DNEXUS_ENABLE_COVERAGE=ON
cmake --build build
```

#### 2. ARM 工具链安装失败
**原因**: apt 源中的工具链版本过旧

**解决方案**:
```yaml
- name: Install ARM Toolchain
  run: |
    wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
    tar -xjf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
    echo "$PWD/gcc-arm-none-eabi-10.3-2021.10/bin" >> $GITHUB_PATH
```

#### 3. 文档构建失败
**原因**: Sphinx 扩展缺失

**解决方案**:
```bash
pip install sphinx sphinx-intl breathe sphinx-rtd-theme
```

#### 4. 缓存未命中
**原因**: 缓存键不匹配

**解决方案**:
- 检查 `hashFiles()` 路径是否正确
- 使用 `restore-keys` 提供回退选项
- 清理旧缓存 (Settings -> Actions -> Caches)

## 性能优化建议

### 1. 并行化
- 使用 `--parallel` 或 `-j` 选项
- 矩阵策略并行运行多个配置

### 2. 缓存
- 缓存依赖项 (pip, apt)
- 缓存构建产物
- 缓存工具链

### 3. 条件执行
- 使用 `paths` 过滤器只在相关文件变化时运行
- 使用 `if` 条件跳过不必要的步骤

### 4. 增量构建
- 保留构建目录
- 使用 ccache 加速编译

## 维护指南

### 定期更新
- 每月检查 Actions 版本更新
- 更新工具链版本
- 更新 Python 依赖

### 监控
- 检查工作流运行时间
- 监控缓存命中率
- 审查失败日志

### 文档
- 更新工作流变更到本文档
- 记录新增的环境变量
- 更新故障排查指南

## 参考资源

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [CMake 文档](https://cmake.org/documentation/)
- [Codecov 文档](https://docs.codecov.com/)
- [Doxygen 文档](https://www.doxygen.nl/manual/)
- [Sphinx 文档](https://www.sphinx-doc.org/)
