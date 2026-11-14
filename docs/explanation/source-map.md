---
id: explanation.source-map
type: explanation
audience: maintainer
source_language: zh_CN
---

# 当前源码与维护边界

本页定位当前实现，不复制各目录的完整合同。生产 MCU 入口只有 STM32F4，精确
器件与模式以[支持范围](../delivery/support.md)为准；F407 Board 与新增料号的软件
参考分别说明。其他 SDK 原位保留，目录、实现、执行记录和物理资格不相互替代。

```text
core/                         结果、时间、请求与整平台冷态协调
arch/arm/cortex_m/            编译期 CPU 原语
arch/native/                  主机测试 CPU 端口
io/                           公共接口与硬件无关机制
soc/stm32/stm32f4/             F4 系列唯一生产 provider
  package.json                身份、事实、binding_files、构建入口与导出归属
  bindings.json / bindings/    公共领域能力与私有静态绑定数据
  drivers/ip/                 仍被 F4 使用的共享硬件实现
  cmake/                      家族选编与官方库私有配置
  private/                    厂商类型、Handle 与私有状态
boards/                       四个现有 F407 板卡包
components/                   可选器件、协议和公共机制
tools/configure/              通用包／领域政策与有限静态装配
tests/support/native/         显式主机测试模型与装配
vendors/                      全部锁定来源；按实际选择消费
```

| 维护问题 | 权威入口 |
|---|---|
| 请求、deadline 与借用 | [Core](../../core/README.md) |
| CPU 原语与上下文 | [Arch](../../arch/README.md) |
| 公共 face、ops 与通用机制 | [I/O](../../io/README.md) |
| F4 启动、时钟和外设实现 | [F4 支持包](../../soc/stm32/stm32f4/README.md) |
| PCB 接线与电气条件 | [Board](../../boards/README.md) |
| 等待、任务和内核责任 | [OS](../../os/README.md) |
| 外部器件与可选机制 | [组件](../../components/README.md) |
| 包、binding 与接入边界 | [接入指南](../guides/provider-porting.md)、[package loader](../../tools/configure/packages.py) |
| 资料身份与已审适用范围 | 包内 `review_profiles.json`、[ProfileSet](../../tools/configure/reference_profiles.py) |
| 领域与私有值 | [DomainPolicies](../../tools/configure/domain_policies.py)、[IR](../../tools/configure/ir.py) |
| 静态对象与 IRQ／endpoint 连接 | [StaticRecipe／StaticObjects](../../tools/configure/static_recipe.py)、[SharedIrq](../../tools/configure/shared_irq.py)、[ChildGroups](../../tools/configure/static_endpoints.py) |
| 单层声明和值片段 | [StaticFragments](../../tools/configure/static_fragments.py) |
| 配置与构建 | [工具](../../tools/README.md)、[CMake](../../cmake/README.md) |
| 模型、链接与执行证据 | [测试](../../tests/README.md)、[交付边界](../delivery/README.md) |

顶层共享 MCU `drivers/` 已归入 F4，不增加运行时转发。ILI9341、XPT2046、W25
继续通过公共 I/O 访问设备，留在组件目录，不随 MCU 支持包迁移。

Board 和 SoC 不加载本地 Python 插件。声明式包由通用 DataProvider 读取同一快照的
事实和显式 binding 文件，DomainPolicies 决定已维护公共语义与私有投影，有限静态
节点生成配置、存储、face、IRQ 与固定生命周期连接。CMake 仍拥有唯一 C 源码图，
SDK 只按选择的包导出归属收集源码。结构图见
[FIG-03 构建期输入与产物](../assets/diagrams/configuration-generation.svg)，
详细字段与迁移边界由[接入指南](../guides/provider-porting.md)唯一维护。

生产默认路径已采用通用 DataProvider，旧 F4 家族 Python 构造模块和导入注册表已
删除。包事实、策略、各模式和消费者仍须本批集成验证；不能从删除旧代码推定整体
资格。主机 Python 不进入固件，硬件算法和执行顺序由 C 维护。

Core `private/platform_lifecycle_impl.h` 维护整平台启动、停止、回滚与诊断，F4
`private/stm32f4_platform_lifecycle_impl.h` 维护 System 与共享资源的 C 协调；各自的
`.c` 公共入口复用同一实现，固定装配让编译器看到常量并裁剪分支。主机只维护
一套配置实现，其代码不进入 MCU 镜像。实际生成 factory、模型与 ARM 产物
各自验证，不能把源码移动视为整条链路已经验收。
Native 必须显式启用测试 scope，不进入生产系列目录或能力聚合表。

精确支持见[支持参考](../reference/platforms/support.md)。重构前的目录与执行范围可从
[固定旧源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
追溯；旧记录保留原始身份，不建立本次重构的软件候选或实板资格。
