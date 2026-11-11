# Nexus 嵌入式通用平台

Nexus 为工业 MCU 提供 C11 通用机制：Core、Arch、类型化 I/O、静态 factory、可选 OS
适配和器件组件。产品任务、业务协议、私有 PCB、Flash 分区与恢复策略由外部工程维护。

当前生产 SoC 入口为 `soc/stm32/stm32f4/`，精确器件事实已覆盖
**STM32F405ZGT6／F407VET6／F407ZGT6／F415ZGT6／F417ZGT6**。
四个实体 Board 包仍为 F407；新增型号的内部 CRC 软件参考不授予其他外设组合或
PCB 资格。实际模式与执行边界见[支持范围](docs/delivery/support.md)。

源码版本号为 **0.2.1**，发布身份以 [GitHub Releases](https://github.com/X-Gen-Lab/nexus/releases)
为准。本次重构不重标已有标签、软件候选或硬件证据。当前重构源码仍需完成各自验收，
MCU 实板 HIL 保持 `not_executed`。

## 架构与源码入口

![FIG-01：平台职责与外部产品边界](docs/assets/diagrams/platform-architecture.svg)

图表达通用职责；本轮具体 MCU 实现只有 F4 包。运行时不按图逐层转发，应用取得
接口后通过 `ops(context, ...)` 进入选定实现。图例见[图册](docs/assets/diagrams/README.md)。

| 位置 | 责任 |
|---|---|
| [Core](core/README.md) | 结果、单调 deadline、请求与存储借用 |
| [Arch](arch/README.md) | CPU 原语、中断保存恢复及明确选入的能力 |
| [STM32F4](soc/stm32/stm32f4/README.md) | 启动、时钟、资源、HAL／LL 适配与私有控制器状态 |
| [Board](boards/README.md) | 精确料号、晶振、PCB 接线、电气条件及未知项 |
| [I/O](io/README.md) | 公共接口、检查分派和硬件无关机制 |
| [OS](os/README.md) | 可选裸机、FreeRTOS、ThreadX 与主机测试后端 |
| [组件](components/README.md) | 通过窄端口访问设备的协议与可选机制 |
| [工具](tools/README.md)／[CMake](cmake/README.md) | 配置、构建、测试与交付 |
| [测试](tests/README.md) | 真实生产 C、工具边界、ARM 链接与独立 HIL 范围 |

原顶层共享硬件 IP 已归入 F4 包，文件位置不增加运行时中间层。Native 位于
`tests/support/native/`，仅在显式测试 scope 下使用，不是另一个生产 SoC。

## 配置、生成与运行

```text
schema-2 TOML + SoC／Board JSON
    → 严格校验与资源计划 → 不可变 typed IR → 原子 bundle
    → CMake 选择源码 → ELF／BIN／map

运行：typed factory → 已有只读 face → ops(context) → F4 实现
```

factory 不分配、不初始化、不探测硬件；新工程使用 `nexus_factory_strong.h`。
CMake 拥有唯一源码和链接图。当前配置工具仍集中在 `tools/configure/` 使用主机
Python，负责配置校验与静态连接；固件中的硬件算法和生命周期直接使用 C。

基础固件保持静态定容和无默认堆。平台不启动 scheduler，也不创建产品 worker；
应用提供任务、缓冲区、执行者、service 进度与停机策略。

## 构建与验证

以下命令从完整仓库根执行，先准备锁定工具与依赖。首次准备可联网，日常构建不隐式
下载未选 SDK：

```sh
python tools/dev/dev.py prepare --scope preset --preset stm32-qiming-baremetal --allow-network
python tools/dev/dev.py configure --preset stm32-qiming-baremetal
python tools/dev/dev.py build --preset stm32-qiming-baremetal --parallel 4
```

主机测试使用声明了 `NEXUS_TEST_SUPPORT=ON` 的 Native preset：

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

GoogleTest／GoogleMock 只进入主机测试，固件保持 C11／必要汇编。安装、格式、注释、
TDD 和提交门禁见[工程手册](docs/design/engineering-handbook.md)。检查本次命令退出码和
新报告；模型、ARM 链接、SDK 消费与实板结果分别验收。

## 所有权与交付边界

ACCEPTED 后持续借用请求和 buffer，直到 acquire 观察 SETTLED；cancel、timeout 和
QUARANTINED 不授权复用存储。DMA 完成、线上完成和引用排空分别观察；adapter、
通知与观察者还需独立 join。权威合同见[所有权](docs/explanation/ownership.md)。

[Source SDK](docs/tutorials/source-sdk.md) 是按选择导出的可移位源码，不是全部 MCU
通用二进制。仓库相对文档以完整 checkout 为上下文；SDK 文档按 manifest 的 source
commit 阅读，精确 host-support payload 不代表打包全部测试。
[示例仓库](https://github.com/X-Gen-Lab/nexus-examples)为私有仓库，需要独立权限。
