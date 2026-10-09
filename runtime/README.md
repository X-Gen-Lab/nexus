# 通用运行时生命周期

Runtime 只负责所选 HAL/OSAL 的串行基础设施装配，不包含产品身份、业务 main、worker、组件顺序、升级政策或健康判定。外部固件链接 `Nexus::Firmware` 以保留所选平台对象/startup；只需要生命周期库时可显式链接 `Nexus::Runtime`。

```c
#include "runtime/nx_runtime.h"

int main(void) {
    nx_boot_report_t report;
    if (nx_runtime_bootstrap(&report) != NX_OK) {
        /* Caller retains the report and decides its own recovery policy. */
        return 1;
    }
    /* Caller now owns its devices, workers, components and execution loop. */
    return 0;
}
```

`nx_platform_get_info()` 返回当前有效配置的 Arch/SoC/Board/backend、物理主 SRAM/Flash 和 main stack/libc heap 预算；查询不启动硬件。Native 没有物理 MCU 内存。当前内存表仅枚举主 SRAM 与内部 Flash，不能作为全部内存域或产品分区声明。OSAL 能力/预算由 `osal_get_backend_info()` 提供，实时调度状态由 `osal_get_execution_info()` 查询；设备能力通过类型化 HAL 查询。

`nx_runtime_bootstrap()` 按 HAL→OSAL 获取所有权，不启动 scheduler。重复成功调用幂等，已有其他 owner 的初始化明确拒绝。OSAL 失败时尝试回滚 HAL，报告同时保留原始错误、rollback 错误与最终所有权。

状态为 `OFFLINE`、`PARTIAL` 或 `READY`。`READY` 只表示该 owner 已初始化基础设施；未结清的部分成功保留为 `PARTIAL`，不能重启 bootstrap，必须先重试 shutdown。所有生命周期调用由外部串行，ISR 拒绝，不提供隐式并发锁。

Shutdown 首先检查设备引用/操作，再释放 OSAL 和 HAL。BUSY 时保留状态；HAL 释放失败时尝试恢复 OSAL，恢复失败保留部分所有权，后续 shutdown 可重试。它不释放应用组件，也不终止其任务。MCU FreeRTOS scheduler 运行期间明确 BUSY，不支持全局 kernel shutdown/restart。

FreeRTOS 应用在启动期显式检查 worker 创建结果和 `osal_start()`，需要阻塞同步的组件在受调度任务中初始化。裸机应用自行拥有循环/pump，不以空 task/timer 模拟不支持能力。可选 Init 注册表由外部固件显式运行并检查结果；平台不存在自动调用 main 的入口。

生产 Runtime 的故障注入回归在 `tests/platform/runtime/test_boot.c`，真实 Native HAL/OSAL 生命周期在同目录 `test_native.c`。它们验证软件所有权与失败恢复，不替代 ARM scheduler/IRQ 或物理板卡资格。
