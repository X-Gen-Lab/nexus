# Linked resource costs / 链接后资源成本

`resources.py` measures the actual ARM ELF LOAD bytes, RAM interval unions and
linker MSP/heap reservations. `interfaces.py` separately checks readonly interface
faces and shared operation tables. Neither source-line counts nor selected
controller counts establish linked cost.

`resources.py` 从真实 ARM ELF 的 LOAD、RAM 区间并集和链接器保留区测量 Flash、
RAM、MSP 与 heap。`interfaces.py` 单独检查只读接口对象和共享操作表。源码行数或
选中控制器的数量不能证明最终镜像成本。

`soc_costs.py observe` binds the exact ELF, resolved configuration, resource
budget, complete actual compile database, selected source bytes and tools. Its
profile retains CPU/ABI, effective optimization/code-generation flags, clock,
backend, routes, mode options and stack/heap reservations. A comparison rejects
different profiles and requires explicit nonnegative growth ceilings for every
measured metric. Compiler receipts identify inputs; they do not attest unsigned
historical build commands.

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

`static_storage.py` adds disjoint RAM subsets from actual unstripped object
symbols: generated provider/endpoint state, generated RX/event/network buffers,
and FreeRTOS idle/timer stack and control storage. Exact aliases in the same
class count once; ambiguous overlaps or readonly objects masquerading as RAM
fail. `other_static` retains other named static objects. Alignment padding and
unattributed reservations remain explicit. These subsets are already inside the
RAM total and must not be added to it again. Arbitrary product buffers and task
stacks cannot be classified from guessed names and remain `other_static`.

`static_storage.py` 通过真实、未剥离的对象符号增加互不重叠的 RAM 子集：生成的
provider/endpoint 状态、RX/事件/网络缓存，以及 FreeRTOS idle/timer 的栈和控制
存储。同类的精确别名只计算一次；有歧义的重叠或被错误归类为 RAM 的只读对象会失败。
其他具名静态对象保留在 `other_static`；对齐空隙和未归属保留区单独记录。这些子集
已经包含在 RAM 总量中，不能再次累加。任意产品缓存和任务栈不能靠猜测名称分类，
因此保留在 `other_static`。

The minimal GPIO/UART workload uses existing generated typed factory IDs.
A single selected instance is unambiguous; multiple instances require an explicit
`-DNEXUS_WORKLOAD_UART_ID=NX_UART_ID_<NAME>` or corresponding GPIO ID. Invalid or
wrong-class IDs fail configuration. The workload selector does not alter the
authored TOML, create aliases or change platform construction.

最小 GPIO/UART 工装使用现有生成的 typed factory ID。仅有一个选中实例时直接使用；
多个实例必须显式传入 `-DNEXUS_WORKLOAD_UART_ID=NX_UART_ID_<NAME>`，GPIO 同理。
不存在或类别错误的 ID 在配置阶段失败。工装选择不会修改 TOML、制造别名或改变平台装配。

All results are linked storage observations. Dynamic stack high-water, cycles,
IRQ latency, electrical behavior and physical qualification require their own
executed measurements. An observation is not a software candidate or release.
Three-board HIL preparation uses the existing `tools/hil` exact-build protocols;
unknown station facts and numeric budgets remain unbound until equipment exists.

这些结果只证明链接后存储量。动态栈峰值、周期、中断延迟、电气行为和物理资格都需要
各自真实执行的测量。观察记录不是软件候选或发布。三板 HIL 准备沿用 `tools/hil`
绑定精确构建的协议；设备未接入前，站点事实和数值预算继续保持未绑定。
