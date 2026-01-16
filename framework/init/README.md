# Nexus Init Framework

静态注册表模式实现，提供编译期自动初始化机制和启动框架。

## 概述

Init Framework 提供三个核心功能：

1. **自动初始化机制 (nx_init)** - 基于链接器段的分级初始化系统
2. **启动框架 (nx_startup)** - 统一的系统启动序列
3. **固件信息 (nx_firmware_info)** - 嵌入式固件元数据

### 核心优势

- 零运行时注册开销（编译期确定）
- 模块完全解耦（驱动无需修改核心代码）
- 编译期确定初始化顺序
- 多编译器支持（GCC、Arm Compiler 5/6、IAR）

## 目录结构

```
framework/init/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── nx_init.h           # 初始化机制公共头文件
│   ├── nx_startup.h        # 启动框架头文件
│   └── nx_firmware_info.h  # 固件信息头文件
└── src/
    ├── nx_init.c           # 初始化机制实现
    ├── nx_startup.c        # 启动框架实现
    └── nx_firmware_info.c  # 固件信息实现
```

## 自动初始化机制

### 初始化级别

系统支持 6 个初始化级别，按顺序执行：

| 级别 | 宏定义 | 用途 |
|------|--------|------|
| 0 | `NX_INIT_BOARD_EXPORT` | 板级初始化（时钟、电源） |
| 1 | `NX_INIT_PREV_EXPORT` | 预初始化（内存、调试） |
| 2 | `NX_INIT_BSP_EXPORT` | BSP 初始化（外设配置） |
| 3 | `NX_INIT_DRIVER_EXPORT` | 驱动初始化 |
| 4 | `NX_INIT_COMPONENT_EXPORT` | 组件初始化（中间件） |
| 5 | `NX_INIT_APP_EXPORT` | 应用初始化 |

### 使用示例

```c
#include "nx_init.h"

/* 驱动初始化函数 */
static int uart_init(void) {
    /* 初始化 UART 硬件 */
    return 0;  /* 返回 0 表示成功 */
}

/* 注册为驱动级别初始化 */
NX_INIT_DRIVER_EXPORT(uart_init);
```

### API 参考

```c
/* 执行所有注册的初始化函数 */
nx_status_t nx_init_run(void);

/* 执行指定级别的初始化函数 */
nx_status_t nx_init_run_level(nx_init_level_t level);

/* 获取初始化统计信息 */
nx_status_t nx_init_get_stats(nx_init_stats_t* stats);

/* 检查是否所有初始化都成功 */
bool nx_init_is_complete(void);
```

## 启动框架

### 启动序列

```
Reset Handler
    │
    ▼
nx_startup()
    ├── nx_board_init()     [弱符号，用户可覆盖]
    ├── nx_os_init()        [弱符号，用户可覆盖]
    ├── nx_init_run()       [自动初始化函数]
    │   ├── Level 0: BOARD
    │   ├── Level 1: PREV
    │   ├── Level 2: BSP
    │   ├── Level 3: DRIVER
    │   ├── Level 4: COMPONENT
    │   └── Level 5: APP
    └── main() 或 main_thread
```

### 板级初始化覆盖

```c
#include "nx_startup.h"

/* 覆盖弱符号实现板级初始化 */
void nx_board_init(void) {
    /* 配置系统时钟 */
    system_clock_config();
    
    /* 配置 GPIO */
    gpio_init();
}
```

### RTOS 模式配置

```c
#include "nx_startup.h"

/* 使用自定义配置启动 */
nx_startup_config_t config;
nx_startup_get_default_config(&config);
config.main_stack_size = 8192;
config.main_priority = 24;
config.use_rtos = true;

nx_startup_with_config(&config);
```

### API 参考

```c
/* 执行系统启动序列 */
void nx_startup(void);

/* 使用自定义配置启动 */
void nx_startup_with_config(const nx_startup_config_t* config);

/* 获取当前启动状态 */
nx_startup_state_t nx_startup_get_state(void);

/* 检查启动是否完成 */
bool nx_startup_is_complete(void);

/* 获取默认启动配置 */
void nx_startup_get_default_config(nx_startup_config_t* config);
```

## 固件信息

### 定义固件信息

```c
#include "nx_firmware_info.h"

NX_FIRMWARE_INFO_DEFINE(
    "Nexus Demo",                      /* 产品名 */
    "NEXUS",                           /* 厂商 */
    NX_VERSION_ENCODE(1, 0, 0, 0),     /* 版本 1.0.0.0 */
    0x12345678                         /* 密钥 */
);
```

### 版本编码

```c
/* 编码版本号 */
uint32_t ver = NX_VERSION_ENCODE(1, 2, 3, 4);  /* 1.2.3.4 */

/* 解码版本号 */
uint8_t major = NX_VERSION_MAJOR(ver);  /* 1 */
uint8_t minor = NX_VERSION_MINOR(ver);  /* 2 */
uint8_t patch = NX_VERSION_PATCH(ver);  /* 3 */
uint8_t build = NX_VERSION_BUILD(ver);  /* 4 */
```

### API 参考

```c
/* 获取固件信息指针 */
const nx_firmware_info_t* nx_get_firmware_info(void);

/* 获取版本字符串 */
size_t nx_get_version_string(char* buf, size_t size);
```

## 链接器配置

### GCC 链接器脚本

在链接器脚本中添加以下段定义：

```ld
/* 初始化函数段 */
.nx_init_fn : {
    PROVIDE(__nx_init_fn_start = .);
    KEEP(*(.nx_init_fn.0))
    KEEP(*(.nx_init_fn.1))
    KEEP(*(.nx_init_fn.2))
    KEEP(*(.nx_init_fn.3))
    KEEP(*(.nx_init_fn.4))
    KEEP(*(.nx_init_fn.5))
    PROVIDE(__nx_init_fn_end = .);
} > FLASH

/* 固件信息段 */
.nx_fw_info : {
    KEEP(*(.nx_fw_info))
} > FLASH
```

### Arm Compiler Scatter File

```scatter
LR_IROM1 0x08000000 {
    ; 初始化函数段
    nx_init_fn 0x08010000 {
        *(.nx_init_fn.*)
    }
    
    ; 固件信息段
    nx_fw_info +0 {
        *(.nx_fw_info)
    }
}
```

## 编译器支持

| 编译器 | 段属性 | 符号前缀 |
|--------|--------|----------|
| GCC | `__attribute__((section))` | `__nx_init_fn_start/end` |
| Arm Compiler 5/6 | `__attribute__((section))` | `Image$nx_init_fn$Base/Limit` |
| IAR | `_Pragma("location=")` | `__section_begin/end` |

## 错误处理

### 初始化错误

- 初始化函数返回非零值时，错误被记录但执行继续
- 使用 `nx_init_get_stats()` 获取失败统计
- 使用 `nx_init_is_complete()` 检查是否全部成功

### 统计信息

```c
nx_init_stats_t stats;
nx_init_get_stats(&stats);

printf("Total: %d, Success: %d, Failed: %d\n",
       stats.total_count, stats.success_count, stats.fail_count);

if (stats.fail_count > 0) {
    printf("Last error: %d\n", stats.last_error);
}
```

## 调试支持

定义 `NX_INIT_DEBUG` 宏启用调试输出：

```c
#define NX_INIT_DEBUG
#include "nx_init.h"
```

调试模式下会打印每个初始化函数的执行状态。

## 完整示例

```c
#include "nx_init.h"
#include "nx_startup.h"
#include "nx_firmware_info.h"

/* 定义固件信息 */
NX_FIRMWARE_INFO_DEFINE(
    "My Product",
    "VENDOR",
    NX_VERSION_ENCODE(1, 0, 0, 0),
    0xDEADBEEF
);

/* 板级初始化 */
static int board_clock_init(void) {
    /* 配置系统时钟 */
    return 0;
}
NX_INIT_BOARD_EXPORT(board_clock_init);

/* 驱动初始化 */
static int uart_driver_init(void) {
    /* 初始化 UART */
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_driver_init);

/* 应用初始化 */
static int app_init(void) {
    /* 应用初始化 */
    return 0;
}
NX_INIT_APP_EXPORT(app_init);

/* 覆盖板级初始化弱符号 */
void nx_board_init(void) {
    /* 早期硬件初始化 */
}

/* 主函数 */
int main(void) {
    /* 打印版本信息 */
    char version[32];
    nx_get_version_string(version, sizeof(version));
    printf("Firmware version: %s\n", version);
    
    /* 检查初始化状态 */
    if (!nx_init_is_complete()) {
        printf("Warning: Some initializations failed\n");
    }
    
    /* 主循环 */
    while (1) {
        /* 应用逻辑 */
    }
    
    return 0;
}
```

## 相关文档

- [设备注册表](../../hal/include/hal/nx_device_registry.h) - 设备静态注册机制
- [链接器脚本模板](../../cmake/linker/) - GCC 和 Arm Compiler 链接器配置
