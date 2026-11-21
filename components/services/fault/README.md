# 原始故障记录与可选 CmBacktrace

`Nexus::Fault` 把一份版本化的 160 字节记录写入调用方明确提供的保留 CPU 内存。
捕获只记录数字事实、有效位和 16 字节构建标识，不依赖 Log、RTT、OS ring、Kernel、
时钟、Flash 或数据库，不解引用文件名或任务名指针。CRC32 用于发现损坏，不提供
认证，也不证明物理保持或掉电行为。

首个捕获占有记录，直到正常上下文明确认。一次 weak exclusive 尝试获取提交字，
不在嵌套故障中无限重试；完成标记最后发布。当前原始捕获实现用于已审 Cortex-M4
路径及 Native 软件合同，ARMv6-M 故障捕获不受支持。

## 异常入口与栈来源

`nexus_add_fault_m4_entry()` 显式创建 `Nexus::FaultM4Entry` OBJECT target，独占
HardFault、MemManage、BusFault、UsageFault 四个强入口。每镜像只链接一份；另一份
强 handler 应产生链接冲突。该 target 不安装 NMI。

汇编先取得 EXC_RETURN、原始 MSP/PSP 和 IPSR，再切换到产品提供的独占、八字节对齐
应急栈。镜像在装配点定义 `nx_fault_m4_profile` 和
`nx_fault_m4_emergency_stack_top`，提供原始静态 MSP／任务栈分配、保留记录、构建与
序号事实，以及可选的有界提交后回调。任务栈采用原始分配 binding，不查询 Kernel，
不依赖 RTOS TCB 偏移。应急栈预算、可读内存、复位保持域和所有分配的生存期归产品。

先采集 SCB 状态；CFSR 的 stacking／unstacking／lazy-state 错误位 `0x3838` 会阻止
读取原栈。`nx_fault_m4_frame_copy` 按 EXC_RETURN 选择 MSP/PSP，核验整个 basic／
extended frame 及 xPSR 对齐填充。依 PM0214 Rev10 图12，八个 core 字始终位于 raw SP
起始处；扩展的 18 个浮点字位于其后，整个 extended frame 为 26 字。组件不复制浮点
寄存器。边界无效时保留原始 SP／EXC_RETURN，清除 frame 有效位；TrustZone 和其他
Cortex-M 布局不由此获得支持。数字地址和边界检查不能证明损坏后的任意地址可读。

## 回溯、启动与持久化

`nexus_add_cm_backtrace()` 选择锁定的真实上游，提供 `Nexus::CmBacktrace`；在故障前
显式初始化。代码范围来自当前镜像 live Flash linker 边界，不覆盖 RAM 执行函数。
同次启动中的解释使用原始被中断栈 binding 和明确扫描字数预算，调用未修改的
`cm_backtrace_call_stack_any`。不启用或修补依赖过期 `vTaskStackAddr/Size/Name` 的
上游 FreeRTOS 分支，不安装上游 handler 或普通 printer。

达到返回地址容量后，上游仍可能扫描整个准入范围，因此地址深度不等于扫描时间上界。
回溯采用启发式，优化和尾调用可能丢失帧；它不能取代原始记录、持久化或精确 ELF。
复位后不再把旧栈内容当作同次启动的有效回溯来源，应解码保留寄存器记录。

`nx_reset_record_read` 把已锁存的 `nx_reset_cause()` 位与可选故障副本组合，保留多重
原因，不清硬件标志，也不决定恢复。`Nexus::FaultStorage` 在正常启动／任务中使用
已打开、产品选定的 `Nexus::Storage`，持久提交成功后才确认保留内存。修改失败或
模糊提交保留原记录，并遵守 Storage 的 reopen／恢复合同。重复处理、区域、认证、
保存限额和恢复／复位策略归产品；未提交记录的处置也须显式决定。

Native 建立 wire、边界、占有和软件存储组合证据；ARM 链接建立真实上游和 ABI 证据。
实际 MSP/PSP/FPU 异常、复位保持和 Flash 掉电行为，在精确板卡执行前仍未获得资格。
