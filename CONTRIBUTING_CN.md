# 为 Nexus 做贡献

感谢您对 Nexus 项目的关注！本文档提供了贡献指南。

[English](CONTRIBUTING.md) | [中文](CONTRIBUTING_CN.md)

## 行为准则

请在所有互动中保持尊重和建设性。

## 开发环境设置

### 环境要求

```bash
# Windows
winget install Kitware.CMake
winget install Git.Git
# 安装 Visual Studio 2019+ 或 Build Tools

# Linux (Ubuntu/Debian)
sudo apt-get install cmake gcc g++ git

# macOS
brew install cmake git
```

### 克隆和构建

```bash
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# 本地构建（用于测试）
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Debug

# 运行测试
ctest --test-dir build -C Debug --output-on-failure
```

### IDE 设置

**VS Code**（推荐）:
- 打开 `nexus` 文件夹
- 安装推荐的扩展（C/C++、CMake Tools）
- 使用提供的 `.vscode/` 配置

## 如何贡献

### 报告 Bug

1. 检查现有 Issues 以避免重复
2. 使用 Bug 报告模板
3. 包含以下信息：
   - 平台和版本（Windows/Linux/macOS，编译器版本）
   - 重现步骤
   - 预期行为 vs 实际行为
   - 相关日志或截图

### 建议新功能

1. 检查现有的功能请求
2. 使用功能请求模板
3. 描述使用场景和好处

### 提交 Pull Request

1. Fork 仓库
2. 创建功能分支: `git checkout -b feature/my-feature`
3. 进行修改
4. 确保测试通过: `ctest --test-dir build -C Debug`
5. 遵循代码风格指南
6. 使用约定式提交: `feat(hal): add PWM support`
7. 推送并创建 Pull Request

## 代码风格

### C 代码

- 遵循 `.clang-format` 配置
- 使用 Doxygen 注释，采用**反斜杠风格**（`\brief`、`\param`）
- 最大行长度: 80 字符
- 缩进: 4 个空格（不使用制表符）
- 指针对齐: 左对齐（`char* ptr`，而不是 `char *ptr`）

### Doxygen 注释风格

所有 Doxygen 注释使用**反斜杠风格**：

```c
/**
 * \file            hal_gpio.h
 * \brief           GPIO 硬件抽象层
 * \author          您的名字
 * \version         1.0.0
 * \date            2026-01-25
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/**
 * \brief           初始化 GPIO 引脚
 * \param[in]       port: GPIO 端口枚举
 * \param[in]       pin: 引脚号（0-15）
 * \param[in]       config: 指向配置结构的指针
 * \return          成功返回 HAL_OK，否则返回错误码
 * \note            引脚必须先去初始化才能重新初始化
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                           const hal_gpio_config_t* config);
```

### 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 文件 | snake_case | `hal_gpio.c` |
| 函数 | snake_case | `hal_gpio_init()` |
| 类型 | snake_case_t | `hal_gpio_config_t` |
| 宏 | UPPER_CASE | `HAL_GPIO_PORT_MAX` |
| 枚举 | UPPER_CASE | `HAL_GPIO_DIR_INPUT` |

### 提交信息

遵循[约定式提交](https://www.conventionalcommits.org/zh-hans/)：

```
<类型>(<范围>): <主题>

[可选的正文]

[可选的脚注]
```

类型: `feat`（新功能）、`fix`（修复）、`docs`（文档）、`style`（格式）、`refactor`（重构）、`perf`（性能）、`test`（测试）、`build`（构建）、`ci`（CI）、`chore`（杂项）

## 测试

### 测试要求

**所有贡献必须包含适当的测试。** 测试是维护代码质量和防止回归的关键部分。

#### 添加新功能时

1. **必须编写单元测试**: 编写验证特定示例和边界情况的单元测试
2. **推荐编写属性测试**: 对于 HAL 实现，编写验证多个输入的通用属性的基于属性的测试
3. **测试覆盖率**: 新代码必须维持或提高整体覆盖率
4. **测试文档**: 包含清晰的注释，解释每个测试验证的内容

#### 修改现有代码时

1. **运行所有测试**: 确保所有现有测试仍然通过
2. **更新测试**: 如果行为变化是有意的，修改测试
3. **添加测试**: 为新覆盖的场景添加新测试
4. **无回归**: 不要降低测试覆盖率

#### 删除功能时

1. **删除测试**: 删除已删除功能的测试
2. **更新依赖**: 更新依赖于已删除功能的测试
3. **验证构建**: 确保测试套件仍然可以编译和运行

### 覆盖率要求

**目标**: Native 平台 HAL 实现 100% 代码覆盖率

**最低要求**:
- 行覆盖率: ≥95%
- 分支覆盖率: ≥95%
- 函数覆盖率: ≥95%

**覆盖率验证**:
```bash
# 生成覆盖率报告（Linux/WSL）
cd scripts/coverage
./run_coverage_linux.sh

# 生成覆盖率报告（Windows）
cd scripts\coverage
.\run_coverage_windows.ps1

# 查看报告
# Linux: xdg-open ../../coverage_html/index.html
# Windows: start ..\..\coverage_report\html\index.html
```

**覆盖率强制执行**:
- CI/CD 流水线自动检查覆盖率
- 降低覆盖率低于阈值的 PR 将被标记
- 维护者可能要求额外的测试以满足覆盖率要求

### 运行测试

#### 快速开始

```bash
# 构建测试
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Debug

# 运行所有测试
cd build && ctest --output-on-failure

# 或直接运行测试可执行文件
./build/tests/nexus_tests
```

#### 运行特定测试

```bash
# 运行特定测试套件
./build/tests/nexus_tests --gtest_filter="GPIO*"

# 运行特定测试用例
./build/tests/nexus_tests --gtest_filter="GPIOTest.BasicInitialization"

# 运行多个测试套件
./build/tests/nexus_tests --gtest_filter="GPIO*:UART*:SPI*"

# 详细输出
./build/tests/nexus_tests --gtest_verbose
```

#### 平台特定说明

**Linux/WSL（推荐用于 Native 平台）**:
- 完整的测试支持
- 覆盖率工具: lcov 或 gcovr
- 所有测试应该通过

**Windows (MSVC)**:
- Native 平台存在已知的设备注册问题
- 使用 WSL 进行 Native 平台测试
- 其他平台工作正常

### 编写测试

#### 测试文件组织

测试使用 Google Test 框架。每个外设应该有两个测试文件：

1. **单元测试**: `tests/hal/test_nx_<peripheral>.cpp`
   - 验证特定示例和边界情况
   - 测试错误处理和边界条件
   - 测试组件之间的集成

2. **属性测试**: `tests/hal/test_nx_<peripheral>_properties.cpp`
   - 验证随机输入的通用属性
   - 每个属性至少运行 100 次迭代
   - 测试设计文档中的正确性属性

#### 单元测试示例

```cpp
/**
 * \file            test_nx_gpio.cpp
 * \brief           GPIO HAL 单元测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

class GPIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        native_gpio_reset_all();
        gpio = nx_factory_gpio(0);
        ASSERT_NE(nullptr, gpio);
    }
    
    void TearDown() override {
        if (gpio != nullptr && gpio->deinit != nullptr) {
            gpio->deinit(gpio);
        }
        native_gpio_reset_all();
    }
    
    nx_gpio_t* gpio = nullptr;
};

TEST_F(GPIOTest, BasicInitialization) {
    nx_gpio_config_t config = {
        .mode = NX_GPIO_MODE_OUTPUT,
        .pull = NX_GPIO_PULL_NONE,
        .level = NX_GPIO_LEVEL_LOW
    };
    
    ASSERT_EQ(NX_OK, gpio->init(gpio, 5, &config));
    
    /* 验证状态 */
    native_gpio_state_t state;
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 5, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(GPIOTest, ErrorHandling_NullPointer) {
    EXPECT_NE(NX_OK, gpio->init(nullptr, 0, nullptr));
}
```

#### 属性测试示例

```cpp
/**
 * \file            test_nx_gpio_properties.cpp
 * \brief           GPIO HAL 基于属性的测试
 */

#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

class GPIOPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng.seed(std::random_device{}());
        native_gpio_reset_all();
        gpio = nx_factory_gpio(0);
        ASSERT_NE(nullptr, gpio);
    }
    
    void TearDown() override {
        if (gpio != nullptr && gpio->deinit != nullptr) {
            gpio->deinit(gpio);
        }
        native_gpio_reset_all();
    }
    
    std::mt19937 rng;
    nx_gpio_t* gpio = nullptr;
};

/**
 * 功能: native-hal-validation, 属性 11: GPIO 读写一致性
 *
 * *对于任何* GPIO 引脚和电平值，写入后立即读取应该返回相同的电平值。
 *
 * **验证: 需求 1.2, 1.3**
 */
TEST_F(GPIOPropertyTest, Property11_ReadWriteConsistency) {
    const int iterations = 100;
    
    for (int i = 0; i < iterations; ++i) {
        /* 生成随机引脚和电平 */
        std::uniform_int_distribution<uint8_t> pin_dist(0, 15);
        uint8_t pin = pin_dist(rng);
        
        nx_gpio_level_t level = (rng() % 2) ? NX_GPIO_LEVEL_HIGH : NX_GPIO_LEVEL_LOW;
        
        /* 初始化引脚为输出 */
        nx_gpio_config_t config = {
            .mode = NX_GPIO_MODE_OUTPUT,
            .pull = NX_GPIO_PULL_NONE,
            .level = NX_GPIO_LEVEL_LOW
        };
        ASSERT_EQ(NX_OK, gpio->init(gpio, pin, &config));
        
        /* 写入电平 */
        ASSERT_EQ(NX_OK, gpio->write(gpio, pin, level));
        
        /* 读取电平 */
        nx_gpio_level_t read_level;
        ASSERT_EQ(NX_OK, gpio->read(gpio, pin, &read_level));
        
        /* 验证一致性 */
        EXPECT_EQ(level, read_level) << "迭代 " << i << ", 引脚 " << (int)pin;
        
        /* 清理 */
        gpio->deinit(gpio);
        native_gpio_reset(0);
    }
}
```

#### 测试辅助函数

对于 Native 平台测试，使用测试辅助函数来：
- 查询内部外设状态
- 注入接收数据（模拟硬件）
- 捕获发送数据（验证输出）
- 推进时间（用于定时器）
- 重置外设到干净状态

示例:
```cpp
#include "tests/hal/native/devices/native_uart_helpers.h"

/* 注入数据以模拟硬件接收 */
uint8_t rx_data[] = {0x01, 0x02, 0x03};
native_uart_inject_rx_data(0, rx_data, sizeof(rx_data));

/* 捕获发送的数据 */
uint8_t tx_buffer[10];
size_t tx_len = sizeof(tx_buffer);
native_uart_get_tx_data(0, tx_buffer, &tx_len);

/* 查询内部状态 */
native_uart_state_t state;
native_uart_get_state(0, &state);
```

### 测试最佳实践

1. **独立性**: 每个测试应该独立运行
2. **可重复性**: 测试应该产生一致的结果
3. **清晰的断言**: 使用描述性的断言消息
4. **单一概念**: 每个测试应该验证一个概念
5. **清理**: 始终在 TearDown() 中清理资源
6. **文档**: 注释每个测试验证的内容

### 详细测试指南

有关全面的测试文档，请参阅：
- **[Native 平台测试指南](tests/hal/native/TESTING_GUIDE.md)**: 运行测试、添加新测试、使用测试辅助函数和生成覆盖率报告的完整指南
- **[覆盖率分析指南](docs/testing/COVERAGE_ANALYSIS.md)**: 详细的覆盖率分析和改进策略
- **[覆盖率脚本 README](scripts/coverage/README.md)**: 覆盖率脚本使用和选项

### 提交前检查清单

在提交 PR 之前，验证：

- [ ] 所有新代码都有相应的测试
- [ ] 所有测试在本地通过: `cd build && ctest --output-on-failure`
- [ ] 覆盖率满足要求（≥95% 或维持 100%）
- [ ] 属性测试至少运行 100 次迭代
- [ ] 测试代码遵循 Nexus 编码标准
- [ ] 测试文档清晰完整
- [ ] 没有测试警告或错误

## 文档

### 构建文档

```bash
# API 文档（Doxygen）
doxygen Doxyfile
# 输出: docs/api/html/index.html

# 用户文档（Sphinx）- 英文
cd docs/sphinx
python -m sphinx -b html . _build/html/en

# 用户文档（Sphinx）- 中文
python -m sphinx -b html . _build/html/cn -D master_doc=index_cn -D language=zh_CN
```

### 文档指南

- 为公共接口更新 API 文档
- 为新功能添加示例
- 保持 README 最新
- 在适用的地方支持中英文

## CI/CD

所有 PR 都会触发 GitHub Actions 工作流：

| 工作流 | 说明 |
|--------|------|
| `build.yml` | 多平台构建（Windows、Linux、macOS）+ ARM 交叉编译 |
| `test.yml` | 单元测试、覆盖率、消毒器、MISRA 检查 |

### 本地 CI 验证

在提交 PR 之前，在本地验证：

```bash
# 1. 构建通过
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Release

# 2. 测试通过
ctest --test-dir build -C Release --output-on-failure

# 3. 代码格式检查
clang-format --dry-run --Werror hal/**/*.c hal/**/*.h

# 4. 文档构建
doxygen Doxyfile
```

## 审查流程

1. 自动化 CI 检查必须通过
2. 至少需要一位维护者批准
3. 处理所有审查意见
4. 如果要求，压缩提交

## 问题？

开启讨论或联系维护者。

感谢您的贡献！🎉

---

## 附录：常见任务

### 添加新的 HAL 外设

1. **定义接口**（`hal/include/hal/nx_<peripheral>.h`）:
```c
/**
 * \file            nx_pwm.h
 * \brief           PWM 设备接口
 * \author          Nexus Team
 */

#ifndef NX_PWM_H
#define NX_PWM_H

#include "hal/nx_common.h"

/**
 * \brief           PWM 配置结构
 */
typedef struct {
    uint32_t frequency;  /**< PWM 频率（Hz）*/
    uint8_t duty_cycle;  /**< 占空比（0-100）*/
} nx_pwm_config_t;

/**
 * \brief           PWM 设备接口
 */
typedef struct nx_pwm {
    /**
     * \brief           初始化 PWM
     * \param[in]       self: PWM 设备指针
     * \param[in]       config: 配置
     * \return          状态码
     */
    nx_status_t (*init)(struct nx_pwm* self, const nx_pwm_config_t* config);
    
    /**
     * \brief           启动 PWM
     * \param[in]       self: PWM 设备指针
     * \return          状态码
     */
    nx_status_t (*start)(struct nx_pwm* self);
    
    /**
     * \brief           停止 PWM
     * \param[in]       self: PWM 设备指针
     * \return          状态码
     */
    nx_status_t (*stop)(struct nx_pwm* self);
} nx_pwm_t;

#endif /* NX_PWM_H */
```

2. **实现平台特定代码**（`platforms/stm32f4/hal/pwm.c`）

3. **添加工厂函数**（`hal/include/hal/nx_factory.h`）:
```c
/**
 * \brief           获取 PWM 设备
 * \param[in]       index: PWM 索引
 * \return          PWM 设备指针
 */
nx_pwm_t* nx_factory_pwm(uint8_t index);
```

4. **编写测试**（`tests/hal/test_nx_pwm.cpp`）

5. **更新文档**（`hal/docs/USER_GUIDE.md`）

### 添加新的 RTOS 适配器

1. **创建适配器目录**: `osal/adapters/<rtos_name>/`

2. **实现 OSAL 接口**:
   - `task.c` - 任务管理
   - `mutex.c` - 互斥锁
   - `semaphore.c` - 信号量
   - `queue.c` - 消息队列
   - `timer.c` - 软件定时器
   - `memory.c` - 内存管理

3. **添加 CMake 配置**: `osal/adapters/<rtos_name>/CMakeLists.txt`

4. **编写测试**: `tests/osal/test_<rtos_name>.cpp`

5. **更新文档**: `osal/docs/PORTING_GUIDE.md`

### 添加新的平台

1. **创建平台目录**: `platforms/<platform>/`

2. **实现 HAL 接口**:
   - `hal/gpio.c`
   - `hal/uart.c`
   - `hal/spi.c`
   - 等等...

3. **添加平台配置**:
   - `CMakeLists.txt`
   - `platform_config.h`
   - 链接器脚本

4. **添加工具链文件**: `cmake/toolchains/<platform>.cmake`

5. **编写测试**: `tests/platforms/<platform>/`

6. **更新文档**: `hal/docs/PORTING_GUIDE.md`

---

**由 Nexus 团队维护**
