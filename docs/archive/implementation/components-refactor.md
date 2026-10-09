# 通用组件边界与 Log/Shell 重构

本批实现 `RF-FIX-04/05/06`、`RF-CMP-01`；日期为 2026-10-09（Asia/Shanghai）。这是提交前工作树的实际定向验证，最终集成 source/config/artifact identity 由维护分支证据包重新绑定。

## 已实施

- Log 的 `log_core` 仅负责 caller-owned 无状态格式化，没有 OSAL/HAL/时钟/分配依赖；`log_framework` 是显式 OSAL runtime；`log_uart_adapter` 才依赖 typed HALDevice。对应公开 targets 为 `Nexus::LogCore`、`Nexus::LogRuntime` / `Nexus::Log`、`Nexus::LogUART`。
- Shell core 不依赖 HAL/OSAL；UART 是 `Nexus::ShellUART`。假 backend 从生产源和通用头迁至 `tests/shell/fixtures`，只有 test support target 提供它。
- UART adapter 均使用 typed owner/generation reference，不调用旧 factory/pointer get_tx_sync。无 typed capability 明确失败。Shell close/cancel/recover 失败保留引用及 TX storage，下次 deinit 重试；旧副本在成功 close 后失效。
- logger 回调在全局 metadata mutex 外执行，保留 lifecycle pin 和有界 per-sink callback admission。慢 sink 不阻塞 metadata 和其他 async producer。注册变更在 callback 活跃时 BUSY；同一 callback 重入 write/flush/shutdown BUSY；shutdown 超时不释放注册、task、queue 或 ctx。
- LogUART 的 caller-owned 固定队列在 dynamic/static 两种 Log 模式均不 malloc/free。默认 4 条、每条 256 byte；FULL、accepted/completed/dropped/failed/pending 与最后 HAL 状态可观察。service 明确由应用调度；flush 等待 memory settlement 与 wire idle；取消失败保留借用队列 slot，零 ticket 违约只经 HAL verified recover。
- Shell 固定 256-byte TX copy，按消息总 1000ms budget 分段；取消失败后禁止覆盖该 buffer。transport read 负值经 core 返回 BACKEND 并丢弃 partial input，不当作 no-data。

这些组件不内置板卡 UART name、应用任务、安全输出、产品命令、复位政策、签名凭据或产品发布流程。Shell session 仍为单例、core 初始化使用动态存储，调用方序列化会话操作；没有声明多会话或全静态 Shell。

## 缺陷复现与回归

原生产路径的三个独立 harness 均触发预期 assertion（exit 134）：Shell BUSY deinit 返回成功；static Log UART creator 调用 malloc；slow sink 持有 logger mutex 令 metadata 写等待 1s 后失败。harness 位于 `build/components-refactor/baseline_*.c`；新的永久生产行为回归在 `tests/log/test_log_contracts.c`、`test_uart_adapter_contracts.c` 和 `tests/shell/test_uart_adapter_contracts.c`。

实际 Native CMake configure/build 成功，CTest `-L '^(log|shell)$' --no-tests=error` 执行 **444 项，0 failure、0 skip**。新检查包括无 runtime 的 formatter 可独立链接；UART static/dynamic allocator wrap；队列背压和 copy ownership；memory complete 与 wire idle 分离；flush 超时和已接受消息错误；close/cancel 失败和 retry；未知 ticket recover；unsupported provider、RX fault、多段 Shell write。已有并发无损数据检查改为显式 bounded async BLOCK queue，每个 producer 验证返回值，并在 FIFO flush barrier 后读取。

生产 registry/UART facade 和实际 Native OSAL 参与测试；port fixture 只模拟硬件所有权与错误，不声称 Native UART provider 已支持 typed TX。分配拦截覆盖自有链接代码的 malloc；宿主 libc/stdio、外部 provider 和全程序分配仍按各自合同管理。

ARM GNU 14.3.rel1 实际编译 `log_core`、`log_framework`、`log_uart_adapter`、`shell_framework`、`shell_uart_adapter`，四组均退出 0：启明 F407ZG 裸机/FreeRTOS、GD32F470 梁山派裸机/FreeRTOS。测试片段在各自现有配置中开启 Log/Shell，使用真实 SDK/OSAL/Arch headers。**这些是组件 archive 构建，没有将本批 adapter 应用链接为最终镜像或上板执行。**

本地证据：`build/components-refactor/native/components-results.xml`、`component-validation.json`（组件源码 hash 和完整用例枚举）、`ctest-components.log`、四组 `*-configure.log` / `*-build.log`。最终平台矩阵须重新执行，不能把上述提交前结果当作之后任意 source 的通过证据。

## 公开使用与后续边界

当前 [Log 使用合同](../../framework/log/README.md) 和 [Shell 使用合同](../../framework/shell/README.md) 已替换旧 pointer UART 及 production fake 的复制式教程。应用需检查 BUSY/FULL/error；需要无损并发 dispatch 时显式选择有界队列，不从同步 callback admission 推断所有消息都已发送。

`RF-CMP-02/03` 的本批审查确认 Storage 是 offset/bank/Flash 窄端口，协议通过窄回调，Update 是外部 trust/digest/storage/counter 的通用状态机。实际缺口包括 Config fake 仍需迁出、Update CMake 不必要地传播 Storage/Security、Native Security default provider 的隐式选取；这些由后续提交实施，**本记录不宣称该两项已全部交付**。Update 的已验证持久化恢复不变量必须保留，不用本批重构无声改变现场格式或产品回滚政策。
