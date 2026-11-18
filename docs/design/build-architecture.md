---
id: design.build-architecture
type: reference
audience: maintainer
source_language: zh_CN
---

# 构建系统架构审查与重构方案

本文说明应用如何生成固件、家族包如何接入器件，以及构建系统如何判断哪些工作需要重做。实现说明对应本次重构；计时表保留审查基线 `5e0b512c`，文末 28 项保留原计划与验收条件。日常反馈与完整交付分别按 B01–B11 验证。[构建合同与验证计划](build-system.md#verification-plan-for-additional-mcu-families)

## 从应用配置到可烧录镜像

1. **选择构建环境。** 应用 preset 在首次 configure 前选择 Ninja、工具链、独立 binary directory 和 `NEXUS_ASSEMBLY_FILE`。工具链负责定位编译器与交叉编译环境。配置解析后，平台把确切 CPU、FPU、浮点 ABI 和优化选项施加到相关目标。一个构建目录对应一套 assembly 和工具链，同一套配置可以创建多个固件目标。[工具链](../../cmake/toolchains/arm-gcc.cmake) · [目标编译选项](../../cmake/platform/Options.cmake)
2. **解析硬件意图。** 开发者编写 TOML，选择板卡、后端、组件和 I/O 实例。主机 resolver 结合 Board、SoC 事实及 binding 检查资源和合法模式，从同一份类型化结果原子生成静态 C、factory 头、链接脚本、CMake 选择数据与审查用 JSON。它还记录事实和工具输入，让 CMake 知道何时应重新配置。失败与输出内容相同各有既定的发布语义。[配置接线](../../cmake/platform/Configure.cmake) · [生成端](../../tools/configure/emission.py)
3. **建立目标图。** 根 CMake 接入 Core、Arch、I/O、OS、组件及选中的 SoC 包。包的 CMake 决定具体模式对应哪些 C 源码和厂商库。生成的 `bindings.c` 编入 `Nexus::Platform`，组件使用公共领域接口。固件入口负责将应用、平台、启动对象和链接脚本连成可执行文件。Ninja 按依赖执行编译、链接和产物检查。[根入口](../../CMakeLists.txt) · [provider 合同](../../cmake/platform/Provider.cmake) · [固件目标](../../cmake/platform/Firmware.cmake)
4. **交付与消费。** 外部工程通过源码或已安装的 Source SDK 取得同一套公共目标。SDK 还要验证内容身份、搬迁后的路径、编译接口、链接和资源。CPU Runtime 入口供调用方自行负责启动与板级集成。其产物与物理板卡固件有不同的责任边界。[SDK 入口](../../cmake/package/NexusConfig.cmake.in) · [SDK 教程](../tutorials/source-sdk.md) · [CPU Runtime](../../cmake/platform/Runtime.cmake)

![配置、静态实例与固件产物的关系](../assets/diagrams/configuration-generation.svg)

应用只需提供 assembly 与应用源文件。以下 Source SDK 示例假定 preset 已设置工具链、`NEXUS_ASSEMBLY_FILE` 和 SDK 的 `CMAKE_PREFIX_PATH` 或 `Nexus_DIR`，应用也已调用 `project()`。源码消费通过 `add_subdirectory` 接入平台，随后调用相同的固件函数。

```cmake
find_package(Nexus CONFIG REQUIRED)
nexus_add_firmware(app SOURCES main.c)
```

**首次构建与热构建。** 准备锁定依赖和 ARM 工具链后，在平台仓库根目录运行 `cmake --preset stm32-qiming-baremetal`，再运行 `cmake --build --preset stm32-qiming-baremetal --target nexus_firmware`。之后修改源码或原地修改 assembly，只需重跑第二条命令。Ninja 会按输入变化重新配置、编译或链接。产物位于 `build/stm32-qiming-baremetal/bin/`。[环境准备](../guides/source-checkout.md) · [板卡 preset](../../cmake/presets/stm32.json)

**外部应用。** 按教程生成最小 GPIO 工程，选择本地源码或已安装 SDK，然后执行模板输出的 CMake 命令。应用拥有 `assembly.toml` 和 `main.c`；更新时沿用应用自己的构建目录。[可执行的外部应用教程](../tutorials/source-sdk.md#public-path-create-the-minimal-gpio-consumer)

## 历史基线与实测成本

以下测量来自重构前的 `5e0b512c`。在独立构建目录用两个并行任务构建 `stm32-qiming-baremetal` 的 `nexus_firmware`，首次配置开启 ControllerModes 的 CMake trace。表中墙钟时间包含主机缓存和跟踪开销。

| 观察项 | 实测 | 工程含义 |
| --- | --- | --- |
| 首次 configure | 4.144 秒，同一模块读取 resolved.json 35 次 | 消除各 IP 模块重复解析同一选择结果，收益须以对照组复测 |
| 首次编译与链接 | 3.278 秒，固件闭包 44 条编译命令 | 全目录编译数据库有 149 条记录，单固件的实际工作应查 Ninja 目标闭包 |
| 原输入直接再次构建 | 0.224 秒，零编译、零链接 | 保留 Ninja 热路径，优先处理总是先 configure 的包装入口 |
| 仅删除临时树中的 BIN | 0.938 秒；直接构建固件恢复 BIN，ELF 与 BIN 各自与原件字节相同 | 产物重建与预算检查是用户依赖的行为，拆分链接后处理时需保住它 |

在同一目录随后执行五轮无跟踪的热构建对照。显式重复 configure 的中位数为 2.932 秒，随后构建的中位数为 0.157 秒；直接增量构建的中位数为 0.156 秒。十次构建均报告无需执行任务。此结果量化了这个样本中包装脚本多请求一次配置的成本。

相邻已验证基线的完整本地门禁耗时 30 分 04 秒，其中辅助工具约 28 分 45 秒。此处的 F407 构建只针对一个固件，两者的目标集合不同。日常驱动反馈 3 分钟、板卡反馈 5 分钟属于验证流水线的 P95 目标。配置优化的收益按 configure 时间、重配次数、编译及链接边数分别报告。[计时范围、工具分解及验收条件](build-system.md#verification-plan-for-additional-mcu-families)

## 当前架构与修改边界

**控制器选择。** resolver 从同一类型化 IR 生成 `selection.cmake`。其中 `NEXUS_SELECTED_CONTROLLERS` 按类别、物理控制器、模式编码；CMake 只索引一次，驱动按类别或指定控制器查询。空选择产生空列表。家族 CMake 决定编入哪些源码，`resolved.json` 供审查和产物校验。启明 F407 对照中，ControllerModes 的 JSON 读取从 35 次降为 0，44 条编译边和 ELF、BIN、map、资源报告逐字节一致。三次无跟踪热配置的中位数从 3.332 秒降至 2.723 秒。[生成投影](../../tools/configure/emission.py) · [模式查询](../../cmake/platform/ControllerModes.cmake) · [F4 构图入口](../../soc/stm32/stm32f4/CMakeLists.txt)

**平台与固件目标。** `Firmware.cmake` 集中定义 5 个必需属性和 6 个可选属性，平台发布端与固件消费端共用同一份清单。物理 provider 另检查启动输入、源文件和包含目录。外部应用通过 `nexus_add_firmware` 使用公共目标；厂商头和启动宏仅在需要的目标上生效。[固件属性合同](../../cmake/platform/Firmware.cmake) · [平台发布端](../../CMakeLists.txt) · [provider 校验](../../cmake/platform/Provider.cmake)

**增量与预算。** CMake 跟踪板卡、器件事实和解析器输入；生成内容相同的文件保留时间戳。Ninja 用 depfile 确定受 C 或头文件修改影响的对象。对象或链接脚本变化会重新链接。预算或检查器变化时先复查已有 ELF；通过时复用镜像，失败时由实际链接和链接后预算检查给出最终结果。直接构建同一固件目标也会重建缺失的 BIN、map 或资源报告。[固件图](../../cmake/platform/Firmware.cmake) · [已有 ELF 复查](../../cmake/platform/RecheckFirmware.cmake) · [挂钩入口](../../scripts/ci/tdd_gate.py)

**新增 MCU 或板卡。** 同一家族复用同一 IP 时，先在包中登记精确型号、封装、内存、引脚路由以及厂商头和启动文件，再用板卡 assembly 解析、交叉链接并从搬迁的 SDK 构建外部应用。STM32F4 的器件目录提供精确厂商路径；家族 CMake 和 SDK 校验器消费包声明。只换 PCB 接线时修改 Board；新 IP 由家族补驱动与 binding，新架构或 ABI 再扩展 Arch 或工具链。[接入步骤](../guides/provider-porting.md) · [F4 包入口](../../soc/stm32/stm32f4/package.json) · [精确器件目录](../../soc/stm32/stm32f4/facts/devices.json) · [SDK 打包](../../cmake/package/package_source_sdk.py)

**根入口与组件。** 根 CMake 装配平台层级与 provider，按开关加入工作负载和测试。Workload 模块持有工作负载接线，组件叶目录持有目标和依赖，外部应用仍可显式链接公共组件。实测最小配置中加载组件目录三次耗时 14–17 毫秒，本轮保留直接加载。每个固件单独拥有启动对象、链接和预算验收，以支持同目录多个固件目标。[组件装配](../../cmake/platform/Components.cmake) · [组件入口](../../components/CMakeLists.txt) · [工作负载](../../cmake/platform/Workload.cmake)

下表按维护职责定位代码。应用开发者主要使用 preset、TOML 和固件函数，移植者按变化所属层修改。

| 目录 | 维护职责 |
| --- | --- |
| `CMakeLists.txt` | 入口、装配顺序、公共目标 |
| `cmake/presets/`、`cmake/toolchains/` | 开发与验证入口；交叉编译环境与工具发现 |
| `cmake/platform/`、`cmake/package/` | 配置接线、目标策略、固件和 Runtime；Source SDK 打包、导入与消费验证 |
| `tools/configure/`、`boards/` | 通用解析、校验和生成；板级事实与接线 |
| `soc/<vendor>/<family>/` | 家族事实、system、drivers、sdk 与局部 CMake |
| `core/`、`arch/`、`io/`、`os/`、`components/` | 各自拥有源码和目标依赖 |
| `tests/` | 主机验证、模型与测试图 |

## 可借鉴的实现

- **Zephyr** 将配置阶段生成的文件作为编译输入，并让构建执行器运行已生成的依赖图。Nexus 可沿用这个阶段边界。现有 TOML、类型化 IR 和静态 factory 已满足当前单镜像用例；多镜像拥有独立配置与链接边界时，再评估 sysbuild 式上层编排。[Zephyr v4.2.0 构建阶段](https://github.com/zephyrproject-rtos/zephyr/blob/v4.2.0/doc/build/cmake/index.rst#L50-L99) · [Sysbuild](https://github.com/zephyrproject-rtos/zephyr/blob/v4.2.0/doc/build/sysbuild/index.rst#L32-L75)
- **ESP-IDF** 让组件区分公开和私有依赖，能从消费端判断哪些头及编译要求应传递。Nexus 可直接用 CMake 目标表达同一原则，把厂商 includes、HAL 定义与启动参数留在 provider 内。验证用最小外部应用，直接检查其编译命令和包含可见性。[ESP-IDF v5.4.2 组件依赖](https://github.com/espressif/esp-idf/blob/v5.4.2/docs/en/api-guides/build-system.rst#L448-L474)
- **CMake 与 Ninja** 分别提供工具链边界、重新配置依赖、输出与副产物规则，以及目标增量执行。Nexus 的脚本负责准备环境、选择测试、报告结果。热路径优先调用已配置的目标，保持原生命令和应用 API 相同的产物保证。[CMake 3.31 工具链](https://cmake.org/cmake/help/v3.31/manual/cmake-toolchains.7.html#cross-compiling) · [重新配置依赖](https://cmake.org/cmake/help/v3.31/prop_dir/CMAKE_CONFIGURE_DEPENDS.html) · [产物声明](https://cmake.org/cmake/help/v3.31/command/add_custom_command.html) · [Ninja 1.12.1 依赖](https://github.com/ninja-build/ninja/blob/v1.12.1/doc/manual.asciidoc#L638-L659)

## 原计划的工作包与验收条件

1. **BR1，基线与回归样本。** 构建维护者先冻结一组小样本，包括 F407 裸机、同板 RTOS、Native、独立 CPU Runtime 和搬迁后的 Source SDK。记录 revision、工具版本、并行度及冷热条件，分别测 configure、首次构建、无修改、单 C 文件和只改 TOML。沿用 B01 的报告；以 Ninja 实际执行、map、Flash/RAM 和产物身份建立比较基准。
2. **BR2，收敛根入口与目标合同。** 在 BR1 后提取工作负载和测试装配，集中生产与消费端共同使用的属性；保留原生 preset、目标别名和应用函数。用真实消费者覆盖缺 startup、缺必需目标、应用请求 C++、Native 及 Runtime。编译器和工具按启用语言或实际度量需求查找。退出条件是等价配置的 ABI、向量、段布局和资源预算一致，应用编译命令不暴露厂商私有目录。
3. **BR3，消除构图时重复解析。** 可与 BR2 并行。配置维护者从 UART、SPI 开始建立最小选择投影，迁移查询，最后清除失去消费者的旧助手。验收空配置、多控制器、重复模式、特殊字符和无关模式裁剪。完成后本样本在 ControllerModes 中读取 resolved.json 的次数从 35 降到 0，直接比较实际选中对象、符号、生成文件时间戳及 configure 耗时。配合 B05 做缓存边界验证。
4. **BR4，恢复原生增量路径。** BR1 后先修复总是 configure 的调用方，再处理固件链接和预算的失效边界。变更工具链、生成器、源码或 SDK 路径须能识别或要求新的构建目录。验收无修改零编译零链接、单 C 文件只重编对应对象及受影响链接、删除 BIN 可直接恢复、降低预算时原命令失败、失败后重试成功。按 B03 分别测平台和外部示例。
5. **BR5，去掉通用层内的器件假设。** BR3 后由 SoC 与 SDK 维护者把设备头、启动文件和家族特有选择归属到已有包；保留供应商锁与许可证。先用第二个已支持 F4 精确料号，再用外置 provider 走同一入口。删除该型号专属启动文件必须失败；只加同 IP 型号时通用 resolver、根 CMake 和 SDK 校验器的型号分支改动数为零。对应 B08、B09；模型与软件交付不授予实板资格。
6. **BR6，缩减可选成本。** 依 BR1 数据决定是否更改组件加载或复用启动对象。目前启明 V4.1 的裸机和 FreeRTOS 样本中，同配置双固件各自编译同一组四个启动对象。仅在生效的编译选项、目录属性、ABI 和 assembly 全部相同时考虑在该构建目录共享。每个 ELF 仍各自链接、校验向量及预算；若实测收益不足以覆盖复杂度，保留现有实现并记录结果。
7. **BR7，外部消费和迁移收尾。** BR2–BR5 的等价对照通过后，按 B07、B10、B11 执行相关 SDK 消费者、受限并发和集成对照。保留源码接入、重复导入 SDK 时的完整性检查、干净搬迁和正式离线复现。文档给出首次创建、热重建和移植新型号的最短操作。各批独立提交；发现漏编、漏检或增量误判即可回退对应批次并复用基线样本重现。

日常以无修改构建、普通驱动修改、板卡修改三条路径评估开发体验；交付以可直接构建的固件目标、独立应用、SDK 和复现验证评估正确性。同一工具链下，等价配置出现 Flash、RAM 或符号变化应逐项解释。只有重复实测的 P95、命令与产物都满足对应边界，才能把性能目标登记为已达成。

## 可独立验收的任务清单

以下 28 项记录原计划的顺序、依赖和退出条件，可用于复核交付。基线测量与上文的当前实现分别记录；任务中的将来时和基线数字保留立项时的含义。时间目标需由对应场景的重复样本验收。

### BR1 建立可比较的起点

- **BR1.1 固定输入与环境。** 在已有计时报告中登记 revision、主机、CMake、Ninja、编译器、并发、缓存和 preset。将启明 F407 裸机、同板 FreeRTOS、Native、CPU Runtime、搬迁 SDK 登记为五个代表场景，每项实验注明适用场景。
- **BR1.2 分开计算工作与等待。** 同一固件分别执行首次配置、首次构建、无修改、单个私有 C 变更、TOML 变更、删除 BIN。保留命令退出码、墙钟时间、编译和链接边、ELF/map 与预算结果。trace 用于数查询次数，耗时用关闭 trace 的独立样本。
- **BR1.3 固定门禁比较集。** 复用 B01 的 50 次提交回放，分别统计 testing、configure、ci、workflows 的选中模块及原因。真实驱动和单板变更各积累至少 30 个暖态样本，报告样本数、中位数、P95（升序第 ceil(0.95 × n) 项）、最大值、扩大检查范围的频率和 CPU 成本。保留失败与主机争用记录。

### BR2 简化生产入口

- **BR2.1 整理根装配顺序。** 在 BR1 后，把根 CMake 中可选的工作负载、测量与测试接线移入已有 Workload 或测试入口。分别用平台自身和外部应用配置，确认原有目标名、preset 和显式链接组件的用法仍有效。
- **BR2.2 统一平台属性合同。** 对照根入口发布的 11 个属性与 Firmware 消费端，集中维护属性名称和必需条件。分别验证 Native、物理固件和 Runtime。物理 provider 缺启动输入时应给出可定位的配置错误，外部应用编译命令中不应出现厂商私有头目录。
- **BR2.3 按语言和工作负载发现工具。** 检查 ARM 工具链的 g++ 与 size 查找条件。用纯 C/ASM 固件、实际链接的 C++ 外部应用以及资源度量命令各跑一次。缺少选中功能所需工具时，诊断应指明工具与触发入口。

### BR3 一次交付控制器选择

- **BR3.1 明确投影字段。** 从现有查询调用者反推其消费的类别、实例与模式，区分按类别和按控制器查询，记录空集语义。选择结果由 IR 派生，驱动源码列表与目标依赖由所属 CMake 声明。
- **BR3.2 迁移 UART 与 SPI。** 在已有 emission 和 selection 文件中输出试点字段，覆盖两个同类控制器选择不同模式、重复模式去重、空配置，以及含分隔字符的输入被安全编码或拒绝。原子发布失败后不得留下混合配置。
- **BR3.3 迁移其余调用者。** 试点的选编对象、定义和链接结果等价后，再逐组切换 F4 驱动。移除查询助手前搜索全部消费者，确认正常固件配置中 ControllerModes 对 resolved.json 的读取数由 35 降至 0。
- **BR3.4 检查等价性和实际收益。** 同工具链、同选项比较实际目标闭包、符号、向量、Flash/RAM 与无跟踪配置耗时。相同输入再次生成时输出时间戳稳定；受影响模式变化时，新增和移除的编译单元与预期一致。

### BR4 修复热构建和失效边界

- **BR4.1 修复调用方。** 先改 tdd_gate 的无条件 configure，再同步外部示例仓库的构建入口。首次构建创建所需目录；同源码、preset、工具链、配置参数与 SDK 身份的热构建直接执行既有目标。源码或 SDK 路径切换、配置中断、并发进入同一目录分别有回归。
- **BR4.2 验证配置与编译失效。** 用同一入口依次修改普通 C、私有头、板级事实和 TOML，再新增或删除被选事实。普通 C 只编译受影响对象，事实变更重新求解，无效输入失败。比较文件内容相同而工具实现有变时的重配、重编和重链接次数。
- **BR4.3 拆清链接与预算依赖。** 在 Firmware 中列清对象、链接脚本、预算、检查器和报告各自的依赖。只收紧预算至镜像占用以下时，直接构建该固件必须返回失败，旧通过记录失效；恢复合法预算后同一命令成功。链接脚本内容实质变化应重新链接。
- **BR4.4 逐项恢复交付产物。** 在独立树中分别删除已声明的 BIN、map 和资源报告，逐次直接构建原固件目标，检查恢复内容与预算状态。允许为重建 map 重新链接。无修改场景始终要求零编译、零链接。

### BR5 将 MCU 差异留在所属包

- **BR5.1 迁移家族选择。** 在 BR3 投影稳定后，清点公共生成器中的 STM32 器件、startup 和特定模式假设。把已有语义对应的数据放入选中家族或 binding，家族私有目标消费这些数据。使用现有 F4 场景核对宏、HAL 源码、启动向量、内存布局与 ABI。
- **BR5.2 迁移 SDK 必备文件规则。** 由选中包声明精确头文件、启动源、供应商依赖和许可，通用打包器验证完整性。用搬迁 SDK 链接另一精确 F4 料号，再删去其专属启动源，要求校验失败。未选厂商依赖不应进入导出包。
- **BR5.3 验证两个接入层次。** 单改 PCB 引脚时只动板级事实和 assembly，验证合法接线和冲突；同家族同 IP 料号检验封装引脚、内存、路由、精确器件头和向量。利用两块 F407 及既有 F411、F446、F479 软件参考，要求公共 resolver、根 CMake 和打包器新增型号分支数为零。
- **BR5.4 验证包隔离与新家族准入。** 先用外置 provider 测包标识冲突、搬迁与私有依赖隔离。未来真实新家族或厂商接入时，以启动、时钟、GPIO、UART、精确工具链链接和独立 SDK 应用贯通首条路径。另增无关家族后，既有 F4 私有改动选中的 ARM 目标集合应保持一致。

### BR6 仅实施收益明确的可选优化

- **BR6.1 度量组件加载。** 记录最小应用中组件目录的配置时间与实际消费目标。只在收益可重复且外部应用仍能显式链接公共组件时才延迟加载。公共组件名、目标映射和 SDK 准备入口分别只有一个维护位置。
- **BR6.2 评估同配置多固件的启动对象。** 比较双固件的源、编译定义、ABI、目录属性和实际命令。条件相同时试验共享四个启动对象，每个 ELF 独立链接和检查。若无可辨别收益或向量、段布局变化，记录结论并维持原实现。

### BR7 精简四个工具域并验收交付

- **BR7.1 实现按变更选检查。** BR1 后即可在现有 run_tool_tests 和 change_paths 中复用变更前后依赖集合的并集，先支持私有驱动、板卡及 assembly。测试共享头、重命名、删除和缺失 depfile。依赖不明确时扩大到家族、工具域或完整验证，并在报告中给出原因。
- **BR7.2 压缩 testing 和 configure 的重复工作。** 同批不可变 fixture 按内容、根路径与包身份复用；合并相同 EXTI、SPI factory 或 SDK 选择准备。每删除一项测试，记录原故障、保留测试和触发该测试的变更，再注入一次故障证明仍能捕获。保留共享资源冲突和配置发布中断用例。
- **BR7.3 压缩 ci 和 workflows 的重复工作。** 工作流改动运行相关真实入口与策略校验；普通板卡提交按 B04 检查使用它的 assembly。完整能力扫描保留一份在集成层。对比四个工具域的模块选择、CPU 时间和端到端等待，避免同一检查被多个包装层重复执行。
- **BR7.4 选择 SDK 外部消费者。** verify_consumers 从现有计划按名称选择，未知名称立即失败。用 TMP117 变更验证 Native、F407 两条搬迁后的消费路径。公共 ABI、锁或导出策略变更扩大范围，正式交付保留全部已声明消费者和消费后完整性验证。
- **BR7.5 引入有界并发。** 工具 fixture 去重后，共享主机并发预算，串行修改同一构建目录，每次运行独立写结果。比较串行、并行的模块与用例集合，注入 worker 失败、超时和取消，要求真实退出状态与子进程清理正确。
- **BR7.6 对照完整门禁。** 在相同提交上对照新增的快速选择与完整集成，注入错误引脚、漏掉共享头依赖、错误 DMA 路由、过期 SDK 导出、错误启动向量。每种故障必须由适用快路径直接捕获或由自动扩大的检查捕获；漏检即恢复对应规则的较宽范围。
- **BR7.7 验收日常反馈时间。** 在 BR1.3 定义的代表样本上，以整条本地门禁确认普通驱动 P95 不超过 180 秒、单板变更含实际 ARM 链接 P95 不超过 300 秒。分别报告 cold、warm 与完整发布耗时。超预算时按阶段耗时定位后再决定下一项优化。
- **BR7.8 切换默认入口。** 当前 F4 样本与故障对照通过后，更新 AGENTS、开发流程、CI 条件以及示例用法。复验源码消费、搬迁 SDK、正式离线复现。按原有结果格式声明实际执行范围，保留旧规则回退路径至首批对照完成；新家族的正式资格随该家族单独验收。
