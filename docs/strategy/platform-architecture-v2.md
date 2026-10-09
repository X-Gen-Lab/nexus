# 平台架构与构建实现设计

日期：2026-10-09。范围：Nexus 通用平台及外部消费者；当前源码实现与定向检查详见 [38 项执行表](../implementation/refactor-execution.csv)。原 `affaa86f` 审查中 HAL/OSAL 混合、Product 身份、固定保留 Flash 和隐式 main 均为历史背景，当前不再作为消费接口。

## 1. 一份配置与显式目标图

```text
external application target
  -> Nexus::Firmware
       -> platform_<selected> + explicit platform OBJECTS/startup
       -> Nexus::Runtime
            -> HALCore + HALSupport + OSAL
       -> HALDevice facades
  -> explicitly selected component cores/adapters

HALCore -> configuration/standard types + private Arch/provider contract
HALGPIO / HALUART / HALSPI / HALI2C / HALFlash -> HALCore
HALSupport -> HALCore + OSAL + Arch
HALRuntime -> HALUART + Arch
HALRuntimeOSAL -> HALRuntime + OSAL
HALCompletion -> configuration + Arch; caller-owned queue, no OSAL worker
controller / SoC / Board objects -> narrow Nexus contracts + private SDK
```

Firmware 的 typedfacade 集合是便利装配；只需要一个 GPIO core 的消费者可直接选 HALGPIO，实际最小 link 检查证明不拉 OSAL。HAL aggregate 仅提供显式兼容性装配，不应成为新 provider 公开 SDK 的捷径。一个 buildroot 内只有一个 Nexus 配置，多个平台/后端需要独立 configure。

| 输入 | 拥有者 | 生效规则 |
|---|---|---|
| CMake preset | 平台维护者/外部工程 | generator、compiler/buildtype、明确 config 路径 |
| `NEXUS_CONFIG_FILE` | 消费方 | Kconfig 软件选择与 generic 资源；非法依赖/choice/range 拒绝 |
| `NEXUS_BOARD_DIR` | 消费方 | 一个 externalBoardpackage；与 BOARD_EXTERNAL 一致 |
| `NEXUS_FLASH_LAYOUT_FILE` | 消费方 | 一个 image/region 输入；默认 wholeFlashimage/no regions |
| `NEXUS_BUILD_TESTS` | 开发工程 | top-levelNative 默认 ON，源码子工程 OFF；与有效 BUILD_TESTS 一致 |
| `NEXUS_BUILD_CONTRACTS` | 开发工程 | top-level 默认 ON，ARM 建独立 linkfixture，源码消费者 OFF |
| expected source revision | 外部仓库/交付方 | 固定真实 SDKsourceSHA，拒绝混入另一个版本 |

根 CMake 只解析并装配选择模块，不列 businesssource。optionalcomponent 在有效 Kconfig 关闭时没有 target，不隐式 findOpenSSL 或拉 Storage。ConfigCore 会使用 Securitycorefacade，但 core 没有 OpenSSL 默认 provider。

## 2. generated bundle

`generated/effective.config`、`nexus_config.h`与`config.cmake`来自同 Kconfig 解析，generated 目录属于 Nexus 自己的 binarycontext，不借父`PROJECT_SOURCE_DIR`或源根.config。Boardmodule 随后根据 resolvedconfig 验证 package，emit`board-identity.json`和`board.cmake`。

Flash layout 同 parse 产生`layout.json`、`nx_flash_layout.h`、`firmware.ld`；linker 包含真实所选 SoCsections，与 MSP/libcheap 预算一起生效。hash、config、inputs 更新导致重新 configure，生成失败阻止旧 bundle 继续编译。Native 没有物理 Flashimage；metadata 查询不伪造 MCUmemory。

重配清理旧 CONFIGcache，默认不写父工程输出设置。Applicationhelper 从`Nexus::Config`读取 platform/source/build/configcontext，EXTRA_DEPS 由外部应用选择。每个 application 输出到 SDKcontext 的 bin，父 sentinel 保持其原输出。C/C++consumer 和含空格外部 Board 都已有真实检查。

## 3. Runtime 与业务入口

公开头为`runtime/nx_runtime.h`、`runtime/nx_platform_info.h`。Runtime 不创建业务 worker、scheduler 或 component，不保留 productname、applicationchoice 或自动 mainwrapper。

```c
nx_boot_report_t report;
if (nx_runtime_bootstrap(&report) != NX_OK) {
    /* Caller examines original status, rollback status and owned state. */
}
const nx_platform_info_t *info = nx_platform_get_info();
```

启动 HAL→OSAL；失败 rollback 保留原错误和 cleanuperror，state 为 OFFLINE/PARTIAL/READY，PARTIAL 必须先 shutdown 结清。所有生命周期由 caller 串行，ISR 明确拒绝。READY 只说 ownedinfra 成功；应用健康、taskcreation/启动失败和安全输出是消费方责任。

FreeRTOS 外部 main 先 bootstrap、创建启动 worker 并检查返回，再调用`osal_start()`；需要 blocking 组件在 scheduler 运行的 worker 中初始化。baremetal 外部 main 拥有 poll/pump。Shutdown 先要求 leases/objects 结清；runningMCUkernel 返回 BUSY，完整热重启未实现。metadata 只含 mainSRAM/Flash 等当前已实现字段，不宣称所有 memorydomain/DMA 属性。

## 4. Board 单路径及 private 实现

Boardmanifest schema1 至少提供 id、soc、hse_hz、interface_target、object_targets、inputs 与 resources。`NEXUS_BOARD_DIR`只输入一个包含 manifest/CMake/source 的 package；sourcepath 不得越界/symlink，编译 Boardobject 的声明文件必须纳入 inputs 并 hashbind。

active 资源 kind 是 GPIO/UART/SPI；`when`对应有效 Kconfig。reviewedcontroller 绑定验证 clock、IRQ、priority、pin/AF 和 selectedDMA，重复 pin/controller/IRQ/DMA 拒绝。UART 例如 STMUSART1/2、GD USART0，SPI 例如 STMSPI1 与 GDSPI4；其余 route 未经实现复核不会自动接受。

FreeRTOS 目前 syscall-safe logicalpriority 下限为 5，调用内核的 controllerIRQ 不能填 0–4。Board 不直接操纵 controller mutableinstance，使用只读 Nexus 资源与 boundedCS/安全初值接口；控制器承担时钟/IRQ/DMA 打开停止与真实 error/settlement。完整自动 pinctrl/topology 求解、所有 AF 组合、外部器件上电 policy 与实物 qualification 不在 manifestschema 能力中。

## 5. Flash 是芯片能力，partition 是外部政策

STM32`FLASH0`按 actualdensity 暴露 8/12sector，GD32`FLASH0`暴露 256×4KiBpage；provider 不依赖 Storage。typedregion 创建权限/owner/generationlease，physicalblock 查询返回完整 block，不把跨 sectoroperation 当 uniformpage。越界/溢出、可写重叠、erase 非整 block 拒绝。

外部 layout schema1 选择 soc、image 的 offset/size 与 namedregions。offset 为相对 physicalFlashbase；当前 imageoffset 必须 0，size 不得侵占 region。默认 image 整 Flash、regions 为空。root 生成`NX_LAYOUT_REGION_LIST(X)`供外部 caller 按 name/offset/size/permissions 明确选 region，平台不自动打开 storage。

ELF 强绑定`__nexus_image_start/end`、regionbounds 及 layout/Board SHA 各 8word；checker 从真实 segment/vector/entry/sections 和物理几何验证，不仅比较 JSON。package/HIL 也重建 Board/layoutbundle 并核对 BIN 与 ELFbytes。非零 imageoffset 需要真实 VTOR/startup/bootloader 和产品安装恢复链，当前拒绝。

`Nexus::StorageHAL`caller-ownedcontext 通过 generation-safeborrow 锁住已经开的 region；oldporttoken 不重导新 bindcontext，inflight/unbind 冲突 BUSY。adapter 只接受 uniformcompleteeraseblocks、0xff 和合适 programgeometry，共享整次 open/load/savebudget；unlock/lock、partition 与 maintenancewindow 由外部 caller 拥有。

## 6. Component targets

| 公开 target | 实际实现边界 |
|---|---|
| `Nexus::LogCore` | formatter，无 HAL/OSAL/clock/heap；拥有必要 buildoptions 以匹配 ARMABI |
| `Nexus::LogRuntime` / `Nexus::Log` | selected OSAL logger；callback 不持 globalmetadata 锁等 UART |
| `Nexus::LogUART` | typedUART、caller-ownedcontext 与有界 TXstorage；flush 等待 wirecompletion，超时保留 lease |
| `Nexus::Shell` / `Nexus::ShellUART` | core 与 typedUARTadapter 分离；mock 只 tests；失败 teardown 保留 owner 可 retry |
| `Nexus::ConfigCore` / RAM / Flash | neutralbackend 与 cryptocontract；RAMvolatile，Flash 显式链接 Storage |
| `Nexus::Storage` / `Nexus::StorageHAL` | dual-bank 快照 core / typedregionadapter，不默认 partition |
| `Nexus::SecurityCore` / `Nexus::CryptoOpenSSL` | providerNULLcore / explicitNativeOpenSSLprovider |
| `Nexus::ModbusRTU` / `Nexus::Update` | protocol/statecore，通过 callerports 连接 wire/trust/install/healthpolicy |

选 OpenSSLtarget 还需要 effective`CRYPTO_PROVIDER_OPENSSL=y`且 Native，然后应用`nx_crypto_set_provider(nx_crypto_openssl_provider())`显式绑定。仅 linkcore 不运行 provider，也不隐式 findOpenSSL。MCU 没有 provider 时 unsupported；不写自制 crypto。

## 7. 官方入口与外部消费

平台权威入口是 CMakePresets/CMake/CTest；Python 只编排。

```sh
python3 scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
python3 scripts/ci/ci_build.py --preset stm32-qiming-armgcc-freertos-release --stage build --jobs 4
python3 scripts/ci/validate_firmware_elf.py \
  --build-dir build/stm32-qiming-armgcc-freertos-release \
  --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json
```

ARMtop-level 建`nexus_contract_firmware`检查 startup/objects/metadata；独立测试不用 examples 源码。已有 fixedARMGNU14.3.rel1 和 SDK 依赖入口不自动任意下载/升级。Native-minimal/native-services 只是 compileprofiles，应用运行命令在 externalrepo。

外部 CMake 先选择工具链，再`project(C CXX ASM)`，设置明确 NEXUS_CONFIG_FILE 并 add_subdirectory 固定 SDK；最后`nexus_add_application(TARGET ... SOURCES ... EXTRA_DEPS ...)`。生产 caller 使用 publicNexusheaders/targets，SDKprivateheader 不得由平台 interface 传播。不同 Board/backend 各自 buildroot。

## 8. Relocatable installed source SDK

完整、干净、固定依赖 checkout 使用：

```sh
python3 -B cmake/package/package_source_sdk.py \
  --source /absolute/path/to/nexus --output '/absolute/path/to/nexus sdk prefix'
python3 -B '/absolute/path/to/nexus sdk prefix/share/nexus/src/cmake/package/package_source_sdk.py' \
  --verify '/absolute/path/to/nexus sdk prefix/share/nexus/src'
```

OUTPUT 必须在 source 之外且尚不存在；失败不留下半包。整个 prefix 移动后 relative`lib/cmake/Nexus`配置访问`share/nexus/src`，所有源码/依赖/许可和 exportSHA 都验证。sourceGitSHA 来自 manifest，不查 consumerancestorGit。

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_firmware LANGUAGES C CXX ASM)
find_package(Nexus 0.1.0 EXACT CONFIG REQUIRED)
nexus_add_application(TARGET firmware SOURCES main.c)
```

```sh
cmake -S /absolute/path/to/application -B /absolute/path/to/application-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  '-DNexus_DIR=/absolute/path/to/nexus sdk prefix/lib/cmake/Nexus' \
  '-DNEXUS_CONFIG_FILE=/absolute/path/to/application/platform.conf' \
  '-DNEXUS_EXPECTED_SOURCE_REVISION=REPLACE_WITH_40_HEX_SOURCE_COMMIT'
cmake --build /absolute/path/to/application-build --parallel 4
```

ARM 首次 configure 再传`CMAKE_TOOLCHAIN_FILE=<prefix>/share/nexus/src/cmake/toolchains/arm-gcc.cmake`和 NEXUS_PLATFORM；toolchain 由消费方按 lock 提供。包不带 developmenttests/GoogleTest，拒绝 NEXUS_BUILD_TESTS/CONTRACTS 开启。developmentfixture 需要`NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`，身份有 snapshotSHA 且 publishable=false，不能 promotion。

真实 relocatedfixture 已有 NativeC/C++运行、STM32C/C++真实 ELF 检查；strictcleanpack/consume 等待 finalsourcecommit。这是 sourceSDK，不是 binarySDK，也没有 crosscompiler/configABI 承诺。详细 license/provenance 参见[packageREADME](../../cmake/package/README.md)。

## 9. 验证和剩余能力

原完整基线`3129550`的 1781Native/8ARM15ELF 留在历史记录。本轮各模块真实 targeted 软件通过不会自动覆盖最终 cleanGCC/Clang/sanitizer/analysis、8ARM 和双仓 pin/在线结果。所有 candidate 绑定 source/config/Board/layout/toolchain/artifacts 与非零 actualtests，不能以 target/定义 workflow 计完成。

NativetypedI2C 支持双 device 和 deadline/cancel；MCUmodernI2C 未实现。Completionadapter 只派发 producer 明确 settledterminal，provider 是 bufferlease 权威。HIL 工装可做静态准入/租约/challenge，但 physicalexecutions、IRQ/DMA/powerloss/longload 完整 workload 和实测 budgets 仍未完成。本阶段用户暂不接实板；SDKpack、软件 matrix 和 HILready 继续推进，企业/LTS/安全/制造资格单独验收。
