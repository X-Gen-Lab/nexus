# Linked resource costs / 链接后资源成本

`resources.py` measures the actual ARM ELF LOAD bytes, RAM interval unions and
linker MSP/heap reservations. `interfaces.py` separately checks readonly interface
faces and shared operation tables. Neither source-line counts nor selected
controller counts establish linked cost.

Shared operation tables remain identifiable after source grouping under
`io/src/controllers/` and `io/src/mechanisms/`. The audit derives exact symbols
from maintained source initializers and checks their actual loaded readonly ELF
storage separately from instance faces. Vendor or similarly named tables do
not become accepted interfaces merely through a name prefix.

`resources.py` 从真实 ARM ELF 的 LOAD、RAM 区间并集和链接器保留区测量 Flash、
RAM、MSP 与 heap。`interfaces.py` 单独检查只读接口对象和共享操作表。源码行数或
选中控制器的数量不能证明最终镜像成本。

共享操作表移动到 `io/src/controllers/` 或 `io/src/mechanisms/` 后仍须正确计量。
审计从维护源码的真实初始化器获得精确符号，检查 ELF 中实际加载的只读存储，
与各实例接口分别计量。厂商目录或任意相似名称的表不会仅凭前缀被认定为公共接口。

`compare.py --parallel 1` explicitly bounds build concurrency and records it in
the matrix; one job is the default. Each requested workload/optimization uses a
new build directory and actual ELF. Board profile files must already exist;
missing reviewed routes are not invented to complete a benchmark matrix.
Different modes or workloads may show marginal linked cost, but they are not
equivalent-profile optimization regressions. Keep those comparisons separate
from `soc_costs.py compare`, which enforces equivalent recorded profiles.
Profiles use the production TOML schema-2 entry only. Derived inputs preserve
the authored tables while relocating root Board/layout paths and replacing the
explicit optimization choice; they never re-enter through a JSON assembly.

`compare.py --parallel 1` 显式限制构建并发并将其写入矩阵，默认只用一个 job。
每个负载和优化组合使用新构建目录与真实 ELF；所需板级配置必须已经存在，
不会为了凑齐矩阵而虚构路线。不同模式或负载的差量可以说明链接成本，不能当作
等价条件下的优化回归；等价比较仍由 `soc_costs.py compare` 严格核对记录条件。
配置只走生产 TOML schema 2 入口。派生输入保留原有表，仅迁移根级 Board／layout
路径并替换显式优化选择，不再通过 JSON 装配入口重新进入解析器。

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

`h743_flash_ram.py` binds the selected RAM audit to the actual compile database
and original compiler stack-usage (`.su`) bytes. For reproducible builds, each
translation unit's exact `-c` input and ordered `-ffile-prefix-map` /
`-fdebug-prefix-map` arguments determine one expected source spelling. The last
matching map applies once; paths are never accepted by basename or suffix.
Wrong sources, ambiguous source spellings and unmodelled path options fail.
Original source and `.su` hashes remain intact, and failed CLI audits print the
specific rejection reason. These checks do not establish physical execution.

`h743_flash_ram.py` 将选定 RAM 审计绑定到实际编译数据库及编译器生成的原始
栈用量（`.su`）字节。可复现构建中，每个编译单元的精确 `-c` 输入和有序
`-ffile-prefix-map`／`-fdebug-prefix-map` 参数只派生一个预期源码名称。
最后匹配的映射只应用一次，不使用文件名或路径后缀放行。错误源码、有歧义的
源码名称及未建模的路径选项均拒绝。原始源码与 `.su` 摘要保持不变，命令行
审计失败时输出具体拒绝原因。这些检查不建立实板执行资格。

All results are linked storage observations. Dynamic stack high-water, cycles,
IRQ latency, electrical behavior and physical qualification require their own
executed measurements. An observation is not a software candidate or release.
Three-board HIL preparation uses the existing `tools/hil` exact-build protocols;
unknown station facts and numeric budgets remain unbound until equipment exists.

这些结果只证明链接后存储量。动态栈峰值、周期、中断延迟、电气行为和物理资格都需要
各自真实执行的测量。观察记录不是软件候选或发布。三板 HIL 准备沿用 `tools/hil`
绑定精确构建的协议；设备未接入前，站点事实和数值预算继续保持未绑定。

`retention.py` normally requires generated factory faces. Only the four exact
canonical U575 retained-time, Stop2, LPDMA-copy and LPTIM1-LPBAM consumers use
`--component-owner <smoke> --consumer <main.c>` together with `--resolved`.
Their role comes from the existing transport tuple and matching canonical
consumer bytes and assembly digest. The audit follows actual readonly clock
and component faces into writable, nonexecutable caller storage and concrete
loaded code/tables. Its `linked_component_owner_retention` report establishes
linked component ownership, not factory retention, execution or HIL. Other empty
assemblies still fail; this mode does not relax factory-face checks.

`retention.py` 默认必须验证生成的 factory 接口对象。仅四个精确的 U575 标准
retained-time、Stop2、LPDMA-copy 和 LPTIM1-LPBAM consumer 可同时使用
`--component-owner <smoke> --consumer <main.c>` 与 `--resolved`。角色由现有
transport 元组、相同的标准 consumer 字节和装配摘要共同确定。检查从真实只读时钟与
组件接口追踪到可写且不可执行的调用方存储，以及实际加载的代码和操作表。
`linked_component_owner_retention` 报告只证明组件的链接关系与存储所有权，
不证明 factory 保留、实际执行或 HIL；其他空装配仍拒绝，factory 接口检查保持严格。
