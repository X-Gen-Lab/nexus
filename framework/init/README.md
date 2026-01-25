# Nexus Init Framework

静态注册表模式实现，提供编译期自动初始化机制和启动框架。

## 特性

- **零运行时开销**: 编译期确定所有初始化函数，无运行时注册
- **模块完全解耦**: 驱动无需修改核心代码即可注册初始化
- **分级初始化**: 6 个初始化级别，确保正确的依赖顺序
- **统一启动序列**: 标准化的系统启动流程
- **弱符号覆盖**: 灵活的板级和 OS 初始化定制
- **RTOS 集成**: 支持裸机和 RTOS 双模式
- **固件元数据**: 编译期嵌入版本和构建信息
- **多编译器支持**: GCC、Arm Compiler 5/6、IAR
- **跨平台**: ARM Cortex-M/A、RISC-V 等
- **错误统计**: 详细的初始化统计和错误追踪

## 概述

Init Framework 提供三个核心功能模块：

### 1. 自动初始化机制 (nx_init)

基于链接器段的编译期自动注册系统，支持分级初始化。

**工作原理**:
- 使用宏将初始化函数指针放入特定链接器段
- 链接器按段名排序，确定执行顺序
- 运行时遍历段中的函数指针并执行

**核心优势**:
- ✅ 零运行时开销 - 编译期确定所有初始化函数
- ✅ 模块解耦 - 驱动无需修改核心代码即可注册
- ✅ 有序执行 - 6 级初始化顺序保证依赖关系
- ✅ 跨编译器 - 支持 GCC、Arm Compiler、IAR

### 2. 启动框架 (nx_startup)

统一的系统启动序列，拦截程序入口点。

**工作原理**:
- 拦截程序入口点（entry 或 $Sub$main）
- 按顺序执行：board_init → os_init → init_run → main
- 支持裸机和 RTOS 双模式

**核心优势**:
- ✅ 统一入口 - 标准化启动流程
- ✅ 弱符号覆盖 - 灵活的板级初始化
- ✅ 双模式支持 - 裸机和 RTOS
- ✅ 可配置 - 自定义栈大小和优先级

### 3. 固件信息 (nx_firmware_info)

嵌入式固件元数据，支持工具提取。

**工作原理**:
- 使用宏在编译期定义固件信息结构
- 存储在独立的链接器段 `.nx_fw_info`
- 外部工具可直接从二进制文件提取

**核心优势**:
- ✅ 编译期嵌入 - 版本信息固化在二进制中
- ✅ 工具可提取 - 无需执行固件即可读取
- ✅ 版本编码 - 标准化版本号格式
- ✅ 独立段 - 专用链接器段存储

## 快速开始

### 最小示例

```c
#include "nx_init.h"
#include "nx_startup.h"

/* 1. 定义初始化函数 */
static int my_driver_init(void) {
    /* 初始化硬件 */
    uart_hw_init();
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

### 自定义板级初始化

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

### 固件版本信息

```c
#include "nx_firmware_info.h"

/* 定义固件信息 */
NX_FIRMWARE_INFO_DEFINE(
    "My Product",                      /* 产品名称 */
    "VENDOR",                          /* 厂商标识 */
    NX_VERSION_ENCODE(1, 0, 0, 0),     /* 版本 1.0.0.0 */
    0x12345678                         /* 固件密钥 */
);

int main(void) {
    /* 打印版本信息 */
    char version[32];
    nx_get_version_string(version, sizeof(version));
    printf("Firmware version: %s\n", version);
    
    return 0;
}
```

## 目录结构

```
framework/init/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── nx_init.h           # 初始化机制公共头文件
│   ├── nx_startup.h        # 启动框架头文件
│   └── nx_firmware_info.h  # 固件信息头文件
├── src/
│   ├── nx_init.c           # 初始化机制实现
│   ├── nx_startup.c        # 启动框架实现
│   └── nx_firmware_info.c  # 固件信息实现
└── docs/
    ├── README.md           # 文档索引
    ├── USER_GUIDE.md       # 详细使用指南
    ├── DESIGN.md           # 架构设计文档
    ├── PORTING_GUIDE.md    # 移植指南
    ├── TEST_GUIDE.md       # 测试指南
    ├── TROUBLESHOOTING.md  # 故障排查指南
    └── CHANGELOG.md        # 版本变更记录
```

## 自动初始化机制

### 初始化级别

系统支持 6 个初始化级别，按顺序执行：

| 级别 | 宏定义 | 用途 | 典型示例 |
|------|--------|------|----------|
| 1 | `NX_INIT_BOARD_EXPORT` | 板级初始化 | 时钟配置、电源管理 |
| 2 | `NX_INIT_PREV_EXPORT` | 预初始化 | 内存初始化、调试接口 |
| 3 | `NX_INIT_BSP_EXPORT` | BSP 初始化 | 外设配置、引脚复用 |
| 4 | `NX_INIT_DRIVER_EXPORT` | 驱动初始化 | UART、SPI、I2C 驱动 |
| 5 | `NX_INIT_COMPONENT_EXPORT` | 组件初始化 | 文件系统、网络栈 |
| 6 | `NX_INIT_APP_EXPORT` | 应用初始化 | 应用特定初始化 |

### 基本使用

```c
#include "nx_init.h"

/* 1. 定义初始化函数 */
static int uart_driver_init(void) {
    /* 初始化 UART 硬件 */
    uart_hw_config_t config = {
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = UART_PARITY_NONE
    };
    
    if (uart_hw_init(UART0, &config) != 0) {
        return -1;  /* 返回非零表示失败 */
    }
    
    return 0;  /* 返回 0 表示成功 */
}

/* 2. 注册为驱动级别初始化 */
NX_INIT_DRIVER_EXPORT(uart_driver_init);

/* 3. 系统启动时自动执行 */
```

### 多级别初始化示例

```c
/* 板级时钟初始化（Level 1）*/
static int board_clock_init(void) {
    SystemClock_Config();
    return 0;
}
NX_INIT_BOARD_EXPORT(board_clock_init);

/* BSP GPIO 初始化（Level 3）*/
static int bsp_gpio_init(void) {
    GPIO_Init();
    return 0;
}
NX_INIT_BSP_EXPORT(bsp_gpio_init);

/* UART 驱动初始化（Level 4）*/
static int uart_driver_init(void) {
    /* 依赖 GPIO 已初始化 */
    uart_hw_init();
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_driver_init);

/* 文件系统初始化（Level 5）*/
static int filesystem_init(void) {
    /* 依赖驱动已初始化 */
    fs_mount("/", "littlefs", 0);
    return 0;
}
NX_INIT_COMPONENT_EXPORT(filesystem_init);

/* 应用配置加载（Level 6）*/
static int app_config_init(void) {
    /* 依赖文件系统已初始化 */
    config_load("/data/app.conf");
    return 0;
}
NX_INIT_APP_EXPORT(app_config_init);
```

### 错误处理

```c
static int driver_init_with_error_handling(void) {
    int ret;
    
    /* 尝试初始化 */
    ret = hardware_init();
    if (ret != 0) {
        /* 记录错误 */
        printf("Hardware init failed: %d\n", ret);
        return -1;  /* 返回错误码 */
    }
    
    /* 验证初始化结果 */
    if (!hardware_is_ready()) {
        printf("Hardware not ready\n");
        return -2;
    }
    
    return 0;
}
NX_INIT_DRIVER_EXPORT(driver_init_with_error_handling);
```

### API 参考

| 函数 | 描述 |
|------|------|
| `nx_init_run()` | 执行所有注册的初始化函数 |
| `nx_init_get_stats(stats)` | 获取初始化统计信息 |
| `nx_init_is_complete()` | 检查是否所有初始化都成功 |

#### nx_init_run

```c
nx_status_t nx_init_run(void);
```

执行所有注册的初始化函数，按级别顺序执行。

**返回值**:
- `NX_OK` - 所有初始化成功
- `NX_ERR_GENERIC` - 至少一个初始化失败

**示例**:
```c
nx_status_t status = nx_init_run();
if (status != NX_OK) {
    printf("Initialization failed\n");
}
```

#### nx_init_get_stats

```c
nx_status_t nx_init_get_stats(nx_init_stats_t* stats);
```

获取初始化统计信息。

**参数**:
- `stats` - 统计信息结构指针

**返回值**:
- `NX_OK` - 成功
- `NX_ERR_NULL_PTR` - stats 为 NULL

**示例**:
```c
nx_init_stats_t stats;
nx_init_get_stats(&stats);

printf("Total: %d, Success: %d, Failed: %d\n",
       stats.total_count, stats.success_count, stats.fail_count);

if (stats.fail_count > 0) {
    printf("Last error: %d\n", stats.last_error);
#ifdef NX_INIT_DEBUG
    printf("Failed function: %s\n", stats.last_failed);
#endif
}
```

#### nx_init_is_complete

```c
bool nx_init_is_complete(void);
```

检查是否所有初始化都成功。

**返回值**:
- `true` - 所有初始化成功
- `false` - 至少一个初始化失败

**示例**:
```c
if (!nx_init_is_complete()) {
    printf("Warning: Some initializations failed\n");
    /* 进入安全模式或降级运行 */
}
```

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
    │   └─→ 时钟配置、电源管理、GPIO 初始化
    │
    ├─→ nx_os_init()         [弱符号，用户可覆盖]
    │   └─→ RTOS 内核初始化、调度器配置
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
        └─→ 应用主循环
```

### 裸机模式

```c
#include "nx_startup.h"

/* 覆盖板级初始化 */
void nx_board_init(void) {
    /* 配置系统时钟 */
    SystemClock_Config();
    
    /* 配置 GPIO */
    GPIO_Init();
    
    /* 配置中断优先级 */
    NVIC_SetPriorityGrouping(4);
}

/* 主函数 */
int main(void) {
    /* 此时所有初始化已完成 */
    
    /* 应用主循环 */
    while (1) {
        /* 应用逻辑 */
        process_tasks();
        
        /* 低功耗模式 */
        __WFI();
    }
    
    return 0;
}
```

### RTOS 模式

#### FreeRTOS 集成

```c
#include "nx_startup.h"
#include "FreeRTOS.h"
#include "task.h"

/* OS 初始化 */
void nx_os_init(void) {
    /* FreeRTOS 会在 nx_startup 中自动启动 */
}

/* 应用任务 */
void app_task(void* param) {
    while (1) {
        /* 任务逻辑 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 主函数（在 FreeRTOS 任务中运行）*/
int main(void) {
    /* 创建应用任务 */
    xTaskCreate(app_task, "app", 1024, NULL, 2, NULL);
    
    /* 主任务逻辑 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

#### RT-Thread 集成

```c
#include "nx_startup.h"
#include <rtthread.h>

/* OS 初始化 */
void nx_os_init(void) {
    /* RT-Thread 会在 nx_startup 中自动启动 */
}

int main(void) {
    /* 创建线程 */
    rt_thread_t thread = rt_thread_create("app",
                                          app_thread_entry,
                                          RT_NULL,
                                          2048,
                                          20,
                                          10);
    if (thread != RT_NULL) {
        rt_thread_startup(thread);
    }
    
    return 0;
}
```

### 自定义启动配置

```c
#include "nx_startup.h"

int entry(void) {
    nx_startup_config_t config;
    
    /* 获取默认配置 */
    nx_startup_get_default_config(&config);
    
    /* 自定义配置 */
    config.main_stack_size = 8192;  /* 8KB 栈 */
    config.main_priority = 24;      /* 高优先级 */
    config.use_rtos = true;         /* 使用 RTOS */
    
    /* 使用自定义配置启动 */
    nx_startup_with_config(&config);
    
    return 0;
}
```

### 启动状态查询

```c
#include "nx_startup.h"

void check_startup_status(void) {
    /* 获取当前状态 */
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
    
    /* 简单检查 */
    if (nx_startup_is_complete()) {
        printf("系统启动完成\n");
    }
}
```

### API 参考

| 函数 | 描述 |
|------|------|
| `nx_startup()` | 执行系统启动序列 |
| `nx_startup_with_config(config)` | 使用自定义配置启动 |
| `nx_startup_get_state()` | 获取当前启动状态 |
| `nx_startup_is_complete()` | 检查启动是否完成 |
| `nx_startup_get_default_config(config)` | 获取默认启动配置 |

#### 弱符号函数

| 函数 | 描述 | 默认实现 |
|------|------|----------|
| `nx_board_init()` | 板级初始化 | 空函数 |
| `nx_os_init()` | OS 初始化 | 空函数 |

## 固件信息

### 定义固件信息

```c
#include "nx_firmware_info.h"

/* 在主文件中定义（通常是 main.c）*/
NX_FIRMWARE_INFO_DEFINE(
    "Nexus IoT Device",                /* 产品名称（最多 31 字符）*/
    "NEXUS",                           /* 厂商标识（最多 15 字符）*/
    NX_VERSION_ENCODE(2, 1, 5, 123),   /* 版本 2.1.5.123 */
    0x12345678                         /* 固件密钥/校验和 */
);
```

### 版本编码和解码

#### 编码版本号

```c
/* 版本格式: MAJOR.MINOR.PATCH.BUILD */

/* 版本 1.0.0.0 */
uint32_t ver1 = NX_VERSION_ENCODE(1, 0, 0, 0);

/* 版本 2.1.5.123 */
uint32_t ver2 = NX_VERSION_ENCODE(2, 1, 5, 123);

/* 版本 255.255.255.255（最大值）*/
uint32_t ver_max = NX_VERSION_ENCODE(255, 255, 255, 255);
```

#### 解码版本号

```c
uint32_t version = NX_VERSION_ENCODE(2, 1, 5, 123);

/* 提取各部分 */
uint8_t major = NX_VERSION_MAJOR(version);  /* 2 */
uint8_t minor = NX_VERSION_MINOR(version);  /* 1 */
uint8_t patch = NX_VERSION_PATCH(version);  /* 5 */
uint8_t build = NX_VERSION_BUILD(version);  /* 123 */

/* 打印版本 */
printf("Version: %d.%d.%d.%d\n", major, minor, patch, build);
```

### 读取固件信息

#### 获取固件信息结构

```c
#include "nx_firmware_info.h"

void print_firmware_info(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    
    if (info == NULL) {
        printf("固件信息未定义\n");
        return;
    }
    
    /* 打印基本信息 */
    printf("产品: %s\n", info->product);
    printf("厂商: %s\n", info->factory);
    printf("构建日期: %s\n", info->date);
    printf("构建时间: %s\n", info->time);
    
    /* 打印版本 */
    printf("版本: %d.%d.%d.%d\n",
           NX_VERSION_MAJOR(info->version),
           NX_VERSION_MINOR(info->version),
           NX_VERSION_PATCH(info->version),
           NX_VERSION_BUILD(info->version));
    
    /* 打印密钥 */
    printf("密钥: 0x%08X\n", info->key);
}
```

#### 获取版本字符串

```c
void print_version(void) {
    char version_str[32];
    
    /* 获取格式化的版本字符串 */
    size_t len = nx_get_version_string(version_str, sizeof(version_str));
    
    if (len > 0) {
        printf("固件版本: %s\n", version_str);  /* 输出: "2.1.5.123" */
    } else {
        printf("固件信息未定义\n");
    }
}
```

### 版本比较

```c
bool is_version_compatible(uint32_t required_version) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    if (info == NULL) {
        return false;
    }
    
    uint8_t current_major = NX_VERSION_MAJOR(info->version);
    uint8_t required_major = NX_VERSION_MAJOR(required_version);
    
    /* 主版本号必须匹配 */
    if (current_major != required_major) {
        return false;
    }
    
    /* 次版本号必须大于等于要求 */
    uint8_t current_minor = NX_VERSION_MINOR(info->version);
    uint8_t required_minor = NX_VERSION_MINOR(required_version);
    
    return current_minor >= required_minor;
}
```

### 固件完整性验证

```c
uint32_t calculate_firmware_checksum(void) {
    /* 计算固件校验和 */
    uint32_t checksum = 0;
    /* ... 校验和计算逻辑 ... */
    return checksum;
}

bool verify_firmware_integrity(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    if (info == NULL) {
        return false;
    }
    
    /* 计算当前固件校验和 */
    uint32_t calculated = calculate_firmware_checksum();
    
    /* 与嵌入的密钥比较 */
    if (calculated != info->key) {
        printf("固件完整性验证失败\n");
        return false;
    }
    
    return true;
}
```

### 外部工具提取

固件信息存储在独立的 `.nx_fw_info` 段中，可以使用外部工具提取：

```bash
# 使用 objdump 查看固件信息段
arm-none-eabi-objdump -s -j .nx_fw_info firmware.elf

# 使用 readelf 查看
arm-none-eabi-readelf -x .nx_fw_info firmware.elf

# 使用 strings 查找产品名
arm-none-eabi-strings firmware.elf | grep "Nexus"

# 自定义 Python 脚本提取
python extract_fw_info.py firmware.bin
```

### API 参考

| 函数 | 描述 |
|------|------|
| `nx_get_firmware_info()` | 获取固件信息指针 |
| `nx_get_version_string(buf, size)` | 获取版本字符串 |

#### 版本编码宏

| 宏 | 描述 |
|----|------|
| `NX_VERSION_ENCODE(major, minor, patch, build)` | 编码版本号 |
| `NX_VERSION_MAJOR(ver)` | 提取主版本号 |
| `NX_VERSION_MINOR(ver)` | 提取次版本号 |
| `NX_VERSION_PATCH(ver)` | 提取修订版本号 |
| `NX_VERSION_BUILD(ver)` | 提取构建号 |

## 链接器配置

Init Framework 需要正确的链接器配置才能工作。

### GCC 链接器脚本

在链接器脚本中添加以下段定义：

```ld
/* linker_script.ld */

SECTIONS
{
    /* 代码段 */
    .text : {
        *(.text*)
    } > FLASH
    
    /* 初始化函数段 */
    .nx_init_fn : {
        /* 起始边界标记 */
        PROVIDE(__nx_init_fn_start = .);
        
        /* 按级别排序（关键！）*/
        KEEP(*(.nx_init_fn.0))  /* 边界标记 */
        KEEP(*(.nx_init_fn.1))  /* BOARD */
        KEEP(*(.nx_init_fn.2))  /* PREV */
        KEEP(*(.nx_init_fn.3))  /* BSP */
        KEEP(*(.nx_init_fn.4))  /* DRIVER */
        KEEP(*(.nx_init_fn.5))  /* COMPONENT */
        KEEP(*(.nx_init_fn.6))  /* APP */
        KEEP(*(.nx_init_fn.7))  /* 边界标记 */
        
        /* 结束边界标记 */
        PROVIDE(__nx_init_fn_end = .);
    } > FLASH
    
    /* 固件信息段 */
    .nx_fw_info : {
        KEEP(*(.nx_fw_info))
    } > FLASH
    
    /* 其他段... */
}
```

**关键点**:
- 使用 `KEEP()` 防止链接器优化移除
- 按数字顺序排列段，确保执行顺序
- 提供边界符号 `__nx_init_fn_start` 和 `__nx_init_fn_end`

### Arm Compiler Scatter File

```scatter
; scatter_file.sct

LR_IROM1 0x08000000 0x00080000 {
    ; 代码段
    ER_IROM1 0x08000000 0x00080000 {
        *.o (RESET, +First)
        *(InRoot$$Sections)
        .ANY (+RO)
    }
    
    ; 初始化函数段
    nx_init_fn +0 {
        *(.nx_init_fn.0)
        *(.nx_init_fn.1)
        *(.nx_init_fn.2)
        *(.nx_init_fn.3)
        *(.nx_init_fn.4)
        *(.nx_init_fn.5)
        *(.nx_init_fn.6)
        *(.nx_init_fn.7)
    }
    
    ; 固件信息段
    nx_fw_info +0 {
        *(.nx_fw_info)
    }
    
    ; 数据段
    RW_IRAM1 0x20000000 0x00020000 {
        .ANY (+RW +ZI)
    }
}
```

### IAR 链接器配置

```icf
/* linker_config.icf */

/* 内存区域定义 */
define symbol __ICFEDIT_region_ROM_start__ = 0x08000000;
define symbol __ICFEDIT_region_ROM_end__   = 0x0807FFFF;
define symbol __ICFEDIT_region_RAM_start__ = 0x20000000;
define symbol __ICFEDIT_region_RAM_end__   = 0x2001FFFF;

define region ROM_region = mem:[from __ICFEDIT_region_ROM_start__ 
                                to __ICFEDIT_region_ROM_end__];
define region RAM_region = mem:[from __ICFEDIT_region_RAM_start__ 
                                to __ICFEDIT_region_RAM_end__];

/* 初始化函数段 */
define block nx_init_fn with alignment = 4 {
    section .nx_init_fn.0,
    section .nx_init_fn.1,
    section .nx_init_fn.2,
    section .nx_init_fn.3,
    section .nx_init_fn.4,
    section .nx_init_fn.5,
    section .nx_init_fn.6,
    section .nx_init_fn.7
};

/* 固件信息段 */
define block nx_fw_info with alignment = 4 {
    section .nx_fw_info
};

/* 段放置 */
place in ROM_region { block nx_init_fn };
place in ROM_region { block nx_fw_info };
place in ROM_region { readonly };
place in RAM_region { readwrite, zeroinit };
```

### 验证链接器配置

编译后验证段是否正确：

```bash
# 查看段信息
arm-none-eabi-objdump -h firmware.elf | grep nx_init_fn

# 查看符号
arm-none-eabi-nm firmware.elf | grep __nx_init_fn

# 查看段内容
arm-none-eabi-objdump -s -j .nx_init_fn firmware.elf

# 查看固件信息
arm-none-eabi-objdump -s -j .nx_fw_info firmware.elf
```

## 编译器支持

Init Framework 支持多种编译器，通过条件编译适配不同的段属性语法。

### 支持的编译器

| 编译器 | 版本要求 | 段属性语法 | 边界符号 | 状态 |
|--------|----------|-----------|----------|------|
| GCC | ≥ 7.0 | `__attribute__((section))` | `__nx_init_fn_start/end` | ✅ 完全支持 |
| Clang | ≥ 10.0 | `__attribute__((section))` | `__nx_init_fn_start/end` | ✅ 完全支持 |
| Arm Compiler 5 | ≥ 5.06 | `__attribute__((section))` | `Image$nx_init_fn$Base/Limit` | ✅ 完全支持 |
| Arm Compiler 6 | ≥ 6.0 | `__attribute__((section))` | `Image$nx_init_fn$Base/Limit` | ✅ 完全支持 |
| IAR | ≥ 8.0 | `_Pragma("location=")` | `__section_begin/end` | ✅ 完全支持 |
| MSVC | 2019+ | 数组模式 | 数组边界 | ⚠️ 测试模式 |

### 编译器特定宏

框架会自动检测编译器并使用相应的语法：

```c
/* GCC / Clang */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    #define NX_SECTION(x) __attribute__((section(x)))
    #define NX_USED __attribute__((used))
    extern const nx_init_fn_t __nx_init_fn_start[];
    extern const nx_init_fn_t __nx_init_fn_end[];

/* Arm Compiler 5/6 */
#elif defined(__ARMCC_VERSION)
    #define NX_SECTION(x) __attribute__((section(x)))
    #define NX_USED __attribute__((used))
    extern const nx_init_fn_t Image$nx_init_fn$Base[];
    extern const nx_init_fn_t Image$nx_init_fn$Limit[];

/* IAR */
#elif defined(__ICCARM__)
    #define NX_SECTION(x) _Pragma(#x)
    #define NX_USED
    #pragma section = "nx_init_fn"
    #define NX_INIT_FN_START \
        ((const nx_init_fn_t*)__section_begin("nx_init_fn"))
    #define NX_INIT_FN_END \
        ((const nx_init_fn_t*)__section_end("nx_init_fn"))
#endif
```

### 宏展开示例

```c
/* 用户代码 */
static int uart_init(void) {
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_init);

/* GCC 展开后 */
__attribute__((used))
__attribute__((section(".nx_init_fn.4")))
const nx_init_fn_t _nx_init_uart_init = uart_init;

/* Arm Compiler 展开后 */
__attribute__((used))
__attribute__((section(".nx_init_fn.4")))
const nx_init_fn_t _nx_init_uart_init = uart_init;

/* IAR 展开后 */
_Pragma("location=\".nx_init_fn.4\"")
const nx_init_fn_t _nx_init_uart_init = uart_init;
```

## 错误处理

### 初始化错误策略

Init Framework 采用"记录但继续"的错误处理策略：

- ✅ 初始化函数返回非零值时，错误被记录
- ✅ 执行继续，不会中断后续初始化
- ✅ 提供统计信息供应用决策
- ✅ 支持调试模式查看失败函数

### 获取统计信息

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
        printf("  最后错误码: %d\n", stats.last_error);
        
#ifdef NX_INIT_DEBUG
        /* 调试模式下可以看到失败的函数名 */
        printf("  失败函数: %s\n", stats.last_failed);
#endif
    }
}
```

### 检查初始化完成

```c
int main(void) {
    /* 检查是否所有初始化都成功 */
    if (!nx_init_is_complete()) {
        printf("警告：部分初始化失败\n");
        
        /* 获取详细信息 */
        nx_init_stats_t stats;
        nx_init_get_stats(&stats);
        
        /* 根据失败数量决定策略 */
        if (stats.fail_count > 3) {
            /* 失败太多，进入安全模式 */
            enter_safe_mode();
        } else {
            /* 少量失败，降级运行 */
            run_degraded_mode();
        }
    }
    
    /* 正常运行 */
    return 0;
}
```

### 错误码定义

| 错误码 | 描述 | 处理建议 |
|--------|------|----------|
| `0` | 成功 | 继续执行 |
| `-1` | 通用错误 | 检查硬件连接 |
| `-2` | 资源不足 | 增加内存或减少功能 |
| `-3` | 超时 | 检查时钟配置 |
| `-4` | 硬件故障 | 检查硬件状态 |
| 其他 | 自定义错误 | 参考模块文档 |

### 错误处理示例

```c
/* 关键初始化 - 失败时返回错误 */
static int critical_init(void) {
    if (hardware_init() != 0) {
        printf("关键硬件初始化失败\n");
        return -1;  /* 返回错误 */
    }
    return 0;
}
NX_INIT_BOARD_EXPORT(critical_init);

/* 可选初始化 - 失败时记录但继续 */
static int optional_init(void) {
    if (feature_init() != 0) {
        printf("可选功能初始化失败，继续运行\n");
        return 0;  /* 返回成功，允许系统继续 */
    }
    return 0;
}
NX_INIT_COMPONENT_EXPORT(optional_init);

/* 带重试的初始化 */
static int init_with_retry(void) {
    int retry = 3;
    
    while (retry-- > 0) {
        if (hardware_init() == 0) {
            return 0;  /* 成功 */
        }
        delay_ms(100);  /* 延迟后重试 */
    }
    
    printf("初始化失败，已重试 3 次\n");
    return -1;  /* 失败 */
}
NX_INIT_DRIVER_EXPORT(init_with_retry);
```

### 启动状态码

| 状态 | 描述 |
|------|------|
| `NX_OK` | 成功 |
| `NX_ERR_GENERIC` | 通用错误 |
| `NX_ERR_NULL_PTR` | 空指针 |
| `NX_ERR_INVALID_PARAM` | 无效参数 |

## 调试支持

### 启用调试模式

定义 `NX_INIT_DEBUG` 宏启用详细的调试输出：

```c
/* 方式 1: 在代码中定义 */
#define NX_INIT_DEBUG
#include "nx_init.h"

/* 方式 2: 编译选项 */
/* gcc -DNX_INIT_DEBUG ... */
```

调试模式下会打印：
- 每个初始化函数的名称
- 执行结果（成功/失败）
- 错误码
- 执行时间（可选）

### 调试输出示例

```
[INIT] Starting initialization...
[INIT] Level 1: board_clock_init ... OK
[INIT] Level 1: board_power_init ... OK
[INIT] Level 3: bsp_gpio_init ... OK
[INIT] Level 4: uart_driver_init ... OK
[INIT] Level 4: spi_driver_init ... FAILED (error: -1)
[INIT] Level 5: filesystem_init ... OK
[INIT] Level 6: app_config_init ... OK
[INIT] Initialization complete: 6/7 succeeded, 1 failed
```

### 测量初始化时间

```c
#include "nx_init.h"
#include <time.h>

static uint32_t g_init_start_time;
static uint32_t g_init_end_time;

/* 在最早的级别记录开始时间 */
static int timing_start(void) {
    g_init_start_time = get_tick_count();
    return 0;
}
NX_INIT_BOARD_EXPORT(timing_start);

/* 在最晚的级别记录结束时间 */
static int timing_end(void) {
    g_init_end_time = get_tick_count();
    uint32_t elapsed = g_init_end_time - g_init_start_time;
    printf("Total initialization time: %u ms\n", elapsed);
    return 0;
}
NX_INIT_APP_EXPORT(timing_end);
```

### 单个函数计时

```c
static int timed_init(void) {
    uint32_t start = get_tick_count();
    
    /* 初始化代码 */
    hardware_init();
    
    uint32_t elapsed = get_tick_count() - start;
    printf("timed_init took %u ms\n", elapsed);
    
    return 0;
}
NX_INIT_DRIVER_EXPORT(timed_init);
```

### 使用断点调试

```c
/* 在初始化函数中设置断点 */
static int debug_init(void) {
    /* 在这里设置断点 */
    __asm("nop");  /* 方便设置断点的位置 */
    
    /* 初始化代码 */
    int ret = hardware_init();
    
    /* 检查返回值 */
    if (ret != 0) {
        __asm("nop");  /* 错误时的断点 */
    }
    
    return ret;
}
```

### 打印初始化顺序

```c
#ifdef NX_INIT_DEBUG
static int debug_level_1(void) {
    printf("[INIT] Level 1 function executed\n");
    return 0;
}
NX_INIT_BOARD_EXPORT(debug_level_1);

static int debug_level_4(void) {
    printf("[INIT] Level 4 function executed\n");
    return 0;
}
NX_INIT_DRIVER_EXPORT(debug_level_4);
#endif
```

### 查看初始化函数表

```bash
# 使用 objdump 查看初始化函数段
arm-none-eabi-objdump -s -j .nx_init_fn firmware.elf

# 查看符号表
arm-none-eabi-nm firmware.elf | grep _nx_init_

# 查看段信息
arm-none-eabi-readelf -S firmware.elf | grep nx_init_fn

# 反汇编初始化函数
arm-none-eabi-objdump -d firmware.elf | grep -A 10 uart_init
```

## 完整示例

### 基础示例

```c
#include "nx_init.h"
#include "nx_startup.h"
#include "nx_firmware_info.h"

/*---------------------------------------------------------------------------*/
/* 固件信息定义                                                              */
/*---------------------------------------------------------------------------*/

NX_FIRMWARE_INFO_DEFINE(
    "Nexus Demo Device",
    "NEXUS",
    NX_VERSION_ENCODE(1, 0, 0, 0),
    0xDEADBEEF
);

/*---------------------------------------------------------------------------*/
/* 板级初始化（Level 1）                                                     */
/*---------------------------------------------------------------------------*/

static int board_clock_init(void) {
    /* 配置系统时钟为 168MHz */
    SystemClock_Config();
    return 0;
}
NX_INIT_BOARD_EXPORT(board_clock_init);

static int board_power_init(void) {
    /* 配置电源管理 */
    PWR_Config();
    return 0;
}
NX_INIT_BOARD_EXPORT(board_power_init);

/*---------------------------------------------------------------------------*/
/* BSP 初始化（Level 3）                                                     */
/*---------------------------------------------------------------------------*/

static int bsp_gpio_init(void) {
    /* 配置 GPIO 引脚 */
    GPIO_Init();
    return 0;
}
NX_INIT_BSP_EXPORT(bsp_gpio_init);

/*---------------------------------------------------------------------------*/
/* 驱动初始化（Level 4）                                                     */
/*---------------------------------------------------------------------------*/

static int uart_driver_init(void) {
    /* 初始化 UART0 */
    uart_config_t config = {
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = UART_PARITY_NONE
    };
    
    if (uart_init(UART0, &config) != 0) {
        return -1;
    }
    
    printf("UART initialized\n");
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_driver_init);

static int spi_driver_init(void) {
    /* 初始化 SPI */
    spi_config_t config = {
        .mode = SPI_MODE_MASTER,
        .speed = 1000000,
        .bits = 8
    };
    
    if (spi_init(SPI1, &config) != 0) {
        return -1;
    }
    
    return 0;
}
NX_INIT_DRIVER_EXPORT(spi_driver_init);

/*---------------------------------------------------------------------------*/
/* 组件初始化（Level 5）                                                     */
/*---------------------------------------------------------------------------*/

static int filesystem_init(void) {
    /* 挂载文件系统 */
    if (fs_mount("/", "littlefs", 0) != 0) {
        printf("Filesystem mount failed\n");
        return -1;
    }
    
    /* 创建必要的目录 */
    fs_mkdir("/data", 0755);
    fs_mkdir("/log", 0755);
    
    return 0;
}
NX_INIT_COMPONENT_EXPORT(filesystem_init);

/*---------------------------------------------------------------------------*/
/* 应用初始化（Level 6）                                                     */
/*---------------------------------------------------------------------------*/

static int app_config_init(void) {
    /* 加载应用配置 */
    if (config_load("/data/app.conf") != 0) {
        printf("Using default configuration\n");
        config_set_defaults();
    }
    
    return 0;
}
NX_INIT_APP_EXPORT(app_config_init);

/*---------------------------------------------------------------------------*/
/* 板级初始化覆盖                                                            */
/*---------------------------------------------------------------------------*/

void nx_board_init(void) {
    /* 早期硬件初始化 */
    /* 在自动初始化之前执行 */
}

/*---------------------------------------------------------------------------*/
/* 主函数                                                                    */
/*---------------------------------------------------------------------------*/

int main(void) {
    /* 打印固件信息 */
    char version[32];
    nx_get_version_string(version, sizeof(version));
    printf("Firmware: %s\n", version);
    
    /* 检查初始化状态 */
    if (!nx_init_is_complete()) {
        printf("Warning: Some initializations failed\n");
        
        nx_init_stats_t stats;
        nx_init_get_stats(&stats);
        printf("Success: %d/%d\n", stats.success_count, stats.total_count);
    }
    
    /* 应用主循环 */
    while (1) {
        /* 处理任务 */
        process_tasks();
        
        /* 低功耗模式 */
        __WFI();
    }
    
    return 0;
}
```

### RTOS 示例

```c
#include "nx_init.h"
#include "nx_startup.h"
#include "FreeRTOS.h"
#include "task.h"

/*---------------------------------------------------------------------------*/
/* 固件信息                                                                  */
/*---------------------------------------------------------------------------*/

NX_FIRMWARE_INFO_DEFINE(
    "RTOS Demo",
    "NEXUS",
    NX_VERSION_ENCODE(1, 0, 0, 0),
    0x12345678
);

/*---------------------------------------------------------------------------*/
/* 驱动初始化                                                                */
/*---------------------------------------------------------------------------*/

static int uart_init_func(void) {
    uart_hw_init();
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_init_func);

/*---------------------------------------------------------------------------*/
/* OS 初始化                                                                 */
/*---------------------------------------------------------------------------*/

void nx_os_init(void) {
    /* FreeRTOS 会在 nx_startup 中自动启动 */
}

/*---------------------------------------------------------------------------*/
/* 应用任务                                                                  */
/*---------------------------------------------------------------------------*/

void led_task(void* param) {
    while (1) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void sensor_task(void* param) {
    while (1) {
        float temp = read_temperature();
        printf("Temperature: %.1f C\n", temp);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/*---------------------------------------------------------------------------*/
/* 主函数                                                                    */
/*---------------------------------------------------------------------------*/

int main(void) {
    /* 创建任务 */
    xTaskCreate(led_task, "led", 256, NULL, 1, NULL);
    xTaskCreate(sensor_task, "sensor", 512, NULL, 2, NULL);
    
    /* 主任务逻辑 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return 0;
}
```

## 编译时配置

| 宏定义 | 默认值 | 描述 |
|--------|--------|------|
| `NX_INIT_DEBUG` | 未定义 | 启用调试输出 |
| `NX_STARTUP_MAIN_STACK_SIZE` | `4096` | 主线程栈大小（字节）|
| `NX_STARTUP_MAIN_PRIORITY` | `16` | 主线程优先级（0-31）|
| `NX_STARTUP_TEST_MODE` | 未定义 | 启用测试模式 |

### 配置示例

```c
/* 在编译选项中定义 */
/* gcc -DNX_INIT_DEBUG -DNX_STARTUP_MAIN_STACK_SIZE=8192 ... */

/* 或在代码中定义 */
#define NX_INIT_DEBUG
#define NX_STARTUP_MAIN_STACK_SIZE 8192
#define NX_STARTUP_MAIN_PRIORITY 24

#include "nx_init.h"
#include "nx_startup.h"
```

## 依赖

Init Framework 的依赖最小化：

### 必需依赖

- **标准 C 库**: `stdint.h`, `stdbool.h`, `stddef.h`
- **HAL 类型定义**: `hal/nx_types.h`, `hal/nx_status.h`

### 可选依赖

- **OSAL**: 如果使用 RTOS 模式（用于创建主任务）
- **printf**: 如果启用调试模式（可替换为自定义日志函数）

### 无依赖模式

Init Framework 可以在完全无依赖的环境中工作：

```c
/* 裸机环境，无 RTOS，无 printf */
#include "nx_init.h"

static int minimal_init(void) {
    /* 最小化初始化 */
    return 0;
}
NX_INIT_DRIVER_EXPORT(minimal_init);

int main(void) {
    /* 无需任何外部依赖 */
    while (1);
    return 0;
}
```

## 性能指标

### 内存占用

| 项目 | 大小 | 说明 |
|------|------|------|
| 代码段 | ~1.5 KB | 核心实现代码 |
| 数据段 | ~100 bytes | 全局变量和状态 |
| 初始化函数表 | 4 bytes × N | N = 初始化函数数量 |
| 固件信息 | 80 bytes | 固件元数据 |

### 执行时间

| 操作 | 时间 | 说明 |
|------|------|------|
| `nx_init_run()` | ~10-20 μs | 100 个函数的遍历开销 |
| 单个初始化函数 | ~0.1-1 μs | 函数调用开销 |
| 完整启动序列 | 10-100 ms | 取决于初始化函数实现 |

### 优化建议

1. **减少初始化函数数量** - 合并相关初始化
2. **延迟初始化** - 非关键功能延迟到需要时
3. **并行初始化** - 在 RTOS 中使用多任务
4. **快速路径** - 关键路径优先初始化

## 示例应用

完整示例请参考：

- `applications/blinky/` - 最小 LED 闪烁示例
- `applications/shell_demo/` - 命令行示例
- `applications/freertos_demo/` - FreeRTOS 集成示例

## 文档

完整文档请参考 `docs/` 目录：

- **[README.md](docs/README.md)** - 文档索引和导航

- **[USER_GUIDE.md](docs/USER_GUIDE.md)** - 详细使用指南
  - 快速开始和基本概念
  - 自动初始化机制详解
  - 启动框架使用方法
  - 固件信息管理
  - 配置选项说明
  - 常见用例和最佳实践
  - 完整 API 参考

- **[DESIGN.md](docs/DESIGN.md)** - 架构设计文档
  - 系统架构和模块关系
  - 自动初始化机制原理
  - 启动框架设计
  - 固件信息设计
  - 设计决策和权衡
  - 性能分析
  - 安全考虑

- **[PORTING_GUIDE.md](docs/PORTING_GUIDE.md)** - 移植指南
  - 编译器适配方法
  - 链接器配置详解
  - 平台差异说明
  - 移植步骤和检查清单
  - 常见问题解答
  - 平台示例（STM32、ESP32、RISC-V）

- **[TEST_GUIDE.md](docs/TEST_GUIDE.md)** - 测试指南
  - 测试策略和覆盖率要求
  - 单元测试示例（15+ 个测试用例）
  - 集成测试场景
  - 性能测试和基准
  - 测试工具和框架
  - CI/CD 集成配置
  - 测试报告模板

- **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化顺序问题
  - 链接器配置问题
  - 编译器兼容性问题
  - 启动框架问题
  - 固件信息问题
  - 性能问题
  - 调试技巧和工具
  - 平台特定问题
  - 最佳实践建议

- **[CHANGELOG.md](docs/CHANGELOG.md)** - 版本变更记录
  - 版本历史
  - 新增功能
  - 问题修复
  - 升级指南

## 常见问题

### Q: 初始化函数未执行？

**A**: 检查链接器脚本是否正确配置，使用 `objdump` 查看段是否存在：
```bash
arm-none-eabi-objdump -h firmware.elf | grep nx_init_fn
```

### Q: 初始化顺序错误？

**A**: 确保使用了正确的级别宏，参考[初始化级别表](#初始化级别)。

### Q: 如何在 RTOS 中使用？

**A**: 实现 `nx_os_init()` 函数，参考 [RTOS 模式](#rtos-模式)。

### Q: 如何调试初始化问题？

**A**: 启用 `NX_INIT_DEBUG` 宏，参考[调试支持](#调试支持)。

### Q: 支持哪些编译器？

**A**: 支持 GCC、Clang、Arm Compiler 5/6、IAR，参考[编译器支持](#编译器支持)。

更多问题请参考 [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)。

## 相关模块

- **[Config Manager](../config/)** - 配置管理框架
- **[Device Registry](../../hal/include/hal/nx_device_registry.h)** - 设备静态注册
- **[OSAL](../../osal/)** - 操作系统抽象层
- **[HAL](../../hal/)** - 硬件抽象层

## 许可证

Copyright (c) 2026 Nexus Team

---

**版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
