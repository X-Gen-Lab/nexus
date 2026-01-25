# Init Framework 故障排查指南

本文档提供 Init Framework 常见问题的诊断和解决方案。

## 目录

1. [初始化顺序问题](#1-初始化顺序问题)
2. [链接器配置问题](#2-链接器配置问题)
3. [编译器兼容性问题](#3-编译器兼容性问题)
4. [启动框架问题](#4-启动框架问题)
5. [固件信息问题](#5-固件信息问题)
6. [性能问题](#6-性能问题)
7. [调试技巧](#7-调试技巧)

## 1. 初始化顺序问题

### 问题 1.1: 初始化函数未执行

**症状**: 使用 `NX_INIT_XXX_EXPORT()` 注册的函数未被调用

**可能原因**:
1. 链接器段配置错误
2. 函数被优化器移除
3. `nx_init_run()` 未被调用

**诊断步骤**:
```c
/* 1. 检查初始化统计 */
nx_init_stats_t stats;
nx_init_get_stats(&stats);
printf("Total init functions: %u\n", stats.total_count);
printf("Success: %u, Failed: %u\n", 
       stats.success_count, stats.fail_count);

/* 2. 验证函数是否在段中 */
/* 使用 objdump 或 readelf 检查 */
/* objdump -t firmware.elf | grep nx_init_fn */

/* 3. 确保调用了 nx_init_run() */
nx_status_t status = nx_init_run();
if (status != NX_OK) {
    printf("Init failed\n");
}
```

**解决方案**:
```c
/* 确保函数不被优化掉 */
/* 方案 1: 使用 volatile 防止优化 */
static int my_init(void) {
    /* 初始化代码 */
    return 0;
}
NX_INIT_DRIVER_EXPORT(my_init);

/* 方案 2: 检查链接器脚本 */
/* 确保包含 .nx_init_fn.* 段 */
/* 参见 PORTING_GUIDE.md */

/* 方案 3: 添加调试输出 */
static int my_init(void) {
    printf("my_init called\n");
    return 0;
}
```

### 问题 1.2: 初始化顺序错误

**症状**: 初始化函数以错误的顺序执行，导致依赖问题

**原因**: 使用了错误的初始化级别

**解决方案**:
```c
/* 错误: 驱动在 BSP 之前初始化 */
static int bsp_init(void) {
    /* 配置引脚复用 */
    return 0;
}
NX_INIT_BSP_EXPORT(bsp_init);  /* Level 3 */

static int driver_init(void) {
    /* 需要 BSP 先初始化 */
    return 0;
}
NX_INIT_PREV_EXPORT(driver_init);  /* Level 2 - 错误！ */

/* 正确: 使用正确的级别 */
NX_INIT_DRIVER_EXPORT(driver_init);  /* Level 4 - 正确 */
```

**初始化级别参考**:

| 级别 | 宏 | 用途 | 示例 |
|------|-----|------|------|
| 1 | `NX_INIT_BOARD_EXPORT` | 板级初始化 | 时钟、电源 |
| 2 | `NX_INIT_PREV_EXPORT` | 预初始化 | 内存、调试 |
| 3 | `NX_INIT_BSP_EXPORT` | BSP 初始化 | 引脚复用、外设配置 |
| 4 | `NX_INIT_DRIVER_EXPORT` | 驱动初始化 | 设备驱动 |
| 5 | `NX_INIT_COMPONENT_EXPORT` | 组件初始化 | 中间件 |
| 6 | `NX_INIT_APP_EXPORT` | 应用初始化 | 应用逻辑 |

### 问题 1.3: 初始化函数返回错误

**症状**: `nx_init_run()` 返回 `NX_ERR_GENERIC`

**原因**: 某个初始化函数返回了非零错误码

**诊断**:
```c
/* 启用调试模式查看失败的函数 */
#define NX_INIT_DEBUG 1

nx_init_stats_t stats;
nx_init_run();
nx_init_get_stats(&stats);

printf("Failed count: %u\n", stats.fail_count);
printf("Last error: %d\n", stats.last_error);
#ifdef NX_INIT_DEBUG
printf("Last failed function: %s\n", stats.last_failed);
#endif
```

**解决方案**:
```c
/* 确保初始化函数正确处理错误 */
static int my_init(void) {
    /* 尝试初始化 */
    if (hardware_init() != 0) {
        printf("Hardware init failed\n");
        return -1;  /* 返回错误码 */
    }
    
    return 0;  /* 成功返回 0 */
}
```

### 问题 1.4: 同级别内的执行顺序不确定

**症状**: 同一级别的多个初始化函数执行顺序不可预测

**原因**: 链接器对同一段内的符号排序是未定义的

**解决方案**:
```c
/* 方案 1: 使用不同级别 */
/* 如果有依赖关系，使用不同级别确保顺序 */

/* 方案 2: 合并为一个初始化函数 */
static int combined_init(void) {
    init_a();  /* 明确的顺序 */
    init_b();
    init_c();
    return 0;
}
NX_INIT_DRIVER_EXPORT(combined_init);

/* 方案 3: 使用显式依赖检查 */
static bool g_init_a_done = false;

static int init_a(void) {
    /* ... */
    g_init_a_done = true;
    return 0;
}

static int init_b(void) {
    if (!g_init_a_done) {
        return -1;  /* A 未初始化 */
    }
    /* ... */
    return 0;
}
```

## 2. 链接器配置问题

### 问题 2.1: 链接器段未定义

**症状**: 链接时报错 "undefined reference to `__nx_init_fn_start`"

**原因**: 链接器脚本未定义 `.nx_init_fn` 段

**解决方案**:

```ld
/* GCC 链接器脚本示例 */
SECTIONS
{
    .text : {
        *(.text*)
        
        /* Init function table */
        . = ALIGN(4);
        __nx_init_fn_start = .;
        KEEP(*(.nx_init_fn.0))  /* Boundary start */
        KEEP(*(.nx_init_fn.1))  /* Board */
        KEEP(*(.nx_init_fn.2))  /* Prev */
        KEEP(*(.nx_init_fn.3))  /* BSP */
        KEEP(*(.nx_init_fn.4))  /* Driver */
        KEEP(*(.nx_init_fn.5))  /* Component */
        KEEP(*(.nx_init_fn.6))  /* App */
        KEEP(*(.nx_init_fn.7))  /* Boundary end */
        __nx_init_fn_end = .;
    } > FLASH
}
```

**验证**:
```bash
# 检查段是否存在
arm-none-eabi-objdump -h firmware.elf | grep nx_init_fn

# 检查符号是否定义
arm-none-eabi-nm firmware.elf | grep __nx_init_fn
```

### 问题 2.2: 初始化函数被链接器移除

**症状**: 函数定义了但未出现在最终二进制文件中

**原因**: 链接器垃圾回收（--gc-sections）移除了未引用的段

**解决方案**:
```ld
/* 使用 KEEP() 防止移除 */
SECTIONS
{
    .text : {
        KEEP(*(.nx_init_fn.*))  /* 保留所有初始化函数 */
    }
}
```

```makefile
# 或在 Makefile 中禁用特定段的垃圾回收
LDFLAGS += -Wl,--undefined=__nx_init_fn_start
LDFLAGS += -Wl,--undefined=__nx_init_fn_end
```

### 问题 2.3: Arm Compiler 段名称错误

**症状**: Arm Compiler 5/6 链接失败

**原因**: Arm Compiler 使用不同的段命名约定

**解决方案**:
```scatter
/* Arm Compiler scatter 文件 */
LR_IROM1 0x08000000 0x00080000 {
    ER_IROM1 0x08000000 0x00080000 {
        *.o (RESET, +First)
        *(InRoot$$Sections)
        .ANY (+RO)
        
        /* Init function table */
        .ANY (.nx_init_fn.*)
    }
    
    /* 定义边界符号 */
    nx_init_fn +0 {
        *(nx_init_fn)
    }
}
```

### 问题 2.4: IAR 链接器配置

**症状**: IAR 编译器链接失败或初始化函数未执行

**解决方案**:
```icf
/* IAR linker configuration file */
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

place in ROM_region { block nx_init_fn };

/* 导出边界符号 */
define symbol __nx_init_fn_start = start(nx_init_fn);
define symbol __nx_init_fn_end = end(nx_init_fn);
```

## 3. 编译器兼容性问题

### 问题 3.1: MSVC 编译失败

**症状**: 在 Windows 上使用 MSVC 编译时出错

**原因**: MSVC 不支持 GCC 风格的段属性

**解决方案**:
```c
/* 对于 MSVC，使用数组方式（仅用于测试）*/
#if defined(_MSC_VER)
/* 手动注册初始化函数 */
static const nx_init_fn_t init_functions[] = {
    board_init,
    driver_init,
    app_init,
    NULL
};

/* 在 main 中手动调用 */
int main(void) {
    for (int i = 0; init_functions[i] != NULL; i++) {
        init_functions[i]();
    }
    /* ... */
}
#endif
```

**注意**: MSVC 主要用于单元测试，实际嵌入式目标应使用 GCC/Clang/Arm Compiler。

### 问题 3.2: C++ 兼容性问题

**症状**: 在 C++ 代码中使用初始化宏出错

**原因**: C++ 名称修饰（name mangling）

**解决方案**:
```cpp
/* 使用 extern "C" */
extern "C" {
    static int my_init(void) {
        /* C++ 代码 */
        return 0;
    }
}
NX_INIT_DRIVER_EXPORT(my_init);

/* 或使用 C++ 包装 */

class MyDriver {
public:
    static int init() {
        /* 初始化代码 */
        return 0;
    }
};

extern "C" {
    static int my_driver_init_wrapper(void) {
        return MyDriver::init();
    }
}
NX_INIT_DRIVER_EXPORT(my_driver_init_wrapper);
```

### 问题 3.3: 优化级别导致的问题

**症状**: 在高优化级别（-O2, -O3）下初始化函数未执行

**原因**: 编译器认为函数未被使用而优化掉

**解决方案**:
```c
/* 使用 NX_USED 属性（已在宏中包含）*/
/* 如果仍有问题，可以添加额外的保护 */

/* 方案 1: 降低优化级别 */
/* 在 Makefile 中 */
CFLAGS += -O1  /* 或 -O0 用于调试 */

/* 方案 2: 禁用特定函数的优化 */
#if defined(__GNUC__)
__attribute__((optimize("O0")))
#endif
static int my_init(void) {
    /* ... */
    return 0;
}

/* 方案 3: 添加 volatile 引用 */
static int my_init(void) {
    volatile int dummy = 0;
    /* ... */
    return dummy;
}
```

### 问题 3.4: 不同编译器的段名称差异

**症状**: 在不同编译器间移植时初始化失败

**解决方案**:
```c
/* 使用条件编译适配不同编译器 */
/* 已在 nx_init.h 中处理，确保包含正确的头文件 */

/* 如果需要自定义，参考以下模式 */
#if defined(__GNUC__)
    #define MY_SECTION(name) __attribute__((section(name)))
#elif defined(__ARMCC_VERSION)
    #define MY_SECTION(name) __attribute__((section(name)))
#elif defined(__ICCARM__)
    #define MY_SECTION(name) _Pragma(#name)
#else
    #error "Unsupported compiler"
#endif
```

## 4. 启动框架问题

### 问题 4.1: nx_startup() 未返回

**症状**: 调用 `nx_startup()` 后程序挂起

**原因**: 
1. RTOS 模式下调度器已启动（正常行为）
2. 某个初始化函数进入死循环
3. main() 函数未正确实现

**诊断**:
```c
/* 检查启动状态 */
nx_startup_state_t state = nx_startup_get_state();
printf("Startup state: %d\n", state);

/* 检查是否完成 */
if (nx_startup_is_complete()) {
    printf("Startup complete\n");
} else {
    printf("Startup not complete\n");
}
```

**解决方案**:
```c
/* 确保 main() 函数正确实现 */
int main(void) {
    printf("Main started\n");
    
    /* 应用代码 */
    while (1) {
        /* 主循环 */
    }
    
    return 0;
}

/* 如果使用 RTOS，确保任务不退出 */
void main_task(void* arg) {
    while (1) {
        /* 任务代码 */
        osal_delay(100);
    }
}
```

### 问题 4.2: nx_board_init() 未被调用

**症状**: 自定义的 `nx_board_init()` 函数未执行

**原因**: 
1. 函数签名不正确
2. 链接器未找到符号
3. 弱符号被其他定义覆盖

**解决方案**:
```c
/* 确保函数签名正确 */
void nx_board_init(void) {  /* 注意：返回 void，无参数 */
    /* 板级初始化代码 */
    printf("Board init called\n");
}

/* 不要使用 static */
/* 错误 */
static void nx_board_init(void) { }  /* 不会覆盖弱符号 */

/* 正确 */
void nx_board_init(void) { }  /* 会覆盖弱符号 */
```

**验证**:
```bash
# 检查符号是否存在
arm-none-eabi-nm firmware.elf | grep nx_board_init

# 应该看到强符号（T），而不是弱符号（W）
```

### 问题 4.3: RTOS 模式下主线程未创建

**症状**: 使用 RTOS 配置但 main() 未执行

**原因**: OSAL 未正确初始化或配置错误

**诊断**:
```c
/* 检查 OSAL 状态 */
void nx_os_init(void) {
    osal_status_t status = osal_init();
    if (status != OSAL_OK) {
        printf("OSAL init failed: %d\n", status);
        while (1);  /* 停止执行 */
    }
}

/* 检查启动配置 */
nx_startup_config_t config;
nx_startup_get_default_config(&config);
printf("RTOS mode: %s\n", config.use_rtos ? "Yes" : "No");
printf("Stack size: %u\n", config.main_stack_size);
printf("Priority: %u\n", config.main_priority);
```

**解决方案**:
```c
/* 自定义启动配置 */
int entry(void) {
    nx_startup_config_t config;
    nx_startup_get_default_config(&config);
    
    /* 调整参数 */
    config.main_stack_size = 8192;  /* 增加栈大小 */
    config.main_priority = 10;      /* 调整优先级 */
    config.use_rtos = true;
    
    nx_startup_with_config(&config);
    return 0;
}
```

### 问题 4.4: 启动顺序混乱

**症状**: 初始化函数在 `nx_board_init()` 之前执行

**原因**: 未使用启动框架，直接调用了 `nx_init_run()`

**解决方案**:
```c
/* 错误: 手动调用初始化 */
int main(void) {
    nx_init_run();  /* 错误！跳过了 board_init */
    /* ... */
}

/* 正确: 使用启动框架 */
int entry(void) {
    nx_startup();  /* 正确的启动顺序 */
    return 0;
}

/* 或在 main 中确保顺序 */
int main(void) {
    nx_board_init();  /* 1. 板级初始化 */
    nx_os_init();     /* 2. OS 初始化 */
    nx_init_run();    /* 3. 自动初始化 */
    /* 4. 应用代码 */
}
```

## 5. 固件信息问题

### 问题 5.1: 固件信息未嵌入

**症状**: `nx_get_firmware_info()` 返回 NULL

**原因**: 未使用 `NX_FIRMWARE_INFO_DEFINE` 宏定义固件信息

**解决方案**:
```c
/* 在某个源文件中定义（通常是 main.c）*/

#include "nx_firmware_info.h"

NX_FIRMWARE_INFO_DEFINE(
    "My Product",           /* 产品名称 */
    "VENDOR",              /* 厂商标识 */
    NX_VERSION_ENCODE(1, 0, 0, 0),  /* 版本 1.0.0.0 */
    0x12345678             /* 固件密钥 */
);

/* 使用固件信息 */
void print_version(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    if (info != NULL) {
        printf("Product: %s\n", info->product);
        printf("Version: %u.%u.%u.%u\n",
               NX_VERSION_MAJOR(info->version),
               NX_VERSION_MINOR(info->version),
               NX_VERSION_PATCH(info->version),
               NX_VERSION_BUILD(info->version));
    }
}
```

### 问题 5.2: 固件信息段被优化掉

**症状**: 定义了固件信息但在二进制文件中找不到

**原因**: 链接器垃圾回收移除了未引用的段

**解决方案**:
```ld
/* 在链接器脚本中保留固件信息段 */
SECTIONS
{
    .rodata : {
        *(.rodata*)
        
        /* Firmware info section */
        . = ALIGN(4);
        KEEP(*(.nx_fw_info))
    } > FLASH
}
```

**验证**:
```bash
# 检查固件信息是否存在
arm-none-eabi-objdump -s -j .nx_fw_info firmware.elf

# 或使用 strings 查找
arm-none-eabi-strings firmware.elf | grep "My Product"
```

### 问题 5.3: 版本号解析错误

**症状**: 版本号显示不正确

**原因**: 版本编码或解析错误

**诊断**:
```c
/* 测试版本编码 */
uint32_t ver = NX_VERSION_ENCODE(1, 2, 3, 4);
printf("Encoded: 0x%08X\n", ver);  /* 应该是 0x01020304 */

printf("Major: %u\n", NX_VERSION_MAJOR(ver));  /* 应该是 1 */
printf("Minor: %u\n", NX_VERSION_MINOR(ver));  /* 应该是 2 */
printf("Patch: %u\n", NX_VERSION_PATCH(ver));  /* 应该是 3 */
printf("Build: %u\n", NX_VERSION_BUILD(ver));  /* 应该是 4 */
```

**解决方案**:
```c
/* 确保使用正确的宏 */
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 2
#define APP_VERSION_PATCH 3
#define APP_VERSION_BUILD 4

NX_FIRMWARE_INFO_DEFINE(
    "My Product",
    "VENDOR",
    NX_VERSION_ENCODE(APP_VERSION_MAJOR, 
                     APP_VERSION_MINOR,
                     APP_VERSION_PATCH,
                     APP_VERSION_BUILD),
    0x12345678
);

/* 使用辅助函数获取版本字符串 */
char version_str[32];
nx_get_version_string(version_str, sizeof(version_str));
printf("Version: %s\n", version_str);  /* "1.2.3.4" */
```

### 问题 5.4: 多个固件信息定义冲突

**症状**: 链接时报错 "multiple definition of `nx_firmware_info`"

**原因**: 在多个源文件中使用了 `NX_FIRMWARE_INFO_DEFINE`

**解决方案**:
```c
/* 只在一个源文件中定义固件信息（通常是 main.c）*/
/* main.c */
NX_FIRMWARE_INFO_DEFINE("Product", "VENDOR", 
                       NX_VERSION_ENCODE(1,0,0,0), 0x12345678);

/* 其他文件中只声明，不定义 */
/* other.c */
extern const nx_firmware_info_t nx_firmware_info;

/* 或使用 API 访问 */
const nx_firmware_info_t* info = nx_get_firmware_info();
```

## 6. 性能问题

### 问题 6.1: 启动时间过长

**症状**: 从上电到 main() 执行耗时过长

**诊断**:
```c
/* 测量各阶段耗时 */
#include <time.h>

static uint32_t g_start_time;

void nx_board_init(void) {
    g_start_time = get_tick_count();
    /* 板级初始化 */
    uint32_t elapsed = get_tick_count() - g_start_time;
    printf("Board init: %u ms\n", elapsed);
}

void nx_os_init(void) {
    uint32_t start = get_tick_count();
    /* OS 初始化 */
    uint32_t elapsed = get_tick_count() - start;
    printf("OS init: %u ms\n", elapsed);
}

int main(void) {
    uint32_t total = get_tick_count() - g_start_time;
    printf("Total startup time: %u ms\n", total);
    /* ... */
}
```

**优化方案**:
```c
/* 1. 延迟非关键初始化 */
static int non_critical_init(void) {
    /* 可以延迟到后台任务执行 */
    return 0;
}
/* 不使用 NX_INIT_XXX_EXPORT，在后台任务中调用 */

/* 2. 并行初始化（RTOS 环境）*/
void parallel_init_task(void* arg) {
    /* 在独立任务中初始化 */
    driver_init();
}

void nx_os_init(void) {
    osal_init();
    /* 创建并行初始化任务 */
    osal_task_create(parallel_init_task, ...);
}

/* 3. 减少初始化函数数量 */
/* 合并相关的初始化函数 */
static int combined_driver_init(void) {
    uart_init();
    spi_init();
    i2c_init();
    return 0;
}
NX_INIT_DRIVER_EXPORT(combined_driver_init);
```

### 问题 6.2: 初始化函数执行太慢

**症状**: 某个初始化函数耗时过长

**诊断**:
```c
/* 在初始化函数中添加计时 */
static int slow_init(void) {
    uint32_t start = get_tick_count();
    
    /* 初始化代码 */
    hardware_init();
    
    uint32_t elapsed = get_tick_count() - start;
    if (elapsed > 100) {  /* 超过 100ms */
        printf("WARNING: slow_init took %u ms\n", elapsed);
    }
    
    return 0;
}
```

**优化**:
```c
/* 1. 移除不必要的延迟 */
static int optimized_init(void) {
    /* 避免使用 delay */
    /* 使用轮询或中断代替 */
    return 0;
}

/* 2. 使用异步初始化 */
static volatile bool g_init_complete = false;

static int async_init(void) {
    /* 启动异步初始化 */
    start_async_hardware_init();
    return 0;
}

void check_init_complete(void) {
    if (is_hardware_ready()) {
        g_init_complete = true;
    }
}
```

### 问题 6.3: 内存占用过高

**症状**: 初始化后可用内存不足

**诊断**:
```c
/* 监控内存使用 */
void print_memory_usage(void) {
    size_t free_heap = get_free_heap_size();
    size_t total_heap = get_total_heap_size();
    size_t used = total_heap - free_heap;
    
    printf("Heap: %zu / %zu bytes (%.1f%%)\n",
           used, total_heap, 
           (float)used * 100 / total_heap);
}

int main(void) {
    print_memory_usage();  /* 启动后检查 */
    /* ... */
}
```

**优化**:
```c
/* 1. 使用静态分配代替动态分配 */
static int init_with_static_buffer(void) {
    static uint8_t buffer[1024];  /* 静态分配 */
    /* 使用 buffer */
    return 0;
}

/* 2. 释放初始化后不需要的内存 */
static int init_with_temp_buffer(void) {
    uint8_t* temp = malloc(4096);
    /* 使用临时缓冲区 */
    free(temp);  /* 初始化完成后释放 */
    return 0;
}

/* 3. 减少全局变量 */
/* 只保留必要的全局状态 */
```

## 7. 调试技巧

### 7.1 启用调试输出

```c
/* 在 nx_init_port.h 或编译选项中定义 */
#define NX_INIT_DEBUG 1

/* 这将启用详细的调试信息 */
/* - 每个初始化函数的名称 */
/* - 执行结果（成功/失败）*/
/* - 错误码 */
```

### 7.2 使用断点调试

```c
/* 在关键位置设置断点 */

void nx_board_init(void) {
    /* 在这里设置断点 */
    __asm("nop");  /* 方便设置断点 */
    
    /* 板级初始化代码 */
}

static int my_init(void) {
    /* 在这里设置断点 */
    __asm("nop");
    
    /* 初始化代码 */
    return 0;
}

/* 在 GDB 中 */
/* (gdb) break nx_board_init */
/* (gdb) break my_init */
/* (gdb) continue */
```

### 7.3 遍历初始化函数表

```c
/* 打印所有注册的初始化函数 */
void dump_init_functions(void) {
    const nx_init_fn_t* fn_ptr = NX_INIT_FN_START;
    int index = 0;
    
    printf("=== Init Function Table ===\n");
    printf("Start: %p\n", (void*)NX_INIT_FN_START);
    printf("End:   %p\n", (void*)NX_INIT_FN_END);
    
    while (fn_ptr < NX_INIT_FN_END) {
        if (*fn_ptr != NULL) {
            printf("[%d] Function at %p\n", index, (void*)*fn_ptr);
        }
        fn_ptr++;
        index++;
    }
    
    printf("Total: %d functions\n", index);
    printf("===========================\n");
}
```

### 7.4 单步执行初始化

```c
/* 手动执行初始化函数，便于调试 */
void manual_init_run(void) {
    const nx_init_fn_t* fn_ptr = NX_INIT_FN_START;
    int index = 0;
    
    while (fn_ptr < NX_INIT_FN_END) {
        if (*fn_ptr != NULL) {
            printf("Executing init function %d...\n", index);
            
            int result = (*fn_ptr)();
            
            if (result != 0) {
                printf("ERROR: Function %d failed with code %d\n", 
                       index, result);
                /* 在这里设置断点检查失败原因 */
                __asm("nop");
            } else {
                printf("Function %d succeeded\n", index);
            }
        }
        fn_ptr++;
        index++;
    }
}
```

### 7.5 使用日志系统

```c
/* 集成日志系统进行调试 */
#include "nx_log.h"

static int my_init(void) {
    NX_LOG_INFO("Starting my_init");
    
    if (hardware_init() != 0) {
        NX_LOG_ERROR("Hardware init failed");
        return -1;
    }
    
    NX_LOG_INFO("my_init completed successfully");
    return 0;
}

void nx_board_init(void) {
    NX_LOG_INFO("Board init started");
    
    /* 板级初始化 */
    clock_init();
    NX_LOG_DEBUG("Clock configured");
    
    power_init();
    NX_LOG_DEBUG("Power management configured");
    
    NX_LOG_INFO("Board init completed");
}
```

### 7.6 内存转储分析

```bash
# 使用 objdump 查看初始化函数段
arm-none-eabi-objdump -s -j .nx_init_fn firmware.elf

# 查看符号表
arm-none-eabi-nm firmware.elf | grep nx_init

# 查看段信息
arm-none-eabi-readelf -S firmware.elf | grep nx_init

# 反汇编特定函数
arm-none-eabi-objdump -d firmware.elf | grep -A 20 my_init
```

### 7.7 使用 JTAG/SWD 调试器

```c
/* 在初始化失败时触发调试器 */
static int my_init(void) {
    if (hardware_init() != 0) {
        /* 触发断点 */
        #if defined(__GNUC__)
        __asm("bkpt 0");
        #elif defined(__ARMCC_VERSION)
        __breakpoint(0);
        #endif
        return -1;
    }
    return 0;
}
```

### 7.8 性能分析工具

```c
/* 使用 DWT（Data Watchpoint and Trace）进行精确计时 */
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

static inline void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t dwt_get_cycles(void) {
    return DWT->CYCCNT;
}

static int my_init(void) {
    uint32_t start = dwt_get_cycles();
    
    /* 初始化代码 */
    hardware_init();
    
    uint32_t cycles = dwt_get_cycles() - start;
    uint32_t us = cycles / (SystemCoreClock / 1000000);
    
    printf("my_init took %u cycles (%u us)\n", cycles, us);
    return 0;
}

#endif
```

### 7.9 静态分析工具

```bash
# 使用 cppcheck 进行静态分析
cppcheck --enable=all --inconclusive framework/init/

# 使用 clang-tidy
clang-tidy framework/init/src/*.c -- -Iframework/init/include

# 使用编译器警告
gcc -Wall -Wextra -Wpedantic -Werror framework/init/src/*.c
```

### 7.10 单元测试辅助

```c
/* 为测试提供钩子函数 */
#ifdef NX_INIT_TEST_MODE

/* 记录初始化调用 */
static int g_init_call_count = 0;
static const char* g_init_call_names[32];

void nx_init_test_reset(void) {
    g_init_call_count = 0;
}

void nx_init_test_record_call(const char* name) {
    if (g_init_call_count < 32) {
        g_init_call_names[g_init_call_count++] = name;
    }
}

int nx_init_test_get_call_count(void) {
    return g_init_call_count;
}

const char* nx_init_test_get_call_name(int index) {
    if (index >= 0 && index < g_init_call_count) {
        return g_init_call_names[index];
    }
    return NULL;
}

/* 在初始化函数中使用 */
static int my_init(void) {
    nx_init_test_record_call("my_init");
    /* ... */
    return 0;
}

#endif /* NX_INIT_TEST_MODE */
```

## 8. 常见错误速查表

| 症状 | 可能原因 | 解决方案 | 参考章节 |
|------|----------|----------|----------|
| 初始化函数未执行 | 链接器段配置错误 | 检查链接器脚本 | 1.1, 2.1 |
| 初始化顺序错误 | 使用了错误的级别 | 使用正确的 `NX_INIT_XXX_EXPORT` | 1.2 |
| 链接失败 | 段符号未定义 | 添加段定义到链接器脚本 | 2.1 |
| 函数被优化掉 | 链接器垃圾回收 | 使用 `KEEP()` 保留段 | 2.2, 3.3 |
| MSVC 编译失败 | 不支持段属性 | 使用数组方式（仅测试）| 3.1 |
| C++ 编译错误 | 名称修饰问题 | 使用 `extern "C"` | 3.2 |
| `nx_startup()` 挂起 | RTOS 调度器启动 | 正常行为或检查 main() | 4.1 |
| `nx_board_init()` 未调用 | 弱符号未覆盖 | 确保函数签名正确 | 4.2 |
| 固件信息为 NULL | 未定义固件信息 | 使用 `NX_FIRMWARE_INFO_DEFINE` | 5.1 |
| 固件信息被移除 | 链接器优化 | 使用 `KEEP()` 保留段 | 5.2 |
| 启动时间过长 | 初始化函数太慢 | 优化或延迟初始化 | 6.1, 6.2 |
| 内存不足 | 初始化占用过多 | 使用静态分配或释放临时内存 | 6.3 |

## 9. 平台特定问题

### 9.1 STM32 平台

```c
/* STM32 HAL 库冲突 */
/* 问题: HAL_Init() 和 nx_board_init() 调用顺序 */

/* 解决方案 */
void nx_board_init(void) {
    /* 先调用 HAL_Init */
    HAL_Init();
    
    /* 然后配置时钟 */
    SystemClock_Config();
    
    /* 其他板级初始化 */
}
```

### 9.2 Nordic nRF 平台

```c
/* nRF SDK 初始化顺序 */
void nx_board_init(void) {
    /* 1. 时钟初始化 */
    nrf_drv_clock_init();
    
    /* 2. 电源管理 */
    nrf_pwr_mgmt_init();
    
    /* 3. 其他外设 */
}
```

### 9.3 ESP32 平台

```c
/* ESP-IDF 集成 */
void app_main(void) {
    /* ESP-IDF 已完成基本初始化 */
    
    /* 执行 Nexus 初始化 */
    nx_init_run();
    
    /* 应用代码 */
}
```

### 9.4 Linux 平台（测试）

```c
/* Linux 下的测试 */
int main(int argc, char* argv[]) {
    /* 执行初始化 */
    nx_init_run();
    
    /* 运行测试 */
    run_tests();
    
    return 0;
}
```

## 10. 最佳实践建议

### 10.1 初始化函数设计

```c
/* 好的初始化函数 */
static int good_init(void) {
    /* 1. 快速执行（< 10ms）*/
    /* 2. 返回明确的错误码 */
    /* 3. 无副作用（可重复调用）*/
    /* 4. 添加日志输出 */
    
    if (hardware_init() != 0) {
        return -1;  /* 明确的错误 */
    }
    
    return 0;  /* 成功 */
}

/* 避免的做法 */
static int bad_init(void) {
    /* 1. 避免长时间延迟 */
    delay_ms(1000);  /* 不好 */
    
    /* 2. 避免死循环 */
    while (!hardware_ready());  /* 不好 */
    
    /* 3. 避免忽略错误 */
    hardware_init();  /* 未检查返回值 */
    
    return 0;
}
```

### 10.2 依赖管理

```c
/* 使用标志管理依赖 */
static bool g_uart_initialized = false;

static int uart_init(void) {
    /* UART 初始化 */
    g_uart_initialized = true;
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_init);

static int logger_init(void) {
    /* 检查依赖 */
    if (!g_uart_initialized) {
        return -1;  /* UART 未初始化 */
    }
    
    /* 初始化日志系统 */
    return 0;
}
NX_INIT_COMPONENT_EXPORT(logger_init);  /* 更高级别 */
```

### 10.3 错误处理

```c
/* 完善的错误处理 */
static int robust_init(void) {
    int ret;
    
    /* 尝试初始化 */
    ret = hardware_init();
    if (ret != 0) {
        /* 记录错误 */
        printf("Hardware init failed: %d\n", ret);
        
        /* 清理资源 */
        cleanup_resources();
        
        /* 返回错误 */
        return ret;
    }
    
    /* 验证初始化结果 */
    if (!hardware_is_ready()) {
        printf("Hardware not ready after init\n");
        return -2;
    }
    
    return 0;
}
```

### 10.4 文档和注释

```c
/**
 * \brief           初始化 SPI 驱动
 * \note            依赖: GPIO 和时钟必须先初始化
 * \note            耗时: 约 5ms
 */
static int spi_driver_init(void) {
    /* 实现 */
    return 0;
}
NX_INIT_DRIVER_EXPORT(spi_driver_init);
```

## 11. 获取帮助

如果以上方法无法解决问题：

1. **查看文档**:
   - [用户指南](USER_GUIDE.md) - 基本使用方法
   - [设计文档](DESIGN.md) - 架构和实现细节
   - [移植指南](PORTING_GUIDE.md) - 平台适配
   - [测试指南](TEST_GUIDE.md) - 测试方法

2. **检查示例代码**:
   - `applications/` 目录下的示例应用
   - `tests/init/` 目录下的测试用例

3. **使用调试工具**:
   - 启用 `NX_INIT_DEBUG` 查看详细日志
   - 使用 JTAG/SWD 调试器单步执行
   - 使用 `objdump` 检查二进制文件

4. **提交问题**:
   如果仍无法解决，请提交 Issue 并包含：
   - 问题详细描述
   - 复现步骤
   - 错误日志和输出
   - 环境信息：
     * 平台/芯片型号
     * 编译器和版本
     * 链接器脚本（如相关）
     * Init Framework 版本
   - 相关代码片段

5. **社区支持**:
   - 查看项目 Wiki
   - 搜索已关闭的 Issues
   - 参与讨论区交流

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
