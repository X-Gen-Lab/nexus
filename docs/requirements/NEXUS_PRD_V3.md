# Nexus 嵌入式软件开发平台

## 产品需求文档 (PRD) V3.0

| 文档信息 | |
|---------|---|
| 文档版本 | v3.0.0 |
| 创建日期 | 2026-01-12 |
| 文档状态 | 正式版 |
| 作者 | Nexus Architecture Team |

---

## 目录

**第一部分：核心平台**
1. [概述](#1-概述)
2. [系统架构](#2-系统架构)
3. [功能需求](#3-功能需求)
4. [非功能需求](#4-非功能需求)
5. [工程目录结构](#5-工程目录结构)
6. [接口规范](#6-接口规范)
7. [代码规范](#7-代码规范)

**第二部分：开发工具链**
8. [文档体系](#8-文档体系)
9. [构建系统](#9-构建系统)
10. [测试体系](#10-测试体系)
11. [DevOps与CI/CD](#11-devops与cicd)
12. [工具链集成](#12-工具链集成)
13. [跨平台开发](#13-跨平台开发)

**第三部分：安全与可靠性**
14. [安全体系](#14-安全体系)
15. [功能安全](#15-功能安全)

**第四部分：性能与调试**
16. [性能优化工具](#16-性能优化工具)
17. [高级调试能力](#17-高级调试能力)

**第五部分：生态与云端**
18. [生态系统](#18-生态系统)
19. [云端集成](#19-云端集成)
20. [AI/ML支持](#20-aiml支持)

**第六部分：企业与合规**
21. [国际化与合规](#21-国际化与合规)
22. [开发者体验](#22-开发者体验)
23. [企业级特性](#23-企业级特性)

**附录**
24. [版本管理](#24-版本管理)
25. [附录](#25-附录)

---

# 第一部分：核心平台

## 1. 概述

### 1.1 项目背景

随着物联网、智能设备、工业自动化、汽车电子等领域的快速发展，嵌入式软件开发面临以下挑战：

- **硬件碎片化**：MCU 架构多样（ARM Cortex-M、RISC-V、Xtensa 等）
- **安全威胁加剧**：物联网设备成为攻击目标，安全需求日益迫切
- **功能安全要求**：汽车、医疗、工业领域对功能安全认证的需求
- **边缘智能兴起**：TinyML 和边缘 AI 的快速发展
- **云端融合**：设备与云平台的深度集成需求
- **开发效率低下**：工具链分散，缺乏标准化的开发流程
- **跨平台协作困难**：Windows/Linux/macOS 开发环境不统一

### 1.2 项目目标

Nexus 平台旨在构建一个**世界级的嵌入式软件开发平台**：

| 目标 | 描述 | 量化指标 |
|------|------|----------|
| **高可移植性** | 应用代码一次编写，多平台运行 | 支持 ≥10 种 MCU 架构 |
| **高可扩展性** | 模块化设计，按需裁剪 | 最小系统 < 8KB ROM |
| **高实时性** | 满足硬实时应用需求 | 中断响应 < 1μs |
| **低功耗** | 支持多级功耗管理 | 待机功耗 < 10μA |
| **高安全性** | 端到端安全保障 | 支持安全启动、TLS 1.3 |
| **功能安全** | 满足安全认证需求 | MISRA C、IEC 61508 |
| **边缘智能** | 支持 TinyML | TensorFlow Lite Micro |
| **云端集成** | 主流云平台支持 | AWS/Azure/阿里云 |
| **高开发效率** | 完善的工具链和文档 | 新项目启动 < 15 分钟 |
| **跨平台开发** | 支持主流开发环境 | Windows/Linux/macOS |
| **TDD支持** | 测试驱动开发 | 测试覆盖率 ≥ 90% |

### 1.3 目标用户

| 用户角色 | 使用场景 | 核心需求 |
|----------|----------|----------|
| **应用开发者** | 基于平台开发产品应用 | 简单易用的 API、丰富的示例 |
| **驱动开发者** | 开发新硬件驱动和组件 | 清晰的接口规范、移植指南 |
| **安全工程师** | 实现安全功能 | 安全模块、加密库、认证支持 |
| **AI/ML工程师** | 部署边缘智能应用 | TinyML 框架、模型优化工具 |
| **系统集成商** | 集成平台到产品中 | 可裁剪、可配置、文档完善 |
| **平台维护者** | 维护和扩展平台功能 | 模块化设计、完善的测试 |
| **DevOps工程师** | 构建CI/CD流水线 | 自动化构建、测试、部署 |
| **企业管理者** | 管理开发团队和项目 | 权限管理、审计、合规 |

### 1.4 术语定义

| 术语 | 全称 | 定义 |
|------|------|------|
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层 |
| BSP | Board Support Package | 板级支持包 |
| TDD | Test-Driven Development | 测试驱动开发 |
| CI/CD | Continuous Integration/Deployment | 持续集成/部署 |
| OTA | Over-The-Air | 空中升级 |
| HSM | Hardware Security Module | 硬件安全模块 |
| TEE | Trusted Execution Environment | 可信执行环境 |
| SBOM | Software Bill of Materials | 软件物料清单 |
| TinyML | Tiny Machine Learning | 微型机器学习 |

---

## 2. 系统架构

### 2.1 分层架构总览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            APPLICATION LAYER                                 │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐    │
│  │  用户应用  │ │  AI/ML    │ │  云端应用  │ │  示例程序  │ │  测试程序  │    │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └───────────┘    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            FRAMEWORK LAYER                                   │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐    │
│  │  事件框架  │ │  状态机   │ │  命令行   │ │  配置管理  │ │  日志系统  │    │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └───────────┘    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            COMPONENTS LAYER                                  │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐    │
│  │ 传感器驱动 │ │  显示驱动  │ │  协议解析  │ │  算法库   │ │  工具组件  │    │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └───────────┘    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            MIDDLEWARE LAYER                                  │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐  │
│  │文件系统  │ │网络协议栈│ │安全模块  │ │USB协议栈 │ │ GUI框架 │ │ TinyML  │  │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘  │
├─────────────────────────────────────────────────────────────────────────────┤
│                              OSAL LAYER                                      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐  │
│  │ 任务管理 │ │ 同步机制 │ │ 消息队列 │ │  定时器  │ │ 内存管理 │ │ 功耗管理 │  │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  Adapters: FreeRTOS | RT-Thread | Zephyr | ThreadX | Linux | Bare   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────────┤
│                               HAL LAYER                                      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│  │ GPIO │ │ UART │ │ SPI  │ │ I2C  │ │ ADC  │ │ PWM  │ │Timer │ │Crypto│ │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│                            SECURITY LAYER                                    │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐    │
│  │ 安全启动  │ │ 安全存储  │ │ 加密引擎  │ │ TrustZone │ │  密钥管理  │    │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ └───────────┘    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            PLATFORM LAYER                                    │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐  │
│  │ STM32   │ │  ESP32  │ │  nRF52  │ │ RISC-V  │ │  RP2040 │ │  i.MX   │  │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 安全架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SECURITY ARCHITECTURE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                        APPLICATION SECURITY                          │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │    │
│  │  │ 输入验证  │ │ 访问控制  │ │ 安全日志  │ │ 异常处理  │               │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                      COMMUNICATION SECURITY                          │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │    │
│  │  │ TLS 1.3  │ │  DTLS    │ │ 证书管理  │ │ 安全协议  │               │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                        DATA SECURITY                                 │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │    │
│  │  │ 加密存储  │ │ 安全擦除  │ │ 数据完整性│ │ 隐私保护  │               │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                       PLATFORM SECURITY                              │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │    │
│  │  │ 安全启动  │ │ TrustZone│ │ 内存保护  │ │ 调试保护  │               │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                       CRYPTO ENGINE                                  │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │    │
│  │  │AES/ChaCha│ │RSA/ECC   │ │SHA/HMAC  │ │ 随机数   │               │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │  Hardware Accelerator: AES-NI | ARM CE | ESP32 AES | HSM    │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.3 云端集成架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CLOUD INTEGRATION                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                              ┌─────────────┐                                │
│                              │   Cloud     │                                │
│                              │  Platform   │                                │
│                              └──────┬──────┘                                │
│                                     │                                        │
│         ┌───────────────────────────┼───────────────────────────┐           │
│         │                           │                           │           │
│         ▼                           ▼                           ▼           │
│  ┌─────────────┐           ┌─────────────┐           ┌─────────────┐       │
│  │  AWS IoT    │           │ Azure IoT   │           │  阿里云 IoT  │       │
│  │   Core      │           │    Hub      │           │   Platform  │       │
│  └──────┬──────┘           └──────┬──────┘           └──────┬──────┘       │
│         │                         │                         │               │
│         └─────────────────────────┼─────────────────────────┘               │
│                                   │                                          │
│                                   ▼                                          │
│                    ┌──────────────────────────────┐                         │
│                    │      Cloud Abstraction       │                         │
│                    │           Layer              │                         │
│                    └──────────────┬───────────────┘                         │
│                                   │                                          │
│         ┌─────────────────────────┼─────────────────────────┐               │
│         │                         │                         │               │
│         ▼                         ▼                         ▼               │
│  ┌─────────────┐           ┌─────────────┐           ┌─────────────┐       │
│  │   Device    │           │     OTA     │           │  Telemetry  │       │
│  │  Shadow     │           │   Update    │           │   Upload    │       │
│  └─────────────┘           └─────────────┘           └─────────────┘       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
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
| HAL-GPIO-006 | 支持引脚复用配置 | P1 | 复用功能正确 |

#### 3.1.2 通信接口模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **UART** | 波特率配置、DMA、中断、流控 | P0 |
| **SPI** | 主/从模式、4种SPI模式、DMA | P0 |
| **I2C** | 主/从模式、标准/快速/高速模式 | P0 |
| **CAN** | CAN 2.0/CAN FD、过滤器配置 | P1 |
| **USB** | Device/Host/OTG、CDC/HID/MSC | P1 |
| **Ethernet** | MAC/PHY、RMII/MII | P2 |

#### 3.1.3 模拟接口模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **ADC** | 单次/连续采样、多通道扫描、DMA | P0 |
| **DAC** | 单通道/双通道、DMA、波形生成 | P1 |
| **Comparator** | 模拟比较、中断触发 | P2 |

#### 3.1.4 定时器模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **Timer** | 定时中断、输入捕获、输出比较 | P0 |
| **PWM** | 频率/占空比配置、互补输出、死区 | P0 |
| **RTC** | 时间设置/读取、闹钟、日历 | P1 |
| **Watchdog** | 独立/窗口看门狗、超时配置 | P0 |

#### 3.1.5 存储接口模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **Flash** | 读写擦除、扇区管理、ECC | P0 |
| **EEPROM** | 字节读写、磨损均衡 | P1 |
| **SDIO/SDMMC** | SD卡读写、SDIO设备 | P1 |
| **QSPI** | XIP执行、内存映射 | P1 |

#### 3.1.6 安全硬件模块

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **Crypto** | AES/DES硬件加速、哈希加速 | P0 |
| **RNG** | 真随机数生成器 | P0 |
| **PKA** | RSA/ECC硬件加速 | P1 |
| **TRNG** | 真随机数熵源 | P1 |

### 3.2 OSAL 层功能需求

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **任务管理** | 创建/删除/挂起/恢复、优先级配置、CPU亲和性 | P0 |
| **互斥锁** | 普通/递归互斥锁、优先级继承、超时等待 | P0 |
| **信号量** | 二值/计数信号量、超时等待 | P0 |
| **消息队列** | 阻塞/非阻塞收发、ISR安全、优先级队列 | P0 |
| **事件标志** | 多事件等待、AND/OR模式、超时 | P1 |
| **定时器** | 软件定时器、单次/周期模式、高精度 | P0 |
| **内存管理** | 动态分配、内存池、统计、碎片整理 | P0 |
| **功耗管理** | 睡眠模式、唤醒源配置、Tickless | P0 |

### 3.3 中间件层功能需求

| 模块 | 核心功能 | 优先级 |
|------|----------|--------|
| **文件系统** | FAT/LittleFS/SPIFFS、掉电保护、磨损均衡 | P1 |
| **网络协议栈** | TCP/IP、UDP、MQTT、CoAP、HTTP | P1 |
| **安全模块** | TLS 1.3、DTLS、X.509证书 | P0 |
| **OTA升级** | 差分升级、A/B分区、回滚、断点续传 | P1 |
| **USB协议栈** | CDC/HID/MSC/Audio、复合设备 | P1 |
| **GUI框架** | LVGL集成、触摸支持、动画 | P2 |
| **TinyML** | TensorFlow Lite Micro、CMSIS-NN | P1 |

### 3.4 云端集成功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| CLOUD-001 | AWS IoT Core 连接 | P1 | 连接成功率 > 99.9% |
| CLOUD-002 | Azure IoT Hub 连接 | P1 | 连接成功率 > 99.9% |
| CLOUD-003 | 阿里云 IoT 连接 | P1 | 连接成功率 > 99.9% |
| CLOUD-004 | 设备影子/数字孪生 | P1 | 状态同步延迟 < 1s |
| CLOUD-005 | 遥测数据上报 | P0 | 支持批量上报 |
| CLOUD-006 | 远程命令下发 | P0 | 命令响应 < 500ms |
| CLOUD-007 | OTA 固件升级 | P0 | 支持差分升级 |
| CLOUD-008 | 设备证书管理 | P1 | 支持证书轮换 |

---

## 4. 非功能需求

### 4.1 性能需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-PERF-001 | 中断响应时间 | < 1μs |
| NFR-PERF-002 | 任务上下文切换时间 | < 5μs |
| NFR-PERF-003 | HAL API 调用开销 | < 100 cycles |
| NFR-PERF-004 | 系统启动时间 | < 100ms |
| NFR-PERF-005 | TLS 握手时间 | < 2s (RSA-2048) |
| NFR-PERF-006 | AES-128 加密速度 | > 10 MB/s (硬件加速) |

### 4.2 资源需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-RES-001 | 最小 ROM 占用（HAL+OSAL） | < 8KB |
| NFR-RES-002 | 最小 RAM 占用（HAL+OSAL） | < 2KB |
| NFR-RES-003 | 典型 ROM 占用（含网络+安全） | < 128KB |
| NFR-RES-004 | TinyML 推理内存 | < 64KB |

### 4.3 可靠性需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-REL-001 | 系统连续运行时间 | > 365 天 |
| NFR-REL-002 | 内存泄漏 | 0 |
| NFR-REL-003 | 单元测试覆盖率 | ≥ 90% |
| NFR-REL-004 | MTBF (平均故障间隔) | > 100,000 小时 |

### 4.4 安全需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-SEC-001 | 安全启动 | 支持签名验证 |
| NFR-SEC-002 | 通信加密 | TLS 1.3 / DTLS 1.2 |
| NFR-SEC-003 | 密钥存储 | 硬件安全存储 |
| NFR-SEC-004 | 漏洞响应时间 | < 72 小时 |
| NFR-SEC-005 | MISRA C 合规 | 100% 强制规则 |

### 4.5 跨平台需求

| 需求ID | 需求描述 | 指标 |
|--------|----------|------|
| NFR-CROSS-001 | 开发主机操作系统 | Windows/Linux/macOS |
| NFR-CROSS-002 | 支持 MCU 架构 | ≥ 10 种 |
| NFR-CROSS-003 | 支持 RTOS | ≥ 5 种 |
| NFR-CROSS-004 | 新平台移植时间 | < 1 周 |

---

## 5. 工程目录结构

### 5.1 完整目录结构

```
nexus/
├── .github/                    # GitHub 配置和工作流
│   ├── workflows/              # CI/CD 工作流
│   │   ├── build.yml           # 多平台构建
│   │   ├── test.yml            # 测试流水线
│   │   ├── security.yml        # 安全扫描
│   │   ├── docs.yml            # 文档生成
│   │   └── release.yml         # 发布流程
│   ├── ISSUE_TEMPLATE/
│   └── CODEOWNERS
│
├── .vscode/                    # VS Code 配置
│
├── build/                      # 构建输出目录（gitignore）
│
├── cmake/                      # CMake 模块
│   ├── toolchains/             # 工具链文件
│   ├── modules/                # CMake 模块
│   └── platforms/              # 平台配置
│
├── hal/                        # 硬件抽象层
│   ├── include/                # 公共头文件
│   │   └── hal/
│   │       ├── hal_gpio.h
│   │       ├── hal_uart.h
│   │       ├── hal_spi.h
│   │       ├── hal_i2c.h
│   │       ├── hal_crypto.h    # 加密硬件抽象
│   │       └── ...
│   ├── src/                    # 通用实现
│   └── ports/                  # 平台移植
│       ├── stm32f4/
│       ├── stm32h7/
│       ├── esp32/
│       └── ...
│
├── osal/                       # 操作系统抽象层
│   ├── include/
│   ├── src/
│   └── adapters/               # RTOS 适配器
│       ├── freertos/
│       ├── rtthread/
│       ├── zephyr/
│       ├── threadx/
│       └── baremetal/
│
├── framework/                  # 框架层 (FRAMEWORK LAYER)
│   ├── log/                    # 日志系统
│   │   ├── include/
│   │   │   └── log/
│   │   │       ├── log.h
│   │   │       ├── log_def.h
│   │   │       └── log_backend.h
│   │   └── src/
│   ├── event/                  # 事件框架
│   │   ├── include/
│   │   └── src/
│   ├── fsm/                    # 状态机
│   │   ├── include/
│   │   └── src/
│   ├── shell/                  # 命令行
│   │   ├── include/
│   │   └── src/
│   └── config/                 # 配置管理
│       ├── include/
│       └── src/
│
├── security/                   # 安全模块 (新增)
│   ├── include/
│   │   └── security/
│   │       ├── secure_boot.h
│   │       ├── secure_storage.h
│   │       ├── crypto_engine.h
│   │       ├── key_manager.h
│   │       └── tls_client.h
│   ├── src/
│   ├── crypto/                 # 加密算法
│   │   ├── aes/
│   │   ├── rsa/
│   │   ├── ecc/
│   │   ├── hash/
│   │   └── rng/
│   └── certs/                  # 证书管理
│
├── middleware/                 # 中间件
│   ├── filesystem/
│   ├── network/
│   │   ├── tcp_ip/
│   │   ├── mqtt/
│   │   ├── coap/
│   │   └── http/
│   ├── usb/
│   ├── gui/
│   └── ota/                    # OTA 升级
│
├── cloud/                      # 云端集成 (新增)
│   ├── include/
│   │   └── cloud/
│   │       ├── cloud_client.h
│   │       ├── device_shadow.h
│   │       ├── telemetry.h
│   │       └── ota_client.h
│   ├── aws/                    # AWS IoT
│   ├── azure/                  # Azure IoT
│   ├── aliyun/                 # 阿里云 IoT
│   └── common/                 # 通用抽象
│
├── ai/                         # AI/ML 模块 (新增)
│   ├── include/
│   │   └── ai/
│   │       ├── tinyml.h
│   │       ├── inference.h
│   │       └── model_loader.h
│   ├── tflite_micro/           # TensorFlow Lite Micro
│   ├── cmsis_nn/               # CMSIS-NN
│   ├── models/                 # 预训练模型
│   └── tools/                  # 模型转换工具
│
├── components/                 # 可复用组件
│   ├── sensors/                # 传感器驱动
│   ├── displays/               # 显示驱动
│   ├── protocols/              # 协议解析
│   └── algorithms/             # 算法库
│
├── boards/                     # 板级支持包
├── platforms/                  # 平台支持
├── drivers/                    # 芯片驱动
│
├── applications/               # 应用程序
│   ├── demos/                  # 示例应用
│   ├── templates/              # 项目模板
│   └── projects/               # 实际项目
│
├── tests/                      # 测试代码
│   ├── unit/                   # 单元测试 (GTest)
│   ├── integration/            # 集成测试
│   ├── security/               # 安全测试
│   ├── performance/            # 性能测试
│   ├── mocks/                  # Mock 对象 (GMock)
│   └── fixtures/               # 测试夹具
│
├── docs/                       # 文档
│   ├── doxygen/
│   ├── sphinx/
│   ├── api/
│   ├── guides/
│   ├── security/               # 安全文档
│   └── requirements/
│
├── tools/                      # 开发工具
│   ├── cli/                    # 命令行工具 (新增)
│   ├── codegen/                # 代码生成器
│   ├── flash/                  # 烧录工具
│   ├── analysis/               # 代码分析
│   ├── profiler/               # 性能分析 (新增)
│   ├── security/               # 安全工具 (新增)
│   │   ├── sbom/               # SBOM 生成
│   │   ├── vuln_scan/          # 漏洞扫描
│   │   └── sign/               # 固件签名
│   └── scripts/
│
├── ext/                        # 第三方库
│   ├── googletest/
│   ├── freertos/
│   ├── catch/
│   ├── mbedtls/
│   ├── lwip/
│   ├── tflite_micro/
│   └── ...
│
├── .clang-format
├── .clang-tidy
├── .gitignore
├── CMakeLists.txt
├── Makefile
├── BUILD.bazel
├── Doxyfile
├── requirements.txt
├── README.md
├── LICENSE
├── SECURITY.md                 # 安全政策 (新增)
├── CONTRIBUTING.md
└── CHANGELOG.md
```

---

## 6. 接口规范

### 6.1 HAL 接口设计模式

采用**函数指针表**模式实现硬件抽象：

```c
/**
 * \file            hal_gpio.h
 * \brief           GPIO Hardware Abstraction Layer Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           GPIO operations interface
 */
typedef struct {
    hal_status_t (*init)(hal_gpio_port_t port,
                         hal_gpio_pin_t pin,
                         const hal_gpio_config_t* config);
    hal_status_t (*deinit)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    hal_status_t (*write)(hal_gpio_port_t port,
                          hal_gpio_pin_t pin,
                          hal_gpio_level_t level);
    hal_status_t (*read)(hal_gpio_port_t port,
                         hal_gpio_pin_t pin,
                         hal_gpio_level_t* level);
    hal_status_t (*toggle)(hal_gpio_port_t port, hal_gpio_pin_t pin);
    hal_status_t (*set_interrupt)(hal_gpio_port_t port,
                                  hal_gpio_pin_t pin,
                                  hal_gpio_irq_mode_t mode,
                                  hal_gpio_irq_callback_t callback,
                                  void* context);
} hal_gpio_interface_t;

const hal_gpio_interface_t* hal_gpio_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
```

### 6.2 安全接口规范

```c
/**
 * \file            crypto_engine.h
 * \brief           Cryptographic Engine Interface
 * \author          Nexus Security Team
 * \version         1.0.0
 * \date            2026-01-12
 */

#ifndef CRYPTO_ENGINE_H
#define CRYPTO_ENGINE_H

#include "security_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           AES encryption context
 */
typedef struct {
    uint8_t key[32];            /**< AES key (128/192/256 bits) */
    uint8_t iv[16];             /**< Initialization vector */
    crypto_aes_mode_t mode;     /**< AES mode (ECB/CBC/CTR/GCM) */
    size_t key_bits;            /**< Key size in bits */
} crypto_aes_context_t;

/**
 * \brief           Crypto engine interface
 */
typedef struct {
    /**
     * \brief           Initialize AES context
     * \param[out]      ctx: AES context to initialize
     * \param[in]       key: Encryption key
     * \param[in]       key_bits: Key size (128/192/256)
     * \param[in]       mode: AES mode
     * \return          CRYPTO_OK on success
     */
    crypto_status_t (*aes_init)(crypto_aes_context_t* ctx,
                                const uint8_t* key,
                                size_t key_bits,
                                crypto_aes_mode_t mode);

    /**
     * \brief           AES encryption
     * \param[in]       ctx: AES context
     * \param[in]       input: Input data
     * \param[out]      output: Output buffer
     * \param[in]       length: Data length
     * \return          CRYPTO_OK on success
     */
    crypto_status_t (*aes_encrypt)(crypto_aes_context_t* ctx,
                                   const uint8_t* input,
                                   uint8_t* output,
                                   size_t length);

    /**
     * \brief           AES decryption
     */
    crypto_status_t (*aes_decrypt)(crypto_aes_context_t* ctx,
                                   const uint8_t* input,
                                   uint8_t* output,
                                   size_t length);

    /**
     * \brief           SHA-256 hash
     * \param[in]       input: Input data
     * \param[in]       length: Data length
     * \param[out]      hash: Output hash (32 bytes)
     * \return          CRYPTO_OK on success
     */
    crypto_status_t (*sha256)(const uint8_t* input,
                              size_t length,
                              uint8_t hash[32]);

    /**
     * \brief           Generate random bytes
     * \param[out]      buffer: Output buffer
     * \param[in]       length: Number of bytes
     * \return          CRYPTO_OK on success
     */
    crypto_status_t (*random)(uint8_t* buffer, size_t length);

} crypto_engine_interface_t;

const crypto_engine_interface_t* crypto_get_engine(void);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_ENGINE_H */
```

### 6.3 云端接口规范

```c
/**
 * \file            cloud_client.h
 * \brief           Cloud Platform Client Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 */

#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include "cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Cloud client configuration
 */
typedef struct {
    const char* endpoint;           /**< Cloud endpoint URL */
    const char* device_id;          /**< Device identifier */
    const char* client_cert;        /**< Client certificate (PEM) */
    const char* client_key;         /**< Client private key (PEM) */
    const char* root_ca;            /**< Root CA certificate (PEM) */
    uint32_t keepalive_sec;         /**< Keep-alive interval */
    cloud_qos_t default_qos;        /**< Default QoS level */
} cloud_client_config_t;

/**
 * \brief           Cloud client interface
 */
typedef struct {
    /**
     * \brief           Connect to cloud platform
     * \param[in]       config: Client configuration
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*connect)(const cloud_client_config_t* config);

    /**
     * \brief           Disconnect from cloud
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*disconnect)(void);

    /**
     * \brief           Publish telemetry data
     * \param[in]       topic: Topic name
     * \param[in]       payload: JSON payload
     * \param[in]       qos: QoS level
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*publish)(const char* topic,
                              const char* payload,
                              cloud_qos_t qos);

    /**
     * \brief           Subscribe to topic
     * \param[in]       topic: Topic pattern
     * \param[in]       callback: Message callback
     * \param[in]       context: User context
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*subscribe)(const char* topic,
                                cloud_message_callback_t callback,
                                void* context);

    /**
     * \brief           Update device shadow
     * \param[in]       reported: Reported state (JSON)
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*shadow_update)(const char* reported);

    /**
     * \brief           Get device shadow
     * \param[out]      desired: Desired state buffer
     * \param[in]       max_len: Buffer size
     * \return          CLOUD_OK on success
     */
    cloud_status_t (*shadow_get)(char* desired, size_t max_len);

} cloud_client_interface_t;

const cloud_client_interface_t* cloud_get_client(cloud_provider_t provider);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_CLIENT_H */
```

---

## 7. 代码规范

### 7.1 代码格式规范

使用 **clang-format** 进行代码格式化：

| 规则 | 配置值 | 说明 |
|------|--------|------|
| `ColumnLimit` | 80 | 每行最大字符数 |
| `IndentWidth` | 4 | 缩进宽度 |
| `UseTab` | Never | 不使用 Tab |
| `PointerAlignment` | Left | 指针符号靠左 |
| `BreakBeforeBraces` | Attach | 花括号不换行 |

### 7.2 Doxygen 注释规范

采用 **反斜杠风格** 的 Doxygen 注释：

```c
/**
 * \file            module.c
 * \brief           Module brief description
 * \author          Author Name (author@example.com)
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/**
 * \brief           Function brief description
 *
 * \param[in]       param1: Input parameter description
 * \param[out]      param2: Output parameter description
 * \param[in,out]   param3: Input/output parameter
 *
 * \return          Return value description
 * \retval          VALUE1: Meaning of VALUE1
 * \retval          VALUE2: Meaning of VALUE2
 *
 * \note            Additional notes
 * \warning         Warning message
 *
 * \code{.c}
 * // Usage example
 * result = function(arg1, &arg2, &arg3);
 * \endcode
 */
```

### 7.3 安全编码规范

| 规则 | 说明 | 示例 |
|------|------|------|
| 输入验证 | 所有外部输入必须验证 | 检查指针、范围、格式 |
| 缓冲区安全 | 使用安全函数 | `strncpy` 替代 `strcpy` |
| 整数溢出 | 检查算术运算 | 使用安全算术宏 |
| 内存安全 | 初始化、边界检查 | `memset` 敏感数据 |
| 错误处理 | 检查所有返回值 | 不忽略错误码 |
| 密钥管理 | 安全存储和清除 | 使用后清零密钥 |

```c
/* ✅ 安全编码示例 */

/**
 * \brief           Secure memory copy with bounds checking
 */
hal_status_t secure_memcpy(void* dest, size_t dest_size,
                           const void* src, size_t count)
{
    /* 输入验证 */
    if (dest == NULL || src == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* 边界检查 */
    if (count > dest_size) {
        return HAL_ERROR_BUFFER_OVERFLOW;
    }

    /* 重叠检查 */
    if ((dest < src && (uint8_t*)dest + count > (uint8_t*)src) ||
        (src < dest && (uint8_t*)src + count > (uint8_t*)dest)) {
        return HAL_ERROR_OVERLAP;
    }

    memcpy(dest, src, count);
    return HAL_OK;
}

/**
 * \brief           Secure key handling
 */
void secure_key_cleanup(uint8_t* key, size_t key_len)
{
    if (key != NULL && key_len > 0) {
        /* 使用 volatile 防止编译器优化 */
        volatile uint8_t* p = (volatile uint8_t*)key;
        while (key_len--) {
            *p++ = 0;
        }
    }
}
```

---

# 第二部分：开发工具链

## 8. 文档体系

### 8.1 文档工具链

采用 **Git + Doxygen + Sphinx + Breathe** 构建完整的文档体系：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DOCUMENTATION PIPELINE                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Source  │───▶│ Doxygen  │───▶│ Breathe  │───▶│  Sphinx  │          │
│  │   Code   │    │   XML    │    │  Bridge  │    │   HTML   │          │
│  │ (C/C++)  │    │          │    │          │    │   PDF    │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 8.2 文档类型

| 文档类型 | 格式 | 说明 |
|----------|------|------|
| API 参考 | Doxygen | 从代码注释自动生成 |
| 开发指南 | Markdown/RST | 手动编写的教程 |
| 架构设计 | Markdown + 图表 | 系统设计文档 |
| 安全指南 | Markdown | 安全最佳实践 |
| 发布说明 | Markdown | 版本变更记录 |

---

## 9. 构建系统

### 9.1 多构建系统支持

| 构建系统 | 适用场景 | 优势 |
|----------|----------|------|
| **CMake** | 主要构建系统 | 跨平台、IDE集成好 |
| **Make** | 简单项目、快速构建 | 轻量、无依赖 |
| **Bazel** | 大型项目、精确依赖 | 增量构建、可重现 |

### 9.2 构建命令

```bash
# CMake 构建
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEXUS_PLATFORM=stm32f4 \
    -DNEXUS_ENABLE_SECURITY=ON \
    -DNEXUS_ENABLE_CLOUD=ON \
    -DNEXUS_ENABLE_AI=ON
cmake --build build --parallel

# Make 构建
make PLATFORM=stm32f4 SECURITY=1 CLOUD=1 -j$(nproc)

# Bazel 构建
bazel build //... --config=stm32f4
```

---

## 10. 测试体系

### 10.1 测试框架

| 框架 | 用途 | 说明 |
|------|------|------|
| **Google Test** | 单元测试 | C++ 测试框架 |
| **Google Mock** | Mock 对象 | 依赖隔离 |
| **Unity** | 嵌入式单元测试 | 纯 C 实现 |
| **CTest** | 测试运行器 | CMake 集成 |

### 10.2 测试驱动开发 (TDD)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          TDD CYCLE                                       │
│                                                                          │
│     ┌─────────┐         ┌─────────┐         ┌─────────┐                │
│     │  RED    │────────▶│  GREEN  │────────▶│REFACTOR │                │
│     │ (Fail)  │         │ (Pass)  │         │(Improve)│                │
│     └─────────┘         └─────────┘         └─────────┘                │
│          │                                        │                      │
│          └────────────────────────────────────────┘                      │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 10.3 测试覆盖率要求

| 模块 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
|------|----------|------------|------------|
| HAL 核心 | ≥ 90% | ≥ 80% | 100% |
| OSAL 核心 | ≥ 90% | ≥ 80% | 100% |
| 安全模块 | ≥ 95% | ≥ 90% | 100% |
| 云端模块 | ≥ 85% | ≥ 75% | ≥ 95% |
| AI 模块 | ≥ 80% | ≥ 70% | ≥ 90% |

---

## 11. DevOps与CI/CD

### 11.1 CI/CD 架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          CI/CD PIPELINE                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │   Code   │───▶│  Build   │───▶│   Test   │───▶│ Security │          │
│  │  Commit  │    │  Stage   │    │  Stage   │    │   Scan   │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                       │                  │
│                                                       ▼                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Deploy  │◀───│  Docs    │◀───│ Coverage │◀───│  SBOM    │          │
│  │  Stage   │    │  Build   │    │  Report  │    │ Generate │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 11.2 质量门禁

| 检查项 | 阈值 | 阻断级别 |
|--------|------|----------|
| 代码格式检查 | 100% 通过 | 阻断 |
| 静态分析 (cppcheck) | 0 错误 | 阻断 |
| MISRA C 检查 | 0 强制规则违规 | 阻断 |
| 编译警告 | 0 警告 (-Werror) | 阻断 |
| 单元测试 | 100% 通过 | 阻断 |
| 代码覆盖率 | ≥ 90% | 警告 |
| 安全漏洞扫描 | 0 高危漏洞 | 阻断 |
| SBOM 生成 | 成功 | 警告 |

---

## 12. 工具链集成

### 12.1 开发工具概览

| 类别 | 工具 | 说明 |
|------|------|------|
| **IDE** | VS Code, CLion, Eclipse | 代码编辑、调试 |
| **编译器** | GCC, Clang, ARMCC, IAR | 交叉编译 |
| **调试器** | GDB, OpenOCD, J-Link | 硬件调试 |
| **分析** | clang-format, cppcheck, MISRA | 代码质量 |
| **仿真** | QEMU, Renode | 软件仿真 |
| **性能** | Tracealyzer, SystemView | 性能分析 |

---

## 13. 跨平台开发

### 13.1 支持的开发主机

| 操作系统 | 版本要求 | 支持状态 |
|----------|----------|----------|
| **Windows** | 10/11 | 完全支持 |
| **Linux** | Ubuntu 20.04+ / Fedora 35+ | 完全支持 |
| **macOS** | 12.0+ (Monterey) | 完全支持 |

### 13.2 Docker 开发环境

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git \
    gcc-arm-none-eabi gdb-multiarch \
    clang clang-format clang-tidy cppcheck \
    python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install sphinx breathe sphinx-rtd-theme

WORKDIR /workspace
CMD ["/bin/bash"]
```

---

# 第三部分：安全与可靠性

## 14. 安全体系

### 14.1 安全架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SECURITY FRAMEWORK                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    SECURE DEVELOPMENT                            │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 安全编码  │ │ 代码审计  │ │ 漏洞扫描  │ │ 渗透测试  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    RUNTIME SECURITY                              │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 安全启动  │ │ 内存保护  │ │ 访问控制  │ │ 异常检测  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    DATA SECURITY                                 │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 加密存储  │ │ 安全传输  │ │ 密钥管理  │ │ 证书管理  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    SUPPLY CHAIN SECURITY                         │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │  SBOM    │ │ 依赖审计  │ │ 签名验证  │ │ 来源追溯  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 14.2 安全启动 (Secure Boot)

#### 14.2.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SEC-BOOT-001 | 支持固件签名验证 | P0 | RSA-2048/ECDSA-P256 |
| SEC-BOOT-002 | 支持信任链验证 | P0 | Root → Bootloader → App |
| SEC-BOOT-003 | 支持回滚保护 | P0 | 版本号单调递增 |
| SEC-BOOT-004 | 支持安全升级 | P0 | 升级前验证签名 |
| SEC-BOOT-005 | 支持故障恢复 | P1 | 升级失败可回滚 |

#### 14.2.2 安全启动流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SECURE BOOT FLOW                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  ROM     │───▶│  First   │───▶│  Second  │───▶│   App    │          │
│  │  Boot    │    │  Stage   │    │  Stage   │    │ Firmware │          │
│  │          │    │Bootloader│    │Bootloader│    │          │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│       │               │               │               │                  │
│       ▼               ▼               ▼               ▼                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Verify  │    │  Verify  │    │  Verify  │    │  Start   │          │
│  │  OTP Key │    │  BL Sig  │    │  App Sig │    │   App    │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                                          │
│  Trust Chain: OTP Root Key → BL Public Key → App Public Key             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

#### 14.2.3 接口定义

```c
/**
 * \file            secure_boot.h
 * \brief           Secure Boot Interface
 */

#ifndef SECURE_BOOT_H
#define SECURE_BOOT_H

#include "security_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Firmware image header
 */
typedef struct {
    uint32_t magic;             /**< Magic number (0x4E455855) */
    uint32_t version;           /**< Firmware version */
    uint32_t size;              /**< Image size (excluding header) */
    uint32_t entry_point;       /**< Entry point address */
    uint8_t  hash[32];          /**< SHA-256 hash of image */
    uint8_t  signature[256];    /**< RSA-2048 signature */
    uint32_t flags;             /**< Image flags */
    uint32_t reserved[4];       /**< Reserved for future use */
} secure_boot_header_t;

/**
 * \brief           Verify firmware image signature
 *
 * \param[in]       header: Firmware header
 * \param[in]       image: Firmware image data
 * \param[in]       public_key: Public key for verification
 *
 * \return          SECURITY_OK if signature is valid
 */
security_status_t secure_boot_verify(const secure_boot_header_t* header,
                                     const uint8_t* image,
                                     const uint8_t* public_key);

/**
 * \brief           Check firmware version (anti-rollback)
 *
 * \param[in]       new_version: New firmware version
 *
 * \return          SECURITY_OK if version is acceptable
 */
security_status_t secure_boot_check_version(uint32_t new_version);

/**
 * \brief           Update anti-rollback counter
 *
 * \param[in]       version: New minimum version
 *
 * \return          SECURITY_OK on success
 */
security_status_t secure_boot_update_counter(uint32_t version);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_BOOT_H */
```

### 14.3 安全存储 (Secure Storage)

#### 14.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SEC-STOR-001 | 支持加密存储 | P0 | AES-256-GCM |
| SEC-STOR-002 | 支持密钥隔离 | P0 | 每设备唯一密钥 |
| SEC-STOR-003 | 支持完整性保护 | P0 | HMAC 验证 |
| SEC-STOR-004 | 支持安全擦除 | P0 | 多次覆写 |
| SEC-STOR-005 | 支持访问控制 | P1 | 基于身份的访问 |

#### 14.3.2 接口定义

```c
/**
 * \file            secure_storage.h
 * \brief           Secure Storage Interface
 */

#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include "security_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Secure storage item ID
 */
typedef uint32_t secure_storage_id_t;

/**
 * \brief           Predefined storage IDs
 */
#define SECURE_STORAGE_ID_DEVICE_KEY    0x0001  /**< Device unique key */
#define SECURE_STORAGE_ID_CERT          0x0002  /**< Device certificate */
#define SECURE_STORAGE_ID_PRIVATE_KEY   0x0003  /**< Private key */
#define SECURE_STORAGE_ID_CONFIG        0x0004  /**< Secure configuration */
#define SECURE_STORAGE_ID_USER_BASE     0x1000  /**< User-defined IDs start */

/**
 * \brief           Write data to secure storage
 *
 * \param[in]       id: Storage item ID
 * \param[in]       data: Data to store
 * \param[in]       length: Data length
 *
 * \return          SECURITY_OK on success
 */
security_status_t secure_storage_write(secure_storage_id_t id,
                                       const void* data,
                                       size_t length);

/**
 * \brief           Read data from secure storage
 *
 * \param[in]       id: Storage item ID
 * \param[out]      data: Buffer for data
 * \param[in,out]   length: Buffer size / actual length
 *
 * \return          SECURITY_OK on success
 */
security_status_t secure_storage_read(secure_storage_id_t id,
                                      void* data,
                                      size_t* length);

/**
 * \brief           Delete item from secure storage
 *
 * \param[in]       id: Storage item ID
 *
 * \return          SECURITY_OK on success
 */
security_status_t secure_storage_delete(secure_storage_id_t id);

/**
 * \brief           Securely erase all storage
 *
 * \return          SECURITY_OK on success
 */
security_status_t secure_storage_erase_all(void);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_STORAGE_H */
```

### 14.4 加密引擎 (Crypto Engine)

#### 14.4.1 支持的算法

| 类别 | 算法 | 密钥长度 | 硬件加速 |
|------|------|----------|----------|
| **对称加密** | AES-ECB/CBC/CTR/GCM | 128/192/256 | ✅ |
| **对称加密** | ChaCha20-Poly1305 | 256 | 部分 |
| **非对称加密** | RSA | 2048/4096 | ✅ |
| **非对称加密** | ECDSA/ECDH | P-256/P-384 | ✅ |
| **哈希** | SHA-256/SHA-384/SHA-512 | - | ✅ |
| **MAC** | HMAC-SHA256 | - | ✅ |
| **KDF** | HKDF/PBKDF2 | - | 软件 |
| **随机数** | TRNG/DRBG | - | ✅ |

#### 14.4.2 性能指标

| 算法 | 软件实现 | 硬件加速 | 目标平台 |
|------|----------|----------|----------|
| AES-128-GCM | 2 MB/s | 50 MB/s | STM32H7 |
| SHA-256 | 5 MB/s | 100 MB/s | STM32H7 |
| RSA-2048 签名 | 500ms | 50ms | STM32H7 |
| ECDSA-P256 签名 | 100ms | 10ms | STM32H7 |

### 14.5 TLS/DTLS 支持

#### 14.5.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SEC-TLS-001 | 支持 TLS 1.3 | P0 | 完整握手 < 2s |
| SEC-TLS-002 | 支持 TLS 1.2 | P0 | 向后兼容 |
| SEC-TLS-003 | 支持 DTLS 1.2 | P1 | UDP 安全通信 |
| SEC-TLS-004 | 支持证书验证 | P0 | X.509 证书链 |
| SEC-TLS-005 | 支持会话恢复 | P1 | 减少握手开销 |
| SEC-TLS-006 | 支持 PSK 模式 | P1 | 预共享密钥 |

#### 14.5.2 支持的密码套件

```
TLS 1.3:
  - TLS_AES_128_GCM_SHA256
  - TLS_AES_256_GCM_SHA384
  - TLS_CHACHA20_POLY1305_SHA256

TLS 1.2:
  - TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  - TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
  - TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
```

### 14.6 密钥管理 (Key Management)

#### 14.6.1 密钥层次结构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        KEY HIERARCHY                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                    ┌─────────────────────┐                              │
│                    │   Root Key (OTP)    │                              │
│                    │   Hardware Fused    │                              │
│                    └──────────┬──────────┘                              │
│                               │                                          │
│              ┌────────────────┼────────────────┐                        │
│              │                │                │                        │
│              ▼                ▼                ▼                        │
│     ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│     │  Storage    │  │   Boot      │  │  Identity   │                  │
│     │    KEK      │  │    Key      │  │    Key      │                  │
│     └──────┬──────┘  └─────────────┘  └──────┬──────┘                  │
│            │                                  │                          │
│     ┌──────┴──────┐                   ┌──────┴──────┐                   │
│     │             │                   │             │                   │
│     ▼             ▼                   ▼             ▼                   │
│ ┌───────┐   ┌───────┐           ┌───────┐   ┌───────┐                  │
│ │ Data  │   │Config │           │ TLS   │   │ Sign  │                  │
│ │  Key  │   │  Key  │           │  Key  │   │  Key  │                  │
│ └───────┘   └───────┘           └───────┘   └───────┘                  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

#### 14.6.2 接口定义

```c
/**
 * \file            key_manager.h
 * \brief           Key Management Interface
 */

#ifndef KEY_MANAGER_H
#define KEY_MANAGER_H

#include "security_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Key types
 */
typedef enum {
    KEY_TYPE_AES_128,           /**< AES-128 key */
    KEY_TYPE_AES_256,           /**< AES-256 key */
    KEY_TYPE_RSA_2048,          /**< RSA-2048 key pair */
    KEY_TYPE_RSA_4096,          /**< RSA-4096 key pair */
    KEY_TYPE_ECC_P256,          /**< ECC P-256 key pair */
    KEY_TYPE_ECC_P384,          /**< ECC P-384 key pair */
    KEY_TYPE_HMAC_SHA256,       /**< HMAC-SHA256 key */
} key_type_t;

/**
 * \brief           Key handle (opaque)
 */
typedef uint32_t key_handle_t;

/**
 * \brief           Generate new key
 *
 * \param[in]       type: Key type
 * \param[out]      handle: Key handle
 *
 * \return          SECURITY_OK on success
 */
security_status_t key_generate(key_type_t type, key_handle_t* handle);

/**
 * \brief           Import key
 *
 * \param[in]       type: Key type
 * \param[in]       key_data: Key material
 * \param[in]       key_len: Key length
 * \param[out]      handle: Key handle
 *
 * \return          SECURITY_OK on success
 */
security_status_t key_import(key_type_t type,
                             const uint8_t* key_data,
                             size_t key_len,
                             key_handle_t* handle);

/**
 * \brief           Export public key
 *
 * \param[in]       handle: Key handle
 * \param[out]      pub_key: Public key buffer
 * \param[in,out]   pub_key_len: Buffer size / actual length
 *
 * \return          SECURITY_OK on success
 */
security_status_t key_export_public(key_handle_t handle,
                                    uint8_t* pub_key,
                                    size_t* pub_key_len);

/**
 * \brief           Delete key
 *
 * \param[in]       handle: Key handle
 *
 * \return          SECURITY_OK on success
 */
security_status_t key_delete(key_handle_t handle);

/**
 * \brief           Derive key using HKDF
 *
 * \param[in]       master_handle: Master key handle
 * \param[in]       info: Context info
 * \param[in]       info_len: Info length
 * \param[in]       derived_type: Derived key type
 * \param[out]      derived_handle: Derived key handle
 *
 * \return          SECURITY_OK on success
 */
security_status_t key_derive(key_handle_t master_handle,
                             const uint8_t* info,
                             size_t info_len,
                             key_type_t derived_type,
                             key_handle_t* derived_handle);

#ifdef __cplusplus
}
#endif

#endif /* KEY_MANAGER_H */
```

### 14.7 漏洞管理与 SBOM

#### 14.7.1 SBOM (软件物料清单)

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SEC-SBOM-001 | 自动生成 SBOM | P0 | SPDX/CycloneDX 格式 |
| SEC-SBOM-002 | 依赖关系追踪 | P0 | 包含所有第三方库 |
| SEC-SBOM-003 | 版本信息记录 | P0 | 精确到 commit hash |
| SEC-SBOM-004 | License 扫描 | P1 | 识别所有 License |
| SEC-SBOM-005 | CVE 关联 | P1 | 自动关联已知漏洞 |

#### 14.7.2 SBOM 示例 (CycloneDX)

```json
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.4",
  "version": 1,
  "metadata": {
    "timestamp": "2026-01-12T00:00:00Z",
    "component": {
      "type": "firmware",
      "name": "nexus-firmware",
      "version": "3.0.0"
    }
  },
  "components": [
    {
      "type": "library",
      "name": "freertos",
      "version": "10.5.1",
      "purl": "pkg:github/FreeRTOS/FreeRTOS-Kernel@V10.5.1",
      "licenses": [{"license": {"id": "MIT"}}]
    },
    {
      "type": "library",
      "name": "mbedtls",
      "version": "3.4.0",
      "purl": "pkg:github/Mbed-TLS/mbedtls@v3.4.0",
      "licenses": [{"license": {"id": "Apache-2.0"}}]
    },
    {
      "type": "library",
      "name": "lwip",
      "version": "2.1.3",
      "purl": "pkg:github/lwip-tcpip/lwip@STABLE-2_1_3_RELEASE",
      "licenses": [{"license": {"id": "BSD-3-Clause"}}]
    }
  ],
  "vulnerabilities": []
}
```

#### 14.7.3 漏洞扫描流程

```yaml
# .github/workflows/security.yml
name: Security Scan

on:
  push:
    branches: [main, develop]
  schedule:
    - cron: '0 0 * * *'  # 每日扫描

jobs:
  sbom:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Generate SBOM
        uses: anchore/sbom-action@v0
        with:
          format: cyclonedx-json
          output-file: sbom.json

      - name: Upload SBOM
        uses: actions/upload-artifact@v3
        with:
          name: sbom
          path: sbom.json

  vulnerability-scan:
    needs: sbom
    runs-on: ubuntu-latest
    steps:
      - name: Download SBOM
        uses: actions/download-artifact@v3
        with:
          name: sbom

      - name: Scan for vulnerabilities
        uses: anchore/scan-action@v3
        with:
          sbom: sbom.json
          fail-build: true
          severity-cutoff: high

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run cppcheck
        run: |
          cppcheck --enable=all --error-exitcode=1 \
            --suppress=missingIncludeSystem \
            hal/ osal/ security/ middleware/

      - name: Run MISRA check
        run: |
          cppcheck --addon=misra --error-exitcode=1 \
            hal/ osal/ security/
```

### 14.8 安全政策 (SECURITY.md)

```markdown
# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 3.x.x   | :white_check_mark: |
| 2.x.x   | :white_check_mark: |
| 1.x.x   | :x:                |

## Reporting a Vulnerability

Please report security vulnerabilities to: security@nexus-platform.io

### Response Timeline

- **Initial Response**: Within 24 hours
- **Triage**: Within 72 hours
- **Fix Development**: Based on severity
  - Critical: 7 days
  - High: 14 days
  - Medium: 30 days
  - Low: 90 days

### Disclosure Policy

We follow coordinated disclosure. Please allow 90 days before public disclosure.

## Security Best Practices

1. Always use the latest stable version
2. Enable secure boot
3. Use hardware-backed key storage
4. Keep certificates up to date
5. Monitor security advisories
```

---

## 15. 功能安全

### 15.1 功能安全概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     FUNCTIONAL SAFETY FRAMEWORK                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    STANDARDS COMPLIANCE                          │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │IEC 61508 │ │ISO 26262 │ │IEC 62443 │ │ MISRA C  │           │   │
│  │  │ (通用)   │ │ (汽车)   │ │ (工业)   │ │ (编码)   │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    SAFETY MECHANISMS                             │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 内存保护  │ │ 栈监控   │ │ 看门狗   │ │ 故障注入  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    VERIFICATION & VALIDATION                     │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 静态分析  │ │ 动态测试  │ │ 覆盖率   │ │ 追溯矩阵  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 15.2 MISRA C 合规

#### 15.2.1 MISRA C:2012 规则分类

| 类别 | 规则数 | 合规要求 |
|------|--------|----------|
| **强制 (Mandatory)** | 10 | 100% 合规 |
| **必需 (Required)** | 101 | 100% 合规 |
| **建议 (Advisory)** | 32 | ≥ 95% 合规 |

#### 15.2.2 关键 MISRA 规则

| 规则 | 描述 | 示例 |
|------|------|------|
| Rule 1.3 | 无未定义行为 | 避免空指针解引用 |
| Rule 10.4 | 算术运算类型一致 | 避免隐式类型转换 |
| Rule 11.3 | 指针类型转换限制 | 避免不安全的强制转换 |
| Rule 14.3 | 控制表达式不变 | 避免恒真/恒假条件 |
| Rule 17.7 | 检查返回值 | 不忽略函数返回值 |
| Rule 21.3 | 禁用动态内存 | 不使用 malloc/free |

#### 15.2.3 MISRA 检查配置

```yaml
# .misra.yml
rules:
  mandatory:
    enabled: true
    treat_as_error: true

  required:
    enabled: true
    treat_as_error: true
    deviations:
      - rule: "Rule 11.5"
        reason: "Required for HAL implementation"
        approved_by: "Safety Team"
        date: "2026-01-12"

  advisory:
    enabled: true
    treat_as_error: false

suppressions:
  - file: "ext/*"
    rules: ["*"]
    reason: "Third-party code"
```

### 15.3 内存保护 (MPU)

#### 15.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SAFE-MPU-001 | 支持内存区域保护 | P0 | 非法访问触发异常 |
| SAFE-MPU-002 | 支持栈溢出检测 | P0 | 溢出时触发异常 |
| SAFE-MPU-003 | 支持代码执行保护 | P0 | 数据区不可执行 |
| SAFE-MPU-004 | 支持外设访问控制 | P1 | 限制外设访问权限 |
| SAFE-MPU-005 | 支持任务隔离 | P1 | 任务间内存隔离 |

#### 15.3.2 接口定义

```c
/**
 * \file            mpu_config.h
 * \brief           Memory Protection Unit Configuration
 */

#ifndef MPU_CONFIG_H
#define MPU_CONFIG_H

#include "safety_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           MPU region attributes
 */
typedef enum {
    MPU_ATTR_RO_NO_EXEC,        /**< Read-only, no execute */
    MPU_ATTR_RW_NO_EXEC,        /**< Read-write, no execute */
    MPU_ATTR_RO_EXEC,           /**< Read-only, executable */
    MPU_ATTR_RW_EXEC,           /**< Read-write, executable */
    MPU_ATTR_NO_ACCESS,         /**< No access */
    MPU_ATTR_DEVICE,            /**< Device memory */
} mpu_attr_t;

/**
 * \brief           MPU region configuration
 */
typedef struct {
    uint32_t base_addr;         /**< Region base address */
    uint32_t size;              /**< Region size */
    mpu_attr_t attr;            /**< Region attributes */
    bool enabled;               /**< Region enabled */
} mpu_region_config_t;

/**
 * \brief           Configure MPU region
 *
 * \param[in]       region: Region number (0-7)
 * \param[in]       config: Region configuration
 *
 * \return          SAFETY_OK on success
 */
safety_status_t mpu_configure_region(uint8_t region,
                                     const mpu_region_config_t* config);

/**
 * \brief           Enable MPU
 *
 * \return          SAFETY_OK on success
 */
safety_status_t mpu_enable(void);

/**
 * \brief           Configure stack guard region
 *
 * \param[in]       stack_bottom: Stack bottom address
 * \param[in]       guard_size: Guard region size
 *
 * \return          SAFETY_OK on success
 */
safety_status_t mpu_configure_stack_guard(uint32_t stack_bottom,
                                          uint32_t guard_size);

#ifdef __cplusplus
}
#endif

#endif /* MPU_CONFIG_H */
```

### 15.4 运行时检查

#### 15.4.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SAFE-RTC-001 | 支持断言检查 | P0 | 断言失败时安全停机 |
| SAFE-RTC-002 | 支持栈使用监控 | P0 | 检测栈使用高水位 |
| SAFE-RTC-003 | 支持 CRC 校验 | P0 | 关键数据完整性 |
| SAFE-RTC-004 | 支持程序流监控 | P1 | 检测异常执行流 |
| SAFE-RTC-005 | 支持时序监控 | P1 | 检测超时和死锁 |

#### 15.4.2 断言和错误处理

```c
/**
 * \file            safety_assert.h
 * \brief           Safety Assertions
 */

#ifndef SAFETY_ASSERT_H
#define SAFETY_ASSERT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Safety assertion macro
 *
 * \param[in]       expr: Expression to check
 *
 * \note            Triggers safe shutdown on failure
 */
#define SAFETY_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            safety_assert_failed(__FILE__, __LINE__, #expr); \
        } \
    } while (0)

/**
 * \brief           Safety assertion with message
 */
#define SAFETY_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            safety_assert_failed_msg(__FILE__, __LINE__, #expr, msg); \
        } \
    } while (0)

/**
 * \brief           Assertion failure handler
 *
 * \param[in]       file: Source file name
 * \param[in]       line: Line number
 * \param[in]       expr: Failed expression
 *
 * \note            This function does not return
 */
void safety_assert_failed(const char* file,
                          uint32_t line,
                          const char* expr) __attribute__((noreturn));

/**
 * \brief           Safe shutdown procedure
 *
 * \param[in]       reason: Shutdown reason code
 *
 * \note            This function does not return
 */
void safety_shutdown(uint32_t reason) __attribute__((noreturn));

/**
 * \brief           CRC-32 calculation
 *
 * \param[in]       data: Data buffer
 * \param[in]       length: Data length
 *
 * \return          CRC-32 value
 */
uint32_t safety_crc32(const void* data, size_t length);

/**
 * \brief           Verify data integrity
 *
 * \param[in]       data: Data buffer
 * \param[in]       length: Data length
 * \param[in]       expected_crc: Expected CRC value
 *
 * \return          true if CRC matches
 */
bool safety_verify_crc(const void* data, size_t length, uint32_t expected_crc);

#ifdef __cplusplus
}
#endif

#endif /* SAFETY_ASSERT_H */
```

### 15.5 故障注入测试

#### 15.5.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| SAFE-FIT-001 | 支持内存故障注入 | P1 | 位翻转、损坏 |
| SAFE-FIT-002 | 支持通信故障注入 | P1 | 丢包、延迟、损坏 |
| SAFE-FIT-003 | 支持时序故障注入 | P1 | 超时、抖动 |
| SAFE-FIT-004 | 支持电源故障模拟 | P2 | 掉电、欠压 |
| SAFE-FIT-005 | 支持故障恢复验证 | P1 | 验证恢复机制 |

#### 15.5.2 故障注入框架

```c
/**
 * \file            fault_injection.h
 * \brief           Fault Injection Framework
 */

#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include "safety_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Fault types
 */
typedef enum {
    FAULT_TYPE_MEMORY_CORRUPTION,   /**< Memory bit flip */
    FAULT_TYPE_COMM_PACKET_LOSS,    /**< Communication packet loss */
    FAULT_TYPE_COMM_DELAY,          /**< Communication delay */
    FAULT_TYPE_TIMING_JITTER,       /**< Timing jitter */
    FAULT_TYPE_WATCHDOG_TIMEOUT,    /**< Watchdog timeout */
} fault_type_t;

/**
 * \brief           Fault injection configuration
 */
typedef struct {
    fault_type_t type;              /**< Fault type */
    uint32_t probability;           /**< Probability (0-10000 = 0-100%) */
    uint32_t duration_ms;           /**< Fault duration */
    void* target;                   /**< Target address/handle */
} fault_config_t;

/**
 * \brief           Enable fault injection (TEST BUILDS ONLY)
 *
 * \param[in]       config: Fault configuration
 *
 * \return          SAFETY_OK on success
 *
 * \warning         Only available in test builds
 */
#ifdef NEXUS_FAULT_INJECTION_ENABLED
safety_status_t fault_inject_enable(const fault_config_t* config);
safety_status_t fault_inject_disable(fault_type_t type);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FAULT_INJECTION_H */
```

---

# 第四部分：性能与调试

## 16. 性能优化工具

### 16.1 性能分析架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      PERFORMANCE ANALYSIS TOOLS                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    CPU PROFILING                                 │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 函数耗时  │ │ 热点分析  │ │ 调用图   │ │ 中断分析  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    MEMORY ANALYSIS                               │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 堆使用   │ │ 栈使用   │ │ 泄漏检测  │ │ 碎片分析  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    POWER ANALYSIS                                │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 功耗建模  │ │ 电流曲线  │ │ 睡眠分析  │ │ 唤醒分析  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    REAL-TIME TRACING                             │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ ITM/ETM  │ │SystemView│ │Tracealyzer│ │ 自定义   │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 16.2 CPU 性能分析

#### 16.2.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| PERF-CPU-001 | 函数执行时间统计 | P0 | 精度 < 1μs |
| PERF-CPU-002 | 热点函数识别 | P0 | 自动排序 Top N |
| PERF-CPU-003 | 调用关系图生成 | P1 | 可视化调用链 |
| PERF-CPU-004 | 中断延迟分析 | P0 | 统计最大/平均延迟 |
| PERF-CPU-005 | CPU 利用率统计 | P0 | 按任务统计 |

#### 16.2.2 性能分析接口

```c
/**
 * \file            profiler.h
 * \brief           Performance Profiler Interface
 */

#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Profiler configuration
 */
typedef struct {
    bool enable_cpu_profiling;      /**< Enable CPU profiling */
    bool enable_memory_profiling;   /**< Enable memory profiling */
    bool enable_power_profiling;    /**< Enable power profiling */
    uint32_t sample_rate_hz;        /**< Sampling rate */
    uint32_t buffer_size;           /**< Trace buffer size */
} profiler_config_t;

/**
 * \brief           Function profile data
 */
typedef struct {
    const char* name;               /**< Function name */
    uint32_t call_count;            /**< Number of calls */
    uint32_t total_cycles;          /**< Total CPU cycles */
    uint32_t min_cycles;            /**< Minimum cycles per call */
    uint32_t max_cycles;            /**< Maximum cycles per call */
    uint32_t avg_cycles;            /**< Average cycles per call */
} profiler_func_stats_t;

/**
 * \brief           Initialize profiler
 *
 * \param[in]       config: Profiler configuration
 *
 * \return          0 on success
 */
int profiler_init(const profiler_config_t* config);

/**
 * \brief           Start profiling
 */
void profiler_start(void);

/**
 * \brief           Stop profiling
 */
void profiler_stop(void);

/**
 * \brief           Enter function (called at function entry)
 *
 * \param[in]       func_addr: Function address
 */
void profiler_enter(void* func_addr);

/**
 * \brief           Exit function (called at function exit)
 *
 * \param[in]       func_addr: Function address
 */
void profiler_exit(void* func_addr);

/**
 * \brief           Get function statistics
 *
 * \param[out]      stats: Statistics array
 * \param[in,out]   count: Array size / actual count
 *
 * \return          0 on success
 */
int profiler_get_stats(profiler_func_stats_t* stats, size_t* count);

/**
 * \brief           Print profiling report
 */
void profiler_print_report(void);

/**
 * \brief           Profiling macros
 */
#ifdef NEXUS_PROFILING_ENABLED
    #define PROFILE_FUNC_ENTER() profiler_enter((void*)__func__)
    #define PROFILE_FUNC_EXIT()  profiler_exit((void*)__func__)
#else
    #define PROFILE_FUNC_ENTER()
    #define PROFILE_FUNC_EXIT()
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROFILER_H */
```

### 16.3 内存分析

#### 16.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| PERF-MEM-001 | 堆使用统计 | P0 | 当前/峰值/总分配 |
| PERF-MEM-002 | 栈使用分析 | P0 | 每任务栈高水位 |
| PERF-MEM-003 | 内存泄漏检测 | P0 | 检测未释放内存 |
| PERF-MEM-004 | 碎片率分析 | P1 | 计算碎片百分比 |
| PERF-MEM-005 | 分配追踪 | P1 | 记录分配调用栈 |

#### 16.3.2 内存分析接口

```c
/**
 * \file            memory_analyzer.h
 * \brief           Memory Analysis Interface
 */

#ifndef MEMORY_ANALYZER_H
#define MEMORY_ANALYZER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Heap statistics
 */
typedef struct {
    size_t total_size;              /**< Total heap size */
    size_t used_size;               /**< Currently used size */
    size_t peak_used;               /**< Peak used size */
    size_t free_size;               /**< Free size */
    size_t largest_free_block;      /**< Largest free block */
    uint32_t alloc_count;           /**< Total allocations */
    uint32_t free_count;            /**< Total frees */
    uint32_t fragmentation_pct;     /**< Fragmentation percentage */
} heap_stats_t;

/**
 * \brief           Stack statistics
 */
typedef struct {
    const char* task_name;          /**< Task name */
    size_t stack_size;              /**< Total stack size */
    size_t stack_used;              /**< Used stack size */
    size_t stack_free;              /**< Free stack size */
    uint32_t high_water_mark;       /**< High water mark */
} stack_stats_t;

/**
 * \brief           Memory leak record
 */
typedef struct {
    void* address;                  /**< Allocated address */
    size_t size;                    /**< Allocation size */
    const char* file;               /**< Source file */
    uint32_t line;                  /**< Source line */
    uint32_t timestamp;             /**< Allocation time */
} leak_record_t;

/**
 * \brief           Get heap statistics
 *
 * \param[out]      stats: Heap statistics
 */
void memory_get_heap_stats(heap_stats_t* stats);

/**
 * \brief           Get stack statistics for all tasks
 *
 * \param[out]      stats: Stack statistics array
 * \param[in,out]   count: Array size / actual count
 */
void memory_get_stack_stats(stack_stats_t* stats, size_t* count);

/**
 * \brief           Check for memory leaks
 *
 * \param[out]      leaks: Leak records array
 * \param[in,out]   count: Array size / actual count
 *
 * \return          Number of leaks detected
 */
uint32_t memory_check_leaks(leak_record_t* leaks, size_t* count);

/**
 * \brief           Print memory report
 */
void memory_print_report(void);

/**
 * \brief           Tracked allocation (for leak detection)
 */
#ifdef NEXUS_MEMORY_TRACKING
    #define tracked_malloc(size) \
        tracked_malloc_impl(size, __FILE__, __LINE__)
    #define tracked_free(ptr) \
        tracked_free_impl(ptr, __FILE__, __LINE__)
#else
    #define tracked_malloc(size) malloc(size)
    #define tracked_free(ptr) free(ptr)
#endif

void* tracked_malloc_impl(size_t size, const char* file, uint32_t line);
void tracked_free_impl(void* ptr, const char* file, uint32_t line);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_ANALYZER_H */
```

### 16.4 功耗分析

#### 16.4.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| PERF-PWR-001 | 功耗状态统计 | P0 | 各状态时间占比 |
| PERF-PWR-002 | 电流曲线记录 | P1 | 与外部功耗仪集成 |
| PERF-PWR-003 | 睡眠效率分析 | P0 | 睡眠/唤醒时间比 |
| PERF-PWR-004 | 功耗预估 | P1 | 基于模型预估 |
| PERF-PWR-005 | 功耗优化建议 | P2 | 自动生成建议 |

#### 16.4.2 功耗分析接口

```c
/**
 * \file            power_analyzer.h
 * \brief           Power Analysis Interface
 */

#ifndef POWER_ANALYZER_H
#define POWER_ANALYZER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Power state
 */
typedef enum {
    POWER_STATE_RUN,                /**< Full run mode */
    POWER_STATE_SLEEP,              /**< Sleep mode */
    POWER_STATE_DEEP_SLEEP,         /**< Deep sleep mode */
    POWER_STATE_STANDBY,            /**< Standby mode */
} power_state_t;

/**
 * \brief           Power statistics
 */
typedef struct {
    uint32_t run_time_ms;           /**< Time in run mode */
    uint32_t sleep_time_ms;         /**< Time in sleep mode */
    uint32_t deep_sleep_time_ms;    /**< Time in deep sleep */
    uint32_t standby_time_ms;       /**< Time in standby */
    uint32_t wakeup_count;          /**< Number of wakeups */
    uint32_t avg_wakeup_time_us;    /**< Average wakeup time */
    float estimated_current_ua;     /**< Estimated average current */
} power_stats_t;

/**
 * \brief           Record power state transition
 *
 * \param[in]       new_state: New power state
 */
void power_record_state(power_state_t new_state);

/**
 * \brief           Get power statistics
 *
 * \param[out]      stats: Power statistics
 */
void power_get_stats(power_stats_t* stats);

/**
 * \brief           Estimate battery life
 *
 * \param[in]       battery_mah: Battery capacity in mAh
 *
 * \return          Estimated battery life in hours
 */
float power_estimate_battery_life(uint32_t battery_mah);

/**
 * \brief           Print power report
 */
void power_print_report(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_ANALYZER_H */
```

### 16.5 实时追踪

#### 16.5.1 支持的追踪工具

| 工具 | 说明 | 集成方式 |
|------|------|----------|
| **ITM/ETM** | ARM CoreSight 追踪 | 硬件 |
| **SystemView** | SEGGER 实时分析 | 软件插桩 |
| **Tracealyzer** | Percepio 追踪分析 | 软件插桩 |
| **自定义追踪** | 内置追踪系统 | 软件插桩 |

#### 16.5.2 追踪接口

```c
/**
 * \file            trace.h
 * \brief           Real-time Tracing Interface
 */

#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Trace event types
 */
typedef enum {
    TRACE_EVENT_TASK_SWITCH,        /**< Task context switch */
    TRACE_EVENT_ISR_ENTER,          /**< ISR entry */
    TRACE_EVENT_ISR_EXIT,           /**< ISR exit */
    TRACE_EVENT_MUTEX_LOCK,         /**< Mutex lock */
    TRACE_EVENT_MUTEX_UNLOCK,       /**< Mutex unlock */
    TRACE_EVENT_QUEUE_SEND,         /**< Queue send */
    TRACE_EVENT_QUEUE_RECEIVE,      /**< Queue receive */
    TRACE_EVENT_USER,               /**< User-defined event */
} trace_event_t;

/**
 * \brief           Initialize tracing
 *
 * \param[in]       buffer: Trace buffer
 * \param[in]       size: Buffer size
 */
void trace_init(void* buffer, size_t size);

/**
 * \brief           Start tracing
 */
void trace_start(void);

/**
 * \brief           Stop tracing
 */
void trace_stop(void);

/**
 * \brief           Record trace event
 *
 * \param[in]       event: Event type
 * \param[in]       param1: Event parameter 1
 * \param[in]       param2: Event parameter 2
 */
void trace_event(trace_event_t event, uint32_t param1, uint32_t param2);

/**
 * \brief           Record user event with string
 *
 * \param[in]       channel: User channel (0-15)
 * \param[in]       message: Event message
 */
void trace_user_event(uint8_t channel, const char* message);

/**
 * \brief           Export trace data
 *
 * \param[out]      buffer: Output buffer
 * \param[in,out]   size: Buffer size / actual size
 *
 * \return          0 on success
 */
int trace_export(void* buffer, size_t* size);

#ifdef __cplusplus
}
#endif

#endif /* TRACE_H */
```

### 16.6 基准测试

#### 16.6.1 内置基准测试

| 基准测试 | 说明 | 指标 |
|----------|------|------|
| **CoreMark** | CPU 性能 | CoreMark/MHz |
| **Dhrystone** | 整数运算 | DMIPS/MHz |
| **Whetstone** | 浮点运算 | MWIPS |
| **MemBench** | 内存带宽 | MB/s |
| **CryptoBench** | 加密性能 | MB/s |

---

## 17. 高级调试能力

### 17.1 调试架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DEBUG INFRASTRUCTURE                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    LOCAL DEBUG                                   │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ GDB/JTAG │ │ SWD/SWO  │ │ RTT      │ │ Semihost │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    REMOTE DEBUG                                  │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 网络调试  │ │ 云端调试  │ │ 远程日志  │ │ 远程Shell │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    POST-MORTEM                                   │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ Core Dump│ │ 崩溃日志  │ │ 回溯分析  │ │ 故障诊断  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 17.2 日志系统

#### 17.2.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| DBG-LOG-001 | 支持分级日志 | P0 | ERROR/WARN/INFO/DEBUG/TRACE |
| DBG-LOG-002 | 支持模块过滤 | P0 | 按模块启用/禁用 |
| DBG-LOG-003 | 支持异步日志 | P0 | 不阻塞主线程 |
| DBG-LOG-004 | 支持多输出 | P1 | UART/RTT/文件/网络 |
| DBG-LOG-005 | 支持时间戳 | P0 | 毫秒级精度 |
| DBG-LOG-006 | 支持颜色输出 | P2 | ANSI 颜色码 |

#### 17.2.2 日志接口

```c
/**
 * \file            logger.h
 * \brief           Logging System Interface
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Log levels
 */
typedef enum {
    LOG_LEVEL_NONE  = 0,            /**< No logging */
    LOG_LEVEL_ERROR = 1,            /**< Error messages */
    LOG_LEVEL_WARN  = 2,            /**< Warning messages */
    LOG_LEVEL_INFO  = 3,            /**< Informational messages */
    LOG_LEVEL_DEBUG = 4,            /**< Debug messages */
    LOG_LEVEL_TRACE = 5,            /**< Trace messages */
} log_level_t;

/**
 * \brief           Log output backends
 */
typedef enum {
    LOG_OUTPUT_UART     = (1 << 0), /**< UART output */
    LOG_OUTPUT_RTT      = (1 << 1), /**< SEGGER RTT output */
    LOG_OUTPUT_FILE     = (1 << 2), /**< File output */
    LOG_OUTPUT_NETWORK  = (1 << 3), /**< Network output */
    LOG_OUTPUT_BUFFER   = (1 << 4), /**< Ring buffer */
} log_output_t;

/**
 * \brief           Logger configuration
 */
typedef struct {
    log_level_t level;              /**< Global log level */
    uint32_t outputs;               /**< Output backends (bitmask) */
    bool async_mode;                /**< Enable async logging */
    bool color_enabled;             /**< Enable ANSI colors */
    bool timestamp_enabled;         /**< Enable timestamps */
    size_t buffer_size;             /**< Async buffer size */
} logger_config_t;

/**
 * \brief           Initialize logger
 *
 * \param[in]       config: Logger configuration
 *
 * \return          0 on success
 */
int logger_init(const logger_config_t* config);

/**
 * \brief           Set module log level
 *
 * \param[in]       module: Module name
 * \param[in]       level: Log level
 */
void logger_set_module_level(const char* module, log_level_t level);

/**
 * \brief           Log message
 *
 * \param[in]       level: Log level
 * \param[in]       module: Module name
 * \param[in]       file: Source file
 * \param[in]       line: Source line
 * \param[in]       fmt: Format string
 * \param[in]       ...: Format arguments
 */
void logger_log(log_level_t level,
                const char* module,
                const char* file,
                uint32_t line,
                const char* fmt, ...);

/**
 * \brief           Flush log buffer
 */
void logger_flush(void);

/**
 * \brief           Logging macros
 */
#define LOG_MODULE(name) static const char* LOG_TAG = name

#define LOG_E(fmt, ...) \
    logger_log(LOG_LEVEL_ERROR, LOG_TAG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) \
    logger_log(LOG_LEVEL_WARN, LOG_TAG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) \
    logger_log(LOG_LEVEL_INFO, LOG_TAG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) \
    logger_log(LOG_LEVEL_DEBUG, LOG_TAG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_T(fmt, ...) \
    logger_log(LOG_LEVEL_TRACE, LOG_TAG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
```

### 17.3 崩溃分析

#### 17.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| DBG-CRASH-001 | 支持 Core Dump | P0 | 保存寄存器和栈 |
| DBG-CRASH-002 | 支持调用栈回溯 | P0 | 完整调用链 |
| DBG-CRASH-003 | 支持故障原因分析 | P0 | 识别故障类型 |
| DBG-CRASH-004 | 支持持久化存储 | P1 | 重启后可读取 |
| DBG-CRASH-005 | 支持远程上报 | P1 | 上报到云端 |

#### 17.3.2 崩溃处理接口

```c
/**
 * \file            crash_handler.h
 * \brief           Crash Handler Interface
 */

#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Fault types
 */
typedef enum {
    FAULT_TYPE_HARD_FAULT,          /**< Hard fault */
    FAULT_TYPE_MEM_MANAGE,          /**< Memory management fault */
    FAULT_TYPE_BUS_FAULT,           /**< Bus fault */
    FAULT_TYPE_USAGE_FAULT,         /**< Usage fault */
    FAULT_TYPE_ASSERT,              /**< Assertion failure */
    FAULT_TYPE_WATCHDOG,            /**< Watchdog timeout */
    FAULT_TYPE_STACK_OVERFLOW,      /**< Stack overflow */
} fault_type_t;

/**
 * \brief           CPU register snapshot
 */
typedef struct {
    uint32_t r0, r1, r2, r3;        /**< General registers */
    uint32_t r12;                   /**< R12 */
    uint32_t lr;                    /**< Link register */
    uint32_t pc;                    /**< Program counter */
    uint32_t psr;                   /**< Program status register */
    uint32_t sp;                    /**< Stack pointer */
    uint32_t msp;                   /**< Main stack pointer */
    uint32_t psp;                   /**< Process stack pointer */
} cpu_registers_t;

/**
 * \brief           Crash report
 */
typedef struct {
    uint32_t magic;                 /**< Magic number */
    uint32_t version;               /**< Report version */
    fault_type_t fault_type;        /**< Fault type */
    uint32_t fault_addr;            /**< Fault address */
    cpu_registers_t regs;           /**< CPU registers */
    uint32_t stack[64];             /**< Stack snapshot */
    uint32_t stack_depth;           /**< Stack depth */
    char task_name[16];             /**< Current task name */
    uint32_t timestamp;             /**< Crash timestamp */
    uint32_t uptime_ms;             /**< System uptime */
    char message[128];              /**< Crash message */
} crash_report_t;

/**
 * \brief           Backtrace entry
 */
typedef struct {
    uint32_t pc;                    /**< Program counter */
    const char* func_name;          /**< Function name (if available) */
    uint32_t offset;                /**< Offset in function */
} backtrace_entry_t;

/**
 * \brief           Initialize crash handler
 */
void crash_handler_init(void);

/**
 * \brief           Get last crash report
 *
 * \param[out]      report: Crash report
 *
 * \return          true if crash report exists
 */
bool crash_get_last_report(crash_report_t* report);

/**
 * \brief           Clear crash report
 */
void crash_clear_report(void);

/**
 * \brief           Get backtrace
 *
 * \param[out]      entries: Backtrace entries
 * \param[in]       max_entries: Maximum entries
 *
 * \return          Number of entries
 */
uint32_t crash_get_backtrace(backtrace_entry_t* entries, uint32_t max_entries);

/**
 * \brief           Print crash report
 *
 * \param[in]       report: Crash report
 */
void crash_print_report(const crash_report_t* report);

/**
 * \brief           Upload crash report to cloud
 *
 * \param[in]       report: Crash report
 *
 * \return          0 on success
 */
int crash_upload_report(const crash_report_t* report);

#ifdef __cplusplus
}
#endif

#endif /* CRASH_HANDLER_H */
```

### 17.4 远程调试

#### 17.4.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| DBG-RMT-001 | 支持网络 GDB | P1 | TCP/IP GDB Server |
| DBG-RMT-002 | 支持远程日志 | P0 | 实时日志流 |
| DBG-RMT-003 | 支持远程 Shell | P1 | 命令行交互 |
| DBG-RMT-004 | 支持变量监控 | P1 | 实时变量查看 |
| DBG-RMT-005 | 支持固件更新 | P0 | 远程 OTA |

### 17.5 硬件在环测试 (HIL)

#### 17.5.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| DBG-HIL-001 | 支持自动化测试 | P1 | 脚本化测试 |
| DBG-HIL-002 | 支持 GPIO 仿真 | P1 | 输入/输出仿真 |
| DBG-HIL-003 | 支持通信仿真 | P1 | UART/SPI/I2C |
| DBG-HIL-004 | 支持时序验证 | P1 | 时序精度 < 1μs |
| DBG-HIL-005 | 支持故障注入 | P2 | 硬件故障模拟 |

---

# 第五部分：生态与云端

## 18. 生态系统

### 18.1 包管理器

#### 18.1.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ECO-PKG-001 | 支持组件安装 | P0 | 一键安装 |
| ECO-PKG-002 | 支持版本管理 | P0 | 语义化版本 |
| ECO-PKG-003 | 支持依赖解析 | P0 | 自动解析依赖 |
| ECO-PKG-004 | 支持私有仓库 | P1 | 企业内部仓库 |
| ECO-PKG-005 | 支持离线安装 | P1 | 无网络环境 |

#### 18.1.2 包管理器设计

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PACKAGE MANAGER                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    CLI INTERFACE                                 │   │
│  │  nexus install <package>                                         │   │
│  │  nexus update <package>                                          │   │
│  │  nexus remove <package>                                          │   │
│  │  nexus search <keyword>                                          │   │
│  │  nexus list                                                      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    PACKAGE REGISTRY                              │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 官方仓库  │ │ 社区仓库  │ │ 企业仓库  │ │ 本地缓存  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    PACKAGE MANIFEST                              │   │
│  │  {                                                               │   │
│  │    "name": "nexus-sensor-bme280",                               │   │
│  │    "version": "1.2.0",                                          │   │
│  │    "dependencies": {                                             │   │
│  │      "nexus-hal": "^2.0.0",                                     │   │
│  │      "nexus-i2c": "^1.0.0"                                      │   │
│  │    }                                                             │   │
│  │  }                                                               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

#### 18.1.3 包清单格式 (nexus.json)

```json
{
  "name": "nexus-sensor-bme280",
  "version": "1.2.0",
  "description": "BME280 temperature/humidity/pressure sensor driver",
  "author": "Nexus Team <team@nexus-platform.io>",
  "license": "MIT",
  "repository": "https://github.com/nexus-platform/sensor-bme280",
  "keywords": ["sensor", "bme280", "temperature", "humidity", "pressure"],
  "platforms": ["stm32f4", "stm32h7", "esp32", "nrf52"],
  "dependencies": {
    "nexus-hal": "^2.0.0",
    "nexus-i2c": "^1.0.0"
  },
  "devDependencies": {
    "nexus-test": "^1.0.0"
  },
  "exports": {
    "include": "include/",
    "src": "src/"
  },
  "scripts": {
    "build": "cmake -B build && cmake --build build",
    "test": "ctest --test-dir build"
  }
}
```

### 18.2 CLI 工具

#### 18.2.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ECO-CLI-001 | 项目创建 | P0 | 模板化创建 |
| ECO-CLI-002 | 构建管理 | P0 | 一键构建 |
| ECO-CLI-003 | 烧录管理 | P0 | 自动检测设备 |
| ECO-CLI-004 | 调试启动 | P1 | 启动调试会话 |
| ECO-CLI-005 | 文档生成 | P1 | 生成 API 文档 |

#### 18.2.2 CLI 命令

```bash
# 项目管理
nexus new <project-name> --template=<template>
nexus init
nexus config

# 包管理
nexus install <package>[@version]
nexus update [package]
nexus remove <package>
nexus search <keyword>
nexus list

# 构建
nexus build [--platform=<platform>] [--release]
nexus clean
nexus rebuild

# 烧录和调试
nexus flash [--device=<device>]
nexus debug [--gdb]
nexus monitor [--port=<port>]

# 测试
nexus test [--coverage]
nexus bench

# 文档
nexus docs [--open]

# 其他
nexus info
nexus doctor
nexus upgrade
```

### 18.3 代码生成器

#### 18.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ECO-GEN-001 | 支持引脚配置 | P0 | 图形化配置 |
| ECO-GEN-002 | 支持时钟配置 | P0 | 时钟树可视化 |
| ECO-GEN-003 | 支持外设配置 | P0 | 外设参数配置 |
| ECO-GEN-004 | 支持代码生成 | P0 | 生成初始化代码 |
| ECO-GEN-005 | 支持项目导出 | P1 | 导出完整项目 |

### 18.4 项目模板

| 模板名称 | 说明 | 包含组件 |
|----------|------|----------|
| `minimal` | 最小系统 | HAL + OSAL |
| `blinky` | LED 闪烁 | HAL + GPIO |
| `uart-echo` | 串口回显 | HAL + UART |
| `freertos` | FreeRTOS 基础 | HAL + OSAL + FreeRTOS |
| `network` | 网络应用 | HAL + OSAL + LwIP + MQTT |
| `secure` | 安全应用 | HAL + OSAL + Security + TLS |
| `cloud-aws` | AWS IoT | HAL + OSAL + Cloud + AWS |
| `tinyml` | TinyML 应用 | HAL + OSAL + TFLite Micro |

---

## 19. 云端集成

### 19.1 云平台支持

| 云平台 | 支持状态 | 功能 |
|--------|----------|------|
| **AWS IoT Core** | ✅ 完全支持 | MQTT、Shadow、OTA、Jobs |
| **Azure IoT Hub** | ✅ 完全支持 | MQTT、Twin、DPS、OTA |
| **阿里云 IoT** | ✅ 完全支持 | MQTT、物模型、OTA |
| **Google Cloud IoT** | 🚧 开发中 | MQTT、Registry |
| **私有云** | ✅ 支持 | 自定义 MQTT Broker |

### 19.2 设备影子/数字孪生

#### 19.2.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| CLOUD-SHADOW-001 | 支持状态上报 | P0 | reported 状态 |
| CLOUD-SHADOW-002 | 支持期望状态 | P0 | desired 状态 |
| CLOUD-SHADOW-003 | 支持增量更新 | P0 | delta 通知 |
| CLOUD-SHADOW-004 | 支持离线同步 | P1 | 重连后同步 |
| CLOUD-SHADOW-005 | 支持版本控制 | P1 | 乐观锁 |

#### 19.2.2 设备影子接口

```c
/**
 * \file            device_shadow.h
 * \brief           Device Shadow Interface
 */

#ifndef DEVICE_SHADOW_H
#define DEVICE_SHADOW_H

#include "cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Shadow delta callback
 *
 * \param[in]       delta: Delta JSON string
 * \param[in]       context: User context
 */
typedef void (*shadow_delta_callback_t)(const char* delta, void* context);

/**
 * \brief           Initialize device shadow
 *
 * \param[in]       callback: Delta callback
 * \param[in]       context: User context
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t shadow_init(shadow_delta_callback_t callback, void* context);

/**
 * \brief           Update reported state
 *
 * \param[in]       reported: Reported state JSON
 *
 * \return          CLOUD_OK on success
 *
 * \code{.c}
 * shadow_update_reported("{\"temperature\": 25.5, \"humidity\": 60}");
 * \endcode
 */
cloud_status_t shadow_update_reported(const char* reported);

/**
 * \brief           Get desired state
 *
 * \param[out]      desired: Buffer for desired state
 * \param[in]       max_len: Buffer size
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t shadow_get_desired(char* desired, size_t max_len);

/**
 * \brief           Get full shadow document
 *
 * \param[out]      document: Buffer for shadow document
 * \param[in]       max_len: Buffer size
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t shadow_get_document(char* document, size_t max_len);

/**
 * \brief           Delete shadow
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t shadow_delete(void);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_SHADOW_H */
```

### 19.3 OTA 升级

#### 19.3.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| CLOUD-OTA-001 | 支持完整升级 | P0 | 全量固件下载 |
| CLOUD-OTA-002 | 支持差分升级 | P0 | 仅下载差异部分 |
| CLOUD-OTA-003 | 支持 A/B 分区 | P0 | 双分区切换 |
| CLOUD-OTA-004 | 支持回滚 | P0 | 升级失败回滚 |
| CLOUD-OTA-005 | 支持断点续传 | P1 | 网络中断恢复 |
| CLOUD-OTA-006 | 支持签名验证 | P0 | 固件签名校验 |
| CLOUD-OTA-007 | 支持进度上报 | P1 | 实时进度反馈 |

#### 19.3.2 OTA 升级流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          OTA UPDATE FLOW                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Check   │───▶│ Download │───▶│  Verify  │───▶│  Apply   │          │
│  │  Update  │    │ Firmware │    │Signature │    │  Update  │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│       │               │               │               │                  │
│       ▼               ▼               ▼               ▼                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │ Version  │    │ Progress │    │  Hash    │    │  Reboot  │          │
│  │  Check   │    │  Report  │    │  Check   │    │  Switch  │          │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘          │
│                                                       │                  │
│                                                       ▼                  │
│                                              ┌──────────────┐            │
│                                              │   Verify     │            │
│                                              │   Boot OK    │            │
│                                              └──────────────┘            │
│                                                       │                  │
│                              ┌─────────────────────────┼─────────────┐   │
│                              │                         │             │   │
│                              ▼                         ▼             │   │
│                       ┌──────────┐              ┌──────────┐         │   │
│                       │  Commit  │              │ Rollback │         │   │
│                       │  Update  │              │  to Old  │         │   │
│                       └──────────┘              └──────────┘         │   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

#### 19.3.3 OTA 接口

```c
/**
 * \file            ota_client.h
 * \brief           OTA Update Client Interface
 */

#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include "cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           OTA state
 */
typedef enum {
    OTA_STATE_IDLE,                 /**< Idle, no update */
    OTA_STATE_CHECKING,             /**< Checking for update */
    OTA_STATE_DOWNLOADING,          /**< Downloading firmware */
    OTA_STATE_VERIFYING,            /**< Verifying signature */
    OTA_STATE_APPLYING,             /**< Applying update */
    OTA_STATE_REBOOTING,            /**< Rebooting */
    OTA_STATE_COMPLETE,             /**< Update complete */
    OTA_STATE_ERROR,                /**< Update error */
} ota_state_t;

/**
 * \brief           OTA progress callback
 *
 * \param[in]       state: Current state
 * \param[in]       progress: Progress percentage (0-100)
 * \param[in]       context: User context
 */
typedef void (*ota_progress_callback_t)(ota_state_t state,
                                        uint8_t progress,
                                        void* context);

/**
 * \brief           OTA configuration
 */
typedef struct {
    const char* update_url;         /**< Update server URL */
    bool auto_check;                /**< Auto check for updates */
    uint32_t check_interval_sec;    /**< Check interval */
    bool auto_apply;                /**< Auto apply updates */
    ota_progress_callback_t callback; /**< Progress callback */
    void* callback_context;         /**< Callback context */
} ota_config_t;

/**
 * \brief           Initialize OTA client
 *
 * \param[in]       config: OTA configuration
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_init(const ota_config_t* config);

/**
 * \brief           Check for available update
 *
 * \param[out]      available: true if update available
 * \param[out]      version: New version string
 * \param[in]       version_len: Version buffer size
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_check_update(bool* available,
                                char* version,
                                size_t version_len);

/**
 * \brief           Start OTA update
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_start_update(void);

/**
 * \brief           Abort OTA update
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_abort(void);

/**
 * \brief           Commit update (mark as successful)
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_commit(void);

/**
 * \brief           Rollback to previous version
 *
 * \return          CLOUD_OK on success
 */
cloud_status_t ota_rollback(void);

/**
 * \brief           Get current OTA state
 *
 * \return          Current OTA state
 */
ota_state_t ota_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_CLIENT_H */
```

### 19.4 遥测数据上报

#### 19.4.1 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| CLOUD-TEL-001 | 支持实时上报 | P0 | 延迟 < 1s |
| CLOUD-TEL-002 | 支持批量上报 | P0 | 减少网络开销 |
| CLOUD-TEL-003 | 支持离线缓存 | P1 | 断网时缓存 |
| CLOUD-TEL-004 | 支持数据压缩 | P1 | 减少带宽 |
| CLOUD-TEL-005 | 支持 QoS 配置 | P0 | QoS 0/1/2 |

---

## 20. AI/ML支持

### 20.1 TinyML 架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          TINYML FRAMEWORK                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    APPLICATION LAYER                             │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 语音识别  │ │ 图像分类  │ │ 异常检测  │ │ 预测维护  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    INFERENCE ENGINE                              │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │              TensorFlow Lite Micro                        │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │                    CMSIS-NN                               │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    MODEL MANAGEMENT                              │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 模型加载  │ │ 模型量化  │ │ 模型优化  │ │ 模型更新  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    HARDWARE ACCELERATION                         │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ CMSIS-DSP│ │ NPU/DSP  │ │ SIMD     │ │ FPU      │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 20.2 功能需求

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| AI-001 | 支持 TFLite Micro | P0 | 完整推理支持 |
| AI-002 | 支持 CMSIS-NN | P0 | 硬件加速 |
| AI-003 | 支持模型量化 | P0 | INT8 量化 |
| AI-004 | 支持模型加载 | P0 | Flash/RAM 加载 |
| AI-005 | 支持模型更新 | P1 | OTA 模型更新 |
| AI-006 | 支持多模型 | P1 | 同时加载多模型 |

### 20.3 推理接口

```c
/**
 * \file            tinyml.h
 * \brief           TinyML Inference Interface
 */

#ifndef TINYML_H
#define TINYML_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Model handle
 */
typedef struct tinyml_model* tinyml_model_t;

/**
 * \brief           Tensor data type
 */
typedef enum {
    TINYML_TYPE_FLOAT32,            /**< 32-bit float */
    TINYML_TYPE_INT8,               /**< 8-bit signed integer */
    TINYML_TYPE_UINT8,              /**< 8-bit unsigned integer */
    TINYML_TYPE_INT16,              /**< 16-bit signed integer */
} tinyml_type_t;

/**
 * \brief           Tensor descriptor
 */
typedef struct {
    void* data;                     /**< Tensor data pointer */
    size_t size;                    /**< Data size in bytes */
    tinyml_type_t type;             /**< Data type */
    int32_t dims[4];                /**< Tensor dimensions */
    uint8_t num_dims;               /**< Number of dimensions */
    float scale;                    /**< Quantization scale */
    int32_t zero_point;             /**< Quantization zero point */
} tinyml_tensor_t;

/**
 * \brief           Load model from memory
 *
 * \param[in]       model_data: Model data (FlatBuffer)
 * \param[in]       model_size: Model size
 * \param[in]       arena: Tensor arena buffer
 * \param[in]       arena_size: Arena size
 * \param[out]      model: Model handle
 *
 * \return          0 on success
 */
int tinyml_load_model(const uint8_t* model_data,
                      size_t model_size,
                      uint8_t* arena,
                      size_t arena_size,
                      tinyml_model_t* model);

/**
 * \brief           Get input tensor
 *
 * \param[in]       model: Model handle
 * \param[in]       index: Input tensor index
 * \param[out]      tensor: Tensor descriptor
 *
 * \return          0 on success
 */
int tinyml_get_input(tinyml_model_t model,
                     uint32_t index,
                     tinyml_tensor_t* tensor);

/**
 * \brief           Get output tensor
 *
 * \param[in]       model: Model handle
 * \param[in]       index: Output tensor index
 * \param[out]      tensor: Tensor descriptor
 *
 * \return          0 on success
 */
int tinyml_get_output(tinyml_model_t model,
                      uint32_t index,
                      tinyml_tensor_t* tensor);

/**
 * \brief           Run inference
 *
 * \param[in]       model: Model handle
 *
 * \return          0 on success
 */
int tinyml_invoke(tinyml_model_t model);

/**
 * \brief           Unload model
 *
 * \param[in]       model: Model handle
 */
void tinyml_unload_model(tinyml_model_t model);

/**
 * \brief           Get inference time
 *
 * \param[in]       model: Model handle
 *
 * \return          Inference time in microseconds
 */
uint32_t tinyml_get_inference_time(tinyml_model_t model);

#ifdef __cplusplus
}
#endif

#endif /* TINYML_H */
```

### 20.4 支持的模型类型

| 模型类型 | 说明 | 典型应用 |
|----------|------|----------|
| **CNN** | 卷积神经网络 | 图像分类、目标检测 |
| **RNN/LSTM** | 循环神经网络 | 时序预测、语音识别 |
| **MLP** | 多层感知机 | 分类、回归 |
| **Autoencoder** | 自编码器 | 异常检测 |

### 20.5 性能指标

| 模型 | 大小 | 推理时间 | 内存占用 | 平台 |
|------|------|----------|----------|------|
| MobileNet V1 (0.25) | 250KB | 50ms | 64KB | STM32H7 |
| Keyword Spotting | 20KB | 10ms | 16KB | STM32F4 |
| Anomaly Detection | 10KB | 5ms | 8KB | STM32F4 |

---

# 第六部分：企业与合规

## 21. 国际化与合规

### 21.1 多语言文档

| 语言 | 状态 | 覆盖范围 |
|------|------|----------|
| **英语** | ✅ 完整 | 全部文档 |
| **中文** | ✅ 完整 | 全部文档 |
| **日语** | 🚧 进行中 | API 参考 |
| **韩语** | 📋 计划中 | - |
| **德语** | 📋 计划中 | - |

### 21.2 认证支持

| 认证 | 适用地区 | 支持状态 |
|------|----------|----------|
| **CE** | 欧盟 | ✅ 指南文档 |
| **FCC** | 美国 | ✅ 指南文档 |
| **CCC** | 中国 | ✅ 指南文档 |
| **TELEC** | 日本 | 🚧 开发中 |
| **KC** | 韩国 | 📋 计划中 |

### 21.3 安全认证

| 认证 | 说明 | 支持状态 |
|------|------|----------|
| **IEC 61508** | 功能安全 (通用) | ✅ SIL 2 |
| **ISO 26262** | 功能安全 (汽车) | 🚧 ASIL B |
| **IEC 62443** | 工业网络安全 | ✅ SL 2 |
| **PSA Certified** | 平台安全 | ✅ Level 1 |
| **SESIP** | 安全评估 | 🚧 进行中 |

### 21.4 开源合规

#### 21.4.1 License 扫描

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| COMP-LIC-001 | 自动 License 扫描 | P0 | 识别所有 License |
| COMP-LIC-002 | License 兼容性检查 | P0 | 检测冲突 |
| COMP-LIC-003 | NOTICE 文件生成 | P0 | 自动生成 |
| COMP-LIC-004 | License 报告 | P1 | 生成合规报告 |

#### 21.4.2 支持的 License

| License | 类型 | 兼容性 |
|---------|------|--------|
| MIT | 宽松 | ✅ 兼容 |
| Apache-2.0 | 宽松 | ✅ 兼容 |
| BSD-3-Clause | 宽松 | ✅ 兼容 |
| LGPL-2.1 | 弱 Copyleft | ⚠️ 需注意 |
| GPL-2.0 | 强 Copyleft | ❌ 不兼容 |

### 21.5 出口合规

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| COMP-EXP-001 | 加密算法分类 | P1 | EAR/ECCN 分类 |
| COMP-EXP-002 | 出口限制检查 | P1 | 自动检查 |
| COMP-EXP-003 | 合规文档 | P1 | 出口文档 |

---

## 22. 开发者体验

### 22.1 开发者门户

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DEVELOPER PORTAL                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    DOCUMENTATION                                 │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │ 快速入门  │ │ API 参考  │ │ 开发指南  │ │  教程    │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    INTERACTIVE                                   │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │在线仿真器 │ │ 代码示例  │ │ Playground│ │ 视频教程  │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    COMMUNITY                                     │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │  论坛    │ │  博客    │ │ GitHub   │ │  Discord │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 22.2 在线 Playground

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| DX-PLAY-001 | 浏览器代码编辑 | P1 | Monaco Editor |
| DX-PLAY-002 | 在线编译 | P1 | 云端编译 |
| DX-PLAY-003 | 仿真运行 | P1 | QEMU/Renode |
| DX-PLAY-004 | 示例加载 | P1 | 一键加载示例 |
| DX-PLAY-005 | 代码分享 | P2 | 生成分享链接 |

### 22.3 交互式教程

| 教程 | 难度 | 时长 | 内容 |
|------|------|------|------|
| **Hello World** | 入门 | 15 分钟 | LED 闪烁 |
| **UART 通信** | 入门 | 30 分钟 | 串口收发 |
| **RTOS 基础** | 中级 | 60 分钟 | 任务、同步 |
| **网络应用** | 中级 | 90 分钟 | MQTT 连接 |
| **安全启动** | 高级 | 120 分钟 | 签名验证 |
| **TinyML** | 高级 | 120 分钟 | 模型部署 |

### 22.4 示例库

| 类别 | 示例数量 | 说明 |
|------|----------|------|
| **基础** | 20+ | GPIO、UART、SPI、I2C |
| **RTOS** | 15+ | 任务、同步、队列 |
| **网络** | 10+ | TCP、MQTT、HTTP |
| **安全** | 10+ | TLS、加密、签名 |
| **云端** | 10+ | AWS、Azure、阿里云 |
| **AI/ML** | 5+ | 分类、检测、预测 |

---

## 23. 企业级特性

### 23.1 多租户支持

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ENT-MT-001 | 组织管理 | P1 | 创建/管理组织 |
| ENT-MT-002 | 团队管理 | P1 | 团队成员管理 |
| ENT-MT-003 | 项目隔离 | P1 | 项目间隔离 |
| ENT-MT-004 | 资源配额 | P2 | 资源使用限制 |

### 23.2 权限管理

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ENT-RBAC-001 | 角色定义 | P1 | 自定义角色 |
| ENT-RBAC-002 | 权限分配 | P1 | 细粒度权限 |
| ENT-RBAC-003 | 访问控制 | P1 | 代码/资源访问 |
| ENT-RBAC-004 | SSO 集成 | P1 | SAML/OIDC |

### 23.3 审计日志

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ENT-AUDIT-001 | 操作记录 | P1 | 所有操作记录 |
| ENT-AUDIT-002 | 日志查询 | P1 | 高级搜索 |
| ENT-AUDIT-003 | 日志导出 | P1 | CSV/JSON 导出 |
| ENT-AUDIT-004 | 日志保留 | P1 | 可配置保留期 |

### 23.4 私有部署

| 需求ID | 需求描述 | 优先级 | 验收标准 |
|--------|----------|--------|----------|
| ENT-PRIV-001 | 私有仓库 | P1 | 内网包仓库 |
| ENT-PRIV-002 | 私有文档 | P1 | 内网文档站 |
| ENT-PRIV-003 | 私有 CI/CD | P1 | 内网构建 |
| ENT-PRIV-004 | 离线支持 | P1 | 完全离线 |

### 23.5 商业支持

| 支持级别 | 响应时间 | 内容 |
|----------|----------|------|
| **社区版** | 最佳努力 | GitHub Issues |
| **专业版** | 24 小时 | 邮件支持 |
| **企业版** | 4 小时 | 专属支持 + 电话 |
| **关键任务** | 1 小时 | 7x24 + 现场支持 |

---

# 附录

## 24. 版本管理

### 24.1 版本号规范

采用 **语义化版本** (Semantic Versioning 2.0.0)：

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

| 版本号 | 变更类型 | 说明 |
|--------|----------|------|
| **MAJOR** | 不兼容的 API 变更 | 重大架构调整 |
| **MINOR** | 向后兼容的功能新增 | 新增功能 |
| **PATCH** | 向后兼容的问题修复 | Bug 修复 |

### 24.2 发布周期

| 版本类型 | 周期 | 说明 |
|----------|------|------|
| **Major** | 12-18 个月 | 重大版本 |
| **Minor** | 3 个月 | 功能版本 |
| **Patch** | 按需 | 修复版本 |
| **Security** | 立即 | 安全修复 |

---

## 25. 附录

### 25.1 参考文档

| 文档 | 链接 | 说明 |
|------|------|------|
| C11 标准 | ISO/IEC 9899:2011 | C 语言标准 |
| MISRA C:2012 | MISRA | 嵌入式 C 编码规范 |
| IEC 61508 | IEC | 功能安全标准 |
| ISO 26262 | ISO | 汽车功能安全 |
| CMSIS | ARM | Cortex-M 软件接口 |
| FreeRTOS | freertos.org | RTOS 参考 |
| TensorFlow Lite | tensorflow.org | TinyML 框架 |
| AWS IoT | aws.amazon.com | 云平台参考 |

### 25.2 术语表

| 术语 | 全称 | 定义 |
|------|------|------|
| ADC | Analog-to-Digital Converter | 模数转换器 |
| API | Application Programming Interface | 应用程序接口 |
| BSP | Board Support Package | 板级支持包 |
| CI/CD | Continuous Integration/Deployment | 持续集成/部署 |
| DMA | Direct Memory Access | 直接内存访问 |
| GPIO | General Purpose Input/Output | 通用输入输出 |
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| HSM | Hardware Security Module | 硬件安全模块 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| MCU | Microcontroller Unit | 微控制器 |
| MISRA | Motor Industry Software Reliability Association | 汽车工业软件可靠性协会 |
| MPU | Memory Protection Unit | 内存保护单元 |
| OSAL | Operating System Abstraction Layer | 操作系统抽象层 |
| OTA | Over-The-Air | 空中升级 |
| RTOS | Real-Time Operating System | 实时操作系统 |
| SBOM | Software Bill of Materials | 软件物料清单 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| TDD | Test-Driven Development | 测试驱动开发 |
| TEE | Trusted Execution Environment | 可信执行环境 |
| TinyML | Tiny Machine Learning | 微型机器学习 |
| TLS | Transport Layer Security | 传输层安全 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发器 |

### 25.3 修订历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| 1.0.0 | 2025-06-01 | Nexus Team | 初始版本 |
| 2.0.0 | 2026-01-12 | Nexus Team | 多构建系统、TDD、跨平台、DevOps |
| 3.0.0 | 2026-01-12 | Nexus Team | 安全体系、功能安全、性能工具、云端集成、AI/ML、企业特性 |

---

## 文档结束

本文档定义了 Nexus 嵌入式软件开发平台的完整需求规范，涵盖：

- **核心平台**：HAL、OSAL、中间件、接口规范、代码规范
- **开发工具链**：文档体系、构建系统、测试体系、DevOps、跨平台
- **安全与可靠性**：安全启动、加密引擎、TLS、密钥管理、SBOM、功能安全、MISRA
- **性能与调试**：CPU/内存/功耗分析、实时追踪、日志系统、崩溃分析
- **生态与云端**：包管理器、CLI工具、云平台集成、OTA、TinyML
- **企业与合规**：国际化、认证、开源合规、权限管理、审计

如有问题或建议，请联系 Nexus 架构团队。

---

*Copyright © 2026 Nexus Team. All rights reserved.*
