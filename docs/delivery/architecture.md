---
id: delivery.architecture
type: explanation
audience: maintainer
source_language: zh_CN
---

# 当前系统架构与接入重构

Nexus 是静态装配的 C11 通用嵌入式平台。当前生产入口是
`soc/stm32/stm32f4/`；[精确支持](support.md)区分八个料号、七个器件视图、
四个 F407 Board、软件参考和实际验证。产品任务、队列、私有 PCB、Flash 分区与
恢复策略由外部工程拥有。Native 是显式测试支撑；未选厂商依赖保留但不进入默认闭包。

本页先说明现有运行合同，再说明接入重构目标与任务。目标不代表实现已经完成。
本批以 [`ba13686`](https://github.com/X-Gen-Lab/nexus/tree/ba13686b91c94d19ef11acf910653ee2fcc9aa8b)
为源码基线；前序软件交付见 [PR #63](https://github.com/X-Gen-Lab/nexus/pull/63)。
每项新结果必须绑定实际源码和配置，不能继承基线的测试或 SDK 资格。

![FIG-01：平台职责与外部产品边界](../assets/diagrams/platform-architecture.svg)

图表示当前职责、编译依赖与接口访问，边的含义以标签为准。可选 OS 和组件不在
每次 I/O 的强制路径中；静态结构不表示启动顺序或物理验收。

## 当前层次、状态与依赖

| 位置 | 责任与所有者 |
| --- | --- |
| `core/` | 结果、请求、借用、单调 deadline、可选资源事务与整平台协调 |
| `arch/arm/cortex_m/` | CPU 原语、中断状态保存恢复、屏障及明确可选能力 |
| `io/` | 公共领域接口、单位、检查分派及硬件无关机制 |
| `soc/stm32/stm32f4/` | 精确器件事实、时钟、启动、内存、IRQ／DMA、控制器与私有 SDK |
| `soc/stm32/stm32f4/drivers/ip/` | 经核对可共享的 F4 私有硬件算法 |
| `boards/` | 晶振、接线、外部器件、安全电平与未知 PCB 条件 |
| `os/` | 显式后端的等待、通知、任务、队列和存储合同 |
| `components/` | 通过公共端口复用的协议、器件和 owner 机制 |
| `tools/configure/` | 用户意图、事实校验、不可变 IR 与静态连接生成 |
| `cmake/` | 唯一源码目标图、工具链和 Source SDK 消费 |
| `tests/support/native/` | 显式 host-test 模型与装配，不是生产 SoC |
| 外部产品 | main、scheduler、执行者、预算、区域与恢复策略 |

Core 不依赖 Arch、SoC、OS 或厂商 SDK；Arch 不依赖 OS 或外设实现。I/O 依赖
Core，具体 provider 依赖公共 I/O、Core、Arch 和私有 SDK。组件通过窄端口访问
设备，所选 OS 后端保留自身真实内核与对象布局。源码入口见
[源码定位](../explanation/source-map.md)。

factory 返回已有静态 face，不初始化硬件。产品取得接口以后，经公共操作与共享
只读 `ops(context, ...)` 进入 provider。Platform 负责装配和生命周期，不转发每次
I/O；普通同步实例不承担异步 request、stream、队列或 OS 状态。

## 对象、所有权与设计原则

| 对象 | 边界 |
| --- | --- |
| controller | 一个物理控制器行为域只有一个 live owner |
| mode | 决定实现、能力、状态和缓冲需求 |
| face | 公共接口及实例 context，不替代 controller owner |
| endpoint | 依赖控制器的逻辑或外部端点，不重复取得控制器 |
| signal | 授权单 pin 与逻辑极性，不新增硬件 owner |
| request／buffer | 接受后被借用，实际结清前保持有效 |
| receipt | 由具体 owner 记录实际剩余副作用 |
| role／region | 显式资源用途与访问授权，不隐含固定 timer 或地址 |

单一职责体现在事实、决策、C 行为和源码图分别维护；开闭原则体现在同 IP 扩展
数据、新 IP 扩展实现、新公共语义扩展合同。可替换实现保持单位、错误、借用和完成
一致；接口按真实消费者划分。Adapter 隔离 HAL／LL，静态 factory 提供类别约束，
组件使用显式注入。SOLID 不要求设备基类、运行时注册或统一内核对象。

## 当前构建与目标接入链路

![FIG-03：当前配置与原子生成](../assets/diagrams/configuration-generation.svg)

图表示当前构建期输入和产物流，不表示运行期调用。现有 schema-2 TOML、只读
SoC／Board 事实、不可变 typed IR、原子 bundle 和 CMake 唯一目标图继续保留。
生产默认路径由声明式 package、通用 DataProvider、DomainPolicies、私有 `constants`
与有限 StaticRecipe 组成；旧 F4 家族 Python 构造模块已删除，不保留家族导入注册表。
事实与静态连接按同一快照解析，协议见[接入指南](../guides/provider-porting.md)。
字段权威、具体模式和消费者仍须本批集成验收；删除旧模块不等于整包取得软件资格。

当前装配链路为：包与事实、Board 和 assembly → 通用解析与资源决策 → 类型化 IR →
静态 C 与选择结果 → 包内 CMake → 编译链接 → SDK 与外部消费者。箭头表示构建期
数据或产物依赖；各模式和消费者的资格以任务退出条件和本批实际回归为准。

| 目标权威 | 拥有的内容 |
| --- | --- |
| SoC／IP 数据 | 器件、封装、内存、控制器能力、路线、DMA／IRQ／trigger |
| Board 数据 | 晶振、电源、接线、外部器件与安全电平 |
| 产品 assembly | 所选接口、模式、容量、预算与连接 |
| 通用配置引擎 | schema、引用、通用领域语义、资源冲突、确定性选择与诊断 |
| C provider | admission、硬件顺序、等待、回读、错误、停止和恢复 |
| CMake | 翻译单元、编译选项、链接目标与实际依赖 |
| 包导出声明 | 生产导出范围和必要资料，不复制源码编译图 |
| 执行记录 | 实际源码、配置、工具、命令与结果 |

同 IP 新料号或路线应只扩展数据与差异验证；新 MCU 的启动、时钟、内存集成仍由
其包维护。新 IP 沿用现有公共语义时新增 C 实现和 binding；真正新增公共语义时才
扩展公共合同。通用引擎不应增加 STM32、F479 或寄存器编码分支。

公共领域 IR 表达单位、格式、预算、借用和能力；私有 binding 值必须受 schema
约束且不可变。有限静态表示只生成对象、常量、数组、引用、face、factory、固定
调用和薄 IRQ 连接。C 编译器验证真实私有类型与布局；工具不模拟 ABI，不执行
寄存器脚本，不重新解析已生成的 C 文本。

目录调整服务于责任：当前 F4 私有实现继续就近复用；有真实跨家族共享证据后再提取。
精确型号和封装作为数据。SoC／Board 不放 Python 插件，通用主机工具集中维护。
厂商 gitlink、锁和许可证全部保留，未选库存不进入准备、编译与默认 SDK。

### 接入包目录责任

以下是目标维护边界，不要求为目录外观移动所有文件。事实可以保留原位；同 IP
行为就近共享。包的 binding 与私有 C 类型一同维护，公共工具不复制私有布局。
家族专属构建靠近 SoC，公开通用合同仍由 Core／Arch／I/O 拥有。

```text
soc/<vendor>/<family>/
  package.json             包入口、事实与导出归属
  soc.json / devices.json  器件与控制器事实
  routes.json              引脚、IRQ、DMA 与触发连接
  references.json          资料和勘误范围
  drivers/                 手写 C 硬件实现与相邻 binding
  private/                 config、runtime、receipt 与 SDK 类型
  cmake/                   家族选编与官方库配置
  CMakeLists.txt           本包唯一源码目标定义
boards/<board>/
  board.json               PCB、电气连接和外部器件
  board_control.c          必要板级行为，按需存在
tools/configure/           通用 schema、resolver、IR 与 emitter
```

新增同 IP 器件不改变公共工具、通用 CMake 或 SDK exporter；改变公共操作语义才
演进领域合同。当前版本和 schema 由
[兼容性事实](../../dependencies/platform-compatibility.json)唯一维护，文档不建立
另一份版本策略。过渡路径只在对应模式迁移并验证后删除。

## 生命周期、失败与性能边界

Reset 和必要 pre-C 初始化先满足启动存储条件；永久 SRAM、已启 watchdog 和保留
RTC 不伪装为普通可逆资源。运行期先完成 controller 准入，再建立 System、共享
资源和控制器。具体 owner 保有收条，失败按逆序撤回，首次拒绝释放时保留剩余责任。
primary、cleanup 和 remaining effects 分别报告。

默认 Platform 协调器与可选 `nx_lifecycle_*` 资源事务职责不同，不要求全部驱动
接入同一组 ops。固定连接和公共入口使用同一手写 C 协调算法，产品启动 scheduler。

ACCEPTED 后请求和缓冲借用持续到 acquire 观察 SETTLED。DMA 完成、线上完成、
外设停止和 IRQ／队列引用撤回分别证明；SETTLED 是 provider 的最后一次请求访问。
timeout 或 cancel 不撤销借用；排空失败保留 QUARANTINED 存储。控制、observer、
wake publisher 和任务仍需对应 join。合同见[所有权](../explanation/ownership.md)。

停机先关闭准入并保留执行能力完成排空，再 join 用户和逆序停止资源。暂停失败停在
SUSPENDING；只有完整 SUSPENDED 才能恢复原 plan。STOP 与时钟变化建立新的时间域，
应用和 OS 显式重建 deadline，不能自动复播旧请求。详见
[System](../design/f407-system-power.md)与[时钟屏障](../design/clock-domain-sleep.md)。

HAL／LL 优先用于满足完整模式合同的硬件路径。直接访问限于明确 SDK 缺口、勘误、
原子合同或已证明预算；同一行为域不并存两个 transport owner。config、runtime、
receipt 和共享只读 ops 按真实需求分开，未选模式不分配对应最大状态和缓冲。
性能与空间用实际 ELF、对象、反汇编及测量判断，不能由 inline 或接口形式推定最优。

QuadSPI 当前是精确 W25Q128JV 集成。[独立协议事实](../../soc/stm32/stm32f4/private/w25q128jv.h)
与其 C 实现已提纯 JEDEC 身份、adopt、保护状态、页边界、几何与命令，不依赖
控制器 SDK。STATUS → WREN → WEL → PULSE → WIP → VERIFY／WRDI 仍由该精确
集成状态机推进；间接与映射共享一个 owner，保留 WIP、排空和失败责任。当前没有
第二种 NOR 消费者，不为其预建虚表，也不声明任意可替换 NOR backend 已全部解耦。

## 本轮重构退出范围

本批要求所有当前承诺模式沿同一装配链迁移，保持公共合同、支持边界和消费者行为。
新增候选模式必须单独实现和验证，不能由头文件、接口或家族目录推定完成。
原有 WP01–WP12、IO01–IO20、R0–R13 与执行结果保留在
[固定基线架构页](https://github.com/X-Gen-Lab/nexus/blob/ba13686b91c94d19ef11acf910653ee2fcc9aa8b/docs/delivery/architecture.md)。
前序 [PR #62](https://github.com/X-Gen-Lab/nexus/pull/62) 和
[PR #63](https://github.com/X-Gen-Lab/nexus/pull/63) 各保留原源码与执行身份；旧计数
不重标为新提交的执行。物理 HIL 与远端 Actions 按用户要求暂缓，本地软件门禁仍必需。

## 本轮架构迭代任务

以下 60 项是本页人工审查表，不是新的配置源、台账引擎或资格流程。状态区分
合同已固定、实施中、已有实现但待集成验证与已验证。只有对应实现、消费者与实际
执行齐全才关闭任务；已有实现不表示当前完整矩阵、SDK 或软件候选通过。A–E 固定
纵向链路，F 迁移现有模式，G／H 验证并清除过渡路径。第二家族的软件切片单独验收，不自动扩大生产支持。

### A：架构合同

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| A01 | 固定层依赖 DAG；公共层无反向依赖，SDK 私有 | 合同已固定，待集成验证 |
| A02 | 固定 controller／face／endpoint／signal／request 和 owner 模型 | 合同已固定，待集成验证 |
| A03 | 固定事实权威；每个字段只有一个作者来源 | pad 单一权威已落；其余字段核对中 |
| A04 | 区分 assembly、包、binding、IR 与接口兼容协议 | 协议已有实现，待集成验证 |
| A05 | 固定迁移基线、支持范围和结果身份，不继承旧资格 | 基线已固定 |

### B：硬件事实与包协议

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| B01 | 统一内外置 package 协议与入口、版本、重复 ID 检查 | 已有实现，待集成验证 |
| B02 | 统一器件与 IP catalog，移除重复 PARTS 与型号白名单 | 已有实现，待集成验证 |
| B03 | 统一 pin／AF／route 数据，不从路线名称推断能力 | 显式 pad／route 已落，接线一致性收尾中 |
| B04 | 统一 DMA／IRQ／trigger 与共享资源，冲突和 owner 可解释 | 已有资源模型，待完整冲突与共享验证 |
| B05 | 统一 clock／memory 事实，明确 DMA 可达性和内存属性 | 已有显式事实，待内存／时钟集成验证 |
| B06 | Board 电气连接与 signal 统一，极性和安全电平分开 | 已有 signal 与电气连接，待集成验证 |
| B07 | 关联资料、勘误和模式，分开硬件、软件与执行状态 | 资料事实已有实现，待独立范围核对 |

### C：配置与静态实例

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| C01 | 通用包加载替代家族 Python 注册，新家族不修改公共注册表 | 已有实现，待完整装配验证 |
| C02 | 公共与私有 binding schema 严格校验，不穿透任意字典 | 已有实现，待集成验证 |
| C03 | 收敛不可变 typed IR，引用、单位、模式与私有记录明确 | 已有实现，待集成验证 |
| C04 | 清除公共 resolver 的家族分支，限制移入事实或私有 C | 公共策略剩余耦合清理中 |
| C05 | 有限静态初始化替代任意 Construction C 字符串 | 生产路径已迁移，待完整生成与编译验证 |
| C06 | 显式 IRQ 与屏障连接，删除正则改写生成 C | 已有实现，待生成 IRQ 编译与模型验证 |
| C07 | 默认 strong factory，跨类别 ID 误传在编译阶段失败 | 待集成验证 |
| C08 | 确定性原子 bundle；失败不消费旧输出 | 待集成验证 |

### D：C 实现与运行连接

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| D01 | 按模式整理 config／runtime／receipt，无最大状态并集或重复收条 | 已有模式拆分，待本批成本与状态审查 |
| D02 | HAL／LL／私有访问边界明确，同一行为域唯一 owner | 待集成验证 |
| D03 | 启动、时钟与 pre-C 依赖清晰，时间基准显式 | 待集成验证 |
| D04 | 停止、回滚与恢复保留真实残效，失败不假结清 | 待集成验证 |
| D05 | 保持 Core／Arch／OS 等待、屏蔽、时间域和内核边界 | 待集成验证 |
| D06 | signal、endpoint、bus owner 由装配统一连接 | 固定连接与 signal 已落，待完整消费者验证 |

### E：构建、依赖与 SDK

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| E01 | 通用 CMake 桥，不再逐项传播私有 feature 变量 | 待集成验证 |
| E02 | 家族 CMake 归入 SoC 包，公共层无 F4 编译细节 | 待集成验证 |
| E03 | SDK 宏、ABI、startup、linker 保持正确作用域 | 待集成验证 |
| E04 | SDK 按包导出范围收集，新驱动不改公共逐文件清单 | 待集成验证 |
| E05 | canonical consumer 与 fake fixture 收敛，helper 不重复维护白名单 | 待集成验证 |
| E06 | 源码与 SDK 共用工程路径，公开用户可独立配置、编译和使用 | 待集成验证 |
| E07 | 包依赖进入现有来源与锁，未选库存不准备或默认导出 | 按包选依赖已有实现，待完整 SDK 验证 |

### F：全部现有外设模式迁移

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| F01 | GPIO／EXTI：授权 view、signal、共享 IRQ 一致 | 待集成验证 |
| F02 | UART／RS485：pin／DMA 路线数据化，DE 复用 signal | 数据绑定与 DE signal 已落，待完整回归 |
| F03 | SPI：controller／endpoint／DMA 解耦，尾部与取消正确 | 静态连接已迁移，CS／endpoint 收尾中 |
| F04 | I2C：消除固定 pin，明确 master／target／恢复边界 | 已有实现，待完整 master／target 回归 |
| F05 | DMA／TIM／ADC／DAC：资源与触发图一致，单位准确 | 已有实现，待资源与触发组合验证 |
| F06 | I2S／SAI：slot／clock／DMA 配置化，块所有权保持 | 已有实现，待选定音频模式集成验证 |
| F07 | DCMI／LTDC／DMA2D：帧、块、图层、区域与退役语义保持 | 已有实现，待帧／图层模式集成验证 |
| F08 | FMC／SDRAM：几何、时序、bank 与时间基准解耦 | 几何与时基解耦已落，外存集成收尾中 |
| F09 | Flash／NOR／NAND：控制器、协议、区域和维护分离 | 已有实现，待本批区域与维护回归 |
| F10 | QuadSPI：精确 NOR 协议事实与物理边界提纯，间接／映射借用由单一 owner 协调 | 已有实现，待同源码回归 |
| F11 | SDIO：host／card／function／block 分层与 DMA 路线一致 | 分层与所选 DMA 路线已落，待完整回归 |
| F12 | CAN／Ethernet：共享资源、PHY、内存和 descriptor 归属明确 | 已有实现，共享资源与投影收尾中 |
| F13 | USB：底层与中间件唯一 owner，角色与供电显式 | 已有实现，factory／角色连接收尾中 |
| F14 | RTC／监督／电源／芯片服务／密码外设：保留域、不可逆操作与敏感状态明确 | 待集成验证 |

### G：测试与成本

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| G01 | 按独立合同精简测试，每个删除项有失效或等价覆盖依据 | 主要合同已迁移，待完整保留集回归 |
| G02 | 新行为记录真实 RED／GREEN，对应实际源码与失败 | 已记录所改行为 RED／GREEN，待最终证据核对 |
| G03 | 未知字段、失效引用、冲突与非法模式负例明确拒绝 | 待集成验证 |
| G04 | 生产 C 覆盖 IRQ、迟到事件、取消与失败排空 | 待集成验证 |
| G05 | ARM 矩阵验证器件差异、startup、ABI、内存和符号 | 待集成验证 |
| G06 | 实测所选镜像 Flash／RAM，解释同步、异步与连续成本 | 待集成验证 |
| G07 | 源码／SDK 双消费、重定位、恢复、身份和边界一致 | 待集成验证 |

### H：工程与交付

| ID | 任务与退出条件 | 状态 |
| --- | --- | --- |
| H01 | 托管入口统一目录锁，同目录受控、不同目录并行、嵌套无死锁 | 待集成验证 |
| H02 | 门禁选域覆盖删除、重命名、未知影响与公共政策变化 | 待集成验证 |
| H03 | 当前文档与教程分清历史身份，中英文一致 | 当前协议与双语更新中，待独立审查与严格构建 |
| H04 | 依赖升级沿组织 fork、gitlink、许可和受影响验证执行 | 待集成验证 |
| H05 | 实际第二家族 SDK 纵向切片验证接口与接入修改面 | 第二家族参考链已有实现，待最终集成验证 |
| H06 | 删除过渡路径，源码、SDK、支持范围与实际结果一致 | 旧家族构造已删除，剩余过渡与声明核对中 |

## 验证与阅读入口

[接入指南](../guides/provider-porting.md)区分新 Board、路线、料号、IP 与公共语义。
[工程手册](../design/engineering-handbook.md)维护 TDD、风格、测试保留与普通提交门禁；
[SDK 教程](../tutorials/source-sdk.md)维护公开消费入口。硬件事实、模型行为、ARM
链接、SDK 消费和物理 HIL 分别验收。当前物理执行仍为 `not_executed`，见
[资格边界](qualification.md)。
