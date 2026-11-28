---
id: explanation.motor-control
type: explanation
audience: consumer
source_language: zh_CN
---

(explanation.motor-control)=
# 电机计算、轴目标与硬件执行

Nexus 将电机计算、轴目标和硬件执行分别交给可选 C11 组件、外部应用桥和唯一
provider。本文说明当前软件合同及其成本，不把软件参考配置授予任何功率板、
电机或物理控制资格。

本文的电机控制硬件路线限定为 STM32F4。GD32F470 和其他 MCU 的电机路线等待独立接入和验证。

![FIG-35：电机与轴控制的调用和退出责任](../assets/diagrams/motor-control.svg)

FIG-35 表达当前可选组合：实线是显式调用或数据交付，虚线是借用和退出责任。
图不要求应用启用全部节点；精确 F407ZG 参考、源路径和边界见
{ref}`图册 <fig-35-motor-control>`。

## 由谁计算，由谁执行

| 边界 | 拥有的内容 | 外部应用仍负责 |
| --- | --- | --- |
| `Nexus::MotorControl` | 有感电流计算、坐标变换、限制、调制计划及零偏候选 | 校准后的相电流、角度来源、电机参数和硬件提交 |
| `Nexus::AxisControl` | 目标准入、速度／位置参考、参数 revision 与软件采用事实 | 执行节拍、机械侧、命令授权和机构恢复 |
| `Nexus::CurrentControl` | 标定身份、物理电流换算、实测／重构来源及观测检查 | 原始来源、零电流依据、周期关联和误差预算 |
| `Nexus::ModulationControl` | 整周期脉冲、有效采样窗口和最终量化电压 | 硬件完整映射、实际模拟建立及 WCET |
| `Nexus::DriveControl` | 有界参考交接、连续反馈分频、独立 inhibit 与可选功率预算计算 | 存储容量、执行者、完整控制组合和硬件保护 |
| `Nexus::IOPowerStage` | session、raw 观察、完整 tick 输出和退出合同 | 完整 callback context、校准桥和有限操作预算 |
| F4 provider | 所选 TIM、ADC、可选角度观察、IRQ 与共享域的生命周期 | 经审核的板级授权、供电、模拟前端和功率能力 |

组件不创建堆、任务、全局时钟或电机注册表。控制数学不包含 HAL；provider 使用私有
厂商类型和经审核的硬件操作。组件 target 仅在被明确链接时进入镜像，实例状态由
调用者提供，未选模式不承担其最大缓冲与运行状态。

| 执行入口 | 合法上下文与推进 |
| --- | --- |
| Axis 命令、参数、step 与停止 | 同一个 Task 执行者；位置规划不进入 ADC IRQ |
| Motor step 与数学函数 | 单一执行者的 Task 或选定 IRQ，满足自身计算预算 |
| PowerStage prepare／start／service／stop | 同一个 privileged、未屏蔽的 Task；显式推进 |
| PowerStage compute | 仅选定 ADC IRQ；不等待 Task、不保存临时指针 |
| CANopen／CiA402 | 单个 Thread 执行者，先完成协议 step，再提供有效轴事实 |

## 一份决定不等于一个物理结果

应用分别保留原始请求、受约束参考、软件采用、输出计划和测量。Axis 的
`adopted_at` 描述其执行者采用目标的时刻；Motor 的输出是归一化 duty 和计划电压。
provider 接受完整 shadow 组，也不证明对应物理边沿已经发生。

`nx_motor_voltage_step` 单独产生电流环电压决策，不生成 duty，由选定调制器联合
决定开关和采样计划。`nx_motor_step` 保留固定 min/max 共模调制入口；调用者仍须
验证所选硬件窗口。两种入口共用数值与故障合同，不在同一周期先后推进同一实例。

位置、速度、电流、机械角、电角和转矩不能混用。齿比、编码器计数倍率、方向和极对数
分别有明确来源。应用桥将 Axis 选定机械侧的角度和速度换算为 Motor 要求的转子
机械侧，再由电机计算应用极对数；电流参考的单位是 A，不按齿比折算。没有核验转矩
常数与电流约定时，安培目标不能改名为转矩反馈。

## 采样、观察与计划输出分别计时

Motor 输入区分 `sample_time_us`、`angle_time_us`、`observed_at_us` 和
`apply_time_us`。角度可以在采样之后、观察之前取得，随后在明确误差预算内向采样
时刻回投影或向输出时刻预测。预测使用调用者提供的有效机械速度；软件读取时刻
不自动成为采样孔径。

当前 F407ZG `injected-power-stage` 维护下列软件配置。文件名的 single／dual
表示轴数，不表示分流拓扑：

| 配置 | 选定采集与资源 | 边界 |
| --- | --- | --- |
| [control.toml](../../tests/integration/assemblies/f407_power_stage/control.toml) | TIM8、ADC1 三相加母线顺序 rank、TIM4 | 原始三相不是同时采样 |
| [single.toml](../../tests/integration/assemblies/f407_power_stage/single.toml) | 单轴、ADC1 两路相电流，无必需母线／编码器 | 角度和母线必须由有效的独立来源提供 |
| [dual.toml](../../tests/integration/assemblies/f407_power_stage/dual.toml) | TIM8／ADC1 与 TIM1／ADC2，各自顺序采集；共享 common／IRQ | 不是同 ADC 时分或单轴双 ADC 同时采样 |
| [pair.toml](../../tests/integration/assemblies/f407_power_stage/pair.toml) | 单轴 ADC1＋ADC2 injected simultaneous，各一 rank、相同采样配置 | 独占共同域；无必需母线／编码器，不与独立 ADC2 轴混用 |
| [center.toml](../../tests/integration/assemblies/f407_power_stage/center.toml) | TIM8 完整载波、下降段 CC4、更新 IRQ 门控 | 专用 center 路线；不推导双轴中心对齐或任意脉冲相移 |
| [tdm.toml](../../tests/integration/assemblies/f407_power_stage/tdm.toml) | TIM8／TIM1 两轴中心对齐、相差半载波，共用 ADC1 | 固定双会话共同启动与显式容差；不与 simultaneous pair 组合，也不支持运行中加入 |
| [dc.toml](../../tests/integration/assemblies/f407_power_stage/dc.toml) | 单轴 ADC1 单路母线分流、边沿对齐、每载波两次有效矢量采集 | 无脉冲相移；必须提供合法非零首计划，零电压退化不产生可观测窗口 |
| [fast.toml](../../tests/integration/assemblies/f407_power_stage/fast.toml) | 20 kHz 载波与声明预算 | 配置与链接不证明实际 WCET 或物理运行 |

`current_phase_mask` 标识真正测量的逻辑相位，未选槽填零不成为测量。只有
`bus_valid`／`encoder_valid` 为真时，才能消费相应字段。`sample_at` 是计划触发
参考，`trigger_ticks` 保留细粒度载波坐标，`scan_span_us` 是声明的转换跨度。
应用组合说明采样偏差与角度估计的适用范围，不靠重写时间戳伪造同步。

DC-link profile 的相电流 count／mask 为零，`dc` 提供同一已采用计划的两个原始
母线电流码、开关矢量和计划采集坐标。`scan_span_us` 覆盖两触发间隔及末次转换，
不能把它解释为一条 ADC rank 的时间。计划坐标仍不是实测模拟孔径，应用使用有界
组时序换算；独立母线电压来源与母线分流电流是不同测量。

有效窗口同时受 duty、deadtime、模拟稳定时间、转换、IRQ 延迟、compute 和提交
预算约束。调制器的数学余量不能代替 provider 的窗口检查，更不能证明真实模拟
稳定时间。所选 provider 必须明确其完整映射能力，不能从数学规划推导中心对齐、
多 ADC 同时采样、单分流或无感换相的硬件支持。

## 电流观测和调制共同规划

Current 接收明确身份与 revision 的标定，把逻辑相位原始码换算为 A。独立测量的
三相可以检查相电流和；从两相重构第三相时，和为零是计算恒等式，不能作为独立
传感器诊断。单分流还必须关联同一已采用计划中的两个有效开关矢量。饱和、无效
窗口、过大孔径偏差或标定身份不匹配都不能产生有效电流。

时序明确分为 `NX_CURRENT_APERTURES_EXPLICIT` 和 `NX_CURRENT_APERTURES_BOUNDED`。
前者使用真实孔径，后者保留合格的组参考与正偏差上界，不读取或填造逐相孔径。
仅知道计划触发和 scan span 时使用后者，不能将软件时间代理升级为实测同时采样。

Modulation 用整载波 tick 坐标共同确定脉宽、起点、采样窗口和最终理想平均电压。
模拟恢复、采样保持、ADC 占用和 CPU 提交分别预算；没有有效窗口就拒绝，输出
保持不变。可选固定相移只是一种规划候选，实际 provider 必须完整映射脉冲起点
与采样安排，不得只取 duty 或重新静默裁剪。

选定 provider 给出可用采集槽，规划器还要求触发、完整结果及 rearm 都留在该槽。
例如中心参考只开放下降段，不能选中数学上可观测但硬件未开放的上升段。

纯 C 规划的中心对齐、双／单分流能力不自动授予对应 F4 硬件模式。具体器件、
路线、capture profile、提交方式及验证范围仍由所选 provider 决定。

选定 F4 DC-link mapper 只接受边沿对齐、无相移的完整脉冲。首计划必须显式包含
两个独立有效矢量；中性等 duty 不能建立采样。初始与后续 DC descriptor 均位于
声明的完整会话 context 内，不能指向 callback 栈；provider 验证后复制，不保留
临时计划指针。规划器对没有可观测窗口的零电压等退化请求返回
`NX_ERROR_UNSUPPORTED`，不发布部分计划。provider 对非法首计划（包括中性等
duty）返回 `NX_ERROR_INVALID`，拒绝借用且不启动会话；两者不是同一种失败。
不能靠虚构电流或隐含激励维持运行。固定相移仍只是规划候选，没有对应 mapper
就不开放。

## 三种期限保持独立

1. 外部命令有效期决定旧运动参考还能否使用，重复帧和心跳不自动续期。
2. PowerStage 用单独的 `startup_deadline` 约束启动；非阻塞长期 session 的
   `base.deadline` 为 `NX_DEADLINE_NEVER`，运行直到显式停止或故障。
3. service、清理尝试和维护使用各自有界执行；超时不能撤销借用或证明已经断能。

Task 通过显式 service 推进启动及最终排空；选定 ADC IRQ 只执行有界 callback。
任务不得逐字段改动被 IRQ 借用的算法状态。跨上下文目标与参数通过已有有界对象
交接，普通结构体复制不构成同步协议。软件只能在 callback 返回后检测其超预算，
不能用 deadline 抢占一个不返回的计算。

## 参考、连续反馈和遥测各有通道

Drive 使用调用者提供的固定容量 SPSC：一个 Task 发布完整有限参考，一个快环
执行者每步最多采用一条。队列满时拒绝，原始 receipt 不随排队刷新。独立 inhibit
不等待参考队列空间；真正的硬件抑制和退出仍由 provider 完成。

Drive 的参数 revision 标识停机初始化的 Motor profile，与 Axis 参数 revision
分别管理。生产者只能交付完整记录，不能访问 IRQ 正在借用的 Motor 或 Drive。
参考接纳、快环采用、输出 staged 和物理结果分别观察。

反馈分频保留实际区间和快环覆盖，丢周期、区间断裂或队列溢出锁存故障，不能靠
读取旧记录或重编 sequence 修复。`accepted_iq_a` 是某个 generation 采用的参考，
不是实测电流；下游 tracking 必须匹配原 Axis 决策，不能回算到更新的决策。
可丢遥测使用独立对象，不参与连续控制反馈的资格判断。

共母线预算计算分别限制消费与回生功率，不假定两轴相抵消。它只计算缩放；产品
提供带有效期的预算、实施限幅并反馈下游采用。供电吸收能力、制动硬件和机械
停车仍需实际证明，软件预算不充当快速过压／过流保护。

## 标定资格与参数采用

偏置、AFE 增益／方向、相序、编码器方向和电角偏移分别建立来源。稳定偏置、零
PWM 或输出关闭不能单独证明零电流。Current 的标定身份和 revision 只检查匹配，
不认证硬件，也不自行写存储或授予激励。外部组合默认未投入运行，必须先核对
实际传感链、电机与参数身份。

运动参考运行中交接；Motor profile 和标定在退出并会合后重新初始化。Axis 按其
公共合同独立采用参数 revision，不能将其版本号当作 Drive 的停机 profile 版本。
持久化由普通维护路径处理；F407 控制期间不因此获得未审核的内部 Flash 写擦资格。

## 每轴状态与共享硬件域

每轴分别拥有算法、会话、参数和故障。配置中的显式 `domain` 把共同 ADC 设置、
门钟和 IRQ 交给一个静态 owner。Factory 只返回既有 face，不创建第二个 owner。
当前独立 ADC 共享域检查共同配置和串行计算预算；它没有合成动态调度器。

同 ADC 时分参考由共同域明确持有当前 capture owner。两会话先完成接纳与准备，
再按完整载波的固定半周期偏移启动；每个时隙覆盖转换、IRQ、计算和重新装载。
公共 ADC IRQ 只分发给已发布 owner，不能按当前寄存器猜测归属。单轴退出不
撤回另一轴已获得的时隙；来源不再可信时关闭共同域准入。该选定实现已有生产
C 模型和 ARM 链接证据，启动容差与物理周期仍需实板验证。

共享域及其成员的生命周期操作使用同一个普通上下文执行者，或由应用显式共同
串行化；不能从每个 face 各自串行推导跨 Task 并发启停。快环保持其单一执行者，
平台不为此添加隐藏 OS 锁。

停止一轴先抑制其输出、撤回采集与回调，再移除其发布资格。其他成员仍运行时，
不能复位共同 ADC 或 detach 整个 IRQ。最后一轴退出后才归还共同效果；恢复失败
保留逐项 receipt。共同模式不再可信时，所有受影响成员关闭准入并抑制输出。

## 故障与退出沿着原 owner 完成

计算错误、角度过期、非法采样或完整输出失败关闭相应准入。Motor 算法故障不能
替代硬件抑制；应用仍按 PowerStage 合同 stop／service。输出关闭后，session 和
全部 context 仍保留到 acquire 观察 `SETTLED`。退出失败保持 `QUARANTINED`，
不能清零状态后复用，也不能用恢复同一个 epoch 数值伪装新会话。

受控减速依赖仍有效的反馈、供电和能量处理。硬件 stop 是输出与源撤回，不是机械
减速或功率隔离。升降承重、抱闸、机械回零和整机重新启动条件属于外部应用；电角
偏置、编码器参考和机械零位分别建立有效性。

## 软件项目提供各自范围的证据

独立示例仓库的双轮 SIL 运行实际 C 控制和独立 DC／机械模型；控制器只看模拟
传感器，验收器可以读取模型真值。轮速、里程计和地面真值分别报告。固定窗口缺样
不能填成零误差；课程参数只属于明确的教学 profile。

`recipes/control/dual_motor_drive` 另用两个独立平均 PMSM 电气／机械模型，量化
ADC 与编码器反馈给实际 Axis／Drive／Current／Motor／Modulation 组合。电流、
速度和位置目标、Task 失联及单轴采样故障走同一生产 C 组合；这不是把原 DC
模型改名，也不证明 STM32F4 的物理双轴时序。

`motor_drive` 可选 AS5047P Task 采集，通过独立有界角度通道交给快环；默认
消费者不因此链接 SPI。换坐标、重新绑定或复位通道前先会合 reader 和快环，
不得把旧缓存与新映射混用。整数位移是多圈权威，软件观察时间与实际孔径分开。

可选 INA226 母线来源同样由 Task 采集，每轴使用独立有界通道。有效期从实际
转换开始的保守时间下界计算，不能从 poll、发布或读取缓存的时刻续期；它不是
快速相电流采样或硬件过压保护。快环还要求母线资格覆盖计划输出边界，源故障
即使队列已满也独立关闭缓存准入。

`recipes/network/cia402_motor` 用同一个 Task 把真实 CANopen／CiA402 的选定
PV／绝对 PP 接到该控制组合。实际 RPDO 的原始本地 receipt 产生有限候选，且
受独立产品授权限制；SDO、心跳和协议轮询不续期。PP 确认等待匹配的快环
`command_sequence`，reset 只提出恢复请求，不启动硬件。该 Native 组合委托
原 CAN owner；退出失败保留协议与电驱两边的借用。

PMSM 数学模型、生产 C 寄存器模型、ARM 链接、安装 SDK 消费和物理测量是不同
证据。DC 模型通过不授予 PMSM 或实板能力；当前物理 HIL 保持 `not_executed`。
MCSDK 不是自有控制核的依赖，尚未接入的后端也不占位声明为已支持。

## 当前交付能力映射

下表按当前已维护能力关联生产实现、合同测试和 ARM／SDK 消费入口；
支持范围以精确 profile 与执行证据为准。

| 当前能力 | 生产 C | 维护合同测试 | ARM／SDK 与消费者边界 |
| --- | --- | --- | --- |
| 标定身份、两／三相换算与单分流重构 | [Current](../../components/control/current/src/current.c) | [电流测试](../../tests/behavior/components/control/current_control_test.cpp) | [FOC SDK](../../cmake/package/verification/foc_control/README.md) 消费相电流；单分流另由外部 single consumer 消费 |
| 电压决策与匹配周期 tracking | [Motor](../../components/control/motor/src/motor.c) | [电机测试](../../tests/behavior/components/control/motor_control_test.cpp) | 同一 FOC SDK consumer：Native 执行、F407 纯算法链接，不启动 PowerStage session |
| 完整脉冲、采样窗口与最终量化电压 | [相电流规划](../../components/control/modulation/src/phase_shunt.c)、[单分流规划](../../components/control/modulation/src/single_shunt.c) | [调制测试](../../tests/behavior/components/control/modulation_control_test.cpp) | FOC SDK 选择 phase planner；外部 single consumer 选择 bus planner；未选 planner 不因此进入镜像 |
| 有限命令、参考交接、连续反馈与 inhibit | [Axis](../../components/control/axis/src/axis.c)、[Drive](../../components/control/drive/src/drive.c) | [轴测试](../../tests/behavior/components/control/axis_control_test.cpp)、[交接测试](../../tests/behavior/components/control/drive_control_test.cpp) | FOC SDK 组合这条公共链；实际 Task／IRQ 分工由外部应用完成 |
| 消费／回生独立预算与实际限流 | [Drive](../../components/control/drive/src/drive.c) | 交接测试及外部双 PMSM 合同 | 外部 [双 PMSM SIL](https://github.com/X-Gen-Lab/nexus-examples/blob/eeed791310a1bb6e471d044f7f16f3f542dbe641/recipes/control/dual_motor_drive/sil.c) 实施预算；不是物理母线保护或功耗测量 |
| F4 相电流、中心采集与独立 ADC 双轴 | [PowerStage owner](../../soc/stm32/stm32f4/drivers/pwm_adc/power_stage.c) | [生产 C 寄存器模型](../../tests/behavior/soc/stm32/stm32f4/pwm_adc/power_stage_test.cpp) | 上表 control／single／fast／center／dual 为冷态 ARM fixture，不据此声称实板快环运行 |
| 单轴双 ADC 同时采集 | [ADC pair](../../soc/stm32/stm32f4/drivers/pwm_adc/adc_pair.c) 与 PowerStage owner | [pair 测试](../../tests/behavior/soc/stm32/stm32f4/pwm_adc/adc_pair_test.cpp)及寄存器模型 | `pair.toml` 同时进入 ARM fixture 和规范 Source SDK consumer，仍不测实际孔径 |
| 同 ADC 固定时隙双轴及共同退出 | PowerStage owner | 寄存器模型的时隙归属、共同启动与单轴退出合同 | `tdm.toml` 的 ARM／SDK 消费仅覆盖固定半载波参考，不开放 hot-join 或任意调度 |
| DC-link 两观察、一次计算与完整提交 | [采集叶](../../soc/stm32/stm32f4/drivers/pwm_adc/single_shunt.c)、[周期组装](../../soc/stm32/stm32f4/drivers/pwm_adc/power_stage_dc.c)及 PowerStage owner | [采集测试](../../tests/behavior/soc/stm32/stm32f4/pwm_adc/single_shunt_test.cpp)、[周期测试](../../tests/behavior/soc/stm32/stm32f4/pwm_adc/power_stage_dc_test.cpp)及寄存器模型 | `dc.toml` 的 ARM／SDK 消费只开放无相移边沿 profile；非零首计划和实际控制组合另由外部 single consumer 检验 |
| 合格角度／母线来源、PMSM 与 CiA402 整机组合 | 外部 [控制组合](https://github.com/X-Gen-Lab/nexus-examples/blob/eeed791310a1bb6e471d044f7f16f3f542dbe641/recipes/control/motor_drive/control.c)、Task adapter 与 [CiA402 桥](https://github.com/X-Gen-Lab/nexus-examples/blob/eeed791310a1bb6e471d044f7f16f3f542dbe641/recipes/network/cia402_motor/cia402_motor.c) | 外部 [维护合同](https://github.com/X-Gen-Lab/nexus-examples/tree/eeed791310a1bb6e471d044f7f16f3f542dbe641/tests/apps/motor_control) | `motor_drive` phase／single 有 ARM consumer；独立双 PMSM 与 CiA402 整机合同在 Native 执行，不据此声明 F4 物理双轴或 CAN 网络通过 |

Source SDK 的对应关系由[现有消费计划](../../tools/maintenance/consumer_profiles.py)
维护：`foc_control` 覆盖 Native／F407 公共算法组合，`f4_power_stage` 覆盖
pair／tdm／dc 三个精确冷态装配。本表不另定义一份消费选择。

ARM fixture 与 SDK 冷态 consumer 检验配置、公共接口、选编、链接和资源；
[fixture main](../../tests/integration/assemblies/f407_power_stage/main.c) 不启动电驱 session。
寄存器模型执行真实 provider C，PMSM 模型执行控制组合；两者都不能证明真实
开关、采样孔径、IRQ 延迟或电气保护。物理 HIL 仍为 `not_executed`。
未列出的拓扑组合、固定相移 mapper、TDM 运行中加入及非 F4 provider 保持未开放。

操作入口见[控制与智能器件](../guides/control-and-smart-devices.md)，权威公共声明见
[电机与轴 API](../reference/api/components/control.md)及
[耦合采样 API](../reference/api/io/analog.md)。
