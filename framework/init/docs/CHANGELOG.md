# Init 变更记录

## 通用平台边界重构

移除 `nx_startup()`、自动 main 编译器入口、weak board/OS hook 及仅为修改内部状态提供的测试 setter。它们没有生产调用者，不能保留未经检查的任务创建与递归 main 范式。

保留显式 `nx_init_run()` 的真实链接注册、失败统计和幂等语义，以及外部固件元数据。测试专用段校验接口宏改名为 `NX_INIT_TEST_MODE`。

基础设施启动迁到独立 Runtime：查询平台身份不启动硬件，bootstrap/shutdown 保留单 owner、失败回滚、PARTIAL 与 BUSY 恢复。调用者负责应用 worker、scheduler、组件顺序和恢复策略。
