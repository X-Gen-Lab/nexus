# 显式初始化注册与固件元数据

本组件提供 `nx_init.h` 的初始化注册表和 `nx_firmware_info.h` 的外部固件元数据。它是可选组件，不接管 `main()`、HAL/OSAL 生命周期、RTOS worker 或 scheduler。

通用基础设施由 `runtime/nx_runtime.h` 的 `nx_runtime_bootstrap()` 初始化。外部固件显式决定何时执行 `nx_init_run()`，检查返回值，再进入业务。FreeRTOS 下需要阻塞同步的组件必须在已调度任务中初始化；任务创建和 `osal_start()` 的返回值由外部调用者检查。

```c
#include "runtime/nx_runtime.h"
#include "nx_init.h"

int main(void) {
    nx_boot_report_t report;
    if (nx_runtime_bootstrap(&report) != NX_OK) return 1;
    if (nx_init_run() != NX_OK) return 1;
    /* Baremetal/Native example: the caller now owns its loop and cleanup. */
    return 0;
}
```

`nx_init_run()` 按链接段级别执行所有注册项，记录每项结果；某项失败仍执行剩余注册项，最终返回失败。此机制没有自动组件回滚，外部固件必须选择和验证适用的失败恢复策略。一次执行后再次调用不会重新执行注册项。

原 `nx_startup()`、weak board/OS hook、自动 main 包装与测试状态 setter 已删除。厂商 `Reset_Handler`、真实向量、C runtime 数据初始化和链接器启动仍由所选平台提供。

初始化回调中不能重复初始化 HAL/OSAL，也不能隐式调用业务 `main()`。业务版本字段由外部应用目标定义，查询元数据不能作为签名验证、健康确认或升级许可。

文档见 [使用说明](docs/USER_GUIDE.md)、[设计](docs/DESIGN.md)、[移植](docs/PORTING_GUIDE.md) 和 [验证](docs/TEST_GUIDE.md)。实际软件执行与实板证据分开记录。
