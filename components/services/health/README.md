# 显式健康与资源观察

`Nexus::Health` 组合现有 I/O 诊断的稳定值副本和显式 progress。缺失事实保持 unknown；
零 findings 不等于健康证书、喂狗许可或恢复授权。Progress 使用调用方提供的同域
单调时间和不循环复用的 generation；有效工作定义、预算、发布同步和执行者归产品。

资源采样复用 `nx_stack_observe`、普通 diagnostic ring 的 dropped 查询，以及可选的
FreeRTOS 静态任务观察接口。不枚举 Kernel、不给 live stack 染色，也不挂起任务。
采样须有原始容量、初始 paint 来源和完整调用期生存期；范围必须静止，排除当前 SP、
live frames 和异常预留。未写入的栈预留或与 paint 相同的写入可能不可见，因此这些
事实不代表物理峰值。

`Nexus::HealthFreeRTOS` 仅在显式 FreeRTOS trace profile 下出现，使用原始
`nx_freertos_stack_binding_t` 和既有 running／特权／未掩码／静止栈校验，不读私有
TCB 偏移，不在 fault 中采样。POSIX 测试 port 使用独立 pthread 执行栈，静态数组的
观察结果不测量 MCU 或 Linux 执行栈峰值。

`Nexus::DiagnosticLog` 每次显式任务调用最多消费一条普通 ring 记录。调用方的一条
consumer slot 在 Log 返回 BUSY 或其他错误时保留已取事件，不继续消耗后续事件。
ring 访问结束后才格式化和输出；沿用其特权单 CPU producer 合同，排除 NMI/HardFault。
组件不增加第二队列、采样任务、堆、自动喂狗或复位。
