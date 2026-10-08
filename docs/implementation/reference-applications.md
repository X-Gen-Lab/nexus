# 参考应用与板级启动

对应任务：HAL-001、OS-001、BSP-001、BSP-002。这些应用用于验证构建、接口装配和设备启动，不代表实板时序、工业控制期限或持久化验证已经完成。

## 板级绑定

`boards/native_reference/nexus_board.h` 定义模拟 A0；`boards/stm32f4discovery/nexus_board.h` 定义 MB997 LD4 的 PD12。头文件仅描述逻辑引脚和板级身份，不包含厂商类型。构建目标传递所选板级 include 目录，通用应用从板级读取引脚，不在应用中假定 A0/A1/A2/B0 都存在。

通用 `blinky` 在 STM32 上先执行 `stm32_platform_init()`，建立 HAL 时基、时钟、NVIC 和裸机 OSAL clock，再初始化 OSAL、Nexus HAL 和 GPIO lifecycle。裸机直接执行有界延时主循环；Native/FreeRTOS 创建 OSAL task 后启动后端。检查每一个初始化和创建失败，不使用错误码按位 OR 混合结果。

STM32 的公共 `nx_platform_init()` 现在调用真实、幂等的板级启动函数并传播错误。
未绑定平台的 HAL 弱入口返回 NOT_SUPPORTED，不会因为应用手工启动厂商 HAL
就让空平台入口返回成功。STM32 参考实现暂不提供进程内整体 shutdown：IRQ、DMA、
设备和 scheduler 的 quiescence 需要产品拥有者实现；整体反初始化明确失败并保留
initialized ownership，使用受控复位。不能仅调用 `HAL_DeInit()` 停止仍在运行的系统。
Native 有完整的受维护 I/O 清理路径，其有限应用可检查并退出；MCU 配置演示的
退出清理仍受整体 shutdown 与 stdio adapter 限制。

Native 支持有限次数的可退出 smoke：

```sh
cmake --build --preset linux-gcc-debug --target blinky
build/linux-gcc-debug/bin/blinky --cycles 3
```

它在真实 OSAL task 中通过公共 factory 获取 write/read 能力，执行 toggle，再读取模拟引脚，要求结果为 `1, 0, 1`。主线程 join 后删除 task、反初始化 GPIO，失败返回非零。Native task 在 pthread 创建时运行，因此有限 smoke 不进入长期 `osal_start()` 主循环。无参数运行保持长期 heartbeat。`--cycles` 只接受 1–1000 的十进制整数。

初次真正运行该应用发现 Native 生产路径未注册任何设备，而 GTest 通过手工 test setup 注册后全部通过。生产平台现在遍历生成配置中的注册对象，传播冲突和初始化错误，清理后允许重试；应用不依赖 test helper 填充设备表。真实 blinky smoke 和不含 fixtures 的生产 registry 合同测试均已通过。

## OSAL producer/consumer

`freertos_demo` 使用同一个 board LED，初始化其 lifecycle。任务工作流为：

1. 分配容量 10 的消息队列、统计互斥锁和四个 worker 的启动 gate。
2. 创建 producer、consumer、heartbeat、statistics 四个 task，全部成功后释放 gate。
3. producer 每 100 ms 产生带 sequence 的模拟值，满队列采用计数并丢弃策略。
4. consumer 在队列中最多等待 100 ms；允许丢弃造成的 sequence 缺口，拒绝重复或乱序。
5. 统计锁最多等待 50 ms，打印发生在释放锁之后。heartbeat 是唯一 GPIO writer。

队列已经提供 data-ready 同步，移除旧例子重复的通知 semaphore；新的 semaphore 只用于启动 barrier。共享退出标记采用 C11 atomics，任务使用合作停止和有界等待。资源/创建失败不能启动残缺应用；Native 已启动 worker 先 join 再回收共享资源，join 失败时保留资源；尚未启动的 FreeRTOS task 无法 join，因此保留其资源并返回失败，不能释放仍被引用的资源。

Native 可使用 `freertos_demo --run-ms 500` 执行有限运行：生成并消费至少一条消息，再请求停止全部 worker，join 后逐项检查 task/queue/semaphore/mutex/GPIO 的回收结果。参数接受 100–10000 ms。任何运行、join 或回收失败返回非零；无参数运行保留长期 demo 模式。

该应用明确拒绝裸机后端。Native/FreeRTOS 的功能一致性仍需对应运行报告；task stack、printf transport 和资源上限需要实际产品预算和上板测量。

## 易失 RAM 配置示例

`config_demo` 是有限运行的配置管理示例，无参数执行后返回进程状态。它检查每项配置 API 和清理操作的返回值，读取缓冲全部初始化，失败时不会输出未定义的字符串。Native 使用标准输出；STM32 先执行共享平台初始化，再初始化 OSAL 与 Nexus HAL，printf/stderr transport 由产品提供。

```sh
cmake --build --preset linux-gcc-debug --target config_demo
build/linux-gcc-debug/bin/config_demo
```

验证顺序如下：

1. 写入并读取 i32、u32、i64、float、bool、string、blob 共七种类型，比较实际值；验证缺失数值键采用调用者的默认值。
2. 在只有默认 namespace 条目的时候导出 JSON，改变数值后通过 JSON 恢复并重新检查类型和值。
3. `motor` 与 `network` 使用相同 key、不同值，检查两个 namespace 与默认 namespace 之间的隔离；明确验证全局 JSON 对非默认 namespace 条目返回 `CONFIG_ERROR_UNSUPPORTED`。
4. 使用 `config_export_namespace()`/`config_import_namespace()` 完成 motor JSON 往返，验证 network 的值保留。
5. 导出全部 11 个条目的 binary v2，验证截断且带 CLEAR 的输入被拒绝、原值保留，再修改三个 namespace 中的数值并通过完整 binary 恢复。

binary v2 的往返保留同一次 manager 生命周期中的 namespace map；该格式不能单独代替携带 namespace map 的 NXCS 持久化快照。应用使用各 2048 字节的 JSON 和 binary 静态缓冲，超过预算立即失败。JSON 容量查询按字符最坏转义长度预留空间，应用分别报告所需容量、固定预算和实际输出长度。类型不匹配、非法参数、缺失字符串、小导出缓冲及无 backend 的 commit 都有明确预期错误检查。

示例仅使用 Config Manager 的 RAM store，并验证 `config_commit()` 返回 `CONFIG_ERROR_NO_BACKEND`，不分配 RAM backend 的额外快照区。所有值在反初始化或复位后丢失；它不测试 Flash、断电恢复、加密或 RTOS 调度。全部配置操作由一个管理上下文串行执行。

## Native Shell console

`shell_demo` 仅提供 Native 标准输入输出 console，控制所选 board 的一个 LED。启动时从公共 factory 获取 GPIO write/read 能力，显式初始化 lifecycle，检查 Shell/command/backend 的初始化与注册状态。结束时关闭 LED、释放 Shell 内存、解除 backend，并检查 GPIO 与 HAL 清理结果。

```sh
cmake --build --preset linux-gcc-debug --target shell_demo
build/linux-gcc-debug/bin/shell_demo --smoke
build/linux-gcc-debug/bin/shell_demo
```

无参数时应用通过 `fgets()` 在 Shell 外读取完整行，Shell backend 的 read 仅消费已有内存缓冲，满足非阻塞读取契约。标准输出 backend 检查每次写入和 flush，输入超长直接拒绝，`quit` 或 EOF 结束会话。它是主机行输入 console，不声明终端 raw-mode 行编辑能力。命令包括 `led on/off/toggle/status`、范围为 1–1000 ms 的 `delay`、`info`、`quit` 和已注册的 Shell 内置命令。

`--smoke` 通过真实 `shell_process()`、解析和注册 handler 执行 10 条命令，逐条验证 handler 状态与公共 GPIO 回读；非法 action、多余参数和负数 delay 是预期失败项。重复/非法 command 注册也必须返回指定错误。任何非预期状态、输出失败、设备状态或清理失败均返回非零。未知启动参数返回 2。

MCU 的 demo 配置和源码明确拒绝没有产品 console binding 的 Shell 示例；不会自动假定 UART0 可用或把开启 UART 等同于绑定 stdio。产品需要先提供拥有明确生命周期、接收、发送、错误和权限政策的 console adapter，再启用相应产品应用。

## STM32 启动与诊断

直接 STM32 HAL `stm32_blinky` 改用 PD12，与 MB997 板级契约一致。`stm32_config_test` 调用共享平台初始化，删除应用中另写一份的 PLL/NVIC 初始化和未被实际 IRQ 调用的 `HAL_SYSTICK_Callback()`。轮询 HAL tick 使用无符号 elapsed subtraction，跨回绕仍可控制 heartbeat 与五秒摘要，避免 modulo 判断在同一个毫秒反复打印。

诊断同时显示配置值与观测的 SYSCLK、芯片 ID/revision、96-bit UID、Flash 容量和 AIRCR group 编码。它不执行 Flash erase/program，也不声称完成持久化验证。printf 必须由产品显式接入 UART/ITM/debug transport；Kconfig 开启 UART 本身不会接管 stdout。

平台把 Kconfig 的逻辑 NVIC group `0..4` 映射为 HAL/CMSIS 编码 `7-n`；group 4 对应 AIRCR 3。FreeRTOS 只允许全 preemption 的 group 4。FreeRTOS profile 中 SVC/PendSV 由 kernel port 定义，F4 interrupt 文件仅提供一个 SysTick wrapper：先维护 HAL 1 ms tick，scheduler 启动后再调用真正 `xPortSysTickHandler()`。Kconfig 固定该组合为 1000 Hz，避免 HAL_Delay 与 kernel tick 单位不同。

## 当前验证范围

- 通用 blinky、OSAL demo 在 Native 头文件下通过 GCC C11 `-Wall -Wextra -Werror -fsyntax-only`。
- 改写后的 config_demo、shell_demo 也通过 Native GCC C11 `-Wall -Wextra -Werror -fsyntax-only`。config_demo 的 STM32 Baremetal/FreeRTOS SDK 语法检查通过，MCU shell_demo 按 console 绑定限制明确拒绝；该检查未生成 ARM 机器码。
- config_demo 已在 `linux-gcc-debug` 实际 CMake 构建和运行，退出状态为 0：JSON 所需容量 1098 字节、实际 336 字节，11 项 binary v2 数据 253 字节。默认/独立 namespace JSON、全部 namespace binary 恢复、截断 CLEAR 保留值及指定预期错误检查全部通过。日志为 `/tmp/nexus-config-demo-build-final.log`、`/tmp/nexus-config-demo-smoke-final.log`，源码、工件、有效配置与工具链身份记录为 `/tmp/nexus-config-demo-evidence.json`。Shell 的真实 `--smoke` CTest 已通过，包含 10 条真实 parser/handler 命令与公共 GPIO 回读。
- 使用真实 ST HAL/CMSIS 和锁定 FreeRTOS 源码头文件，对 STM32 应用、配置显示、平台启动和中断文件执行 13 个支持配置组合的 GCC 语法检查，全部通过；另一个裸机 task-demo 组合按设计明确拒绝。宿主 LP64 下厂商 ADC 头出现位宽相关 overflow warning；这些检查未生成 ARM 机器码。
- 本环境没有可用 ARM GCC/newlib 工具链，不能把语法检查或工作流定义作为完成 Cortex-M4 编译/链接的证据。
- 实板 SysTick、FreeRTOS 启动、NVIC 优先级、PD12、stdio transport 和资源预算的 HIL 尚未执行。

最终普通 Native 整合执行通过 1696/1696 CTest 注册，包含 blinky、config_demo、shell_demo、industrial_controller 四个有限 smoke；报告为 `build/linux-gcc-debug/ctest-results.xml`，日志为 `/tmp/nexus-native-final-all.log`。随后为四任务 `freertos_demo --run-ms 500` 增加长期 CI 覆盖，补充 CTest 1/1 通过，报告为 `build/evidence/native-freertos-demo-final.xml`。五个有限应用和 18 个独立合同随后共同通过 ASan/UBSan 23/23 注册；本容器 ptrace 限制使该次运行使用 `detect_leaks=0`，CI 保留 leak checking。

SDK 检查日志位于 `/tmp/nexus-app-official-sdk-syntax.log` 与 `/tmp/nexus-config-shell-sdk-syntax.log`。最终强平台启动桥与 IRQ 文件另以新 Baremetal/FreeRTOS 配置执行四个真实 SDK 宿主语法检查并全部通过，日志为 `/tmp/nexus-final-sdk-syntax.log`；这些仍不生成 ARM 机器码。最终交付应归档到与提交、有效配置及工具链绑定的验证包。
