# ThreadX runtime 合同夹具

host Google 程序调用 `nx_threadx_kernel_enter`，在官方真实 Linux kernel thread
内跑用例；bootstrap task 由真实 adapter callback 在该线程启动前创建／discard。
`clock.c` 为真实 CLOCK_MONOTONIC，mock entry 只代替应用边界，task／queue／wait
是真实服务。kernel、GoogleTest／Mock archive 精确锁定。

M4／external-Tick 夹具编译生产 `m4.c`，只 mock CPU mask／寄存器或产品 Tick；
IRQ 夹具对真实准入 mock Arch／SCB／NVIC。Linux timer fixture 使用完整上游派生
timer source，仅模拟 libc clock／wait、scheduler／timer IRQ 边界；独立 kernel
程序仍执行真实调度／阻塞。它们证明软件顺序，不证明物理 priority。

每个翻译单元使用实际选定 target 的 ABI definitions，Linux 不能链接 M4 ASM／
伪 low-level 原型，MCU 不链接 host kernel。不导入上游 demo 的 clock／DWT／heap／
产品 callback。[后端合同](../../../os/threadx/README.md)说明 callback、优先级、
借用和不支持范围。

证据保留 discovery、命令／exit、原始日志、fresh XML、source／kernel／config／
tool、object／ELF。失败编译不是行为 RED；前实现快照说明修正 fixture 才得到真实
RED 的阶段。dirty 开发不资格化 clean candidate，M4 Board link 和实板工位另记。

CMake 链接实际选定平台、固定 kernel、真实 vector／startup、task／queue／notify／
close／join。三块维护 M4F Board 分别构建 BASEPRI／PRIMASK。缺 `external_tick.c`
的负链接必须因 strong provider symbol 失败；加入它仅得到返回 UNSUPPORTED 的
provider，不虚构周期时钟。固件只链接／测预算，不刷写。retention-only main 不是
产品启动／错误处理示范，产品须检查每次初始化并提供安全失败和清理策略。
