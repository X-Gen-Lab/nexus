# Init 验证范围

`tests/init` 保留真实 API、统计、边界、固件元数据与属性回归。`init_registry_tests` 通过真正注册的 board/driver/app 回调验证链接段排序、失败后继续、错误统计与重复调用不重跑。

旧 startup 状态 setter 测试与仅模拟弱 hook/空 main 的性能测试已移除。它们不能证明实际平台启动、任务创建失败门禁或 MCU 时序。

Runtime owner、HAL/OSAL 失败、回滚失败、BUSY 与恢复重试由 `tests/platform/runtime` 的独立故障注入和真实 Native smoke 负责。真实 typed SPI 生命周期另有生产路径测试。

在配置完整 checkout 后运行 CTest，拒绝零测试，记录实际枚举、配置、source 与工具链。编译或测试定义不等于已执行；没有实板时不得宣称启动/IRQ/DMA/电气资格。
