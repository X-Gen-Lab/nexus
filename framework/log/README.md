# Log：格式核心、OSAL 运行时和 UART 适配器

Log 由三个显式目标组成：`Nexus::LogCore` 是无状态格式化器，只有 C 标准库依赖；`Nexus::LogRuntime`（`Nexus::Log`）负责过滤、注册、同步与有界异步派发，依赖选定 OSAL；`Nexus::LogUART` 使用 typed HALDevice，拥有 caller-owned 固定队列和 UART 引用。通用头 `log/log_backend.h` 不包含 HAL。平台不选择产品 UART、任务、丢失政策或故障复位策略。

## 最小核心

```c
#include "log/log_format.h"
char output[128];
log_format_options_t options = {.pattern = "[%T] [%L] %M: %m"};
log_record_t record = {.level = LOG_LEVEL_INFO, .module = "sample",
                       .message = "ready", .timestamp_ms = 120};
size_t length = log_format_record(output, sizeof(output), &options, &record);
```

格式化器不访问时钟、不启动任务、不分配内存。所有输入和输出由调用者拥有。`%t` 使用显式 `clock_text`；未提供时输出 `--:--:--`。OSAL runtime 的 `%T` 是毫秒时间，`%t` 在 Native 使用宿主时间、MCU 使用 uptime 的 HH:MM:SS，并不表示设备已同步日历时间。

## UART 装配

```c
#include "log/log.h"
#include "log/log_uart.h"
static log_uart_backend_t sink; /* 始终保留到成功 shutdown。 */

log_status_t start_log(const char* board_uart_name) {
    log_status_t status = log_backend_uart_init(&sink, board_uart_name, 100, 1000);
    if (status != LOG_OK) return status; /* opened=true 时仍需显式清理。 */
    status = log_init(NULL);
    if (status != LOG_OK) return status;
    return log_backend_register(&sink.backend);
}
```

应用管理错误路径并在单独管理上下文安排 `log_backend_uart_service(&sink)`。write 仅将消息复制到固定队列：默认 4 条，每条最多 `LOG_MAX_MSG_LEN * 2` 字节。队列满/过长返回 `LOG_ERROR_FULL` 并增加 dropped；没有偷偷同步等待 UART 的备用路径。`service` 非阻塞地 submit/poll；`flush` 在显式 budget 内派发已接受数据，等待 `settled && wire_idle`。任务调度、service 周期、队列容量和 line rate 应由外部应用核算。

## 生命周期与并发

注册的 backend、ctx、name 必须一直有效。回调在 logger metadata mutex 外执行；每个 sink 同时只能被一个回调使用。并发同步调用该 sink 可返回 BUSY，调用者必须检查结果。需要无损并行派发时选择有界异步队列和 BLOCK policy，不能把默认同步 API 的返回值丢弃。

回调持有 lifecycle pin，所以 unregister 在回调活跃时返回 BUSY；shutdown 超时保留 task/queue/backend。同一回调递归 write、flush、shutdown 返回 BUSY。metadata 和其他 async producer 不等待慢 sink 的 UART 线速。独立调用 adapter 与 logger 注册生命周期时也必须遵守应用管理的 admission/停止顺序。

先停止独立 service/producer，再 unregister，最后 shutdown。普通 unregister/deinit 要求 drain 成功，失败保留注册/引用/队列。`log_backend_uart_shutdown(&sink, true)` 只能在 unregister 后显式取消和丢弃；取消、close 或未知 ticket recover 失败保留被借的固定存储。不得 memset、释放或重新 init 来绕过 lease。

`log_uart_stats_t` 提供 accepted、completed、dropped、failed、pending、last_status 和 buffer_leased。一次已接受消息的终态错误会令 flush 持续报告 BACKEND，直到显式 discard/shutdown/reinit；队列为空不伪造成功。多个普通 backend 的 logger 返回值沿用“至少一个 sink 成功则 OK”，每个 UART sink 的失配/丢失必须看自己的统计。

## 分配模式

UART adapter 在 `LOG_USE_STATIC_ALLOC=0/1` 均为 caller-owned，不调用 malloc/free。核心 formatter 不分配。runtime 的 console/memory backend 在 static 模式使用固定 pool；OSAL、宿主 stdio、外部 provider/libc 有自己的资源合同，这不是全程序禁止 malloc 的证明。

## 验证

新的 core-only formatter、UART dynamic/static、Shell typed teardown 与 logger 慢 sink 回归使用生产源码和实际 Native OSAL/typed facade；硬件 port 仅作为所有权故障 fixture。验证入口和真实范围见 [组件实施记录](../../docs/implementation/components-refactor.md)。STM32F407/GD32F470 裸机/FreeRTOS 的组件 archive 可交叉编译，不代表 UART 实板、IRQ、TC 或 DMA 时序合格。
