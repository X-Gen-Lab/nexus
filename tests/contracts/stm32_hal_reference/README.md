# Official STM32F4 HAL behavior reference

This explicit Linux-host comparison executes the original locked HAL UART and
DMA sources. It is separate from the production provider, which selects official
LL operations and retains one Nexus request/drain state machine. It does not
qualify silicon timing, interrupt latency, physical DMA or Board wiring.
On Linux it is also selected by the normal Native GoogleTest plan, including
the commit gate; the standalone entry shares the same `reference.cmake` graph.

The source lock in `dependencies/stm32f4-hal.lock.json` owns the upstream revision
and CMSIS pairing. Configuration verifies both clean official Git checkouts. The
reference needs the full repository checkout; the minimal Source SDK deliberately
contains only the production LL closure and cannot build this fixture.

## Execute

Use the installed repository CMake/Ninja tuple, with its executable directory on
`PATH`. Choose an out-of-source build directory:

```sh
cmake -S tests/contracts/stm32_hal_reference -B /tmp/nexus-hal-reference \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNEXUS_REFERENCE_SANITIZER=none
cmake --build /tmp/nexus-hal-reference --parallel 1
ctest --test-dir /tmp/nexus-hal-reference --output-on-failure
```

The explicit sanitizer choices are `none`, `address` and `thread`. Use separate
build directories. The CTest result and `reference.xml` must report every discovered
GoogleTest case completed, with no failures or skips. Missing framework bytes,
source mismatch, unsupported host and unavailable MMIO mappings fail execution.
No network fallback or skipped-only success is provided.

## Model and observations

The fixture maps isolated zero-filled regions at the real F4 peripheral and SRAM
addresses with `MAP_FIXED_NOREPLACE`; it never replaces an existing mapping.
It supplies equivalent host CPU primitives and explicit hardware status facts.
Vendor source is unchanged. Register storage does not emulate physical bus
timing, automatic W0C behavior, DMA arbitration or interrupt preemption.

The four maintained observations are:

1. Normal UART DMA completion waits for the separate UART TC observation.
2. DMA abort completion may restore HAL READY before physical UART TC.
3. Abort without active DMA may call back before the abort API returns.
4. DMA admission failure restores UART READY while its private TX pointer remains.

These are HAL behaviors to adapt, not claims that HAL is defective. In particular,
an abort callback or Handle READY is insufficient to publish Nexus SETTLED.

## Finite transport evaluation and production decision

The private `adapter.c` evaluation uses the real UART and DMA source through one
serialized owner. It is limited to USART1, DMA2 Stream7/channel 4, normal finite
8-bit TX in reviewed SRAM. Its caller supplies already initialized Handles and
explicit IRQ/service calls. No other owner may access those Handles or registers.
It is not selected by a product assembly, installed SDK or firmware target.

The adapter establishes admission before calling HAL, uses the Nexus absolute
request deadline, and defers settlement until the complete HAL call stack has
returned. Cancel and timeout retain the payload through abort callbacks and the
separate wire-TC observation. A fixed 1000 microsecond drain-observation budget
quarantines unresolved storage; another cancel cannot extend it. Service clears
TX register addresses, payload pointers and DMA callback references before the
Core release publication. Failed HAL startup is an accepted operation followed
by explicit withdrawal, because HAL has already retained references. A real DMA
TE interrupt causes an I/O failure withdrawal without waiting for the request
expiry or falsely reporting success.

The additional behavior cases cover rejection before effects, inaccessible SRAM,
DMA-versus-wire completion, synchronous callback reentry, cancel and quarantine,
startup failure, absolute deadlines, late IRQs, DMA errors, reserved clock values
and inconsistent remaining counts. The existing four
raw HAL observations remain independent. A new evaluation behavior requires a
real failing regression before its implementation; these tests do not replace
production-provider lifecycle tests.

`HAL_GetTick` uses a progressing host monotonic clock, independent of the request
clock passed to service. It does not install SysTick or model an MCU tick port.
The normal TX/abort-IT and observed error cases do not read that tick. In this
locked HAL revision, `HAL_DMA_Init`, blocking `HAL_DMA_Abort` and polling APIs can
wait using `HAL_GetTick`; the DMA IRQ TE cleanup instead polls EN using a count
budget derived from `SystemCoreClock / 9600`. That is a bounded loop count, not a
measured ISR duration or an accepted Nexus production IRQ budget. The host model cannot
hold EN against a CPU clear and cannot establish that failure's physical timing.

Retain the single existing production LL/provider implementation. The reference
proves a narrow adaptation is feasible; it does not replace the production mode
set, NVIC/preemption withdrawal, simultaneous RX, board clock bootstrap, measured
IRQ budgets or physical qualification. No runtime selection switch or second
peripheral state machine is added. Promotion requires those missing contracts
and an equivalent linked-image cost comparison, not a general preference for
HAL or register code.

The independent `nexus_gd32_official_gpio_test` target executes the locked GD32F4xx
3.3.3 `gd32f4xx_gpio.c` through the real GD32F470 provider. It checks selected-pin
writes, independent banks/instances, and invalid-mask rejection before clock or
GPIO effects. It fills the real-vendor-function boundary that the broader GD32
MMIO model substitutes. It does not emulate GPIO electrical behavior; the current
production SPL calls remain unchanged.

`layout.c` can be compiled with the maintained Arm compiler and the same CPU,
hard-float and short-enum flags as F407 firmware. Its named arrays expose separate
`sizeof` values through `arm-none-eabi-nm -S`. They compare structure layouts only;
the sum of HAL Handles is not the cost of a complete replacement provider.

## 中文说明

该夹具在 Linux 主机执行锁定版本的官方 HAL UART／DMA 原始代码。生产 provider
仍选择适用的官方 LL 操作，并由 Nexus 独自管理 request、取消、排空与最终借用归还。
它不能证明真实芯片时序、IRQ 延迟、DMA 行为或板级电气资格。
Linux 的正常 Native GoogleTest／commit 计划也执行该目标；独立入口复用同一
`reference.cmake` 源码图。

依赖版本与 CMSIS 配对由现有官方库锁定文件决定。配置阶段验证两个原始 Git
checkout；Source SDK 只包含生产所需的 LL 文件，因此不支持构建此比较夹具。

上面的命令使用仓库安装的 CMake／Ninja。`none`、`address`、`thread` 分别在独立
目录运行；必须检查 CTest 和 `reference.xml`，确认发现的所有 GoogleTest 全部执行且无失败、
无跳过。缺少依赖、来源不一致、不支持的主机或无法取得映射均作为失败报告。

模型通过 `MAP_FIXED_NOREPLACE` 请求独占 MMIO／SRAM 地址，绝不覆盖已有映射；
仅替换主机不存在的 CPU 原语，并显式提供硬件状态变化。官方代码不修改。模型
不提供物理总线、自动 W0C、副作用时序、DMA 仲裁或中断抢占证明。

四项测试分别验证正常 DMA 完成仍等待 UART TC、abort callback 可以先于物理 TC、
无活动 DMA 时 abort 可同步回调，以及 DMA 启动失败后 HAL 的私有 payload 指针仍在。
这些行为需要适配，不表示 HAL 有缺陷，也不能直接等同于 Nexus SETTLED。

`layout.c` 使用相同 Arm CPU／ABI 参数编译后，可通过命名数组记录各结构体大小。
它只提供布局事实，不把 HAL Handle 大小之和冒充完整替代方案的资源成本。

## 有限传输评估与生产选择

私有 `adapter.c` 通过单一串行 owner 调用真实 UART／DMA 源码，限定为 USART1、
DMA2 Stream7／channel 4、普通有限 8 位 TX 和已审查 SRAM。调用者提供已初始化
Handle，并显式推进 IRQ 与 service；其他 owner 不得访问同一 Handle 或寄存器。
它不进入产品装配、Source SDK 或固件目标。

适配层先建立 admission，再调用 HAL；使用 Nexus 绝对 deadline，并等待整个 HAL
回调调用栈返回后才允许 settlement。取消与超时必须跨过 abort callback、独立 wire
TC 和撤引用；固定 1000 微秒排空观察预算耗尽后保留 QUARANTINED，重复取消不会延长
预算。service 在 Core release publication 前清除 TX 地址、payload 和 DMA callback
引用。HAL 启动失败时已经持有引用，因此是先接受、再显式撤回；真实 DMA TE 错误直接
触发 IO 原因的撤回，不等待请求超时，也不误报成功。

新增行为覆盖拒绝无副作用、不可达内存、DMA／wire 完成区别、同步回调重入、取消与
隔离、启动失败、绝对 deadline、迟到 IRQ、DMA 错误、保留时钟值与异常剩余计数；
原四项 HAL 观察独立保留。
新增行为按真实 RED 后实现 GREEN，不能代替生产 provider 的生命周期验证。

`HAL_GetTick` 使用独立推进的主机单调时钟，与 service 的请求时间参数分开；它不安装
SysTick，也不代表 MCU tick port。正常 TX／abort-IT 与本夹具观察的错误路径不读取该
时钟。锁定版本的 DMA Init、阻塞 Abort 和轮询 API 可能使用 tick 等待；DMA IRQ 的
TE 清理则使用由 `SystemCoreClock / 9600` 推导的计数预算轮询 EN。循环次数有限，
但这不是已测量
ISR 耗时，也不代表满足生产 IRQ 预算。主机模型不能模拟 CPU 清 EN 后硬件仍保持 EN，
因此不能证明该故障路径的实板时序。

生产继续只选择现有 LL／provider 实现。此参考证明窄模式可适配，不代表已经完成
全部生产模式、NVIC／抢占排空、并行 RX、板级时钟启动、IRQ 预算与物理验收。没有新增
运行时切换开关，也没有两个状态机共管外设。推广需要补齐这些合同，并比较等价模式
的链接镜像成本；不以 HAL 或寄存器代码的泛化偏好作结论。

独立 `nexus_gd32_official_gpio_test` 通过真实 GD32F470 provider 执行锁定的
GD32F4xx 3.3.3 `gd32f4xx_gpio.c`，验证选定引脚、多个 bank／instance 隔离，以及非法
mask 在时钟与 GPIO 副作用前被拒绝。它补齐广域 GD32 MMIO 模型替代的 vendor 函数
边界，不模拟 GPIO 电气行为；现有生产 SPL 调用保持不变。
