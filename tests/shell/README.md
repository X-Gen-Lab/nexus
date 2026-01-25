# Shell Framework 测试套件

本目录包含 Nexus Shell Framework 的完整测试套件。

## 📋 概述

Shell Framework 测试套件包含 **300 个测试**，覆盖所有核心功能、边界条件和错误处理路径。

### 测试统计

| 指标 | 值 |
|------|---|
| **测试套件数** | 16 |
| **总测试数** | 300 |
| **单元测试** | 230 |
| **属性测试** | 70 |
| **通过率** | 100% ✅ |
| **代码覆盖率** | ≥ 93% |

## 🗂️ 测试文件结构

### 单元测试

```
tests/shell/
├── test_shell_core.cpp              # Shell 核心功能测试 (45 tests)
├── test_shell_command.cpp           # 命令管理测试 (28 tests)
├── test_shell_line_editor.cpp       # 行编辑器测试 (38 tests)
├── test_shell_history.cpp           # 历史管理测试 (32 tests)
├── test_shell_parser.cpp            # 命令解析器测试 (18 tests)
├── test_shell_autocomplete.cpp      # 自动补全测试 (20 tests)
├── test_shell_backend.cpp           # 后端抽象测试 (18 tests)
└── test_shell_builtin.cpp           # 内置命令测试 (15 tests)
```

### 属性测试

```
tests/shell/
├── test_shell_core_properties.cpp           # 核心属性测试 (10 tests)
├── test_shell_command_properties.cpp        # 命令属性测试 (10 tests)
├── test_shell_line_editor_properties.cpp    # 编辑器属性测试 (10 tests)
├── test_shell_history_properties.cpp        # 历史属性测试 (10 tests)
├── test_shell_parser_properties.cpp         # 解析器属性测试 (10 tests)
├── test_shell_autocomplete_properties.cpp   # 补全属性测试 (10 tests)
└── test_shell_backend_properties.cpp        # 后端属性测试 (10 tests)
```

### 配置和文档

```
tests/shell/
├── CMakeLists.txt                   # CMake 构建配置
├── README.md                        # 本文件
├── TEST_EXECUTION_REPORT.md         # 详细测试执行报告
└── TEST_SUCCESS_SUMMARY.md          # 测试成功总结
```

## 🚀 运行测试

### 使用 CMake 和 CTest

```bash
# 构建测试
cd build
cmake --build . --config Debug --target shell_tests

# 运行所有 Shell 测试
ctest -R shell --output-on-failure

# 运行特定测试套件
ctest -R ShellCoreTest --output-on-failure
```

### 直接运行测试可执行文件

```bash
# Windows
cd build/tests/shell/Debug
./shell_tests.exe

# Linux/macOS
cd build/tests/shell
./shell_tests

# 使用 Google Test 过滤器
./shell_tests --gtest_filter=ShellCoreTest.*
./shell_tests --gtest_filter=*Init*

# 简洁输出
./shell_tests --gtest_brief=1

# 详细输出
./shell_tests --gtest_verbose=1
```

### 生成覆盖率报告

```bash
# 使用 Coverage 构建类型
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage

# 查看覆盖率报告
open coverage/index.html
```

## 📊 测试覆盖范围

### 1. Shell Core Tests (test_shell_core.cpp)

**测试数量**: 45

**覆盖功能**:
- 初始化与反初始化
- 配置参数验证（提示符、缓冲区大小、历史深度）
- 输入处理（字符、转义序列、控制字符）
- 命令执行流程
- 错误处理与恢复
- 版本信息

**关键测试**:
- `InitWithValidConfig` - 正常初始化
- `InitWithNullConfig` - NULL 参数验证
- `ProcessWithoutBackendReturnsError` - 后端检查
- `ExecuteRegisteredCommand` - 命令执行
- `RecoverResetsState` - 错误恢复

### 2. Command Management Tests (test_shell_command.cpp)

**测试数量**: 28

**覆盖功能**:
- 命令注册与验证
- 重复命令检测
- 容量限制（最大 32 个命令）
- 命令注销
- 命令查找
- 命令列表获取
- 补全回调管理

**关键测试**:
- `RegisterValidCommand` - 注册命令
- `RegisterDuplicateNameRejected` - 重复检测
- `RegisterUpToMaxCommands` - 容量测试
- `UnregisterValidCommand` - 注销命令

### 3. Line Editor Tests (test_shell_line_editor.cpp)

**测试数量**: 38

**覆盖功能**:
- 字符插入（行首、行中、行尾）
- Backspace 删除
- Delete 键删除
- 光标移动（左右箭头）
- Home/End 键
- Ctrl+K（删除到行尾）
- Ctrl+U（删除到行首）
- Ctrl+W（删除单词）
- 缓冲区管理

**关键测试**:
- `InsertAtMiddle` - 中间插入
- `BackspaceAtEnd` - 退格删除
- `DeleteAtStart` - Delete 删除
- `MoveCursorLeft` - 光标移动
- `DeleteToEndFromMiddle` - Ctrl+K

### 4. History Manager Tests (test_shell_history.cpp)

**测试数量**: 32

**覆盖功能**:
- 命令添加
- 重复命令去重
- FIFO 容量管理（4-32 条）
- 历史浏览（上下箭头）
- 浏览状态管理
- 历史清除

**关键测试**:
- `AddDuplicateConsecutiveRejected` - 去重
- `FIFORemovesOldest` - FIFO 行为
- `GetPrevMultipleEntries` - 向后浏览
- `GetNextAfterPrev` - 向前浏览

### 5. Parser Tests (test_shell_parser.cpp)

**测试数量**: 18

**覆盖功能**:
- 基本命令解析
- 参数分隔（空格、Tab）
- 引号字符串（单引号、双引号）
- 转义字符
- 边界条件（空行、最大参数）

**关键测试**:
- `ParseSimpleCommand` - 简单解析
- `ParseDoubleQuotedString` - 引号处理
- `ParseMaxArgs` - 最大参数
- `ParseTooManyArgs` - 溢出检测

### 6. Auto-Completion Tests (test_shell_autocomplete.cpp)

**测试数量**: 20

**覆盖功能**:
- 唯一匹配补全
- 多匹配显示
- 公共前缀计算
- 无匹配处理
- Tab 键处理

**关键测试**:
- `UniqueMatchCompletion` - 唯一匹配
- `MultipleMatchCompletion` - 多匹配
- `CommonPrefixCalculation` - 前缀计算

### 7. Backend Tests (test_shell_backend.cpp)

**测试数量**: 18

**覆盖功能**:
- 后端设置与获取
- Printf 格式化输出
- Write 二进制写入
- Putchar 字符输出
- Puts 字符串输出

**关键测试**:
- `SetBackendWithValidBackend` - 设置后端
- `PrintfWithBackend` - 格式化输出
- `WriteWithBackend` - 二进制写入

### 8. Mock Backend Tests (test_shell_backend.cpp)

**测试数量**: 16

**覆盖功能**:
- 输入注入
- 输出捕获
- 缓冲区管理
- 重置功能

**关键测试**:
- `InjectInputData` - 注入输入
- `ReadInjectedData` - 读取数据
- `WriteAndGetOutput` - 捕获输出

### 9. Built-in Commands Tests (test_shell_builtin.cpp)

**测试数量**: 15

**覆盖功能**:
- help 命令（列表与详情）
- version 命令
- clear 命令（ANSI 转义序列）
- history 命令
- echo 命令

**关键测试**:
- `HelpListsAllCommands` - 列出命令
- `VersionShowsVersion` - 显示版本
- `ClearSendsEscapeSequence` - 清屏

### 10-16. Property-Based Tests

**测试数量**: 70 (每个模块 10 个)

**覆盖功能**:
- 随机输入生成
- 不变量验证
- 状态一致性检查
- 边界条件探索

## 🎯 测试策略

### 测试金字塔

```
        /\
       /  \      E2E Tests (手动)
      /____\
     /      \    Integration Tests (自动化)
    /________\
   /          \  Unit Tests (自动化)
  /____________\
```

### 测试类型

1. **单元测试** (230 个)
   - 测试单个函数或模块
   - 快速执行（< 1ms/test）
   - 高覆盖率（≥ 90%）

2. **属性测试** (70 个)
   - 基于属性的测试
   - 随机输入生成
   - 不变量验证

3. **集成测试** (包含在单元测试中)
   - 测试模块间交互
   - 完整命令执行流程
   - 后端集成

## 📈 代码覆盖率目标

| 模块 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
|------|---------|-----------|-----------|
| shell.c | ≥ 95% | ≥ 90% | 100% |
| shell_command.c | ≥ 95% | ≥ 90% | 100% |
| shell_line_editor.c | ≥ 95% | ≥ 92% | 100% |
| shell_history.c | ≥ 95% | ≥ 90% | 100% |
| shell_parser.c | ≥ 92% | ≥ 88% | 100% |
| shell_autocomplete.c | ≥ 90% | ≥ 85% | 100% |
| shell_backend.c | ≥ 95% | ≥ 90% | 100% |
| shell_builtin.c | ≥ 90% | ≥ 85% | 100% |
| **总计** | **≥ 93%** | **≥ 89%** | **100%** |

## 🔧 测试工具

### Google Test Framework

使用 Google Test 作为测试框架：

```cpp
#include <gtest/gtest.h>

TEST(TestSuiteName, TestName) {
    EXPECT_EQ(expected, actual);
    ASSERT_NE(nullptr, pointer);
}
```

### Mock Backend

使用自定义 Mock 后端进行测试：

```cpp
MockBackend::set_input("test command\r");
shell_process();
std::string output = MockBackend::get_output();
EXPECT_NE(std::string::npos, output.find("expected"));
```

## 📝 编写新测试

### 测试命名规范

```cpp
TEST_F(TestFixture, MethodName_StateUnderTest_ExpectedBehavior) {
    // Arrange
    // Act
    // Assert
}
```

### 示例

```cpp
TEST_F(ShellCoreTest, Init_WithValidConfig_ReturnsSuccess) {
    /* Arrange */
    shell_config_t config = get_default_config();
    
    /* Act */
    shell_status_t status = shell_init(&config);
    
    /* Assert */
    EXPECT_EQ(SHELL_OK, status);
    EXPECT_TRUE(shell_is_initialized());
}
```

## 🐛 调试测试

### 运行单个测试

```bash
./shell_tests --gtest_filter=ShellCoreTest.InitWithValidConfig
```

### 重复运行测试

```bash
./shell_tests --gtest_repeat=100 --gtest_break_on_failure
```

### 详细输出

```bash
./shell_tests --gtest_verbose=1
```

## 📚 相关文档

- [TEST_GUIDE.md](../../framework/shell/docs/TEST_GUIDE.md) - 详细测试指南
- [TEST_EXECUTION_REPORT.md](TEST_EXECUTION_REPORT.md) - 测试执行报告
- [TEST_SUCCESS_SUMMARY.md](TEST_SUCCESS_SUMMARY.md) - 测试成功总结
- [DESIGN.md](../../framework/shell/docs/DESIGN.md) - 架构设计文档

## ✅ 测试检查清单

在提交代码前，确保：

- [ ] 所有测试通过
- [ ] 新功能有对应测试
- [ ] 代码覆盖率 ≥ 90%
- [ ] 测试执行时间 < 100ms
- [ ] 无内存泄漏
- [ ] 遵循测试命名规范
- [ ] 测试独立且可重复

## 🎓 最佳实践

1. **测试独立性** - 每个测试独立运行
2. **快速执行** - 单个测试 < 1ms
3. **清晰命名** - 测试名称描述测试内容
4. **AAA 模式** - Arrange, Act, Assert
5. **一个断言** - 每个测试关注一个行为
6. **Mock 隔离** - 使用 Mock 隔离外部依赖
7. **边界测试** - 测试边界条件和错误路径

---

**维护者**: Nexus Team  
**最后更新**: 2026-01-24  
**测试状态**: ✅ 全部通过 (300/300)
