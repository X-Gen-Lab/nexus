---
id: delivery.architecture
type: explanation
audience: maintainer
source_language: zh_CN
---

# 当前系统架构

Nexus 提供静态 C11 通用机制。当前生产 SoC 入口只有 `soc/stm32/stm32f4/`，现有实现
限定 F407VE／ZG；系列复用与精确型号验证分开。厂商依赖全部保留，只有实际选中
闭包进入准备、校验、编译与 SDK。

![FIG-01：平台职责与外部产品边界](../assets/diagrams/platform-architecture.svg)

图表达通用职责和构建／调用关系，本轮 MCU 入口为 F4。可选 OS 与组件不是每次 I/O
的强制中间层；物理验收另行取得。

## 目录、状态与资源

| 位置 | 责任与所有者 |
|---|---|
| `core/` | 结果、单调 deadline、请求与借用规则、整平台冷态协调 |
| `arch/` | 编译期 CPU 原语、中断状态保存恢复和明确能力 |
| `io/` | 公共类型、检查分派与硬件无关机制 |
| `soc/stm32/stm32f4/` | 精确芯片事实、启动、时钟、IRQ／DMA、私有控制器状态与 HAL／LL 适配 |
| `soc/stm32/stm32f4/drivers/ip/` | 仍被 F4 使用的私有硬件算法，无额外运行时注册 |
| `boards/` | 四个 F407 板卡的实际接线、晶振、安全电平及未知项 |
| `os/` | 可选任务、等待、通知及明确后端合同 |
| `components/` | 器件协议、owner、存储、网络等显式可选机制 |
| `tests/support/native/` | 显式测试模型、Board 与装配，不是生产 SoC |
| `tools/`、`cmake/` | 配置、唯一源码图、测试、SDK 与交付 |
| 外部产品 | 任务、队列、预算、私有分区、恢复与安全政策 |

完整导航见[源码定位](../explanation/source-map.md)。顶层共享 MCU `drivers/` 已归入
F4，目录移动本身不改变指令数或资源成本。

## 构建与运行

一份 schema-2 TOML 和只读 SoC／Board JSON 经严格校验、资源计划和不可变 typed IR
生成原子 bundle。CMake 选择已声明的实际源码与链接依赖；生成器不维护平行源码图。

每个固件选择精确器件宏和启动文件。厂商头、Handle 和可变 provider 状态私有；
生成器按选中实例定容配置、状态、buffer、只读 face 与强类型 factory。

运行中 factory 只返回已有接口。日常 I/O 通过 `ops(context, ...)` 直接进入 F4
实现，硬件操作优先复用官方 HAL／LL。不会因共享源码引入最大池、动态注册表、
默认堆或隐藏 worker。应用负责串行执行者、服务进度和 scheduler 启动。

## 生命周期与失败

F4 C 实现维护资源准入、取得、收条、硬件撤回和 readback。部分取得失败必须处理
当前步骤已经产生的效果，再逆序撤回前序步骤；失败释放保留实际责任。

默认静态装配通过 Core 私有 `platform_lifecycle_impl.h` 使用手写 C 协调器。
公共 `nx_platform_lifecycle_start/stop` 以运行期参数调用同一实现。固定装配的配置、
可选 owner 和诊断在同一编译单元可见，编译器可以裁剪恒定分支和不再需要的回调表；
产品仅包含公共接口，不增加运行时转发层或第二份算法。
全部 controller 准入先于硬件效果；随后依次建立 System、共享资源与 controller。
失败按逆序撤回，在首个释放拒绝处保留剩余责任，后续 stop 只重试未完成部分。
primary、cleanup、remaining effects 和调用期诊断独立保留，不以清理成功覆盖原始失败。
F4 的 System／DMA／SYSCFG／pins／CAN／Ethernet 共享责任由其私有 C 协调器处理。

ACCEPTED 后借用请求与 buffer，直到 acquire 观察 SETTLED。DMA 完成、线上完成、
IRQ／callback 撤回和最终发布分别证明；timeout／cancel 不提前释放借用。
QUARANTINED 保留存储与恢复责任，观察者和 adapter 还需独立 join。

启动外部 SRAM、已启 watchdog 和保留 RTC 状态不能套用普通关闭。HAL 与自写
实现也不能各自独立推进同一个 live controller 行为域。

## 当前实现与后续边界

Board／SoC 不再保留 Python 插件。默认主机工具仍使用 Python 校验与生成静态对象、
只读 plan、按索引选择的固定 typed call 和薄 IRQ 入口；启动、停止、回滚与诊断控制流
由手写 C 维护。既有 `nx_lifecycle_*` 可撤销资源事务与整平台协调器职责不同，不要求
为统一外观把全部驱动改接同一组 ops。

协调状态保留身份、16-bit 前缀和重入保护，M4 ABI 为 8 字节；超过 65535 个物理
controller 的 plan 在任何 owner 回调前拒绝。它不改变日常 I/O 热路径；实际资源、
模型行为、ARM 链接和
物理测量各自验收，不能从源码迁移推导整平台通过。

其他 F4 型号需要精确内存、封装、时钟、DMA、IRQ 与硬件 profile；官方 HAL 覆盖
不建立 Nexus 全系列资格。当前软件验证绑定本次执行，旧记录不继承；物理 HIL 为
`not_executed`。精确边界见[支持](support.md)与[资格](../reference/platforms/qualification.md)。
