# Config Manager Tests

本目录包含 Config Manager 模块的完整测试套件。

## 测试结构

```
tests/config/
├── test_config_store.cpp              # 存储和基本操作测试
├── test_config_namespace.cpp          # 命名空间测试
├── test_config_callback.cpp           # 回调测试
├── test_config_default.cpp            # 默认值测试
├── test_config_backend.cpp            # 后端测试
├── test_config_import_export.cpp      # 导入/导出测试
├── test_config_crypto.cpp             # 加密测试
├── test_config_query.cpp              # 查询测试
├── test_config_integration.cpp        # 集成测试
├── test_config_performance.cpp        # 性能测试
├── test_config_thread_safety.cpp      # 线程安全测试
├── test_config_*_properties.cpp       # 属性测试
├── test_config_helpers.h              # 测试辅助函数
├── CMakeLists.txt                     # 构建配置
├── run_tests.sh                       # Linux/Mac 测试脚本
├── run_tests.bat                      # Windows 测试脚本
└── README.md                          # 本文件
```

## 测试类型

### 1. 单元测试 (Unit Tests)

测试单个功能模块：

- **test_config_store.cpp**: 初始化、基本 CRUD 操作、类型测试
- **test_config_namespace.cpp**: 命名空间隔离、操作
- **test_config_callback.cpp**: 回调注册、触发、管理
- **test_config_default.cpp**: 默认值注册、重置
- **test_config_backend.cpp**: 后端接口、RAM/Flash 后端
- **test_config_import_export.cpp**: JSON/二进制导入导出
- **test_config_crypto.cpp**: 加密存储、密钥轮换
- **test_config_query.cpp**: 查询、遍历、统计

### 2. 集成测试 (Integration Tests)

测试多个模块协同工作：

- **test_config_integration.cpp**: 
  - 命名空间 + 回调
  - 命名空间 + 默认值
  - 默认值 + 回调
  - 持久化集成
  - 复杂场景测试

### 3. 性能测试 (Performance Tests)

测试性能指标：

- **test_config_performance.cpp**:
  - Set/Get 操作基准测试
  - Commit 性能测试
  - 内存使用测试
  - 压力测试
  - 回调开销测试
  - 命名空间开销测试
  - 搜索性能测试

### 4. 线程安全测试 (Thread Safety Tests)

测试多线程环境：

- **test_config_thread_safety.cpp**:
  - 并发读写测试
  - 并发命名空间操作
  - 并发回调触发
  - 并发删除和创建
  - 死锁预防测试
  - 数据竞争检测

### 5. 属性测试 (Property-Based Tests)

使用随机输入测试通用属性：

- **test_config_*_properties.cpp**: 各模块的属性测试

## 快速开始

### Linux/Mac

```bash
# 运行所有测试
./run_tests.sh

# 运行特定测试
./run_tests.sh -f "ConfigStore*"

# 生成覆盖率报告
./run_tests.sh -c

# 使用 Valgrind 检测内存泄漏
./run_tests.sh -v

# 启用 Sanitizers
./run_tests.sh -s

# 详细输出
./run_tests.sh -V
```

### Windows

```cmd
REM 运行所有测试
run_tests.bat

REM 运行特定测试
run_tests.bat -f "ConfigStore*"

REM 生成覆盖率报告
run_tests.bat -c

REM 详细输出
run_tests.bat -V
```

### 手动运行

```bash
# 构建
cd build
cmake ..
cmake --build . --target config_tests

# 运行
./tests/config_tests

# 运行特定测试
./tests/config_tests --gtest_filter="ConfigStore*"

# 列出所有测试
./tests/config_tests --gtest_list_tests
```

## 测试覆盖率

### 覆盖率目标

- **行覆盖率**: ≥ 90%
- **分支覆盖率**: ≥ 85%
- **函数覆盖率**: 100%

### 生成覆盖率报告

#### Linux/Mac

```bash
# 使用脚本
./run_tests.sh -c

# 手动生成
cd build
cmake -DENABLE_COVERAGE=ON ..
cmake --build . --target config_tests
./tests/config_tests
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html
```

#### Windows

```cmd
REM 使用 OpenCppCoverage
run_tests.bat -c

REM 或手动运行
OpenCppCoverage --sources framework\config --export_type html:coverage_html -- tests\Debug\config_tests.exe
```

## 性能基准

### 预期性能指标

| 操作 | 目标性能 |
|------|---------|
| Set I32 | > 10,000 ops/sec |
| Get I32 | > 40,000 ops/sec |
| Set String | > 8,000 ops/sec |
| Get String | > 30,000 ops/sec |
| Commit (50 keys) | < 50 ms |
| 回调开销 | < 0.01 ms/call |
| 命名空间开销 | < 0.005 ms/call |

### 运行性能测试

```bash
# 只运行性能测试
./run_tests.sh -f "*Performance*"

# 详细输出查看性能数据
./run_tests.sh -f "*Performance*" -V
```

## 内存检测

### Valgrind (Linux/Mac)

```bash
# 使用脚本
./run_tests.sh -v

# 手动运行
valgrind --leak-check=full --show-leak-kinds=all ./tests/config_tests
```

### AddressSanitizer

```bash
# 使用脚本
./run_tests.sh -s

# 手动构建
cd build
cmake -DENABLE_SANITIZERS=ON ..
cmake --build . --target config_tests
./tests/config_tests
```

## 调试测试

### 运行单个测试

```bash
./tests/config_tests --gtest_filter="ConfigStoreTest.SetGetI32"
```

### 重复运行测试

```bash
# 运行 100 次以检测间歇性问题
./tests/config_tests --gtest_repeat=100
```

### 打乱测试顺序

```bash
# 检测测试间依赖
./tests/config_tests --gtest_shuffle
```

### 调试器中运行

```bash
# GDB
gdb --args ./tests/config_tests --gtest_filter="ConfigStoreTest.SetGetI32"

# LLDB
lldb -- ./tests/config_tests --gtest_filter="ConfigStoreTest.SetGetI32"
```

## 持续集成

### GitHub Actions 示例

```yaml
name: Config Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y lcov valgrind
      
      - name: Run tests with coverage
        run: |
          cd tests/config
          ./run_tests.sh -c
      
      - name: Upload coverage
        uses: codecov/codecov-action@v2
        with:
          files: ./build/coverage.info
```

## 故障排查

### 测试失败

1. 查看详细输出：`./run_tests.sh -V`
2. 运行单个失败的测试：`./tests/config_tests --gtest_filter="FailedTest"`
3. 检查是否有资源泄漏：`./run_tests.sh -v`

### 性能测试失败

1. 确保系统负载较低
2. 关闭其他应用程序
3. 多次运行以获得稳定结果
4. 检查是否启用了优化（Release 构建）

### 线程安全测试失败

1. 使用 ThreadSanitizer：`cmake -DENABLE_THREAD_SANITIZER=ON ..`
2. 增加迭代次数以暴露竞态条件
3. 使用 Helgrind：`valgrind --tool=helgrind ./tests/config_tests`

## 贡献指南

### 添加新测试

1. 在适当的测试文件中添加测试用例
2. 使用 `test_config_helpers.h` 中的辅助函数
3. 遵循现有的命名约定
4. 添加注释说明测试目的
5. 运行所有测试确保没有破坏现有功能

### 测试命名约定

```cpp
TEST_F(ConfigStoreTest, SetGetI32)  // 功能描述
TEST_F(ConfigStoreTest, SetI32NullKey)  // 错误情况
TEST_F(ConfigStoreTest, I32MinMax)  // 边界条件
```

### 代码风格

- 遵循 Nexus 代码注释规范
- 使用 Doxygen 风格注释
- 保持测试简洁明了
- 每个测试只测试一个功能点

## 参考文档

- [TEST_GUIDE.md](../../framework/config/docs/TEST_GUIDE.md) - 完整测试指南
- [USER_GUIDE.md](../../framework/config/docs/USER_GUIDE.md) - 使用指南
- [DESIGN.md](../../framework/config/docs/DESIGN.md) - 架构设计
- [TROUBLESHOOTING.md](../../framework/config/docs/TROUBLESHOOTING.md) - 故障排查

## 许可证

Copyright (c) 2026 Nexus Team
