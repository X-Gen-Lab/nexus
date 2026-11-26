---
id: guides.provider-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 接入 MCU、IP 与静态实例

先查看[当前支持](../reference/platforms/support.md)，选择实际需要的最小增量。
生产入口为 `soc/<vendor>/<family>/`，硬件算法由 C 实现；主机配置器校验事实并
生成静态连接，不在 SoC／Board 中增加 Python 插件。

| 增量 | 修改边界 |
| --- | --- |
| 已有 IP 和时钟方案的新料号／封装 | 精确器件、内存、封装和差异数据 |
| 已有模式的新实例／路线 | route、资源和 Board 授权；复用 C 驱动 |
| 新 SoC 复用已知 IP | 包内启动、时钟、内存、binding 和 CMake 集成 |
| 新 IP 沿用已有公开语义 | 包内 C、binding 与能力数据 |
| 新公开语义或硬件模型 | 先扩展领域合同／policy 和真实测试 |

`package.json` 选择事实、binding 和构建入口。SoC facts 描述精确器件拥有的资源，
`board.json` 授权路线及外部时钟等电气条件，binding 将选择连接到已有 C 实例。
沿用现有 IP 和时钟模型时，先在这三个输入边界表达差异。若所需分频、共享 IRQ 或
DMA 拓扑无法表达，先用一个精确装配和拒绝用例确定新模型边界，再扩展对应的
`tools/configure/domains/` 或 `tools/configure/policies/` 校验与包内 C 行为。

同一 MCU 的新 PCB 走[Board 接入](board-porting.md)。新增现有模型的 MCU 不给
公共 resolver 添加型号分支；数据也不能替代未知时钟、DMA 或外设算法。

精确器件的物理上限必须贯通到 C 准入和归还。例如 GPIO bank 是否存在由选中的
器件头与 C traits 约束，封装实际引出的 pad 由 facts 约束，PCB 是否允许使用由
Board 约束；不能只提高一个家族共用最大值。退出收条也须接受该器件实际取得的
bank，同时拒绝外来资源。机械事实优先使用既有投影／compiler probe 对照，不增加
运行时器件表或第二份手工能力清单。

## 先划分公共语义与硬件模型

公共 I/O 维护帧、单位、借用、完成和错误语义。IP policy 维护该硬件的时序范围、
FIFO、滤波器和中断原因；SoC 提供实际控制器、时钟域与向量，Board 授权路线和电气
条件。新增同模型器件只补事实和 C 绑定；新模型先增加有真实回归的窄 policy。

CAN 控制器显式引用已经解析的 ClockPlan 域，时序检查和生成的 C 配置消费同一
结果。已有 UART／SPI 的时钟事实按各自合同维护。公共 CAN 不猜测 APB、core 时钟
或另一控制器的频率。需要同源、同频或主控制器 gate
的 IP 分别声明这些约束；相同数值频率不代替同一资源身份。

中断原因与物理向量分别声明，不通过列表位置推导 TX、RX 或 error。共享向量只有
在包声明关系、存在已审 C dispatcher，且优先级、OS 调用约束和来源所有权兼容时
接纳。生成器输出一个固定入口，pending、ack 和硬件撤回仍由 C owner 处理。

pinmux 使用包选定的编码模型。当前 F4 数值 AF 仅接受 0..15；其他范围由对应模型
和 C driver 同时维护，未知编码拒绝。共享 remap 需要真实资源和实现，不能伪装为
较大的 AF 数字。应用继续选择 Board binding，不编写寄存器编码或执行脚本。

## 按一条纵向路径完成接入

1. 固定官方手册、数据手册、勘误和 SDK 身份，确认精确料号及可复用 IP。
2. 在一个包中声明 `package.json`、所需 facts、binding、私有实例布局和 C 行为。
3. 先以状态 GPIO／console 等最小路径验证 Clock、IRQ、内存与生命周期。
4. 通过同一 TOML 解析和静态生成，实际编译、链接并检查向量及资源。
5. 再按所选模式补请求、取消、迟到 IRQ、排空失败、恢复和实例隔离回归。
6. 对实际导出范围执行源码与安装 SDK 消费，记录精确结果；物理行为另行验收。

接入后记录这条路径实际修改的文件：器件 facts、Board、binding、C 实现与 SDK／
CMake 选择各承担什么差异。已有公共语义的同类器件应复用同一应用 C，新增实例不
增加专用 emitter 或中央型号分支。解析器仍统一取得输入快照，先固定 CPU／后端／
时钟执行条件，再解析设备图，最后冻结唯一 IR 并原子发布；不维护另一份可变决定值。
最小装配只生成所选接口和存储，生成源码字节数不能当作最终 Flash／RAM 成本。

现有 [F4 同源消费参考](../../tests/integration/assemblies/stm32f4_family/README.md)使用同一
`gpio_consumer.c` 驱动 `status` 和 `aux`。F407 的软件 Board 授权 PG9／PB2，F479 的
软件 Board 授权 PJ0／PK3。两份 TOML 选择逻辑极性并生成相同的接口类型，F479 的新路线
复用已有 GPIO 实现。按该目录的 ARM 构建命令验证两份装配，同时用越权 pad 或路线验证
解析拒绝。物理 PCB 接线另需测量证据。

新行为先实际 RED，再最小实现、GREEN 和整理。驱动优先复用审核过的官方 HAL／LL，
厂商类型、Handle 和寄存器布局留在实现内部。一个活跃控制器行为域只保留一个
硬件传输 owner，公开接口继续使用 Nexus 类型。

## 构建与生成的责任

```text
package 与事实 → 资源校验 → typed IR → 静态对象／连接
包内 CMake → 所选 C 实现／SDK → 镜像与资源检查
```

Provider target 发布 `NEXUS_STARTUP_SOURCES` 和 `NEXUS_STARTUP_USAGE_TARGET`；
Platform 用 `NEXUS_PROVIDER_TARGET` 引用它。每个镜像独立创建 startup OBJECT，
私有消费器件宏、配置头和必要 SDK include，不通过公共 `INTERFACE_SOURCES`
把厂商源注入应用。

驱动直接声明 SDK、DMA 和辅助实现需求；System 声明自己的启动环境，不修改兄弟
目录稍后读取的开关。HAL 对象复用须证明器件、ABI、配置头与回调相同。

package 字段、同一输入快照、静态 recipe 与 fragment 语法集中见
[包与绑定合同](../reference/contracts/package-bindings.md)。公开构建入口见
[CMake 接入](cmake-consumption.md)，固件行为和失败责任见所属领域合同；不要再维护
另一份 C 源码列表或运行时寄存器脚本。
