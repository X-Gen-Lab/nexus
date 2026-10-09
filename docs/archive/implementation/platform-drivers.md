# 平台与驱动实施记录

本文保留首次HAL-003/004/005与Discovery SPI重构的契约和执行记录；43项Native SPI与最初3个host CTest target是历史检查点，不能冒充当前完整矩阵。当前实现覆盖F407VG Discovery、F407ZG启明V3.1、F407VE天空星青春版和GD32F470ZG梁山派，各有裸机/FreeRTOS profile。旧GD32F407占位目标仍不受支持，不与实际GD32F470实现混同。当前依赖、类型化SPI façade、UART和源码SDK边界见 [平台交付](platform-delivery.md)、[STM32 runtime](stm32-runtime.md)、[Typed device core](typed-device-core.md) 和 [GD32F470](../../platforms/gd32f470/README.md)。没有物理板卡、PCB revision、探针或电气实测报告，以下软件契约、host fakes与ARM构建均不构成工业资格或HIL通过。

## 已实现的 SPI 契约

`nx_spi_bus_t` 增加可选的 `open_device`、`close_device`、`service`；`nx_spi_device_t` 提供 `transfer`、`submit`、`cancel`。STM32 与 Native 已实现全部这些显式方法。Native 只模拟串行总线、字节捕获和未注入 RX 的回显，不能代替真实 STM32 驱动或硬件证据。

显式 `open_device` 将值写入调用者拥有的 `nx_spi_device_t`，包含 owner 和 uint64 generation token；可复制该值。close 成功和总线 deinit 使所有旧副本失效，设备槽复用后旧副本的 transfer/submit/cancel/close 明确失败，不影响新 owner。generation 跨 deinit/reinit 保留，到 UINT64_MAX 后拒绝新分配。旧 getter 返回无 release 的不可变配置/回调/context 槽；相同键复用，不同键消耗有界容量，耗尽返回 NULL，生产 deinit 不将旧槽重新分配给其他键。

STM32 总线只持有仲裁、硬件与正在执行的事务；每个固定槽设备复制自己的模式、最大速率、位序和板级逻辑 CS ID。获取第二个 slave 不会覆盖第一个的配置和回调上下文。池容量为 8，每个设备最多一个排队事务；没有收发路径动态分配。STM32F407 实际编号为 SPI1、SPI2、SPI3，SPI0 无法构建。

同步 `transfer` 只在任务上下文执行。一个从调用起点计算的预算覆盖等待总线锁、配置、CS、传输和 DMA 等待；异步预算从 `submit` 起点计算，包含排队时间。毫秒计数支持 32 位回绕；有限预算不超过 INT32_MAX。同步 UINT32_MAX 表示无限等待；异步必须给有限非零预算以及终态回调。超时时没有启动硬件就不发送；硬件已启动后必须先停止并回收所有权。安全清理可能超过业务 deadline，真实板上还需测量这个独立清理上界。

`submit` 复制事务描述而不复制显式 API 的 payload；调用者必须保持 buffers 有效直到终态回调。一个任务周期性调用 `bus.service()` 处理 FIFO 队列。终态回调在该任务中、总线解锁及 DMA drain 后调用一次，可再次提交或关闭本设备；重复进入 `service` 和回调期间改变生命周期返回 BUSY。`cancel` 请求停止，不能代替完成通知或使 buffers 提前可回收。排队取消不启动硬件；DMA 取消由等待者统一清理。已开始的 HAL polling 传输拒绝取消，以免承诺无法实现的抢占。

旧的异步 TX 接口使用固定 256 字节内部副本，调用成功后 caller TX 可以复用；旧 RX 成功回调接收内部缓冲区，数据只在回调期间有效。旧接口同样需要 `service()`，TX 默认预算 1000 ms，状态查询保留失败状态。超过内部容量返回错误；没有隐藏队列或线程。

## Native SPI 的软件执行证据

Native SPI 采用同一 8 槽设备/队列模型、调用方值 handle 与 generation 失效规则。实际 OSAL mutex 串行执行不同设备的事务，短 metadata guard 原子完成 lookup、准入 pin、队列和关闭。有限 deadline 包含等待锁和模拟 IO；等待者按原预算的短间隔检查取消。TX capture 满时在写入前失败，避免半包且失败后统计不宣称全部发送。RX 注入数据计数不因取出而重复累加。lifecycle 在已准入等待、执行、queued 及 terminal callback 时返回 BUSY。

`SPITest.*:SPIPropertyTest.*` 实际执行 43 项，全部通过，日志/XML 为 `/tmp/nexus-native-spi.log`、`/tmp/nexus-native-spi.xml`。包含两线程共 100 次事务，每次记录 token、CS、speed、mode、bit order 和首字节，分别校验配置；锁等待 deadline 和取消、active 取消、queue timeout/cancel、callback 内 close/reopen、旧副本错误关闭隔离、generation 跨重初始化与 1000 次关闭/回收均有行为回归。保留原随机数据、传输顺序、计数和生命周期断言，随机配置改为显式 open/close，防止把 legacy 池耗尽当作成功覆盖。properties 记录可重放 seed。

host 中额外的模拟延迟和 128 项有界操作 trace 仅用于软件契约验证，不能给出 MCU 的中断或 DMA 时序、SPI 波形和实时性证据。测试 reset 在无活动事务时撤销测试句柄，但不重置 token；生产生命周期不调用测试 reset。

## DMA 终止与生命周期

ST HAL 完成、错误、超时和取消竞争同一个终态槽，首次发布获胜；ISR 通过 `osal_sem_give_from_isr` 通知，任务通过普通 give。回调只查找已登记的本驱动 HAL handle，不对任意 vendor handle 做 container_of。同步和异步路径最终都在任务中停止 SPI DMA request、关闭 stream 与中断、清除 DMA flags 和回调、清除硬件 buffer pointer，再释放 caller buffer。半启动失败也执行 drain。若 stream 的 EN 仍未能清除，默认 fail-stop，不返回仍被硬件访问的内存；产品可以覆盖为带故障记录的安全复位。

DMA 需要真实板级 prepare 完成时钟、引脚、stream 初始化、HAL link 和 IRQ 路由；缺失 prepare 或 DMA handles 返回错误，移除了成功的占位初始化。F407VG 的 64 KiB CCM 无 DMA 访问能力，生产默认校验拒绝 CCM 和未知内存；TX 允许主 SRAM 和内部 Flash，RX 仅允许主 SRAM。

生命周期和电源接口使用当前公共签名，排队、等待、执行与 worker 回调期间均拒绝 deinit/suspend。初始化失败销毁已创建的 OSAL 对象并回收板级资源；硬件停止失败进入 ERROR，需要受控恢复。

## 真实板级代码

`boards/stm32f4discovery/spi.c` 实现 SPI1 PA5/PA6/PA7、PB0/PB1 逻辑 CS 0/1，DMA2 stream 3 TX、stream 0 RX、channel 3。IRQ 通过已有 ISR manager 连接，优先级 5；更改 FreeRTOS syscall priority 后必须重新检查此约束。PD12 用于 LED，避免与 PA5 SCK 冲突。HSE 为板卡资料的 8 MHz 候选配置；PCB revision 和实际电路仍需核实。

F407 的最高 SYSCLK 限制修正为 168 MHz，不能从其他 F4 型号推导出 180 MHz/Over-Drive。GPIO 初始化现在开启对应 RCC port clock，先写输出 latch 再切输出模式，填满 lifecycle 与 power 方法，逻辑 suspend 保留电气状态并抑制读写。共享 port clock 不会因一个 pin deinit 被关掉。没有实现的 EXTI routing 返回 NOT_SUPPORTED。

`soc/stm32f407/flash.c` 当前按F407 xG/xE真实密度暴露完整物理Flash：xG/ZG为1MiB/12sector，xE为512KiB/8sector，包含16/64/128KiB非均匀擦除几何与4字节program。它核对实际密度，验证1→0、HAL错误、读回与cache flush。VDD须满足2.7..3.6V条件。`soc/gd32f470/flash.c`暴露完整1MiB/256个独立4KiB page和2字节program；它不复用ST sector擦除假设。外部layout选择region，默认没有保留storage。读取、program、erase是同步任务操作，产品串行管理，不能保留caller buffers；同bank擦写可暂停取指/IRQ，仅允许产品维护窗口，实板供电与断电恢复仍待验收。

`soc/stm32f407/identity.c` 读取实际 96 位 UID、硅片 device/revision 和 Flash 容量；PCB revision 必须来自板卡资产/工装记录，不能用硅片 revision 代替。

## 执行证据与限制

已执行 standalone host 构建：

```sh
cmake -S tests/drivers -B build/driver-contracts
cmake --build build/driver-contracts
ctest --test-dir build/driver-contracts --output-on-failure
```

最初的3个 CTest target 通过：`driver_stm32_spi_bare`、`driver_stm32_spi_osal`、`driver_stm32_gpio`。两个 SPI target 每个执行 10 组行为检查、6 种 DMA 完成/错误/重复/超时/取消/部分启动/abort 故障组合及 1000 次固定 seed 的 queued 操作；GPIO 检查时钟、输出顺序、生命周期、电源、ISR 拒绝及未实现 EXTI 的错误。测试链接生产驱动 C 文件与 tests/drivers/fake 的 ST HAL 模型；OSAL 模式使用可控锁和信号量模型。它们验证控制流及所有权，不验证真正的 IRQ 抢占、总线传输、芯片 DMA 时序或 FreeRTOS 调度。

SPI、board、identity、Flash 及 GPIO 代码还使用 checkout 中固定 commit 的真实 CMSIS/ST HAL headers 通过宿主 `-fsyntax-only` 检查。此项证明 SDK 符号和函数签名一致，不是 ARM 链接、固件运行或硬件证据。完整 ARM 构建的最新结果由主构建验证记录统一管理。

为恢复完整 Native 构建，还修复了 I2C/SPI/UART 声明中完全缺失的统计字段，以及 Native ISR 的旧公共 API 漂移：一个 IRQ 一个 owner，connect 返回 status 并启用，disconnect 接受 IRQ；执行中的 callback 拒绝 disconnect 防止 context 提前释放，测试同步迁移。Native dispatch 是主机调用，不能声称实现真实硬件优先级或 ISR 上下文。生产 Native shutdown 改为读取已缓存的 I/O 对象并调用实际生命周期，不再依赖测试 helper，也不为 shutdown 创建新设备；watchdog 默认使用 CLOCK_MONOTONIC/Windows QPC，测试通过显式 weak clock 注入点覆盖。

## HIL 退出条件

`profiles/support-matrix.json` 当前区分host契约与8个STM32/GD32F470软件候选，所有MCU组合均保持 `hardware_verification: false`。旧 `hil-report.template.json` 保存未执行的历史模板，不是通过报告；旧 `validate_hil_evidence.py` 已移除。当前入口是 [`scripts/hil/run_hil.py`](../../scripts/hil/run_hil.py)，adapter与manifest契约见 [HIL说明](../../scripts/hil/README.md)：独占板卡和probe租约，执行真实identify/flash/readback/reset/serial/cleanup，核对身份、摘要、测试、预算和超时；未配置设备不能通过，`--model`报告禁止作为物理HIL。企业准入由 `scripts/evidence/release_gate.py` 检查审核过的policy、物理报告及签名身份，不以JSON字段单独证明设备或工装可信。

STM32 仍需实际执行启动/时钟/UART、GPIO、双 slave 并发、DMA IRQ 故障与取消、Flash 任意 program/erase 断电恢复，以及通信/日志/Flash 压力下的控制时序。记录板卡和 PCB revision、UID、source/config/dependency/toolchain identities、实际执行次数、超时清理上界、波形与故障 seed。FreeRTOS 还需检查实际中断优先级与并发锁等待预算。

GD32F470梁山派的官方SDK3.3.3、独立startup/clock/vector/IRQ/GPIO/UART/SPI4/TIMER1/Flash已实现，并有生产端口host fault模型和真实ARM构建。BSP-003/004的软件移植阶段不再标为缺SDK阻塞；GD32 DMA仍明确不支持，实板供电/时钟/IRQ/总线/Flash/控制budget须独立HIL。启明和天空星stock profiles的SPI禁用，启用需另行审核板级资源绑定；Native typed I2C已实施；两 MCU typed I2C provider仍明确不支持，Native模型不表示这些板有生产I2C。禁止把host模型或cross-build当作对应物理资格。
