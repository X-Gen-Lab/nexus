# Init Framework 用户使用指南

**版本**: 1.0.0  
**最后更新**: 2026-01-24

---

## 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [自动初始化机制](#自动初始化机制)
- [启动框架](#启动框架)
- [固件信息](#固件信息)
- [配置选项](#配置选项)
- [常见用例](#常见用例)
- [最佳实践](#最佳实践)
- [API 参考](#api-参考)

---

## 概述

Init Framework 提供三个核心功能模块：

### 1. 自动初始化机制 (nx_init)

基于链接器段的编译期自动注册系统，支持分级初始化。

**核心优势**:
- ✅ 零运行时开销 - 编译期确定所有初始化函数
- ✅ 模块解耦 - 驱动无需修改核心代码即可注册
- ✅ 有序执行 - 6 级初始化顺序保证依赖关系
- ✅ 跨编译器 - 支持 GCC、Arm Compiler、IAR

### 2. 启动框架 (nx_startup)

统一的系统启动序列，拦截程序入口点。

**核心优势**:
- ✅ 统一入口 - 标准化启动流程
- ✅ 弱符号覆盖 - 灵活的板级初始化
- ✅ 双模式支持 - 裸机和 RTOS
- ✅ 可配置 - 自定义栈大小和优先级

### 3. 固件信息 (nx_firmware_info)

嵌入式固件元数据，支持工具提取。

**核心优势**:
- ✅ 编译期嵌入 - 版本信息固化在二进制中
- ✅ 工具可提取 - 无需执行固件即可读取
- ✅ 版本编码 - 标准化版本号格式
- ✅ 独立段 - 专用链接器段存储

---

## 快速开始

### 最小示例

```c
#include "nx_init.h"
#include "nx_startup.h"

/* 1. 定义初始化函数 */
static int my_driver_init(void) {
    /* 初始化硬件 */
    return 0;  /* 返回 0 表示成功 */
}

/* 2. 注册为驱动级别初始化 */
NX_INIT_DRIVER_EXPORT(my_driver_init);

/* 3. 主函数 */
int main(void) {
    /* 此时 my_driver_init 已自动执行 */
    
    /* 应用逻辑 */
    while (1) {
        /* ... */
    }
    
    return 0;
}
```

### 编译和链接

**GCC**:
```bash
# 编译
arm-none-eabi-gcc -c my_driver.c -o my_driver.o

# 链接（需要正确的链接器脚本）
arm-none-eabi-gcc my_driver.o -T linker_script.ld -o firmware.elf
```

**链接器脚本要求**:
- 必须定义 `.nx_init_fn.*` 段
- 必须提供 `__nx_init_fn_start` 和 `__nx_init_fn_end` 符号

---

## 自动初始化机制

### 初始化级别

系统定义了 6 个初始化级别，按数字顺序执行：

| 级别 | 宏定义 | 用途 | 典型示例 |
|------|--------|------|----------|
| 1 | `NX_INIT_BOARD_EXPORT` | 板级初始化 | 时钟配置、电源管理 |
| 2 | `NX_INIT_PREV_EXPORT` | 预初始化 | 内存初始化、调试接口 |
| 3 | `NX_INIT_BSP_EXPORT` | BSP 初始化 | 外设配置、引脚复用 |
| 4 | `NX_INIT_DRIVER_EXPORT` | 驱动初始化 | UART、SPI、I2C 驱动 |
| 5 | `NX_INIT_COMPONENT_EXPORT` | 组件初始化 | 文件系统、网络栈 |
| 6 | `NX_INIT_APP_EXPORT` | 应用初始化 | 应用特定初始化 |

### 注册初始化函数

#### 基本用法

```c
#include "nx_init.h"

/* 定义初始化函数 */
static int uart_init(void) {
    /* 配置 UART 硬件 */
    uart_hw_config();
    
    /* 返回 0 表示成功 */
    return 0;
}

/* 注册为驱动级别 */
NX_INIT_DRIVER_EXPORT(uart_init);
```

#### 带错误处理

```c
static int spi_init(void) {
    int ret;
    
    /* 初始化 SPI 控制器 */
    ret = spi_controller_init();
    if (ret != 0) {
        return -1;  /* 返回非零表示失败 */
    }
    
    /* 注册 SPI 设备 */
    ret = spi_device_register();
    if (ret != 0) {
        return -2;
    }
    
    return 0;
}

NX_INIT_DRIVER_EXPORT(spi_init);
```

### 执行初始化

#### 自动执行（推荐）

使用启动框架时，初始化函数会自动执行：

```c
int main(void) {
    /* 此时所有初始化函数已执行完毕 */
    
    /* 检查初始化状态 */
    if (!nx_init_is_complete()) {
        /* 处理初始化失败 */
    }
    
    /* 应用逻辑 */
    return 0;
}
```

#### 手动执行

如果不使用启动框架，需要手动调用：

```c
#include "nx_init.h"

int main(void) {
    /* 手动执行所有初始化函数 */
    nx_status_t status = nx_init_run();
    
    if (status != NX_OK) {
        /* 处理初始化失败 */
    }
    
    /* 应用逻辑 */
    return 0;
}
```

### 获取初始化统计

```c
#include "nx_init.h"

void check_init_status(void) {
    nx_init_stats_t stats;
    
    /* 获取统计信息 */
    nx_init_get_stats(&stats);
    
    printf("初始化统计:\n");
    printf("  总数: %d\n", stats.total_count);
    printf("  成功: %d\n", stats.success_count);
    printf("  失败: %d\n", stats.fail_count);
    
    if (stats.fail_count > 0) {
        printf("  最后错误: %d\n", stats.last_error);
    }
}
```

---

## 启动框架

### 启动序列

完整的启动序列如下：

```
Reset Handler
    │
    ▼
nx_startup()
    │
    ├─→ nx_board_init()      [弱符号，用户可覆盖]
    │   └─→ 时钟配置、电源管理
    │
    ├─→ nx_os_init()         [弱符号，用户可覆盖]
    │   └─→ RTOS 内核初始化
    │
    ├─→ nx_init_run()        [自动初始化]
    │   ├─→ Level 1: BOARD
    │   ├─→ Level 2: PREV
    │   ├─→ Level 3: BSP
    │   ├─→ Level 4: DRIVER
    │   ├─→ Level 5: COMPONENT
    │   └─→ Level 6: APP
    │
    └─→ main() 或 main_thread
```

### 基本使用

#### 裸机模式

```c
#include "nx_startup.h"

int main(void) {
    /* 启动框架已完成所有初始化 */
    
    /* 应用主循环 */
    while (1) {
        /* 应用逻辑 */
    }
    
    return 0;
}
```

#### 覆盖板级初始化

```c
#include "nx_startup.h"

/* 覆盖弱符号实现板级初始化 */
void nx_board_init(void) {
    /* 配置系统时钟 */
    SystemClock_Config();
    
    /* 配置 GPIO */
    GPIO_Init();
    
    /* 配置中断优先级 */
    NVIC_SetPriorityGrouping(4);
}

int main(void) {
    /* 应用逻辑 */
    return 0;
}
```

### RTOS 模式

#### 使用默认配置

```c
#include "nx_startup.h"
#include "FreeRTOS.h"
#include "task.h"

/* 覆盖 OS 初始化 */
void nx_os_init(void) {
    /* FreeRTOS 会在 nx_startup 中自动启动 */
}

int main(void) {
    /* 此函数会在 FreeRTOS 任务中运行 */
    
    /* 创建应用任务 */
    xTaskCreate(app_task, "app", 512, NULL, 1, NULL);
    
    /* 主任务逻辑 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

#### 自定义配置

```c
#include "nx_startup.h"

int main(void) {
    /* 此代码在 nx_startup 之前不会执行 */
    return 0;
}

/* 在启动前配置 */
void early_init(void) {
    nx_startup_config_t config;
    
    /* 获取默认配置 */
    nx_startup_get_default_config(&config);
    
    /* 自定义配置 */
    config.main_stack_size = 8192;  /* 8KB 栈 */
    config.main_priority = 24;      /* 高优先级 */
    config.use_rtos = true;
    
    /* 使用自定义配置启动 */
    nx_startup_with_config(&config);
}
```

### 查询启动状态

```c
#include "nx_startup.h"

void check_startup_state(void) {
    nx_startup_state_t state = nx_startup_get_state();
    
    switch (state) {
        case NX_STARTUP_STATE_NOT_STARTED:
            printf("启动未开始\n");
            break;
        case NX_STARTUP_STATE_BOARD_INIT:
            printf("板级初始化中\n");
            break;
        case NX_STARTUP_STATE_OS_INIT:
            printf("OS 初始化中\n");
            break;
        case NX_STARTUP_STATE_AUTO_INIT:
            printf("自动初始化中\n");
            break;
        case NX_STARTUP_STATE_MAIN_RUNNING:
            printf("主函数运行中\n");
            break;
        case NX_STARTUP_STATE_COMPLETE:
            printf("启动完成\n");
            break;
    }
    
    /* 或使用简单检查 */
    if (nx_startup_is_complete()) {
        printf("启动已完成\n");
    }
}
```

---

## 固件信息

### 定义固件信息

#### 基本定义

```c
#include "nx_firmware_info.h"

/* 定义固件信息 */
NX_FIRMWARE_INFO_DEFINE(
    "My Product",                      /* 产品名称 */
    "VENDOR",                          /* 厂商标识 */
    NX_VERSION_ENCODE(1, 0, 0, 0),     /* 版本 1.0.0.0 */
    0xDEADBEEF                         /* 固件密钥 */
);
```

#### 在主文件中定义

```c
/* main.c */
#include "nx_firmware_info.h"

/* 固件信息（通常在主文件中定义一次） */
NX_FIRMWARE_INFO_DEFINE(
    "Nexus IoT Device",
    "NEXUS",
    NX_VERSION_ENCODE(2, 1, 5, 123),
    0x12345678
);

int main(void) {
    /* 应用逻辑 */
    return 0;
}
```

### 版本编码和解码

#### 编码版本号

```c
/* 版本 1.2.3.4 */
uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

/* 版本 2.0.0.0 */
uint32_t version_2 = NX_VERSION_ENCODE(2, 0, 0, 0);
```

#### 解码版本号

```c
uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

uint8_t major = NX_VERSION_MAJOR(version);  /* 1 */
uint8_t minor = NX_VERSION_MINOR(version);  /* 2 */
uint8_t patch = NX_VERSION_PATCH(version);  /* 3 */
uint8_t build = NX_VERSION_BUILD(version);  /* 4 */

printf("版本: %d.%d.%d.%d\n", major, minor, patch, build);
```

### 读取固件信息

#### 获取固件信息指针

```c
#include "nx_firmware_info.h"

void print_firmware_info(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    
    if (info == NULL) {
        printf("固件信息未定义\n");
        return;
    }
    
    printf("产品: %s\n", info->product);
    printf("厂商: %s\n", info->factory);
    printf("构建日期: %s\n", info->date);
    printf("构建时间: %s\n", info->time);
    
    /* 解码版本 */
    printf("版本: %d.%d.%d.%d\n",
           NX_VERSION_MAJOR(info->version),
           NX_VERSION_MINOR(info->version),
           NX_VERSION_PATCH(info->version),
           NX_VERSION_BUILD(info->version));
    
    printf("密钥: 0x%08X\n", info->key);
}
```

#### 获取版本字符串

```c
#include "nx_firmware_info.h"

void print_version(void) {
    char version_str[32];
    
    /* 获取格式化的版本字符串 */
    size_t len = nx_get_version_string(version_str, sizeof(version_str));
    
    if (len > 0) {
        printf("固件版本: %s\n", version_str);
    } else {
        printf("固件信息未定义\n");
    }
}
```

### 外部工具提取

固件信息存储在独立的链接器段 `.nx_fw_info` 中，可以使用外部工具提取：

```bash
# 使用 objdump 查看
arm-none-eabi-objdump -s -j .nx_fw_info firmware.elf

# 使用 readelf 查看
arm-none-eabi-readelf -x .nx_fw_info firmware.elf

# 使用自定义工具提取
./extract_fw_info.py firmware.bin
```

---

## 配置选项

### 编译时配置

#### 启动框架配置

```c
/* 在 nx_config.h 或编译选项中定义 */

/* 主线程栈大小（字节） */
#define NX_STARTUP_MAIN_STACK_SIZE 4096

/* 主线程优先级（0-31） */
#define NX_STARTUP_MAIN_PRIORITY 16
```

#### 调试模式

```c
/* 启用初始化调试输出 */
#define NX_INIT_DEBUG

/* 启用启动框架测试模式 */
#define NX_STARTUP_TEST_MODE
```

### 运行时配置

#### 启动配置

```c
nx_startup_config_t config = {
    .main_stack_size = 8192,    /* 8KB 栈 */
    .main_priority = 24,        /* 优先级 24 */
    .use_rtos = true            /* 使用 RTOS */
};

nx_startup_with_config(&config);
```

---

## 常见用例

### 用例 1: 驱动自动初始化

**场景**: UART 驱动需要在系统启动时自动初始化。

```c
/* uart_driver.c */
#include "nx_init.h"
#include "uart_hw.h"

static int uart_driver_init(void) {
    /* 配置 UART 硬件 */
    uart_hw_config_t config = {
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = UART_PARITY_NONE
    };
    
    /* 初始化 UART0 */
    if (uart_hw_init(UART0, &config) != 0) {
        return -1;
    }
    
    /* 注册设备 */
    uart_device_register(UART0);
    
    return 0;
}

/* 注册为驱动级别初始化 */
NX_INIT_DRIVER_EXPORT(uart_driver_init);
```

### 用例 2: 文件系统初始化

**场景**: 文件系统需要在驱动初始化后初始化。

```c
/* filesystem.c */
#include "nx_init.h"
#include "fs.h"

static int filesystem_init(void) {
    /* 挂载根文件系统 */
    if (fs_mount("/", "littlefs", 0) != 0) {
        return -1;
    }
    
    /* 创建必要的目录 */
    fs_mkdir("/data", 0755);
    fs_mkdir("/log", 0755);
    
    return 0;
}

/* 注册为组件级别（在驱动之后） */
NX_INIT_COMPONENT_EXPORT(filesystem_init);
```

### 用例 3: 网络栈初始化

**场景**: 网络栈需要在以太网驱动初始化后初始化。

```c
/* network.c */
#include "nx_init.h"
#include "lwip/init.h"
#include "netif/etharp.h"

static int network_init(void) {
    /* 初始化 lwIP */
    lwip_init();
    
    /* 配置网络接口 */
    struct netif netif;
    ip4_addr_t ipaddr, netmask, gw;
    
    IP4_ADDR(&ipaddr, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);
    
    netif_add(&netif, &ipaddr, &netmask, &gw, NULL, 
              ethernetif_init, ethernet_input);
    netif_set_default(&netif);
    netif_set_up(&netif);
    
    return 0;
}

/* 注册为组件级别 */
NX_INIT_COMPONENT_EXPORT(network_init);
```

### 用例 4: 应用配置加载

**场景**: 应用启动时加载配置文件。

```c
/* app_config.c */
#include "nx_init.h"
#include "config.h"

static int app_config_init(void) {
    /* 加载配置文件 */
    if (config_load("/data/app.conf") != 0) {
        /* 使用默认配置 */
        config_set_defaults();
    }
    
    /* 应用配置 */
    config_apply();
    
    return 0;
}

/* 注册为应用级别（最后执行） */
NX_INIT_APP_EXPORT(app_config_init);
```

### 用例 5: 板级时钟配置

**场景**: 在所有初始化之前配置系统时钟。

```c
/* board.c */
#include "nx_startup.h"

void nx_board_init(void) {
    /* 配置系统时钟为 168MHz */
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* 配置 HSE */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    /* 配置系统时钟 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | 
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | 
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
```

### 用例 6: FreeRTOS 集成

**场景**: 使用 FreeRTOS 作为操作系统。

```c
/* main.c */
#include "nx_startup.h"
#include "FreeRTOS.h"
#include "task.h"

/* OS 初始化 */
void nx_os_init(void) {
    /* FreeRTOS 内核会在 nx_startup 中自动启动 */
}

/* 应用任务 */
void app_task(void* param) {
    while (1) {
        /* 任务逻辑 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main(void) {
    /* 此函数在 FreeRTOS 任务中运行 */
    
    /* 创建应用任务 */
    xTaskCreate(app_task, "app", 1024, NULL, 2, NULL);
    
    /* 主任务逻辑 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

---

## 最佳实践

### 1. 初始化函数设计

#### ✅ 推荐做法

```c
/* 清晰的函数名 */
static int uart0_driver_init(void) {
    /* 单一职责 */
    return uart_hw_init(UART0);
}

/* 适当的错误处理 */
static int spi_driver_init(void) {
    int ret;
    
    ret = spi_hw_init();
    if (ret != 0) {
        return ret;  /* 返回具体错误码 */
    }
    
    return 0;
}

/* 幂等性 - 可以多次调用 */
static int gpio_init(void) {
    static bool initialized = false;
    
    if (initialized) {
        return 0;  /* 已初始化，直接返回 */
    }
    
    /* 初始化逻辑 */
    gpio_hw_config();
    
    initialized = true;
    return 0;
}
```

#### ❌ 避免做法

```c
/* 避免：函数名不清晰 */
static int init(void) {  /* 太泛化 */
    /* ... */
}

/* 避免：忽略错误 */
static int bad_init(void) {
    hw_init();  /* 没有检查返回值 */
    return 0;
}

/* 避免：过于复杂 */
static int complex_init(void) {
    /* 初始化太多不相关的东西 */
    uart_init();
    spi_init();
    i2c_init();
    /* 应该拆分为多个函数 */
    return 0;
}

/* 避免：阻塞操作 */
static int blocking_init(void) {
    while (!hw_ready()) {  /* 可能永久阻塞 */
        /* 等待 */
    }
    return 0;
}
```

### 2. 初始化级别选择

#### 选择合适的级别

```c
/* Level 1: BOARD - 最基础的硬件配置 */
NX_INIT_BOARD_EXPORT(system_clock_init);
NX_INIT_BOARD_EXPORT(power_management_init);

/* Level 2: PREV - 预初始化 */
NX_INIT_PREV_EXPORT(heap_init);
NX_INIT_PREV_EXPORT(debug_uart_init);

/* Level 3: BSP - 板级支持包 */
NX_INIT_BSP_EXPORT(gpio_mux_init);
NX_INIT_BSP_EXPORT(interrupt_init);

/* Level 4: DRIVER - 设备驱动 */
NX_INIT_DRIVER_EXPORT(uart_driver_init);
NX_INIT_DRIVER_EXPORT(spi_driver_init);

/* Level 5: COMPONENT - 中间件组件 */
NX_INIT_COMPONENT_EXPORT(filesystem_init);
NX_INIT_COMPONENT_EXPORT(network_init);

/* Level 6: APP - 应用初始化 */
NX_INIT_APP_EXPORT(app_config_init);
NX_INIT_APP_EXPORT(app_service_init);
```

### 3. 依赖关系管理

#### 明确依赖

```c
/* 驱动依赖 BSP */
/* bsp_gpio.c - Level 3 */
static int bsp_gpio_init(void) {
    /* GPIO 引脚配置 */
    return 0;
}
NX_INIT_BSP_EXPORT(bsp_gpio_init);

/* led_driver.c - Level 4 */
static int led_driver_init(void) {
    /* LED 驱动依赖 GPIO BSP */
    /* 因为在 Level 4，GPIO 已在 Level 3 初始化 */
    return 0;
}
NX_INIT_DRIVER_EXPORT(led_driver_init);
```

#### 避免循环依赖

```c
/* ❌ 错误：循环依赖 */
/* module_a.c */
extern int module_b_function(void);
static int module_a_init(void) {
    return module_b_function();  /* 依赖 B */
}
NX_INIT_DRIVER_EXPORT(module_a_init);

/* module_b.c */
extern int module_a_function(void);
static int module_b_init(void) {
    return module_a_function();  /* 依赖 A */
}
NX_INIT_DRIVER_EXPORT(module_b_init);

/* ✅ 正确：解耦依赖 */
/* 使用回调或事件机制解耦 */
```

### 4. 错误处理策略

#### 记录但继续

```c
static int optional_feature_init(void) {
    int ret = feature_hw_init();
    
    if (ret != 0) {
        /* 记录错误但不阻止系统启动 */
        log_warning("Optional feature init failed: %d", ret);
        return 0;  /* 返回成功，允许系统继续 */
    }
    
    return 0;
}
```

#### 关键失败

```c
static int critical_init(void) {
    int ret = critical_hw_init();
    
    if (ret != 0) {
        /* 关键初始化失败，返回错误 */
        log_error("Critical init failed: %d", ret);
        return ret;  /* 返回错误码 */
    }
    
    return 0;
}
```

#### 检查初始化状态

```c
int main(void) {
    /* 检查是否所有初始化都成功 */
    if (!nx_init_is_complete()) {
        nx_init_stats_t stats;
        nx_init_get_stats(&stats);
        
        log_error("Initialization failed:");
        log_error("  Total: %d", stats.total_count);
        log_error("  Failed: %d", stats.fail_count);
        log_error("  Last error: %d", stats.last_error);
        
        /* 决定如何处理 */
        if (stats.fail_count > 3) {
            /* 失败太多，进入安全模式 */
            enter_safe_mode();
        }
    }
    
    /* 正常运行 */
    return 0;
}
```

### 5. 调试技巧

#### 启用调试输出

```c
/* 在编译选项中定义 */
#define NX_INIT_DEBUG

/* 或在代码中定义 */
#define NX_INIT_DEBUG
#include "nx_init.h"
```

#### 打印初始化顺序

```c
#ifdef NX_INIT_DEBUG
static int debug_init_level_1(void) {
    printf("[INIT] Level 1: debug_init_level_1\n");
    return 0;
}
NX_INIT_BOARD_EXPORT(debug_init_level_1);

static int debug_init_level_4(void) {
    printf("[INIT] Level 4: debug_init_level_4\n");
    return 0;
}
NX_INIT_DRIVER_EXPORT(debug_init_level_4);
#endif
```

#### 测量初始化时间

```c
#include "nx_init.h"
#include <time.h>

static uint32_t init_start_time;
static uint32_t init_end_time;

static int timing_init_start(void) {
    init_start_time = get_tick_count();
    return 0;
}
NX_INIT_BOARD_EXPORT(timing_init_start);

static int timing_init_end(void) {
    init_end_time = get_tick_count();
    printf("Total init time: %u ms\n", 
           init_end_time - init_start_time);
    return 0;
}
NX_INIT_APP_EXPORT(timing_init_end);
```

### 6. 模块化设计

#### 每个驱动独立文件

```
drivers/
├── uart/
│   ├── uart_driver.c      /* 包含 NX_INIT_DRIVER_EXPORT */
│   └── uart_driver.h
├── spi/
│   ├── spi_driver.c       /* 包含 NX_INIT_DRIVER_EXPORT */
│   └── spi_driver.h
└── i2c/
    ├── i2c_driver.c       /* 包含 NX_INIT_DRIVER_EXPORT */
    └── i2c_driver.h
```

#### 驱动示例

```c
/* uart_driver.c */
#include "nx_init.h"
#include "uart_driver.h"

/* 驱动私有数据 */
static uart_dev_t uart_devices[UART_MAX_DEVICES];

/* 初始化函数 */
static int uart_driver_init(void) {
    /* 初始化所有 UART 设备 */
    for (int i = 0; i < UART_MAX_DEVICES; i++) {
        uart_hw_init(&uart_devices[i], i);
    }
    return 0;
}

/* 注册初始化 */
NX_INIT_DRIVER_EXPORT(uart_driver_init);

/* 公共 API */
int uart_open(int id) {
    /* ... */
}

int uart_write(int id, const void* data, size_t len) {
    /* ... */
}
```

### 7. 固件版本管理

#### 版本号规范

```c
/* 使用语义化版本 */
/* MAJOR.MINOR.PATCH.BUILD */

/* 主版本：不兼容的 API 变更 */
/* 次版本：向后兼容的功能新增 */
/* 修订版本：向后兼容的问题修正 */
/* 构建号：每次构建递增 */

NX_FIRMWARE_INFO_DEFINE(
    "Product Name",
    "VENDOR",
    NX_VERSION_ENCODE(1, 2, 3, 456),  /* 1.2.3.456 */
    0x12345678
);
```

#### 版本检查

```c
void check_firmware_version(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    
    if (info == NULL) {
        return;
    }
    
    uint8_t major = NX_VERSION_MAJOR(info->version);
    uint8_t minor = NX_VERSION_MINOR(info->version);
    
    /* 检查最低版本要求 */
    if (major < 2) {
        printf("警告：固件版本过低，建议升级\n");
    }
    
    /* 检查兼容性 */
    if (major != EXPECTED_MAJOR_VERSION) {
        printf("错误：固件版本不兼容\n");
    }
}
```

---

## API 参考

### 自动初始化 API

#### nx_init_run

```c
nx_status_t nx_init_run(void);
```

**描述**: 执行所有注册的初始化函数

**返回值**:
- `NX_OK` - 所有初始化成功
- `NX_ERR_GENERIC` - 至少一个初始化失败

**示例**:
```c
if (nx_init_run() != NX_OK) {
    /* 处理初始化失败 */
}
```

#### nx_init_get_stats

```c
nx_status_t nx_init_get_stats(nx_init_stats_t* stats);
```

**描述**: 获取初始化统计信息

**参数**:
- `stats` - 统计信息结构指针

**返回值**:
- `NX_OK` - 成功
- `NX_ERR_NULL_PTR` - stats 为 NULL

**示例**:
```c
nx_init_stats_t stats;
nx_init_get_stats(&stats);
printf("成功: %d, 失败: %d\n", 
       stats.success_count, stats.fail_count);
```

#### nx_init_is_complete

```c
bool nx_init_is_complete(void);
```

**描述**: 检查是否所有初始化都成功

**返回值**:
- `true` - 所有初始化成功
- `false` - 至少一个初始化失败

**示例**:
```c
if (!nx_init_is_complete()) {
    printf("警告：部分初始化失败\n");
}
```

### 启动框架 API

#### nx_startup

```c
void nx_startup(void);
```

**描述**: 执行系统启动序列

**注意**: 此函数不返回（在正常操作中）

#### nx_startup_with_config

```c
void nx_startup_with_config(const nx_startup_config_t* config);
```

**描述**: 使用自定义配置执行启动序列

**参数**:
- `config` - 启动配置（NULL 使用默认值）

**示例**:
```c
nx_startup_config_t config;
nx_startup_get_default_config(&config);
config.main_stack_size = 8192;
nx_startup_with_config(&config);
```

#### nx_startup_get_state

```c
nx_startup_state_t nx_startup_get_state(void);
```

**描述**: 获取当前启动状态

**返回值**: 当前启动状态枚举值

#### nx_startup_is_complete

```c
bool nx_startup_is_complete(void);
```

**描述**: 检查启动是否完成

**返回值**:
- `true` - 启动完成
- `false` - 启动未完成

#### nx_startup_get_default_config

```c
void nx_startup_get_default_config(nx_startup_config_t* config);
```

**描述**: 获取默认启动配置

**参数**:
- `config` - 配置结构指针

### 固件信息 API

#### nx_get_firmware_info

```c
const nx_firmware_info_t* nx_get_firmware_info(void);
```

**描述**: 获取固件信息指针

**返回值**:
- 固件信息结构指针
- `NULL` - 未定义固件信息

**示例**:
```c
const nx_firmware_info_t* info = nx_get_firmware_info();
if (info != NULL) {
    printf("产品: %s\n", info->product);
}
```

#### nx_get_version_string

```c
size_t nx_get_version_string(char* buf, size_t size);
```

**描述**: 获取格式化的版本字符串

**参数**:
- `buf` - 输出缓冲区
- `size` - 缓冲区大小

**返回值**: 写入的字符数（不包括 null 终止符）

**示例**:
```c
char version[32];
nx_get_version_string(version, sizeof(version));
printf("版本: %s\n", version);  /* 输出: "1.2.3.4" */
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
