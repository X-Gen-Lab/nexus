# 链接后资源成本与测量边界

`resources.py` 从真实 ARM ELF 的 LOAD、RAM 区间并集和链接器保留区测量 Flash、
RAM、MSP 与 heap。`interfaces.py` 单独检查只读接口对象和共享操作表。源码行数或
选中控制器的数量不能证明最终镜像成本。

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

`retention.py` 默认验证生成的 factory 接口对象。仅精确的
`f407-secure-services` 标准 consumer 支持 `--component-owner`；它同时要求
`--consumer <application.c>` 与 `--resolved`，按维护的 consumer、装配及源码字节
校验身份，再追踪真实接口、操作表与可写调用方存储。该分支不授权其他空装配。
链接保留报告只证明当前 ELF 的代码与存储关系，不证明运行、物理 HIL 或发布资格。
