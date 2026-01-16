# Design Document: Static Registry Pattern

## Overview

本设计实现 Nexus 项目的静态注册表模式，提供两个核心机制：

1. **自动初始化机制 (nx_init)** - 基于链接器段的分级初始化系统
2. **设备注册机制 (nx_device_registry)** - 编译期设备静态注册

设计目标：
- 零运行时注册开销
- 模块完全解耦
- 编译期确定初始化顺序
- 多编译器支持（GCC、Arm Compiler 5/6、IAR）

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application                               │
├─────────────────────────────────────────────────────────────────┤
│                     nx_hal_init()                                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  nx_init_run()                           │    │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │    │
│  │  │ BOARD   │→│  PREV   │→│   BSP   │→│ DRIVER  │→ ...   │    │
│  │  │ Level 0 │ │ Level 1 │ │ Level 2 │ │ Level 3 │        │    │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘        │    │
│  └─────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              nx_device_registry                          │    │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │    │
│  │  │ UART0   │ │ SPI1    │ │ I2C0    │ │ GPIO    │ ...    │    │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘        │    │
│  └─────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                      Linker Sections                             │
│  .nx_init_fn.0  .nx_init_fn.1  ...  .nx_init_fn.5  .nx_device   │
└─────────────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. 初始化函数类型

```c
/**
 * \brief           初始化函数类型
 * \return          0 表示成功，非零表示错误码
 */
typedef int (*nx_init_fn_t)(void);
```

### 2. 初始化级别定义

```c
/**
 * \brief           初始化级别枚举
 */
typedef enum {
    NX_INIT_LEVEL_BOARD     = 0,  /**< 板级初始化（时钟、电源） */
    NX_INIT_LEVEL_PREV      = 1,  /**< 预初始化（内存、调试） */
    NX_INIT_LEVEL_BSP       = 2,  /**< BSP 初始化（外设配置） */
    NX_INIT_LEVEL_DRIVER    = 3,  /**< 驱动初始化 */
    NX_INIT_LEVEL_COMPONENT = 4,  /**< 组件初始化（中间件） */
    NX_INIT_LEVEL_APP       = 5,  /**< 应用初始化 */
    NX_INIT_LEVEL_MAX
} nx_init_level_t;
```

### 3. 初始化导出宏

```c
/* 内部实现宏 */
#define _NX_INIT_EXPORT(fn, level)                                  \
    NX_USED const nx_init_fn_t _nx_init_##fn                        \
    NX_SECTION(".nx_init_fn." #level) = (fn)

/* 各级别导出宏 */
#define NX_INIT_BOARD_EXPORT(fn)     _NX_INIT_EXPORT(fn, 0)
#define NX_INIT_PREV_EXPORT(fn)      _NX_INIT_EXPORT(fn, 1)
#define NX_INIT_BSP_EXPORT(fn)       _NX_INIT_EXPORT(fn, 2)
#define NX_INIT_DRIVER_EXPORT(fn)    _NX_INIT_EXPORT(fn, 3)
#define NX_INIT_COMPONENT_EXPORT(fn) _NX_INIT_EXPORT(fn, 4)
#define NX_INIT_APP_EXPORT(fn)       _NX_INIT_EXPORT(fn, 5)
```

### 4. 设备注册宏

```c
/**
 * \brief           设备注册宏
 * \param[in]       _var: 变量名
 * \param[in]       _name: 设备名称字符串
 * \param[in]       ...: NX_DEVICE_DEFINE 参数
 */
#define NX_DEVICE_REGISTER(_var, ...)                               \
    NX_USED NX_ALIGNED(sizeof(void*))                               \
    const nx_device_t _var                                          \
    NX_SECTION(".nx_device") = NX_DEVICE_DEFINE(__VA_ARGS__)
```

### 5. 段边界符号（编译器相关）

```c
/* GCC / Clang */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
    extern const nx_init_fn_t __nx_init_fn_start[];
    extern const nx_init_fn_t __nx_init_fn_end[];
    extern const nx_device_t __nx_device_start[];
    extern const nx_device_t __nx_device_end[];
    
    #define NX_INIT_FN_START  __nx_init_fn_start
    #define NX_INIT_FN_END    __nx_init_fn_end
    #define NX_DEVICE_START   __nx_device_start
    #define NX_DEVICE_END     __nx_device_end

/* Arm Compiler 5/6 */
#elif defined(__ARMCC_VERSION)
    extern const nx_init_fn_t Image$$nx_init_fn$$Base[];
    extern const nx_init_fn_t Image$$nx_init_fn$$Limit[];
    extern const nx_device_t Image$$nx_device$$Base[];
    extern const nx_device_t Image$$nx_device$$Limit[];
    
    #define NX_INIT_FN_START  Image$$nx_init_fn$$Base
    #define NX_INIT_FN_END    Image$$nx_init_fn$$Limit
    #define NX_DEVICE_START   Image$$nx_device$$Base
    #define NX_DEVICE_END     Image$$nx_device$$Limit

/* IAR */
#elif defined(__ICCARM__)
    #pragma section = "nx_init_fn"
    #pragma section = "nx_device"
    
    #define NX_INIT_FN_START  ((const nx_init_fn_t*)__section_begin("nx_init_fn"))
    #define NX_INIT_FN_END    ((const nx_init_fn_t*)__section_end("nx_init_fn"))
    #define NX_DEVICE_START   ((const nx_device_t*)__section_begin("nx_device"))
    #define NX_DEVICE_END     ((const nx_device_t*)__section_end("nx_device"))
#endif
```

### 6. 初始化统计结构

```c
/**
 * \brief           初始化统计信息
 */
typedef struct nx_init_stats_s {
    uint16_t total_count;    /**< 总初始化函数数量 */
    uint16_t success_count;  /**< 成功数量 */
    uint16_t fail_count;     /**< 失败数量 */
    int last_error;          /**< 最后一个错误码 */
    const char* last_failed; /**< 最后失败的函数名（调试模式） */
} nx_init_stats_t;
```

### 7. 设备遍历宏

```c
/**
 * \brief           遍历所有注册的设备
 * \param[in]       _dev: 设备指针变量名
 */
#define NX_DEVICE_FOREACH(_dev)                                     \
    for (const nx_device_t* _dev = NX_DEVICE_START;                 \
         _dev < NX_DEVICE_END;                                      \
         _dev++)

/**
 * \brief           获取注册设备数量
 */
#define NX_DEVICE_COUNT() ((size_t)(NX_DEVICE_END - NX_DEVICE_START))
```

### 8. 固件信息结构

```c
/**
 * \brief           固件信息结构
 */
typedef struct nx_firmware_info_s {
    char product[32];        /**< 产品名称 */
    char factory[16];        /**< 厂商标识 */
    char date[12];           /**< 编译日期 (__DATE__) */
    char time[12];           /**< 编译时间 (__TIME__) */
    uint32_t version;        /**< 版本号 (major.minor.patch.build) */
    uint32_t key;            /**< 固件密钥/校验值 */
} nx_firmware_info_t;

/**
 * \brief           版本号编码宏
 */
#define NX_VERSION_ENCODE(major, minor, patch, build)               \
    (((uint32_t)(major) << 24) | ((uint32_t)(minor) << 16) |        \
     ((uint32_t)(patch) << 8) | (uint32_t)(build))

/**
 * \brief           固件信息定义宏
 */
#ifdef NX_FIRMWARE_INFO
#define NX_FIRMWARE_INFO_DEFINE(_product, _factory, _ver, _key)     \
    NX_USED const nx_firmware_info_t nx_firmware_info               \
    NX_SECTION(".nx_fw_info") = {                                   \
        .product = _product,                                        \
        .factory = _factory,                                        \
        .date = __DATE__,                                           \
        .time = __TIME__,                                           \
        .version = _ver,                                            \
        .key = _key                                                 \
    }
#else
#define NX_FIRMWARE_INFO_DEFINE(_product, _factory, _ver, _key)
#endif
```

### 9. 启动框架

```c
/**
 * \brief           启动配置
 */
typedef struct nx_startup_config_s {
    uint32_t main_stack_size;  /**< 主线程栈大小（RTOS 模式） */
    uint8_t main_priority;     /**< 主线程优先级 */
    bool use_rtos;             /**< 是否使用 RTOS */
} nx_startup_config_t;

/**
 * \brief           弱符号声明的板级初始化
 */
NX_WEAK void nx_board_init(void);

/**
 * \brief           弱符号声明的 OS 初始化
 */
NX_WEAK void nx_os_init(void);

/*---------------------------------------------------------------------------*/
/* Arm Compiler 入口重定向                                                   */
/*---------------------------------------------------------------------------*/
#if defined(__ARMCC_VERSION)
extern int $Super$$main(void);

int $Sub$$main(void) {
    nx_startup();
    return 0;
}

#define NX_CALL_MAIN() $Super$$main()

/*---------------------------------------------------------------------------*/
/* GCC 入口重定向                                                            */
/*---------------------------------------------------------------------------*/
#elif defined(__GNUC__)
extern int main(void);

/* 使用 -eentry 链接选项指定入口点 */
int entry(void) {
    /* 加载数据段（如果需要） */
    extern void __libc_init_array(void);
    __libc_init_array();
    
    nx_startup();
    return 0;
}

#define NX_CALL_MAIN() main()
#endif
```

### 10. 初始化边界标记

```c
/**
 * \brief           边界标记级别
 */
#define NX_INIT_LEVEL_BOUNDARY_START  "0"
#define NX_INIT_LEVEL_BOUNDARY_END    "6"

/**
 * \brief           内部边界标记函数
 */
static int _nx_init_boundary_start(void) { return 0; }
static int _nx_init_boundary_end(void) { return 0; }

/* 自动注册边界标记 */
NX_INIT_EXPORT(_nx_init_boundary_start, NX_INIT_LEVEL_BOUNDARY_START);
NX_INIT_EXPORT(_nx_init_boundary_end, NX_INIT_LEVEL_BOUNDARY_END);

/**
 * \brief           使用边界标记遍历初始化函数
 */
#define NX_INIT_FOREACH(_fn)                                        \
    for (const nx_init_fn_t* _fn = &_nx_init__nx_init_boundary_start; \
         _fn < &_nx_init__nx_init_boundary_end;                     \
         _fn++)
```

## Data Models

### 初始化函数表布局

```
Memory Layout (Linker Sections):

.nx_init_fn.0:  [board_init_1] [board_init_2] ...
.nx_init_fn.1:  [prev_init_1]  [prev_init_2]  ...
.nx_init_fn.2:  [bsp_init_1]   [bsp_init_2]   ...
.nx_init_fn.3:  [drv_init_1]   [drv_init_2]   ...
.nx_init_fn.4:  [comp_init_1]  [comp_init_2]  ...
.nx_init_fn.5:  [app_init_1]   [app_init_2]   ...

链接器按段名排序，确保执行顺序：
.nx_init_fn.0 < .nx_init_fn.1 < ... < .nx_init_fn.5
```

### 设备注册表布局

```
.nx_device section:

┌──────────────────┐
│ nx_device_t[0]   │  ← NX_DEVICE_START
│   name: "uart0"  │
│   config: ...    │
│   init: ...      │
├──────────────────┤
│ nx_device_t[1]   │
│   name: "spi1"   │
│   ...            │
├──────────────────┤
│ ...              │
├──────────────────┤
│ nx_device_t[n-1] │
└──────────────────┘  ← NX_DEVICE_END
```

## API Design

### 初始化管理 API

```c
/**
 * \brief           执行所有注册的初始化函数
 * \return          NX_OK 全部成功，NX_ERR_INIT 有失败
 */
nx_status_t nx_init_run(void);

/**
 * \brief           执行指定级别的初始化函数
 * \param[in]       level: 初始化级别
 * \return          NX_OK 成功，错误码 失败
 */
nx_status_t nx_init_run_level(nx_init_level_t level);

/**
 * \brief           获取初始化统计信息
 * \param[out]      stats: 统计信息输出
 * \return          NX_OK 成功
 */
nx_status_t nx_init_get_stats(nx_init_stats_t* stats);

/**
 * \brief           检查是否所有初始化都成功
 * \return          true 全部成功，false 有失败
 */
bool nx_init_is_complete(void);
```

### 设备注册表 API

```c
/**
 * \brief           按名称查找设备
 * \param[in]       name: 设备名称
 * \return          设备指针，NULL 未找到
 */
const nx_device_t* nx_device_registry_find(const char* name);

/**
 * \brief           获取注册设备数量
 * \return          设备数量
 */
size_t nx_device_registry_count(void);

/**
 * \brief           按索引获取设备
 * \param[in]       index: 设备索引
 * \return          设备指针，NULL 索引越界
 */
const nx_device_t* nx_device_registry_get(size_t index);

/**
 * \brief           初始化所有注册的设备
 * \return          NX_OK 成功，错误码 失败
 */
nx_status_t nx_device_registry_init_all(void);
```

### 启动框架 API

```c
/**
 * \brief           执行系统启动序列
 * \return          不返回（进入 main 或 RTOS 调度）
 *
 * 启动序列：
 * 1. nx_board_init()    - 板级初始化（时钟、电源）
 * 2. nx_os_init()       - OS 初始化（如果使用 RTOS）
 * 3. nx_init_run()      - 执行所有注册的初始化函数
 * 4. main() 或主线程   - 用户应用入口
 */
void nx_startup(void);

/**
 * \brief           获取固件信息
 * \return          固件信息指针，NULL 如果未定义
 */
const nx_firmware_info_t* nx_get_firmware_info(void);

/**
 * \brief           获取固件版本字符串
 * \param[out]      buf: 输出缓冲区
 * \param[in]       size: 缓冲区大小
 * \return          版本字符串长度
 */
size_t nx_get_version_string(char* buf, size_t size);
```

## Linker Script Templates

### GCC Linker Script (ld)

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

/* 设备注册表段 */
.nx_device : {
    PROVIDE(__nx_device_start = .);
    KEEP(*(.nx_device))
    PROVIDE(__nx_device_end = .);
} > FLASH
```

### Arm Compiler Scatter File

```scatter
LR_IROM1 0x08000000 {
    ; ... 其他段 ...
    
    ; 初始化函数段
    nx_init_fn 0x08010000 {
        *(.nx_init_fn.*)
    }
    
    ; 设备注册表段
    nx_device +0 {
        *(.nx_device)
    }
}
```

### IAR Linker Configuration

```icf
define block NX_INIT_FN with alignment = 4 {
    ro section .nx_init_fn.0,
    ro section .nx_init_fn.1,
    ro section .nx_init_fn.2,
    ro section .nx_init_fn.3,
    ro section .nx_init_fn.4,
    ro section .nx_init_fn.5,
    ro section .nx_init_fn.6
};

define block NX_DEVICE with alignment = 4 {
    ro section .nx_device
};

define block NX_FW_INFO with alignment = 4 {
    ro section .nx_fw_info
};

place in ROM_region { block NX_INIT_FN, block NX_DEVICE, block NX_FW_INFO };
```

## Startup Sequence

```
┌─────────────────────────────────────────────────────────────────┐
│                    Reset Handler                                 │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Arm Compiler: $Sub$$main()                               │    │
│  │ GCC: entry()                                             │    │
│  └─────────────────────────────────────────────────────────┘    │
│                           │                                      │
│                           ▼                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ nx_startup()                                             │    │
│  │   1. nx_board_init()     [weak, user override]           │    │
│  │   2. nx_os_init()        [weak, user override]           │    │
│  │   3. nx_init_run()       [auto init functions]           │    │
│  │      ├─ Level 0: BOARD                                   │    │
│  │      ├─ Level 1: PREV                                    │    │
│  │      ├─ Level 2: BSP                                     │    │
│  │      ├─ Level 3: DRIVER                                  │    │
│  │      ├─ Level 4: COMPONENT                               │    │
│  │      └─ Level 5: APP                                     │    │
│  │   4. main() or main_thread                               │    │
│  └─────────────────────────────────────────────────────────┘    │
│                           │                                      │
│              ┌────────────┴────────────┐                        │
│              ▼                         ▼                        │
│  ┌─────────────────────┐   ┌─────────────────────┐              │
│  │ Bare-metal Mode     │   │ RTOS Mode           │              │
│  │ Direct call main()  │   │ Create main_thread  │              │
│  │                     │   │ Start scheduler     │              │
│  └─────────────────────┘   └─────────────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Init Level Order Preservation

*For any* set of registered initialization functions at different levels, when `nx_init_run()` is called, all functions at level N must complete before any function at level N+1 begins execution.

**Validates: Requirements 1.2**

### Property 2: Init Error Continuation

*For any* initialization function that returns a non-zero error code, the Init_Manager shall continue executing remaining functions and correctly record the error in statistics (fail_count incremented, last_error updated).

**Validates: Requirements 1.4, 5.3**

### Property 3: Device Registry Iteration Completeness

*For any* set of registered devices, iterating with `NX_DEVICE_FOREACH` shall visit each device exactly once, and `NX_DEVICE_COUNT()` shall equal the number of devices visited.

**Validates: Requirements 2.3, 2.5**

### Property 4: Device Lookup Correctness

*For any* registered device with name N, `nx_device_registry_find(N)` shall return a pointer to that device. *For any* name M not registered, `nx_device_registry_find(M)` shall return NULL.

**Validates: Requirements 2.4**

### Property 5: Init Stats Consistency

*For any* execution of `nx_init_run()`, the resulting stats shall satisfy: `total_count == success_count + fail_count`, and `nx_init_is_complete()` returns true if and only if `fail_count == 0`.

**Validates: Requirements 5.1, 5.4**

### Property 6: Device Alignment

*For any* registered device, its memory address shall be aligned to `sizeof(void*)` boundary.

**Validates: Requirements 3.2**

## Error Handling

### 初始化错误处理

| 错误场景 | 处理方式 | 返回值 |
|---------|---------|--------|
| 初始化函数返回非零 | 记录错误，继续执行 | NX_ERR_INIT |
| 无初始化函数注册 | 正常返回 | NX_OK |
| 重复调用 nx_init_run | 幂等，直接返回 | NX_OK |

### 设备注册表错误处理

| 错误场景 | 处理方式 | 返回值 |
|---------|---------|--------|
| 设备名为 NULL | 跳过该设备 | - |
| 查找不存在的设备 | 返回 NULL | NULL |
| 索引越界 | 返回 NULL | NULL |

## Testing Strategy

### 测试框架

使用项目现有的 Google Test (GTest) + GMock 框架进行测试。

### 单元测试

由于静态注册表依赖链接器段，单元测试需要特殊处理：

1. **模拟测试** - 使用数组模拟链接器段边界符号
2. **集成测试** - 在目标平台上验证实际行为

### 属性测试

使用 GTest 参数化测试实现属性测试：

```cpp
/* tests/init/test_nx_init.cpp */
#include <gtest/gtest.h>
#include <gmock/gmock.h>

/* 测试初始化顺序 */
TEST(NxInitTest, InitLevelOrderPreservation) {
    /* 注册不同级别的函数，验证执行顺序 */
}

/* 测试错误处理 */
TEST(NxInitTest, InitErrorContinuation) {
    /* 注册返回错误的函数，验证继续执行和统计 */
}

/* 测试设备遍历 */
TEST(NxDeviceRegistryTest, IterationCompleteness) {
    /* 验证遍历访问所有设备且仅一次 */
}

/* 属性测试：设备查找正确性 */
class DeviceLookupPropertyTest : public ::testing::TestWithParam<std::string> {};

TEST_P(DeviceLookupPropertyTest, LookupReturnsCorrectDevice) {
    /* Feature: static-registry, Property 4: Device Lookup Correctness */
    const std::string& name = GetParam();
    /* 验证查找返回正确设备 */
}
```

### 测试配置

- 最小 100 次迭代（属性测试使用参数化）
- 测试标签格式：**Feature: static-registry, Property N: property_text**
- 测试文件位置：`tests/init/`

## File Structure

```
framework/
└── init/
    ├── CMakeLists.txt
    ├── include/
    │   ├── nx_init.h           # 初始化机制公共头文件
    │   ├── nx_startup.h        # 启动框架头文件
    │   └── nx_firmware_info.h  # 固件信息头文件
    └── src/
        ├── nx_init.c           # 初始化机制实现
        └── nx_startup.c        # 启动框架实现

hal/
├── include/
│   └── hal/
│       └── nx_device_registry.h  # 设备注册表头文件
└── src/
    └── nx_device_registry.c      # 设备注册表实现

cmake/
└── linker/
    ├── nx_sections.ld          # GCC 链接器段定义
    ├── nx_sections.sct         # Arm scatter file 段定义
    └── nx_sections.icf         # IAR 链接器配置
```

## Usage Examples

### 驱动自动初始化示例

```c
/* uart_driver.c */
#include "nx_init.h"

static int uart_init(void) {
    /* 初始化 UART 硬件 */
    return 0;  /* 成功 */
}

/* 注册为驱动级别初始化 */
NX_INIT_DRIVER_EXPORT(uart_init);
```

### 设备注册示例

```c
/* uart_device.c */
#include "hal/nx_device_registry.h"

static void* uart0_init(const nx_device_t* dev) {
    /* 初始化 UART0 */
    return &uart0_interface;
}

/* 静态注册 UART0 设备 */
NX_DEVICE_REGISTER(uart0_dev,
    "uart0",           /* 设备名 */
    &uart0_config,     /* 默认配置 */
    &uart0_runtime,    /* 运行时配置 */
    sizeof(uart_config_t),
    uart0_init,        /* 初始化函数 */
    uart0_deinit,      /* 反初始化函数 */
    NULL,              /* 挂起函数 */
    NULL               /* 恢复函数 */
);
```

### 固件信息定义示例

```c
/* firmware_info.c */
#include "nx_firmware_info.h"

NX_FIRMWARE_INFO_DEFINE(
    "Nexus Demo",                              /* 产品名 */
    "NEXUS",                                   /* 厂商 */
    NX_VERSION_ENCODE(1, 0, 0, 0),            /* 版本 1.0.0.0 */
    0x12345678                                 /* 密钥 */
);
```

### 板级初始化覆盖示例

```c
/* board_init.c */
#include "nx_startup.h"

/* 覆盖弱符号 */
void nx_board_init(void) {
    /* 配置系统时钟 */
    system_clock_config();
    
    /* 配置 GPIO */
    gpio_init();
}
```

