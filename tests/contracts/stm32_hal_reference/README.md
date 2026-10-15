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
cmake --build /tmp/nexus-hal-reference --parallel 2
ctest --test-dir /tmp/nexus-hal-reference --output-on-failure
```

The explicit sanitizer choices are `none`, `address` and `thread`. Use separate
build directories. The CTest result and `reference.xml` must report all four
GoogleTest cases completed, with no failures or skips. Missing framework bytes,
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
目录运行；必须检查 CTest 和 `reference.xml`，确认四项 GoogleTest 全部执行且无失败、
无跳过。缺少依赖、来源不一致、不支持的主机或无法取得映射均作为失败报告。

模型通过 `MAP_FIXED_NOREPLACE` 请求独占 MMIO／SRAM 地址，绝不覆盖已有映射；
仅替换主机不存在的 CPU 原语，并显式提供硬件状态变化。官方代码不修改。模型
不提供物理总线、自动 W0C、副作用时序、DMA 仲裁或中断抢占证明。

四项测试分别验证正常 DMA 完成仍等待 UART TC、abort callback 可以先于物理 TC、
无活动 DMA 时 abort 可同步回调，以及 DMA 启动失败后 HAL 的私有 payload 指针仍在。
这些行为需要适配，不表示 HAL 有缺陷，也不能直接等同于 Nexus SETTLED。

`layout.c` 使用相同 Arm CPU／ABI 参数编译后，可通过命名数组记录各结构体大小。
它只提供布局事实，不把 HAL Handle 大小之和冒充完整替代方案的资源成本。
