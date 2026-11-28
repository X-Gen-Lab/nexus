---
id: reference.contracts.package-bindings
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.package-bindings)=
# 声明式包与静态绑定合同

本页拥有包字段、快照、领域投影和静态连接的规则。逐步接入见[Provider指南](../../guides/provider-porting.md)，硬件算法由C11维护，CMake拥有唯一源码图。

## 包、事实与目录

```text
soc/<vendor>/<family>/
  package.json
  facts/
  system/
  drivers/<domain>/
  drivers/ip/
  private/
  sdk/
  CMakeLists.txt
boards/<board>/board.json
tools/configure/
```

这是当前 F4 的维护布局。binding 与其私有 C 类型和实现相邻；只有真实共享的
合同、片段与算法保留共享入口。跨家族提取需要相同编程模型的证据。运行调用保持
公共 face 到具体 provider 的直接分派，见
[源码归属图](../../assets/diagrams/driver-source-ownership.svg)。

| package 字段 | 合同 |
| --- | --- |
| `schema_version`／`interface_version` | 独立格式与工具协议，未知版本拒绝 |
| `id`／`family`／`vendor` | 显式身份；重复不能靠搜索优先级覆盖 |
| `source_directory` | 相对 `package.json` 所在目录解析的所属源码根 |
| `facts` | `soc`／`routes`；按选定 provider 另给 `controllers`／`clocks`／`device_catalog`／`references`／`review_profiles`／`bindings` |
| `binding_files` | 显式有序输入列表，不能重复、越界或冒充其他权威输入 |
| `dependencies` | 引用既有依赖来源 ID，不另抄 URL、gitlink 或许可 |
| `cmake.entry`／`cmake.target` | 选中包的构建入口与 target，不列第二份 `.c` 图 |
| `exports.roots`／`exports.exclude` | 包拥有的生产导出范围，不是当前 ELF 的编译闭包 |
| `scope` | 生产或 reference；reference 必须显式启用 test support |

生产校验由 [packages.py](../../../tools/configure/packages.py)拥有，
[编辑器 schema](../../../tools/configure/soc-package.schema.json)从同一字段合同派生。
在仓库根执行下列只读命令可打印当前 JSON schema；输出不替代生产加载与资源校验。

```sh
python -m tools.configure.packages --schema
```

`facts`、`binding_files`、`cmake.entry` 和 `exports` 路径均相对已解析的
`source_directory`；加载器检查所属边界、文件类型与 symlink。选择声明式包不执行家族
Python 模块；CMake 与 C 仍是受信构建代码，哈希只绑定身份，不构成执行沙箱。

### 同一快照读取

manifest、SoC、route、binding 主文件和每个 `binding_files` 都经同一受控 reader
读取并记录字节身份。重复读取不得换成另一代输入，生成结束前复核；资料、器件选择
和私有硬件投影不得绕过该快照重新打开文件并产生第二份决策。事实按 manifest 的
逻辑键读取，不要求文件必须叫 `soc.json` 或 `routes.json`。

F4 的 `facts.soc` 保留身份、CPU、精确型号、device profile、封装／内存与供应依赖
身份；可选
`facts.controllers` 独占 `controller_groups`／`can_domain`，可选 `facts.clocks`
独占 `clock_profiles`。通用 `read_soc` 通过同一 reader 组合这些输入；base 与片段
重复声明同字段、片段未知字段或错误 schema 均拒绝。未声明片段的参考包仍可使用
内联事实。资料仅由 `facts.references` 选择；timebase 由 System binding 的角色
选择，Board 查询和生产解析使用同一资源预留结果，不再各自维护默认 timer。

生产 DataProvider 包必须同时声明 `facts.references` 与 `facts.review_profiles`；
只读取包元数据的 `packages.load` 不受此约束。DataProvider 经同一 reader 读取后构造
不可变 ProfileSet，显式持有手册、数据手册、勘误身份与适用范围。通用工具不内置
F4 的资料表，不默认选择某个勘误 profile。显式启用 test support 的 reference 包
可使用仅记录来源的 schema 1 资料，不取得生产资料或执行资格；原始资料身份、软件
模式与实际执行仍分别校验。

binding 文件片段声明 `schema_version`、`bindings`，以及可选 `domains` 和
`static_fragments`。控制器 binding 有序加入；重复 domain key 拒绝，不能用后读覆盖
先读。文件拆分与下文 StaticFragments 的声明复用是两种机制，不能互相替代。

所选 binding 文档的顶层 `pads` 是 pad 坐标的唯一作者来源，每个显式身份记录
`registers`、`port_index` 和 `number`；不从 `PA0` 一类名字推导坐标。
`hardware.pads` 被拒绝。DataProvider 保存从同一文档构造的 BindingPolicy，资源
校验与静态连接复用它的只读投影；不重新打开事实或维护第二张坐标表。具体 IP 的
硬件模型按实际资源声明；软件参考不应伪造其他 IP 的 ADC／DMA 硬件协议。

## 公共领域与私有绑定

[DomainPolicies](../../../tools/configure/domain_policies.py)读取包声明的格式、限额、
时钟、共享 IRQ、数据流几何和具体领域／IP 政策。缺失领域不授予可选能力，不从厂家
名称、相似寄存器或目录推断支持。物理事实、可用模式、Board 路线与资源约束仍共同
决定某个完整装配是否合法。

### 时钟、IRQ 与 pinmux 的唯一解析

SoC 的 CAN 控制器事实通过 `clock_domain` 引用所选 ClockPlan 域。CAN 时序校验、
绑定值和审计输出复用同一解析结果；缺少域引用拒绝，不回退到 core 时钟或全局
CAN 频率。已有 UART／SPI 的事实继续按原领域合同校验。需要共享
时钟来源的约束属于具体 IP，时钟频率相等不代替时钟域身份相同。

路线可以用 `clock_profiles` 限定已审运行档位。配置在静态装配时核对名称与选定
时钟计划。器件目录可用 `startup_vector_tail` 声明官方启动表在外部 IRQ 之后的
32 位字。链接检查校验字值和完整表长，外部 IRQ 槽数独立取自器件事实。

CAN 的公开帧与借用合同保持独立；bxCAN 的时序范围、mailbox/FIFO、bank/split 和
FMI 属于明确的 IP policy。控制器事实的 `irq_sources` 将逻辑原因映射至已审物理
IRQ，不能仅按列表序号推导语义。多个原因引用同一向量时，所选绑定必须提供对应的
固定分发；跨控制器共享还须有明确共享组、兼容优先级和互斥来源。未知 dispatcher
或未审核的异构共享拒绝。

`domains.can.model` 明确选择 `bxcan`。其必填 `transport` 按控制器记录已审 C 实现
接受的 `clock_domain` 和 `irq_sources`，与物理事实分别核对；F4 当前仅接受自己的
APB1 和四条独立向量。应用 TOML 不能覆盖这些 policy。修改 transport 本身属于
实现合同变更，须重新审查 C 行为、ARM 链接和消费结果；这份受信声明不证明任意
元数据与任意 C 算法等价，也不扩大芯片能力。

包接口代际由 `packages.INTERFACE_VERSION` 和同源生成的编辑器 schema 固定。
本轮原子迁移声明式包，旧接口明确拒绝。包清单格式、binding fragment 格式、TOML
和 factory 代际各自维护；消费方不能只因语义版本相同而混用不同提交的工具与事实。

绑定事实的 `pinmux_policy` 选择有限的编码合同。数值编码声明其合法 selectors，
每个 AF 使用点都由同一 policy 检查；F4 的合法集合仍为 0..15。不同编码需要对应的
事实、资源和 C 实现，不能仅放宽整数上限。定时器滤波等独立硬件位段继续按自身
合同校验，不受 pinmux 范围影响。

公共语义在 UART／CAN／USB 等窄合同中维护；采集、SAI、波形、外部总线、存储与
图形政策在 `tools/configure/policies/` 就近校验。新 IP 符合已有模型时增加数据与 C
绑定；模型不同则扩展经过审查的 policy 和测试，不给公共 resolver 增加型号条件。

`DomainPolicies.binding_values()` 的实际投影入口是
`ControllerIntegrationIR.constants`。它是显式私有命名值索引：叶值由具体政策的
schema 校验，包含标量、冻结 dataclass、tuple 与已解析引用。Packet、Graphics 等
记录只服务于所选绑定；外围装配仍使用具名不可变 IR。它不是任意 resolved 字典，
也不新增可随意访问属性的 namespace facade。公共 IR 不为每个 IP 私有字段加成员。

## 静态布局与模式选择

[DataProvider](../../../tools/configure/data_provider.py)连接已校验事实和 C 实现。
每条 binding 声明 `kind`、`implementation`，并在 `mode` 与 `modes` 中二选一；
`modes` 表示同一绑定允许的模式别名。可选 `controllers` 限定实际控制器，候选
重叠或不唯一时拒绝，不按声明先后选一个实现。

binding 可以选择一个 `static_recipe`，或用 `profiles` 映射已解析的
`execution_profile`。profile 是公共语义与端点关系产生的有限键；未维护键拒绝，
不在固件中创建模式查找器。不同模式只承担自己选择的存储。

| 机制 | 输入、产物和边界 |
| --- | --- |
| `StaticRecipe` | 为一个 owner 声明真实 C 类型、对象角色、常量、数组、引用及固定 admit／prepare／close／IRQ 连接 |
| `StaticObjects` | 为已选择的共享或平台记录声明固定命名对象和连接，不生成硬件状态机 |
| `SharedIrqGroup` | 核对实际 handler、相同优先级、互斥 source mask 与不同 state，生成一个有界分发表和薄入口 |
| `ChildGroups` | 为已有 controller 的有界 endpoint 元组生成私有存储、face 与固定调用，不再取得第二个 controller |
| `IrqConnection` | 显式固定 handler、调用与退出屏障，不用正则重写生成 C |

静态 recipe 的 `source` 只遍历命名不可变 IR；`ref` 只能引用当前布局的已声明角色。
有界 map 只展开已解析 tuple，不能嵌套任意循环。明确的可选值和枚举映射用于静态
布局，不会将未支持模式换成别的模式。C 编译器检查私有结构、函数签名、对齐和
初始化；主机工具不猜测 ABI 或 `sizeof`，也不执行任意 C 字符串。

### 单层 StaticFragments

[StaticFragments](../../../tools/configure/generation/static_fragments.py)已接入生产 DataProvider。
文件片段顶层 `static_fragments` 合入选中软件的 `software.static_fragments`，分为
`declarations` 与 `values` 两库。每库在整个选中包内使用统一名字，重名拒绝，不允许
后读覆盖。尚未使用的片段也检查有限语法；不据机制接入声称所有 F4 模式已采用。

使用点只允许完整 `use` 引用：声明列表中的引用平铺声明列表，其他引用只替换一个值。
不能带参数、覆盖字段、merge、引用另一个片段或执行表达式；引用层数为一，有界
record／array 本身仍可嵌套。

展开只复用作者数据，不创建新模式或硬件行为。在 Binding／Profile 创建前展开，之后仍由 StaticRecipe 校验实际
角色、类型、引用和长度；不能因为片段在另一处合法就跳过本处作用域检查。

## 固件行为与 C 常量

硬件 admission、MMIO、readback、等待、IRQ 处理、停止、部分清理与恢复全部由
手写 C 执行。优先复用满足完整模式合同的官方 HAL／LL；SDK Handle 保持私有，同一
live 行为域只有一个 owner。factory 只返回已有 face，不取得硬件或创建任务。

RCC PLL／分频编码、ADC 分辨率、SAI 帧长度和 LTDC 累加时序在私有 C 常量入口计算。
例如 ClockPlan 保存物理分频与频率，`stm32f4_clock_encoding.h` 使用锁定 CMSIS
位定义形成初始化常量。配置政策先验证合法范围，C 编译器再处理编码；这些常量表达式
不访问寄存器，不增加运行时解释器。实际寄存器顺序、等待与失败仍由 C owner 负责。
