---
id: design.build-architecture
type: reference
audience: maintainer
source_language: zh_CN
---

# 构建系统架构审查与重构方案

本方案供应用开发者、SoC 接入者和构建维护者审查。审查基线是 Nexus 提交 `5e0b512c`。建议保留 CMake 3.31、Ninja、静态实例和现有应用入口，依次改进配置数据交接、增量失效和新 MCU 接入。下文的 BR1–BR7 是待实施工作，B01–B11 已规定验证流程和时间目标。[现行构建合同与验证计划](build-system.md#verification-plan-for-additional-mcu-families)

## 从应用配置到可烧录镜像

1. **选择构建环境。** 应用 preset 在首次 configure 前选择 Ninja、工具链、独立 binary directory 和 `NEXUS_ASSEMBLY_FILE`。工具链负责定位编译器与交叉编译环境。配置解析后，平台把确切 CPU、FPU、浮点 ABI 和优化选项施加到相关目标。一个构建目录对应一套 assembly 和工具链，同一套配置可以创建多个固件目标。[工具链](../../cmake/toolchains/arm-gcc.cmake) · [目标编译选项](../../cmake/platform/Options.cmake)
2. **解析硬件意图。** 开发者编写 TOML，选择板卡、后端、组件和 I/O 实例。主机 resolver 结合 Board、SoC 事实及 binding 检查资源和合法模式，从同一份类型化结果原子生成静态 C、factory 头、链接脚本、CMake 选择数据与审查用 JSON。它还记录事实和工具输入，让 CMake 知道何时应重新配置。失败与输出内容相同各有既定的发布语义。[配置接线](../../cmake/platform/Configure.cmake) · [生成端](../../tools/configure/emission.py)
3. **建立目标图。** 根 CMake 接入 Core、Arch、I/O、OS、组件及选中的 SoC 包。包的 CMake 决定具体模式对应哪些 C 源码和厂商库。生成的 `bindings.c` 编入 `Nexus::Platform`，组件使用公共领域接口。固件入口负责将应用、平台、启动对象和链接脚本连成可执行文件。Ninja 按依赖执行编译、链接和产物检查。[根入口](../../CMakeLists.txt) · [provider 合同](../../cmake/platform/Provider.cmake) · [固件目标](../../cmake/platform/Firmware.cmake)
4. **交付与消费。** 外部工程通过源码或已安装的 Source SDK 取得同一套公共目标。SDK 还要验证内容身份、搬迁后的路径、编译接口、链接和资源。CPU Runtime 入口供调用方自行负责启动与板级集成。其产物与物理板卡固件有不同的责任边界。[SDK 入口](../../cmake/package/NexusConfig.cmake.in) · [SDK 教程](../tutorials/source-sdk.md) · [CPU Runtime](../../cmake/platform/Runtime.cmake)

![配置、静态实例与固件产物的关系](../assets/diagrams/configuration-generation.svg)

应用只需提供 assembly 与应用源文件。以下 Source SDK 示例假定 preset 已设置工具链、`NEXUS_ASSEMBLY_FILE` 和 SDK 的 `CMAKE_PREFIX_PATH` 或 `Nexus_DIR`，应用也已调用 `project()`。源码消费通过 `add_subdirectory` 接入平台，随后调用相同的固件函数。

```cmake
find_package(Nexus CONFIG REQUIRED)
nexus_add_firmware(app SOURCES src/main.c)
```

## 实测成本与优化顺序

在独立构建目录运行 `stm32-qiming-baremetal` preset，以两个并行任务构建 `nexus_firmware`。首次 configure 针对 ControllerModes 开启了 CMake trace。下表是同一工作区的一次观察，包含主机缓存和跟踪开销。时间用于定位方向，后续产品验收应使用相同环境、无跟踪的重复样本。

| 观察项 | 实测 | 工程含义 |
| --- | --- | --- |
| 首次 configure | 4.144 秒，同一模块读取 resolved.json 35 次 | 消除各 IP 模块重复解析同一选择结果，收益须以对照组复测 |
| 首次编译与链接 | 3.278 秒，固件闭包 44 条编译命令 | 全目录编译数据库有 149 条记录，单固件的实际工作应查 Ninja 目标闭包 |
| 原输入直接再次构建 | 0.224 秒，零编译、零链接 | 保留 Ninja 热路径，优先处理总是先 configure 的包装入口 |
| 仅删除临时树中的 BIN | 0.938 秒；直接构建固件恢复 BIN，ELF 与 BIN 各自与原件字节相同 | 产物重建与预算检查是用户依赖的行为，拆分链接后处理时需保住它 |

在同一目录随后执行五轮无跟踪的热构建对照。显式重复 configure 的中位数为 2.932 秒，随后构建的中位数为 0.157 秒；直接增量构建的中位数为 0.156 秒。十次构建均报告无需执行任务。此结果量化了这个样本中包装脚本多请求一次配置的成本。

相邻已验证基线的完整本地门禁耗时 30 分 04 秒，其中辅助工具约 28 分 45 秒。此处的 F407 构建只针对一个固件，两者的目标集合不同。日常驱动反馈 3 分钟、板卡反馈 5 分钟属于验证流水线的 P95 目标。配置优化的收益按 configure 时间、重配次数、编译及链接边数分别报告。[计时范围、工具分解及验收条件](build-system.md#verification-plan-for-additional-mcu-families)

## 目标架构与修改边界

**选择事实只传一次。** 当前 F4 CMake 包含多个 IP 片段，`nexus_controller_values` 每次调用均读取 JSON 并遍历控制器。建议把构图实际消费的类别、控制器和模式按需投影进已有 `selection.cmake`，从同一 IR 一次生成。用经校验的标识、稳定顺序、空集和正确的 CMake 列表转义覆盖用户输入。CMake 继续拥有源码、目标、可见性与链接边。审查工具继续使用 `resolved.json`。先迁移 UART 与 SPI，核对编译命令和镜像，再迁移余下调用者。[现有查询](../../cmake/platform/ControllerModes.cmake) · [F4 构图入口](../../soc/stm32/stm32f4/CMakeLists.txt)

**让构建合同可见。** 根入口当前将 11 个环境和启动属性发布给固件函数，消费端再分别读取并检查其中 5 个必需属性。应在现有模块中集中定义这项内部合同，区分通用必需项、物理目标必需项及可选项。API、编译定义与头目录按实际消费者使用 PUBLIC、PRIVATE 或 INTERFACE。厂商头和启动宏只进入需要它们的目标。验证直接属性之外，还应实际编译外部消费者，以检查经依赖传递的使用要求。[目标发布端](../../CMakeLists.txt) · [固件消费端](../../cmake/platform/Firmware.cmake)

**把失效范围交给真正的输入。** Board、芯片事实、binding 或解析器变化应重新求解；生成内容相同则保持文件时间戳。普通 C 变更使用编译器 depfile。链接脚本或实际对象变化触发链接。预算与检查器变化必须重新验证产物。当前固件把配置实现文件也列为链接依赖，直接删去依赖会让部分检查丢失。应先用产物缺失、预算缩紧、解析器改动但生成内容相同、链接脚本实际变化四种情形验证，再缩小依赖。工作区与外部示例的热构建均应保留路径切换、失败恢复及并发隔离。[固件依赖和产物](../../cmake/platform/Firmware.cmake) · [当前门禁入口](../../scripts/ci/tdd_gate.py)

**让新增 MCU 的修改留在所属包。** 同家族同 IP 的新型号通常增加型号、封装、内存、路由、启动与 SDK 身份；复用家族 CMake 和驱动。新 IP 在所属家族增加驱动、binding、局部选编和合同测试。新厂商包提供自己的 facts、provider 目标、startup 和 SDK 清单。新架构或 ABI 才涉及 Arch 或工具链扩展。当前生成器存在 STM32 专用选项，SDK 的必备文件检查还直接列出 F407 头及启动文件。把这些假设移入选中包的元数据，以另一款 F4 和外置 provider 分别检验同家族与跨包接入成本。[生成器](../../tools/configure/emission.py) · [SDK 打包与校验](../../cmake/package/package_source_sdk.py) · [provider 接入](../guides/provider-porting.md)

**简化入口和组件登记。** 根文件保留配置、层级、provider、平台和可选开发入口。工作负载、度量或测试接线移回已有的 Workload 或 tests 所属位置。组件公共标识属于配置合同，标识到目标的映射和依赖属于 CMake。标准 CMake 调用保留在所属叶目录。组件目录使用 EXCLUDE_FROM_ALL 仍会被配置，应先计时，再决定是否延迟加载，并验证手动链接公共组件的现有用法。[组件装配](../../cmake/platform/Components.cmake) · [组件入口](../../components/CMakeLists.txt) · [工作负载](../../cmake/platform/Workload.cmake)

以下是目标职责布局，绝大多数目录已存在。日常接入按修改的责任定位文件。应用作者只需学习 preset、TOML 与固件函数；平台维护者才需要进入后面的工具和模块。

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

## 迭代任务与验收

1. **BR1，基线与回归样本。** 构建维护者先冻结一组小样本，包括 F407 裸机、同板 RTOS、Native、独立 CPU Runtime 和搬迁后的 Source SDK。记录 revision、工具版本、并行度及冷热条件，分别测 configure、首次构建、无修改、单 C 文件和只改 TOML。沿用 B01 的报告；以 Ninja 实际执行、map、Flash/RAM 和产物身份建立比较基准。
2. **BR2，收敛根入口与目标合同。** 在 BR1 后提取工作负载和测试装配，集中生产与消费端共同使用的属性；保留原生 preset、目标别名和应用函数。用真实消费者覆盖缺 startup、缺必需目标、应用请求 C++、Native 及 Runtime。编译器和工具按启用语言或实际度量需求查找。退出条件是等价配置的 ABI、向量、段布局和资源预算一致，应用编译命令不暴露厂商私有目录。
3. **BR3，消除构图时重复解析。** 可与 BR2 并行。配置维护者从 UART、SPI 开始建立最小选择投影，迁移查询，最后清除失去消费者的旧助手。验收空配置、多控制器、重复模式、特殊字符和无关模式裁剪。完成后本样本在 ControllerModes 中读取 resolved.json 的次数从 35 降到 0，直接比较实际选中对象、符号、生成文件时间戳及 configure 耗时。配合 B05 做缓存边界验证。
4. **BR4，恢复原生增量路径。** BR1 后先修复总是 configure 的调用方，再处理固件链接和预算的失效边界。变更工具链、生成器、源码或 SDK 路径须能识别或要求新的构建目录。验收无修改零编译零链接、单 C 文件只重编对应对象及受影响链接、删除 BIN 可直接恢复、降低预算时原命令失败、失败后重试成功。按 B03 分别测平台和外部示例。
5. **BR5，去掉通用层内的器件假设。** BR3 后由 SoC 与 SDK 维护者把设备头、启动文件和家族特有选择归属到已有包；保留供应商锁与许可证。先用第二个已支持 F4 精确料号，再用外置 provider 走同一入口。删除该型号专属启动文件必须失败；只加同 IP 型号时通用 resolver、根 CMake 和 SDK 校验器的型号分支改动数为零。对应 B08、B09；模型与软件交付不授予实板资格。
6. **BR6，缩减可选成本。** 依 BR1 数据决定是否更改组件加载或复用启动对象。目前启明 V4.1 的裸机和 FreeRTOS 样本中，同配置双固件各自编译同一组四个启动对象。仅在生效的编译选项、目录属性、ABI 和 assembly 全部相同时考虑在该构建目录共享。每个 ELF 仍各自链接、校验向量及预算；若实测收益不足以覆盖复杂度，保留现有实现并记录结果。
7. **BR7，外部消费和迁移收尾。** BR2–BR5 的等价对照通过后，按 B07、B10、B11 执行相关 SDK 消费者、受限并发和集成对照。保留源码接入、重复导入 SDK 时的完整性检查、干净搬迁和正式离线复现。文档给出首次创建、热重建和移植新型号的最短操作。各批独立提交；发现漏编、漏检或增量误判即可回退对应批次并复用基线样本重现。

日常以无修改构建、普通驱动修改、板卡修改三条路径评估开发体验；交付以可直接构建的固件目标、独立应用、SDK 和复现验证评估正确性。同一工具链下，等价配置出现 Flash、RAM 或符号变化应逐项解释。只有重复实测的 P95、命令与产物都满足对应边界，才能把性能目标登记为已达成。
