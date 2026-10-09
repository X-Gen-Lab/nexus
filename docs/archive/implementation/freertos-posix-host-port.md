# FreeRTOS POSIX host event 修补与验证

Nexus 的 FreeRTOS OSAL 合同测试使用子模块固定的 FreeRTOS kernel 与 POSIX `port.c`，依赖 commit 为 `dbf70559b27d39c1fdb68dfb9a32140b6a6777a0`。Host event helper 使用 [本地 MIT 适配](../../tests/osal/freertos_runtime/wait_for_event.c)，源头为该 commit 的 `portable/ThirdParty/GCC/Posix/utils/wait_for_event.c`；文件保留原版权、许可证和改动说明。没有修改子模块，测试不能描述成完全未经修补的上游 POSIX port。

## 修补原因与边界

上游 `event_wait()` 调用 `pthread_cond_wait()` 时没有注册取消清理。POSIX 取消 condition waiter 会先重新取得 mutex；等待线程退出后，mutex 仍被持有。独立复现中，`pthread_cancel()` 与 `pthread_join()` 完成，随后 `event_signal()` 永久等待。FreeRTOS POSIX `vPortCancelThread()` 同样使用 cancel、signal、join，因此这种竞态可以阻塞任务回收。

本地 helper 在普通与限时等待中注册 mutex 解锁 cleanup，并在正常返回时执行同一 cleanup。Condition 使用 `CLOCK_MONOTONIC`，限时等待规范化 `tv_nsec` 并检查 deadline 的秒数溢出；timeout 和 pthread 错误返回 `false`。创建失败释放已取得的资源，事件仍是单个 coalescing signal。调用方必须先结束并 join 所有 waiter，再删除事件；修补没有增加“删除正在等待的事件”的能力。

这项修补限于 Linux 主机 fixture。它不验证 Cortex-M 上下文切换、NVIC 优先级、电气时序、DMA 或真实 FreeRTOS 实时预算，也不改变 MCU 后端。

## 执行证据

2026-10-08，在本地 GCC 14.2 / Linux pthread 环境执行 [独立回归](../../tests/osal/freertos_runtime/test_posix_event.c)：

- 普通 waiter 取消、join 后重新 signal / consume：100 次。
- 限时 waiter 取消、join 后重新 signal / consume：100 次。
- Signal coalescing、零超时、负超时及空参数行为。
- 强制跨秒的 900 ms monotonic deadline、超时后的 mutex 复用。
- 限时 waiter 的 signal 唤醒与单次消费。

严格 C11 `-Wall -Wextra -Werror` 的 `-O0`、`-O2` 两种构建均完成 5 组检查并自然返回 0；每个进程有 10 秒外部截止。ASan + UBSan 同样通过，使用 `ASAN_OPTIONS=detect_leaks=0`。本地默认 LeakSanitizer 报告 sandbox 的 ptrace 限制，未获得 leak 检查通过证据。

未修补 helper 的孤立取消复现在 3 秒截止时仍阻塞，只作为缺陷复现，诊断终止没有计为通过。修补前已重建的真实 kernel 合同进程连续 130 次自然退出；这个结果不能消除已经独立证明的取消竞态。修补后的 kernel 合同和 CTest 汇总以本次最终集成记录为准。

直接运行独立 fixture：

```sh
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pthread \
  -Iext/freertos/portable/ThirdParty/GCC/Posix/utils \
  tests/osal/freertos_runtime/test_posix_event.c \
  tests/osal/freertos_runtime/wait_for_event.c \
  -O2 -o /tmp/nexus_posix_event_contracts
timeout 10s /tmp/nexus_posix_event_contracts
```
