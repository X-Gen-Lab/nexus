# Design Document: STM32F4 HAL Adapter

## Overview

本设计文档描述了基于 ST 官方固件库 (stm32f4xx-hal-driver) 和 ARM CMSIS 实现 STM32F4 HAL 适配层的技术方案。该适配层将 Nexus 框架的抽象 HAL 接口映射到 STM32F4 硬件外设。

### 设计目标

1. 完全实现 Nexus HAL 接口定义的所有外设驱动
2. 基于 ST 官方 HAL 库的寄存器定义，确保硬件访问的正确性
3. 集成 ARM CMSIS-Core 提供标准化的 Cortex-M4 处理器访问
4. 支持 STM32F407VG 作为主要目标设备
5. 提供完整的中断处理和回调机制
6. **可扩展架构**: 支持未来扩展到其他 Cortex-M 系列 (M0/M0+/M3/M7/M33)

### 外部依赖

- **CMSIS-Core**: ARM Cortex-M4 处理器核心支持 (core_cm4.h)
- **STM32F4xx CMSIS Device**: ST 设备头文件 (stm32f4xx.h, stm32f407xx.h)
- **STM32F4xx HAL Driver**: ST HAL 驱动库函数

### ST HAL 封装策略

本适配层采用 **封装 ST HAL 函数** 的策略实现外设驱动，而非直接操作寄存器：

#### 设计原则

1. **优先使用官方实现**: 当 ST HAL 提供相应功能时，封装 ST HAL 函数而非重新实现
2. **保持接口一致**: Nexus HAL 接口保持不变，内部实现调用 ST HAL 函数
3. **参数映射**: 将 Nexus HAL 参数映射到 ST HAL 数据结构 (如 GPIO_InitTypeDef, UART_HandleTypeDef)
4. **错误码转换**: 将 ST HAL 返回值 (HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT) 映射到 Nexus 错误码

#### 封装示例

| 外设 | Nexus HAL 函数 | ST HAL 函数 |
|------|----------------|-------------|
| GPIO | hal_gpio_init | HAL_GPIO_Init() |
| GPIO | hal_gpio_write | HAL_GPIO_WritePin() |
| GPIO | hal_gpio_read | HAL_GPIO_ReadPin() |
| GPIO | hal_gpio_toggle | HAL_GPIO_TogglePin() |
| UART | hal_uart_init | HAL_UART_Init() |
| UART | hal_uart_transmit | HAL_UART_Transmit() |
| UART | hal_uart_receive | HAL_UART_Receive() |
| SPI | hal_spi_init | HAL_SPI_Init() |
| SPI | hal_spi_transfer | HAL_SPI_TransmitReceive() |
| I2C | hal_i2c_init | HAL_I2C_Init() |
| I2C | hal_i2c_master_transmit | HAL_I2C_Master_Transmit() |
| I2C | hal_i2c_mem_read | HAL_I2C_Mem_Read() |
| Timer | hal_timer_init | HAL_TIM_Base_Init() |
| Timer | hal_pwm_init | HAL_TIM_PWM_Init() |
| ADC | hal_adc_init | HAL_ADC_Init() |
| ADC | hal_adc_read | HAL_ADC_Start() + HAL_ADC_PollForConversion() + HAL_ADC_GetValue() |

#### 优势

- **可靠性**: 使用经过 ST 验证的官方实现
- **维护性**: 减少自定义代码，降低维护成本
- **兼容性**: 与 ST 生态系统工具和示例代码兼容
- **文档**: 可参考 ST 官方文档和示例

### 官方代码集成 (Git Submodules)

本适配层通过 git submodule 集成官方代码库，确保使用经过验证的官方实现：

#### Submodule 配置

```
vendors/
├── arm/
│   └── CMSIS_5/                  # ARM CMSIS 核心
│       └── (git submodule: ARM-software/CMSIS_5)
└── st/
    ├── stm32f4xx_hal_driver/     # ST HAL 驱动库
    │   └── (git submodule: STMicroelectronics/stm32f4xx_hal_driver)
    └── cmsis_device_f4/          # ST CMSIS 设备文件
        └── (git submodule: STMicroelectronics/cmsis_device_f4)
```

#### Submodule 仓库

| Submodule | 仓库 URL | 用途 |
|-----------|----------|------|
| stm32f4xx_hal_driver | https://github.com/STMicroelectronics/stm32f4xx_hal_driver | ST HAL 驱动和寄存器定义 |
| cmsis_device_f4 | https://github.com/STMicroelectronics/cmsis_device_f4 | STM32F4 设备头文件和启动代码 |
| CMSIS_5 | https://github.com/ARM-software/CMSIS_5 | ARM CMSIS-Core 头文件 |

#### 头文件引用路径

```cmake
# CMakeLists.txt 中的包含路径配置
set(CMSIS_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include
)

set(ST_DEVICE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include
)

set(ST_HAL_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver/Inc
)
```

#### 初始化命令

```bash
# 克隆项目后初始化 submodules
git submodule update --init --recursive

# 或在克隆时一并获取
git clone --recursive <repository-url>
```

#### 版本管理

- 每个 submodule 锁定到特定 commit/tag
- 版本信息记录在 `.gitmodules` 和 `vendors/VERSIONS.md`
- 更新 submodule 需要显式操作并测试兼容性

### 编译器支持

本适配层支持以下编译器：
- **GCC (arm-none-eabi-gcc)**: GNU Arm Embedded Toolchain
- **Clang (armclang)**: Arm Compiler 6 / LLVM-based
- **IAR (iccarm)**: IAR Embedded Workbench for Arm

### Cortex-M 系列扩展性设计

为支持未来扩展到其他 Cortex-M 系列，采用以下设计策略：

#### 支持的 Cortex-M 核心

| 核心 | FPU | DSP | TrustZone | 典型设备系列 |
|------|-----|-----|-----------|-------------|
| Cortex-M0 | ❌ | ❌ | ❌ | STM32F0, STM32L0 |
| Cortex-M0+ | ❌ | ❌ | ❌ | STM32L0, STM32G0 |
| Cortex-M3 | ❌ | ❌ | ❌ | STM32F1, STM32F2, STM32L1 |
| Cortex-M4 | ✅ | ✅ | ❌ | STM32F4, STM32L4, STM32G4 |
| Cortex-M7 | ✅ | ✅ | ❌ | STM32F7, STM32H7 |
| Cortex-M33 | ✅ | ✅ | ✅ | STM32L5, STM32U5 |

#### 核心特性抽象

```c
/* core_config.h - Cortex-M 核心配置抽象 */
#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

/* 核心类型定义 */
#define CORE_CM0    0
#define CORE_CM0P   1
#define CORE_CM3    3
#define CORE_CM4    4
#define CORE_CM7    7
#define CORE_CM33   33

/* 当前核心配置 (由设备头文件定义) */
#ifndef CORE_TYPE
    #if defined(__CORTEX_M) && (__CORTEX_M == 4)
        #define CORE_TYPE   CORE_CM4
    #elif defined(__CORTEX_M) && (__CORTEX_M == 7)
        #define CORE_TYPE   CORE_CM7
    #elif defined(__CORTEX_M) && (__CORTEX_M == 33)
        #define CORE_TYPE   CORE_CM33
    #elif defined(__CORTEX_M) && (__CORTEX_M == 3)
        #define CORE_TYPE   CORE_CM3
    #elif defined(__CORTEX_M) && (__CORTEX_M == 0)
        #if defined(__CM0PLUS_REV)
            #define CORE_TYPE   CORE_CM0P
        #else
            #define CORE_TYPE   CORE_CM0
        #endif
    #else
        #error "Unknown Cortex-M core type"
    #endif
#endif

/* FPU 支持检测 */
#if (CORE_TYPE == CORE_CM4) || (CORE_TYPE == CORE_CM7) || (CORE_TYPE == CORE_CM33)
    #define CORE_HAS_FPU    1
#else
    #define CORE_HAS_FPU    0
#endif

/* DSP 指令支持检测 */
#if (CORE_TYPE == CORE_CM4) || (CORE_TYPE == CORE_CM7) || (CORE_TYPE == CORE_CM33)
    #define CORE_HAS_DSP    1
#else
    #define CORE_HAS_DSP    0
#endif

/* MPU 支持检测 */
#if (CORE_TYPE >= CORE_CM3)
    #define CORE_HAS_MPU    1
#else
    #define CORE_HAS_MPU    0
#endif

/* Cache 支持检测 (仅 CM7) */
#if (CORE_TYPE == CORE_CM7)
    #define CORE_HAS_CACHE  1
#else
    #define CORE_HAS_CACHE  0
#endif

/* TrustZone 支持检测 */
#if (CORE_TYPE == CORE_CM33)
    #define CORE_HAS_TZ     1
#else
    #define CORE_HAS_TZ     0
#endif

/* NVIC 优先级位数 */
#if (CORE_TYPE == CORE_CM0) || (CORE_TYPE == CORE_CM0P)
    #define CORE_NVIC_PRIO_BITS     2
#elif (CORE_TYPE == CORE_CM3) || (CORE_TYPE == CORE_CM4)
    #define CORE_NVIC_PRIO_BITS     4
#elif (CORE_TYPE == CORE_CM7) || (CORE_TYPE == CORE_CM33)
    #define CORE_NVIC_PRIO_BITS     4  /* 可配置 */
#endif

#endif /* CORE_CONFIG_H */
```

## Architecture

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
├─────────────────────────────────────────────────────────────┤
│                    Nexus HAL Interface                       │
│  (hal_gpio.h, hal_uart.h, hal_spi.h, hal_i2c.h, etc.)       │
├─────────────────────────────────────────────────────────────┤
│                 STM32F4 HAL Adapter Layer                    │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐   │
│  │  GPIO    │  UART    │   SPI    │   I2C    │  Timer   │   │
│  │ Driver   │ Driver   │  Driver  │  Driver  │  Driver  │   │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘   │
│  ┌──────────┬──────────────────────────────────────────┐    │
│  │   ADC    │           System Driver                   │    │
│  │  Driver  │    (Clock, SysTick, Delay, Reset)        │    │
│  └──────────┴──────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│              ST HAL Register Definitions                     │
│         (stm32f4xx.h, stm32f407xx.h peripherals)            │
├─────────────────────────────────────────────────────────────┤
│                    CMSIS-Core Layer                          │
│    (core_cm4.h, NVIC, SysTick, SCB, FPU access)             │
├─────────────────────────────────────────────────────────────┤
│                   STM32F4 Hardware                           │
└─────────────────────────────────────────────────────────────┘
```


### 文件结构

```
vendors/                            # 硬件芯片厂商官方库 (git submodules)
├── arm/
│   └── CMSIS_5/                    # ARM CMSIS-Core
│       └── CMSIS/Core/Include/     # core_cm4.h, cmsis_gcc.h 等
├── st/
│   ├── cmsis_device_f4/            # ST CMSIS 设备文件
│   │   └── Include/                # stm32f4xx.h, stm32f407xx.h 等
│   └── stm32f4xx_hal_driver/       # ST HAL 驱动库
│       └── Inc/                    # stm32f4xx_hal_*.h
└── VERSIONS.md                     # 版本记录文件

platforms/stm32f4/
├── CMakeLists.txt              # 平台构建配置
├── include/
│   ├── stm32f4xx_hal_conf.h    # HAL 配置文件
│   ├── system_stm32f4xx.h      # 系统配置头文件
│   ├── compiler_abstraction.h  # 编译器抽象层
│   └── core_config.h           # Cortex-M 核心配置
├── linker/
│   ├── STM32F407VGTx_FLASH.ld  # GCC/Clang 链接脚本
│   └── STM32F407VGTx_FLASH.icf # IAR 链接脚本
└── src/
    ├── startup_stm32f407xx.c   # 启动代码和向量表
    ├── system_stm32f4xx.c      # 系统初始化
    ├── hal_system_stm32f4.c    # HAL 系统驱动
    ├── hal_gpio_stm32f4.c      # GPIO 驱动
    ├── hal_uart_stm32f4.c      # UART 驱动
    ├── hal_spi_stm32f4.c       # SPI 驱动
    ├── hal_i2c_stm32f4.c       # I2C 驱动
    ├── hal_timer_stm32f4.c     # Timer 驱动
    └── hal_adc_stm32f4.c       # ADC 驱动
```

### 编译器抽象层

为支持多编译器，定义统一的编译器抽象宏：

```c
/* compiler_abstraction.h */
#ifndef COMPILER_ABSTRACTION_H
#define COMPILER_ABSTRACTION_H

#include "core_config.h"

/* 编译器检测 */
#if defined(__GNUC__) && !defined(__clang__)
    #define COMPILER_GCC    1
#elif defined(__clang__)
    #define COMPILER_CLANG  1
#elif defined(__ICCARM__)
    #define COMPILER_IAR    1
#else
    #error "Unsupported compiler"
#endif

/* 内联函数 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_INLINE      static inline __attribute__((always_inline))
#elif defined(COMPILER_IAR)
    #define HAL_INLINE      static inline
#endif

/* 弱符号定义 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_WEAK        __attribute__((weak))
#elif defined(COMPILER_IAR)
    #define HAL_WEAK        __weak
#endif

/* 对齐属性 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_ALIGN(n)    __attribute__((aligned(n)))
#elif defined(COMPILER_IAR)
    #define HAL_ALIGN(n)    _Pragma("data_alignment=" #n)
#endif

/* 段定义 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_SECTION(name)   __attribute__((section(name)))
#elif defined(COMPILER_IAR)
    #define HAL_SECTION(name)   @ name
#endif

/* 无返回函数 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_NORETURN    __attribute__((noreturn))
#elif defined(COMPILER_IAR)
    #define HAL_NORETURN    __noreturn
#endif

/* 内存屏障 - 根据核心类型选择实现 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #if (CORE_TYPE >= CORE_CM3)
        #define HAL_DSB()       __asm volatile("dsb" ::: "memory")
        #define HAL_ISB()       __asm volatile("isb" ::: "memory")
        #define HAL_DMB()       __asm volatile("dmb" ::: "memory")
    #else
        /* CM0/CM0+ 没有 DSB/ISB/DMB 指令，使用 NOP 序列 */
        #define HAL_DSB()       __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
        #define HAL_ISB()       __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
        #define HAL_DMB()       __asm volatile("nop\nnop\nnop\nnop" ::: "memory")
    #endif
#elif defined(COMPILER_IAR)
    #define HAL_DSB()       __DSB()
    #define HAL_ISB()       __ISB()
    #define HAL_DMB()       __DMB()
#endif

/* 中断控制 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    #define HAL_DISABLE_IRQ()   __asm volatile("cpsid i" ::: "memory")
    #define HAL_ENABLE_IRQ()    __asm volatile("cpsie i" ::: "memory")
#elif defined(COMPILER_IAR)
    #define HAL_DISABLE_IRQ()   __disable_irq()
    #define HAL_ENABLE_IRQ()    __enable_irq()
#endif

/* 获取 PRIMASK */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    HAL_INLINE uint32_t hal_get_primask(void) {
        uint32_t result;
        __asm volatile("mrs %0, primask" : "=r"(result));
        return result;
    }
    HAL_INLINE void hal_set_primask(uint32_t primask) {
        __asm volatile("msr primask, %0" :: "r"(primask) : "memory");
    }
#elif defined(COMPILER_IAR)
    #define hal_get_primask()   __get_PRIMASK()
    #define hal_set_primask(x)  __set_PRIMASK(x)
#endif

/* FPU 控制 (仅支持 FPU 的核心) */
#if CORE_HAS_FPU
    #if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
        HAL_INLINE void hal_fpu_enable(void) {
            /* 使能 CP10 和 CP11 协处理器 */
            SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
            HAL_DSB();
            HAL_ISB();
        }
    #elif defined(COMPILER_IAR)
        #define hal_fpu_enable()    do { \
            SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2)); \
            __DSB(); __ISB(); \
        } while(0)
    #endif
#else
    #define hal_fpu_enable()    /* FPU not available */
#endif

/* Cache 控制 (仅 CM7) */
#if CORE_HAS_CACHE
    HAL_INLINE void hal_icache_enable(void) {
        SCB_EnableICache();
    }
    HAL_INLINE void hal_dcache_enable(void) {
        SCB_EnableDCache();
    }
    HAL_INLINE void hal_dcache_clean(void) {
        SCB_CleanDCache();
    }
    HAL_INLINE void hal_dcache_invalidate(void) {
        SCB_InvalidateDCache();
    }
#else
    #define hal_icache_enable()     /* Cache not available */
    #define hal_dcache_enable()     /* Cache not available */
    #define hal_dcache_clean()      /* Cache not available */
    #define hal_dcache_invalidate() /* Cache not available */
#endif

/* MPU 控制 (CM3+) */
#if CORE_HAS_MPU
    HAL_INLINE void hal_mpu_enable(void) {
        ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
    }
    HAL_INLINE void hal_mpu_disable(void) {
        ARM_MPU_Disable();
    }
#else
    #define hal_mpu_enable()    /* MPU not available */
    #define hal_mpu_disable()   /* MPU not available */
#endif

#endif /* COMPILER_ABSTRACTION_H */
```

## Components and Interfaces

### 1. CMSIS 集成组件

#### 1.1 CMSIS-Core 头文件集成

```c
/* 使用 submodule 中的官方头文件 */
/* 包含路径: vendors/arm/CMSIS_5/CMSIS/Core/Include */
/* 包含路径: vendors/st/cmsis_device_f4/Include */

/* stm32f4xx_hal_conf.h 或平台配置中引用 */
#include "stm32f4xx.h"  /* 来自 vendors/st/cmsis_device_f4/Include */

/* stm32f4xx.h 内部会包含:
 * - core_cm4.h (来自 vendors/arm/CMSIS_5/CMSIS/Core/Include)
 * - stm32f407xx.h (来自 vendors/st/cmsis_device_f4/Include)
 */
```

#### 1.2 中断向量表

```c
/* startup_stm32f407xx.c - 向量表定义 (多编译器支持) */
#include "compiler_abstraction.h"

typedef void (*vector_fn)(void);

extern uint32_t _estack;

/* GCC/Clang 向量表定义 */
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
__attribute__((section(".isr_vector")))
const vector_fn g_pfnVectors[] = {
#elif defined(COMPILER_IAR)
#pragma location = ".intvec"
const vector_fn __vector_table[] = {
#endif
    (vector_fn)&_estack,        /* Initial Stack Pointer */
    Reset_Handler,              /* Reset Handler */
    NMI_Handler,                /* NMI Handler */
    HardFault_Handler,          /* Hard Fault Handler */
    MemManage_Handler,          /* MPU Fault Handler */
    BusFault_Handler,           /* Bus Fault Handler */
    UsageFault_Handler,         /* Usage Fault Handler */
    0, 0, 0, 0,                 /* Reserved */
    SVC_Handler,                /* SVCall Handler */
    DebugMon_Handler,           /* Debug Monitor Handler */
    0,                          /* Reserved */
    PendSV_Handler,             /* PendSV Handler */
    SysTick_Handler,            /* SysTick Handler */
    /* External Interrupts */
    WWDG_IRQHandler,            /* Window Watchdog */
    /* ... 更多中断处理程序 ... */
};

/* 默认中断处理程序 (弱符号) */
HAL_WEAK void NMI_Handler(void)         { while(1); }
HAL_WEAK void HardFault_Handler(void)   { while(1); }
HAL_WEAK void MemManage_Handler(void)   { while(1); }
HAL_WEAK void BusFault_Handler(void)    { while(1); }
HAL_WEAK void UsageFault_Handler(void)  { while(1); }
HAL_WEAK void SVC_Handler(void)         { }
HAL_WEAK void DebugMon_Handler(void)    { }
HAL_WEAK void PendSV_Handler(void)      { }
HAL_WEAK void SysTick_Handler(void)     { }
```


### 2. GPIO 驱动组件

#### 2.1 GPIO 寄存器映射

```c
#include "compiler_abstraction.h"

/* GPIO 端口基地址映射 */
static GPIO_TypeDef* const gpio_ports[HAL_GPIO_PORT_MAX] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH
};

/* GPIO 时钟使能位 */
static const uint32_t gpio_clk_bits[HAL_GPIO_PORT_MAX] = {
    RCC_AHB1ENR_GPIOAEN, RCC_AHB1ENR_GPIOBEN, /* ... */
};

/* 使能 GPIO 时钟 (使用编译器抽象的内存屏障) */
HAL_INLINE void gpio_enable_clock(hal_gpio_port_t port) {
    RCC->AHB1ENR |= gpio_clk_bits[port];
    HAL_DSB();  /* 确保时钟使能完成 */
}
```

#### 2.2 GPIO 配置算法

```
GPIO_Init(port, pin, config):
    1. 验证参数 (port < MAX, pin <= 15, config != NULL)
    2. 使能 GPIO 端口时钟 (RCC->AHB1ENR |= clk_bit)
    3. 配置模式寄存器 (MODER): 输入/输出/复用/模拟
    4. 配置输出类型 (OTYPER): 推挽/开漏
    5. 配置速度 (OSPEEDR): 低/中/高/极高
    6. 配置上下拉 (PUPDR): 无/上拉/下拉
    7. 设置初始电平 (BSRR): 高/低
    8. 返回 HAL_OK
```

#### 2.3 GPIO 中断配置

```
GPIO_IRQ_Config(port, pin, mode, callback, context):
    1. 使能 SYSCFG 时钟
    2. 配置 EXTI 线路选择 (SYSCFG->EXTICR[pin/4])
    3. 配置触发边沿 (EXTI->RTSR, EXTI->FTSR)
    4. 存储回调函数和上下文
    5. 设置 NVIC 优先级
    6. 返回 HAL_OK
```

### 3. UART 驱动组件

#### 3.1 UART 实例映射

```c
/* UART 实例数据结构 */
typedef struct {
    USART_TypeDef* instance;        /* USART 外设指针 */
    hal_uart_config_t config;       /* 配置参数 */
    hal_uart_rx_callback_t rx_cb;   /* 接收回调 */
    hal_uart_tx_callback_t tx_cb;   /* 发送回调 */
    void* rx_context;               /* 接收上下文 */
    void* tx_context;               /* 发送上下文 */
    bool initialized;               /* 初始化标志 */
} uart_data_t;

static uart_data_t uart_data[3] = {
    {USART1, ...}, {USART2, ...}, {USART3, ...}
};
```

#### 3.2 波特率计算

```
BRR = f_clk / baudrate
其中:
- USART1: f_clk = APB2 时钟 (通常 84MHz)
- USART2/3: f_clk = APB1 时钟 (通常 42MHz)
```


### 4. SPI 驱动组件

#### 4.1 SPI 实例映射

```c
typedef struct {
    SPI_TypeDef* instance;          /* SPI 外设指针 */
    hal_spi_config_t config;        /* 配置参数 */
    hal_spi_callback_t callback;    /* 完成回调 */
    void* context;                  /* 回调上下文 */
    bool initialized;               /* 初始化标志 */
} spi_data_t;

static spi_data_t spi_data[3] = {
    {SPI1, ...}, {SPI2, ...}, {SPI3, ...}
};
```

#### 4.2 SPI 模式配置

```
SPI Mode | CPOL | CPHA | CR1 配置
---------|------|------|----------
Mode 0   |  0   |  0   | 0x0000
Mode 1   |  0   |  1   | 0x0001
Mode 2   |  1   |  0   | 0x0002
Mode 3   |  1   |  1   | 0x0003
```

### 5. I2C 驱动组件

#### 5.1 I2C 实例映射

```c
typedef struct {
    I2C_TypeDef* instance;          /* I2C 外设指针 */
    hal_i2c_config_t config;        /* 配置参数 */
    hal_i2c_callback_t callback;    /* 事件回调 */
    void* context;                  /* 回调上下文 */
    bool initialized;               /* 初始化标志 */
} i2c_data_t;

static i2c_data_t i2c_data[3] = {
    {I2C1, ...}, {I2C2, ...}, {I2C3, ...}
};
```

#### 5.2 I2C 时钟配置

```
Standard Mode (100kHz):  CCR = f_pclk / (2 * 100000)
Fast Mode (400kHz):      CCR = f_pclk / (3 * 400000) (Duty=0)
                         CCR = f_pclk / (25 * 400000) (Duty=1)
```

### 6. Timer 驱动组件

#### 6.1 Timer 实例映射

```c
typedef struct {
    TIM_TypeDef* instance;          /* TIM 外设指针 */
    hal_timer_config_t config;      /* 配置参数 */
    hal_timer_callback_t callback;  /* 溢出回调 */
    void* context;                  /* 回调上下文 */
    bool initialized;               /* 初始化标志 */
} timer_data_t;

static timer_data_t timer_data[4] = {
    {TIM2, ...}, {TIM3, ...}, {TIM4, ...}, {TIM5, ...}
};
```

#### 6.2 PWM 占空比计算

```
ARR = (f_tim / frequency) - 1
CCR = (ARR + 1) * duty_cycle / 10000
其中 duty_cycle 范围: 0-10000 (0.00% - 100.00%)
```


### 7. ADC 驱动组件 (封装 ST HAL)

#### 7.1 ADC 实例映射

```c
/* ADC 驱动数据结构 - 封装 ST HAL Handle */
typedef struct {
    ADC_HandleTypeDef hadc;         /* ST HAL ADC Handle */
    hal_adc_config_t config;        /* Nexus 配置参数 */
    hal_adc_callback_t callback;    /* 转换完成回调 */
    void* context;                  /* 回调上下文 */
    bool initialized;               /* 初始化标志 */
} adc_data_t;

static adc_data_t adc_data[3] = {
    {.hadc.Instance = ADC1, ...},
    {.hadc.Instance = ADC2, ...},
    {.hadc.Instance = ADC3, ...}
};
```

#### 7.2 ST HAL 函数封装映射

| Nexus HAL 函数 | ST HAL 函数 |
|----------------|-------------|
| hal_adc_init | HAL_ADC_Init() |
| hal_adc_deinit | HAL_ADC_DeInit() |
| hal_adc_config_channel | HAL_ADC_ConfigChannel() |
| hal_adc_read | HAL_ADC_Start() + HAL_ADC_PollForConversion() + HAL_ADC_GetValue() + HAL_ADC_Stop() |
| hal_adc_read_multi | 循环调用 HAL_ADC_ConfigChannel() + HAL_ADC_Start() + ... |
| hal_adc_set_callback | HAL_ADC_Start_IT() + HAL_ADC_ConvCpltCallback() |
| (中断处理) | HAL_ADC_IRQHandler() |
| (错误获取) | HAL_ADC_GetError() |

#### 7.3 ADC 分辨率配置

```
Resolution | ADC_RESOLUTION_* | 最大值
-----------|------------------|--------
12-bit     | ADC_RESOLUTION_12B | 4095
10-bit     | ADC_RESOLUTION_10B | 1023
8-bit      | ADC_RESOLUTION_8B  | 255
6-bit      | ADC_RESOLUTION_6B  | 63
```

#### 7.4 ADC 初始化示例

```c
hal_status_t hal_adc_init(hal_adc_instance_t instance, const hal_adc_config_t* config) {
    /* 参数验证 */
    if (instance >= HAL_ADC_INSTANCE_MAX || config == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
    adc_data_t* adc = &adc_data[instance];
    
    /* 配置 ST HAL ADC_InitTypeDef */
    adc->hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc->hadc.Init.Resolution = map_resolution(config->resolution);
    adc->hadc.Init.ScanConvMode = DISABLE;
    adc->hadc.Init.ContinuousConvMode = DISABLE;
    adc->hadc.Init.DiscontinuousConvMode = DISABLE;
    adc->hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc->hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->hadc.Init.NbrOfConversion = 1;
    adc->hadc.Init.DMAContinuousRequests = DISABLE;
    adc->hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    
    /* 调用 ST HAL 初始化 */
    if (HAL_ADC_Init(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }
    
    adc->config = *config;
    adc->initialized = true;
    return HAL_OK;
}
```

#### 7.5 ADC 读取示例

```c
hal_status_t hal_adc_read(hal_adc_instance_t instance, uint8_t channel, uint16_t* value) {
    adc_data_t* adc = &adc_data[instance];
    
    /* 配置通道 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = map_channel(channel);
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    
    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* 启动转换 */
    if (HAL_ADC_Start(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* 等待转换完成 */
    if (HAL_ADC_PollForConversion(&adc->hadc, HAL_MAX_DELAY) != HAL_OK) {
        HAL_ADC_Stop(&adc->hadc);
        return HAL_ERROR_TIMEOUT;
    }
    
    /* 获取结果 */
    *value = (uint16_t)HAL_ADC_GetValue(&adc->hadc);
    
    /* 停止 ADC */
    HAL_ADC_Stop(&adc->hadc);
    
    return HAL_OK;
}
```

#### 7.6 电压转换公式

```
voltage_mv = (raw_value * vref_mv) / max_value
其中 max_value 根据分辨率确定
```

### 8. System 驱动组件

#### 8.1 系统时钟配置

```c
/* 默认时钟配置: HSE 8MHz -> PLL -> 168MHz */
void SystemClock_Config(void) {
    /* 使能 HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    
    /* 配置 PLL: PLLM=8, PLLN=336, PLLP=2, PLLQ=7 */
    RCC->PLLCFGR = (8 << 0) | (336 << 6) | (0 << 16) | (7 << 24) |
                   RCC_PLLCFGR_PLLSRC_HSE;
    
    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));
    
    /* 配置 Flash 等待周期 */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    
    /* 配置总线分频: AHB=1, APB1=4, APB2=2 */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
                RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;
    
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    
    SystemCoreClock = 168000000;
}
```

#### 8.2 SysTick 配置

```c
#include "compiler_abstraction.h"

hal_status_t hal_system_init(void) {
    /* 配置 SysTick 为 1ms 中断 */
    if (SysTick_Config(SystemCoreClock / 1000) != 0) {
        return HAL_ERROR;
    }
    NVIC_SetPriority(SysTick_IRQn, 15);  /* 最低优先级 */
    return HAL_OK;
}

/* 进入临界区 (多编译器支持) */
uint32_t hal_enter_critical(void) {
    uint32_t primask = hal_get_primask();
    HAL_DISABLE_IRQ();
    return primask;
}

/* 退出临界区 */
void hal_exit_critical(uint32_t state) {
    hal_set_primask(state);
}
```

### 9. CMake 多编译器支持

```cmake
# CMakeLists.txt - 编译器检测和配置

# 检测编译器类型
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_TYPE "GCC")
    set(COMPILER_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.ld")
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_TYPE "CLANG")
    set(COMPILER_FLAGS "--target=arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.ld")
elseif(CMAKE_C_COMPILER_ID STREQUAL "IAR")
    set(COMPILER_TYPE "IAR")
    set(COMPILER_FLAGS "--cpu=Cortex-M4 --fpu=VFPv4_sp")
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407VGTx_FLASH.icf")
endif()

# Git Submodule 包含路径
set(CMSIS_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include
)

set(ST_DEVICE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include
)

set(ST_HAL_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver/Inc
)

# 平台包含路径
target_include_directories(hal_stm32f4 PUBLIC
    ${CMSIS_INCLUDE_DIRS}
    ${ST_DEVICE_INCLUDE_DIRS}
    ${ST_HAL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

message(STATUS "Compiler: ${COMPILER_TYPE}")
message(STATUS "Linker script: ${LINKER_SCRIPT}")
```


## Data Models

### 外设寄存器结构

#### GPIO_TypeDef

```c
typedef struct {
    volatile uint32_t MODER;    /* 模式寄存器, offset: 0x00 */
    volatile uint32_t OTYPER;   /* 输出类型寄存器, offset: 0x04 */
    volatile uint32_t OSPEEDR;  /* 输出速度寄存器, offset: 0x08 */
    volatile uint32_t PUPDR;    /* 上下拉寄存器, offset: 0x0C */
    volatile uint32_t IDR;      /* 输入数据寄存器, offset: 0x10 */
    volatile uint32_t ODR;      /* 输出数据寄存器, offset: 0x14 */
    volatile uint32_t BSRR;     /* 位设置/复位寄存器, offset: 0x18 */
    volatile uint32_t LCKR;     /* 锁定寄存器, offset: 0x1C */
    volatile uint32_t AFR[2];   /* 复用功能寄存器, offset: 0x20-0x24 */
} GPIO_TypeDef;
```

#### USART_TypeDef

```c
typedef struct {
    volatile uint32_t SR;       /* 状态寄存器, offset: 0x00 */
    volatile uint32_t DR;       /* 数据寄存器, offset: 0x04 */
    volatile uint32_t BRR;      /* 波特率寄存器, offset: 0x08 */
    volatile uint32_t CR1;      /* 控制寄存器1, offset: 0x0C */
    volatile uint32_t CR2;      /* 控制寄存器2, offset: 0x10 */
    volatile uint32_t CR3;      /* 控制寄存器3, offset: 0x14 */
    volatile uint32_t GTPR;     /* 保护时间和预分频寄存器, offset: 0x18 */
} USART_TypeDef;
```

#### SPI_TypeDef

```c
typedef struct {
    volatile uint32_t CR1;      /* 控制寄存器1, offset: 0x00 */
    volatile uint32_t CR2;      /* 控制寄存器2, offset: 0x04 */
    volatile uint32_t SR;       /* 状态寄存器, offset: 0x08 */
    volatile uint32_t DR;       /* 数据寄存器, offset: 0x0C */
    volatile uint32_t CRCPR;    /* CRC多项式寄存器, offset: 0x10 */
    volatile uint32_t RXCRCR;   /* RX CRC寄存器, offset: 0x14 */
    volatile uint32_t TXCRCR;   /* TX CRC寄存器, offset: 0x18 */
    volatile uint32_t I2SCFGR;  /* I2S配置寄存器, offset: 0x1C */
    volatile uint32_t I2SPR;    /* I2S预分频寄存器, offset: 0x20 */
} SPI_TypeDef;
```

#### I2C_TypeDef

```c
typedef struct {
    volatile uint32_t CR1;      /* 控制寄存器1, offset: 0x00 */
    volatile uint32_t CR2;      /* 控制寄存器2, offset: 0x04 */
    volatile uint32_t OAR1;     /* 自身地址寄存器1, offset: 0x08 */
    volatile uint32_t OAR2;     /* 自身地址寄存器2, offset: 0x0C */
    volatile uint32_t DR;       /* 数据寄存器, offset: 0x10 */
    volatile uint32_t SR1;      /* 状态寄存器1, offset: 0x14 */
    volatile uint32_t SR2;      /* 状态寄存器2, offset: 0x18 */
    volatile uint32_t CCR;      /* 时钟控制寄存器, offset: 0x1C */
    volatile uint32_t TRISE;    /* 上升时间寄存器, offset: 0x20 */
    volatile uint32_t FLTR;     /* 滤波器寄存器, offset: 0x24 */
} I2C_TypeDef;
```


#### TIM_TypeDef

```c
typedef struct {
    volatile uint32_t CR1;      /* 控制寄存器1, offset: 0x00 */
    volatile uint32_t CR2;      /* 控制寄存器2, offset: 0x04 */
    volatile uint32_t SMCR;     /* 从模式控制寄存器, offset: 0x08 */
    volatile uint32_t DIER;     /* DMA/中断使能寄存器, offset: 0x0C */
    volatile uint32_t SR;       /* 状态寄存器, offset: 0x10 */
    volatile uint32_t EGR;      /* 事件生成寄存器, offset: 0x14 */
    volatile uint32_t CCMR1;    /* 捕获/比较模式寄存器1, offset: 0x18 */
    volatile uint32_t CCMR2;    /* 捕获/比较模式寄存器2, offset: 0x1C */
    volatile uint32_t CCER;     /* 捕获/比较使能寄存器, offset: 0x20 */
    volatile uint32_t CNT;      /* 计数器, offset: 0x24 */
    volatile uint32_t PSC;      /* 预分频器, offset: 0x28 */
    volatile uint32_t ARR;      /* 自动重装载寄存器, offset: 0x2C */
    uint32_t RESERVED1;
    volatile uint32_t CCR1;     /* 捕获/比较寄存器1, offset: 0x34 */
    volatile uint32_t CCR2;     /* 捕获/比较寄存器2, offset: 0x38 */
    volatile uint32_t CCR3;     /* 捕获/比较寄存器3, offset: 0x3C */
    volatile uint32_t CCR4;     /* 捕获/比较寄存器4, offset: 0x40 */
    /* ... */
} TIM_TypeDef;
```

#### ADC_TypeDef

```c
typedef struct {
    volatile uint32_t SR;       /* 状态寄存器, offset: 0x00 */
    volatile uint32_t CR1;      /* 控制寄存器1, offset: 0x04 */
    volatile uint32_t CR2;      /* 控制寄存器2, offset: 0x08 */
    volatile uint32_t SMPR1;    /* 采样时间寄存器1, offset: 0x0C */
    volatile uint32_t SMPR2;    /* 采样时间寄存器2, offset: 0x10 */
    volatile uint32_t JOFR1;    /* 注入通道数据偏移寄存器1, offset: 0x14 */
    volatile uint32_t JOFR2;    /* 注入通道数据偏移寄存器2, offset: 0x18 */
    volatile uint32_t JOFR3;    /* 注入通道数据偏移寄存器3, offset: 0x1C */
    volatile uint32_t JOFR4;    /* 注入通道数据偏移寄存器4, offset: 0x20 */
    volatile uint32_t HTR;      /* 看门狗高阈值寄存器, offset: 0x24 */
    volatile uint32_t LTR;      /* 看门狗低阈值寄存器, offset: 0x28 */
    volatile uint32_t SQR1;     /* 规则序列寄存器1, offset: 0x2C */
    volatile uint32_t SQR2;     /* 规则序列寄存器2, offset: 0x30 */
    volatile uint32_t SQR3;     /* 规则序列寄存器3, offset: 0x34 */
    volatile uint32_t JSQR;     /* 注入序列寄存器, offset: 0x38 */
    volatile uint32_t JDR1;     /* 注入数据寄存器1, offset: 0x3C */
    volatile uint32_t JDR2;     /* 注入数据寄存器2, offset: 0x40 */
    volatile uint32_t JDR3;     /* 注入数据寄存器3, offset: 0x44 */
    volatile uint32_t JDR4;     /* 注入数据寄存器4, offset: 0x48 */
    volatile uint32_t DR;       /* 规则数据寄存器, offset: 0x4C */
} ADC_TypeDef;
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: GPIO 配置一致性

*For any* valid GPIO port (A-H), pin (0-15), and configuration (direction, pull, speed, output_mode), calling `hal_gpio_init` followed by reading the GPIO registers SHALL result in register values that match the requested configuration.

**Validates: Requirements 3.1, 3.8**

### Property 2: GPIO 读写一致性

*For any* initialized GPIO output pin, calling `hal_gpio_write(port, pin, level)` followed by `hal_gpio_read(port, pin, &read_level)` SHALL return `read_level == level`.

**Validates: Requirements 3.3, 3.4**

### Property 3: GPIO Toggle 幂等性

*For any* initialized GPIO output pin with initial level L, calling `hal_gpio_toggle` twice SHALL result in the pin returning to level L. (toggle(toggle(L)) == L)

**Validates: Requirements 3.5**

### Property 4: GPIO 参数验证

*For any* invalid port (>= HAL_GPIO_PORT_MAX) or invalid pin (> 15), calling any GPIO function SHALL return HAL_ERROR_INVALID_PARAM without modifying hardware state.

**Validates: Requirements 3.2, 10.2**

### Property 5: UART 配置有效性

*For any* valid UART instance (0-2) and valid configuration (baudrate 9600-921600, valid wordlen/parity/stopbits), calling `hal_uart_init` SHALL configure the USART registers correctly and return HAL_OK.

**Validates: Requirements 4.1, 4.8, 4.9**

### Property 6: UART 参数验证

*For any* invalid baudrate (< 9600 or > 921600) or invalid instance (>= 3), calling `hal_uart_init` SHALL return HAL_ERROR_INVALID_PARAM.

**Validates: Requirements 4.2, 10.2**

### Property 7: UART 传输完整性

*For any* initialized UART instance and data buffer of length N, calling `hal_uart_transmit` SHALL send exactly N bytes in order, and calling `hal_uart_receive` SHALL receive exactly N bytes.

**Validates: Requirements 4.3, 4.4**

### Property 8: SPI 模式配置

*For any* SPI mode (0-3), the SPI CR1 register CPOL and CPHA bits SHALL be set according to the mode definition: Mode 0 (CPOL=0,CPHA=0), Mode 1 (CPOL=0,CPHA=1), Mode 2 (CPOL=1,CPHA=0), Mode 3 (CPOL=1,CPHA=1).

**Validates: Requirements 5.1, 5.7**

### Property 9: SPI 全双工传输

*For any* SPI transfer of length N, `hal_spi_transfer` SHALL transmit N bytes from tx_buffer while simultaneously receiving N bytes into rx_buffer.

**Validates: Requirements 5.5**


### Property 10: I2C 速度模式配置

*For any* I2C speed mode (Standard/Fast/Fast-Plus), the I2C CCR register SHALL be configured to achieve the target frequency within ±5% tolerance.

**Validates: Requirements 6.1, 6.9**

### Property 11: I2C 设备探测

*For any* I2C address, `hal_i2c_is_device_ready` SHALL return HAL_OK if the device ACKs the address, and HAL_ERROR_IO if NACK is received.

**Validates: Requirements 6.6, 6.7**

### Property 12: Timer 周期配置

*For any* timer period in microseconds, the timer ARR and PSC registers SHALL be configured such that the actual period is within ±1% of the requested period.

**Validates: Requirements 7.1**

### Property 13: Timer 启停控制

*For any* initialized timer, calling `hal_timer_start` SHALL set CR1.CEN=1, and calling `hal_timer_stop` SHALL clear CR1.CEN=0.

**Validates: Requirements 7.2, 7.3**

### Property 14: PWM 占空比精度

*For any* PWM duty cycle value D (0-10000), the CCR register SHALL be set such that actual_duty = D ± 1 (0.01% precision).

**Validates: Requirements 7.7, 7.8**

### Property 15: ADC 电压转换精度

*For any* raw ADC value V, resolution R, and reference voltage Vref, `hal_adc_to_millivolts(V, Vref)` SHALL return `(V * Vref) / max_value` where max_value = 2^R - 1.

**Validates: Requirements 8.4**

### Property 16: ADC 分辨率配置

*For any* ADC resolution (6/8/10/12 bit), the ADC CR1.RES bits SHALL be configured correctly: 12-bit=00, 10-bit=01, 8-bit=10, 6-bit=11.

**Validates: Requirements 8.1, 8.8**

### Property 17: 系统 Tick 单调递增

*For any* two consecutive calls to `hal_get_tick()` with t1 and t2, if no overflow occurs, then t2 >= t1.

**Validates: Requirements 9.3**

### Property 18: 延时精度

*For any* delay value D milliseconds, `hal_delay_ms(D)` SHALL block for at least D milliseconds and at most D + 2 milliseconds.

**Validates: Requirements 9.4**

### Property 19: 空指针检查

*For any* HAL function that accepts a pointer parameter, passing NULL SHALL return HAL_ERROR_NULL_POINTER without causing a crash.

**Validates: Requirements 10.1, 10.6**

### Property 20: 未初始化检查

*For any* peripheral operation function, calling it before the corresponding init function SHALL return HAL_ERROR_NOT_INIT.

**Validates: Requirements 10.3**


## Error Handling

### 错误码映射

| 场景 | 返回码 |
|------|--------|
| 空指针参数 | HAL_ERROR_NULL_POINTER |
| 无效端口/实例 | HAL_ERROR_INVALID_PARAM |
| 无效引脚号 | HAL_ERROR_INVALID_PARAM |
| 无效配置值 | HAL_ERROR_INVALID_PARAM |
| 外设未初始化 | HAL_ERROR_NOT_INIT |
| 外设已初始化 | HAL_ERROR_ALREADY_INIT |
| 操作超时 | HAL_ERROR_TIMEOUT |
| 硬件错误 | HAL_ERROR_IO |
| UART 奇偶校验错误 | HAL_ERROR_PARITY |
| UART 帧错误 | HAL_ERROR_FRAMING |
| UART 溢出错误 | HAL_ERROR_OVERRUN |
| I2C NACK | HAL_ERROR_IO |

### 错误处理策略

1. **参数验证优先**: 所有函数在执行任何硬件操作前先验证参数
2. **原子性保证**: 配置操作要么完全成功，要么不修改任何状态
3. **超时机制**: 所有阻塞操作支持超时参数
4. **错误恢复**: 错误发生后外设保持可用状态

### 超时处理

```c
/* 超时计数器实现 */
static inline bool wait_flag_timeout(volatile uint32_t* reg, 
                                     uint32_t flag, 
                                     uint32_t timeout_ms) {
    uint32_t start = hal_get_tick();
    while (!(*reg & flag)) {
        if (timeout_ms != HAL_WAIT_FOREVER) {
            if ((hal_get_tick() - start) >= timeout_ms) {
                return false;  /* 超时 */
            }
        }
    }
    return true;  /* 成功 */
}
```

## Testing Strategy

### 测试框架

- **单元测试**: 使用 Google Test (C++) 框架
- **属性测试**: 使用 RapidCheck 库进行属性基测试
- **硬件模拟**: 使用寄存器模拟层进行离线测试

### 测试分类

#### 1. 单元测试 (Unit Tests)

- 参数验证测试
- 边界条件测试
- 错误处理测试
- 特定示例测试

#### 2. 属性测试 (Property-Based Tests)

每个正确性属性对应一个属性测试，使用 RapidCheck 生成随机输入：

```cpp
// 示例: GPIO 读写一致性属性测试
RC_GTEST_PROP(GPIOProperties, ReadWriteConsistency,
              (hal_gpio_port_t port, hal_gpio_pin_t pin, hal_gpio_level_t level)) {
    // Feature: stm32f4-hal-adapter, Property 2: GPIO 读写一致性
    RC_PRE(port < HAL_GPIO_PORT_MAX && pin <= 15);
    
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT};
    RC_ASSERT(hal_gpio_init(port, pin, &config) == HAL_OK);
    RC_ASSERT(hal_gpio_write(port, pin, level) == HAL_OK);
    
    hal_gpio_level_t read_level;
    RC_ASSERT(hal_gpio_read(port, pin, &read_level) == HAL_OK);
    RC_ASSERT(read_level == level);
}
```

### 测试配置

- 属性测试最少运行 100 次迭代
- 每个测试标注对应的设计文档属性编号
- 测试覆盖所有外设驱动模块
