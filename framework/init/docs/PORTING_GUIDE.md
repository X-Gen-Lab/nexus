# Init Framework 移植指南

**版本**: 1.0.0  
**最后更新**: 2026-01-24

---

## 目录

- [概述](#概述)
- [编译器适配](#编译器适配)
- [链接器配置](#链接器配置)
- [平台差异](#平台差异)
- [移植检查清单](#移植检查清单)
- [常见问题](#常见问题)

---

## 概述

Init Framework 设计为跨平台和跨编译器，但需要正确的配置才能在新平台上工作。

### 移植要求

**必需**:
- ✅ 支持链接器段（Section）
- ✅ 支持函数指针
- ✅ 支持弱符号（Weak Symbol）

**可选**:
- ⚪ RTOS 支持
- ⚪ 多线程支持

---

## 编译器适配

### 支持的编译器

| 编译器 | 状态 | 注意事项 |
|--------|------|----------|
| GCC | ✅ 完全支持 | 需要 ≥ 7.0 |
| Arm Compiler 5 | ✅ 完全支持 | 需要 ≥ 5.06 |
| Arm Compiler 6 | ✅ 完全支持 | 需要 ≥ 6.0 |
| IAR | ✅ 完全支持 | 需要 ≥ 8.0 |
| MSVC | ⚠️ 测试模式 | 仅用于单元测试 |

### 添加新编译器支持

#### 1. 定义段属性宏

```c
/* 在 nx_init.h 中添加 */
#elif defined(__NEW_COMPILER__)

#define NX_SECTION(x) /* 编译器特定的段属性 */
#define NX_USED       /* 编译器特定的 used 属性 */
```

#### 2. 定义边界符号

```c
/* 方式 1: 链接器提供的符号 */
extern const nx_init_fn_t __nx_init_fn_start[];
extern const nx_init_fn_t __nx_init_fn_end[];

#define NX_INIT_FN_START __nx_init_fn_start
#define NX_INIT_FN_END   __nx_init_fn_end

/* 方式 2: 编译器内置函数 */
#define NX_INIT_FN_START \
    ((const nx_init_fn_t*)__section_begin("nx_init_fn"))
#define NX_INIT_FN_END \
    ((const nx_init_fn_t*)__section_end("nx_init_fn"))
```

#### 3. 测试验证

```c
/* 创建测试文件 */
#include "nx_init.h"

static int test_init_1(void) {
    return 0;
}
NX_INIT_BOARD_EXPORT(test_init_1);

static int test_init_2(void) {
    return 0;
}
NX_INIT_DRIVER_EXPORT(test_init_2);

int main(void) {
    /* 验证初始化函数被正确注册 */
    nx_init_run();
    
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    
    if (stats.total_count != 2) {
        /* 移植失败 */
        return -1;
    }
    
    return 0;
}
```

---

## 链接器配置

### GCC 链接器脚本

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
        /* 起始边界 */
        PROVIDE(__nx_init_fn_start = .);
        
        /* 按级别排序 */
        KEEP(*(.nx_init_fn.0))  /* 边界标记 */
        KEEP(*(.nx_init_fn.1))  /* BOARD */
        KEEP(*(.nx_init_fn.2))  /* PREV */
        KEEP(*(.nx_init_fn.3))  /* BSP */
        KEEP(*(.nx_init_fn.4))  /* DRIVER */
        KEEP(*(.nx_init_fn.5))  /* COMPONENT */
        KEEP(*(.nx_init_fn.6))  /* APP */
        KEEP(*(.nx_init_fn.7))  /* 边界标记 */
        
        /* 结束边界 */
        PROVIDE(__nx_init_fn_end = .);
    } > FLASH
    
    /* 固件信息段 */
    .nx_fw_info : {
        KEEP(*(.nx_fw_info))
    } > FLASH
    
    /* 其他段... */
}
```

### Arm Compiler Scatter File

```scatter
; scatter_file.sct

LR_IROM1 0x08000000 {
    ; 代码段
    ER_IROM1 0x08000000 {
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
    RW_IRAM1 0x20000000 {
        .ANY (+RW +ZI)
    }
}
```

### IAR 链接器配置

```icf
/* linker_config.icf */

define symbol __ICFEDIT_region_ROM_start__ = 0x08000000;
define symbol __ICFEDIT_region_ROM_end__   = 0x0807FFFF;
define symbol __ICFEDIT_region_RAM_start__ = 0x20000000;
define symbol __ICFEDIT_region_RAM_end__   = 0x2001FFFF;

define region ROM_region = mem:[from __ICFEDIT_region_ROM_start__ 
                                to __ICFEDIT_region_ROM_end__];
define region RAM_region = mem:[from __ICFEDIT_region_RAM_start__ 
                                to __ICFEDIT_region_RAM_end__];

/* 初始化函数段 */
place in ROM_region { section nx_init_fn };

/* 固件信息段 */
place in ROM_region { section nx_fw_info };

/* 标准段 */
place in ROM_region { readonly };
place in RAM_region { readwrite, zeroinit };
```

---

## 平台差异

### ARM Cortex-M

**特点**:
- ✅ 完全支持
- ✅ 所有编译器支持
- ✅ 裸机和 RTOS 模式

**配置**:
```c
/* 无特殊配置需求 */
```

### ARM Cortex-A

**特点**:
- ✅ 完全支持
- ⚠️ 需要 MMU 配置
- ✅ Linux 和 RTOS 支持

**配置**:
```c
/* 确保初始化段在可执行内存中 */
```

### RISC-V

**特点**:
- ✅ 完全支持
- ✅ GCC 支持良好
- ⚠️ 链接器脚本需要适配

**链接器脚本**:
```ld
SECTIONS
{
    .text : {
        *(.text.init)
        *(.text*)
    } > flash
    
    .nx_init_fn : {
        PROVIDE(__nx_init_fn_start = .);
        KEEP(*(.nx_init_fn.*))
        PROVIDE(__nx_init_fn_end = .);
    } > flash
}
```

### x86/x64

**特点**:
- ⚠️ 测试模式
- ✅ 用于单元测试
- ❌ 不推荐用于生产

**注意事项**:
- 链接器段行为可能不同
- 主要用于开发和测试

---

## 移植检查清单

### 编译器检查

- [ ] 段属性宏定义正确
- [ ] 弱符号支持
- [ ] 函数指针类型兼容
- [ ] 边界符号可访问

### 链接器检查

- [ ] 初始化段定义
- [ ] 段顺序正确
- [ ] KEEP 指令防止优化
- [ ] 边界符号导出

### 功能测试

- [ ] 基本初始化测试
- [ ] 多级别初始化测试
- [ ] 启动框架测试
- [ ] 固件信息测试

### 性能测试

- [ ] 初始化时间测量
- [ ] 内存占用检查
- [ ] 代码大小分析

### 集成测试

- [ ] 与现有代码集成
- [ ] RTOS 集成（如适用）
- [ ] 多模块初始化

---

## 常见问题

### Q1: 链接器报错找不到边界符号

**问题**:
```
undefined reference to `__nx_init_fn_start'
```

**解决**:
检查链接器脚本是否正确定义了边界符号：
```ld
PROVIDE(__nx_init_fn_start = .);
PROVIDE(__nx_init_fn_end = .);
```

### Q2: 初始化函数未执行

**问题**: `nx_init_get_stats()` 显示 `total_count = 0`

**可能原因**:
1. 链接器优化移除了段
2. 段名不匹配
3. KEEP 指令缺失

**解决**:
```ld
/* 添加 KEEP 防止优化 */
KEEP(*(.nx_init_fn.*))
```

### Q3: 初始化顺序错误

**问题**: 驱动在 BSP 之前初始化

**原因**: 链接器段排序问题

**解决**:
```ld
/* 明确指定顺序 */
KEEP(*(.nx_init_fn.1))  /* BOARD */
KEEP(*(.nx_init_fn.2))  /* PREV */
KEEP(*(.nx_init_fn.3))  /* BSP */
KEEP(*(.nx_init_fn.4))  /* DRIVER */
```

### Q4: RTOS 模式下启动失败

**问题**: 系统在 `nx_startup()` 后挂起

**可能原因**:
1. `nx_os_init()` 未正确实现
2. 调度器未启动
3. 栈大小不足

**解决**:
```c
void nx_os_init(void) {
    /* 确保 RTOS 内核已初始化 */
    osKernelInitialize();
}

/* 检查栈配置 */
#define NX_STARTUP_MAIN_STACK_SIZE 8192
```

### Q5: 固件信息无法读取

**问题**: `nx_get_firmware_info()` 返回 NULL

**原因**: 固件信息段被优化或未链接

**解决**:
```ld
/* 链接器脚本中添加 */
.nx_fw_info : {
    KEEP(*(.nx_fw_info))
} > FLASH
```

---

## 移植示例

### 示例 1: 新 MCU 平台

```c
/* 1. 板级初始化 */
void nx_board_init(void) {
    /* 配置系统时钟 */
    SystemClock_Config();
    
    /* 配置 GPIO */
    GPIO_Init();
}

/* 2. 驱动初始化 */
static int uart_init(void) {
    /* UART 初始化 */
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_init);

/* 3. 主函数 */
int main(void) {
    /* 应用逻辑 */
    return 0;
}
```

### 示例 2: 新 RTOS 集成

```c
/* 1. OS 初始化 */
void nx_os_init(void) {
    /* 初始化 RTOS 内核 */
    MyRTOS_Init();
}

/* 2. 主函数作为任务运行 */
int main(void) {
    /* 创建应用任务 */
    MyRTOS_CreateTask(app_task, ...);
    
    /* 主任务逻辑 */
    while (1) {
        MyRTOS_Delay(1000);
    }
    
    return 0;
}
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
