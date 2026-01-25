# Init Framework 架构设计文档

**版本**: 1.0.0  
**最后更新**: 2026-01-24

---

## 目录

- [系统架构](#系统架构)
- [自动初始化机制](#自动初始化机制)
- [启动框架](#启动框架)
- [固件信息](#固件信息)
- [设计决策](#设计决策)
- [性能分析](#性能分析)
- [安全考虑](#安全考虑)

---

## 系统架构

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     Init Framework                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │   nx_init    │  │  nx_startup  │  │nx_firmware   │    │
│  │              │  │              │  │    _info     │    │
│  │ 自动初始化   │  │  启动框架    │  │  固件信息    │    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
│         │                  │                  │            │
│         │                  │                  │            │
│         ▼                  ▼                  ▼            │
│  ┌──────────────────────────────────────────────────┐    │
│  │          链接器段 (Linker Sections)              │    │
│  │  .nx_init_fn.*  │  Entry Point  │  .nx_fw_info  │    │
│  └──────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
                    ┌───────────────┐
                    │  Application  │
                    └───────────────┘
```

### 模块关系

```
nx_startup
    │
    ├──→ nx_board_init()  [用户覆盖]
    │
    ├──→ nx_os_init()     [用户覆盖]
    │
    ├──→ nx_init_run()
    │       │
    │       ├──→ Level 1: BOARD
    │       ├──→ Level 2: PREV
    │       ├──→ Level 3: BSP
    │       ├──→ Level 4: DRIVER
    │       ├──→ Level 5: COMPONENT
    │       └──→ Level 6: APP
    │
    └──→ main()
```

---

## 自动初始化机制

### 核心原理

自动初始化基于**链接器段**（Linker Section）机制实现：

1. **编译期注册**: 使用宏将函数指针放入特定链接器段
2. **链接期排序**: 链接器按段名排序，确定执行顺序
3. **运行期执行**: 遍历段中的函数指针并执行

### 链接器段布局

```
内存布局:
┌────────────────────────────────────┐
│  .nx_init_fn.0  (边界标记)         │  ← __nx_init_fn_start
├────────────────────────────────────┤
│  .nx_init_fn.1  (BOARD)            │
│    ├─ clock_init                   │
│    └─ power_init                   │
├────────────────────────────────────┤
│  .nx_init_fn.2  (PREV)             │
│    ├─ heap_init                    │
│    └─ debug_init                   │
├────────────────────────────────────┤
│  .nx_init_fn.3  (BSP)              │
│    └─ gpio_init                    │
├────────────────────────────────────┤
│  .nx_init_fn.4  (DRIVER)           │
│    ├─ uart_init                    │
│    ├─ spi_init                     │
│    └─ i2c_init                     │
├────────────────────────────────────┤
│  .nx_init_fn.5  (COMPONENT)        │
│    ├─ fs_init                      │
│    └─ net_init                     │
├────────────────────────────────────┤
│  .nx_init_fn.6  (APP)              │
│    └─ app_init                     │
├────────────────────────────────────┤
│  .nx_init_fn.7  (边界标记)         │  ← __nx_init_fn_end
└────────────────────────────────────┘
```

### 实现细节

#### 宏展开

```c
/* 用户代码 */
NX_INIT_DRIVER_EXPORT(uart_init);

/* 展开为 (GCC) */
__attribute__((used))
__attribute__((section(".nx_init_fn.4")))
const nx_init_fn_t _nx_init_uart_init = uart_init;
```

#### 执行流程

```c
nx_status_t nx_init_run(void) {
    const nx_init_fn_t* fn_ptr;
    int ret;
    
    /* 遍历所有初始化函数 */
    for (fn_ptr = NX_INIT_FN_START; 
         fn_ptr < NX_INIT_FN_END; 
         fn_ptr++) {
        
        if (*fn_ptr == NULL) {
            continue;  /* 跳过 NULL 指针 */
        }
        
        /* 执行初始化函数 */
        ret = (*fn_ptr)();
        
        /* 记录统计信息 */
        if (ret != 0) {
            /* 记录失败但继续执行 */
        }
    }
    
    return NX_OK;
}
```

### 编译器适配

#### GCC / Clang

```c
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED __attribute__((used))

extern const nx_init_fn_t __nx_init_fn_start[];
extern const nx_init_fn_t __nx_init_fn_end[];
```

#### Arm Compiler 5/6

```c
#define NX_SECTION(x) __attribute__((section(x)))
#define NX_USED __attribute__((used))

extern const nx_init_fn_t Image$nx_init_fn$Base[];
extern const nx_init_fn_t Image$nx_init_fn$Limit[];
```

#### IAR

```c
#define NX_SECTION(x) _Pragma(#x)
#define NX_USED

#pragma section = "nx_init_fn"
#define NX_INIT_FN_START \
    ((const nx_init_fn_t*)__section_begin("nx_init_fn"))
#define NX_INIT_FN_END \
    ((const nx_init_fn_t*)__section_end("nx_init_fn"))
```

---

## 启动框架

### 入口点拦截

启动框架通过拦截程序入口点实现统一启动序列。

#### GCC 实现

```c
/* 定义 entry 函数作为真正的入口点 */
void entry(void) {
    /* 执行启动序列 */
    nx_startup();
    
    /* 不应该到达这里 */
    while (1);
}

/* 链接器选项: -eentry */
```

#### Arm Compiler 实现

```c
/* 使用 $Sub$main 拦截 main */
int $Sub$main(void) {
    /* 执行启动序列 */
    nx_startup();
    
    /* 不应该到达这里 */
    while (1);
    return 0;
}

/* 用户的 main 变为 $Super$main */
extern int $Super$main(void);
```

### 启动状态机

```
┌─────────────────┐
│  NOT_STARTED    │
└────────┬────────┘
         │ nx_startup()
         ▼
┌─────────────────┐
│   BOARD_INIT    │ ← nx_board_init()
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│    OS_INIT      │ ← nx_os_init()
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   AUTO_INIT     │ ← nx_init_run()
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  MAIN_RUNNING   │ ← main()
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│    COMPLETE     │
└─────────────────┘
```

### RTOS 集成

#### 裸机模式

```c
void nx_startup(void) {
    /* 1. 板级初始化 */
    nx_board_init();
    
    /* 2. OS 初始化（空操作） */
    nx_os_init();
    
    /* 3. 自动初始化 */
    nx_init_run();
    
    /* 4. 调用 main */
    main();
    
    /* 5. main 返回后进入死循环 */
    while (1);
}
```

#### RTOS 模式

```c
void nx_startup(void) {
    /* 1. 板级初始化 */
    nx_board_init();
    
    /* 2. OS 初始化 */
    nx_os_init();  /* 用户在此初始化 RTOS */
    
    /* 3. 自动初始化 */
    nx_init_run();
    
    /* 4. 创建 main 任务 */
    osThreadNew(main_thread, NULL, &main_thread_attr);
    
    /* 5. 启动调度器 */
    osKernelStart();
    
    /* 不应该到达这里 */
    while (1);
}
```

---

## 固件信息

### 数据结构

```c
typedef struct nx_firmware_info_s {
    char product[32];    /* 产品名称 */
    char factory[16];    /* 厂商标识 */
    char date[12];       /* 构建日期 */
    char time[12];       /* 构建时间 */
    uint32_t version;    /* 版本号 */
    uint32_t key;        /* 固件密钥 */
} nx_firmware_info_t;
```

### 版本编码

```
32-bit 版本号:
┌────────┬────────┬────────┬────────┐
│ Major  │ Minor  │ Patch  │ Build  │
│ [31:24]│ [23:16]│ [15:8] │ [7:0]  │
└────────┴────────┴────────┴────────┘

示例: 1.2.3.4
  Major = 1  (0x01)
  Minor = 2  (0x02)
  Patch = 3  (0x03)
  Build = 4  (0x04)
  
  Encoded = 0x01020304
```

### 链接器段

固件信息存储在独立的 `.nx_fw_info` 段中：

```ld
/* GCC 链接器脚本 */
.nx_fw_info : {
    KEEP(*(.nx_fw_info))
} > FLASH
```

这允许外部工具直接从二进制文件中提取固件信息，无需执行固件。

---

## 设计决策

### 1. 为什么使用链接器段？

**优势**:
- ✅ 零运行时开销 - 编译期确定
- ✅ 模块解耦 - 无需修改核心代码
- ✅ 自动排序 - 链接器保证顺序
- ✅ 类型安全 - 编译期检查

**劣势**:
- ❌ 编译器依赖 - 需要适配不同编译器
- ❌ 链接器配置 - 需要正确的链接器脚本
- ❌ 调试困难 - 初始化顺序不直观

**替代方案对比**:

| 方案 | 运行时开销 | 模块解耦 | 实现复杂度 |
|------|-----------|---------|-----------|
| 链接器段 | 无 | 完全 | 中 |
| 函数指针数组 | 小 | 部分 | 低 |
| 注册函数 | 中 | 完全 | 低 |
| 构造函数 | 小 | 完全 | 低 (C++) |

**选择理由**: 嵌入式系统对性能敏感，零运行时开销是关键优势。

### 2. 为什么 6 个初始化级别？

**设计考虑**:
- 足够细粒度以处理常见依赖关系
- 不过于复杂，易于理解和使用
- 覆盖典型的嵌入式系统初始化场景

**级别划分依据**:
1. **BOARD** - 硬件最底层（时钟、电源）
2. **PREV** - 基础设施（内存、调试）
3. **BSP** - 板级配置（引脚、中断）
4. **DRIVER** - 设备驱动
5. **COMPONENT** - 中间件
6. **APP** - 应用层

### 3. 为什么使用弱符号？

**优势**:
- ✅ 可选覆盖 - 用户可以选择性实现
- ✅ 默认实现 - 提供空实现避免链接错误
- ✅ 灵活性 - 不同项目可以有不同实现

**示例**:
```c
/* 框架提供弱符号默认实现 */
NX_WEAK void nx_board_init(void) {
    /* 空实现 */
}

/* 用户可以覆盖 */
void nx_board_init(void) {
    /* 用户的板级初始化 */
}
```

### 4. 错误处理策略

**设计原则**: 记录但继续

**理由**:
- 部分初始化失败不应阻止整个系统
- 允许系统在降级模式下运行
- 提供统计信息供应用决策

**实现**:
```c
/* 初始化失败不中断执行 */
for (fn_ptr = start; fn_ptr < end; fn_ptr++) {
    ret = (*fn_ptr)();
    if (ret != 0) {
        stats.fail_count++;
        stats.last_error = ret;
        /* 继续执行下一个 */
    }
}
```

---

## 性能分析

### 内存开销

#### 代码段

```
每个初始化函数:
  - 函数指针: 4 bytes (32-bit) / 8 bytes (64-bit)
  - 函数代码: 取决于实现

示例 (100 个初始化函数):
  - 函数指针表: 400 bytes (32-bit)
  - 总代码: ~10-20 KB (典型)
```

#### 数据段

```
固件信息:
  - nx_firmware_info_t: 80 bytes
  
启动框架:
  - 状态变量: 4 bytes
  - 配置: 8 bytes
  
初始化统计:
  - nx_init_stats_t: 12 bytes
```

### 执行时间

#### 初始化开销

```
nx_init_run() 执行时间:
  - 遍历开销: O(n), n = 初始化函数数量
  - 每次迭代: ~10-20 CPU 周期
  - 100 个函数: ~1000-2000 周期 (~10-20 μs @ 100MHz)
  
实际时间主要取决于初始化函数本身的执行时间。
```

#### 启动时间

```
典型启动序列:
  1. nx_board_init():    1-10 ms   (时钟配置)
  2. nx_os_init():       0-5 ms    (RTOS 初始化)
  3. nx_init_run():      10-100 ms (所有初始化)
  4. main():             -         (应用相关)
  
总计: 11-115 ms (不包括应用初始化)
```

### 优化建议

1. **减少初始化函数数量** - 合并相关初始化
2. **延迟初始化** - 非关键功能延迟到需要时
3. **并行初始化** - 在 RTOS 中使用多任务
4. **快速路径** - 关键路径优先初始化

---

## 安全考虑

### 1. 函数指针验证

**风险**: 损坏的函数指针导致系统崩溃

**缓解措施**:
```c
/* 检查 NULL 指针 */
if (*fn_ptr == NULL) {
    continue;
}

/* 可选: 检查地址范围 */
if ((uintptr_t)*fn_ptr < CODE_START ||
    (uintptr_t)*fn_ptr > CODE_END) {
    /* 无效地址 */
    continue;
}
```

### 2. 初始化顺序

**风险**: 错误的初始化顺序导致依赖问题

**缓解措施**:
- 明确文档化级别用途
- 代码审查检查级别选择
- 运行时依赖检查（可选）

### 3. 固件完整性

**风险**: 固件被篡改

**缓解措施**:
```c
/* 使用固件密钥进行完整性检查 */
uint32_t calculate_firmware_checksum(void);

void verify_firmware(void) {
    const nx_firmware_info_t* info = nx_get_firmware_info();
    uint32_t checksum = calculate_firmware_checksum();
    
    if (checksum != info->key) {
        /* 固件被篡改 */
        enter_safe_mode();
    }
}
```

### 4. 栈溢出

**风险**: 初始化函数栈使用过多

**缓解措施**:
- 限制初始化函数栈使用
- 使用栈保护（Stack Guard）
- 监控栈使用情况

### 5. 死锁预防

**风险**: 初始化函数中的锁导致死锁

**缓解措施**:
- 避免在初始化中使用锁
- 如必须使用，确保正确的锁顺序
- 使用超时机制

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
