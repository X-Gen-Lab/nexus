# HAL、OSAL 与 CPU 契约

本文以当前公开头和生产实现为准；原`affaa86f`提案不再用作可编译接口。实际定向检查、finalclean 与 physical 范围见 [RF 执行记录](../implementation/refactor-execution.md)。STM32/GD32typedI2Chardware、完整 kernel 热重启、cache/MPU 和未 reviewed 外设明确未实现。

## 目标边界

| 目标 | 实际职责/依赖 |
|---|---|
| `Nexus::HALCore` | 目录、opaqueidentity、owner/generation 和最小错误；privateArch/providercontract，无 OSAL/vendor/Board/framework |
| `Nexus::HALGPIO/UART/SPI/I2C/Flash` | 独立 typedfacade，只拉 core 及必要 Arch，不强制整个设备集合 |
| `Nexus::HALDevice` | 上述 facade 的显式便利集合 |
| `Nexus::HALProviders` | providerregistration/state 契约，普通应用不自动包含 providerheader |
| `Nexus::HALSupport` | HALbootstrap、内存/互斥/电源 support，明确 OSAL 依赖 |
| `Nexus::HALRuntime` | finitebudget/UARTtransfer 等待，显式 clock/waitport，无默认 worker |
| `Nexus::HALRuntimeOSAL` | 使用真实 OSALclock/taskwait 的 optionalport |
| `Nexus::HALCompletion` | caller-owned 有限 terminal 队列，privateArch，无 OSAL/heap/worker |
| `Nexus::OSAL` | 当前 Native、baremetal 或 pinnedFreeRTOSadapter；查询能力与预算 |

公共 device 类型位于`hal/base/nx_device.h`，provider 完整 state/registration 位于显式`hal/provider/nx_device_provider.h`。同一目录可见 providerheader 不代表普通 typedconsumer 获得 mutabledevice 定义。vendorSDK 和 controller 运行结构留在 provider 编译面；`nx_hal.h`不自动引入 factory 内部入口。

## 发现与资源取得

真实 API 不是历史 deadline-open 提案：

```c
nx_status_t nx_device_discover(const char *name, nx_device_class_t expected,
                              const nx_device_t **out);
nx_status_t nx_device_open(const char *name, nx_device_class_t expected,
                          uintptr_t owner, nx_device_ref_t *out);
nx_status_t nx_device_query(nx_device_ref_t ref, nx_device_caps_t *out);
nx_status_t nx_device_close(nx_device_ref_t ref);
nx_status_t nx_device_recover(const char *name, uintptr_t owner);
```

Discover/describe/query 不初始化硬件。open/close 是 taskcontext 的 no-waitadmission；providerlifecyclecallback 必须有界、非阻塞，不能在 core 短 metadata 区运行厂商初始化。不存在、类别不符、不支持、资源 busy、初始化失败/故障保持区别；失败 output 无效。

`nx_device_ref_t`含 opaquedescriptor、owner、generation、class。close 成功使旧副本失效，失败保持 ref 可 retry。failedopencleanup 如果无法证明 deinit，instance 隔离；recover 由同 owner 重试，不允许 anothercaller 绕过故障后重新 open。normaltyped 操作全部重新检查 class、owner/generation 和 phase。

SPI/I2Cchild 引用含 parent、slot 与 generation；有界 childpool，无 hiddenallocation。父 close 等所有 child 结清；childclose/deinit 失败保留状态。lifetimeslot 可复用，generation 不重定向；identity 耗尽明确 NO_RESOURCE 而不回绕。

## 各类传输与 bufferlease

| 接口 | 当前合同 |
|---|---|
| GPIO | typedread/write/toggle；真实 provideraction，unsupported 路径不伪造成功 |
| UART | submit 借 TXstorage 到 poll 的 settled 或其 cancel 契约成功；oneoutstandingticket；RXevent 含 provider 实际支持数据 |
| SPI | controller/child/transaction；阻塞 transfer 共享 bus/操作 budget 且返回前 settle；asyncsubmit 有限 budget，caller drives service |
| I2C | Native modernprovider 支持 unshifted7-bitaddress、parent/child、queue/deadline/callback；MCUmodernprovider 缺失时 unsupported |
| Flash | wholephysicalcontroller→权限/offset/size 受检 region；完整 block 擦除；parent/region/borrow 独立寿命 |

UARTmemorycomplete 与 wireidle/TC 不同。flush/RS485DE 必须依靠实际 wirecompletion；GPIODE/RE 和外部收发器不由通用 UART 硬编码。SPICS/DMA 取消要求 hardwaredrain，不从 memorycallback 推断 wire 停稳。Native/Vendorfake 无法证明真实波形和中断时序。

SPI/I2C`cancel()`只是 queued/current 请求取消：NX_OK 不代表 settled；error 不归还 lease，terminalcallback 返回或 pollsettled 才证明可复用。blockingtransfer 的 cancel_transfer 调用者仍要等原 transfer 返回。callback 不持 metadata 锁，在 providerdrain/unlock 后交付，samechild 重入/close 活跃时 BUSY。

provider 若 UARTsubmit 返回 NX_OK 却没有 ticket，core 进入 recovery-requiredunknownlease，普通 poll/cancel 无法伪造 settlement。`nx_device_uart_recover()`只有 deinit 证明 UNINITIALIZED 后才返回 NX_OK、invalidate 旧 ref 和归还 buffer；failure 保持 callerborrowstorage，需明确 retry/recovery。

## Deadline 与等待端口

真实公共头`hal/runtime/nx_deadline.h`提供`nx_hal_wait_port_t`、deadline_start/remaining 及`nx_device_uart_transfer()`。caller 给真实 wrappingmillisecondclock 和有界 wait；interval 不为 0 且不超过 remaining。OSALbinding 提供同合同，不拥有私有 task 或 heap。

budget 在 admission 前建立，submit/poll/taskwait 共享一次 budget；zero/expired 不开始新 hardware。任何 timeout/clock/wait/pollfailure 尝试 cancel，result.settled 才是 successfulsubmit 后的归还证明。cancel 失败返回其 error 与有效 ticket 供 laterpoll，storage 仍借出；zero-ticket 走 recover。

32-bitclock 约束在 API 支持的 finitewrap 范围。Flashpulse 或 provider 结清有安全返回上界，C 函数不能硬抢占失控 callback。worstcasecleanup 成本由外部 budget 与真实硬件 measurement 决定；不以轮询次数、sleepunit 或 model 时长冒充 deadline。

## Caller-owned terminal completion

真实公开头`hal/runtime/nx_completion.h`提供 init/deinit/arm/post/dispatch：

1. 外部 owner 提供 zero-initializedqueue、slotarray 和 entryarray，init 时容量非零且 queue≤slots；lifecycle 由 caller 串行。
2. 在将 ticket 交 producer 之前 arm，callback/context 借出并保留有限 slot；资源或 sequence 耗尽明确失败。
3. producer 只 post 真实 settledterminal。unsettledcancel/timeout/abortfailure 不能 post 为完成；模块不 poll/cancel/drain 或认证设备 settlement。
4. FIFO 满时 FULL 保留 armedticket/context，producer 显式 latch/retry；重复、stale、wrongqueue 拒绝，一次成功 post 只能派发一次。
5. 单 taskowner 显式 dispatch 有限条数，callback 在 metadata 锁外/consumercontext 运行，callback 返回后才还 slot。callback 时长由 caller 限制，重入/concurrentdispatch 拒绝 BUSY。

post 在 configurableCortexIRQ/task 中只做固定 O(1)metadata 动作，无 wait/alloc/usercallback；NMI/HardFault/SMP 以及 NativePOSIXsignalproducer 不支持。taskarm/dispatch/lifecycle 拒绝 ISR/已有 mask。deinit 遇 armed/queued/dispatchcallbackBUSY 并保留 storage；owner 需先 quiesceproducer，deinit 本身不关闭硬件 IRQ 或线程。

模块不偷偷接线现有 providers、不创建默认 worker。它是 caller 可组合的 notificationadapter，设备本身仍是 bufferlease/真实终态权威。Native 并发、FULLretry/ringwrap、callback 内部新 ticket、sequence 耗尽和 minimal-onlylink 已有真实软件检查；Cortexarchive 编译不是 ISRlatency/实际 ELF 或 HIL。

## Arch 与 FreeRTOS 同步机制

`nx_arch_irq_save/restore()`在 Cortex-M4 保存恢复 PRIMASK，Native 保存线程 nesting 并用真实锁。token 在同 CPU/thread 严格逆序恢复；不能替代 FreeRTOSBASEPRI/syscalltoken。短 metadata 区不 wait、OSAL、alloc、vendorcallback 或 log；它不是 SMP/NMIlock。

`nx_arch_in_isr()`在 Cortex 读 IPSR，Nativefalse；`nx_arch_irq_is_masked()`保守检查 PRIMASK/BASEPRI/FAULTMASK。屏障提供真实 CPU 顺序 primitive，不宣称 DMAcache 或 MPU 保护。FreeRTOSlogicalpriority0–4 的 highurgencyIRQ 不能调用 profile 的 FromISR（当前 syscall 下限 5）；优先级与 route 由 Board/port 规则和实测一起约束。

## OSAL 能力与生命周期

`osal_get_backend_info()`和`osal_get_execution_info()`是操作能力/上下文的权威。Nativethread 模型、baremetalloop 和 FreeRTOSscheduler 不是可任意互换的 RTOS。baremetal 没有 stackfulscheduler，optional 对象能力需按 effectiveconfig/backendinfo 检查，不创建不运行的 task/timer 假实现。

FreeRTOS 六类 object 使用静态控制块及明确 payload/stack/pool 预算；idle/daemonstack 单列。slot 重复 create/delete 可 reuse，lifetimetoken 累计不重复，耗尽明确失败。有限预算 variant 真实编译入 Native 统计 off、baremetal 和 pinnedPOSIXkernel，验证累计耗尽/reservedslot 诊断；不等于长期 hardwareload。

启动期可按能力创建对象，但 scheduler 未运行时拒绝 blocking 及 FromISR。adapter 保存构造前 incomingportmask，避免 kernelcriticalsentinel 遗留 BASEPRI。自然结束 task 保留 finished 身份，由其他 task 同步删除；持 mutex 的 task 不得把 TCB 回收给新 task 继承旧锁。

删除/关闭前 caller 停止新使用并结清 waiter/callback；BUSY 保留 object，Native 特定 wake/cancel 能力不扩大成所有后端强删。OSALheap seal 只控制 OSALallocator 入口，不代表 vendor/libc 全程序 zeroheap。MCUrunningkernel 不支持 runtime 整体 shutdown/restart。

## 验证条件

每次公共 contract 变更一起更新 caller、provider、test 与文档，覆盖 owner/generation、busy/failretry、zero/allocationexhaustion、deadline/clockwrap、cancelrace 和 finalbufferownership。普通最小 facade 实际链接无 OSAL、publicheader 无 vendor、真实 startup/object/strongIRQ/registration 保留由 linkerconsumer 验证。

Hostassertion/model、actualpinnedFreeRTOSPOSIXexecution、ARMprovidercompile、ARMfirmwareELF、physicalHIL 分别记录。历史结果不能继承到新 source。当前物理未执行；MCUtypedI2C、DMA/TC/electrical、powercut、worstload/jitter 和完整内存/cache/MPU 都保留独立实施/资格边界。
