# 链接后资源成本与测量边界

[资源预算 schema](resource-budget.schema.json)说明编辑时使用的 JSON 字段。
实际测量还核对映像、装配输入和预算身份，并检查字段间约束。

`resources.py` 从真实 ARM ELF 的 LOAD、RAM 区间并集和链接器保留区测量 Flash、
RAM、MSP 与 heap。`interfaces.py` 单独检查只读接口对象和共享操作表。源码行数或
选中控制器的数量不能证明最终镜像成本。

## F407 固定软件负载

| 负载 | 固定入口 | 选定条件与存储 |
| --- | --- | --- |
| 最小 GPIO | [Qiming GPIO TOML](../configure/assemblies/qiming-baremetal-gpio.toml) 和 `NEXUS_WORKLOAD=gpio` | STM32F407ZGT6 Qiming v3.1、HSE8／PLL168、baremetal、`Os`，只选 LED 输出 |
| 最小 UART | [Qiming UART TOML](../configure/assemblies/qiming-baremetal-uart.toml) 和 `NEXUS_WORKLOAD=uart` | 同型号、Board、时钟、后端和优化，USART1 `irq-byte-event`、32 个 RX event |
| 完整网络 | [Qiming 网络参考](../../tests/integration/assemblies/f407_qiming_network/README.md) | STM32F407ZGT6、HSE8／PLL168、baremetal、`Os`，DP83848、copied-RX lwIP TCP、TLS、MQTT、8 KiB MSP |
| 完整 phase FOC | [例库 `motor_drive_reference`](https://github.com/X-Gen-Lab/nexus-examples/tree/main/recipes/control/motor_drive) | STM32F407ZGT6、HSE8／PLL168、baremetal、`Os`，phase 电流、PowerStage、编码器、Axis／Drive／Motor、2 KiB MSP，默认未 commissioning |

GPIO 与 UART 各保留 2 KiB MSP。所有四项选定 `libc_heap_bytes = 0`，没有 RTOS
任务栈或 TCB。Qiming USART1 的最小已准入 UART 参考使用 IRQ。请求 `mode = "poll"`
由选定装配拒绝，因而没有该模式的成本记录。

以一对 GPIO 和 UART 参考为例，保持两个独立构建目录并沿用各自的 TOML。

```sh
for workload in gpio uart; do
    cmake -S . -B "/absolute/new/$workload" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
        -DNEXUS_BUILD_TESTS=OFF -DNEXUS_WORKLOAD="$workload" \
        -DNEXUS_ASSEMBLY_FILE="$PWD/tools/configure/assemblies/qiming-baremetal-$workload.toml"
    cmake --build "/absolute/new/$workload" --target nexus_firmware --parallel 1
done
```

网络和 phase FOC 各使用自身入口生成完整 firmware。每项保留原 TOML、resolved、
实际编译数据库、编译器版本和摘要、ELF／map／预算及实际命令与退出码。工具按 ELF
计算 Flash LOAD、Flash footprint、RAM 区间并集和独立的 MSP／heap 视图。
需要定位差异时再读取实际静态对象，避免重复计算已包含于 SRAM 总数的预留。

同一固定负载的前后观察可以提交给 `soc_costs.py compare`，由该工具检查等价条件。
四项负载自身的数值只描述各自的精确镜像。

## 链接计量与等价条件

共享操作表移动到 `drivers/` 或 `io/src/mechanisms/` 后仍须正确计量。
审计从维护源码的真实初始化器获得精确符号，检查 ELF 中实际加载的只读存储，
与各实例接口分别计量。厂商目录或任意相似名称的表不会仅凭前缀被认定为公共接口。

`compare.py --parallel 1` 显式限制构建并发并将其写入矩阵，默认只用一个 job。
每个负载和优化组合使用新构建目录与真实 ELF；所需板级配置必须已经存在，
不会为了凑齐矩阵而虚构路线。不同模式或负载的差量可以说明链接成本，不能当作
等价条件下的优化回归；等价比较仍由 `soc_costs.py compare` 严格核对记录条件。
配置只走生产 TOML schema 2 入口。派生输入保留原有表，仅迁移根级 Board／layout
路径并替换显式优化选择，不再通过 JSON 装配入口重新进入解析器。

`soc_costs.py observe` 绑定精确 ELF、resolved 配置、资源预算、实际编译数据库、
源码字节及工具。比较条件保留 CPU/ABI、最终优化和代码生成参数、时钟、后端、路线、
模式选项及栈/heap 保留区。不同条件拒绝比较；每个指标都需要显式非负增长上限。
编译记录标识输入，不能对无签名的历史构建命令提供真实性背书。

```sh
python tools/measurement/soc_costs.py observe \
    --elf build/target/bin/nexus_firmware.elf \
    --resolved build/target/generated/resolved.json \
    --budget build/target/generated/resource-budget.json \
    --compile-commands build/target/compile_commands.json \
    --output evidence/new-cost-observation
python tools/measurement/soc_costs.py compare \
    --baseline evidence/baseline/report.json \
    --candidate evidence/new-cost-observation/report.json \
    --growth evidence/reviewed-growth-limits.json \
    --report evidence/cost-comparison.json
```

`static_storage.py` 通过真实、未剥离的对象符号增加互不重叠的 RAM 子集：生成的
provider/endpoint 状态、RX/事件/网络缓存，以及 FreeRTOS idle/timer 的栈和控制
存储。同类的精确别名只计算一次；有歧义的重叠或被错误归类为 RAM 的只读对象会失败。
其他具名静态对象保留在 `other_static`；对齐空隙和未归属保留区单独记录。这些子集
已经包含在 RAM 总量中，不能再次累加。任意产品缓存和任务栈不能靠猜测名称分类，
因此保留在 `other_static`。

最小 GPIO/UART 工装使用现有生成的 typed factory ID。仅有一个选中实例时直接使用；
多个实例必须显式传入 `-DNEXUS_WORKLOAD_UART_ID=NX_UART_ID_<NAME>`，GPIO 同理。
不存在或类别错误的 ID 在配置阶段失败。工装选择不会修改 TOML、制造别名或改变平台装配。

```sh
python scripts/ci/os_resources.py --output /new/os-resource-observation \
    --core-arch-costs --native-commands build/native-debug/compile_commands.json
```

既有 OS 资源矩阵提供可选 Core/Arch 探针，共用 Runtime 目标与编译器，不另建固件
源码图。八组 M0/M4、standard/minimal、Os/O2 记录实际 OS 对象，以及 0/1/2/4 个
Core 实例的 ELF、map 和静态指令形状。Native 编译数据库提供维护目标的真实参数，
用于执行 wait/select 与诊断边界调用计数；实际观察 N=1/4/16 扫描和调用方维护的
循环游标，调用方仍须保证 predicate 有界。队列深度、item、waiter 表明确属于
`derived_budget`，由实测 ABI 和声明维度推导，不是吞吐实测。IRQ 保存/恢复与
调度暂停分别说明；不增加默认生产仪表状态，也不建立周期、栈峰值、竞争上界或
物理资格。输出目录必须为新目录。

这些结果只证明链接后存储量。动态栈峰值、周期、中断延迟、电气行为和物理资格都需要
各自真实执行的测量。观察记录不是软件候选或发布。四份 F407 板卡 HIL 工装准备沿用 `tools/hil`
绑定精确构建的协议；设备未接入前，站点事实和数值预算继续保持未绑定。

## 有界软件观察与采集计划

选定时为每个独立向量或唯一 Task 执行者提供一个零值 `nx_io_cycle_probe_t`，
在同一次有界工作前后使用独立的 `nx_io_cycle_token_t`。Task 从
`nx_io_cycle_probe_snapshot` 复制计数器可用性、原始端点和模差，固定记录再由
产品调度的维护任务消费。工作无论探针结果如何都推进一次。未启用的 IRQ 宏不创建
sidecar 或求值参数。采用现有可选 service wrapper 时，NULL 跳过采样，原操作仍执行。

同一启动域内用 `nx_time_now_us()` 记录软件事件时刻。用调用方固定容量的
`nx_diagnostic_event_t` 数组初始化现有 ring，每个采样点最多写入一项。ring 满时
保留已有项并计入 dropped，调用方按预算读取，并在退出前 join 发布者和读者。
在维护任务中以 `nx_health_stack_resources` 读取 ring 的 loss 和可选的原始栈 paint
区间，首次 paint 与采样都要求合同规定的静止边界。缺少输入时保持 unknown；
无事件、缺失计数器或未知栈都保留原来的语义。

`nexus_cycle_probe_test`、`nexus_cycle_probe_disabled_test`、
`nexus_cycle_probe_native_test`、`nexus_io_observation_contract_test`、
`nexus_os_diagnostic_test` 和 `nexus_common_diagnostics_vendor_test` 覆盖 wrap、
不可用／未选定、空 ring、overflow 和安静栈观测。独立采样时保留时钟、装配、样本数、
采样周期、负载和 dropped。软件观测本身不提供 DWT epoch 连续性、IRQ 到达延迟或
真实的 stack high-water。参见 [栈观察合同](../../docs/reference/contracts/platform/stack-observation.md)。

为同一精确 Board 的已解析构建准备后续采集。

```sh
python tools/hil/measure.py plan \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved /absolute/qualified-build/generated/resolved.json \
    --report /absolute/new/qiming-measurement-plan.json
```

报告为 `prepared_unbound`，各物理项保持 `not_executed`。实际站点必须绑定真实
镜像、工作负载、探针、校准、测量时钟和数值预算，并采集原始数据。参见
[资源与硬件验收指南](../../docs/guides/performance-hil.md)。

`retention.py` 默认验证生成的 factory 接口对象。仅精确的
`f407-secure-services` 标准 consumer 支持 `--component-owner`；它同时要求
`--consumer <application.c>` 与 `--resolved`，按维护的 consumer、装配及源码字节
校验身份，再追踪真实接口、操作表与可写调用方存储。该分支不授权其他空装配。
链接保留报告只证明当前 ELF 的代码与存储关系，不证明运行、物理 HIL 或发布资格。
