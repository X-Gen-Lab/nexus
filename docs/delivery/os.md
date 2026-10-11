# OS 交付范围与验证

当前 OS 保持公共 Wait + 后端专属显式存储。平台不创建默认 worker、不启动 scheduler、
不隐式占用 task-notification index，也不创建最大对象池。产品 composition 和恢复策略
位于独立 examples/product 工程。

当前机制的完整合同在 [OS contracts](../design/os-contracts.md)，逐项实际状态在
[OS01–OS55 台账](../design/os-execution.csv)。`implemented` 只表示代码存在，只有
与当前 clean Git/配置/dependency/工具身份绑定的实际执行才建立软件资格。

| 机制 | 交付边界 |
|---|---|
| Wait | 单 waiter、多个合法 publisher、绝对同域 deadline、业务 predicate 权威 |
| Baremetal | 有界轮询，无任务仿真；睡眠须使用显式 Arch 合同 |
| Native | 调用方栈、join、closable byte-copy queue、真实 monotonic 条件等待 |
| FreeRTOS | 受维护九核 Port、静态对象、合法 task/IRQ 上下文、可 join 生命周期 |
| direct notify | 明示 receiver/index/生命周期，减少可选二值信号量成本 |
| permanent task | 无 join semaphore，entry 不返回且存储保留到 reset |
| closable queue | 独立 opt-in storage/waiters；raw queue 不承担默认关闭成本 |
| Tick/profile | SoC IRQ/CPU facts 与运行时政策分离，合法 Tick 来源明确 |
| low power | 浅睡眠＋显式 timer port；无 port 不自动睡眠 |
| trace | 调用方 bounded ring＋weak hook，无默认大型 trace buffer |
| MPU | 独立 restricted-task profile，protected metadata、显式 user linker/regions、窄 delay/tick SVC |
| split security | 匹配 Secure/NS Runtime、实际 TZ-aware port、caller Secure contexts 和 Idle 存储 |

## 外部应用交付

`nexus-examples/apps/uart_owner_runtime` 使用生成 typed factory 的 UART face。Native
创建两个 producer 和一个 owner 的真实 pthread；FreeRTOS 创建 producer/owner/stop
coordinator 的显式静态任务；裸机用同一 owner 进行 cooperative service。三板引用
已有 console route，不定义新的接线或 provider。

关闭流程包含 stop admission、active cancel/drain、producer join、owner join、IRQ wake
quiescence 和通知销毁。GoogleTest/GoogleMock 测试真实 BusOwner 和应用 shutdown 模块，
mock 只替代外部 join/硬件 quiescence；覆盖阶段顺序、超时、错误、提前回收和重试。

Native 的显式 host 栈为三个 64 KiB buffer；应用预算相应独立增加。这是 host thread
fixture 的资源，不是 MCU 栈成本。MCU 采用各 256 StackType_t word 的四任务参考预算，
声明 8-byte 对齐，仍需实板测量运行峰值，不能从启动最低容量推导。
FreeRTOS 示例停止 I/O 与发布者后保留 scheduler 所需平台时钟/Tick，coordinator 挂起；
Native/裸机可继续 platform stop。产品 reset/scheduler-stop 策略单独拥有。

## 软件验证与实板边界

软件执行应包含 deadline 持续 wake、最终完成竞争、端口启动栈拒绝、Baseline IRQ
完整绑定、close/drain、任务复用、direct notification receiver/index、tickless 失败
和 trace overflow。GoogleMock 上下文模型、真实 POSIX kernel、Native 多任务示例、
九核 ABI 链接和安装 SDK 消费分别建立自己的证据。

可选 MPU 的十二个实际 ARM 软件变体和 V7/V8 host contract 已执行；当前拒绝 M0/M0+、
M7 kernel MPU 及 MPU+split。可选 split 的 31 个 Secure/NS 配对（62 ELF）覆盖当前
M23/M33/M55/M85 CPU authority 组合，并核对七个真实 SG veneers、import、上下文 ASM
与 caller Idle callback，没有默认 Secure heap/pool。split 的两个 host lease targets
分别覆盖整数和共享 FP/MVE bank 软件合同。两类都需要消费者提供明确 linker、存储
和可信生命周期；软件 fixture 的地址是 synthetic，未执行实际 MPU fault、安全归属、
跨世界启动或真实 context switch。细节见 [OS contracts](../design/os-contracts.md)。

OS49–55 的物理工装定义已经可执行为 prepare/validate 工具，但硬件尚未连接，未运行
实际 IRQ→task 延迟、FP/MVE register retention、MSP/task peaks、Tick/deadline 误差、
满载生命周期、睡眠恢复或异常安全输出。准备计划保持 `prepared_unbound`，物理状态
保持 `not_executed`。计划不是测量结果，更不是 WCET、安全认证或产品 release。

```sh
python tools/hil/os_acceptance.py plan \
    --resolved build/stm32-qiming-freertos/generated/resolved.json \
    --report build/stm32-qiming-freertos/os-hil-plan.json
```

实板接入后由 station owner 配置五项明确预算 `os_irq_latency`、`os_deadline_lateness`、
`os_low_power_lateness`、`os_lifecycle`、`os_fail_response`；分别使用 us/us/us/count/us。
生命周期必须提供正 minimum，其他四项使用 maximum。记录与 station admission 绑定，
不能在 acquisition context 内放宽预算。

OS context 严格字段为 schema_version=1/kind=os_hil_context、admission_sha256、
workload/instrument file identities、实际 load/executor/irq_priorities/cpu_clock_hz、
features.fpu/mve、完整 `stack_names` 和 requirements。stack_names 列出 MSP 及该 workload
每个选定任务，raw stack trace 必须完全对应，不能只提交一个示例任务。features 与已
admission 的 CPU ABI 完全一致。
每项独立 raw trace 绑定 context SHA，保留逐样本/寄存器/stack/lifecycle/fail-stop 数据，
不能传入一个自行填写 `status=passed` 的摘要替代。采集时间需 fresh UTC offset。

```sh
python tools/hil/os_acceptance.py validate \
    --admission station/admission.json \
    --raw-evidence station/os-run.json \
    --report station/os-validation.json
```

报告要求未使用的输出目标，旧证据与输入永不覆盖。
这些命令只读取已保留记录，绝不自动操作硬件。测量方法、观测 load、供电、安全输出和
产品验收预算由实际 station/workload 审核，不能继承主机模型或旧提交的物理资格。

## 显式优化成本

32-bit ABI 的 `standard` development resource probe 已测得下列对象尺寸；具体 profile、
CPU Port、枚举 ABI 和编译器改变后须重新执行成本门禁。这些是实际 ELF 对象大小，
不表示运行 cycles 或最终软件候选资格。

| 对象 | RAM bytes | 成本责任 |
|---|---:|---|
| Wait face | 16 | 保持现有公共 ABI |
| 普通 notify | 84 | 每对象静态 latch |
| direct notify | 16 | receiver/index 明示，省 68 B |
| 可 join 任务 | 172 | 不含 caller stack |
| 永久任务 | 88 | 不含 caller stack，省 84 B |
| raw queue | 80 | 不含 payload |
| closable queue | 96 | 比 raw 多 16 B，不含 payload/waiters |
| 每个 concurrent queue waiter | 84 | 调用方按实际并发数提供 |
| Idle TCB＋默认栈 | 596 | 默认 84＋512 B；profile 可以改变 |

`maximum_waiters` 控制 close 的 scheduler-pause 遍历预算，payload item size 控制内核
单次复制的临界区预算。不可把整个 close 遍历的时间宣称为 IRQ 屏蔽时间，也不可用
静态 instruction 数代替测量延迟。成本门禁保留各自 argv、ELF/nm/disassembly 和尺寸；
资源最小 profile 与完整能力 profile 分别比较，禁止把不同负载的图像作为同一基线。
