# STM32 平台

STM32 平台为 Nexus 框架提供对 STM32 系列 MCU 的支持。

## 支持的系列

- STM32F0/F1/F2/F3/F4/F7
- STM32G0/G4
- STM32H5/H7
- STM32L0/L1/L4/L5
- STM32U0/U3/U5
- STM32WB/WL
- STM32C0

## 快速开始

### 1. 配置平台

```bash
# 使用 menuconfig 配置
python scripts/building/build.py --config

# 选择:
# - Platform: STM32
# - Series: STM32F4
# - Chip: STM32F407VG
# - Clock: HSE 25MHz, SYSCLK 180MHz
```

### 2. 编译

```bash
# 使用 CMake Preset
cmake --preset stm32f4-debug
cmake --build --preset stm32f4-debug

# 或使用构建脚本
python scripts/building/build.py --platform stm32f4
```

### 3. 使用示例

```c
#include "boot/stm32_boot.h"
#include "stm32f4xx_hal.h"

int main(void) {
    /* 初始化平台 */
    if (stm32_platform_init() != 0) {
        Error_Handler();
    }

    /* 初始化外设 */
    /* ... */

    /* 主循环 */
    while (1) {
        /* 用户代码 */
    }

    return 0;
}
```

## 模块结构

```
platforms/stm32/
├── src/
│   ├── boot/              # 启动和平台初始化
│   ├── clock/             # 时钟配置
│   ├── interrupt/         # 中断处理
│   ├── system/            # 系统功能和错误处理
│   ├── peripheral/        # 外设驱动 (未来)
│   └── hal/               # HAL 适配 (未来)
├── include/
│   ├── boot/              # 启动接口
│   ├── clock/             # 时钟接口
│   ├── interrupt/         # 中断接口
│   └── system/            # 系统接口
├── config/                # HAL 配置文件
├── linker/                # 链接脚本
├── chips/                 # 芯片配置
└── docs/                  # 文档
```

## 核心 API

### 平台初始化

```c
/* 初始化平台 (HAL + 时钟 + NVIC) */
int stm32_platform_init(void);

/* 反初始化平台 */
int stm32_platform_deinit(void);

/* 检查初始化状态 */
int stm32_platform_is_initialized(void);

/* 获取系统时钟频率 */
uint32_t stm32_platform_get_sysclk(void);
```

### 时钟配置

```c
/* 配置系统时钟 */
int SystemClock_Config(void);
```

### 错误处理

```c
/* HAL 错误回调 */
void Error_Handler(void);

/* 断言失败回调 (USE_FULL_ASSERT) */
void assert_failed(uint8_t* file, uint32_t line);
```

## 启动流程

```
硬件复位
  ↓
Reset_Handler (startup_*.s)
  ├─ 初始化堆栈
  ├─ 复制 .data 段
  ├─ 清零 .bss 段
  └─ 调用 SystemInit()
  ↓
SystemInit() (system_*.c)
  ├─ 配置 FPU
  ├─ 重置 RCC
  └─ 配置向量表
  ↓
main()
  ↓
stm32_platform_init()
  ├─ HAL_Init()
  ├─ SystemClock_Config()
  └─ 配置 NVIC
  ↓
用户代码
```

## 配置选项

### 时钟配置

```kconfig
CONFIG_STM32_HSE_ENABLE=y           # 使用外部晶振
CONFIG_STM32_HSE_VALUE=25000000     # HSE 频率
CONFIG_STM32_SYSCLK_FREQ=180000000  # 系统时钟
```

### 内存配置

```kconfig
CONFIG_STM32_FLASH_SIZE=1048576     # Flash 大小
CONFIG_STM32_SRAM_SIZE=131072       # SRAM 大小
CONFIG_STACK_SIZE=4096              # 堆栈大小
CONFIG_HEAP_SIZE=32768              # 堆大小
```

### HAL 模块

```kconfig
CONFIG_STM32_UART_ENABLE=y          # 启用 UART
CONFIG_STM32_SPI_ENABLE=y           # 启用 SPI
CONFIG_STM32_I2C_ENABLE=y           # 启用 I2C
CONFIG_STM32_TIMER_ENABLE=y         # 启用 Timer
CONFIG_STM32_ADC_ENABLE=y           # 启用 ADC
```

## 文档

- [启动指南](docs/STM32_BOOT_GUIDE.md) - 详细的启动流程和组件说明
- [架构设计](docs/ARCHITECTURE.md) - 平台架构和模块设计

## 示例应用

- [stm32_blinky](../../applications/stm32_blinky/) - LED 闪烁示例

## 依赖

### Vendor 库

- ARM CMSIS Core (`vendors/arm/CMSIS_5/`)
- ST CMSIS Device (`vendors/st/cmsis_device_*`)
- ST HAL Driver (`vendors/st/stm32*xx_hal_driver/`)

### 工具链

- ARM GCC (`arm-none-eabi-gcc`)
- ARM Clang (`armclang`)
- IAR EWARM (`iccarm`)

## 常见问题

### Q: 如何更改系统时钟频率？

修改 Kconfig 配置：
```kconfig
CONFIG_STM32_SYSCLK_FREQ=168000000  # 改为 168MHz
```

### Q: 如何添加新的外设支持？

1. 在 Kconfig 中启用对应的 HAL 模块
2. 在应用代码中使用 STM32 HAL API

### Q: 如何调试启动问题？

1. 使用调试器查看 PC 寄存器位置
2. 检查链接脚本中的内存地址
3. 在启动的不同阶段点亮 LED 指示

## 贡献

欢迎贡献代码！请参考 [贡献指南](../../CONTRIBUTING.md)。

## 许可证

MIT License - 详见 [LICENSE](../../LICENSE)
