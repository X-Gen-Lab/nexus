# HAL 移植指南

本文档指导如何将 Nexus HAL 移植到新的硬件平台。

## 目录

1. [移植概述](#1-移植概述)
2. [准备工作](#2-准备工作)
3. [创建平台目录](#3-创建平台目录)
4. [实现设备驱动](#4-实现设备驱动)
5. [配置构建系统](#5-配置构建系统)
6. [测试验证](#6-测试验证)
7. [优化调优](#7-优化调优)
8. [故障排查](#8-故障排查)

## 1. 移植概述

### 1.1 依赖项

HAL 移植需要以下依赖：

- **OSAL**: 操作系统抽象层（互斥锁、信号量等）
- **编译器**: 支持 C99 或更高标准
- **链接器**: 支持自定义段（section）
- **硬件文档**: 目标平台的数据手册和参考手册

### 1.2 工作量评估

| 任务 | 工作量 | 说明 |
|------|--------|------|
| 平台初始化 | 1-2 天 | 时钟、中断控制器等 |
| GPIO 驱动 | 1-2 天 | 基础 IO 操作 |
| UART 驱动 | 2-3 天 | 串口通信 |
| SPI 驱动 | 2-3 天 | SPI 总线 |
| I2C 驱动 | 2-3 天 | I2C 总线 |
| Timer 驱动 | 3-4 天 | 定时器和 PWM |
| ADC 驱动 | 2-3 天 | 模数转换 |
| Flash 驱动 | 2-3 天 | 内部 Flash |
| DMA 支持 | 3-5 天 | DMA 控制器 |
| 测试验证 | 3-5 天 | 单元测试和集成测试 |
| **总计** | **20-35 天** | 取决于平台复杂度 |

### 1.3 移植流程

```
1. 创建平台目录结构
   ↓
2. 实现平台初始化
   ↓
3. 实现 GPIO 驱动（最简单）
   ↓
4. 实现 UART 驱动（用于调试）
   ↓
5. 实现其他外设驱动
   ↓
6. 配置构建系统
   ↓
7. 编写测试用例
   ↓
8. 测试验证
   ↓
9. 性能优化
   ↓
10. 文档编写
```

## 2. 准备工作

### 2.1 收集硬件信息

- 处理器型号和架构
- 外设列表和地址映射
- 中断向量表
- DMA 通道配置
- 时钟树结构
- 引脚复用配置

### 2.2 准备开发环境

```bash
# 安装交叉编译工具链
sudo apt-get install gcc-arm-none-eabi

# 安装调试工具
sudo apt-get install openocd gdb-multiarch

# 克隆项目
git clone https://github.com/nexus-embedded/nexus.git
cd nexus
```

### 2.3 阅读参考实现

```bash
# 查看现有平台实现
ls platforms/
# arm_cortex_m/stm32f4/
# arm_cortex_m/stm32h7/
# native/

# 参考 STM32F4 实现
cd platforms/arm_cortex_m/stm32f4
```

## 3. 创建平台目录

### 3.1 目录结构

```
platforms/
└── <vendor>/              # 厂商名称（如 stm32、nrf、esp32）
    └── <chip_family>/     # 芯片系列（如 stm32f4、nrf52、esp32s3）
        ├── hal/           # HAL 驱动实现
        │   ├── gpio.c
        │   ├── uart.c
        │   ├── spi.c
        │   ├── i2c.c
        │   └── ...
        ├── startup/       # 启动代码
        │   ├── startup.c
        │   └── system.c
        ├── linker/        # 链接器脚本
        │   └── linker.ld
        ├── include/       # 平台特定头文件
        │   └── platform_config.h
        ├── CMakeLists.txt # 构建配置
        └── Kconfig        # 配置选项
```

### 3.2 创建基础文件

```bash
# 创建平台目录
mkdir -p platforms/myvendor/mychip/{hal,startup,linker,include}

# 创建 CMakeLists.txt
cat > platforms/myvendor/mychip/CMakeLists.txt << 'EOF'
# MyChip Platform

add_library(platform_mychip STATIC)

target_sources(platform_mychip
    PRIVATE
        startup/startup.c
        startup/system.c
)

target_include_directories(platform_mychip
    PUBLIC
        include
)

target_link_libraries(platform_mychip
    PUBLIC
        hal
)

# 链接器脚本
target_link_options(platform_mychip
    PUBLIC
        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/linker.ld
)
EOF
```

