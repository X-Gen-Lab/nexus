# 使用显式 Init 组件

首先在应用目标显式链接 `Nexus::Init`，然后包含 `nx_init.h`。注册函数返回 0 表示成功，其他返回值表示失败：

```c
static int initialize_component(void) {
    /* Initialize only this externally configured component. */
    return 0;
}
NX_INIT_COMPONENT_EXPORT(initialize_component);
```

外部固件先检查 `nx_runtime_bootstrap()`，然后在适当执行上下文调用并检查 `nx_init_run()`。平台不会自动执行该调用，也不会创建应用任务。

FreeRTOS 先在启动上下文创建由应用拥有的 bootstrap worker，检查创建结果，再显式启动 scheduler。需要 operational mutex/queue 的组件初始化在已调度 worker 中执行。裸机自行拥有循环或 pump；不支持的 task/timer 必须失败。

注册表失败后继续执行其余条目，最后返回 `NX_ERR_GENERIC`。调用 `nx_init_get_stats()` 可取得总数、成功数、失败数和最后原始错误。它没有组件 rollback callback；恢复与退出策略由外部 owner 提供。重复调用不会重新执行已尝试的注册表。

`nx_firmware_info.h` 只读取外部定义的版本/构建字段，不代表可信镜像、产品身份或健康检查。

此前自动 main 入口与 weak board/OS hook 已移除，不能沿用递归调用 main 的启动范式。
