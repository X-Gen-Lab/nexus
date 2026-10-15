# 故障观测与保留责任

Core 的 `nx_fault_observe(domain, reason, file, line)` 是一个可覆盖的有限
观测入口。弱默认实现没有状态、分配、队列、OS 锁或副作用；没有隐含事件
对象、registry、worker 或实例 RAM。四个标量参数避免将临时事件结构作为
借用对象传给产品。产品若需要记录，必须提供一个强实现和自身的静态存储。

## 终止调用路径

维护的 FreeRTOS 默认 assert/stack-overflow hook 在 park 前调用观测入口。
stack hook 不解引用可能损坏的 task/name。返回的 observer 不会允许损坏
的内核继续执行，也不会释放 request、栈、TCB、IRQ 或 DMA 借用。

FreeRTOS terminal path 用只读 Arch leaf 读取当前 masks。已有 PRIMASK、
FAULTMASK，或者等于/强于 kernel ceiling 的 BASEPRI 保持不变；其他情况
使用实际 port 的禁用操作。这样不会被 Mainline port 的直接 BASEPRI store
减弱已有 mask。Baseline/POSIX 编译仍遵循各自 port，不声称主机有 MCU IRQ。
这是单核终止合同；observer 必须保持进入时的 masks。

`nx_freertos_assert_failed` 和 stack hook 本身仍允许强覆盖。直接覆盖它们
会替换默认观测，因此产品须显式保留 `nx_fault_observe` 调用，并保证
non-returning。公共声明保持 C11/C++ ABI；返回到损坏的 `configASSERT` 调用者
不属于允许行为。

ThreadX 在中央 adapter fail-stop 和 M4 外部 timer setup 失败处，先观测，再
调用产品 fail-stop port。产品 port 返回时 adapter 继续保留并 park；不会
假装 kernel startup、任务 borrow 或 clock effects 已撤销。其支持范围仍以
ThreadX 文档与实际 target 为准，不继承所有 FreeRTOS CPU 组合。

## 产品合同与限制

observer 在原来的 task、ISR 或已屏蔽中断上下文运行，权限和上下文不会被
规范化。它必须有界、无分配，不获取 OS 锁、不进入 provider、不改变
accepted request、不释放 retained storage，也不能假定 kernel、clock、
stack、transport 或 peripheral 仍工作。文件名仅为静态源码 literal 或
NULL；不能解引用未知动态内存。损坏栈的观测是 best effort。

kernel ceiling 不是所有中断的全局静止证明，也不撤销 NMI、DMA 或外围电气
效果。该 API 不实现安全输出、复位、Flash 持久化、loan 回收或安全认证。
产品可实现另行审查的 non-returning recovery；责任仍由外部工程承担。

`nexus-examples/apps/uart_owner_runtime/fault_policy.*` 演示一个 caller-owned
首故障记录和明确的产品 output port。先 release 发布诊断，再调用 port；
嵌套/重复报告不能重复输出动作或替换首个故障。未提供或失败的 port 保持
未验证，不 cleanup/reset/reclaim。示例默认不选择任何 GPIO 或假定板级安全
电平。成功的软件 callback 也不证明实际电气安全，仍需产品/Board/HIL 验收。

Google death contracts 验证默认 hook 的观测→mask→park 顺序、坏 task/name
不解引用、返回 observer 保留 loan，以及强/等/弱 BASEPRI、PRIMASK 和
FAULTMASK。它们使用真实默认生产 hook 和明确的 port/Arch leaf 模型；
实际故障后的可记录性、物理安全输出及最坏时间没有实板资格。
