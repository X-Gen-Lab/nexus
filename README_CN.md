# Nexus 嵌入式平台

[![CI 状态](https://github.com/nexus-platform/nexus/workflows/CI/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![构建矩阵](https://github.com/nexus-platform/nexus/workflows/Build%20Matrix/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![文档构建](https://github.com/nexus-platform/nexus/workflows/Documentation%20Build/badge.svg)](https://github.com/nexus-platform/nexus/actions)
[![codecov](https://codecov.io/gh/nexus-platform/nexus/branch/main/graph/badge.svg)](https://codecov.io/gh/nexus-platform/nexus)
[![许可证: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![版本](https://img.shields.io/badge/version-0.1.0-blue.svg)](CHANGELOG.md)

[English](README.md) | [中文](#中文)

---

**Nexus** 是一个专业的嵌入式软件开发平台，专为构建可靠、安全、可移植的嵌入式应用而设计，支持多种 MCU 平台，具有完善的测试和文档体系。

## ✨ 核心特性

### 核心层次
- **🔧 硬件抽象层 (HAL)** - 统一的硬件接口，基于 Kconfig 的编译时配置
- **⚙️ 操作系统抽象层 (OSAL)** - 支持 FreeRTOS、RT-Thread、Zephyr 和裸机
- **📦 框架层** - 配置管理、日志、Shell 和初始化系统

### 开发体验
- **🌐 跨平台开发** - Windows、Linux、macOS，支持原生模拟
- **🧪 全面测试** - 1539+ 个测试，原生平台 100% 代码覆盖率
- **📚 双语文档** - 完整的中英文文档
- **🛠️ Python 构建工具** - 跨平台的构建、测试和格式化脚本
- **⚡ Kconfig 配置** - 所有外设的编译时配置系统

### 高级特性
- **🔒 安全特性** - 安全启动、TLS 1.3、硬件加密加速（计划中）
- **☁️ 云端集成** - AWS IoT、Azure IoT、阿里云（计划中）
- **🤖 TinyML** - 支持 TensorFlow Lite Micro 边缘 AI（计划中）

## 🎯 支持的平台

| 平台 | 状态 | 外设支持 | RTOS 支持 |
|------|------|---------|-----------|
| **Native** | ✅ 生产就绪 | 完整的测试模拟 | 裸机、FreeRTOS |
| **STM32F4** | ✅ 生产就绪 | GPIO、UART、SPI、I2C、ADC、PWM、定时器、DMA、CAN | 裸机、FreeRTOS |
| **STM32H7** | 🚧 开发中 | + TrustZone、加密、以太网 | 裸机、FreeRTOS |
| **GD32** | 🚧 开发中 | GPIO、UART、SPI、I2C | 裸机 |
| **ESP32** | 📋 计划中 | + WiFi、BLE、触摸 | FreeRTOS |
| **nRF52** | 📋 计划中 | + BLE、NFC、加密 | FreeRTOS、Zephyr |

## 🚀 快速开始

### 环境要求

**所有平台：**
- CMake 3.16+
- Git
- Python 3.8+（用于构建脚本）

**本地构建（测试）：**
- Windows: Visual Studio 2019+ 或 MSVC Build Tools
- Linux: GCC 9+ 或 Clang 10+
- macOS: Xcode 命令行工具 (Clang 12+)

**ARM 交叉编译：**
- ARM GCC 工具链 10.3+ (`arm-none-eabi-gcc`)
- 下载地址: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

**文档生成：**
- Doxygen 1.9+
- Python 包: `pip install sphinx breathe sphinx_rtd_theme`

### 构建本地版本（测试）

```bash
# 克隆仓库
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# 方法 1：使用 Python 脚本（推荐，跨平台）
python scripts/building/build.py

# 方法 2：使用 CMake 预设（推荐用于 CMake 3.19+）
cmake --preset native-debug      # 调试构建
cmake --build --preset native-debug

cmake --preset native-release    # 发布构建
cmake --build --preset native-release

# 方法 3：直接使用 CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=native
cmake --build build --config Release

# 运行测试
python scripts/test/test.py
# 或: ctest --test-dir build -C Release --output-on-failure
```

### 构建 STM32F4 版本

```bash
# 方法 1：使用 Python 脚本
python scripts/building/build.py --platform stm32f4 --toolchain arm-none-eabi

# 方法 2：使用 CMake 预设（推荐用于 CMake 3.19+）
cmake --preset stm32f4-debug     # 调试构建
cmake --build --preset stm32f4-debug

cmake --preset stm32f4-release   # 发布构建
cmake --build --preset stm32f4-release

# 方法 3：直接使用 CMake
cmake -B build-stm32f4 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
    -DNEXUS_PLATFORM=stm32f4

cmake --build build-stm32f4 --config Release

# 输出: build-stm32f4/applications/blinky/blinky.elf
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `NEXUS_PLATFORM` | `native` | 目标平台: `native`, `stm32f4`, `stm32h7`, `gd32`, `esp32`, `nrf52` |
| `NEXUS_OSAL_BACKEND` | `baremetal` | OSAL 后端: `baremetal`, `freertos`, `rtthread`, `zephyr` |
| `NEXUS_BUILD_TESTS` | `ON` | 构建单元测试（仅 native） |
| `NEXUS_BUILD_EXAMPLES` | `ON` | 构建示例应用 |
| `NEXUS_ENABLE_COVERAGE` | `OFF` | 启用代码覆盖率分析 |
| `CMAKE_BUILD_TYPE` | `Debug` | 构建类型: `Debug`, `Release`, `MinSizeRel`, `RelWithDebInfo` |

### CMake 预设

项目包含 CMakePresets.json 用于标准化配置：

```bash
# 列出可用预设
cmake --list-presets

# 使用预设
cmake --preset native-debug
cmake --build --preset native-debug

# 常用预设：
# - native-debug: Native 平台调试构建
# - native-release: Native 平台发布构建
# - stm32f4-debug: STM32F4 调试构建
# - stm32f4-release: STM32F4 发布构建
```

## 📖 第一个项目

创建一个简单的 LED 闪烁应用：

```c
#include "hal/nx_factory.h"
#include "osal/osal.h"

int main(void)
{
    /* 初始化 OSAL 和 HAL */
    osal_init();
    nx_hal_init();

    /* 获取 GPIO 设备（端口 A，引脚 5）*/
    nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
    if (!led) {
        return -1;
    }

    /* 配置为输出（通过 Kconfig 在编译时完成）*/
    led->set_mode(led, NX_GPIO_MODE_OUTPUT_PP);

    /* 闪烁循环 */
    while (1) {
        led->toggle(led);
        osal_task_delay(500);  /* 500ms 延时 */
    }

    /* 清理（永远不会到达）*/
    nx_factory_gpio_release((nx_gpio_t*)led);
    nx_hal_deinit();
    return 0;
}
```

### 通过 Kconfig 配置

```kconfig
# 在项目的 Kconfig 或 defconfig 中

# 启用 GPIO 端口 A 引脚 5
CONFIG_HAL_GPIO_A_5=y
CONFIG_HAL_GPIO_A_5_MODE=OUTPUT_PP
CONFIG_HAL_GPIO_A_5_PULL=NONE
CONFIG_HAL_GPIO_A_5_SPEED=LOW
CONFIG_HAL_GPIO_A_5_LEVEL=LOW
```

## 📁 项目结构

```
nexus/
├── hal/                    # 硬件抽象层
│   ├── include/hal/        #   公共 API 头文件
│   ├── src/                #   通用实现
│   ├── docs/               #   完整文档（中英文）
│   └── Kconfig             #   HAL 配置选项
├── osal/                   # 操作系统抽象层
│   ├── include/osal/       #   公共 API 头文件
│   ├── adapters/           #   RTOS 适配器（裸机、FreeRTOS、RT-Thread）
│   ├── docs/               #   完整文档（中英文）
│   └── Kconfig             #   OSAL 配置选项
├── framework/              # 高级框架
│   ├── config/             #   配置管理系统
│   ├── log/                #   日志系统
│   ├── shell/              #   命令行 Shell
│   └── init/               #   初始化框架
├── platforms/              # 平台特定实现
│   ├── native/             #   主机模拟（Windows/Linux/macOS）
│   ├── stm32/              #   STM32 系列（F4、H7）
│   ├── gd32/               #   兆易创新 GD32
│   ├── esp32/              #   乐鑫 ESP32
│   ├── nrf52/              #   Nordic nRF52
│   └── Kconfig             #   平台配置
├── applications/           # 示例应用
│   ├── blinky/             #   LED 闪烁示例
│   ├── shell_demo/         #   命令行 Shell 演示
│   ├── config_demo/        #   配置系统演示
│   └── freertos_demo/      #   FreeRTOS 集成演示
├── tests/                  # 完整测试套件（1539+ 个测试）
│   ├── hal/                #   HAL 单元测试和属性测试
│   ├── osal/               #   OSAL 测试
│   ├── config/             #   Config 框架测试
│   ├── log/                #   Log 框架测试
│   ├── shell/              #   Shell 框架测试
│   ├── init/               #   Init 框架测试
│   └── integration/        #   集成测试
├── docs/                   # 文档
│   ├── api/                #   Doxygen API 文档
│   ├── sphinx/             #   用户指南（中英文）
│   └── requirements/       #   需求和设计文档
├── scripts/                # 构建和实用脚本
│   ├── building/           #   构建脚本（Python/Bash/Batch）
│   ├── test/               #   测试脚本
│   ├── tools/              #   格式化、清理、文档脚本
│   └── coverage/           #   覆盖率分析脚本
├── cmake/                  # CMake 模块
│   ├── modules/            #   辅助函数
│   └── toolchains/         #   交叉编译工具链
├── vendors/                # 厂商 SDK 和库
│   ├── st/                 #   意法半导体
│   ├── espressif/          #   乐鑫
│   ├── nordic/             #   Nordic 半导体
│   └── arm/                #   ARM CMSIS
├── ext/                    # 外部依赖
│   ├── freertos/           #   FreeRTOS 内核
│   └── googletest/         #   Google Test 框架
└── .github/workflows/      # CI/CD 流水线
    ├── ci.yml              #   统一持续集成
    ├── build-matrix.yml    #   多平台构建矩阵
    ├── docs-build.yml      #   文档构建和部署
    ├── quality-checks.yml  #   代码质量检查
    ├── performance.yml     #   性能测试
    └── release.yml         #   发布自动化
```

## 📚 文档

### 在线文档

访问我们的完整文档站点：**[nexus-platform.github.io/nexus](https://nexus-platform.github.io/nexus/)**

- **English**: https://nexus-platform.github.io/nexus/en/
- **中文**: https://nexus-platform.github.io/nexus/zh_CN/
- **API 参考**: https://nexus-platform.github.io/nexus/api/

### 本地构建文档

```bash
# 安装依赖
pip install sphinx breathe sphinx_rtd_theme

# 构建所有文档（API + 用户指南）
python scripts/tools/docs.py

# 或分别构建：

# API 文档（Doxygen）
doxygen Doxyfile

# 用户指南（Sphinx）
cd docs/sphinx
sphinx-build -b html . _build/html/en                    # 英文
sphinx-build -b html -D language=zh_CN . _build/html/cn  # 中文
```

### 模块文档

每个模块都有完整的文档：

#### HAL（硬件抽象层）
- [📖 概述](hal/docs/README.md)
- [👤 用户指南](hal/docs/USER_GUIDE.md) - 完整的 API 使用和示例
- [🏗️ 设计文档](hal/docs/DESIGN.md) - 架构和实现细节
- [🧪 测试指南](hal/docs/TEST_GUIDE.md) - 测试策略和用例
- [🔧 移植指南](hal/docs/PORTING_GUIDE.md) - 如何移植到新平台
- [🔍 故障排查](hal/docs/TROUBLESHOOTING.md) - 常见问题和解决方案

#### OSAL（操作系统抽象层）
- [📖 概述](osal/docs/README.md)
- [👤 用户指南](osal/docs/USER_GUIDE.md) - 任务、同步、内存管理
- [🏗️ 设计文档](osal/docs/DESIGN.md) - RTOS 适配器架构
- [🧪 测试指南](osal/docs/TEST_GUIDE.md) - 单元测试和集成测试
- [🔧 移植指南](osal/docs/PORTING_GUIDE.md) - 适配新的 RTOS
- [🔍 故障排查](osal/docs/TROUBLESHOOTING.md) - 调试技巧

#### 框架模块
- [⚙️ Config 系统](framework/config/docs/README.md) - 配置管理
- [📝 Log 系统](framework/log/docs/README.md) - 日志框架
- [💻 Shell 系统](framework/shell/docs/README.md) - 命令行 Shell
- [🚀 Init 系统](framework/init/docs/README.md) - 初始化框架

## 🛠️ 开发指南

### 使用 Python 脚本（推荐）

```bash
# 构建
python scripts/building/build.py                 # Debug 构建
python scripts/building/build.py -t release      # Release 构建
python scripts/building/build.py -c              # 清理后构建
python scripts/building/build.py -j 8            # 并行构建（8 个作业）

# 测试
python scripts/test/test.py                      # 运行所有测试
python scripts/test/test.py -f "GPIO*"           # 过滤测试
python scripts/test/test.py -v                   # 详细输出
python scripts/test/test.py --xml report.xml     # 生成 XML 报告

# 格式化代码
python scripts/tools/format.py                   # 格式化所有代码
python scripts/tools/format.py --check           # 仅检查格式

# 清理
python scripts/tools/clean.py                    # 清理构建产物

# 生成文档
python scripts/tools/docs.py                     # 构建所有文档
```

### 代码风格

本项目遵循严格的编码标准：

- **C 标准**: C11
- **C++ 标准**: C++17（测试使用 C++20）
- **行长度**: 最多 80 字符
- **缩进**: 4 个空格（不使用制表符）
- **命名**: 函数和变量使用 snake_case
- **注释**: Doxygen 反斜杠风格 (`\brief`, `\param`)

提交前格式化代码：

```bash
python scripts/tools/format.py
```

### Doxygen 注释风格

使用反斜杠风格的 Doxygen 注释：

```c
/**
 * \file            nx_gpio.h
 * \brief           GPIO 设备接口定义
 * \author          Nexus Team
 */

/**
 * \brief           获取具有写入能力的 GPIO 设备
 * \param[in]       port: GPIO 端口（'A'-'K'）
 * \param[in]       pin: GPIO 引脚号（0-15）
 * \return          GPIO 写入接口指针，失败返回 NULL
 */
nx_gpio_write_t* nx_factory_gpio_write(char port, uint8_t pin);
```

### 运行测试

```bash
# 使用 Python 脚本（推荐）
python scripts/test/test.py                      # 所有测试
python scripts/test/test.py -f "GPIO*"           # 特定套件
python scripts/test/test.py -l unit              # 按标签
python scripts/test/test.py -v                   # 详细输出

# 直接使用 CTest
cd build
ctest -C Release --output-on-failure             # 所有测试
ctest -C Release -R "GPIO*"                      # 特定套件
ctest -C Release -L unit                         # 按标签
ctest -C Release -j8                             # 并行（8 个作业）
```

### 测试统计

当前测试套件包含：

- **总测试数**: 1539+ 个测试
- **HAL 测试**: ~400 个测试（单元 + 属性测试）
- **OSAL 测试**: ~200 个测试
- **Config 测试**: ~300 个测试
- **Log 测试**: ~130 个测试
- **Shell 测试**: ~400 个测试
- **Init 测试**: ~15 个测试
- **集成测试**: ~40 个测试

### 代码覆盖率

生成代码覆盖率报告：

```bash
# Linux/WSL
cd scripts/coverage
./run_coverage_linux.sh

# Windows (PowerShell)
cd scripts\coverage
.\run_coverage_windows.ps1

# 查看报告
# Linux: xdg-open ../../coverage_html/index.html
# Windows: start ..\..\coverage_report\html\index.html
```

**目标**: 原生平台 HAL 实现达到 100% 代码覆盖率。

## 🔄 CI/CD

GitHub Actions 工作流在每次推送和拉取请求时自动运行：

| 工作流 | 说明 | 触发条件 |
|--------|------|---------|
| **ci.yml** | 统一的持续集成流水线 | Push、PR |
| **build-matrix.yml** | 多平台构建测试（Windows、Linux、macOS、ARM） | Push、PR |
| **docs-build.yml** | 构建并部署文档到 GitHub Pages | Push to main |
| **quality-checks.yml** | 代码质量验证和静态分析 | Push、PR |
| **performance.yml** | 性能基准测试和回归测试 | Push to main、手动 |
| **release.yml** | 自动化发布流程和产物发布 | Tag push |

### 模块化架构

CI/CD 系统采用模块化架构，使用可复用的 actions：

- **可复用 Actions** 位于 `.github/actions/`：
  - `setup-build/` - 通用构建环境设置
  - 在多个工作流之间共享以保持一致性

### CI 状态

- ✅ 所有平台构建成功
- ✅ 1539+ 个测试通过
- ✅ 代码覆盖率 > 95%
- ✅ 文档构建成功
- ✅ 质量检查通过

## 🤝 贡献

我们欢迎贡献！请查看 [贡献指南](CONTRIBUTING_CN.md) 了解：

- 开发环境设置
- 代码风格指南
- 测试要求（所有贡献必须包含测试）
- 拉取请求流程
- 文档指南

提交 PR 前的快速检查清单：

- [ ] 代码遵循风格指南（`python scripts/tools/format.py`）
- [ ] 所有测试通过（`python scripts/test/test.py`）
- [ ] 新代码有相应的测试
- [ ] 文档已更新
- [ ] 提交信息遵循约定式提交

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 🌟 社区

- **问题反馈**: [GitHub Issues](https://github.com/nexus-platform/nexus/issues)
- **讨论**: [GitHub Discussions](https://github.com/nexus-platform/nexus/discussions)
- **文档**: [在线文档](https://nexus-platform.github.io/nexus/)
- **更新日志**: [CHANGELOG.md](CHANGELOG.md)

## 🗺️ 路线图

### v0.1.0（当前）

- ✅ HAL 核心功能（GPIO、UART、SPI、I2C、ADC、PWM、CAN）
- ✅ OSAL 核心功能（任务、同步、内存）
- ✅ 框架模块（Config、Log、Shell、Init）
- ✅ STM32F4 平台支持
- ✅ Native 平台测试
- ✅ Kconfig 配置系统
- ✅ 全面测试（1539+ 个测试）
- ✅ 完整的双语文档

### v0.2.0（计划中）

- 🚧 STM32H7 平台支持
- 🚧 GD32 平台支持
- 🚧 DMA 高级功能
- 🚧 低功耗管理
- 🚧 增强的安全特性

### v1.0.0（未来）

- 📋 ESP32 平台支持
- 📋 nRF52 平台支持
- 📋 云端集成（AWS IoT、Azure IoT）
- 📋 TinyML 支持
- 📋 安全启动实现

## 🙏 致谢

感谢所有为 Nexus 项目做出贡献的开发者！

特别感谢：
- FreeRTOS 团队提供的优秀 RTOS
- Google Test 团队提供的测试框架
- Doxygen 和 Sphinx 团队提供的文档工具

---

**由 Nexus 团队用 ❤️ 制作**

*构建嵌入式系统的未来，一次提交一个进步。*
