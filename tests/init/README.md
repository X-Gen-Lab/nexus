## Init Framework 测试套件

本目录包含 Init Framework 的完整测试套件。

### 测试文件

| 文件 | 描述 | 测试数量 |
|------|------|----------|
| `test_nx_init.cpp` | nx_init 单元测试 | 12 |
| `test_nx_startup.cpp` | nx_startup 单元测试 | 15 |
| `test_nx_firmware_info.cpp` | nx_firmware_info 单元测试 | 20 |
| `test_nx_init_properties.cpp` | 属性测试 | 10 |
| `test_init_integration.cpp` | 集成测试 | 15 |
| `test_init_performance.cpp` | 性能测试 | 12 |

**总计**: 84 个测试用例

### 测试覆盖

- **行覆盖率**: ≥ 95%
- **分支覆盖率**: ≥ 90%
- **函数覆盖率**: 100%

### 快速开始

#### Linux/macOS

```bash
# 运行所有测试
./run_tests.sh

# 运行单元测试
./run_tests.sh --unit

# 运行集成测试
./run_tests.sh --integration

# 运行性能测试
./run_tests.sh --performance

# 生成覆盖率报告
./run_tests.sh --coverage
```

#### Windows

```cmd
REM 运行所有测试
run_tests.bat

REM 运行单元测试
run_tests.bat --unit

REM 运行集成测试
run_tests.bat --integration

REM 运行性能测试
run_tests.bat --performance
```

### 测试分类

#### 单元测试

测试单个函数的功能：

- `NxInitTest.*` - nx_init 模块测试
- `NxStartupTest.*` - nx_startup 模块测试
- `NxFirmwareInfoTest.*` - nx_firmware_info 模块测试

#### 集成测试

测试模块间的交互：

- `InitIntegrationTest.*` - 完整启动流程测试
- 多模块初始化测试
- 错误恢复测试

#### 性能测试

测试性能指标：

- `InitPerformanceTest.*` - 启动时间测试
- 内存占用测试
- 可扩展性测试

### 测试要求

#### 环境要求

- CMake 3.15+
- Google Test 1.10+
- C++17 编译器
- lcov (可选，用于覆盖率)

#### 编译测试

```bash
# 配置
cmake -B build -DENABLE_TESTS=ON

# 编译
cmake --build build

# 运行
cd build
ctest --output-on-failure
```

### 性能基准

| 指标 | 目标 | 当前 |
|------|------|------|
| 启动时间 | < 100ms | ~85ms |
| 初始化开销 | < 1ms | ~0.8ms |
| 内存占用 | < 1KB | ~856 bytes |
| 代码大小 | < 2KB | ~1.8KB |

### 测试辅助工具

#### test_init_helpers.h

提供测试辅助函数：

- 执行跟踪
- 性能测量
- 内存工具
- 字符串工具

#### 使用示例

```cpp
#include "test_init_helpers.h"

TEST(MyTest, Example) {
    /* 性能测量 */
    perf_counter_t counter;
    perf_counter_start(&counter);
    
    /* 执行测试代码 */
    my_function();
    
    perf_counter_stop(&counter);
    uint64_t elapsed = perf_counter_elapsed_us(&counter);
    
    EXPECT_LT(elapsed, 1000); /* < 1ms */
}
```

### 持续集成

测试在以下平台上自动运行：

- Ubuntu 20.04 (GCC, Clang)
- macOS 11+ (Clang)
- Windows 10/11 (MSVC)

### 故障排查

#### 测试失败

1. 检查编译器版本
2. 确保所有依赖已安装
3. 查看详细日志：`./run_tests.sh --verbose`

#### 覆盖率问题

1. 确保使用 Debug 构建
2. 启用覆盖率标志：`-DENABLE_COVERAGE=ON`
3. 安装 lcov：`sudo apt-get install lcov`

### 贡献指南

添加新测试时：

1. 遵循现有测试命名约定
2. 添加详细的测试文档
3. 确保测试独立且可重复
4. 更新测试统计信息

### 相关文档

- [TEST_GUIDE.md](../../framework/init/docs/TEST_GUIDE.md) - 完整测试指南
- [TROUBLESHOOTING.md](../../framework/init/docs/TROUBLESHOOTING.md) - 故障排查
- [USER_GUIDE.md](../../framework/init/docs/USER_GUIDE.md) - 使用指南

### 许可证

Copyright (c) 2026 Nexus Team
