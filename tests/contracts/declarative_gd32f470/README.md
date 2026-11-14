# GD32F470 声明式接入软件参考

本目录用于验证第二家厂商通过 `package.json`、事实和 binding 数据接入同一配置工具、
静态 factory 与公共接口。它属于显式 `reference` 范围，不是生产 SoC；当前生产范围
仍为 STM32F4。包内没有 Python 插件，没有应用任务、动态对象池或隐藏 worker。

## 完整链路

```text
assembly.toml + board/board.json
        + package.json / soc.json / routes.json / bindings.json
    → 公共 resolver / 不可变 IR
    → 静态 config / state / strong factory / 薄 IRQ
    → 包内 CMake
    → Nexus 公共合同 + 私有 GD32 官方 SPL
    → ARM ELF / BIN / MAP / 资源报告
```

`gpio.c` 与 `uart.c` 实现实际 C 操作，factory 只返回静态 face。`system.c` 建立内部
IRC16M 的固定运行域和 TIMER1 的 1 MHz 时间基准，保留 RTC、PLL、外部晶振和 RTOS
范围之外的责任。UART 的 PA9／PA10 AF7、GPIO 的 PA4 仅为数据手册中的硅片路线，
不表示梁山派或其他 PCB 已审核此接线。`physical_pcb_revision` 保持空值，物理 HIL
仍为 `not_executed`。

`gpio-only.toml` 验证选编边界：它不编译 UART 实现和 USART SPL，也不取得 USART0
时钟或 PA9／PA10 路线。两个 assembly 复用同一包与消费者，不增加另一套注册表。

## 精确边界

- 型号为 GD32F470ZGT6，LQFP144；资料身份见 `references.json`。
- 固定 IRC16M，8N1 UART、IRQ TX 与 bytes/events RX；不支持 DMA、流式 blocks、
  其他 USART、动态时钟、低功耗或内核 IRQ 调用。USART0 IRQ 只推进硬件事实，任务
  调用 `service` 执行截止时间检查和最终结清。
- GPIO 使用显式授权 mask；混合 set/reset 使用一条 BOP 写，官方 SPL 的独立置位／
  复位函数不能保证这一公共原子合同。其他 GPIO 配置与 toggle 使用官方 SPL。
- UART 使用官方 SPL 配置及数据操作；一次 STAT0 快照加 DATA 读取保留错误与字节
  配对。SPL 的布尔 flag 查询无法提供这一快照，因此该处直接读取私有 SDK 寄存器。
- 取消与超时停止新增 payload 访问，借用持续到真实 TC。硬件不再产生 TC 时，请求
  保持 DRAINING，`stop` 返回 BUSY；不能据超时释放请求或缓冲。本参考没有强制复位
  UART 的恢复策略。产品需要保持 service 进度并决定恢复责任。
- RX 环满后保留一个独立 LOSS／NO_BYTE 边界；该边界发布前继续丢弃新字节，防止
  新字节越过已丢失的数据边界。存储容量显式；bytes 面也使用声明的事件槽，不声称
  已做到最小 byte-ring 成本。
- 控制器、配置、环形存储和请求在 IRQ 撤回、SETTLED 与调用者会合前必须保持有效。
  一个特权且未屏蔽的任务串行执行准入、service、取消和关闭；没有跨任务自动加锁。
- TX 描述符与 payload 均不得重叠控制器、配置或 RX 环；RX 的输出和 count 也不得
  写入仍借用的 TX 描述符或 payload。检查先于准入和复制，不修补调用者存储。
- `stop` 先关闭准入并取消剩余 TX payload，再等待真实 TC 和 service 结清；即使
  请求已经结清，重新准入也保持关闭，直到 stop 撤回 IRQ 并结束整个控制器生命周期。
- GPIO 和 UART 路线在 bank 时钟开启后检查所选 pad 的硬件锁，再写 latch／AF。
  这些 GPIO／路线字段写入及恢复均检查读回；失败保留配置或共享资源 receipt 和所需时钟，
  由调用者重试退出，不把部分恢复报告为已经释放。
  此范围不声明 UART prepare／deinit 对全部格式寄存器的完整读回。
- TIMER1 溢出必须在下一整圈前处理，最大间隔严格小于 `2^32` 微秒；不能由计数器
  推断漏掉的完整回绕。停止后旧 deadline 不得继续使用。
- `SystemInit` 和 timer/resource 初稿先于专门运行期测试；后续外来 timer 接管、GPIO
  slew、RX 顺序与输出别名缺陷使用真实 RED/GREEN 回归，没有重构历史测试顺序。

## 来源与存储事实

官方 SDK 为原位保留的 GD32F4xx Firmware Library 3.3.3，来源、许可与逐文件摘要在
[锁文件](../../../vendors/gigadevice/gd32f4xx/source.lock.json)。它是受审源码导入，
不是虚构的 Git 子模块。公共 CMSIS 使用当前锁定的 CMSIS 6；GD32 类型仅在私有
实现和受信装配内可见。

精确芯片事实复用固定历史 `03fc3f84` 的已锁定 DS Rev2.2、RM Rev3.3 来源索引：
1 MiB Flash，192 KiB 主 SRAM、256 KiB 附加 SRAM、64 KiB TCM，4 KiB page erase。
它们不代表本目录提供 Flash 驱动或启用可选内存。SDK 的 `gd32f470xG_flash.ld` 写有
512 KiB Flash，与该精确器件资料冲突；本参考使用 Nexus 从事实生成的链接脚本，
不复用这一数值。资料来源、软件实现和物理资格分别表达。

默认 Source SDK 不导出该参考或 GD32 库存；以下命令在准备好锁定工具与依赖的完整
源码 checkout 中执行，不触发下载、烧写或调试器操作。

## 验证

从仓库根执行，先把锁定 CMake、Ninja 和 ARM 编译器放到 PATH：

```sh
python tools/configure/configure.py check \
    --assembly tests/contracts/declarative_gd32f470/assembly.toml --test-support
cmake -S tests/contracts/declarative_gd32f470/testing \
    -B build/gd32-reference-host -G Ninja \
    -DNEXUS_GD32_VENDOR_ROOT="$PWD/vendors/gigadevice/gd32f4xx"
cmake --build build/gd32-reference-host --parallel 2
ctest --test-dir build/gd32-reference-host --output-on-failure --no-tests=error
cmake -S tests/contracts/declarative_gd32f470/consumer \
    -B build/gd32-reference-arm -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEXUS_GD32_VENDOR_ROOT="$PWD/vendors/gigadevice/gd32f4xx"
cmake --build build/gd32-reference-arm --parallel 2
```

验证 GPIO 单独选择时，为另一个构建目录增加
`-DNEXUS_ASSEMBLY_FILE="$PWD/tests/contracts/declarative_gd32f470/gpio-only.toml"`。

Host 程序执行真实 Nexus C 与未修改的 SPL 源码。MMIO、NVIC、Cortex 上下文和定时器
W0C 是外部硬件模型；它不模拟整颗 MCU。LP64 host 的 C 编译针对 SDK 的 32-bit
MMIO 宏关闭 int-to-pointer-cast 告警；仅原厂源另外关闭其 32-bit mask 的
conversion warning。其他自有代码警告保留。ARM 编译保持真实
C11／M4F／hard-float／short-enums 上下文和完整警告。ARM 产物只证明编译、链接和资源预算。

## 已安装 SDK 与外置参考包

默认 F4 Source SDK 不包含 GD32 库存。外置包的 `references.json.supplier` 固定原始
`source.lock.json` 的 SHA256；CMake 在建立任何库之前比对该摘要，并调用所选 SDK
的 `verify_vendor_source.py` 逐文件检查完整 Firmware。缺少供应包、锁文件改字节或
Firmware 改字节均先拒绝，不创建目标。GD32 原始锁文件、来源 URL、许可与上游归档
摘要保持原样。`soc.source_lock` 只声明实际消费的 CMSIS 精确 Git pin。

外置 GD32 供应包和 host 模型不属于核心 SDK 的签名或快照身份。维护入口显式传入
供应目录，不从旧 checkout 隐式补文件；正式报告分别记录核心 SDK revision、外置
输入 `inputs.json`、供应校验和产物摘要。GPIO-only 必须没有 UART 私有实现和 USART
SPL。下面的准备命令只复制公开参考、原锁定供应内容和测试支持，不复制 `.git`、
凭据或构建缓存；输出目录必须是新目录。
准备脚本先验证上述锁文件摘要与完整供应内容，通过后才创建输出目录。

```sh
python tests/contracts/declarative_gd32f470/testing/prepare_external.py \
    --output /tmp/nexus-gd32-external
```

使用实际已发布的安装前缀、对应 source revision 和工具链。以下变量由调用者提供，
不能指向旧源码工作区。先执行完整 SDK 验证，再配置两个独立 ARM 构建目录：

```sh
SDK_SOURCE="$SDK_PREFIX/share/nexus/src"
EXTERNAL=/tmp/nexus-gd32-external
python "$SDK_SOURCE/cmake/package/package_source_sdk.py" --verify "$SDK_SOURCE"
cmake -S "$EXTERNAL/provider/consumer" -B "$EXTERNAL/arm" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SDK_SOURCE/cmake/toolchains/arm-gcc.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNEXUS_INSTALLED_PREFIX="$SDK_PREFIX" \
    -DNEXUS_EXPECTED_SOURCE_REVISION="$SDK_REVISION" \
    -DNEXUS_GD32_VENDOR_ROOT="$EXTERNAL/suppliers/gd32f4xx"
cmake --build "$EXTERNAL/arm" --parallel 2
```

GPIO-only 用另一个 binary directory，并增加
`-DNEXUS_ASSEMBLY_FILE="$EXTERNAL/provider/gpio-only.toml"`。每个目录保留 ELF、BIN、MAP、
资源报告、`compile_commands.json` 与 `resolved-sdk.txt`，检查所有平台源、头文件和
链接输入都来自安装前缀或显式外置输入，不能回读原 checkout。

Host 模型使用同一安装源和外置支持，执行全部 GoogleTest，不按用例过滤：

```sh
cmake -S "$EXTERNAL/provider/testing" -B "$EXTERNAL/host" -G Ninja \
    -DNEXUS_INSTALLED_SOURCE="$SDK_SOURCE" \
    -DNEXUS_EXPECTED_SOURCE_REVISION="$SDK_REVISION" \
    -DNEXUS_GD32_VENDOR_ROOT="$EXTERNAL/suppliers/gd32f4xx" \
    -DNEXUS_GTEST_ROOT="$EXTERNAL/support/googletest" \
    -DNEXUS_CMSIS_HOST_ROOT="$EXTERNAL/support/cmsis_host"
cmake --build "$EXTERNAL/host" --parallel 2
ctest --test-dir "$EXTERNAL/host" --output-on-failure --no-tests=error
NEXUS_INSTALLED_SOURCE="$SDK_SOURCE" \
NEXUS_GD32_VENDOR_ROOT="$EXTERNAL/suppliers/gd32f4xx" \
python "$EXTERNAL/provider/testing/test_supplier_admission.py"
```

该流程证明显式 reference 包可以消费已安装平台，不能提升为生产支持或物理 HIL。
