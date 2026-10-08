# Nexus 嵌入式平台

[English](README.md) | 中文

Nexus 面向工业控制与设备联网，以公共 HAL/OSAL、独立 SoC/board/product 配置和可追溯的开发交付流程组织嵌入式产品。当前维护基线优先 Linux Native 软件验证与 STM32F407VG / STM32F4DISCOVERY 参考组合，覆盖裸机和 FreeRTOS。

| 组合 | 实现与证据 | 边界 |
|---|---|---|
| Linux Native / GCC | 完整目标编译、契约与应用测试 | 主机模型，不代表硬件结果 |
| FreeRTOS / POSIX port | 锁定的真实 kernel 与 OSAL 契约执行 | 不验证 Cortex-M 中断与时序 |
| STM32F407VG / MB997 | GPIO、SPI/DMA/IRQ、Flash 分区与启动装配，真实 SDK 头验证 | ARM 完整链接与上板验证待执行 |
| GD32 | 独立移植约束与候选描述 | 尚无 SDK/具体板验证；配置明确拒绝 |
| Windows / macOS | 保留 Native 构建预设 | 本次没有执行该平台验证 |

支持升级依据见 [支持矩阵](docs/strategy/support-matrix.yaml)。当前没有企业支持或 LTS 承诺。

## 构建与验证

需要 CMake 3.21+、Python 3.11+、C11/C++17 编译器与 OpenSSL 3 开发包。ARM 还需完整 ARM GCC/newlib 工具链。配置过程不联网获取依赖；先显式初始化锁定的子模块。

```sh
git clone https://github.com/X-Gen-Lab/nexus.git
cd nexus
python -m pip install kconfiglib==14.1.0
git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel 4
ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
python -m unittest discover -s scripts/ci -p 'test_*.py'
```

Linux 可用系统包安装 GCC、CMake、Ninja 与 libssl-dev。其他宿主预设见 cmake --list-presets；本次执行证据不能用于认证未运行的平台。

每个 build 目录独享 generated/effective.config、nexus_config.h、config.cmake。预设选择编译器和构建模式；配置 fragment 选择设备与资源。源码根 .config 和生成头不再参与构建。未知、矛盾、越界及生成失败会阻断，不保留旧 CONFIG 缓存继续假成功。

STM32 候选构建：

```sh
cmake --preset stm32-armgcc-release
cmake --build --preset stm32-armgcc-release --parallel 4
cmake --preset stm32-armgcc-freertos-release
cmake --build --preset stm32-armgcc-freertos-release --parallel 4
```

固件、map、bin、hex 输出至对应 build 目录的 bin。上板前核对芯片、板级修订、供电、Flash 分区与有效配置。本环境没有完成 ARM 链接或 HIL，以上命令是维护入口。

## 平台契约

- HAL 临界区按 CPU 选择；资源和等待对象拥有明确生命周期、句柄世代、超时、ISR 和回收规则。
- STM32 SPI 分离 bus、不可变 device 和 transaction，使用有界异步队列；取消/超时先停止并 drain DMA，再结束缓冲所有权。
- 裸机 OSAL 不伪装调度器；任务、事件和软件定时器等不支持能力明确拒绝。产品用主循环与板级单调时钟组织控制。
- 持久化使用真实 Flash port 的双银行原子快照；Native 使用跨进程可恢复的文件 Flash 模型。RAM 不被称为持久化。
- Config 由一个管理 owner 串行访问；AES-GCM 认证记录使用维护中的密码 provider。MCU 未绑定 provider/熵源时返回 unsupported。
- 安全更新包含镜像策略、认证端口和可恢复 trial/confirm/rollback 元数据；产品启动器、安全 vault 与实板证据仍需接入。

```sh
build/linux-gcc-debug/bin/blinky --cycles 3
cmake --preset native-services-debug
cmake --build --preset native-services-debug --parallel 4
build/native-services-debug/bin/freertos_demo --run-ms 500
```

详细接口和已执行证据见 [构建](docs/implementation/build-config.md)、[平台驱动](docs/implementation/platform-drivers.md)、[存储与安全](docs/implementation/storage-security.md)、[更新](docs/implementation/update.md)和[参考应用](docs/implementation/reference-applications.md)。故障注入是软件证据，不能替代真实断电、电气与控制期限测量。

## 企业开发管理

[架构与工作流方案](docs/strategy/README.md)定义约 10 人职责配置、3–6 月路线、需求到测试映射、风险评审、依赖与发布门禁。[任务清单](docs/strategy/backlog.csv)记录实现状态和外部阻碍；计划和已验证能力分别列明。

候选发布必须绑定相同源提交、有效配置、依赖、工件摘要和非零测试结果。生产发布还需要对应 board 的 HIL、签名身份、资源/实时预算与制造证据，候选不能在缺证据时冒充正式版本。

请阅读 [AGENTS.md](AGENTS.md) 后修改公共接口、持久格式或构建图。遵循 [MIT 许可证](LICENSE)，第三方组件遵循各自许可证。
