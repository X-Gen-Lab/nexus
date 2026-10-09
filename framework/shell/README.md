# Shell：通用命令核心与显式 UART adapter

`Nexus::Shell` 负责命令注册、解析、行编辑、历史、补全和 backend 调用，不依赖 HAL/OSAL 或厂商 SDK。`Nexus::ShellUART` 显式连接 typed HALDevice 和 OSAL。假 backend 仅位于 `tests/shell/fixtures`，不进入生产 Shell archive 或 public core 头。应用命令、鉴权、产品权限和恢复策略由外部应用提供。

## 通用装配

```c
#include "shell/shell.h"
#include "shell/shell_backend.h"
shell_config_t config = SHELL_CONFIG_DEFAULT;
/* backend 的 read/write 与所有 storage 均由应用持有。 */
shell_status_t status = shell_init(&config);
```

用 `shell_set_backend()` 绑定输入输出；应用在自己的任务/主循环调用 `shell_process()`。命令描述符传给 `shell_register_command()` 并在注册期间保持有效。默认 parser/editor/session 是一个单例，应用序列化 core 调用；没有隐藏 shell worker 或多会话承诺。Shell core 初始化目前使用有界配置大小的动态存储，不宣称全静态 Shell。

read 返回 0 表示当前无输入，负值表示 transport error。`shell_process()` 将负值变为 `SHELL_ERROR_BACKEND`，保留诊断并丢弃 partial line/escape/history browse，防止数据丢失前后的字节拼成一条命令。恢复后应用可显式调用 `shell_recover()`。

## 类型化 UART

```c
#include "shell/shell.h"
#include "shell/shell_uart.h"

shell_status_t open_console(const char* board_uart_name) {
    shell_status_t status = shell_uart_backend_init(board_uart_name);
    if (status != SHELL_OK) return status;
    status = shell_set_backend(&shell_uart_backend);
    if (status != SHELL_OK) return status;
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    return shell_init(&config);
}
```

芯片与板卡只给出 Nexus device name；adapter 调用 typed open/query/submit/poll/cancel/close，不取得旧 `nx_uart_t*`。必须提供 typed TX 与 RX-events capability，无 provider 时显式 UNSUPPORTED。当前 Native UART 不具备完整 typed operation capability，因此不伪装为正常 provider。

UART RX 非阻塞，错误不等价于 no-data，`shell_uart_backend_last_status()` 保存 HAL 状态。write 在任务上下文同步等候，使用固定 256-byte TX copy、总 1000 ms budget，超过一段的消息按块发送，返回已完成的字节数。每段都等待 memory settlement 和 wire idle；不持有 logger mutex。此 console adapter 适合管理流，不承诺控制周期实时输出。

## 失败与清理

UART adapter 使用短 admission gate 串行化 I/O/init/deinit。BUSY/timeout/cancel/close/recover 失败保留 device owner、初始化状态以及永久 TX buffer；下次 deinit 可重试。存在 TX lease 时禁止 overwrite，再次 init 返回 ALREADY_INIT。未知 zero ticket provider 违约经过 HAL recover，只有硬件 settlement 被证实时才能释放引用。

先停管理 I/O 调用、解绑 `shell_set_backend(NULL)`、结束 Shell session，再执行 `shell_uart_backend_deinit()`；成功前不得将 teardown 视为完成。必须检查状态并由外部应用决定有限重试、人工恢复或设备复位，平台不内置产品安全政策。

## 构建与验证

只需 parser/editor 等功能时链接 `Nexus::Shell`。UART 集成另链接 `Nexus::ShellUART` 并通过 Firmware 装配选定 provider。测试 fake 在 `shell_test_support` 中，其 include root 不传播到生产目标。

真实 typed facade + Native OSAL 的故障 port 验证 BUSY close 重试、取消失败持有 TX copy、零 ticket recover、RX error、unsupported provider 和多段 write；它们不表示 MCU electrical/ISR/HIL 通过。详见 [组件实施记录](../../docs/implementation/components-refactor.md)。
