# Native 平台 HAL 测试指南

## 概述

本指南提供了 Nexus Native 平台 HAL 测试的完整说明，包括如何运行测试、添加新测试、使用测试辅助函数以及生成覆盖率报告。

Native 平台是一个在 PC 上运行的模拟硬件环境，用于开发和测试 HAL 实现。完整的测试覆盖对于确保 HAL 接口的正确实现至关重要。

## 目录

1. [快速开始](#快速开始)
2. [运行测试](#运行测试)
3. [添加新测试](#添加新测试)
4. [使用测试辅助函数](#使用测试辅助函数)
5. [生成覆盖率报告](#生成覆盖率报告)
6. [测试组织结构](#测试组织结构)
7. [最佳实践](#最佳实践)
8. [故障排查](#故障排查)

---

## 快速开始

### 前置条件

**Linux/WSL (推荐)**:
- GCC 或 Clang 编译器
- CMake 3.15+
- GoogleTest (自动下载)
- lcov 或 gcovr (用于覆盖率分析)

**Windows**:
- MSVC 编译器
- CMake 3.15+
- GoogleTest (自动下载)
- OpenCppCoverage (用于覆盖率分析)

**注意**: Native 平台在 MSVC 上存在设备注册问题，推荐使用 Linux/WSL 进行测试。

### 构建和运行测试

```bash
# 1. 配置项目（启用测试）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DNEXUS_PLATFORM=native \
  -DNEXUS_BUILD_TESTS=ON

# 2. 编译
cmake --build build --config Debug

# 3. 运行所有测试
cd build && ctest --output-on-failure

# 或直接运行测试可执行文件
./build/tests/nexus_tests
```


---

## 运行测试

### 运行所有测试

```bash
# 使用 CTest
cd build
ctest --output-on-failure

# 或直接运行测试可执行文件
./tests/nexus_tests

# Windows
.\tests\Debug\nexus_tests.exe
```

### 运行特定测试套件

使用 Google Test 的过滤器功能：

```bash
# 运行所有 GPIO 测试
./tests/nexus_tests --gtest_filter="GPIO*"

# 运行所有 UART 测试
./tests/nexus_tests --gtest_filter="UART*"

# 运行所有属性测试
./tests/nexus_tests --gtest_filter="*Property*"

# 运行多个测试套件
./tests/nexus_tests --gtest_filter="GPIO*:UART*:SPI*"
```

### 运行单个测试用例

```bash
# 运行特定测试用例
./tests/nexus_tests --gtest_filter="GPIOTest.BasicInitialization"

# 运行特定属性测试
./tests/nexus_tests --gtest_filter="GPIOPropertyTest.Property1_InitIdempotence"
```

### 测试输出选项

```bash
# 详细输出
./tests/nexus_tests --gtest_verbose

# 彩色输出
./tests/nexus_tests --gtest_color=yes

# 重复运行测试（用于检测不稳定的测试）
./tests/nexus_tests --gtest_repeat=10

# 随机顺序运行测试
./tests/nexus_tests --gtest_shuffle

# 列出所有测试而不运行
./tests/nexus_tests --gtest_list_tests
```


---

## 添加新测试

### 测试文件结构

每个外设应该有两个测试文件：

1. **单元测试**: `test_nx_<peripheral>.cpp` - 验证具体示例和边界条件
2. **属性测试**: `test_nx_<peripheral>_properties.cpp` - 验证通用属性

### 创建单元测试

#### 步骤 1: 创建测试文件

在 `tests/hal/` 目录下创建 `test_nx_<peripheral>.cpp`：

```cpp
/**
 * \file            test_nx_<peripheral>.cpp
 * \brief           <Peripheral> HAL unit tests
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_<peripheral>_helpers.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

class PeripheralTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 重置所有实例 */
        native_<peripheral>_reset_all();
        
        /* 获取外设实例 */
        peripheral = nx_factory_<peripheral>(0);
        ASSERT_NE(nullptr, peripheral);
    }
    
    void TearDown() override {
        /* 清理 */
        if (peripheral != nullptr && peripheral->deinit != nullptr) {
            peripheral->deinit(peripheral);
        }
        native_<peripheral>_reset_all();
    }
    
    nx_<peripheral>_t* peripheral = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests                                                */
/*---------------------------------------------------------------------------*/

TEST_F(PeripheralTest, BasicInitialization) {
    /* 配置 */
    nx_<peripheral>_config_t config = {
        /* 填充配置 */
    };
    
    /* 初始化 */
    ASSERT_EQ(NX_OK, peripheral->init(peripheral, &config));
    
    /* 验证状态 */
    native_<peripheral>_state_t state;
    ASSERT_EQ(NX_OK, native_<peripheral>_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(PeripheralTest, NullPointerHandling) {
    /* 测试空指针参数 */
    EXPECT_NE(NX_OK, peripheral->init(nullptr, nullptr));
}
```


#### 步骤 2: 创建属性测试

在 `tests/hal/` 目录下创建 `test_nx_<peripheral>_properties.cpp`：

```cpp
/**
 * \file            test_nx_<peripheral>_properties.cpp
 * \brief           <Peripheral> HAL property-based tests
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_<peripheral>_helpers.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

class PeripheralPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng.seed(std::random_device{}());
        native_<peripheral>_reset_all();
        peripheral = nx_factory_<peripheral>(0);
        ASSERT_NE(nullptr, peripheral);
    }
    
    void TearDown() override {
        if (peripheral != nullptr && peripheral->deinit != nullptr) {
            peripheral->deinit(peripheral);
        }
        native_<peripheral>_reset_all();
    }
    
    /* 随机数生成器 */
    std::mt19937 rng;
    nx_<peripheral>_t* peripheral = nullptr;
    
    /* 辅助函数：生成随机数据 */
    std::vector<uint8_t> randomData(size_t min_len, size_t max_len) {
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        size_t len = len_dist(rng);
        
        std::vector<uint8_t> data(len);
        std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
        for (auto& byte : data) {
            byte = byte_dist(rng);
        }
        return data;
    }
};

/*---------------------------------------------------------------------------*/
/* Property Tests                                                            */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Init Idempotence
 *
 * *For any* peripheral instance and configuration, multiple initializations
 * with the same configuration should produce the same result state.
 *
 * **Validates: Requirements X.Y**
 */
TEST_F(PeripheralPropertyTest, Property1_InitIdempotence) {
    const int iterations = 100;
    
    for (int i = 0; i < iterations; ++i) {
        /* 生成随机配置 */
        nx_<peripheral>_config_t config = {
            /* 随机配置参数 */
        };
        
        /* 第一次初始化 */
        ASSERT_EQ(NX_OK, peripheral->init(peripheral, &config));
        
        /* 获取状态 */
        native_<peripheral>_state_t state1;
        ASSERT_EQ(NX_OK, native_<peripheral>_get_state(0, &state1));
        
        /* 第二次初始化（相同配置） */
        ASSERT_EQ(NX_OK, peripheral->init(peripheral, &config));
        
        /* 获取状态 */
        native_<peripheral>_state_t state2;
        ASSERT_EQ(NX_OK, native_<peripheral>_get_state(0, &state2));
        
        /* 验证状态相同 */
        EXPECT_EQ(state1.initialized, state2.initialized);
        /* 验证其他状态字段 */
        
        /* 清理 */
        peripheral->deinit(peripheral);
        native_<peripheral>_reset(0);
    }
}
```


#### 步骤 3: 更新 CMakeLists.txt

在 `tests/CMakeLists.txt` 中添加新的测试文件：

```cmake
# 添加测试源文件
target_sources(nexus_tests PRIVATE
    hal/test_nx_<peripheral>.cpp
    hal/test_nx_<peripheral>_properties.cpp
)
```

#### 步骤 4: 编译和运行

```bash
# 重新编译
cmake --build build --config Debug

# 运行新测试
./build/tests/nexus_tests --gtest_filter="<Peripheral>*"
```

### 测试命名约定

- **测试套件名**: `<Peripheral>Test` (单元测试) 或 `<Peripheral>PropertyTest` (属性测试)
- **测试用例名**: 描述性名称，使用 PascalCase
- **属性测试**: `Property<N>_<Description>`

**示例**:
```cpp
TEST_F(GPIOTest, BasicInitialization)
TEST_F(GPIOTest, InterruptTriggering)
TEST_F(GPIOPropertyTest, Property1_InitIdempotence)
TEST_F(GPIOPropertyTest, Property11_ReadWriteConsistency)
```


---

## 使用测试辅助函数

### 测试辅助函数概述

测试辅助函数位于 `tests/hal/native/devices/` 目录下，为每个外设提供：

1. **状态查询**: 访问外设内部状态
2. **数据注入**: 模拟硬件接收数据
3. **数据捕获**: 捕获硬件发送的数据
4. **时间推进**: 模拟时间流逝（定时器外设）
5. **重置功能**: 重置外设到初始状态

### 获取外设实例

**始终使用 `nx_factory_*()` 获取外设实例**：

```cpp
extern "C" {
#include "hal/nx_factory.h"
}

/* ✅ 正确方式 */
nx_uart_t* uart = nx_factory_uart(0);

/* ❌ 错误方式（已废弃） */
// nx_uart_t* uart = nx_uart_native_get(0);
```

### 通用测试辅助函数

所有外设都提供以下通用函数：

#### 1. 获取状态

```cpp
#include "tests/hal/native/devices/native_uart_helpers.h"

native_uart_state_t state;
nx_status_t status = native_uart_get_state(0, &state);

if (status == NX_OK) {
    printf("Initialized: %d\n", state.initialized);
    printf("Baudrate: %u\n", state.baudrate);
    printf("TX Count: %zu\n", state.tx_count);
}
```

#### 2. 重置外设

```cpp
/* 重置单个实例 */
native_uart_reset(0);

/* 重置所有实例 */
native_uart_reset_all();
```

### 通信外设特定函数

通信外设（UART、SPI、I2C、USB、SDIO）提供数据注入和捕获功能：

#### 注入接收数据

```cpp
/* 模拟硬件接收数据 */
uint8_t rx_data[] = {0x01, 0x02, 0x03, 0x04};
native_uart_inject_rx_data(0, rx_data, sizeof(rx_data));

/* 现在可以通过 HAL 接口读取 */
uint8_t buffer[10];
size_t len = uart->read(uart, buffer, sizeof(buffer));
EXPECT_EQ(4, len);
EXPECT_EQ(0x01, buffer[0]);
```

#### 捕获发送数据

```cpp
/* 通过 HAL 接口发送数据 */
uint8_t tx_data[] = {0xAA, 0xBB, 0xCC};
uart->write(uart, tx_data, sizeof(tx_data));

/* 捕获发送的数据 */
uint8_t captured[10];
size_t captured_len = sizeof(captured);
native_uart_get_tx_data(0, captured, &captured_len);

EXPECT_EQ(3, captured_len);
EXPECT_EQ(0xAA, captured[0]);
```


### GPIO 特定函数

#### 模拟引脚变化

```cpp
#include "tests/hal/native/devices/native_gpio_helpers.h"

/* 模拟引脚电平变化（触发中断） */
native_gpio_simulate_pin_change(0, 5, NX_GPIO_LEVEL_HIGH);

/* 检查中断是否被触发 */
bool triggered = native_gpio_is_interrupt_triggered(0, 5);
EXPECT_TRUE(triggered);
```

### Timer 特定函数

#### 推进时间

```cpp
#include "tests/hal/native/devices/native_timer_helpers.h"

/* 推进时间 1000 毫秒 */
native_timer_advance_time(0, 1000);

/* 检查定时器状态 */
native_timer_state_t state;
native_timer_get_state(0, &state);
EXPECT_EQ(1000, state.current_value);
```

### ADC 特定函数

#### 设置模拟输入值

```cpp
#include "tests/hal/native/devices/native_adc_helpers.h"

/* 设置通道 0 的模拟输入值 */
native_adc_set_analog_value(0, 0, 2048);  /* 12-bit 中点值 */

/* 读取 ADC 值 */
uint16_t value;
adc->read(adc, 0, &value);
EXPECT_EQ(2048, value);
```

### 完整示例

#### UART 数据传输测试

```cpp
TEST_F(UARTTest, DataTransmission) {
    /* 初始化 UART */
    nx_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = NX_UART_PARITY_NONE
    };
    ASSERT_EQ(NX_OK, uart->init(uart, &config));
    
    /* 发送数据 */
    uint8_t tx_data[] = "Hello";
    uart->write(uart, tx_data, 5);
    
    /* 捕获发送的数据 */
    uint8_t captured[10];
    size_t captured_len = sizeof(captured);
    native_uart_get_tx_data(0, captured, &captured_len);
    
    /* 验证 */
    EXPECT_EQ(5, captured_len);
    EXPECT_STREQ("Hello", (char*)captured);
    
    /* 注入接收数据 */
    uint8_t rx_data[] = "World";
    native_uart_inject_rx_data(0, rx_data, 5);
    
    /* 读取数据 */
    uint8_t buffer[10];
    size_t len = uart->read(uart, buffer, sizeof(buffer));
    
    /* 验证 */
    EXPECT_EQ(5, len);
    EXPECT_STREQ("World", (char*)buffer);
}
```


---

## 生成覆盖率报告

### Linux/WSL (推荐)

#### 使用 lcov

```bash
# 1. 配置项目（启用覆盖率）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DNEXUS_PLATFORM=native \
  -DNEXUS_BUILD_TESTS=ON \
  -DNEXUS_ENABLE_COVERAGE=ON

# 2. 编译
cmake --build build --config Debug

# 3. 运行覆盖率脚本
cd scripts/coverage
./run_coverage_linux.sh

# 4. 查看报告
xdg-open ../../coverage_html/index.html
```

#### 使用 gcovr

```bash
# 运行覆盖率脚本（使用 gcovr）
cd scripts/coverage
./run_coverage_linux.sh -t gcovr

# 查看报告
xdg-open ../../coverage_html/index.html
```

#### 手动生成覆盖率

```bash
# 1. 配置和编译（启用覆盖率）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DNEXUS_PLATFORM=native \
  -DNEXUS_BUILD_TESTS=ON \
  -DNEXUS_ENABLE_COVERAGE=ON
cmake --build build --config Debug

# 2. 运行测试
cd build
ctest --output-on-failure

# 3. 生成覆盖率数据
lcov --capture --directory . --output-file coverage.info

# 4. 过滤不需要的文件
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --remove coverage.info '*/ext/*' --output-file coverage.info
lcov --remove coverage.info '*/tests/*' --output-file coverage.info

# 5. 生成 HTML 报告
genhtml coverage.info --output-directory coverage_html

# 6. 查看报告
xdg-open coverage_html/index.html
```

### Windows

#### 使用 OpenCppCoverage

```powershell
# 1. 安装 OpenCppCoverage
# 从 https://github.com/OpenCppCoverage/OpenCppCoverage/releases 下载并安装

# 2. 配置和编译
cmake -B build -DCMAKE_BUILD_TYPE=Debug `
  -DNEXUS_PLATFORM=native `
  -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Debug

# 3. 运行覆盖率脚本
cd scripts\coverage
.\run_coverage_windows.ps1

# 4. 查看报告
start ..\..\coverage_report\html\index.html
```


### 覆盖率脚本选项

#### Linux 脚本选项

```bash
# 运行特定测试套件
./run_coverage_linux.sh -f "GPIO*"

# 自定义输出目录
./run_coverage_linux.sh -o my_coverage

# 使用 gcovr 而不是 lcov
./run_coverage_linux.sh -t gcovr

# 显示帮助
./run_coverage_linux.sh -h
```

#### Windows 脚本选项

```powershell
# 运行特定测试套件
.\run_coverage_windows.ps1 -TestFilter "GPIO*"

# 自定义输出目录
.\run_coverage_windows.ps1 -OutputDir "my_coverage"

# 只生成 HTML 报告
.\run_coverage_windows.ps1 -ExportFormat html

# 只生成 Cobertura XML
.\run_coverage_windows.ps1 -ExportFormat cobertura
```

### 解读覆盖率报告

#### 覆盖率指标

1. **行覆盖率 (Line Coverage)**: 执行的代码行百分比
2. **分支覆盖率 (Branch Coverage)**: 执行的条件分支百分比
3. **函数覆盖率 (Function Coverage)**: 调用的函数百分比

**目标**: 所有指标达到 100%

#### 颜色编码

- **绿色**: 代码已执行 / 所有分支已覆盖
- **黄色**: 部分分支已覆盖（部分覆盖）
- **红色**: 代码未执行 / 无分支覆盖

#### 常见未覆盖代码

1. **错误处理路径**: 处理错误条件的代码
2. **边界情况**: 边界值和特殊情况
3. **清理代码**: 反初始化和资源清理
4. **电源管理**: 挂起/恢复循环

### 提高覆盖率

#### 识别未覆盖代码

```bash
# 查看覆盖率报告，找到红色或黄色标记的代码
# 分析为什么这些代码未被执行
```

#### 添加测试用例

```cpp
/* 示例：覆盖错误处理路径 */
TEST_F(PeripheralTest, ErrorHandling_NullPointer) {
    /* 测试空指针参数 */
    EXPECT_NE(NX_OK, peripheral->init(nullptr, nullptr));
}

TEST_F(PeripheralTest, ErrorHandling_InvalidParameter) {
    /* 测试无效参数 */
    nx_<peripheral>_config_t config = {
        .invalid_field = 9999  /* 超出范围 */
    };
    EXPECT_NE(NX_OK, peripheral->init(peripheral, &config));
}
```


---

## 测试组织结构

### 目录结构

```
tests/
├── CMakeLists.txt                      # 测试构建配置
├── test_main.cpp                       # 测试主入口
├── hal/                                # HAL 测试
│   ├── test_nx_gpio.cpp               # GPIO 单元测试
│   ├── test_nx_gpio_properties.cpp    # GPIO 属性测试
│   ├── test_nx_uart.cpp               # UART 单元测试
│   ├── test_nx_uart_properties.cpp    # UART 属性测试
│   ├── ...                            # 其他外设测试
│   └── native/                        # Native 平台测试辅助
│       ├── README.md                  # 测试辅助说明
│       ├── TESTING_GUIDE.md           # 本文件
│       ├── MIGRATION_GUIDE.md         # 迁移指南
│       └── devices/                   # 设备特定辅助
│           ├── native_gpio_helpers.h
│           ├── native_gpio_helpers.c
│           ├── native_uart_helpers.h
│           ├── native_uart_helpers.c
│           └── ...
└── ...
```

### 测试文件命名

- **单元测试**: `test_nx_<peripheral>.cpp`
- **属性测试**: `test_nx_<peripheral>_properties.cpp`
- **测试辅助头文件**: `native_<peripheral>_helpers.h`
- **测试辅助实现**: `native_<peripheral>_helpers.c`

### 测试套件组织

每个外设的测试分为多个部分：

```cpp
/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/
class PeripheralTest : public ::testing::Test { /* ... */ };

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests                                                */
/*---------------------------------------------------------------------------*/
TEST_F(PeripheralTest, BasicInitialization) { /* ... */ }
TEST_F(PeripheralTest, BasicOperation) { /* ... */ }

/*---------------------------------------------------------------------------*/
/* Error Handling Tests                                                     */
/*---------------------------------------------------------------------------*/
TEST_F(PeripheralTest, NullPointerHandling) { /* ... */ }
TEST_F(PeripheralTest, InvalidParameterHandling) { /* ... */ }

/*---------------------------------------------------------------------------*/
/* Power Management Tests                                                   */
/*---------------------------------------------------------------------------*/
TEST_F(PeripheralTest, SuspendResume) { /* ... */ }

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests                                                          */
/*---------------------------------------------------------------------------*/
TEST_F(PeripheralTest, InitDeinit) { /* ... */ }
```


---

## 最佳实践

### 测试设计原则

#### 1. 独立性

每个测试应该独立运行，不依赖其他测试：

```cpp
/* ✅ 好的做法 */
TEST_F(PeripheralTest, Test1) {
    /* 设置 */
    native_peripheral_reset(0);
    
    /* 测试 */
    /* ... */
    
    /* 清理 */
    peripheral->deinit(peripheral);
}

/* ❌ 坏的做法 */
TEST_F(PeripheralTest, Test1) {
    /* 假设 Test0 已经初始化了外设 */
    /* ... */
}
```

#### 2. 可重复性

测试应该每次运行产生相同的结果：

```cpp
/* ✅ 好的做法 */
TEST_F(PeripheralTest, DataTransmission) {
    /* 使用固定的测试数据 */
    uint8_t data[] = {0x01, 0x02, 0x03};
    /* ... */
}

/* ⚠️ 属性测试使用随机数据，但应该设置种子 */
TEST_F(PeripheralPropertyTest, Property1) {
    rng.seed(12345);  /* 固定种子用于调试 */
    /* ... */
}
```

#### 3. 清晰的断言

使用清晰的断言消息：

```cpp
/* ✅ 好的做法 */
EXPECT_EQ(5, len) << "Expected to receive 5 bytes";
EXPECT_TRUE(state.initialized) << "Peripheral should be initialized";

/* ❌ 坏的做法 */
EXPECT_EQ(5, len);
EXPECT_TRUE(state.initialized);
```

#### 4. 测试一个概念

每个测试应该只测试一个概念：

```cpp
/* ✅ 好的做法 */
TEST_F(PeripheralTest, Initialization) {
    /* 只测试初始化 */
}

TEST_F(PeripheralTest, DataTransmission) {
    /* 只测试数据传输 */
}

/* ❌ 坏的做法 */
TEST_F(PeripheralTest, InitAndTransmitAndDeinit) {
    /* 测试太多东西 */
}
```

### 属性测试最佳实践

#### 1. 足够的迭代次数

属性测试应该运行至少 100 次迭代：

```cpp
TEST_F(PeripheralPropertyTest, Property1) {
    const int iterations = 100;  /* 最少 100 次 */
    
    for (int i = 0; i < iterations; ++i) {
        /* 生成随机输入 */
        /* 执行操作 */
        /* 验证属性 */
    }
}
```

#### 2. 智能的随机数据生成

生成有意义的随机数据：

```cpp
/* ✅ 好的做法 */
std::uniform_int_distribution<uint32_t> baudrate_dist(9600, 115200);
uint32_t baudrate = baudrate_dist(rng);

/* ❌ 坏的做法 */
uint32_t baudrate = rng();  /* 可能生成无效值 */
```


#### 3. 清理状态

每次迭代后清理状态：

```cpp
TEST_F(PeripheralPropertyTest, Property1) {
    for (int i = 0; i < 100; ++i) {
        /* 测试 */
        /* ... */
        
        /* 清理 */
        peripheral->deinit(peripheral);
        native_peripheral_reset(0);
    }
}
```

#### 4. 标注属性

使用注释标注属性编号和验证的需求：

```cpp
/**
 * Feature: native-hal-validation, Property 1: Init Idempotence
 *
 * *For any* peripheral instance and configuration, multiple initializations
 * with the same configuration should produce the same result state.
 *
 * **Validates: Requirements 1.1, 2.1, 3.1**
 */
TEST_F(PeripheralPropertyTest, Property1_InitIdempotence) {
    /* ... */
}
```

### 测试辅助函数最佳实践

#### 1. 最小化测试辅助代码

只提供必要的测试辅助功能：

```cpp
/* ✅ 好的做法：提供必要的测试功能 */
nx_status_t native_uart_inject_rx_data(uint8_t instance, 
                                       const uint8_t* data, 
                                       size_t len);

/* ❌ 坏的做法：简单包装公共接口 */
nx_status_t native_uart_init(uint8_t instance, 
                             const nx_uart_config_t* config) {
    nx_uart_t* uart = nx_factory_uart(instance);
    return uart->init(uart, config);
}
```

#### 2. 参数验证

测试辅助函数应该验证参数：

```cpp
nx_status_t native_uart_get_state(uint8_t instance, 
                                  native_uart_state_t* state) {
    /* 验证参数 */
    if (instance >= MAX_UART_INSTANCES || state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }
    
    /* 实现 */
    /* ... */
    
    return NX_OK;
}
```

#### 3. 文档化

遵循 Nexus 代码注释规范：

```cpp
/**
 * \brief           注入 UART 接收数据
 * \param[in]       instance: UART 实例 ID
 * \param[in]       data: 数据缓冲区
 * \param[in]       len: 数据长度
 * \return          NX_OK 成功，其他值失败
 */
nx_status_t native_uart_inject_rx_data(uint8_t instance, 
                                       const uint8_t* data, 
                                       size_t len);
```


---

## 故障排查

### 常见问题

#### 1. 测试编译失败

**问题**: 找不到头文件

```
fatal error: hal/nx_factory.h: No such file or directory
```

**解决方案**:
```bash
# 确保配置时启用了测试
cmake -B build -DNEXUS_BUILD_TESTS=ON -DNEXUS_PLATFORM=native
```

#### 2. 测试运行失败

**问题**: 大多数测试失败（Windows/MSVC）

```
[  FAILED  ] GPIOTest.BasicInitialization
[  FAILED  ] UARTTest.BasicInitialization
...
```

**解决方案**: Native 平台在 MSVC 上存在设备注册问题。使用 Linux/WSL：

```bash
# 在 WSL 中运行
wsl
cd /mnt/d/code/nexus/nexus
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build
cd build && ctest
```

#### 3. 覆盖率工具未找到

**问题**: `lcov: command not found`

**解决方案**:
```bash
# Ubuntu/Debian
sudo apt-get install lcov

# 或使用 gcovr
pip install gcovr
```

**问题**: `OpenCppCoverage not found` (Windows)

**解决方案**:
1. 从 https://github.com/OpenCppCoverage/OpenCppCoverage/releases 下载
2. 运行安装程序
3. 重启终端

#### 4. 覆盖率数据未生成

**问题**: 运行测试后没有 `.gcda` 文件

**解决方案**:
```bash
# 确保启用了覆盖率
cmake -B build -DNEXUS_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 运行测试
cd build && ctest

# 检查 .gcda 文件
find . -name "*.gcda"
```

#### 5. 测试超时

**问题**: 属性测试运行时间过长

**解决方案**: 减少迭代次数（仅用于调试）：

```cpp
/* 临时减少迭代次数 */
const int iterations = 10;  /* 正常应该是 100 */
```


#### 6. 测试辅助函数链接错误

**问题**: 未定义的引用

```
undefined reference to `native_uart_inject_rx_data'
```

**解决方案**: 确保测试辅助 `.c` 文件已添加到 CMakeLists.txt：

```cmake
# 在 tests/CMakeLists.txt 中
target_sources(nexus_tests PRIVATE
    hal/native/devices/native_uart_helpers.c
    hal/native/devices/native_spi_helpers.c
    # ... 其他辅助文件
)
```

#### 7. 随机测试失败

**问题**: 属性测试偶尔失败

**解决方案**: 
1. 使用固定种子重现问题：
```cpp
rng.seed(12345);  /* 使用失败时的种子 */
```

2. 检查测试逻辑是否正确
3. 检查是否有竞态条件

### 调试技巧

#### 1. 运行单个测试

```bash
# 只运行失败的测试
./tests/nexus_tests --gtest_filter="GPIOTest.BasicInitialization"
```

#### 2. 启用详细输出

```bash
# 详细输出
./tests/nexus_tests --gtest_verbose

# 或使用 CTest
cd build
ctest --verbose --output-on-failure
```

#### 3. 使用调试器

```bash
# GDB
gdb ./tests/nexus_tests
(gdb) run --gtest_filter="GPIOTest.BasicInitialization"
(gdb) break native_gpio_get_state
(gdb) continue

# LLDB
lldb ./tests/nexus_tests
(lldb) run --gtest_filter="GPIOTest.BasicInitialization"
(lldb) breakpoint set --name native_gpio_get_state
(lldb) continue
```

#### 4. 添加调试输出

```cpp
TEST_F(PeripheralTest, Debug) {
    /* 添加调试输出 */
    native_peripheral_state_t state;
    native_peripheral_get_state(0, &state);
    
    std::cout << "Initialized: " << state.initialized << std::endl;
    std::cout << "TX Count: " << state.tx_count << std::endl;
    
    /* 测试 */
    /* ... */
}
```


---

## 附录

### A. 支持的外设列表

| 外设 | 单元测试 | 属性测试 | 测试辅助 |
|------|---------|---------|---------|
| GPIO | ✅ | ✅ | ✅ |
| UART | ✅ | ✅ | ✅ |
| SPI | ✅ | ✅ | ✅ |
| I2C | ✅ | ✅ | ✅ |
| Timer | ✅ | ✅ | ✅ |
| ADC | ✅ | ✅ | ✅ |
| DAC | ✅ | ✅ | ✅ |
| CRC | ✅ | ✅ | ⚠️ |
| Flash | ✅ | ✅ | ⚠️ |
| RTC | ✅ | ✅ | ⚠️ |
| Watchdog | ✅ | ✅ | ⚠️ |
| USB | ✅ | ✅ | ⚠️ |
| SDIO | ✅ | ✅ | ⚠️ |
| Option Bytes | ✅ | ✅ | ⚠️ |

✅ = 已实现  
⚠️ = 需要迁移到新结构

### B. 测试辅助函数参考

#### 通用函数（所有外设）

```c
/* 获取状态 */
nx_status_t native_<peripheral>_get_state(uint8_t instance, 
                                          native_<peripheral>_state_t* state);

/* 重置 */
nx_status_t native_<peripheral>_reset(uint8_t instance);
void native_<peripheral>_reset_all(void);
```

#### 通信外设（UART、SPI、I2C、USB、SDIO）

```c
/* 数据注入 */
nx_status_t native_<peripheral>_inject_rx_data(uint8_t instance, 
                                               const uint8_t* data, 
                                               size_t len);

/* 数据捕获 */
nx_status_t native_<peripheral>_get_tx_data(uint8_t instance, 
                                            uint8_t* data, 
                                            size_t* len);
```

#### 定时器外设（Timer、RTC、Watchdog）

```c
/* 时间推进 */
nx_status_t native_<peripheral>_advance_time(uint8_t instance, 
                                             uint32_t ms);
```

#### GPIO 特定

```c
/* 模拟引脚变化 */
nx_status_t native_gpio_simulate_pin_change(uint8_t port, 
                                            uint8_t pin, 
                                            nx_gpio_level_t level);

/* 检查中断 */
bool native_gpio_is_interrupt_triggered(uint8_t port, uint8_t pin);
```

#### ADC 特定

```c
/* 设置模拟值 */
nx_status_t native_adc_set_analog_value(uint8_t instance, 
                                        uint8_t channel, 
                                        uint16_t value);
```

#### DAC 特定

```c
/* 获取输出值 */
uint16_t native_dac_get_output_value(uint8_t instance, uint8_t channel);
```


### C. 相关文档

- **[README.md](README.md)**: 测试辅助文件概述
- **[MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)**: 从旧结构迁移指南
- **[Coverage Scripts README](../../../scripts/coverage/README.md)**: 覆盖率脚本详细说明
- **[Coverage Analysis Guide](../../../docs/testing/COVERAGE_ANALYSIS.md)**: 覆盖率分析完整指南
- **[CI/CD Configuration](../../../docs/testing/CI_CD_CONFIGURATION.md)**: CI/CD 集成配置

### D. 快速参考

#### 常用命令

```bash
# 构建测试
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build

# 运行所有测试
cd build && ctest --output-on-failure

# 运行特定测试
./build/tests/nexus_tests --gtest_filter="GPIO*"

# 生成覆盖率（Linux）
cd scripts/coverage && ./run_coverage_linux.sh

# 生成覆盖率（Windows）
cd scripts\coverage && .\run_coverage_windows.ps1
```

#### 测试文件模板

**单元测试**:
```cpp
#include <gtest/gtest.h>
extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_<peripheral>_helpers.h"
}

class PeripheralTest : public ::testing::Test {
protected:
    void SetUp() override {
        native_<peripheral>_reset_all();
        peripheral = nx_factory_<peripheral>(0);
        ASSERT_NE(nullptr, peripheral);
    }
    
    void TearDown() override {
        if (peripheral && peripheral->deinit) {
            peripheral->deinit(peripheral);
        }
        native_<peripheral>_reset_all();
    }
    
    nx_<peripheral>_t* peripheral = nullptr;
};

TEST_F(PeripheralTest, BasicTest) {
    /* 测试代码 */
}
```

**属性测试**:
```cpp
#include <gtest/gtest.h>
#include <random>
extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_<peripheral>_helpers.h"
}

class PeripheralPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng.seed(std::random_device{}());
        native_<peripheral>_reset_all();
        peripheral = nx_factory_<peripheral>(0);
        ASSERT_NE(nullptr, peripheral);
    }
    
    void TearDown() override {
        if (peripheral && peripheral->deinit) {
            peripheral->deinit(peripheral);
        }
        native_<peripheral>_reset_all();
    }
    
    std::mt19937 rng;
    nx_<peripheral>_t* peripheral = nullptr;
};

/**
 * Feature: native-hal-validation, Property N: Title
 * *For any* description
 * **Validates: Requirements X.Y**
 */
TEST_F(PeripheralPropertyTest, PropertyN_Title) {
    for (int i = 0; i < 100; ++i) {
        /* 测试代码 */
    }
}
```

---

## 总结

本指南涵盖了 Native 平台 HAL 测试的所有方面：

1. ✅ **运行测试**: 使用 CTest 或直接运行测试可执行文件
2. ✅ **添加新测试**: 创建单元测试和属性测试文件
3. ✅ **使用测试辅助函数**: 注入数据、查询状态、模拟硬件行为
4. ✅ **生成覆盖率报告**: 使用 lcov/gcovr (Linux) 或 OpenCppCoverage (Windows)
5. ✅ **最佳实践**: 测试设计原则和代码规范
6. ✅ **故障排查**: 常见问题和解决方案

遵循本指南，您可以有效地编写、运行和维护 Native 平台 HAL 测试，确保代码质量和 100% 覆盖率目标。

**需要帮助？** 查看相关文档或联系 Nexus 团队。

