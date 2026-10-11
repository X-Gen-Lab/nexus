# Nexus 嵌入式软件开发平台

## 产品需求文档 (PRD) V2.0

| 文档信息 | |
|---------|---|
| 文档版本 | v2.0.0 |
| 创建日期 | 2026-01-12 |
| 文档状态 | 正式版 |
| 作者 | Nexus Architecture Team |

---

## 目录

1. [概述](#1-概述)
2. [系统架构](#2-系统架构)
3. [功能需求](#3-功能需求)
4. [非功能需求](#4-非功能需求)
5. [工程目录结构](#5-工程目录结构)
6. [接口规范](#6-接口规范)
7. [代码规范](#7-代码规范)
8. [文档体系](#8-文档体系)
9. [构建系统](#9-构建系统)
10. [测试体系](#10-测试体系)
11. [DevOps与CI/CD](#11-devops与cicd)
12. [工具链集成](#12-工具链集成)
13. [跨平台开发](#13-跨平台开发)
14. [版本管理](#14-版本管理)
15. [附录](#15-附录)

---

## 1. 概述

### 1.1 项目背景

随着物联网、智能设备、工业自动化等领域的快速发展，嵌入式软件开发面临以下挑战：

- **硬件碎片化**：MCU 架构多样（ARM Cortex-M、RISC-V、Xtensa 等）
- **代码复用困难**：缺乏统一的抽象层，应用代码与硬件强耦合
- **开发效率低下**：工具链分散，缺乏标准化的开发流程
- **质量难以保证**：测试框架不完善，代码规范不统一
- **跨平台协作困难**：Windows/Linux/macOS 开发环境不统一

### 1.2 项目目标

Nexus 平台旨在构建一个**世界级的嵌入式软件开发平台**：

| 目标 | 描述 | 量化指标 |
|------|------|----------|
| **高可移植性** | 应用代码一次编写，多平台运行 | 支持 ≥5 种 MCU 架构 |
| **高可扩展性** | 模块化设计，按需裁剪 | 最小系统 < 8KB ROM |
| **高实时性** | 满足硬实时应用需求 | 中断响应 < 1μs |
| **低功耗** | 支持多级功耗管理 | 待机功耗 < 10μA |
| **高安全性** | 端到端安全保障 | 支持安全启动、加密存储 |
| **高开发效率** | 完善的工具链和文档 | 新项目启动 < 30 分钟 |
| **跨平台开发** | 支持主流开发环境 | Windows/Linux/macOS |
| **TDD支持** | 测试驱动开发 | 测试覆盖率 ≥ 90% |

### 1.3 目标用户

| 用户角色 | 使用场景 | 核心需求 |
|----------|----------|----------|
| **应用开发者** | 基于平台开发产品应用 | 简单易用的 API、丰富的示例 |
| **驱动开发者** | 开发新硬件驱动和组件 | 清晰的接口规范、移植指南 |
| **系统集成商** | 集成平台到产品中 | 可裁剪、可配置、文档完善 |
| **平台维护者** | 维护和扩展平台功能 | 模块化设计、完善的测试 |
| **DevOps工程师** | 构建CI/CD流水线 | 自动化构建、测试、部署 |

### 1.4 术语定义

| 术语 | 全称 | 定义 |
|------|------|------|
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层 |
| BSP | Board Support Package | 板级支持包 |
| TDD | Test-Driven Development | 测试驱动开发 |
| CI/CD | Continuous Integration/Deployment | 持续集成/部署 |
| DevOps | Development and Operations | 开发运维一体化 |

---

## 2. 系统架构

### 2.1 分层架构总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐       │
│  │   用户应用   │ │  示例程序   │ │  产品项目   │ │  测试程序   │       │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘       │
├─────────────────────────────────────────────────────────────────────────┤
│                         COMPONENTS LAYER                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 传感器驱动 │ │ 显示驱动  │ │ 协议栈   │ │  算法库  │ │ 工具组件  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
├─────────────────────────────────────────────────────────────────────────┤
│                         MIDDLEWARE LAYER                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 文件系统  │ │ 网络协议栈 │ │ 安全模块  │ │ USB协议栈 │ │ GUI框架  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
├─────────────────────────────────────────────────────────────────────────┤
│                            OSAL LAYER                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 任务管理  │ │ 同步机制  │ │ 消息队列  │ │ 定时器   │ │ 内存管理  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  Adapters: FreeRTOS | RT-Thread | Zephyr | Linux | Baremetal    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────┤
│                             HAL LAYER                                    │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐      │
│  │ GPIO │ │ UART │ │ SPI  │ │ I2C  │ │ ADC  │ │ PWM  │ │Timer │      │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘      │
├─────────────────────────────────────────────────────────────────────────┤
│                          PLATFORM LAYER                                  │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐           │
│  │     STM32       │ │      ESP32      │ │      nRF52      │           │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘           │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 开发工具链架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          DEVELOPMENT TOOLS                               │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        IDE & Editor                              │   │
│  │  VS Code + Extensions | CLion | Eclipse CDT | Keil | IAR        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │ Build System │ │   Testing    │ │Documentation │ │   DevOps     │  │
│  │ CMake/Make/  │ │ GTest/GMock  │ │Doxygen+Sphinx│ │GitHub Actions│  │
│  │   Bazel      │ │ Unity/CTest  │ │  +Breathe    │ │Jenkins/GitLab│  │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘  │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │Code Analysis │ │   Debug      │ │   Flash      │ │  Simulation  │  │
│  │clang-format  │ │ GDB/OpenOCD  │ │ J-Link/ST-   │ │    QEMU      │  │
│  │cppcheck/MISRA│ │ Cortex-Debug │ │ Link/esptool │ │   Renode     │  │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 功能需求

### 3.1 HAL 层功能需求

#### 3.1.1 GPIO 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-GPIO-001 | 支持引脚初始化（输入/输出模式） | P0 | 所有支持平台通过测试 |
| HAL-GPIO-002 | 支持电平读写操作 | P0 | 读写延迟 < 100ns |
| HAL-GPIO-003 | 支持上拉/下拉/浮空配置 | P0 | 配置后电平符合预期 |
| HAL-GPIO-004 | 支持推挽/开漏输出模式 | P1 | 输出模式可切换 |
| HAL-GPIO-005 | 支持中断配置（边沿触发） | P0 | 中断响应 < 1μs |

#### 3.1.2 UART 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-UART-001 | 支持波特率配置（9600-921600） | P0 | 波特率误差 < 2% |
| HAL-UART-002 | 支持数据位/停止位/校验位配置 | P0 | 配置组合正确工作 |
| HAL-UART-003 | 支持阻塞式发送/接收 | P0 | 数据完整无丢失 |
| HAL-UART-004 | 支持非阻塞式发送/接收 | P0 | 回调正确触发 |
| HAL-UART-005 | 支持 DMA 传输模式 | P1 | CPU 占用率降低 50% |

#### 3.1.3 其他 HAL 模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **SPI** | 主机模式、4种SPI模式、DMA | P0 |
| **I2C** | 主机模式、标准/快速模式、设备扫描 | P0 |
| **ADC** | 单次/连续采样、多通道扫描、DMA | P0 |
| **PWM** | 频率/占空比配置、互补输出 | P1 |
| **Timer** | 定时中断、输入捕获、输出比较 | P0 |
| **Flash** | 读写擦除、扇区管理 | P0 |
| **RTC** | 时间设置/读取、闹钟 | P1 |
| **DMA** | 内存到外设、链式传输 | P1 |
| **Watchdog** | 独立/窗口看门狗 | P0 |

### 3.2 OSAL 层功能需求

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **任务管理** | 创建/删除/挂起/恢复、优先级配置 | P0 |
| **互斥锁** | 普通/递归互斥锁、优先级继承 | P0 |
| **信号量** | 二值/计数信号量 | P0 |
| **消息队列** | 阻塞/非阻塞收发、ISR安全 | P0 |
| **事件标志** | 多事件等待、AND/OR模式 | P1 |
| **定时器** | 软件定时器、单次/周期模式 | P0 |
| **内存管理** | 动态分配、内存池、统计 | P0 |

### 3.3 中间件层功能需求

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **文件系统** | FAT/LittleFS、文件操作、掉电保护 | P1 |
| **网络协议栈** | TCP/IP、Socket API、MQTT | P1 |
| **安全模块** | AES/SHA/RSA、安全启动、TLS | P1 |
| **OTA升级** | 固件下载、验证、双分区切换 | P1 |

---

## 4. 非功能需求

### 4.1 性能需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-PERF-001 | 中断响应时间 | < 1μs |
| NFR-PERF-002 | 任务上下文切换时间 | < 5μs |
| NFR-PERF-003 | HAL API 调用开销 | < 100 cycles |
| NFR-PERF-004 | 系统启动时间 | < 100ms |

### 4.2 资源需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-RES-001 | 最小 ROM 占用（HAL+OSAL） | < 8KB |
| NFR-RES-002 | 最小 RAM 占用（HAL+OSAL） | < 2KB |
| NFR-RES-003 | 典型 ROM 占用（含中间件） | < 64KB |

### 4.3 可靠性需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-REL-001 | 系统连续运行时间 | > 30 天 |
| NFR-REL-002 | 内存泄漏 | 0 |
| NFR-REL-003 | 单元测试覆盖率 | ≥ 90% |

### 4.4 跨平台需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-CROSS-001 | 开发主机操作系统 | Windows/Linux/macOS |
| NFR-CROSS-002 | 支持 MCU 架构 | ≥ 5 种 |
| NFR-CROSS-003 | 支持 RTOS | ≥ 4 种 |
| NFR-CROSS-004 | 新平台移植时间 | < 1 周 |

---

## 5. 工程目录结构

### 5.1 顶层目录结构

```
nexus/
├── .github/                    # GitHub 配置和工作流
│   ├── workflows/              # CI/CD 工作流
│   │   ├── build.yml           # 多平台构建
│   │   ├── test.yml            # 测试流水线
│   │   ├── docs.yml            # 文档生成
│   │   └── release.yml         # 发布流程
│   └── ISSUE_TEMPLATE/
│
├── .vscode/                    # VS Code 配置
│   ├── settings.json           # 工作区设置
│   ├── tasks.json              # 构建任务
│   ├── launch.json             # 调试配置
│   ├── extensions.json         # 推荐扩展
│   └── *.code-snippets         # 代码片段
│
├── build/                      # 构建输出目录（gitignore）
│
├── cmake/                      # CMake 模块
│   ├── toolchains/             # 工具链文件
│   │   ├── arm-none-eabi.cmake
│   │   ├── xtensa-esp32.cmake
│   │   └── host-native.cmake
│   ├── modules/                # CMake 模块
│   │   ├── FindHAL.cmake
│   │   ├── FindOSAL.cmake
│   │   └── AddComponent.cmake
│   └── platforms/              # 平台配置
│
├── bazel/                      # Bazel 构建配置
│   ├── BUILD.bazel
│   ├── WORKSPACE
│   └── toolchains/
│
├── hal/                        # 硬件抽象层
├── osal/                       # 操作系统抽象层
├── middleware/                 # 中间件
├── components/                 # 可复用组件
├── boards/                     # 板级支持包
├── platforms/                  # 平台支持
├── drivers/                    # 芯片驱动
│
├── applications/               # 应用程序
│   ├── demos/                  # 示例应用
│   └── projects/               # 实际项目
│
├── tests/                      # 测试代码
│   ├── unit/                   # 单元测试 (GTest)
│   ├── integration/            # 集成测试
│   ├── mocks/                  # Mock 对象 (GMock)
│   └── fixtures/               # 测试夹具
│
├── docs/                       # 文档
│   ├── doxygen/                # Doxygen 配置
│   ├── sphinx/                 # Sphinx 配置
│   ├── api/                    # API 文档（生成）
│   ├── guides/                 # 开发指南
│   └── requirements/           # 需求文档
│
├── tools/                      # 开发工具
│   ├── codegen/                # 代码生成器
│   ├── flash/                  # 烧录工具
│   ├── analysis/               # 代码分析
│   └── scripts/                # 实用脚本
│
├── third_party/                # 第三方库
│   ├── googletest/             # Google Test
│   ├── freertos/               # FreeRTOS
│   └── ...
│
├── .clang-format               # 代码格式配置
├── .clang-tidy                 # 静态分析配置
├── .gitignore
├── CMakeLists.txt              # 根 CMake 配置
├── Makefile                    # 顶层 Makefile
├── BUILD.bazel                 # Bazel 构建文件
├── Doxyfile                    # Doxygen 配置
├── requirements.txt            # Python 依赖
├── README.md
├── LICENSE
├── CONTRIBUTING.md
└── CHANGELOG.md
```

---

## 6. 接口规范

### 6.1 HAL 接口设计模式

采用**函数指针表**模式实现硬件抽象：

```c
/**
 * \file            hal_gpio.h
 * \brief           GPIO Hardware Abstraction Layer Interface
 * \author          Nexus Team (nexus@example.com)
 * \version         1.0.0
 * \date            2026-01-12
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           GPIO direction enumeration
 */
typedef enum {
    HAL_GPIO_DIR_INPUT  = 0,    /**< Input mode */
    HAL_GPIO_DIR_OUTPUT = 1     /**< Output mode */
} hal_gpio_dir_t;

/**
 * \brief           GPIO configuration structure
 */
typedef struct {
    hal_gpio_dir_t         direction;      /**< Pin direction */
    hal_gpio_pull_t        pull;           /**< Pull-up/down config */
    hal_gpio_output_mode_t output_mode;    /**< Output mode */
    hal_gpio_level_t       init_level;     /**< Initial level */
} hal_gpio_config_t;

/**
 * \brief           GPIO operations interface
 */
typedef struct {
    /**
     * \brief           Initialize GPIO pin
     * \param[in]       port: GPIO port number
     * \param[in]       pin: GPIO pin number
     * \param[in]       config: Configuration parameters
     * \return          HAL_OK on success, error code otherwise
     */
    hal_status_t (*init)(hal_gpio_port_t port,
                         hal_gpio_pin_t pin,
                         const hal_gpio_config_t* config);

    /**
     * \brief           Write GPIO pin level
     * \param[in]       port: GPIO port number
     * \param[in]       pin: GPIO pin number
     * \param[in]       level: Level to write
     * \return          HAL_OK on success, error code otherwise
     */
    hal_status_t (*write)(hal_gpio_port_t port,
                          hal_gpio_pin_t pin,
                          hal_gpio_level_t level);

    /**
     * \brief           Read GPIO pin level
     * \param[in]       port: GPIO port number
     * \param[in]       pin: GPIO pin number
     * \param[out]      level: Pointer to store read level
     * \return          HAL_OK on success, error code otherwise
     */
    hal_status_t (*read)(hal_gpio_port_t port,
                         hal_gpio_pin_t pin,
                         hal_gpio_level_t* level);

    /**
     * \brief           Toggle GPIO pin level
     * \param[in]       port: GPIO port number
     * \param[in]       pin: GPIO pin number
     * \return          HAL_OK on success, error code otherwise
     */
    hal_status_t (*toggle)(hal_gpio_port_t port, hal_gpio_pin_t pin);

} hal_gpio_interface_t;

/**
 * \brief           Get GPIO interface instance
 * \return          Pointer to GPIO interface, NULL if not available
 */
const hal_gpio_interface_t* hal_gpio_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
```

### 6.2 错误码规范

```c
/**
 * \brief           HAL status codes
 */
typedef enum {
    /* Success */
    HAL_OK                  = 0,    /**< Operation successful */

    /* General errors (1-99) */
    HAL_ERROR               = 1,    /**< Generic error */
    HAL_ERROR_INVALID_PARAM = 2,    /**< Invalid parameter */
    HAL_ERROR_NULL_POINTER  = 3,    /**< Null pointer */
    HAL_ERROR_NOT_INIT      = 4,    /**< Not initialized */
    HAL_ERROR_ALREADY_INIT  = 5,    /**< Already initialized */
    HAL_ERROR_NOT_SUPPORTED = 6,    /**< Not supported */

    /* Resource errors (100-199) */
    HAL_ERROR_NO_MEMORY     = 100,  /**< Out of memory */
    HAL_ERROR_BUSY          = 102,  /**< Resource busy */

    /* Timeout errors (200-299) */
    HAL_ERROR_TIMEOUT       = 200,  /**< Operation timeout */

    /* IO errors (300-399) */
    HAL_ERROR_IO            = 300,  /**< IO error */

} hal_status_t;
```

---

## 7. 代码规范

### 7.1 代码格式规范

使用 **clang-format** 进行代码格式化，配置文件 `.clang-format` 主要规则：

| 规则 | 配置值 | 说明 |
|------|--------|------|
| `ColumnLimit` | 80 | 每行最大字符数 |
| `IndentWidth` | 4 | 缩进宽度 |
| `UseTab` | Never | 不使用 Tab |
| `PointerAlignment` | Left | 指针符号靠左 |
| `BreakBeforeBraces` | Attach | 花括号不换行 |
| `AllowShortFunctionsOnASingleLine` | None | 短函数不单行 |
| `AllowShortIfStatementsOnASingleLine` | Never | 短if不单行 |
| `AlignConsecutiveMacros` | true | 对齐连续宏定义 |
| `AlignConsecutiveBitFields` | true | 对齐连续位域 |
| `SortIncludes` | CaseSensitive | 排序头文件 |

### 7.2 Doxygen 注释规范

采用 **反斜杠风格** 的 Doxygen 注释：

#### 7.2.1 文件头注释

```c
/**
 * \file            hal_gpio.c
 * \brief           GPIO Hardware Abstraction Layer Implementation
 * \author          Author Name (author@example.com)
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements the GPIO HAL interface for
 *                  STM32F4xx series microcontrollers.
 */
```

#### 7.2.2 函数注释

```c
/**
 * \brief           Initialize GPIO pin with specified configuration
 *
 * \details         This function configures the GPIO pin according to
 *                  the provided configuration structure. It sets up
 *                  direction, pull-up/down, output mode, and initial level.
 *
 * \param[in]       port: GPIO port number (0-7)
 * \param[in]       pin: GPIO pin number (0-15)
 * \param[in]       config: Pointer to configuration structure
 *
 * \return          Operation status
 * \retval          HAL_OK: Success
 * \retval          HAL_ERROR_INVALID_PARAM: Invalid parameter
 * \retval          HAL_ERROR_NULL_POINTER: Null config pointer
 *
 * \note            Clock for the GPIO port must be enabled before calling
 * \warning         Re-initializing a pin will override previous configuration
 *
 * \code{.c}
 * hal_gpio_config_t config = {
 *     .direction = HAL_GPIO_DIR_OUTPUT,
 *     .pull = HAL_GPIO_PULL_NONE,
 *     .output_mode = HAL_GPIO_OUTPUT_PP,
 *     .init_level = HAL_GPIO_LEVEL_LOW
 * };
 * hal_status_t status = hal_gpio_init(0, 5, &config);
 * \endcode
 *
 * \see             hal_gpio_deinit
 * \see             hal_gpio_config_t
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port,
                           hal_gpio_pin_t pin,
                           const hal_gpio_config_t* config);
```

#### 7.2.3 结构体/枚举注释

```c
/**
 * \brief           GPIO configuration structure
 *
 * \details         Contains all parameters needed to configure a GPIO pin.
 */
typedef struct {
    hal_gpio_dir_t direction;           /**< Pin direction (input/output) */
    hal_gpio_pull_t pull;               /**< Pull-up/down configuration */
    hal_gpio_output_mode_t output_mode; /**< Output mode (push-pull/open-drain) */
    hal_gpio_level_t init_level;        /**< Initial output level */
} hal_gpio_config_t;

/**
 * \brief           GPIO direction enumeration
 */
typedef enum {
    HAL_GPIO_DIR_INPUT  = 0,            /**< Input mode */
    HAL_GPIO_DIR_OUTPUT = 1             /**< Output mode */
} hal_gpio_dir_t;
```

### 7.3 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 文件名 | 小写 + 下划线 | `hal_gpio.c`, `osal_task.h` |
| 函数名 | 小写 + 下划线 | `hal_gpio_init()` |
| 类型名 | 小写 + 下划线 + `_t` | `hal_gpio_config_t` |
| 枚举值 | 大写 + 下划线 | `HAL_GPIO_DIR_INPUT` |
| 宏定义 | 大写 + 下划线 | `HAL_GPIO_PORT_MAX` |
| 全局变量 | `g_` 前缀 | `g_hal_initialized` |
| 静态变量 | `s_` 前缀 | `s_gpio_callbacks` |
| 局部变量 | 小写 + 下划线 | `pin_level` |

### 7.4 VS Code 配置

#### 7.4.1 Doxygen 生成器配置

```json
// .vscode/settings.json
{
    "C_Cpp.doxygen.generatedStyle": "/**",
    "doxdocgen.file.fileTemplate": "\\file            {name}",
    "doxdocgen.generic.briefTemplate": "\\brief           {text}",
    "doxdocgen.generic.paramTemplate": "\\param[in]       {param}: ",
    "doxdocgen.generic.returnTemplate": "\\return          {type} ",
    "doxdocgen.generic.authorTag": "\\author          {author} ({email})",
    "doxdocgen.generic.dateTemplate": "\\date            {date}",
    "doxdocgen.file.versionTag": "\\version         0.0.1",
    "doxdocgen.generic.useGitUserName": true,
    "doxdocgen.generic.useGitUserEmail": true,
    "doxdocgen.cpp.tparamTemplate": "\\tparam          {param} ",
    "doxdocgen.generic.commandSuggestionAddPrefix": true
}
```

### 7.5 SOLID 原则应用

#### 7.5.1 单一职责原则 (SRP)

```c
/* ✅ 正确：每个函数只做一件事 */
hal_status_t sensor_read(sensor_data_t* data);
hal_status_t data_process(const sensor_data_t* raw, processed_data_t* result);
hal_status_t network_send(const processed_data_t* data);
```

#### 7.5.2 开闭原则 (OCP)

```c
/* ✅ 通过接口扩展，而非修改现有代码 */
typedef struct {
    hal_status_t (*init)(void* config);
    hal_status_t (*read)(void* data);
    hal_status_t (*deinit)(void);
} sensor_interface_t;

/* 不同传感器实现相同接口 */
extern const sensor_interface_t bme280_interface;
extern const sensor_interface_t mpu6050_interface;
```

#### 7.5.3 依赖倒置原则 (DIP)

```c
/* ✅ 高层模块依赖抽象接口 */
typedef struct {
    const comm_interface_t* comm;  /* 依赖注入 */
} app_context_t;

hal_status_t app_send_data(app_context_t* ctx, const uint8_t* data, size_t len) {
    return ctx->comm->send(data, len);
}
```

---

## 8. 文档体系

### 8.1 文档工具链

采用 **Git + Doxygen + Sphinx + Breathe** 构建完整的文档体系：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DOCUMENTATION PIPELINE                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Source  │───▶│ Doxygen  │───▶│ Breathe  │───▶│  Sphinx  │          │
│  │   Code   │    │   XML    │    │  Bridge  │    │   HTML   │          │
│  │ (C/C++)  │    │          │    │          │    │   PDF    │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│       │                                               │                  │
│       │          ┌──────────┐                        │                  │
│       └─────────▶│   RST    │────────────────────────┘                  │
│                  │Markdown  │                                            │
│                  │  Docs    │                                            │
│                  └──────────┘                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Doxygen 配置

```
# Doxyfile
PROJECT_NAME           = "Nexus Embedded Platform"
PROJECT_NUMBER         = "2.0.0"
PROJECT_BRIEF          = "World-class Embedded Software Development Platform"

# 输入配置
INPUT                  = hal/include osal/include components middleware
RECURSIVE              = YES
FILE_PATTERNS          = *.h *.c *.md
EXCLUDE_PATTERNS       = */test/* */third_party/*

# 输出配置
OUTPUT_DIRECTORY       = docs/build/doxygen
GENERATE_HTML          = YES
GENERATE_XML           = YES
XML_OUTPUT             = xml

# 提取配置
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
EXTRACT_PRIVATE        = NO

# 图表配置
HAVE_DOT               = YES
DOT_IMAGE_FORMAT       = svg
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
CLASS_DIAGRAMS         = YES

# 警告配置
WARN_IF_UNDOCUMENTED   = YES
WARN_NO_PARAMDOC       = YES
WARN_AS_ERROR          = NO
```

### 8.3 Sphinx 配置

```python
# docs/sphinx/conf.py
project = 'Nexus Embedded Platform'
copyright = '2026, Nexus Team'
author = 'Nexus Team'
version = '2.0.0'

# 扩展
extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.graphviz',
    'sphinx_rtd_theme',
    'myst_parser',
]

# Breathe 配置
breathe_projects = {
    "nexus": "../build/doxygen/xml"
}
breathe_default_project = "nexus"
breathe_default_members = ('members', 'undoc-members')

# 主题配置
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'navigation_depth': 4,
    'collapse_navigation': False,
}

# Markdown 支持
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}
```

### 8.4 文档目录结构

```
docs/
├── doxygen/                    # Doxygen 配置
│   ├── Doxyfile               # 主配置文件
│   └── custom/                # 自定义样式
│
├── sphinx/                     # Sphinx 配置
│   ├── conf.py                # Sphinx 配置
│   ├── index.rst              # 文档首页
│   ├── _static/               # 静态资源
│   └── _templates/            # 模板
│
├── source/                     # 文档源文件
│   ├── getting_started/       # 入门指南
│   │   ├── installation.md
│   │   ├── first_project.md
│   │   └── quick_start.md
│   ├── api/                   # API 参考
│   │   ├── hal/
│   │   ├── osal/
│   │   └── components/
│   ├── guides/                # 开发指南
│   │   ├── porting.md
│   │   ├── testing.md
│   │   └── tdd.md
│   ├── design/                # 设计文档
│   │   ├── architecture.md
│   │   └── interfaces.md
│   └── tutorials/             # 教程
│
├── build/                      # 构建输出（gitignore）
│   ├── doxygen/
│   │   ├── html/
│   │   └── xml/
│   └── sphinx/
│       ├── html/
│       └── pdf/
│
└── requirements/               # 需求文档
    ├── NEXUS_PRD.md
    └── NEXUS_SUMMARY.md
```

### 8.5 文档构建命令

```bash
# 生成 Doxygen 文档
doxygen docs/doxygen/Doxyfile

# 生成 Sphinx HTML 文档
cd docs/sphinx
make html

# 生成 Sphinx PDF 文档
make latexpdf

# 一键构建所有文档
python tools/scripts/build_docs.py --all
```

### 8.6 文档 CI 流水线

```yaml
# .github/workflows/docs.yml
name: Documentation

on:
  push:
    branches: [main, develop]
    paths:
      - 'docs/**'
      - 'hal/include/**'
      - 'osal/include/**'

jobs:
  build-docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get install -y doxygen graphviz
          pip install sphinx breathe sphinx-rtd-theme myst-parser

      - name: Build Doxygen
        run: doxygen docs/doxygen/Doxyfile

      - name: Build Sphinx
        run: |
          cd docs/sphinx
          make html

      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: docs/build/sphinx/html
```

---

## 9. 构建系统

### 9.1 多构建系统支持

Nexus 平台支持三种主流构建系统：

| 构建系统 | 适用场景 | 优势 |
|----------|----------|------|
| **CMake** | 主要构建系统 | 跨平台、IDE集成好 |
| **Make** | 简单项目、快速构建 | 轻量、无依赖 |
| **Bazel** | 大型项目、精确依赖 | 增量构建、可重现 |

### 9.2 CMake 构建系统

#### 9.2.1 根 CMakeLists.txt

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 项目定义
project(nexus
    VERSION 2.0.0
    DESCRIPTION "Nexus Embedded Software Development Platform"
    LANGUAGES C CXX ASM
)

# 选项
option(NEXUS_BUILD_TESTS "Build unit tests" ON)
option(NEXUS_BUILD_DOCS "Build documentation" OFF)
option(NEXUS_USE_GTEST "Use Google Test framework" ON)

# 平台选择
set(NEXUS_PLATFORM "stm32f4" CACHE STRING "Target platform")
set_property(CACHE NEXUS_PLATFORM PROPERTY STRINGS
    stm32f4 stm32h7 esp32 nrf52 linux
)

# OSAL 后端选择
set(NEXUS_OSAL_BACKEND "freertos" CACHE STRING "OSAL backend")
set_property(CACHE NEXUS_OSAL_BACKEND PROPERTY STRINGS
    freertos rtthread zephyr linux baremetal
)

# 包含工具链
if(NOT CMAKE_CROSSCOMPILING)
    include(cmake/toolchains/host-native.cmake)
else()
    include(cmake/toolchains/${NEXUS_PLATFORM}.cmake)
endif()

# 包含模块
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(
    -Wall -Wextra -Werror
    -ffunction-sections -fdata-sections
)

# 子目录
add_subdirectory(hal)
add_subdirectory(osal)
add_subdirectory(middleware)
add_subdirectory(components)

# 测试
if(NEXUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 文档
if(NEXUS_BUILD_DOCS)
    add_subdirectory(docs)
endif()
```

#### 9.2.2 工具链配置

```cmake
# cmake/toolchains/arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# Cortex-M4 with FPU
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-specs=nano.specs -specs=nosys.specs -Wl,--gc-sections"
)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

### 9.3 Makefile 构建系统

```makefile
# Makefile
# Nexus Embedded Platform - Top Level Makefile

# 配置
PLATFORM ?= stm32f4
OSAL_BACKEND ?= freertos
BUILD_TYPE ?= debug

# 目录
BUILD_DIR := build/$(PLATFORM)/$(BUILD_TYPE)
SRC_DIRS := hal osal middleware components

# 工具链
include make/toolchains/$(PLATFORM).mk

# 编译选项
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections

ifeq ($(BUILD_TYPE),debug)
    CFLAGS += -Og -g3 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

# 源文件
SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# 目标
.PHONY: all clean flash test docs

all: $(BUILD_DIR)/nexus.elf

$(BUILD_DIR)/nexus.elf: $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
	rm -rf build/

flash: $(BUILD_DIR)/nexus.elf
	$(FLASH_TOOL) $<

test:
	$(MAKE) -C tests

docs:
	doxygen Doxyfile

-include $(DEPS)
```

### 9.4 Bazel 构建系统

#### 9.4.1 WORKSPACE

```python
# WORKSPACE
workspace(name = "nexus")

# 规则
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Google Test
http_archive(
    name = "com_google_googletest",
    urls = ["https://github.com/google/googletest/archive/v1.14.0.zip"],
    strip_prefix = "googletest-1.14.0",
)

# 工具链
register_toolchains(
    "//bazel/toolchains:arm_none_eabi_toolchain",
)
```

#### 9.4.2 BUILD.bazel

```python
# BUILD.bazel
load("//bazel:nexus.bzl", "nexus_library", "nexus_binary")

# HAL 库
nexus_library(
    name = "hal",
    srcs = glob(["hal/src/**/*.c"]),
    hdrs = glob(["hal/include/**/*.h"]),
    includes = ["hal/include"],
    visibility = ["//visibility:public"],
)

# OSAL 库
nexus_library(
    name = "osal",
    srcs = glob(["osal/src/**/*.c"]),
    hdrs = glob(["osal/include/**/*.h"]),
    includes = ["osal/include"],
    deps = [":hal"],
    visibility = ["//visibility:public"],
)

# 测试
cc_test(
    name = "hal_test",
    srcs = glob(["tests/unit/hal/*.cpp"]),
    deps = [
        ":hal",
        "@com_google_googletest//:gtest_main",
    ],
)
```

### 9.5 构建命令汇总

```bash
# CMake 构建
mkdir build && cd build
cmake -DNEXUS_PLATFORM=stm32f4 -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)

# Make 构建
make PLATFORM=stm32f4 BUILD_TYPE=debug -j$(nproc)

# Bazel 构建
bazel build //...
bazel test //tests/...

# 跨平台构建
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake ..
```

---

## 10. 测试体系

### 10.1 测试框架选型

| 框架 | 用途 | 说明 |
|------|------|------|
| **Google Test** | 单元测试 | C++ 测试框架，功能强大 |
| **Google Mock** | Mock 对象 | 与 GTest 配合使用 |
| **Unity** | 嵌入式单元测试 | 纯 C 实现，资源占用小 |
| **CTest** | 测试运行器 | CMake 集成测试 |

### 10.2 测试驱动开发 (TDD)

#### 10.2.1 TDD 流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          TDD CYCLE                                       │
│                                                                          │
│     ┌─────────┐         ┌─────────┐         ┌─────────┐                │
│     │  RED    │────────▶│  GREEN  │────────▶│REFACTOR │                │
│     │ (Fail)  │         │ (Pass)  │         │(Improve)│                │
│     └─────────┘         └─────────┘         └─────────┘                │
│          │                                        │                      │
│          └────────────────────────────────────────┘                      │
│                                                                          │
│  1. RED:      Write a failing test first                                │
│  2. GREEN:    Write minimal code to pass the test                       │
│  3. REFACTOR: Improve code while keeping tests green                    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

#### 10.2.2 TDD 示例

```cpp
// tests/unit/hal/test_hal_gpio.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hal/hal_gpio.h"
#include "mocks/mock_hal_gpio.h"

using namespace testing;

/**
 * \brief           GPIO Test Fixture
 */
class HalGpioTest : public Test {
protected:
    void SetUp() override {
        // 初始化 Mock
        mock_hal_gpio_init();
    }

    void TearDown() override {
        // 清理 Mock
        mock_hal_gpio_deinit();
    }
};

/**
 * \brief           Test GPIO initialization with valid parameters
 *
 * \details         RED: Write test first
 *                  GREEN: Implement hal_gpio_init()
 *                  REFACTOR: Optimize implementation
 */
TEST_F(HalGpioTest, InitWithValidConfig_ReturnsOk) {
    // Arrange
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT,
        .pull = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .init_level = HAL_GPIO_LEVEL_LOW
    };

    // Act
    hal_status_t status = hal_gpio_init(0, 5, &config);

    // Assert
    EXPECT_EQ(HAL_OK, status);
}

/**
 * \brief           Test GPIO initialization with null config
 */
TEST_F(HalGpioTest, InitWithNullConfig_ReturnsError) {
    // Act
    hal_status_t status = hal_gpio_init(0, 5, nullptr);

    // Assert
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, status);
}

/**
 * \brief           Test GPIO write operation
 */
TEST_F(HalGpioTest, WriteHigh_SetsCorrectLevel) {
    // Arrange
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT
    };
    hal_gpio_init(0, 5, &config);

    // Act
    hal_status_t status = hal_gpio_write(0, 5, HAL_GPIO_LEVEL_HIGH);

    // Assert
    EXPECT_EQ(HAL_OK, status);

    hal_gpio_level_t level;
    hal_gpio_read(0, 5, &level);
    EXPECT_EQ(HAL_GPIO_LEVEL_HIGH, level);
}

/**
 * \brief           Test GPIO toggle operation
 */
TEST_F(HalGpioTest, Toggle_InvertsLevel) {
    // Arrange
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT,
        .init_level = HAL_GPIO_LEVEL_LOW
    };
    hal_gpio_init(0, 5, &config);

    // Act
    hal_gpio_toggle(0, 5);

    // Assert
    hal_gpio_level_t level;
    hal_gpio_read(0, 5, &level);
    EXPECT_EQ(HAL_GPIO_LEVEL_HIGH, level);
}
```

### 10.3 Mock 对象

```cpp
// tests/mocks/mock_hal_gpio.h
#ifndef MOCK_HAL_GPIO_H
#define MOCK_HAL_GPIO_H

#include <gmock/gmock.h>
#include "hal/hal_gpio.h"

/**
 * \brief           Mock class for HAL GPIO
 */
class MockHalGpio {
public:
    MOCK_METHOD(hal_status_t, init,
                (hal_gpio_port_t port, hal_gpio_pin_t pin,
                 const hal_gpio_config_t* config));

    MOCK_METHOD(hal_status_t, write,
                (hal_gpio_port_t port, hal_gpio_pin_t pin,
                 hal_gpio_level_t level));

    MOCK_METHOD(hal_status_t, read,
                (hal_gpio_port_t port, hal_gpio_pin_t pin,
                 hal_gpio_level_t* level));

    MOCK_METHOD(hal_status_t, toggle,
                (hal_gpio_port_t port, hal_gpio_pin_t pin));
};

// 全局 Mock 实例
extern MockHalGpio* g_mock_hal_gpio;

// Mock 初始化/清理
void mock_hal_gpio_init(void);
void mock_hal_gpio_deinit(void);

#endif /* MOCK_HAL_GPIO_H */
```

### 10.4 测试目录结构

```
tests/
├── unit/                       # 单元测试
│   ├── hal/                    # HAL 单元测试
│   │   ├── test_hal_gpio.cpp
│   │   ├── test_hal_uart.cpp
│   │   ├── test_hal_spi.cpp
│   │   └── CMakeLists.txt
│   ├── osal/                   # OSAL 单元测试
│   │   ├── test_osal_task.cpp
│   │   ├── test_osal_mutex.cpp
│   │   └── test_osal_queue.cpp
│   └── components/             # 组件单元测试
│
├── integration/                # 集成测试
│   ├── hal_osal/               # HAL+OSAL 集成
│   └── middleware/             # 中间件集成
│
├── mocks/                      # Mock 对象
│   ├── mock_hal_gpio.h
│   ├── mock_hal_gpio.cpp
│   ├── mock_hal_uart.h
│   └── mock_osal_task.h
│
├── fixtures/                   # 测试夹具
│   ├── test_data.h
│   └── test_helpers.h
│
├── CMakeLists.txt              # 测试 CMake 配置
└── main.cpp                    # 测试入口
```

### 10.5 测试 CMake 配置

```cmake
# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 查找 Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# 测试可执行文件
add_executable(nexus_tests
    main.cpp
    unit/hal/test_hal_gpio.cpp
    unit/hal/test_hal_uart.cpp
    unit/osal/test_osal_task.cpp
    mocks/mock_hal_gpio.cpp
)

target_include_directories(nexus_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/osal/include
)

target_link_libraries(nexus_tests
    GTest::gtest
    GTest::gmock
    hal
    osal
)

# 注册测试
gtest_discover_tests(nexus_tests)

# 覆盖率
if(NEXUS_COVERAGE)
    target_compile_options(nexus_tests PRIVATE --coverage)
    target_link_options(nexus_tests PRIVATE --coverage)
endif()
```

### 10.6 测试覆盖率要求

| 模块 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
|------|----------|------------|------------|
| HAL 核心 | ≥ 90% | ≥ 80% | 100% |
| OSAL 核心 | ≥ 90% | ≥ 80% | 100% |
| 中间件 | ≥ 80% | ≥ 70% | ≥ 95% |
| 组件 | ≥ 80% | ≥ 70% | ≥ 95% |

---

## 11. DevOps与CI/CD

### 11.1 CI/CD 架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          CI/CD PIPELINE                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │   Code   │───▶│  Build   │───▶│   Test   │───▶│  Deploy  │          │
│  │  Commit  │    │  Stage   │    │  Stage   │    │  Stage   │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│       │              │               │               │                   │
│       ▼              ▼               ▼               ▼                   │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Lint    │    │ Compile  │    │   Unit   │    │ Release  │          │
│  │  Format  │    │ All Plat │    │  Tests   │    │ Artifact │          │
│  │  Check   │    │          │    │          │    │          │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│       │              │               │               │                   │
│       ▼              ▼               ▼               ▼                   │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Static  │    │   Size   │    │Coverage  │    │   Docs   │          │
│  │ Analysis │    │  Report  │    │  Report  │    │ Publish  │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 11.2 GitHub Actions 工作流

#### 11.2.1 主构建流水线

```yaml
# .github/workflows/build.yml
name: Build

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

env:
  BUILD_TYPE: Release

jobs:
  # 代码检查
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check code format
        run: |
          find . -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror

      - name: Run cppcheck
        run: |
          cppcheck --enable=all --error-exitcode=1 \
            --suppress=missingIncludeSystem \
            hal/ osal/ middleware/ components/

  # 多平台构建
  build:
    needs: lint
    strategy:
      matrix:
        platform: [stm32f4, stm32h7, esp32, nrf52, linux]
        os: [ubuntu-latest, windows-latest, macos-latest]
        exclude:
          - platform: esp32
            os: windows-latest
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install toolchain (Linux)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc-arm-none-eabi cmake ninja-build

      - name: Install toolchain (Windows)
        if: runner.os == 'Windows'
        run: |
          choco install cmake ninja arm-none-eabi-gcc

      - name: Install toolchain (macOS)
        if: runner.os == 'macOS'
        run: |
          brew install cmake ninja arm-none-eabi-gcc

      - name: Configure CMake
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=${{ env.BUILD_TYPE }} \
            -DNEXUS_PLATFORM=${{ matrix.platform }}

      - name: Build
        run: cmake --build build --parallel

      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: nexus-${{ matrix.platform }}-${{ matrix.os }}
          path: build/*.elf
```

#### 11.2.2 测试流水线

```yaml
# .github/workflows/test.yml
name: Test

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build lcov

      - name: Configure with coverage
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DNEXUS_BUILD_TESTS=ON \
            -DNEXUS_COVERAGE=ON \
            -DNEXUS_PLATFORM=linux

      - name: Build tests
        run: cmake --build build --target nexus_tests

      - name: Run tests
        run: ctest --test-dir build --output-on-failure

      - name: Generate coverage report
        run: |
          lcov --capture --directory build --output-file coverage.info
          lcov --remove coverage.info '/usr/*' --output-file coverage.info
          lcov --list coverage.info

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v3
        with:
          files: coverage.info
          fail_ci_if_error: true
```

#### 11.2.3 发布流水线

```yaml
# .github/workflows/release.yml
name: Release

on:
  push:
    tags:
      - 'v*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Build all platforms
        run: |
          for platform in stm32f4 stm32h7 esp32 nrf52; do
            cmake -B build-$platform \
              -DCMAKE_BUILD_TYPE=Release \
              -DNEXUS_PLATFORM=$platform
            cmake --build build-$platform --parallel
          done

      - name: Create release package
        run: |
          mkdir -p release
          cp -r hal/ osal/ middleware/ components/ release/
          cp -r docs/ release/
          tar -czvf nexus-${{ github.ref_name }}.tar.gz release/

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          files: nexus-${{ github.ref_name }}.tar.gz
          generate_release_notes: true
```

### 11.3 Jenkins 流水线

```groovy
// Jenkinsfile
pipeline {
    agent any

    environment {
        NEXUS_VERSION = sh(script: 'git describe --tags --always', returnStdout: true).trim()
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
                sh 'git submodule update --init --recursive'
            }
        }

        stage('Lint') {
            parallel {
                stage('Format Check') {
                    steps {
                        sh 'find . -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror'
                    }
                }
                stage('Static Analysis') {
                    steps {
                        sh 'cppcheck --enable=all --xml --xml-version=2 hal/ osal/ 2> cppcheck.xml'
                    }
                    post {
                        always {
                            recordIssues tools: [cppCheck(pattern: 'cppcheck.xml')]
                        }
                    }
                }
            }
        }

        stage('Build') {
            matrix {
                axes {
                    axis {
                        name 'PLATFORM'
                        values 'stm32f4', 'stm32h7', 'esp32', 'nrf52'
                    }
                }
                stages {
                    stage('Build Platform') {
                        steps {
                            sh """
                                cmake -B build-${PLATFORM} \
                                    -DCMAKE_BUILD_TYPE=Release \
                                    -DNEXUS_PLATFORM=${PLATFORM}
                                cmake --build build-${PLATFORM} --parallel
                            """
                        }
                    }
                }
            }
        }

        stage('Test') {
            steps {
                sh '''
                    cmake -B build-test -DNEXUS_BUILD_TESTS=ON -DNEXUS_PLATFORM=linux
                    cmake --build build-test --target nexus_tests
                    cd build-test && ctest --output-on-failure
                '''
            }
            post {
                always {
                    junit 'build-test/test-results.xml'
                }
            }
        }

        stage('Documentation') {
            steps {
                sh 'doxygen Doxyfile'
                sh 'cd docs/sphinx && make html'
            }
            post {
                success {
                    publishHTML(target: [
                        reportDir: 'docs/build/sphinx/html',
                        reportFiles: 'index.html',
                        reportName: 'API Documentation'
                    ])
                }
            }
        }
    }

    post {
        always {
            cleanWs()
        }
    }
}
```

### 11.4 GitLab CI 配置

```yaml
# .gitlab-ci.yml
stages:
  - lint
  - build
  - test
  - deploy

variables:
  GIT_SUBMODULE_STRATEGY: recursive

# 代码检查
lint:format:
  stage: lint
  image: silkeh/clang:latest
  script:
    - find . -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror

lint:cppcheck:
  stage: lint
  image: neszt/cppcheck-docker
  script:
    - cppcheck --enable=all --error-exitcode=1 hal/ osal/

# 多平台构建
.build_template: &build_template
  stage: build
  image: gcc:latest
  before_script:
    - apt-get update && apt-get install -y cmake ninja-build
  script:
    - cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=$PLATFORM
    - cmake --build build --parallel
  artifacts:
    paths:
      - build/*.elf
    expire_in: 1 week

build:stm32f4:
  <<: *build_template
  variables:
    PLATFORM: stm32f4

build:esp32:
  <<: *build_template
  variables:
    PLATFORM: esp32

# 测试
test:unit:
  stage: test
  image: gcc:latest
  before_script:
    - apt-get update && apt-get install -y cmake ninja-build lcov
  script:
    - cmake -B build -DNEXUS_BUILD_TESTS=ON -DNEXUS_COVERAGE=ON -DNEXUS_PLATFORM=linux
    - cmake --build build --target nexus_tests
    - cd build && ctest --output-on-failure
    - lcov --capture --directory . --output-file coverage.info
  coverage: '/lines......: (\d+\.\d+)%/'
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: build/coverage.xml

# 部署文档
pages:
  stage: deploy
  image: python:3.11
  before_script:
    - apt-get update && apt-get install -y doxygen graphviz
    - pip install sphinx breathe sphinx-rtd-theme myst-parser
  script:
    - doxygen Doxyfile
    - cd docs/sphinx && make html
    - mv _build/html ../../public
  artifacts:
    paths:
      - public
  only:
    - main
```

### 11.5 质量门禁

| 检查项 | 阈值 | 阻断级别 |
|--------|------|----------|
| 代码格式检查 | 100% 通过 | 阻断 |
| 静态分析 (cppcheck) | 0 错误 | 阻断 |
| 编译警告 | 0 警告 (-Werror) | 阻断 |
| 单元测试 | 100% 通过 | 阻断 |
| 代码覆盖率 | ≥ 80% | 警告 |
| 代码覆盖率 | ≥ 90% | 推荐 |
| 文档覆盖率 | ≥ 95% | 警告 |
| ROM 大小增长 | < 5% | 警告 |

---

## 12. 工具链集成

### 12.1 开发工具概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DEVELOPMENT TOOLCHAIN                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                         IDE / Editor                             │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ VS Code  │ │  CLion   │ │Eclipse   │ │  Keil    │           │   │
│  │  │+Cortex-  │ │          │ │  CDT     │ │  MDK     │           │   │
│  │  │ Debug    │ │          │ │          │ │          │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │   Compiler   │ │   Debugger   │ │   Flasher    │ │  Simulator   │  │
│  │ GCC/Clang/   │ │ GDB/OpenOCD  │ │ J-Link/ST-   │ │ QEMU/Renode  │  │
│  │ ARMCC/IAR    │ │ pyOCD        │ │ Link/esptool │ │              │  │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘  │
│                                                                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │Code Analysis │ │   Testing    │ │Documentation │ │   DevOps     │  │
│  │clang-format  │ │ GTest/GMock  │ │Doxygen+Sphinx│ │GitHub Actions│  │
│  │clang-tidy    │ │ Unity/CTest  │ │  +Breathe    │ │Jenkins/GitLab│  │
│  │cppcheck/MISRA│ │ gcov/lcov    │ │              │ │              │  │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 12.2 编译器支持

| 编译器 | 版本要求 | 支持平台 | 说明 |
|--------|----------|----------|------|
| **GCC ARM** | ≥ 10.3 | ARM Cortex-M | 主要编译器 |
| **GCC RISC-V** | ≥ 10.2 | RISC-V | RISC-V 平台 |
| **Clang/LLVM** | ≥ 14.0 | 多平台 | 静态分析 |
| **Xtensa GCC** | ≥ 8.4 | ESP32 | ESP32 平台 |
| **ARMCC** | ≥ 6.0 | ARM | Keil MDK |
| **IAR** | ≥ 9.0 | ARM | IAR Workbench |

### 12.3 调试器配置

#### 12.3.1 VS Code + Cortex-Debug

```json
// .vscode/launch.json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug (J-Link)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "jlink",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/nexus.elf",
            "device": "STM32F407VG",
            "interface": "swd",
            "runToEntryPoint": "main",
            "svdFile": "${workspaceFolder}/platforms/stm32f4/STM32F407.svd",
            "preLaunchTask": "Build Debug",
            "postDebugTask": "Reset Device"
        },
        {
            "name": "Debug (ST-Link)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "stlink",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/nexus.elf",
            "device": "STM32F407VG",
            "interface": "swd",
            "runToEntryPoint": "main",
            "svdFile": "${workspaceFolder}/platforms/stm32f4/STM32F407.svd"
        },
        {
            "name": "Debug (OpenOCD)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "openocd",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/nexus.elf",
            "configFiles": [
                "interface/stlink.cfg",
                "target/stm32f4x.cfg"
            ],
            "searchDir": ["/usr/share/openocd/scripts"],
            "runToEntryPoint": "main"
        }
    ]
}
```

#### 12.3.2 VS Code 任务配置

```json
// .vscode/tasks.json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Debug",
            "type": "shell",
            "command": "cmake",
            "args": [
                "--build", "build",
                "--config", "Debug",
                "--parallel"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Build Release",
            "type": "shell",
            "command": "cmake",
            "args": [
                "--build", "build",
                "--config", "Release",
                "--parallel"
            ],
            "group": "build",
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Configure CMake",
            "type": "shell",
            "command": "cmake",
            "args": [
                "-B", "build",
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Debug",
                "-DNEXUS_PLATFORM=${input:platform}"
            ],
            "problemMatcher": []
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": "ctest",
            "args": ["--test-dir", "build", "--output-on-failure"],
            "group": "test",
            "problemMatcher": []
        },
        {
            "label": "Flash Device",
            "type": "shell",
            "command": "JLinkExe",
            "args": [
                "-device", "STM32F407VG",
                "-if", "SWD",
                "-speed", "4000",
                "-CommandFile", "tools/flash/jlink_flash.jlink"
            ],
            "problemMatcher": []
        },
        {
            "label": "Format Code",
            "type": "shell",
            "command": "find",
            "args": [
                ".", "-name", "*.c", "-o", "-name", "*.h",
                "|", "xargs", "clang-format", "-i"
            ],
            "problemMatcher": []
        },
        {
            "label": "Generate Docs",
            "type": "shell",
            "command": "doxygen",
            "args": ["Doxyfile"],
            "problemMatcher": []
        }
    ],
    "inputs": [
        {
            "id": "platform",
            "type": "pickString",
            "description": "Select target platform",
            "options": ["stm32f4", "stm32h7", "esp32", "nrf52", "linux"],
            "default": "stm32f4"
        }
    ]
}
```

### 12.4 静态分析工具

#### 12.4.1 clang-tidy 配置

```yaml
# .clang-tidy
Checks: >
  -*,
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  misc-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers

WarningsAsErrors: ''

HeaderFilterRegex: '.*'

CheckOptions:
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.GlobalConstantCase
    value: UPPER_CASE
  - key: readability-identifier-naming.MacroDefinitionCase
    value: UPPER_CASE
  - key: readability-identifier-naming.TypedefCase
    value: lower_case
  - key: readability-identifier-naming.TypedefSuffix
    value: '_t'
  - key: readability-function-size.LineThreshold
    value: 100
  - key: readability-function-cognitive-complexity.Threshold
    value: 25
```

#### 12.4.2 cppcheck 配置

```xml
<!-- cppcheck.cfg -->
<?xml version="1.0"?>
<project version="1">
    <paths>
        <dir name="hal/"/>
        <dir name="osal/"/>
        <dir name="middleware/"/>
        <dir name="components/"/>
    </paths>
    <exclude>
        <path name="third_party/"/>
        <path name="build/"/>
    </exclude>
    <libraries>
        <library>posix</library>
    </libraries>
    <suppressions>
        <suppression>missingIncludeSystem</suppression>
    </suppressions>
    <addons>
        <addon>misra</addon>
    </addons>
</project>
```

### 12.5 仿真器支持

#### 12.5.1 QEMU 仿真

```bash
# 启动 QEMU 仿真 (STM32F4)
qemu-system-arm \
    -machine netduinoplus2 \
    -cpu cortex-m4 \
    -kernel build/nexus.elf \
    -nographic \
    -serial mon:stdio \
    -gdb tcp::3333 \
    -S

# GDB 连接
arm-none-eabi-gdb build/nexus.elf \
    -ex "target remote localhost:3333" \
    -ex "load" \
    -ex "monitor reset halt"
```

#### 12.5.2 Renode 仿真

```python
# renode/stm32f4.resc
mach create "nexus"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery.repl

sysbus LoadELF @build/nexus.elf

showAnalyzer sysbus.usart2

# 启动仿真
start
```

### 12.6 推荐 VS Code 扩展

```json
// .vscode/extensions.json
{
    "recommendations": [
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "marus25.cortex-debug",
        "cschlosser.doxdocgen",
        "xaver.clang-format",
        "jeff-hykin.better-cpp-syntax",
        "twxs.cmake",
        "ms-vscode.makefile-tools",
        "streetsidesoftware.code-spell-checker",
        "eamodio.gitlens",
        "mhutchie.git-graph"
    ]
}
```

---

## 13. 跨平台开发

### 13.1 支持的开发主机

| 操作系统 | 版本要求 | 支持状态 |
|----------|----------|----------|
| **Windows** | 10/11 | 完全支持 |
| **Linux** | Ubuntu 20.04+ / Fedora 35+ | 完全支持 |
| **macOS** | 12.0+ (Monterey) | 完全支持 |

### 13.2 环境配置脚本

#### 13.2.1 Windows 环境配置

```powershell
# tools/scripts/setup_windows.ps1
# Nexus 开发环境配置脚本 (Windows)

param(
    [switch]$InstallToolchain,
    [switch]$InstallIDE,
    [switch]$All
)

$ErrorActionPreference = "Stop"

function Install-Chocolatey {
    if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    }
}

function Install-Toolchain {
    Write-Host "Installing development toolchain..."
    choco install -y cmake ninja git python3
    choco install -y gcc-arm-embedded
    choco install -y llvm  # clang-format, clang-tidy
    pip install sphinx breathe sphinx-rtd-theme myst-parser
}

function Install-IDE {
    Write-Host "Installing VS Code and extensions..."
    choco install -y vscode
    code --install-extension ms-vscode.cpptools
    code --install-extension ms-vscode.cmake-tools
    code --install-extension marus25.cortex-debug
    code --install-extension cschlosser.doxdocgen
}

# 主逻辑
Install-Chocolatey

if ($All -or $InstallToolchain) { Install-Toolchain }
if ($All -or $InstallIDE) { Install-IDE }

Write-Host "Setup complete! Please restart your terminal."
```

#### 13.2.2 Linux 环境配置

```bash
#!/bin/bash
# tools/scripts/setup_linux.sh
# Nexus 开发环境配置脚本 (Linux)

set -e

# 检测发行版
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

install_ubuntu() {
    echo "Installing packages for Ubuntu/Debian..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake ninja-build git \
        gcc-arm-none-eabi gdb-multiarch \
        clang clang-format clang-tidy cppcheck \
        doxygen graphviz \
        python3 python3-pip python3-venv \
        openocd stlink-tools

    pip3 install --user sphinx breathe sphinx-rtd-theme myst-parser
}

install_fedora() {
    echo "Installing packages for Fedora..."
    sudo dnf install -y \
        gcc gcc-c++ cmake ninja-build git \
        arm-none-eabi-gcc-cs arm-none-eabi-newlib \
        clang clang-tools-extra cppcheck \
        doxygen graphviz \
        python3 python3-pip \
        openocd

    pip3 install --user sphinx breathe sphinx-rtd-theme myst-parser
}

# 安装 VS Code
install_vscode() {
    if ! command -v code &> /dev/null; then
        echo "Installing VS Code..."
        if [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "debian" ]; then
            wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
            sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
            sudo sh -c 'echo "deb [arch=amd64] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
            sudo apt-get update
            sudo apt-get install -y code
        elif [ "$DISTRO" = "fedora" ]; then
            sudo rpm --import https://packages.microsoft.com/keys/microsoft.asc
            sudo sh -c 'echo -e "[code]\nname=Visual Studio Code\nbaseurl=https://packages.microsoft.com/yumrepos/vscode\nenabled=1\ngpgcheck=1\ngpgkey=https://packages.microsoft.com/keys/microsoft.asc" > /etc/yum.repos.d/vscode.repo'
            sudo dnf install -y code
        fi
    fi

    # 安装扩展
    code --install-extension ms-vscode.cpptools
    code --install-extension ms-vscode.cmake-tools
    code --install-extension marus25.cortex-debug
    code --install-extension cschlosser.doxdocgen
}

# 主逻辑
case $DISTRO in
    ubuntu|debian) install_ubuntu ;;
    fedora) install_fedora ;;
    *) echo "Unsupported distribution: $DISTRO"; exit 1 ;;
esac

install_vscode

echo "Setup complete! Please restart your terminal."
```

#### 13.2.3 macOS 环境配置

```bash
#!/bin/bash
# tools/scripts/setup_macos.sh
# Nexus 开发环境配置脚本 (macOS)

set -e

# 安装 Homebrew
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

echo "Installing development toolchain..."
brew install cmake ninja git python@3
brew install --cask gcc-arm-embedded
brew install llvm doxygen graphviz cppcheck

# 添加 LLVM 到 PATH
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc

# Python 包
pip3 install sphinx breathe sphinx-rtd-theme myst-parser

# VS Code
if ! command -v code &> /dev/null; then
    echo "Installing VS Code..."
    brew install --cask visual-studio-code
fi

# VS Code 扩展
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools
code --install-extension marus25.cortex-debug
code --install-extension cschlosser.doxdocgen

echo "Setup complete! Please restart your terminal."
```

### 13.3 跨平台构建矩阵

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     CROSS-PLATFORM BUILD MATRIX                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│              │  Windows  │   Linux   │   macOS   │                      │
│  ────────────┼───────────┼───────────┼───────────┤                      │
│  STM32F4     │     ✓     │     ✓     │     ✓     │                      │
│  STM32H7     │     ✓     │     ✓     │     ✓     │                      │
│  ESP32       │     ✓     │     ✓     │     ✓     │                      │
│  nRF52       │     ✓     │     ✓     │     ✓     │                      │
│  RISC-V      │     ✓     │     ✓     │     ✓     │                      │
│  Linux Host  │     -     │     ✓     │     ✓     │                      │
│  ────────────┼───────────┼───────────┼───────────┤                      │
│  Unit Tests  │     ✓     │     ✓     │     ✓     │                      │
│  QEMU Sim    │     ✓     │     ✓     │     ✓     │                      │
│  Docs Build  │     ✓     │     ✓     │     ✓     │                      │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 13.4 Docker 开发环境

```dockerfile
# Dockerfile
FROM ubuntu:22.04

# 避免交互式安装
ENV DEBIAN_FRONTEND=noninteractive

# 安装基础工具
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git \
    gcc-arm-none-eabi gdb-multiarch \
    clang clang-format clang-tidy cppcheck \
    doxygen graphviz \
    python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Python 包
RUN pip3 install sphinx breathe sphinx-rtd-theme myst-parser

# 工作目录
WORKDIR /workspace

# 默认命令
CMD ["/bin/bash"]
```

```yaml
# docker-compose.yml
version: '3.8'

services:
  nexus-dev:
    build: .
    image: nexus-dev:latest
    volumes:
      - .:/workspace
      - ~/.gitconfig:/root/.gitconfig:ro
    working_dir: /workspace
    command: /bin/bash

  nexus-build:
    image: nexus-dev:latest
    volumes:
      - .:/workspace
    working_dir: /workspace
    command: >
      bash -c "
        cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release &&
        cmake --build build --parallel
      "

  nexus-test:
    image: nexus-dev:latest
    volumes:
      - .:/workspace
    working_dir: /workspace
    command: >
      bash -c "
        cmake -B build -DNEXUS_BUILD_TESTS=ON -DNEXUS_PLATFORM=linux &&
        cmake --build build --target nexus_tests &&
        ctest --test-dir build --output-on-failure
      "
```

---

## 14. 版本管理

### 14.1 版本号规范

采用 **语义化版本** (Semantic Versioning 2.0.0)：

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]

示例：
  2.0.0           - 正式版本
  2.1.0-alpha.1   - Alpha 预发布版
  2.1.0-beta.2    - Beta 预发布版
  2.1.0-rc.1      - Release Candidate
  2.1.0+build.123 - 带构建元数据
```

| 版本号 | 变更类型 | 说明 |
|--------|----------|------|
| **MAJOR** | 不兼容的 API 变更 | 重大架构调整、接口删除 |
| **MINOR** | 向后兼容的功能新增 | 新增功能、新增接口 |
| **PATCH** | 向后兼容的问题修复 | Bug 修复、性能优化 |

### 14.2 Git 分支策略

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         GIT BRANCHING MODEL                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  main ─────●─────────────●─────────────●─────────────●───────▶          │
│            │             │             │             │                   │
│            │   v2.0.0    │   v2.1.0    │   v2.2.0    │                   │
│            │             │             │             │                   │
│  develop ──●──●──●──●────●──●──●──●────●──●──●──●────●───────▶          │
│               │     │       │     │       │     │                        │
│               │     │       │     │       │     │                        │
│  feature/  ───●─────┘       │     │       │     │                        │
│  gpio-irq                   │     │       │     │                        │
│                             │     │       │     │                        │
│  feature/  ─────────────────●─────┘       │     │                        │
│  uart-dma                                 │     │                        │
│                                           │     │                        │
│  hotfix/   ───────────────────────────────●─────┘                        │
│  fix-mem-leak                                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

| 分支类型 | 命名规范 | 说明 |
|----------|----------|------|
| **main** | `main` | 稳定发布分支 |
| **develop** | `develop` | 开发集成分支 |
| **feature** | `feature/<name>` | 功能开发分支 |
| **bugfix** | `bugfix/<name>` | Bug 修复分支 |
| **hotfix** | `hotfix/<name>` | 紧急修复分支 |
| **release** | `release/<version>` | 发布准备分支 |

### 14.3 提交信息规范

采用 **Conventional Commits** 规范：

```
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]
```

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(hal): add GPIO interrupt support` |
| `fix` | Bug 修复 | `fix(osal): fix mutex deadlock issue` |
| `docs` | 文档更新 | `docs(api): update GPIO API reference` |
| `style` | 代码格式 | `style: apply clang-format` |
| `refactor` | 重构 | `refactor(hal): simplify UART init` |
| `perf` | 性能优化 | `perf(osal): optimize task switch` |
| `test` | 测试相关 | `test(hal): add GPIO unit tests` |
| `build` | 构建相关 | `build: update CMake to 3.20` |
| `ci` | CI 相关 | `ci: add coverage report` |
| `chore` | 其他 | `chore: update .gitignore` |

### 14.4 变更日志

```markdown
# CHANGELOG.md

## [Unreleased]

### Added
- GPIO interrupt support for STM32H7

### Changed
- Improved UART DMA performance

### Fixed
- Fixed memory leak in queue module

---

## [2.0.0] - 2026-01-12

### Added
- Multi-platform build system (CMake, Make, Bazel)
- Google Test/Mock integration
- TDD workflow support
- Cross-platform development (Windows/Linux/macOS)
- Doxygen + Sphinx + Breathe documentation pipeline
- GitHub Actions CI/CD

### Changed
- Migrated to backslash-style Doxygen comments
- Updated code format to 80 char line limit

### Breaking Changes
- HAL interface restructured
- OSAL API renamed for consistency

---

## [1.0.0] - 2025-06-01

### Added
- Initial release
- HAL layer (GPIO, UART, SPI, I2C, ADC, Timer)
- OSAL layer (Task, Mutex, Semaphore, Queue)
- FreeRTOS adapter
- STM32F4 platform support
```

### 14.5 发布流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         RELEASE WORKFLOW                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. Create Release Branch                                                │
│     └─▶ git checkout -b release/2.1.0 develop                           │
│                                                                          │
│  2. Version Bump                                                         │
│     └─▶ Update version in CMakeLists.txt, Doxyfile, etc.                │
│                                                                          │
│  3. Update CHANGELOG                                                     │
│     └─▶ Move [Unreleased] items to new version section                  │
│                                                                          │
│  4. Final Testing                                                        │
│     └─▶ Run full test suite, fix any issues                             │
│                                                                          │
│  5. Merge to main                                                        │
│     └─▶ git checkout main && git merge release/2.1.0                    │
│                                                                          │
│  6. Tag Release                                                          │
│     └─▶ git tag -a v2.1.0 -m "Release 2.1.0"                            │
│                                                                          │
│  7. Merge back to develop                                                │
│     └─▶ git checkout develop && git merge main                          │
│                                                                          │
│  8. Push & Publish                                                       │
│     └─▶ git push origin main develop --tags                             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 15. 附录

### 15.1 参考文档

| 文档 | 链接 | 说明 |
|------|------|------|
| C11 标准 | ISO/IEC 9899:2011 | C 语言标准 |
| MISRA C:2012 | MISRA | 嵌入式 C 编码规范 |
| CMSIS | ARM | Cortex-M 软件接口标准 |
| FreeRTOS | freertos.org | RTOS 参考 |
| Google Test | github.com/google/googletest | 测试框架 |
| Doxygen | doxygen.nl | 文档生成 |
| Sphinx | sphinx-doc.org | 文档系统 |
| Semantic Versioning | semver.org | 版本规范 |
| Conventional Commits | conventionalcommits.org | 提交规范 |

### 15.2 术语表

| 术语 | 全称 | 定义 |
|------|------|------|
| ADC | Analog-to-Digital Converter | 模数转换器 |
| API | Application Programming Interface | 应用程序接口 |
| BSP | Board Support Package | 板级支持包 |
| CI/CD | Continuous Integration/Deployment | 持续集成/部署 |
| DMA | Direct Memory Access | 直接内存访问 |
| GPIO | General Purpose Input/Output | 通用输入输出 |
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| IDE | Integrated Development Environment | 集成开发环境 |
| ISR | Interrupt Service Routine | 中断服务程序 |
| MCU | Microcontroller Unit | 微控制器 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层 |
| OTA | Over-The-Air | 空中升级 |
| PWM | Pulse Width Modulation | 脉宽调制 |
| RTOS | Real-Time Operating System | 实时操作系统 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| TDD | Test-Driven Development | 测试驱动开发 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发器 |

### 15.3 修订历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| 1.0.0 | 2025-06-01 | Nexus Team | 初始版本 |
| 2.0.0 | 2026-01-12 | Nexus Team | 重大更新：多构建系统、TDD、跨平台、DevOps |

### 15.4 .clang-format 完整配置

```yaml
# .clang-format
---
Language: Cpp
BasedOnStyle: LLVM

# 缩进
IndentWidth: 4
TabWidth: 4
UseTab: Never
IndentCaseLabels: true
IndentPPDirectives: BeforeHash
NamespaceIndentation: None

# 行宽
ColumnLimit: 80

# 对齐
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignConsecutiveBitFields: true
AlignConsecutiveDeclarations: false
AlignConsecutiveMacros: true
AlignEscapedNewlines: Left
AlignOperands: Align
AlignTrailingComments: true

# 花括号
BreakBeforeBraces: Attach
BraceWrapping:
  AfterCaseLabel: false
  AfterClass: false
  AfterControlStatement: Never
  AfterEnum: false
  AfterFunction: false
  AfterNamespace: false
  AfterStruct: false
  AfterUnion: false
  BeforeCatch: false
  BeforeElse: false
  IndentBraces: false
  SplitEmptyFunction: false
  SplitEmptyRecord: false
  SplitEmptyNamespace: false

# 短语句
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: false
AllowShortEnumsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false

# 指针对齐
PointerAlignment: Left
DerivePointerAlignment: false

# 换行
BreakBeforeBinaryOperators: None
BreakBeforeTernaryOperators: true
BreakStringLiterals: true

# 空格
SpaceAfterCStyleCast: false
SpaceAfterLogicalNot: false
SpaceBeforeAssignmentOperators: true
SpaceBeforeParens: ControlStatements
SpaceInEmptyBlock: false
SpaceInEmptyParentheses: false
SpacesInCStyleCastParentheses: false
SpacesInConditionalStatement: false
SpacesInParentheses: false
SpacesInSquareBrackets: false

# 头文件排序
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^".*\.h"'
    Priority: 1
  - Regex: '^<.*\.h>'
    Priority: 2
  - Regex: '^<.*>'
    Priority: 3

# 其他
MaxEmptyLinesToKeep: 1
ReflowComments: true
...
```

### 15.5 VS Code 完整配置

```json
// .vscode/settings.json
{
    // C/C++ 配置
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.cStandard": "c11",
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.clang_format_style": "file",
    "C_Cpp.clang_format_fallbackStyle": "LLVM",
    "C_Cpp.doxygen.generatedStyle": "/**",

    // Doxygen 生成器配置 (反斜杠风格)
    "doxdocgen.file.fileTemplate": "\\file            {name}",
    "doxdocgen.generic.briefTemplate": "\\brief           {text}",
    "doxdocgen.generic.paramTemplate": "\\param[{direction}]       {param}: ",
    "doxdocgen.generic.returnTemplate": "\\return          {type}",
    "doxdocgen.generic.authorTag": "\\author          {author} ({email})",
    "doxdocgen.generic.dateTemplate": "\\date            {date}",
    "doxdocgen.file.versionTag": "\\version         0.0.1",
    "doxdocgen.generic.useGitUserName": true,
    "doxdocgen.generic.useGitUserEmail": true,
    "doxdocgen.cpp.tparamTemplate": "\\tparam          {param} ",
    "doxdocgen.generic.commandSuggestionAddPrefix": true,
    "doxdocgen.file.fileOrder": [
        "file",
        "brief",
        "author",
        "version",
        "date",
        "empty",
        "copyright"
    ],

    // 编辑器配置
    "editor.formatOnSave": true,
    "editor.rulers": [80],
    "editor.tabSize": 4,
    "editor.insertSpaces": true,
    "editor.detectIndentation": false,

    // 文件关联
    "files.associations": {
        "*.h": "c",
        "*.c": "c",
        "*.hpp": "cpp",
        "*.cpp": "cpp"
    },

    // CMake 配置
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.generator": "Ninja",

    // 搜索排除
    "search.exclude": {
        "**/build": true,
        "**/third_party": true,
        "**/docs/build": true
    },

    // 文件排除
    "files.exclude": {
        "**/.git": true,
        "**/build": true,
        "**/*.o": true,
        "**/*.elf": true
    }
}
```

---

## 文档结束

本文档定义了 Nexus 嵌入式软件开发平台的完整需求规范，包括系统架构、功能需求、代码规范、构建系统、测试体系、DevOps 流程、工具链集成、跨平台开发和版本管理等方面。

如有问题或建议，请联系 Nexus 架构团队。

---

*Copyright © 2026 Nexus Team. All rights reserved.*
