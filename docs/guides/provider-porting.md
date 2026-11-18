---
id: docs-guides-provider-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

(docs-guides-provider-porting)=
# 接入 MCU、IP 与静态实例

本页帮助维护者扩展已维护 IP 的器件、路线或实现新的硬件模式。当前生产范围仍由
[精确支持](../delivery/support.md)限定；声明式 GD32F470 是独立软件参考，不自动
成为生产系列或实板资格。先读[接入合同](../design/integration-contracts.md)与
[官方库边界](../design/official-stm32-provider.md#build-time-provider-contract)。

生产默认配置使用声明式 package、通用 DataProvider、DomainPolicies 与有限静态
装配；旧 F4 家族 Python 构造模块已删除，不再由家族导入注册表选择实现。硬件行为
仍由手写 C 拥有。事实权威与全部选定模式的集成仍须实际验收；任务状态见
[当前架构](../delivery/architecture.md#本轮架构迭代任务)，不以一次生成或 ARM
链接关闭全部任务。

## 先确定最小修改边界

| 增量 | 应修改的位置与边界 |
| --- | --- |
| 已有 IP、已有 System profile 的新料号或封装 | 精确器件、内存、封装与差异数据；公共配置引擎不加型号分支 |
| 已有能力的新路线或实例 | route、资源与 Board 授权；C 实现从只读配置取得接线 |
| 相同料号的新 Board | PCB、晶振、外部器件与安全电平；必要板级行为用 C |
| 新 SoC 复用已知 IP | 复用驱动与 binding，补包内启动、时钟、内存与 CMake 集成 |
| 新 IP 沿用已有公共语义 | 包内 C、binding 与对应能力数据；按真实编程模型复用 |
| 新公共语义或未维护的 IP 模型 | 显式扩展领域合同／policy，先写实际拒绝与行为测试 |

新 MCU 复用现有模型不需要复制一套配置器。新时钟拓扑、DMA 模型或公共操作超出现有
合同后，仍需实现其通用领域政策和 C 行为；数据不能替代未知硬件算法。外部器件通过
组件消费公共端口，产品任务、分区和恢复策略保持外置。

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
[源码归属图](../assets/diagrams/driver-source-ownership.svg)。

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

生产校验由 [packages.py](../../tools/configure/packages.py)拥有，
[编辑器 schema](../../tools/configure/soc-package.schema.json)从同一字段合同派生。
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
硬件模型按实际需要声明，GD32 软件参考不需伪造 F4 的 ADC／DMA 硬件协议。

## 公共领域与私有绑定

[DomainPolicies](../../tools/configure/domain_policies.py)读取包声明的格式、限额、
时钟、共享 IRQ、数据流几何和具体领域／IP 政策。缺失领域不授予可选能力，不从厂家
名称、相似寄存器或目录推断支持。物理事实、可用模式、Board 路线与资源约束仍共同
决定某个完整装配是否合法。

公共语义在 UART／CAN／USB 等窄合同中维护；采集、SAI、波形、外部总线、存储与
图形政策在 `tools/configure/policies/` 就近校验。新 IP 符合已有模型时增加数据与 C
绑定；模型不同则扩展经过审查的 policy 和测试，不给公共 resolver 增加型号条件。

`DomainPolicies.binding_values()` 的实际投影入口是
`ControllerIntegrationIR.constants`。它是显式私有命名值索引：叶值由具体政策的
schema 校验，包含标量、冻结 dataclass、tuple 与已解析引用。Packet、Graphics 等
记录只服务于所选绑定；外围装配仍使用具名不可变 IR。它不是任意 resolved 字典，
也不新增可随意访问属性的 namespace facade。公共 IR 不为每个 IP 私有字段加成员。

## 静态布局与模式选择

[DataProvider](../../tools/configure/data_provider.py)连接已校验事实和 C 实现。
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

[StaticFragments](../../tools/configure/static_fragments.py)已接入生产 DataProvider。
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

## Provider 构建合同

公共装配按 `package.json` 的 `cmake.entry` 和 `cmake.target` 载入所选包，不增加 MCU
名字分支。生产 SoC 接入使用以下固定字段，公共入口验证类型、路径和所属范围：

| 属性 | 责任 |
| --- | --- |
| `NEXUS_STARTUP_SOURCES` | 启动与 System 源绝对路径列表 |
| `NEXUS_STARTUP_USAGE_TARGET` | Provider 拥有的 INTERFACE 启动编译要求目标 |
| `NEXUS_PROVIDER_TARGET` | 由 `Nexus::Platform` 保存的实际 Provider 引用 |

启动 usage 声明私有 Device／HAL include、器件宏、配置头和必要依赖。Firmware 为每个
镜像单独创建 startup OBJECT，并私有消费该 usage；应用源码不因此看到厂商头或器件
宏。Provider 不通过父作用域回传供根入口搬运的 include／define 列表，也不将厂商
启动源作为公共 `INTERFACE_SOURCES` 注入应用。保持启动对象与弱符号的正确链接顺序。

包内驱动直接声明 SDK、DMA 与公共机制需求，System 明确声明自身启动环境；禁止驱动
修改兄弟模块随后读取的开关。F4 保留主要 archive，不按外设机械拆库。HAL 对象复用
必须核对实际器件、ABI、配置头和回调定义，不能仅根据源码文件名相同就共用。

应用先 `find_package(Nexus CONFIG REQUIRED)`，再显式
`nexus_add_platform(ASSEMBLY ...)`。发现不创建装配，源码与安装 SDK 进入同一生产图；
`Nexus::ABI` 与 `Nexus::OS` 提供稳定使用要求。新 part／Board 复用已有能力时不改
公共配置调度器；新公共语义或架构按其实际合同扩展。完整边界见
[构建架构](../design/build-architecture.md)。

## 构建、SDK 与验收

![FIG-03：配置输入、静态装配与目标构建](../assets/diagrams/configuration-generation.svg)

图表示当前通用构建流：TOML 与 facts → 具名 IR → 静态连接与原子 bundle → CMake →
实际产物。箭头不是运行调用或物理资格；默认机制切换不代表所有模式与消费者都已
通过本批集成。具体目录归属由[源码地图](../explanation/source-map.md)维护。

CMake 是 C 翻译单元与链接依赖的唯一权威。SDK 按明确选择的包、导出归属和依赖锁
收集源码；不从单一 ELF 推导整个 SDK，也不自动初始化库存。默认 package 清单来自
兼容性事实，安装验证读取冻结的 `profile.packages`；完整合同见
[源码与 SDK 兼容性](../design/source-compatibility.md)。

1. 固定手册、数据手册、勘误与 SDK 的精确身份，再增加事实和所需 C 集成。
2. 对新行为先执行实际 RED，再写最小 C 或工具实现并执行 GREEN。
3. 保留拒绝、实例隔离、部分取得、取消、迟到 IRQ、失败排空与借用回归。
4. 用实际编译上下文生成、编译和链接精确 part／mode，核对 ABI、向量、内存与预算。
5. 演练同 IP 新 part／route 的引擎零修改；源码消费与安装 SDK 各自验证。
6. 完成独立语义与翻译审查，再记录当前任务状态和对应实际执行身份。

接受后请求与缓冲保持到 acquire-observed SETTLED，控制、观察者和发布者还需
独立 join；失败排空保留 QUARANTINED 与原存储。停机按具体 owner 收条逆序撤回，
不把超时或取消写成借用结束。合同见[所有权](../explanation/ownership.md)。

软件参考、配置成功、模型、ARM 链接、SDK 消费与实板分别验收。当前物理 HIL 为
`not_executed`；具体模式、剩余耦合和最终集成按当前任务保留，旧记录可从
[固定前序源码](https://github.com/X-Gen-Lab/nexus/tree/ba13686b91c94d19ef11acf910653ee2fcc9aa8b)
追溯，不继承其资格。
