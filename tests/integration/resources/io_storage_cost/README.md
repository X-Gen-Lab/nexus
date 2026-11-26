# 显式 I/O 存储成本夹具

C11 link-only 保留真实 `nx_bus_owner_t`、`nx_owner_slot_t`、UART request 和
可选 observer。`COST_SLOTS=1/4`、`COST_OBSERVER=0/1`；每 slot 另保留一个
request 和 32 B caller payload，分别报告。五 word 地址根使真实符号存活，不猜
大小、不执行操作；这些 anchor 是测量开销，不是平台要求。

使用维护 ARM GNU、选定 CPU／FPU／float／enum ABI、
`-Os -ffunction-sections -fdata-sections`，Core／I/O／OS／BusOwner／Observer
include、`-nostdlib -Wl,--gc-sections`，以及
[`os_wait_cost/linker.ld`](../os_wait_cost/linker.ld)。从 ELF 符号／section 读大小
地址，保留真实命令、编译器、输入 hash、exit 与四份 ELF。slot、request、payload、
observer、owner、anchor 分别比较，padding 独立。

没有 startup、Board、operation callback、IRQ、scheduler 或可运行产品，只证明
选定 ABI 的链接分配成本，不证明吞吐、延迟、动态栈、所有权、HIL 或完整 transport
成本。实例／controller／mode 成本由实际固件矩阵测量，8 B face 不代表完整镜像。
公共观察布局变化需用同参数对精确旧 header 另编 size-only object，保留两身份；
字段落旧 padding 也需重编消费者，相同大小不证明跨 ABI 兼容或零成本。

[SPI endpoint 夹具](spi_endpoints/README.md)另链 0／1／2／4 endpoints，走 factory、
topology validation 与 polling loop，非零共 SPI1。原一／二 Board 保留 PA5／6／7、
PE4／PE3 软件 pad 合同，四路另用 ZGT6 PG9／PB2，三个 CS bank 增加 GPIO lease／
activation，因此 2→4 不是线性 endpoint 差量。后端／clock／CPU／优化／stack／heap
一致，zero 不选 SPI。对每 ELF 用既有 resource／storage／interface／retention，
空 retention 记不适用。整镜像与 state／face／table／GPIO／consumer 成本分别报告。
早期 endpoint-only workload 未保留未用 controller face，比较前明确此覆盖差异；
软件 Board 不声明 PCB 接线。
