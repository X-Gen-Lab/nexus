# Runtime 与显式入口重构

本次工作实现 `RF-BOOT-01/02/03` 的通用软件边界，删除平台内部 Product 身份与自动 main 包装。实板启动与 FreeRTOS MCU scheduler 时序仍需独立验证，本文不作为产品或发布资格。

## 已实施

- `runtime/nx_runtime.h` 提供串行 bootstrap/shutdown、`OFFLINE/PARTIAL/READY` 状态及包含原始错误、rollback 错误和最终所有权的报告。
- 保留 HAL→OSAL 初始化、单 owner 拒绝、失败回滚、PARTIAL 禁止重启、device/OSAL BUSY 与 HAL 释放失败后恢复 OSAL 的保障。
- `runtime/nx_platform_info.h` 提供无副作用的 Arch/SoC/Board/backend 和主 SRAM/Flash、main stack/libc heap 预算及真实 Board/layout SHA-256。内存表仅枚举主 SRAM 与内部 Flash，不是完整内存域或产品 partition map。
- `Nexus::Runtime` 仅连接 HAL core/support 与 OSAL；`Nexus::Firmware` 显式保留选定平台对象和类型化 HAL facade。组件继续由外部固件选择。
- 旧 `products/` 与 `tests/products/` 移除，契约测试迁到 `tests/platform/runtime/`；生产 Native typed SPI caller 改用 Runtime。
- 删除没有生产调用者的 `nx_startup()`、weak board/OS hook、编译器 main 包装和测试状态 setter，保留显式 Init registry/firmware info。测试专用 registry 边界校验宏改为 `NX_INIT_TEST_MODE`。
- Init 文档收敛到真实接口，删除未实际执行入口的合成 state/performance 支持宣称。外部 `main()`、worker 创建、scheduler、组件初始化与失败恢复由应用明确拥有。

Runtime 不代表应用健康，不负责设备业务恢复或组件停止。MCU FreeRTOS scheduler 运行期间 shutdown 明确 BUSY，不承诺 kernel 全局 shutdown/restart。

## 统一后的 HAL/MCU 生命周期

当前 HAL 具有 OFFLINE/PARTIAL/READY 与独立 cleanup status。失败 init 保留原错误并真实 cleanup，失败 cleanup 继续被 Runtime 持有为 PARTIAL。common HAL 独占 token admission fence；只读 typed/provider/IRQ/DMA preflight 失败保持 READY，进入实际 mutating cleanup 后的任一错误保持 PARTIAL/fence。新 open/construct/register/IRQ-DMA 准入拒绝，原 owner 可 close/recover/settle，重试真实cleanup成功才恢复OFFLINE。

Runtime 先 OSAL 后 HAL，仅 HAL 仍 READY 时尝试恢复 OSAL。idle baremetal/FreeRTOS调度前 MCU cleanup 实际结清 IRQ/DMA、vendor/timebase 与 clock binding，可再次 bootstrap；running/suspended MCU kernel 保持 BUSY。直接 SDK 用户先 quiesce 自己的资源；产品拥有 safe-output。ST peripheral bank reset 后重建窄 Board 初值不保证执行器电平连续。

当前新源码的生产/faultmodel/ARM检查由 external source-pair delivery reports 绑定，不以本文历史 53 个检查作为新 lifecycle 通过证明。

## 历史定向执行（统一前）

2026-10-09 在工作树、Native Debug 配置下使用 CMake 4.4.4 / GNU 14.2.0 构建并执行以下范围，尚未绑定最终干净提交：

| 范围 | 实际数量与结果 |
|---|---|
| Runtime 生产故障注入与 Native HAL/OSAL smoke | 2 个 CTest，均通过 |
| 真实 Native typed SPI ownership/deadline/cancel | 6 个，均通过 |
| Init/firmware info 单元、属性、显式集成与真实 linker registry | 45 个，均通过 |
| 合计 | 53 个，0 failure、0 skipped |

构建目录 `build/runtime-refactor-native`，真实 JUnit 为该目录的 `runtime-init-results.xml`。测试选择使用 `--no-tests=error`，JUnit 已实际解析为 53 个 testcase。最终源码、有效配置、依赖、工具链与全矩阵证据应由集成交付记录重新绑定。

生产故障注入验证 ISR、外部 owner、HAL 错误、OSAL 错误、HAL 回滚失败、部分状态与清理重试、device/OSAL BUSY、HAL 释放失败及 OSAL 恢复失败，并验证实际 HAL/OSAL 调用顺序。Native smoke 验证身份查询不初始化、真实 mutex 使 shutdown BUSY、结清后关闭与重新 bootstrap。

本次没有执行 ARM Runtime 镜像上板、FreeRTOS MCU scheduler 停止、IRQ/DMA、电气或 Flash 断电验证，也未把 Native clock/线程行为解释为 MCU 实时资格。
