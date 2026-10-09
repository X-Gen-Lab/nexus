# 独立边界审查与回归

本次审查聚焦配置导入政策、可复用句柄、通知回调及存储重新打开的可观察失败行为，对应 HAL-001、OS-002、CFG-001/002、SEC-001。审查后补充行为回归并修复实现；这里的结果限于这些边界，不代表整个项目通过安全审计、线程安全证明或硬件验收。

## 已关闭的缺陷

| 触发 | 修复后的行为 | 实现与回归 |
|---|---|---|
| 对已有 readonly/encrypted key 使用带 CLEAR 的导入 | 暂存阶段始终检查原始 key 的 namespace/key/政策身份；readonly 拒绝替换，encrypted 不得被明文降级，persistent 政策保留 | `framework/config/src/config_import.c`；`tests/config/test_config_crypto.cpp` |
| 同时启用 CLEAR 与 SKIP_ERRORS，并输入畸形 JSON、数字或 framing | 词法和整体格式错误中止整个事务，保留原 store；SKIP_ERRORS 只适用于完整解析后允许跳过的语义错误 | `config_import.c`；`SkipErrorsCannotTurnMalformedJsonIntoClearAuthorization` |
| 关闭 namespace 后重新使用同一固定槽，或 manager 反初始化后重新创建 | handle 以不解引用的 lifetime token 查找；旧 handle 和伪造 token 明确失败，不能操作新 namespace；计数跨反初始化保留，耗尽显式失败 | `framework/config/src/config_namespace.c`；`tests/config/test_config_namespace.cpp` |
| 注销 listener 后复用 callback 槽，或通知中注册新 listener | callback 使用独立 lifetime token，旧 token 不能注销新 owner；本次通知捕获 token 快照，新 listener 只参与后续通知 | `framework/config/src/config_callback.c`；`tests/config/test_config_callback.cpp` |
| callback 执行中自注销或反初始化 manager，试图释放正在使用的 context | 执行引用与通知深度保留 context 所有权，自注销或反初始化返回 BUSY；只读查询可以重入，移除其他空闲 listener 不会把旧通知转给新 listener | `config_callback.c`；`NotificationPinsExecutingContextAndDefersNewListeners` |
| `nx_storage_open(&store, &store.flash, ...)` 重新打开，port 与待清空对象别名 | 在重置对象前复制完整 Flash port，使用副本计算几何，消除清零后 program_size 为零导致的除零；正常 reopen 和 I/O 错误后的 reopen 保留有效旧值 | `services/storage/src/storage.c`；`tests/storage_security/test_storage.c` 中 `aliased_port_reopen()` |
| FreeRTOS timer daemon 确认删除后，最后 pin 释放与 slot reclaim 分属临界区 | 最后引用递减、资源计数递减和 `used=false` 在同一临界区完成；并发 retry-delete 不能取得已关闭 lifetime，旧 token 不影响新 slot | `osal/adapters/freertos/osal_freertos_timer.inc`；真实 kernel 的 30 轮并发删除/重试/新槽/旧句柄与 heap 恢复回归 |
| FreeRTOS POSIX port 的线程在 event 等待中取消，未释放 pthread mutex | 本地 host-only event helper 用 cancellation cleanup 释放已重新取得的 mutex；timed wait 采用规范化 monotonic deadline，timeout/error 不假报成功 | `tests/osal/freertos_runtime/wait_for_event.c`；`test_posix_event.c` |
| Native GTest 手工注册掩盖生产平台没有 descriptor 注册 | 生产初始化遍历有效配置中的 descriptor 并注册，有限示例从真实 factory 获得设备；没有平台实现的 weak startup 返回 NOT_SUPPORTED | `platforms/native/src/platform/nx_platform_init.c`；`hal/src/nx_hal.c`；实际生产入口 smoke |

Config 仍采用单一管理 owner，调用者串行执行修改、导入、持久化和生命周期操作。token 与执行引用消除本次发现的槽复用和回调释放问题，没有把全局 store 改成任意线程同时读写安全。管理权限与产品 schema 仍由产品授权层负责；明确删除并重建 key 是独立管理操作，不能让 CLEAR 隐式取得降级权限。

## 已执行的定向证据

独立构建的 ConfigCrypto、ConfigNamespace、ConfigCallback 子集执行 119 项，failures/errors/disabled 均为 0。该次使用 C11 严格警告编译，并引入 11 项针对上述缺陷的回归。执行报告位于临时工作区 `/tmp/nexus-independent-review-build/result.json`；它是该次检查的记录，不是最终提交的全量验证包。

修复 storage port 别名后，`/tmp/nexus-storage-reopen-test` 实际执行成功，覆盖原有 630 个 erase/program/sync 故障边界、跨进程读取，以及新增加的正常和失败后的 in-place reopen。该 C11 构建使用 `-Wall -Wextra -Werror`。这些是 Native 文件 Flash 证据，不能代替器件掉电、供电与擦写时序测试。

真实 pinned FreeRTOS kernel 与 POSIX port 运行 10 组契约，包含新增 30 轮并发 timer delete/retry、重新分配 slot、旧 handle 拒绝和 kernel heap 恢复；进程自然退出为 0。host-only event helper 的 5 组独立契约也实际通过，其中 timed/untimed cancellation 各 100 次，共 200 次取消后复用。该 helper 是保留上游许可的 [本地 host-test adaptation](freertos-posix-host-port.md)，pinned kernel 和 POSIX port 本体没有修改，不能描述为完全未经修补的上游 port。这些检查关闭了软件删除竞争与宿主退出缺陷，不覆盖 Cortex-M 中断端口。

生产 Native descriptor 注册和无平台 weak startup 的失败行为已完成定向 smoke。STM32 的公共平台初始化桥实际调用 `stm32_platform_init()`；全局 in-process shutdown 明确不支持，保留 IRQ/DMA/设备/scheduler 所有权到受控 reset，不能以调用 `HAL_DeInit()` 报告虚假的完整回收。

整合后的 Linux GCC Debug CMake/CTest 已实际执行 1696 项，failure/error/skip 均为 0，耗时 49.93 秒，报告为 `build/linux-gcc-debug/ctest-results.xml`，日志为 `/tmp/nexus-native-final-all.log`。其中包含 blinky、industrial controller、config demo、shell demo 四个有限生产入口 smoke，以及生产注册、初始化、日志、真实 FreeRTOS kernel 和 POSIX helper 的回归。

随后新增的第五个 `freertos_demo --run-ms 500` smoke 单独执行 1/1 通过，进程自然退出，耗时约 0.54 秒，报告为 `build/evidence/native-freertos-demo-final.xml`。ASan/UBSan 子集执行 23/23，通过且 failure/error/skip 均为 0，耗时 11.56 秒，报告为 `build/linux-gcc-sanitizers/ctest-results.xml`，日志为 `/tmp/nexus-sanitizer-final-contracts.log`。该子集包含 18 个选定契约与 5 个有限应用 smoke，不能代替全部 Google Test 用例的 sanitizer 运行。

本地运行使用 `ASAN_OPTIONS=detect_leaks=0`，原因是 ptrace 环境阻止 LeakSanitizer；不能把 ASan/UBSan 通过扩展为泄漏检查通过。CI 保留独立 leak detection 设置，在线执行结果另行验收。

这三份报告分别是 1696 项完整检查点、新增 1 项 smoke、23 项 sanitizer，没有合并成一份已执行 1697 项的全量 XML。最终 source/config/ELF 绑定将在所有源码提交完成后重新构建并统一归档；当前不写尚未形成的最终 SHA，也不扩展为 ARM 或物理后端通过。

## 剩余验收边界

真实 ARM ISR/DMA、产品 key vault/熵源、快照整体防回放、bootloader 与安全计数器、物理 HIL 以及正式发布签名属于独立验收边界。本次没有为它们生成通过声明。
