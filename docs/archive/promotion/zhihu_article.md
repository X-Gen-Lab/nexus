# 告别重复造轮子：我开源了一个让嵌入式开发效率提升 10 倍的框架

> 作为一个写了多年嵌入式代码的开发者，我终于受够了每换一个 MCU 就要重写驱动的日子。于是我做了 Nexus。

## 前言

先问大家几个问题：

- 你是否每次新项目都要从零开始写 GPIO、UART 驱动？
- 你是否换个 MCU 平台就要把代码大改一遍？
- 你是否想给嵌入式代码写单元测试，却不知从何下手？
- 你是否羡慕 Web 开发者有那么多现成的框架可用？

如果你的答案是"是"，那这篇文章就是为你写的。

---

## 一、嵌入式开发的痛，谁懂？

### 1.1 平台碎片化

今天用 STM32F4，明天客户要求换成 ESP32，后天又要支持 nRF52。每换一个平台，驱动代码基本要重写。

```c
// STM32 的 GPIO
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

// ESP32 的 GPIO
gpio_set_level(GPIO_NUM_5, 1);

// nRF52 的 GPIO
nrf_gpio_pin_set(5);
```

同样是点亮一个 LED，三个平台三种写法。业务代码和硬件代码耦合在一起，换平台就是噩梦。

### 1.2 代码质量参差不齐

很多嵌入式项目的代码是这样的：

- 没有统一的代码风格
- 没有注释或注释过时
- 没有单元测试
- 全局变量满天飞
- 复制粘贴大法好

结果就是：代码能跑，但没人敢改。

### 1.3 测试困难

嵌入式代码怎么测试？烧到板子上跑？

- 硬件不一定随时有
- 调试效率低
- 边界条件难覆盖
- CI/CD 难以集成

很多团队的"测试"就是：编译通过 = 测试通过。

---

## 二、Nexus 是什么？

**Nexus** 是一个开源的嵌入式软件开发平台，目标是：

> **一次编写，多平台运行**

它提供了一套统一的 API，让你的业务代码与硬件解耦。无论底层是 STM32、ESP32 还是 nRF52，上层代码都不用改。

### 2.1 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                        应用层                                │
├─────────────────────────────────────────────────────────────┤
│                       中间件层                               │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │  Shell  │ │   日志  │ │  配置   │ │  事件   │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
├─────────────────────────────────────────────────────────────┤
│                    操作系统抽象层 (OSAL)                      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │  任务   │ │  互斥锁 │ │  队列   │ │  定时器 │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
├─────────────────────────────────────────────────────────────┤
│                    硬件抽象层 (HAL)                          │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │  GPIO   │ │  UART   │ │   SPI   │ │   I2C   │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
├─────────────────────────────────────────────────────────────┤
│                      平台 / 硬件层                           │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │ STM32F4 │ │ STM32H7 │ │  ESP32  │ │  nRF52  │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
└─────────────────────────────────────────────────────────────┘
```

**核心思想**：上层只依赖抽象接口，下层负责具体实现。换平台只需要换底层实现，上层代码一行不改。

### 2.2 统一的 API

无论什么平台，GPIO 操作都是这样写：

```c
#include "hal/hal_gpio.h"

// 配置 GPIO
hal_gpio_config_t config = {
    .direction   = HAL_GPIO_DIR_OUTPUT,
    .pull        = HAL_GPIO_PULL_NONE,
    .output_mode = HAL_GPIO_OUTPUT_PP,
    .speed       = HAL_GPIO_SPEED_LOW,
    .init_level  = HAL_GPIO_LEVEL_LOW
};

hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

// 操作 GPIO
hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);
hal_gpio_toggle(HAL_GPIO_PORT_A, 5);
```

换平台？改一下 CMake 配置就行：

```bash
# STM32F4
cmake -B build -DNEXUS_PLATFORM=stm32f4

# ESP32
cmake -B build -DNEXUS_PLATFORM=esp32

# 本地测试
cmake -B build -DNEXUS_PLATFORM=native
```

---

## 三、核心特性

### 3.1 完整的 HAL 层

支持常用外设：

| 模块 | 功能 |
|------|------|
| GPIO | 输入/输出、中断、上下拉 |
| UART | 收发、中断、DMA |
| SPI | 主从模式、全双工 |
| I2C | 主从模式、7/10位地址 |
| Timer | 定时、PWM、输入捕获 |
| ADC | 单次/连续采样、DMA |

### 3.2 灵活的 OSAL 层

支持多种 RTOS 后端：

- **FreeRTOS**：最流行的嵌入式 RTOS
- **裸机**：适合资源受限场景
- **Native**：PC 上用 pthreads 模拟，方便测试

```c
#include "osal/osal.h"

// 创建任务
osal_task_config_t config = {
    .name       = "my_task",
    .func       = my_task_func,
    .stack_size = 1024,
    .priority   = OSAL_PRIORITY_NORMAL
};
osal_task_handle_t handle;
osal_task_create(&config, &handle);

// 创建互斥锁
osal_mutex_handle_t mutex;
osal_mutex_create(&mutex);
osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
// 临界区
osal_mutex_unlock(mutex);
```

### 3.3 强大的日志框架

不是简单的 printf，而是企业级日志系统：

```c
#define LOG_MODULE "app"
#include "log/log.h"

LOG_TRACE("详细跟踪信息");
LOG_DEBUG("调试信息: value = %d", 42);
LOG_INFO("应用启动");
LOG_WARN("资源使用率 80%%");
LOG_ERROR("文件打开失败: %s", filename);
LOG_FATAL("系统崩溃");
```

特性：
- 6 个日志级别
- 多后端输出（串口、控制台、内存）
- 模块级过滤（支持通配符）
- 自定义格式（时间戳、文件名、行号）
- 同步/异步模式
- 线程安全

### 3.4 MISRA C 合规

代码遵循 MISRA C:2012 规范，适合：

- 汽车电子
- 医疗设备
- 工业控制
- 航空航天

### 3.5 90%+ 测试覆盖率

使用 Google Test 框架，支持：

- 单元测试
- 属性测试（Property-Based Testing）
- 集成测试

```cpp
TEST(HalGpioTest, InitOutput) {
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT
    };
    EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
}
```

关键是：**可以在 PC 上跑测试**，不需要硬件！

---

## 四、5 分钟快速上手

### 4.1 环境准备

```bash
# Windows
winget install Kitware.CMake
winget install Git.Git

# Linux
sudo apt install cmake git build-essential

# macOS
brew install cmake git
```

### 4.2 克隆项目

```bash
git clone https://github.com/X-Gen-Lab/nexus.git
cd nexus
```

### 4.3 本地构建测试

```bash
# 配置（native 平台，用于 PC 测试）
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON

# 构建
cmake --build build

# 运行测试
ctest --test-dir build --output-on-failure
```

### 4.4 第一个程序

```c
// main.c
#include "hal/hal.h"

#define LED_PORT    HAL_GPIO_PORT_A
#define LED_PIN     5

int main(void)
{
    hal_system_init();
    
    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };
    hal_gpio_init(LED_PORT, LED_PIN, &config);

    while (1) {
        hal_gpio_toggle(LED_PORT, LED_PIN);
        hal_delay_ms(500);
    }
}
```

---

## 五、与其他方案对比

| 特性 | Nexus | 直接用厂商 HAL | Zephyr | Arduino |
|------|-------|---------------|--------|---------|
| 学习曲线 | 低 | 低 | 高 | 低 |
| 跨平台 | ✅ | ❌ | ✅ | 部分 |
| 代码体积 | 小 | 小 | 大 | 小 |
| RTOS 支持 | FreeRTOS/裸机 | 看厂商 | 内置 | 无 |
| 测试友好 | ✅ | ❌ | ✅ | ❌ |
| MISRA 合规 | ✅ | 部分 | 部分 | ❌ |
| 中文文档 | ✅ | 部分 | ❌ | 部分 |
| 商用友好 | MIT | 看厂商 | Apache | LGPL |

---

## 六、谁适合用 Nexus？

### ✅ 适合

- 需要支持多个 MCU 平台的产品
- 重视代码质量和可维护性的团队
- 想在 PC 上做单元测试的开发者
- 需要 MISRA C 合规的项目
- 嵌入式教学和培训

### ❌ 不太适合

- 只用一个平台，永远不换的项目
- 对代码体积极度敏感（< 8KB Flash）
- 需要极致性能优化的场景

---

## 七、路线图

### 已完成 ✅
- HAL 层（GPIO、UART、SPI、I2C、Timer、ADC）
- OSAL 层（FreeRTOS、裸机、Native）
- 日志框架
- STM32F4 平台支持
- 完整的单元测试
- 中英文文档

### 进行中 🚧
- STM32H7 平台
- ESP32 平台
- nRF52 平台
- Shell 组件
- 配置管理组件

### 计划中 📋
- 文件系统抽象
- 网络协议栈抽象
- 云平台集成（AWS IoT、阿里云）
- 低功耗管理

---

## 八、如何参与

Nexus 是 MIT 协议的开源项目，欢迎：

- ⭐ Star 支持
- 🐛 提交 Issue 报告问题
- 🔧 提交 PR 贡献代码
- 📖 完善文档
- 📢 分享给更多人

**GitHub 地址**：https://github.com/X-Gen-Lab/nexus

---

## 结语

嵌入式开发不应该这么痛苦。

我们有了 React、Vue 让前端开发更简单，有了 Spring、Django 让后端开发更高效。嵌入式领域也应该有这样的工具。

Nexus 就是我的尝试。它可能还不完美，但我相信方向是对的。

如果你也受够了重复造轮子，欢迎来试试 Nexus。

**一次编写，多平台运行。让我们一起让嵌入式开发更简单。**

---

> 💬 欢迎在评论区交流，有任何问题我都会回复。
> 
> 👍 如果觉得有用，点个赞让更多人看到。
> 
> ⭐ GitHub: https://github.com/X-Gen-Lab/nexus

---

*本文首发于知乎，转载请注明出处。*
