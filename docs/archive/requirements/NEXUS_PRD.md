# Nexus 嵌入式软件开发平台

## 产品需求文档 (PRD)

| 文档信息 | |
|---------|---|
| 文档版本 | v1.0.0 |
| 创建日期 | 2026-01-12 |
| 文档状态 | 草案 |
| 作者 | Nexus Architecture Team |

---

## 目录

1. [概述](#1-概述)
2. [系统架构](#2-系统架构)
3. [功能需求](#3-功能需求)
4. [非功能需求](#4-非功能需求)
5. [工程目录结构](#5-工程目录结构)
6. [接口规范](#6-接口规范)
7. [开发规范](#7-开发规范)
8. [质量保证](#8-质量保证)
9. [工具链](#9-工具链)
10. [文档体系](#10-文档体系)
11. [版本管理](#11-版本管理)
12. [附录](#12-附录)

---

## 1. 概述

### 1.1 项目背景

随着物联网、智能设备、工业自动化等领域的快速发展，嵌入式软件开发面临以下挑战：

- **硬件碎片化**：MCU 架构多样（ARM Cortex-M、RISC-V、Xtensa 等），开发者需要针对不同平台重复开发
- **代码复用困难**：缺乏统一的抽象层，应用代码与硬件强耦合
- **开发效率低下**：工具链分散，缺乏标准化的开发流程
- **质量难以保证**：测试框架不完善，代码规范不统一

### 1.2 项目目标

Nexus 平台旨在构建一个**世界级的嵌入式软件开发平台**，实现：

| 目标 | 描述 | 量化指标 |
|------|------|----------|
| **高可移植性** | 应用代码一次编写，多平台运行 | 支持 ≥5 种 MCU 架构 |
| **高可扩展性** | 模块化设计，按需裁剪 | 最小系统 < 8KB ROM |
| **高实时性** | 满足硬实时应用需求 | 中断响应 < 1μs |
| **低功耗** | 支持多级功耗管理 | 待机功耗 < 10μA |
| **高安全性** | 端到端安全保障 | 支持安全启动、加密存储 |
| **高开发效率** | 完善的工具链和文档 | 新项目启动 < 30 分钟 |

### 1.3 目标用户

| 用户角色 | 使用场景 | 核心需求 |
|----------|----------|----------|
| **应用开发者** | 基于平台开发产品应用 | 简单易用的 API、丰富的示例 |
| **驱动开发者** | 开发新硬件驱动和组件 | 清晰的接口规范、移植指南 |
| **系统集成商** | 集成平台到产品中 | 可裁剪、可配置、文档完善 |
| **平台维护者** | 维护和扩展平台功能 | 模块化设计、完善的测试 |

### 1.4 术语定义

| 术语 | 全称 | 定义 |
|------|------|------|
| HAL | Hardware Abstraction Layer | 硬件抽象层，屏蔽硬件差异 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层，屏蔽 OS 差异 |
| BSP | Board Support Package | 板级支持包，特定开发板的配置 |
| MCU | Microcontroller Unit | 微控制器单元 |
| RTOS | Real-Time Operating System | 实时操作系统 |
| OTA | Over-The-Air | 空中升级 |

---

## 2. 系统架构

### 2.1 分层架构总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐       │
│  │   用户应用   │ │  示例程序   │ │  产品项目   │ │  测试程序   │       │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘       │
├─────────────────────────────────────────────────────────────────────────┤
│                         COMPONENTS LAYER                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 传感器驱动 │ │ 显示驱动  │ │ 协议栈   │ │  算法库  │ │ 工具组件  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
├─────────────────────────────────────────────────────────────────────────┤
│                         MIDDLEWARE LAYER                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 文件系统  │ │ 网络协议栈 │ │ 安全模块  │ │ USB协议栈 │ │ GUI框架  │     │
│  │ FAT/LFS  │ │ TCP/IP   │ │ mbedTLS  │ │ Device   │ │  LVGL   │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
├─────────────────────────────────────────────────────────────────────────┤
│                            OSAL LAYER                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │ 任务管理  │ │ 同步机制  │ │ 消息队列  │ │ 定时器   │ │ 内存管理  │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  Adapters: FreeRTOS | RT-Thread | Zephyr | Linux | Baremetal    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────┤
│                             HAL LAYER                                    │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐      │
│  │ GPIO │ │ UART │ │ SPI  │ │ I2C  │ │ ADC  │ │ PWM  │ │Timer │      │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐      │
│  │ DMA  │ │Flash │ │ RTC  │ │ WDG  │ │Crypto│ │Power │ │ IRQ  │      │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘      │
├─────────────────────────────────────────────────────────────────────────┤
│                          PLATFORM LAYER                                  │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐           │
│  │     STM32       │ │      ESP32      │ │      nRF52      │           │
│  │  F4/F7/H7/L4    │ │   S2/S3/C3      │ │   840/833/832   │           │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘           │
├─────────────────────────────────────────────────────────────────────────┤
│                            HARDWARE                                      │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐           │
│  │  Nucleo-F401RE  │ │  ESP32-DevKitC  │ │   nRF52840-DK   │           │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘           │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 层次职责定义

| 层次 | 职责 | 依赖关系 |
|------|------|----------|
| **Application** | 用户业务逻辑实现 | 依赖 Components、Middleware、OSAL |
| **Components** | 可复用功能组件（传感器、协议、算法） | 依赖 HAL、OSAL |
| **Middleware** | 通用服务（文件系统、网络、安全） | 依赖 HAL、OSAL |
| **OSAL** | 操作系统抽象，提供统一的任务、同步接口 | 依赖 HAL、底层 OS |
| **HAL** | 硬件抽象，提供统一的外设接口 | 依赖 Platform |
| **Platform** | 芯片和开发板特定代码 | 依赖 Hardware |

### 2.3 数据流架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层数据流                              │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐     │
│  │ 传感器   │───▶│ 数据处理 │───▶│ 业务逻辑 │───▶│ 输出控制 │     │
│  │ 采集任务 │    │  任务   │    │  任务   │    │  任务   │     │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘     │
│       │              │              │              │           │
│       ▼              ▼              ▼              ▼           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    消息队列 / 事件                        │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 功能需求

### 3.1 HAL 层功能需求

#### 3.1.1 GPIO 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-GPIO-001 | 支持引脚初始化（输入/输出模式） | P0 | 所有支持平台通过测试 |
| HAL-GPIO-002 | 支持电平读写操作 | P0 | 读写延迟 < 100ns |
| HAL-GPIO-003 | 支持上拉/下拉/浮空配置 | P0 | 配置后电平符合预期 |
| HAL-GPIO-004 | 支持推挽/开漏输出模式 | P1 | 输出模式可切换 |
| HAL-GPIO-005 | 支持中断配置（边沿触发） | P0 | 中断响应 < 1μs |
| HAL-GPIO-006 | 支持引脚复用功能配置 | P1 | 复用功能正确映射 |
| HAL-GPIO-007 | 支持引脚电平翻转操作 | P2 | 单指令完成翻转 |

#### 3.1.2 UART 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-UART-001 | 支持波特率配置（9600-921600） | P0 | 波特率误差 < 2% |
| HAL-UART-002 | 支持数据位/停止位/校验位配置 | P0 | 配置组合正确工作 |
| HAL-UART-003 | 支持阻塞式发送/接收 | P0 | 数据完整无丢失 |
| HAL-UART-004 | 支持非阻塞式发送/接收 | P0 | 回调正确触发 |
| HAL-UART-005 | 支持 DMA 传输模式 | P1 | CPU 占用率降低 50% |
| HAL-UART-006 | 支持硬件流控（RTS/CTS） | P2 | 流控信号正确 |
| HAL-UART-007 | 支持接收超时检测 | P1 | 超时回调正确触发 |

#### 3.1.3 SPI 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-SPI-001 | 支持主机模式 | P0 | 通信正确 |
| HAL-SPI-002 | 支持 4 种 SPI 模式（CPOL/CPHA） | P0 | 模式切换正确 |
| HAL-SPI-003 | 支持时钟频率配置 | P0 | 频率误差 < 5% |
| HAL-SPI-004 | 支持全双工传输 | P0 | 收发同步正确 |
| HAL-SPI-005 | 支持 DMA 传输 | P1 | 大数据传输效率提升 |
| HAL-SPI-006 | 支持片选信号控制 | P0 | CS 时序正确 |

#### 3.1.4 I2C 模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| HAL-I2C-001 | 支持主机模式 | P0 | 通信正确 |
| HAL-I2C-002 | 支持标准/快速/高速模式 | P0 | 速率符合规范 |
| HAL-I2C-003 | 支持 7 位/10 位地址 | P1 | 地址识别正确 |
| HAL-I2C-004 | 支持寄存器读写操作 | P0 | 读写正确 |
| HAL-I2C-005 | 支持设备扫描功能 | P2 | 检测所有在线设备 |
| HAL-I2C-006 | 支持总线错误恢复 | P1 | 错误后自动恢复 |

#### 3.1.5 其他 HAL 模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **ADC** | 单次/连续采样、多通道扫描、DMA | P0 |
| **PWM** | 频率/占空比配置、互补输出、死区 | P1 |
| **Timer** | 定时中断、输入捕获、输出比较 | P0 |
| **Flash** | 读写擦除、扇区管理、写保护 | P0 |
| **RTC** | 时间设置/读取、闹钟、备份寄存器 | P1 |
| **DMA** | 内存到外设、外设到内存、链式传输 | P1 |
| **Watchdog** | 独立/窗口看门狗、超时配置 | P0 |
| **Crypto** | AES/SHA/RSA 硬件加速 | P2 |
| **Power** | 功耗模式切换、唤醒源配置 | P1 |

### 3.2 OSAL 层功能需求

#### 3.2.1 任务管理

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| OSAL-TASK-001 | 支持任务创建/删除 | P0 | 任务正确运行/终止 |
| OSAL-TASK-002 | 支持任务优先级配置（≥32 级） | P0 | 优先级调度正确 |
| OSAL-TASK-003 | 支持任务挂起/恢复 | P0 | 状态切换正确 |
| OSAL-TASK-004 | 支持任务延时（ms/tick） | P0 | 延时精度 < 1ms |
| OSAL-TASK-005 | 支持任务栈溢出检测 | P1 | 溢出时触发回调 |
| OSAL-TASK-006 | 支持任务运行时统计 | P2 | CPU 占用率准确 |

#### 3.2.2 同步机制

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| OSAL-SYNC-001 | 支持互斥锁（Mutex） | P0 | 互斥访问正确 |
| OSAL-SYNC-002 | 支持递归互斥锁 | P1 | 递归加锁正确 |
| OSAL-SYNC-003 | 支持优先级继承 | P1 | 避免优先级反转 |
| OSAL-SYNC-004 | 支持二值信号量 | P0 | 同步正确 |
| OSAL-SYNC-005 | 支持计数信号量 | P0 | 计数正确 |
| OSAL-SYNC-006 | 支持事件标志组 | P1 | 多事件等待正确 |

#### 3.2.3 消息队列

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| OSAL-QUEUE-001 | 支持队列创建/删除 | P0 | 队列正确工作 |
| OSAL-QUEUE-002 | 支持阻塞/非阻塞发送 | P0 | 发送行为正确 |
| OSAL-QUEUE-003 | 支持阻塞/非阻塞接收 | P0 | 接收行为正确 |
| OSAL-QUEUE-004 | 支持队列头部插入 | P1 | 紧急消息优先 |
| OSAL-QUEUE-005 | 支持 ISR 中发送消息 | P0 | ISR 安全 |

#### 3.2.4 定时器

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| OSAL-TIMER-001 | 支持软件定时器创建 | P0 | 定时器正确触发 |
| OSAL-TIMER-002 | 支持单次/周期模式 | P0 | 模式切换正确 |
| OSAL-TIMER-003 | 支持定时器启动/停止 | P0 | 控制正确 |
| OSAL-TIMER-004 | 支持定时器周期修改 | P1 | 动态修改生效 |

#### 3.2.5 内存管理

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| OSAL-MEM-001 | 支持动态内存分配/释放 | P0 | 分配释放正确 |
| OSAL-MEM-002 | 支持内存池管理 | P1 | 固定块分配高效 |
| OSAL-MEM-003 | 支持内存使用统计 | P1 | 统计数据准确 |
| OSAL-MEM-004 | 支持内存泄漏检测 | P2 | 检测泄漏位置 |

### 3.3 中间件层功能需求

#### 3.3.1 文件系统

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| MW-FS-001 | 支持 FAT 文件系统 | P1 | SD 卡读写正确 |
| MW-FS-002 | 支持 LittleFS 文件系统 | P0 | Flash 读写正确 |
| MW-FS-003 | 支持文件打开/关闭/读/写 | P0 | 基本操作正确 |
| MW-FS-004 | 支持目录创建/删除/遍历 | P1 | 目录操作正确 |
| MW-FS-005 | 支持多挂载点 | P2 | 多设备同时访问 |
| MW-FS-006 | 支持掉电保护 | P1 | 数据不丢失 |

#### 3.3.2 网络协议栈

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| MW-NET-001 | 支持 TCP/IP 协议栈（lwIP） | P1 | 网络通信正确 |
| MW-NET-002 | 支持 Socket API | P1 | API 兼容标准 |
| MW-NET-003 | 支持 DHCP 客户端 | P1 | 自动获取 IP |
| MW-NET-004 | 支持 DNS 解析 | P1 | 域名解析正确 |
| MW-NET-005 | 支持 MQTT 客户端 | P1 | 云平台对接 |
| MW-NET-006 | 支持 HTTP 客户端 | P2 | REST API 调用 |
| MW-NET-007 | 支持 TLS/SSL 加密 | P1 | 安全通信 |

#### 3.3.3 安全模块

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| MW-SEC-001 | 支持安全启动 | P1 | 验证固件签名 |
| MW-SEC-002 | 支持 AES 加密（128/256） | P0 | 加解密正确 |
| MW-SEC-003 | 支持 SHA 哈希（256/512） | P0 | 哈希值正确 |
| MW-SEC-004 | 支持 RSA/ECC 非对称加密 | P1 | 签名验证正确 |
| MW-SEC-005 | 支持安全存储（密钥保护） | P1 | 密钥不可读取 |
| MW-SEC-006 | 支持随机数生成（TRNG） | P0 | 随机性符合标准 |

#### 3.3.4 OTA 升级

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| MW-OTA-001 | 支持固件下载 | P1 | 下载完整无误 |
| MW-OTA-002 | 支持固件验证（签名/CRC） | P0 | 验证失败拒绝升级 |
| MW-OTA-003 | 支持双分区切换 | P1 | 升级失败可回滚 |
| MW-OTA-004 | 支持差分升级 | P2 | 减少下载量 |
| MW-OTA-005 | 支持升级进度回调 | P1 | 进度准确 |
| MW-OTA-006 | 支持断点续传 | P2 | 网络中断可恢复 |

### 3.4 组件层功能需求

#### 3.4.1 传感器驱动组件

| 组件名称 | 功能描述 | 接口类型 | 优先级 |
|----------|----------|----------|--------|
| BME280 | 温度/湿度/气压传感器 | I2C/SPI | P1 |
| MPU6050 | 6 轴陀螺仪/加速度计 | I2C | P1 |
| MAX30102 | 血氧/心率传感器 | I2C | P2 |
| BH1750 | 光照强度传感器 | I2C | P2 |
| DS18B20 | 数字温度传感器 | 1-Wire | P2 |

#### 3.4.2 显示驱动组件

| 组件名称 | 功能描述 | 接口类型 | 优先级 |
|----------|----------|----------|--------|
| SSD1306 | 128x64 OLED 显示屏 | I2C/SPI | P1 |
| ST7789 | 240x240 TFT 彩屏 | SPI | P2 |
| LCD1602 | 16x2 字符 LCD | GPIO/I2C | P2 |
| ILI9341 | 320x240 TFT 彩屏 | SPI | P2 |

#### 3.4.3 协议组件

| 组件名称 | 功能描述 | 优先级 |
|----------|----------|--------|
| Modbus RTU | 工业串口通信协议 | P1 |
| Modbus TCP | 工业以太网通信协议 | P2 |
| CANopen | CAN 总线应用层协议 | P2 |
| MQTT | 物联网消息协议 | P1 |

#### 3.4.4 算法组件

| 组件名称 | 功能描述 | 优先级 |
|----------|----------|--------|
| PID 控制器 | 比例-积分-微分控制 | P1 |
| 卡尔曼滤波 | 状态估计滤波算法 | P2 |
| FFT | 快速傅里叶变换 | P2 |
| CRC | 循环冗余校验（8/16/32） | P0 |

#### 3.4.5 工具组件

| 组件名称 | 功能描述 | 优先级 |
|----------|----------|--------|
| 环形缓冲区 | 高效的 FIFO 数据结构 | P0 |
| 命令行接口 | 调试用 Shell 命令行 | P1 |
| 日志系统 | 分级日志输出 | P0 |
| 状态机 | 有限状态机框架 | P1 |

---

## 4. 非功能需求

### 4.1 性能需求

| 需求ID | 需求描述 | 指标 | 测试方法 |
|--------|----------|------|----------|
| NFR-PERF-001 | 中断响应时间 | < 1μs | 示波器测量 |
| NFR-PERF-002 | 任务上下文切换时间 | < 5μs | 性能测试 |
| NFR-PERF-003 | HAL API 调用开销 | < 100 cycles | 基准测试 |
| NFR-PERF-004 | 系统启动时间 | < 100ms | 时间戳测量 |
| NFR-PERF-005 | 最大任务数量 | ≥ 32 | 压力测试 |

### 4.2 资源需求

| 需求ID | 需求描述 | 指标 | 备注 |
|--------|----------|------|------|
| NFR-RES-001 | 最小 ROM 占用（HAL+OSAL） | < 8KB | 最小配置 |
| NFR-RES-002 | 最小 RAM 占用（HAL+OSAL） | < 2KB | 最小配置 |
| NFR-RES-003 | 典型 ROM 占用（含中间件） | < 64KB | 典型配置 |
| NFR-RES-004 | 典型 RAM 占用（含中间件） | < 16KB | 典型配置 |
| NFR-RES-005 | 单任务最小栈大小 | 256 bytes | 简单任务 |

### 4.3 可靠性需求

| 需求ID | 需求描述 | 指标 | 验证方法 |
|--------|----------|------|----------|
| NFR-REL-001 | 系统连续运行时间 | > 30 天 | 长时间测试 |
| NFR-REL-002 | 内存泄漏 | 0 | 内存分析工具 |
| NFR-REL-003 | 看门狗覆盖 | 100% 关键路径 | 代码审查 |
| NFR-REL-004 | 错误恢复能力 | 自动恢复 | 故障注入测试 |
| NFR-REL-005 | 掉电数据保护 | 关键数据不丢失 | 掉电测试 |

### 4.4 可移植性需求

| 需求ID | 需求描述 | 指标 | 验证方法 |
|--------|----------|------|----------|
| NFR-PORT-001 | 支持 MCU 架构数量 | ≥ 5 种 | 平台测试 |
| NFR-PORT-002 | 支持 RTOS 数量 | ≥ 4 种 | 适配测试 |
| NFR-PORT-003 | 新平台移植时间 | < 1 周 | 移植实践 |
| NFR-PORT-004 | 应用代码修改量 | 0 | 跨平台编译 |
| NFR-PORT-005 | 编译器支持 | GCC/Clang/IAR/Keil | 编译测试 |

### 4.5 可维护性需求

| 需求ID | 需求描述 | 指标 | 验证方法 |
|--------|----------|------|----------|
| NFR-MAINT-001 | 代码圈复杂度 | < 15 | 静态分析 |
| NFR-MAINT-002 | 函数行数 | < 100 行 | 代码审查 |
| NFR-MAINT-003 | 模块耦合度 | 低耦合 | 依赖分析 |
| NFR-MAINT-004 | 注释覆盖率 | > 30% | 文档工具 |
| NFR-MAINT-005 | API 文档完整性 | 100% | Doxygen |

### 4.6 安全性需求

| 需求ID | 需求描述 | 指标 | 验证方法 |
|--------|----------|------|----------|
| NFR-SEC-001 | 安全启动支持 | 必须 | 安全测试 |
| NFR-SEC-002 | 固件加密存储 | AES-256 | 加密验证 |
| NFR-SEC-003 | 通信加密 | TLS 1.2+ | 协议分析 |
| NFR-SEC-004 | 密钥安全存储 | 硬件保护 | 安全审计 |
| NFR-SEC-005 | 漏洞扫描 | 0 高危漏洞 | 安全扫描 |

### 4.7 功耗需求

| 需求ID | 需求描述 | 指标 | 测试方法 |
|--------|----------|------|----------|
| NFR-PWR-001 | 深度睡眠电流 | < 10μA | 电流测量 |
| NFR-PWR-002 | 待机电流 | < 100μA | 电流测量 |
| NFR-PWR-003 | 唤醒时间 | < 1ms | 示波器测量 |
| NFR-PWR-004 | 功耗模式数量 | ≥ 4 级 | 功能测试 |
| NFR-PWR-005 | 动态功耗管理 | 支持 DVFS | 功能测试 |

---

## 5. 工程目录结构

### 5.1 顶层目录结构

```
nexus/
├── .github/                    # GitHub 配置和工作流
│   ├── workflows/              # CI/CD 工作流
│   │   ├── build.yml           # 构建工作流
│   │   ├── test.yml            # 测试工作流
│   │   └── release.yml         # 发布工作流
│   └── ISSUE_TEMPLATE/         # Issue 模板
│
├── .vscode/                    # VSCode 配置
│   ├── extensions.json         # 推荐扩展
│   ├── settings.json           # 工作区设置
│   ├── tasks.json              # 构建任务
│   └── launch.json             # 调试配置
│
├── applications/               # 应用程序
│   ├── demos/                  # 示例应用
│   └── projects/               # 实际项目
│
├── boards/                     # 板级支持包 (BSP)
│   ├── stm32/                  # STM32 系列开发板
│   ├── esp32/                  # ESP32 系列开发板
│   ├── nrf/                    # Nordic 系列开发板
│   └── rp2040/                 # RP2040 开发板
│
├── components/                 # 可复用组件
│   ├── core/                   # 核心组件（组件管理器）
│   ├── drivers/                # 驱动组件（传感器、显示等）
│   ├── protocols/              # 协议组件（Modbus、MQTT 等）
│   ├── algorithms/             # 算法组件（PID、滤波等）
│   └── utilities/              # 工具组件（日志、CRC 等）
│
├── configs/                    # 平台配置
│   ├── platform/               # 平台级配置模板
│   ├── mcu/                    # MCU 系列配置
│   ├── toolchains/             # 工具链配置
│   └── templates/              # 配置模板
│
├── docs/                       # 文档
│   ├── getting_started/        # 入门指南
│   ├── api_reference/          # API 参考手册
│   ├── guides/                 # 开发指南
│   ├── design/                 # 设计文档
│   ├── tutorials/              # 教程
│   └── images/                 # 文档图片
│
├── drivers/                    # 芯片外设驱动
│   ├── stm32/                  # STM32 驱动
│   ├── esp32/                  # ESP32 驱动
│   ├── nrf52/                  # nRF52 驱动
│   └── third_party/            # 第三方驱动（CMSIS 等）
│
├── hal/                        # 硬件抽象层
│   ├── include/hal/            # HAL 公共头文件
│   ├── src/                    # HAL 通用实现
│   └── platforms/              # 平台特定实现
│
├── kernel/                     # 内核（可选）
│   ├── rtos/                   # 自研 RTOS 实现
│   └── scheduler/              # 轻量级调度器
│
├── middleware/                 # 中间件
│   ├── filesystem/             # 文件系统（FAT、LittleFS）
│   ├── network/                # 网络协议栈（lwIP、MQTT）
│   ├── security/               # 安全模块（mbedTLS）
│   ├── usb/                    # USB 协议栈
│   └── gui/                    # 图形界面（LVGL）
│
├── osal/                       # 操作系统抽象层
│   ├── include/osal/           # OSAL 公共头文件
│   ├── src/                    # OSAL 通用实现
│   └── adapters/               # OS 适配器（FreeRTOS、RT-Thread 等）
│
├── platforms/                  # 平台支持
│   ├── stm32/                  # STM32 平台（启动、时钟）
│   ├── esp32/                  # ESP32 平台
│   ├── nrf52/                  # nRF52 平台
│   └── common/                 # 通用平台代码
│
├── projects/                   # 项目目录
│   ├── template/               # 项目模板
│   └── [project_name]/         # 具体项目
│
├── scripts/                    # 构建和工具脚本
│   ├── build/                  # 构建脚本（CMake、Make）
│   ├── tools/                  # 工具脚本（代码生成、烧录）
│   ├── ci/                     # CI/CD 脚本
│   └── utilities/              # 实用脚本
│
├── tests/                      # 测试代码
│   ├── unit/                   # 单元测试
│   ├── integration/            # 集成测试
│   ├── system/                 # 系统测试
│   ├── hardware/               # 硬件测试
│   ├── mocks/                  # Mock 对象
│   └── test_runner/            # 测试运行器
│
├── tools/                      # 开发工具
│   ├── nexus-studio/           # IDE 扩展
│   ├── config-tool/            # 图形化配置工具
│   ├── trace-viewer/           # 跟踪查看器
│   └── memory-analyzer/        # 内存分析器
│
├── third_party/                # 第三方库
│   ├── cmsis/                  # CMSIS
│   ├── freertos/               # FreeRTOS
│   ├── lwip/                   # lwIP
│   ├── mbedtls/                # mbedTLS
│   ├── lvgl/                   # LVGL
│   └── unity/                  # Unity 测试框架
│
├── vendor/                     # 厂商特定文件
│   ├── st/                     # ST Microelectronics
│   ├── espressif/              # Espressif
│   ├── nordic/                 # Nordic
│   └── arm/                    # ARM
│
├── CMakeLists.txt              # 根 CMakeLists
├── Makefile                    # 顶层 Makefile
├── pyproject.toml              # Python 项目配置
├── requirements.txt            # Python 依赖
├── README.md                   # 项目说明
├── LICENSE                     # 许可证
├── CONTRIBUTING.md             # 贡献指南
├── CODE_OF_CONDUCT.md          # 行为准则
└── CHANGELOG.md                # 变更日志
```

### 5.2 HAL 目录详细结构

```
hal/
├── include/hal/                    # 公共头文件
│   ├── hal_types.h                 # 基础类型定义
│   ├── hal_error.h                 # 错误码定义
│   ├── hal_config.h                # 配置接口
│   ├── hal_core.h                  # HAL 核心管理
│   ├── hal_gpio.h                  # GPIO 接口
│   ├── hal_uart.h                  # UART 接口
│   ├── hal_spi.h                   # SPI 接口
│   ├── hal_i2c.h                   # I2C 接口
│   ├── hal_adc.h                   # ADC 接口
│   ├── hal_pwm.h                   # PWM 接口
│   ├── hal_timer.h                 # Timer 接口
│   ├── hal_flash.h                 # Flash 接口
│   ├── hal_rtc.h                   # RTC 接口
│   ├── hal_dma.h                   # DMA 接口
│   ├── hal_watchdog.h              # 看门狗接口
│   ├── hal_crypto.h                # 加密接口
│   ├── hal_power.h                 # 电源管理接口
│   ├── hal_clock.h                 # 时钟管理接口
│   ├── hal_interrupt.h             # 中断管理接口
│   ├── hal_test.h                  # 测试接口
│   └── hal_version.h               # 版本信息
│
├── src/                            # 通用实现
│   ├── core/
│   │   ├── hal_manager.c           # HAL 管理器
│   │   ├── hal_error.c             # 错误处理
│   │   ├── hal_config.c            # 配置管理
│   │   └── hal_stats.c             # 统计信息
│   ├── utilities/
│   │   ├── hal_assert.c            # 断言实现
│   │   ├── hal_log.c               # 日志系统
│   │   └── hal_trace.c             # 跟踪系统
│   └── common/
│       ├── hal_gpio_common.c       # GPIO 通用实现
│       ├── hal_uart_common.c       # UART 通用实现
│       └── hal_common_internal.h   # 内部头文件
│
└── platforms/                      # 平台特定实现
    ├── stm32/
    │   ├── include/
    │   │   └── hal_stm32.h
    │   ├── src/
    │   │   ├── hal_stm32_core.c
    │   │   ├── hal_stm32_gpio.c
    │   │   ├── hal_stm32_uart.c
    │   │   ├── hal_stm32_spi.c
    │   │   ├── hal_stm32_i2c.c
    │   │   ├── hal_stm32_adc.c
    │   │   ├── hal_stm32_pwm.c
    │   │   ├── hal_stm32_timer.c
    │   │   ├── hal_stm32_flash.c
    │   │   └── hal_stm32_dma.c
    │   └── config/
    │       └── hal_stm32_config.h
    ├── esp32/
    │   └── ...
    ├── nrf52/
    │   └── ...
    ├── linux/                      # Linux 模拟平台
    │   └── ...
    └── template/                   # 移植模板
        ├── README.md
        ├── hal_template_core.c
        └── hal_template_gpio.c
```

### 5.3 OSAL 目录详细结构

```
osal/
├── include/osal/                   # 公共头文件
│   ├── osal_types.h                # 基础类型
│   ├── osal_error.h                # 错误码
│   ├── osal_config.h               # 配置
│   ├── osal_core.h                 # 核心接口
│   ├── osal_task.h                 # 任务管理
│   ├── osal_mutex.h                # 互斥锁
│   ├── osal_semaphore.h            # 信号量
│   ├── osal_queue.h                # 消息队列
│   ├── osal_event.h                # 事件标志
│   ├── osal_timer.h                # 定时器
│   ├── osal_memory.h               # 内存管理
│   ├── osal_workqueue.h            # 工作队列
│   ├── osal_debug.h                # 调试接口
│   └── osal_version.h              # 版本信息
│
├── src/                            # 通用实现
│   ├── core/
│   │   ├── osal_manager.c          # OSAL 管理器
│   │   ├── osal_object.c           # 对象管理
│   │   └── osal_stats.c            # 统计信息
│   ├── utilities/
│   │   ├── osal_assert.c
│   │   └── osal_log.c
│   └── common/
│       ├── osal_task_common.c
│       └── osal_queue_common.c
│
└── adapters/                       # 适配器实现
    ├── freertos/
    │   ├── include/
    │   │   └── osal_freertos.h
    │   ├── src/
    │   │   ├── osal_freertos_core.c
    │   │   ├── osal_freertos_task.c
    │   │   ├── osal_freertos_mutex.c
    │   │   ├── osal_freertos_semaphore.c
    │   │   ├── osal_freertos_queue.c
    │   │   ├── osal_freertos_event.c
    │   │   ├── osal_freertos_timer.c
    │   │   └── osal_freertos_memory.c
    │   └── config/
    │       └── osal_freertos_config.h
    ├── rtthread/
    │   └── ...
    ├── zephyr/
    │   └── ...
    ├── linux/
    │   └── ...
    └── baremetal/
        ├── include/
        │   └── osal_baremetal.h
        ├── src/
        │   ├── osal_baremetal_core.c
        │   ├── osal_baremetal_task.c
        │   ├── osal_baremetal_timer.c
        │   └── osal_baremetal_memory.c
        └── scheduler/
            ├── cooperative.c       # 协作式调度
            └── preemptive.c        # 抢占式调度
```

### 5.4 组件目录详细结构

```
components/
├── core/                           # 核心组件
│   ├── include/components/
│   │   ├── component_manager.h
│   │   ├── component_types.h
│   │   └── dependency_graph.h
│   ├── src/
│   │   ├── component_manager.c
│   │   ├── dependency_resolver.c
│   │   └── component_registry.c
│   └── CMakeLists.txt
│
├── drivers/                        # 驱动组件
│   ├── sensors/
│   │   ├── bme280/
│   │   │   ├── include/sensors/bme280.h
│   │   │   ├── src/
│   │   │   │   ├── bme280.c
│   │   │   │   └── bme280_i2c.c
│   │   │   ├── config/bme280_config.h
│   │   │   ├── examples/bme280_example.c
│   │   │   ├── tests/test_bme280.c
│   │   │   └── README.md
│   │   ├── mpu6050/
│   │   └── template/               # 传感器模板
│   ├── displays/
│   │   ├── oled_ssd1306/
│   │   └── lcd_1602/
│   └── peripherals/
│       ├── flash_w25q/
│       └── rtc_ds3231/
│
├── protocols/                      # 协议组件
│   ├── modbus/
│   │   ├── include/protocols/
│   │   │   ├── modbus.h
│   │   │   ├── modbus_rtu.h
│   │   │   └── modbus_tcp.h
│   │   ├── src/
│   │   │   ├── modbus_core.c
│   │   │   ├── modbus_rtu.c
│   │   │   └── modbus_tcp.c
│   │   └── examples/
│   ├── mqtt_client/
│   └── canopen/
│
├── algorithms/                     # 算法组件
│   ├── pid_controller/
│   │   ├── include/algorithms/pid.h
│   │   ├── src/
│   │   │   ├── pid.c
│   │   │   └── pid_autotune.c
│   │   └── examples/pid_example.c
│   ├── kalman_filter/
│   └── fft/
│
└── utilities/                      # 工具组件
    ├── circular_buffer/
    ├── command_line/
    ├── log_system/
    └── crc/
```

---

## 6. 接口规范

### 6.1 HAL 接口设计原则

#### 6.1.1 接口设计模式

采用**函数指针表**模式实现硬件抽象：

```c
/**
 * @brief HAL 接口结构体模式
 * 
 * 每个 HAL 模块定义一个接口结构体，包含该模块所有操作的函数指针。
 * 平台实现时填充这些函数指针，上层通过统一接口调用。
 */
typedef struct {
    hal_status_t (*init)(const xxx_config_t *config);
    hal_status_t (*deinit)(void);
    hal_status_t (*read)(xxx_data_t *data);
    hal_status_t (*write)(const xxx_data_t *data);
    // ... 其他操作
} xxx_interface_t;

/**
 * @brief 获取接口实例
 * 
 * 每个平台实现此函数，返回对应的接口实例。
 */
const xxx_interface_t* hal_xxx_get_interface(void);
```

#### 6.1.2 GPIO 接口规范

```c
/**
 * @file hal_gpio.h
 * @brief GPIO 硬件抽象层接口
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                              类型定义                                       */
/*============================================================================*/

/** @brief GPIO 端口类型 */
typedef uint8_t hal_gpio_port_t;

/** @brief GPIO 引脚类型 */
typedef uint8_t hal_gpio_pin_t;

/** @brief GPIO 方向 */
typedef enum {
    HAL_GPIO_DIR_INPUT  = 0,    /**< 输入模式 */
    HAL_GPIO_DIR_OUTPUT = 1     /**< 输出模式 */
} hal_gpio_dir_t;

/** @brief GPIO 电平 */
typedef enum {
    HAL_GPIO_LEVEL_LOW  = 0,    /**< 低电平 */
    HAL_GPIO_LEVEL_HIGH = 1     /**< 高电平 */
} hal_gpio_level_t;

/** @brief GPIO 上下拉配置 */
typedef enum {
    HAL_GPIO_PULL_NONE = 0,     /**< 无上下拉 */
    HAL_GPIO_PULL_UP   = 1,     /**< 上拉 */
    HAL_GPIO_PULL_DOWN = 2      /**< 下拉 */
} hal_gpio_pull_t;

/** @brief GPIO 输出模式 */
typedef enum {
    HAL_GPIO_OUTPUT_PP = 0,     /**< 推挽输出 */
    HAL_GPIO_OUTPUT_OD = 1      /**< 开漏输出 */
} hal_gpio_output_mode_t;

/** @brief GPIO 中断触发模式 */
typedef enum {
    HAL_GPIO_IRQ_NONE         = 0,  /**< 无中断 */
    HAL_GPIO_IRQ_RISING       = 1,  /**< 上升沿触发 */
    HAL_GPIO_IRQ_FALLING      = 2,  /**< 下降沿触发 */
    HAL_GPIO_IRQ_BOTH_EDGE    = 3,  /**< 双边沿触发 */
    HAL_GPIO_IRQ_HIGH_LEVEL   = 4,  /**< 高电平触发 */
    HAL_GPIO_IRQ_LOW_LEVEL    = 5   /**< 低电平触发 */
} hal_gpio_irq_mode_t;

/** @brief GPIO 配置结构体 */
typedef struct {
    hal_gpio_dir_t         direction;      /**< 方向 */
    hal_gpio_pull_t        pull;           /**< 上下拉 */
    hal_gpio_output_mode_t output_mode;    /**< 输出模式 */
    hal_gpio_level_t       init_level;     /**< 初始电平（输出模式） */
} hal_gpio_config_t;

/** @brief GPIO 中断回调函数类型 */
typedef void (*hal_gpio_irq_callback_t)(hal_gpio_port_t port, 
                                        hal_gpio_pin_t pin, 
                                        void *user_data);

/*============================================================================*/
/*                              接口定义                                       */
/*============================================================================*/

/** @brief GPIO 操作接口 */
typedef struct {
    /**
     * @brief 初始化 GPIO 引脚
     * @param port   端口号
     * @param pin    引脚号
     * @param config 配置参数
     * @return HAL_OK 成功，其他值表示错误
     */
    hal_status_t (*init)(hal_gpio_port_t port, 
                         hal_gpio_pin_t pin, 
                         const hal_gpio_config_t *config);
    
    /**
     * @brief 反初始化 GPIO 引脚
     */
    hal_status_t (*deinit)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    
    /**
     * @brief 设置引脚电平
     */
    hal_status_t (*write)(hal_gpio_port_t port, 
                          hal_gpio_pin_t pin, 
                          hal_gpio_level_t level);
    
    /**
     * @brief 读取引脚电平
     */
    hal_status_t (*read)(hal_gpio_port_t port, 
                         hal_gpio_pin_t pin, 
                         hal_gpio_level_t *level);
    
    /**
     * @brief 翻转引脚电平
     */
    hal_status_t (*toggle)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    
    /**
     * @brief 配置中断
     */
    hal_status_t (*irq_config)(hal_gpio_port_t port, 
                               hal_gpio_pin_t pin,
                               hal_gpio_irq_mode_t mode,
                               hal_gpio_irq_callback_t callback,
                               void *user_data);
    
    /**
     * @brief 使能中断
     */
    hal_status_t (*irq_enable)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    
    /**
     * @brief 禁用中断
     */
    hal_status_t (*irq_disable)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    
} hal_gpio_interface_t;

/*============================================================================*/
/*                              API 函数                                       */
/*============================================================================*/

/**
 * @brief 获取 GPIO 接口实例
 * @return GPIO 接口指针，失败返回 NULL
 */
const hal_gpio_interface_t* hal_gpio_get_interface(void);

/* 便捷宏定义 */
#define HAL_GPIO_INIT(port, pin, cfg)    \
    hal_gpio_get_interface()->init(port, pin, cfg)

#define HAL_GPIO_WRITE(port, pin, level) \
    hal_gpio_get_interface()->write(port, pin, level)

#define HAL_GPIO_READ(port, pin, level)  \
    hal_gpio_get_interface()->read(port, pin, level)

#define HAL_GPIO_TOGGLE(port, pin)       \
    hal_gpio_get_interface()->toggle(port, pin)

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
```

### 6.2 OSAL 接口规范

#### 6.2.1 任务管理接口

```c
/**
 * @file osal_task.h
 * @brief OSAL 任务管理接口
 */

#ifndef OSAL_TASK_H
#define OSAL_TASK_H

#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                              类型定义                                       */
/*============================================================================*/

/** @brief 任务句柄 */
typedef void* osal_task_t;

/** @brief 任务优先级（0 最低，数值越大优先级越高） */
typedef uint8_t osal_task_priority_t;

/** @brief 任务状态 */
typedef enum {
    OSAL_TASK_STATE_READY,      /**< 就绪 */
    OSAL_TASK_STATE_RUNNING,    /**< 运行中 */
    OSAL_TASK_STATE_BLOCKED,    /**< 阻塞 */
    OSAL_TASK_STATE_SUSPENDED,  /**< 挂起 */
    OSAL_TASK_STATE_DELETED     /**< 已删除 */
} osal_task_state_t;

/** @brief 任务函数类型 */
typedef void (*osal_task_func_t)(void *arg);

/** @brief 任务配置 */
typedef struct {
    const char          *name;          /**< 任务名称 */
    osal_task_func_t     entry;         /**< 入口函数 */
    void                *arg;           /**< 入口参数 */
    osal_task_priority_t priority;      /**< 优先级 */
    size_t               stack_size;    /**< 栈大小（字节） */
    void                *stack_buffer;  /**< 栈缓冲区（NULL 则动态分配） */
} osal_task_config_t;

/** @brief 任务信息 */
typedef struct {
    const char          *name;
    osal_task_state_t    state;
    osal_task_priority_t priority;
    size_t               stack_size;
    size_t               stack_used;    /**< 栈使用量 */
    uint32_t             runtime_ms;    /**< 运行时间 */
} osal_task_info_t;

/*============================================================================*/
/*                              API 函数                                       */
/*============================================================================*/

/**
 * @brief 创建任务
 * @param config 任务配置
 * @param task   输出任务句柄
 * @return OSAL_OK 成功
 */
osal_status_t osal_task_create(const osal_task_config_t *config, 
                               osal_task_t *task);

/**
 * @brief 删除任务
 * @param task 任务句柄（NULL 删除当前任务）
 */
osal_status_t osal_task_delete(osal_task_t task);

/**
 * @brief 挂起任务
 */
osal_status_t osal_task_suspend(osal_task_t task);

/**
 * @brief 恢复任务
 */
osal_status_t osal_task_resume(osal_task_t task);

/**
 * @brief 设置任务优先级
 */
osal_status_t osal_task_set_priority(osal_task_t task, 
                                     osal_task_priority_t priority);

/**
 * @brief 获取当前任务句柄
 */
osal_task_t osal_task_get_current(void);

/**
 * @brief 获取任务信息
 */
osal_status_t osal_task_get_info(osal_task_t task, osal_task_info_t *info);

/**
 * @brief 任务延时（毫秒）
 */
void osal_task_delay_ms(uint32_t ms);

/**
 * @brief 任务延时到指定时间点
 */
void osal_task_delay_until(uint32_t *prev_wake_time, uint32_t increment_ms);

/**
 * @brief 主动让出 CPU
 */
void osal_task_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TASK_H */
```

### 6.3 错误码规范

```c
/**
 * @file hal_error.h / osal_error.h
 * @brief 统一错误码定义
 */

/** @brief 通用状态码 */
typedef enum {
    /* 成功 */
    HAL_OK                  = 0,    /**< 操作成功 */
    
    /* 通用错误 (1-99) */
    HAL_ERROR               = 1,    /**< 通用错误 */
    HAL_ERROR_INVALID_PARAM = 2,    /**< 无效参数 */
    HAL_ERROR_NULL_POINTER  = 3,    /**< 空指针 */
    HAL_ERROR_NOT_INIT      = 4,    /**< 未初始化 */
    HAL_ERROR_ALREADY_INIT  = 5,    /**< 已初始化 */
    HAL_ERROR_NOT_SUPPORTED = 6,    /**< 不支持 */
    HAL_ERROR_NOT_FOUND     = 7,    /**< 未找到 */
    
    /* 资源错误 (100-199) */
    HAL_ERROR_NO_MEMORY     = 100,  /**< 内存不足 */
    HAL_ERROR_NO_RESOURCE   = 101,  /**< 资源不足 */
    HAL_ERROR_BUSY          = 102,  /**< 资源忙 */
    HAL_ERROR_LOCKED        = 103,  /**< 资源被锁定 */
    
    /* 超时错误 (200-299) */
    HAL_ERROR_TIMEOUT       = 200,  /**< 操作超时 */
    
    /* IO 错误 (300-399) */
    HAL_ERROR_IO            = 300,  /**< IO 错误 */
    HAL_ERROR_READ          = 301,  /**< 读取错误 */
    HAL_ERROR_WRITE         = 302,  /**< 写入错误 */
    HAL_ERROR_OVERFLOW      = 303,  /**< 溢出 */
    HAL_ERROR_UNDERFLOW     = 304,  /**< 下溢 */
    
    /* 通信错误 (400-499) */
    HAL_ERROR_COMM          = 400,  /**< 通信错误 */
    HAL_ERROR_NACK          = 401,  /**< 无应答 */
    HAL_ERROR_BUS           = 402,  /**< 总线错误 */
    HAL_ERROR_ARBITRATION   = 403,  /**< 仲裁丢失 */
    
    /* 权限错误 (500-599) */
    HAL_ERROR_PERMISSION    = 500,  /**< 权限错误 */
    
} hal_status_t;

/* OSAL 使用相同的错误码体系 */
typedef hal_status_t osal_status_t;
#define OSAL_OK HAL_OK
```

---

## 7. 开发规范

### 7.1 代码规范

#### 7.1.1 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 文件名 | 小写 + 下划线 | `hal_gpio.c`, `osal_task.h` |
| 函数名 | 小写 + 下划线 | `hal_gpio_init()`, `osal_task_create()` |
| 类型名 | 小写 + 下划线 + `_t` | `hal_gpio_config_t`, `osal_task_t` |
| 枚举值 | 大写 + 下划线 | `HAL_GPIO_DIR_INPUT`, `OSAL_OK` |
| 宏定义 | 大写 + 下划线 | `HAL_GPIO_PORT_A`, `OSAL_WAIT_FOREVER` |
| 全局变量 | `g_` 前缀 + 小写下划线 | `g_hal_initialized` |
| 静态变量 | `s_` 前缀 + 小写下划线 | `s_gpio_callbacks` |
| 局部变量 | 小写 + 下划线 | `pin_level`, `task_handle` |
| 常量 | `k` 前缀 + 驼峰 或 大写下划线 | `kMaxRetryCount`, `MAX_RETRY_COUNT` |

#### 7.1.2 模块前缀规范

| 模块 | 前缀 | 示例 |
|------|------|------|
| HAL 核心 | `hal_` | `hal_init()` |
| HAL GPIO | `hal_gpio_` | `hal_gpio_init()` |
| HAL UART | `hal_uart_` | `hal_uart_write()` |
| OSAL 核心 | `osal_` | `osal_init()` |
| OSAL 任务 | `osal_task_` | `osal_task_create()` |
| OSAL 互斥锁 | `osal_mutex_` | `osal_mutex_lock()` |
| 组件 | `组件名_` | `bme280_read()` |

#### 7.1.3 代码格式规范

```c
/**
 * @brief 代码格式示例
 */

/* 1. 头文件保护 */
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

/* 2. 包含头文件顺序：系统头文件 -> 第三方头文件 -> 项目头文件 */
#include <stdint.h>
#include <stdbool.h>

#include "hal_types.h"
#include "hal_gpio.h"

/* 3. 宏定义 */
#define MAX_BUFFER_SIZE     256
#define MIN(a, b)           ((a) < (b) ? (a) : (b))

/* 4. 类型定义 */
typedef struct {
    uint32_t field1;        /**< 字段1说明 */
    uint8_t  field2;        /**< 字段2说明 */
} module_config_t;

/* 5. 函数声明 */
hal_status_t module_init(const module_config_t *config);

/* 6. 函数实现 */
hal_status_t module_init(const module_config_t *config)
{
    /* 参数检查 */
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    
    /* 局部变量声明 */
    hal_status_t status = HAL_OK;
    uint32_t retry_count = 0;
    
    /* 主逻辑 */
    while (retry_count < MAX_RETRY_COUNT) {
        status = do_something();
        if (status == HAL_OK) {
            break;
        }
        retry_count++;
    }
    
    /* 返回结果 */
    return status;
}

#endif /* MODULE_NAME_H */
```

#### 7.1.4 注释规范

```c
/**
 * @file hal_gpio.c
 * @brief GPIO 硬件抽象层实现
 * @version 1.0.0
 * @date 2026-01-12
 * 
 * @copyright Copyright (c) 2026 Nexus Team
 * 
 * @details
 * 本文件实现了 GPIO 的硬件抽象层接口，支持以下功能：
 * - 引脚初始化和配置
 * - 电平读写操作
 * - 中断配置和处理
 */

/**
 * @brief 初始化 GPIO 引脚
 * 
 * 根据配置参数初始化指定的 GPIO 引脚，包括方向、上下拉、输出模式等。
 * 
 * @param[in]  port   GPIO 端口号 (0-7)
 * @param[in]  pin    GPIO 引脚号 (0-15)
 * @param[in]  config 配置参数指针，不能为 NULL
 * 
 * @return 操作状态
 * @retval HAL_OK              初始化成功
 * @retval HAL_ERROR_INVALID_PARAM 参数无效
 * @retval HAL_ERROR_NOT_SUPPORTED 不支持的配置
 * 
 * @note 调用此函数前需要先使能对应端口的时钟
 * @warning 重复初始化同一引脚会覆盖之前的配置
 * 
 * @code
 * hal_gpio_config_t config = {
 *     .direction = HAL_GPIO_DIR_OUTPUT,
 *     .pull = HAL_GPIO_PULL_NONE,
 *     .output_mode = HAL_GPIO_OUTPUT_PP,
 *     .init_level = HAL_GPIO_LEVEL_LOW
 * };
 * hal_status_t status = hal_gpio_init(0, 5, &config);
 * @endcode
 * 
 * @see hal_gpio_deinit()
 * @see hal_gpio_config_t
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port, 
                           hal_gpio_pin_t pin, 
                           const hal_gpio_config_t *config);
```

### 7.2 SOLID 原则应用

#### 7.2.1 单一职责原则 (SRP)

```c
/* ❌ 错误示例：一个函数做太多事情 */
void process_sensor_data(void) {
    // 读取传感器
    // 处理数据
    // 发送到网络
    // 保存到文件
    // 更新显示
}

/* ✅ 正确示例：每个函数只做一件事 */
hal_status_t sensor_read(sensor_data_t *data);
hal_status_t data_process(const sensor_data_t *raw, processed_data_t *result);
hal_status_t network_send(const processed_data_t *data);
hal_status_t storage_save(const processed_data_t *data);
hal_status_t display_update(const processed_data_t *data);
```

#### 7.2.2 开闭原则 (OCP)

```c
/* ✅ 通过接口扩展，而非修改现有代码 */

/* 定义传感器接口 */
typedef struct {
    hal_status_t (*init)(void *config);
    hal_status_t (*read)(void *data);
    hal_status_t (*deinit)(void);
} sensor_interface_t;

/* 不同传感器实现相同接口 */
extern const sensor_interface_t bme280_interface;
extern const sensor_interface_t mpu6050_interface;
extern const sensor_interface_t max30102_interface;

/* 通用传感器管理器，无需修改即可支持新传感器 */
hal_status_t sensor_manager_register(const sensor_interface_t *sensor);
```

#### 7.2.3 依赖倒置原则 (DIP)

```c
/* ✅ 高层模块依赖抽象，而非具体实现 */

/* 抽象接口 */
typedef struct {
    hal_status_t (*send)(const uint8_t *data, size_t len);
    hal_status_t (*receive)(uint8_t *data, size_t len, size_t *received);
} comm_interface_t;

/* 应用层依赖抽象接口 */
typedef struct {
    const comm_interface_t *comm;  /* 依赖注入 */
} app_context_t;

hal_status_t app_send_data(app_context_t *ctx, const uint8_t *data, size_t len) {
    return ctx->comm->send(data, len);  /* 通过接口调用 */
}

/* 可以注入不同的实现：UART、SPI、网络等 */
extern const comm_interface_t uart_comm;
extern const comm_interface_t spi_comm;
extern const comm_interface_t tcp_comm;
```

### 7.3 Clean Code 规范

#### 7.3.1 函数设计规范

| 规范 | 要求 | 说明 |
|------|------|------|
| 函数长度 | ≤ 50 行 | 超过则考虑拆分 |
| 参数数量 | ≤ 4 个 | 超过则使用结构体 |
| 嵌套层级 | ≤ 3 层 | 超过则提取子函数 |
| 圈复杂度 | ≤ 10 | 使用静态分析工具检查 |
| 单一出口 | 推荐 | 便于资源清理 |

#### 7.3.2 错误处理规范

```c
/* ✅ 推荐的错误处理模式 */
hal_status_t function_with_cleanup(void)
{
    hal_status_t status = HAL_OK;
    resource_t *res1 = NULL;
    resource_t *res2 = NULL;
    
    /* 分配资源 */
    res1 = allocate_resource1();
    if (res1 == NULL) {
        status = HAL_ERROR_NO_MEMORY;
        goto cleanup;
    }
    
    res2 = allocate_resource2();
    if (res2 == NULL) {
        status = HAL_ERROR_NO_MEMORY;
        goto cleanup;
    }
    
    /* 主逻辑 */
    status = do_work(res1, res2);
    
cleanup:
    /* 统一清理 */
    if (res2 != NULL) {
        free_resource(res2);
    }
    if (res1 != NULL) {
        free_resource(res1);
    }
    
    return status;
}
```

---

## 8. 质量保证

### 8.1 测试策略

#### 8.1.1 测试金字塔

```
                    ┌─────────┐
                    │  系统   │  ← 端到端测试（10%）
                   ─┴─────────┴─
                  ┌─────────────┐
                  │   集成     │  ← 模块集成测试（20%）
                 ─┴─────────────┴─
                ┌─────────────────┐
                │     单元       │  ← 单元测试（70%）
               ─┴─────────────────┴─
```

#### 8.1.2 测试覆盖率要求

| 模块类型 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
|----------|----------|------------|------------|
| HAL 核心 | ≥ 90% | ≥ 80% | 100% |
| OSAL 核心 | ≥ 90% | ≥ 80% | 100% |
| 中间件 | ≥ 80% | ≥ 70% | ≥ 95% |
| 组件 | ≥ 80% | ≥ 70% | ≥ 95% |
| 应用示例 | ≥ 60% | ≥ 50% | ≥ 80% |

### 8.2 单元测试规范

#### 8.2.1 测试框架

使用 **Unity** 测试框架：

```c
/**
 * @file test_hal_gpio.c
 * @brief HAL GPIO 单元测试
 */

#include "unity.h"
#include "hal_gpio.h"
#include "mock_hal_gpio.h"

/*============================================================================*/
/*                              测试夹具                                       */
/*============================================================================*/

void setUp(void)
{
    /* 每个测试前执行 */
    mock_hal_gpio_reset();
}

void tearDown(void)
{
    /* 每个测试后执行 */
    mock_hal_gpio_verify();
}

/*============================================================================*/
/*                              测试用例                                       */
/*============================================================================*/

/**
 * @brief 测试 GPIO 初始化 - 正常情况
 */
void test_hal_gpio_init_success(void)
{
    /* Arrange */
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT,
        .pull = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .init_level = HAL_GPIO_LEVEL_LOW
    };
    
    /* Act */
    hal_status_t status = hal_gpio_init(0, 5, &config);
    
    /* Assert */
    TEST_ASSERT_EQUAL(HAL_OK, status);
}

/**
 * @brief 测试 GPIO 初始化 - 空指针参数
 */
void test_hal_gpio_init_null_config(void)
{
    /* Act */
    hal_status_t status = hal_gpio_init(0, 5, NULL);
    
    /* Assert */
    TEST_ASSERT_EQUAL(HAL_ERROR_NULL_POINTER, status);
}

/**
 * @brief 测试 GPIO 初始化 - 无效端口号
 */
void test_hal_gpio_init_invalid_port(void)
{
    /* Arrange */
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT
    };
    
    /* Act */
    hal_status_t status = hal_gpio_init(255, 5, &config);
    
    /* Assert */
    TEST_ASSERT_EQUAL(HAL_ERROR_INVALID_PARAM, status);
}

/**
 * @brief 测试 GPIO 电平写入
 */
void test_hal_gpio_write_high(void)
{
    /* Arrange */
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT
    };
    hal_gpio_init(0, 5, &config);
    
    /* Act */
    hal_status_t status = hal_gpio_write(0, 5, HAL_GPIO_LEVEL_HIGH);
    
    /* Assert */
    TEST_ASSERT_EQUAL(HAL_OK, status);
    
    hal_gpio_level_t level;
    hal_gpio_read(0, 5, &level);
    TEST_ASSERT_EQUAL(HAL_GPIO_LEVEL_HIGH, level);
}

/*============================================================================*/
/*                              测试运行器                                     */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_hal_gpio_init_success);
    RUN_TEST(test_hal_gpio_init_null_config);
    RUN_TEST(test_hal_gpio_init_invalid_port);
    RUN_TEST(test_hal_gpio_write_high);
    
    return UNITY_END();
}
```

#### 8.2.2 Mock 对象规范

```c
/**
 * @file mock_hal_gpio.h
 * @brief HAL GPIO Mock 对象
 */

#ifndef MOCK_HAL_GPIO_H
#define MOCK_HAL_GPIO_H

#include "hal_gpio.h"

/* Mock 控制函数 */
void mock_hal_gpio_reset(void);
void mock_hal_gpio_verify(void);

/* 期望设置 */
void mock_hal_gpio_expect_init(hal_gpio_port_t port, 
                               hal_gpio_pin_t pin,
                               hal_status_t return_value);

void mock_hal_gpio_expect_write(hal_gpio_port_t port,
                                hal_gpio_pin_t pin,
                                hal_gpio_level_t level,
                                hal_status_t return_value);

/* 模拟返回值设置 */
void mock_hal_gpio_set_read_value(hal_gpio_level_t level);

#endif /* MOCK_HAL_GPIO_H */
```

### 8.3 集成测试规范

```c
/**
 * @file test_hal_uart_integration.c
 * @brief HAL UART 集成测试
 * 
 * 测试 UART 与 DMA、中断的集成工作
 */

#include "unity.h"
#include "hal_uart.h"
#include "hal_dma.h"
#include "osal_semaphore.h"

static osal_semaphore_t s_tx_complete_sem;
static osal_semaphore_t s_rx_complete_sem;

static void uart_tx_callback(hal_uart_port_t port, void *user_data)
{
    osal_semaphore_give(&s_tx_complete_sem);
}

static void uart_rx_callback(hal_uart_port_t port, void *user_data)
{
    osal_semaphore_give(&s_rx_complete_sem);
}

void setUp(void)
{
    osal_semaphore_create(&s_tx_complete_sem, 0, 1);
    osal_semaphore_create(&s_rx_complete_sem, 0, 1);
    
    hal_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = HAL_UART_DATA_8,
        .stop_bits = HAL_UART_STOP_1,
        .parity = HAL_UART_PARITY_NONE,
        .flow_control = HAL_UART_FLOW_NONE,
        .tx_callback = uart_tx_callback,
        .rx_callback = uart_rx_callback
    };
    hal_uart_init(0, &config);
}

void tearDown(void)
{
    hal_uart_deinit(0);
    osal_semaphore_delete(&s_tx_complete_sem);
    osal_semaphore_delete(&s_rx_complete_sem);
}

/**
 * @brief 测试 UART DMA 发送
 */
void test_uart_dma_transmit(void)
{
    /* Arrange */
    uint8_t tx_data[] = "Hello, UART DMA!";
    
    /* Act */
    hal_status_t status = hal_uart_transmit_dma(0, tx_data, sizeof(tx_data));
    
    /* Assert */
    TEST_ASSERT_EQUAL(HAL_OK, status);
    
    /* 等待发送完成 */
    osal_status_t sem_status = osal_semaphore_take(&s_tx_complete_sem, 1000);
    TEST_ASSERT_EQUAL(OSAL_OK, sem_status);
}

/**
 * @brief 测试 UART 回环通信
 * @note 需要硬件将 TX 和 RX 短接
 */
void test_uart_loopback(void)
{
    /* Arrange */
    uint8_t tx_data[] = "Loopback Test";
    uint8_t rx_data[32] = {0};
    
    /* 启动接收 */
    hal_uart_receive_dma(0, rx_data, sizeof(tx_data));
    
    /* 发送数据 */
    hal_uart_transmit_dma(0, tx_data, sizeof(tx_data));
    
    /* 等待接收完成 */
    osal_status_t status = osal_semaphore_take(&s_rx_complete_sem, 1000);
    TEST_ASSERT_EQUAL(OSAL_OK, status);
    
    /* 验证数据 */
    TEST_ASSERT_EQUAL_MEMORY(tx_data, rx_data, sizeof(tx_data));
}
```

### 8.4 代码审查清单

#### 8.4.1 通用检查项

| 检查项 | 说明 |
|--------|------|
| □ 命名规范 | 变量、函数、类型命名符合规范 |
| □ 注释完整 | 文件头、函数、关键逻辑有注释 |
| □ 错误处理 | 所有可能失败的操作都有错误处理 |
| □ 资源管理 | 分配的资源都有对应的释放 |
| □ 边界检查 | 数组访问、指针操作有边界检查 |
| □ 线程安全 | 共享资源访问有适当的同步 |
| □ 代码复杂度 | 函数长度、嵌套层级符合要求 |
| □ 测试覆盖 | 新增代码有对应的测试用例 |

#### 8.4.2 安全检查项

| 检查项 | 说明 |
|--------|------|
| □ 缓冲区溢出 | 字符串操作使用安全函数 |
| □ 整数溢出 | 算术运算检查溢出 |
| □ 空指针 | 指针使用前检查有效性 |
| □ 未初始化变量 | 变量使用前已初始化 |
| □ 敏感数据 | 密钥、密码不硬编码 |
| □ 输入验证 | 外部输入有验证和清理 |

### 8.5 持续集成

#### 8.5.1 CI 流水线

```yaml
# .github/workflows/ci.yml
name: CI Pipeline

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  # 代码检查
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Run clang-format
        run: ./scripts/ci/check_format.py
      - name: Run cppcheck
        run: cppcheck --enable=all --error-exitcode=1 hal/ osal/
      - name: Run MISRA check
        run: ./scripts/ci/misra_check.py

  # 单元测试
  unit-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build tests
        run: |
          mkdir build && cd build
          cmake -DENABLE_TESTS=ON ..
          make -j4
      - name: Run tests
        run: ctest --output-on-failure
      - name: Upload coverage
        run: ./scripts/ci/coverage.py --upload

  # 多平台构建
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        platform: [stm32f4, esp32, nrf52, linux]
    steps:
      - uses: actions/checkout@v3
      - name: Setup toolchain
        run: ./scripts/ci/setup_toolchain.py ${{ matrix.platform }}
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DPLATFORM=${{ matrix.platform }} ..
          make -j4
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: build-${{ matrix.platform }}
          path: build/*.elf
```

---

## 9. 工具链

### 9.1 开发环境

#### 9.1.1 支持的 IDE

| IDE | 支持程度 | 说明 |
|-----|----------|------|
| **VS Code** | ★★★★★ | 推荐，提供专用扩展 |
| **CLion** | ★★★★☆ | CMake 原生支持 |
| **Eclipse CDT** | ★★★☆☆ | 传统嵌入式 IDE |
| **Keil MDK** | ★★★☆☆ | ARM 官方 IDE |
| **IAR EWARM** | ★★★☆☆ | 商业 IDE |

#### 9.1.2 VS Code 配置

```json
// .vscode/settings.json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "files.associations": {
        "*.h": "c",
        "*.c": "c"
    },
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "file"
}
```

```json
// .vscode/launch.json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Cortex Debug",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "openocd",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/app.elf",
            "device": "STM32F401RE",
            "configFiles": [
                "interface/stlink.cfg",
                "target/stm32f4x.cfg"
            ],
            "svdFile": "${workspaceFolder}/vendor/st/STM32F401.svd"
        }
    ]
}
```

### 9.2 编译工具链

#### 9.2.1 支持的编译器

| 编译器 | 版本要求 | 平台支持 |
|--------|----------|----------|
| **ARM GCC** | ≥ 10.3 | STM32, nRF52 |
| **Xtensa GCC** | ≥ 8.4 | ESP32 |
| **RISC-V GCC** | ≥ 10.2 | RISC-V |
| **Clang** | ≥ 12.0 | 所有平台 |
| **IAR** | ≥ 8.50 | ARM |
| **Keil ARMCC** | ≥ 6.0 | ARM |

#### 9.2.2 CMake 工具链配置

```cmake
# configs/toolchains/arm-gcc.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# 工具链路径
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# 编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# 编译选项
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# 链接选项
set(CMAKE_EXE_LINKER_FLAGS_INIT "-specs=nano.specs -specs=nosys.specs -Wl,--gc-sections")

# 禁用编译器检查（交叉编译）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

### 9.3 调试工具

#### 9.3.1 支持的调试器

| 调试器 | 接口 | 支持平台 |
|--------|------|----------|
| **J-Link** | SWD/JTAG | STM32, nRF52 |
| **ST-Link** | SWD | STM32 |
| **CMSIS-DAP** | SWD/JTAG | ARM Cortex |
| **ESP-Prog** | JTAG | ESP32 |
| **Black Magic Probe** | SWD/JTAG | ARM Cortex |

#### 9.3.2 OpenOCD 配置

```tcl
# configs/debug/stm32f4_openocd.cfg
source [find interface/stlink.cfg]
transport select hla_swd

source [find target/stm32f4x.cfg]

reset_config srst_only srst_nogate

# 烧录命令
proc flash_image {image_file} {
    reset halt
    flash write_image erase $image_file 0x08000000
    reset run
}
```

### 9.4 烧录工具

#### 9.4.1 烧录脚本

```python
#!/usr/bin/env python3
# scripts/tools/flash/flash.py
"""
统一烧录工具
支持多种调试器和目标平台
"""

import argparse
import subprocess
import sys
from pathlib import Path

FLASH_TOOLS = {
    'openocd': {
        'stm32': 'openocd -f interface/stlink.cfg -f target/stm32f4x.cfg '
                 '-c "program {elf} verify reset exit"',
        'nrf52': 'openocd -f interface/jlink.cfg -f target/nrf52.cfg '
                 '-c "program {elf} verify reset exit"',
    },
    'jlink': {
        'stm32': 'JLinkExe -device STM32F401RE -if SWD -speed 4000 '
                 '-CommanderScript {script}',
    },
    'esptool': {
        'esp32': 'esptool.py --chip esp32 --port {port} --baud 921600 '
                 'write_flash 0x10000 {bin}',
    }
}

def flash(platform: str, tool: str, firmware: Path, **kwargs):
    """执行烧录"""
    if tool not in FLASH_TOOLS:
        print(f"Error: Unknown tool '{tool}'")
        return 1
    
    if platform not in FLASH_TOOLS[tool]:
        print(f"Error: Tool '{tool}' does not support platform '{platform}'")
        return 1
    
    cmd = FLASH_TOOLS[tool][platform].format(
        elf=firmware,
        bin=firmware.with_suffix('.bin'),
        **kwargs
    )
    
    print(f"Flashing: {cmd}")
    return subprocess.call(cmd, shell=True)

def main():
    parser = argparse.ArgumentParser(description='Flash firmware to target')
    parser.add_argument('firmware', type=Path, help='Firmware file')
    parser.add_argument('-p', '--platform', required=True, 
                        choices=['stm32', 'esp32', 'nrf52'])
    parser.add_argument('-t', '--tool', default='openocd',
                        choices=['openocd', 'jlink', 'esptool'])
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    
    args = parser.parse_args()
    
    if not args.firmware.exists():
        print(f"Error: Firmware file not found: {args.firmware}")
        return 1
    
    return flash(args.platform, args.tool, args.firmware, port=args.port)

if __name__ == '__main__':
    sys.exit(main())
```

### 9.5 代码生成工具

#### 9.5.1 驱动代码生成器

```python
#!/usr/bin/env python3
# scripts/tools/codegen/generate_driver.py
"""
驱动代码生成器
根据配置文件生成驱动框架代码
"""

import argparse
import yaml
from pathlib import Path
from jinja2 import Environment, FileSystemLoader

def load_template(template_name: str) -> str:
    """加载模板"""
    env = Environment(
        loader=FileSystemLoader('scripts/tools/codegen/templates'),
        trim_blocks=True,
        lstrip_blocks=True
    )
    return env.get_template(template_name)

def generate_driver(config_file: Path, output_dir: Path):
    """生成驱动代码"""
    # 加载配置
    with open(config_file) as f:
        config = yaml.safe_load(f)
    
    # 创建输出目录
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # 生成头文件
    header_template = load_template('driver_header.h.j2')
    header_content = header_template.render(**config)
    (output_dir / f"{config['name']}.h").write_text(header_content)
    
    # 生成源文件
    source_template = load_template('driver_source.c.j2')
    source_content = source_template.render(**config)
    (output_dir / f"{config['name']}.c").write_text(source_content)
    
    print(f"Generated driver: {config['name']}")
    print(f"  - {output_dir / config['name']}.h")
    print(f"  - {output_dir / config['name']}.c")

def main():
    parser = argparse.ArgumentParser(description='Generate driver code')
    parser.add_argument('config', type=Path, help='Driver config file (YAML)')
    parser.add_argument('-o', '--output', type=Path, default=Path('.'),
                        help='Output directory')
    
    args = parser.parse_args()
    generate_driver(args.config, args.output)

if __name__ == '__main__':
    main()
```

#### 9.5.2 驱动配置模板

```yaml
# 驱动配置示例: bme280.yaml
name: bme280
description: BME280 温湿度气压传感器驱动
version: 1.0.0
author: Nexus Team

interface: i2c
default_address: 0x76

registers:
  - name: CHIP_ID
    address: 0xD0
    access: read
    
  - name: CTRL_MEAS
    address: 0xF4
    access: read_write
    fields:
      - name: osrs_t
        bits: [7, 5]
        description: 温度过采样
      - name: osrs_p
        bits: [4, 2]
        description: 气压过采样
      - name: mode
        bits: [1, 0]
        description: 工作模式

data_types:
  - name: bme280_data_t
    fields:
      - name: temperature
        type: int32_t
        unit: 0.01 °C
      - name: pressure
        type: uint32_t
        unit: Pa
      - name: humidity
        type: uint32_t
        unit: 0.01 %RH

functions:
  - name: init
    description: 初始化传感器
    params:
      - name: config
        type: const bme280_config_t *
    return: hal_status_t
    
  - name: read
    description: 读取传感器数据
    params:
      - name: data
        type: bme280_data_t *
    return: hal_status_t
```

---

## 10. 文档体系

### 10.1 文档结构

```
docs/
├── getting_started/                # 入门指南
│   ├── 01_installation.md          # 安装指南
│   ├── 02_first_project.md         # 第一个项目
│   ├── 03_build_system.md          # 构建系统
│   └── 04_debugging.md             # 调试指南
│
├── api_reference/                  # API 参考手册
│   ├── hal/                        # HAL API
│   │   ├── hal_gpio.md
│   │   ├── hal_uart.md
│   │   └── ...
│   ├── osal/                       # OSAL API
│   │   ├── osal_task.md
│   │   ├── osal_mutex.md
│   │   └── ...
│   └── components/                 # 组件 API
│
├── guides/                         # 开发指南
│   ├── porting_guide.md            # 移植指南
│   ├── testing_guide.md            # 测试指南
│   ├── performance_guide.md        # 性能优化指南
│   ├── security_guide.md           # 安全指南
│   └── power_guide.md              # 功耗优化指南
│
├── design/                         # 设计文档
│   ├── architecture.md             # 架构设计
│   ├── hal_design.md               # HAL 设计
│   ├── osal_design.md              # OSAL 设计
│   └── component_design.md         # 组件设计
│
├── tutorials/                      # 教程
│   ├── 01_led_blink.md             # LED 闪烁
│   ├── 02_uart_communication.md    # 串口通信
│   ├── 03_sensor_reading.md        # 传感器读取
│   ├── 04_rtos_basics.md           # RTOS 基础
│   └── 05_ota_update.md            # OTA 升级
│
├── faq/                            # 常见问题
│   ├── build_issues.md             # 构建问题
│   ├── debug_issues.md             # 调试问题
│   └── runtime_issues.md           # 运行时问题
│
└── changelog/                      # 变更日志
    ├── CHANGELOG.md                # 总变更日志
    └── migration/                  # 迁移指南
        ├── v1_to_v2.md
        └── v2_to_v3.md
```

### 10.2 API 文档模板

```markdown
# hal_gpio_init

## 概述

初始化 GPIO 引脚，配置引脚的方向、上下拉、输出模式等参数。

## 函数原型

```c
hal_status_t hal_gpio_init(hal_gpio_port_t port, 
                           hal_gpio_pin_t pin, 
                           const hal_gpio_config_t *config);
```

## 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| port | hal_gpio_port_t | 输入 | GPIO 端口号 (0-7) |
| pin | hal_gpio_pin_t | 输入 | GPIO 引脚号 (0-15) |
| config | const hal_gpio_config_t * | 输入 | 配置参数指针 |

### hal_gpio_config_t 结构体

| 字段 | 类型 | 说明 |
|------|------|------|
| direction | hal_gpio_dir_t | 引脚方向 |
| pull | hal_gpio_pull_t | 上下拉配置 |
| output_mode | hal_gpio_output_mode_t | 输出模式 |
| init_level | hal_gpio_level_t | 初始电平 |

## 返回值

| 值 | 说明 |
|----|------|
| HAL_OK | 初始化成功 |
| HAL_ERROR_INVALID_PARAM | 参数无效 |
| HAL_ERROR_NULL_POINTER | config 为 NULL |
| HAL_ERROR_NOT_SUPPORTED | 不支持的配置 |

## 示例

### 基本用法

```c
#include "hal_gpio.h"

void gpio_example(void)
{
    hal_gpio_config_t config = {
        .direction = HAL_GPIO_DIR_OUTPUT,
        .pull = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .init_level = HAL_GPIO_LEVEL_LOW
    };
    
    hal_status_t status = hal_gpio_init(0, 5, &config);
    if (status != HAL_OK) {
        // 错误处理
    }
}
```

### 配置输入引脚带上拉

```c
hal_gpio_config_t config = {
    .direction = HAL_GPIO_DIR_INPUT,
    .pull = HAL_GPIO_PULL_UP
};

hal_gpio_init(0, 0, &config);
```

## 注意事项

1. 调用此函数前需要先使能对应端口的时钟
2. 重复初始化同一引脚会覆盖之前的配置
3. 某些引脚可能有特殊功能限制，请参考具体 MCU 数据手册

## 相关函数

- [hal_gpio_deinit](hal_gpio_deinit.md) - 反初始化 GPIO
- [hal_gpio_write](hal_gpio_write.md) - 写入电平
- [hal_gpio_read](hal_gpio_read.md) - 读取电平

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-01-12 | 初始版本 |
```

### 10.3 Doxygen 配置

```
# docs/Doxyfile
PROJECT_NAME           = "Nexus Embedded Platform"
PROJECT_NUMBER         = "1.0.0"
PROJECT_BRIEF          = "World-class Embedded Software Development Platform"

OUTPUT_DIRECTORY       = docs/api
INPUT                  = hal/include osal/include components
RECURSIVE              = YES
FILE_PATTERNS          = *.h *.c *.md

EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES

GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = YES

HTML_OUTPUT            = html
HTML_THEME             = doxygen-awesome

USE_MDFILE_AS_MAINPAGE = README.md

WARN_IF_UNDOCUMENTED   = YES
WARN_NO_PARAMDOC       = YES

HAVE_DOT               = YES
DOT_IMAGE_FORMAT       = svg
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
```

---

## 11. 版本管理

### 11.1 版本号规范

采用 **语义化版本** (Semantic Versioning)：

```
v<MAJOR>.<MINOR>.<PATCH>[-<PRE-RELEASE>][+<BUILD>]

示例：
- v1.0.0          正式发布版本
- v1.2.3          补丁版本
- v2.0.0-alpha.1  预发布版本
- v1.0.0+build123 带构建号
```

| 版本号 | 变更类型 | 说明 |
|--------|----------|------|
| MAJOR | 不兼容的 API 修改 | 需要用户修改代码 |
| MINOR | 向下兼容的功能新增 | 新功能，旧代码可用 |
| PATCH | 向下兼容的问题修正 | Bug 修复 |

### 11.2 分支策略

```
main                        # 主分支，稳定版本
│
├── develop                 # 开发分支
│   │
│   ├── feature/xxx         # 功能分支
│   │   ├── feature/hal-can
│   │   ├── feature/osal-zephyr
│   │   └── feature/component-ble
│   │
│   └── bugfix/xxx          # Bug 修复分支
│       └── bugfix/uart-overflow
│
├── release/vX.Y.Z          # 发布分支
│   ├── release/v1.0.0
│   └── release/v1.1.0
│
└── hotfix/xxx              # 热修复分支
    └── hotfix/critical-bug
```

### 11.3 发布流程

```
1. 功能开发
   develop ← feature/xxx (PR + Code Review)

2. 发布准备
   release/vX.Y.Z ← develop
   - 版本号更新
   - CHANGELOG 更新
   - 文档更新
   - 回归测试

3. 正式发布
   main ← release/vX.Y.Z (Tag: vX.Y.Z)
   develop ← main (同步)

4. 热修复
   hotfix/xxx ← main
   main ← hotfix/xxx (Tag: vX.Y.Z+1)
   develop ← main (同步)
```

### 11.4 变更日志格式

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- 新增 CAN 总线 HAL 支持

### Changed
- 优化 UART DMA 传输性能

### Fixed
- 修复 GPIO 中断回调内存泄漏问题

## [1.1.0] - 2026-02-01

### Added
- 新增 ESP32-S3 平台支持
- 新增 Zephyr OSAL 适配器
- 新增 BME280 传感器组件

### Changed
- 重构 HAL 接口，统一错误码
- 优化内存分配器性能

### Deprecated
- hal_gpio_set_level() 将在 v2.0 移除，请使用 hal_gpio_write()

### Fixed
- 修复 STM32 SPI DMA 传输不完整问题 (#123)
- 修复 FreeRTOS 适配器内存泄漏 (#125)

### Security
- 更新 mbedTLS 到 3.0.0，修复 CVE-2023-xxxx

## [1.0.0] - 2026-01-12

### Added
- 初始版本发布
- HAL 层：GPIO, UART, SPI, I2C, ADC, Timer
- OSAL 层：FreeRTOS, RT-Thread, Baremetal 适配
- 中间件：LittleFS, lwIP, mbedTLS
- 组件：日志系统, 命令行, CRC
```

---

## 12. 附录

### 附录 A：支持的硬件平台

#### A.1 STM32 系列

| 型号 | 内核 | Flash | RAM | 状态 |
|------|------|-------|-----|------|
| STM32F401RE | Cortex-M4 | 512KB | 96KB | ✅ 支持 |
| STM32F407VG | Cortex-M4 | 1MB | 192KB | ✅ 支持 |
| STM32F429ZI | Cortex-M4 | 2MB | 256KB | ✅ 支持 |
| STM32H743ZI | Cortex-M7 | 2MB | 1MB | 🚧 开发中 |
| STM32L476RG | Cortex-M4 | 1MB | 128KB | 📋 计划中 |

#### A.2 ESP32 系列

| 型号 | 内核 | Flash | RAM | 状态 |
|------|------|-------|-----|------|
| ESP32 | Xtensa LX6 | 4MB | 520KB | 🚧 开发中 |
| ESP32-S3 | Xtensa LX7 | 8MB | 512KB | 📋 计划中 |
| ESP32-C3 | RISC-V | 4MB | 400KB | 📋 计划中 |

#### A.3 Nordic 系列

| 型号 | 内核 | Flash | RAM | 状态 |
|------|------|-------|-----|------|
| nRF52840 | Cortex-M4 | 1MB | 256KB | 📋 计划中 |
| nRF52833 | Cortex-M4 | 512KB | 128KB | 📋 计划中 |

### 附录 B：第三方库版本

| 库名称 | 版本 | 许可证 | 用途 |
|--------|------|--------|------|
| FreeRTOS | 10.5.1 | MIT | RTOS 内核 |
| lwIP | 2.1.3 | BSD | TCP/IP 协议栈 |
| mbedTLS | 3.0.0 | Apache 2.0 | 加密库 |
| LittleFS | 2.5.0 | BSD | 文件系统 |
| LVGL | 8.3.0 | MIT | GUI 框架 |
| Unity | 2.5.2 | MIT | 测试框架 |
| CMSIS | 5.9.0 | Apache 2.0 | ARM 标准接口 |

### 附录 C：编码规范检查工具

#### C.1 clang-format 配置

```yaml
# .clang-format
Language: Cpp
BasedOnStyle: LLVM

IndentWidth: 4
TabWidth: 4
UseTab: Never

ColumnLimit: 100
BreakBeforeBraces: Allman

AlignConsecutiveAssignments: true
AlignConsecutiveDeclarations: true
AlignTrailingComments: true

AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false

PointerAlignment: Right
SpaceAfterCStyleCast: true
SpaceBeforeParens: ControlStatements

IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^"hal_.*\.h"'
    Priority: 2
  - Regex: '^"osal_.*\.h"'
    Priority: 3
  - Regex: '.*'
    Priority: 4
```

#### C.2 cppcheck 配置

```xml
<!-- .cppcheck -->
<?xml version="1.0" encoding="UTF-8"?>
<project version="1">
    <paths>
        <dir name="hal/"/>
        <dir name="osal/"/>
        <dir name="components/"/>
    </paths>
    <exclude>
        <path name="third_party/"/>
        <path name="vendor/"/>
    </exclude>
    <libraries>
        <library>posix</library>
    </libraries>
    <suppressions>
        <suppression>missingIncludeSystem</suppression>
    </suppressions>
    <addons>
        <addon>misra</addon>
    </addons>
</project>
```

### 附录 D：项目配置示例

```yaml
# projects/my_project/config/project_config.yaml
project:
  name: "my_project"
  version: "1.0.0"
  description: "My embedded project"

target:
  board: "nucleo-f401re"
  mcu: "STM32F401RET6"
  clock: 84000000

build:
  toolchain: "arm-none-eabi-gcc"
  optimization: "O2"
  debug: true
  
hal:
  gpio: true
  uart: true
  spi: true
  i2c: true
  adc: true
  timer: true
  
osal:
  backend: "freertos"
  max_tasks: 16
  tick_rate: 1000
  
middleware:
  filesystem:
    enabled: true
    type: "littlefs"
    size: 65536
  network:
    enabled: false
  security:
    enabled: true
    
components:
  - name: "logger"
    config:
      level: "INFO"
      output: "uart"
  - name: "cli"
    config:
      enabled: true
      
memory:
  flash:
    origin: 0x08000000
    length: 512K
  ram:
    origin: 0x20000000
    length: 96K
  stack_size: 4K
  heap_size: 32K
```

### 附录 E：术语表

| 术语 | 英文全称 | 中文解释 |
|------|----------|----------|
| API | Application Programming Interface | 应用程序编程接口 |
| BSP | Board Support Package | 板级支持包 |
| CI/CD | Continuous Integration/Continuous Deployment | 持续集成/持续部署 |
| DMA | Direct Memory Access | 直接内存访问 |
| GPIO | General Purpose Input/Output | 通用输入输出 |
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| ISR | Interrupt Service Routine | 中断服务程序 |
| MCU | Microcontroller Unit | 微控制器单元 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层 |
| OTA | Over-The-Air | 空中升级 |
| PWM | Pulse Width Modulation | 脉冲宽度调制 |
| RTOS | Real-Time Operating System | 实时操作系统 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发器 |

---

## 文档修订历史

| 版本 | 日期 | 作者 | 修订内容 |
|------|------|------|----------|
| 1.0.0 | 2026-01-12 | Nexus Team | 初始版本 |

---

**文档结束**
