# FreeRTOS Secure 配套镜像：显式双镜像上下文

Secure 镜像使用明确的 v8-M Secure CPU 事实与裸机后端，通过
`nexus_add_runtime(ASSEMBLY ...)` 建立现有生产 CPU／Arch 图。调用
`nexus_enable_secure_context()` 并链接 `Nexus::SecureContext` 后，增加上下文
网关和精确锁定的 Secure 汇编，不启动第二个内核。Nonsecure 镜像使用匹配的
Nonsecure CPU 事实和 FreeRTOS，并显式选择 `[os].security_model = "split"`。

模块关系见[OS 总览](../../README.md)，能力分层见
[FIG-13](../../../docs/assets/diagrams/cortex-m-capabilities.svg)。本模块不生成
板级内存或安全路由，图和软件检查不构成物理隔离资格。

## 双镜像集成

两镜像必须匹配 CPU 指令、FP、MVE 和浮点调用 ABI。Secure／Nonsecure 启动、
向量表、内存归属、入口、中断目标及 NSC 区域均由应用／SoC SDK 明确提供。
Secure 链接导出 CMSE 导入库，Nonsecure 链接消费其真实跳板；软件参考
夹具演示实际过程，不合成 Board 内存或安全路由。

分域模式选择真实锁定的 `ARM_CM{23,33,55,85}/non_secure` 具备 TrustZone 上下文支持的端口。
`ARM_CM*_NTZ` 汇编没有 Secure 保存／加载／分配，改变宏不能将它变为分域端口。
锁定内核的 Idle 也取得 Secure 上下文；调用者通过
`vApplicationGetIdleTaskMemory()` 提供静态 NS TCB／栈，并在不可变选择器中组合
匹配的 Secure 元数据／栈。`secure_idle_stack_bytes` 仅控制请求预算，默认
128 B，不提供存储。未知或未准备的身份直接拒绝。

## 调用者存储与租约

暴露 NS 任务前，准备自有 Secure 元数据和对齐的 Secure 栈，包括八字节栈封印（seal）。
受信且不可变的 `nx_freertos_secure_context_for_task()` 只返回已准备记录。
没有后备堆、默认最大池或动态对象发现。每个上下文以单调增加的 epoch 区分租约，
复合身份为 `(trusted task, epoch)`。释放保留元数据和代次历史；任务退出等待成功后
重绑也不能清零，代次耗尽拒绝新租约。

锁定的 NS SVC／PendSV 代码受信，NS Thread 不能进入仅允许 Handler 使用的上下文
操作。此机制不抵御恶意 NS 特权内核伪造任务参数；需要更强边界时，产品另行审查
Secure 策略。用户 MPU 任务和分域上下文仍为独立配置，在共同 SVC／CONTROL／ACL
合同获得资格前拒绝组合。

保存／加载使用真实锁定 PSP／PSPLIM 汇编，包括惰性 FP／MVE 完成，精确保存恢复
Secure 和 Nonsecure PRIMASK。仅跟踪单 CPU 当前加载的 PSP；这是活动借用，不是
上下文池。已加载的上下文不能释放。栈封印损坏、栈越界或 CPU 上下文损坏时保留
租约并进入明确的 Secure 故障停机。NMI／HardFault 不得访问这些元数据。

## 成本与验收

端口最小 64 B 不包括八字节栈封印（seal），也不是应用栈预算。另行预算实际 Secure 调用
深度、FP／MVE 异常栈帧、MSP、中断嵌套和诊断处理。宿主测试建立租约软件合同，
双镜像编译／链接建立网关与上下文代码证据；两者均不证明物理归属或栈峰值。
产品验收前需要真实分域 MCU 的 HIL。
