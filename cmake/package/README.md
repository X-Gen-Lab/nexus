# Relocatable Nexus source SDK

Nexus 0.2.1 is the current Source SDK version. A version value does not establish
publication or software or hardware qualification. The consumer owns its assembly,
application sources, compiler, ABI, workers and resource budgets. Vendor headers
and concrete provider storage stay private. No precompiled MCU binary ABI or
application generator is exported.

`dependencies/platform-compatibility.json` is the sole authored semantic-version
policy. CMake, generated headers and the exported package derive their version
from it. Source interface epoch `1` and authored assembly schema `2` are separate
identities; they do not change solely because the semantic version changes.

Prepare from an initialized, clean, committed checkout:

```sh
python cmake/package/package_source_sdk.py --source . --output /tmp/nexus-sdk
```

The package records the source commit/tree, every packaged file hash, required
recursive dependency commits/trees, the reviewed GD32 import and license notices.
The installed CMake package verifies those bytes before configuring a consumer.
Modified, missing, extra or symlinked files fail verification. Preparation and
verification do not fetch missing dependencies.

A dirty checkout can produce an explicitly nonpublishable development fixture
with `--development-fixture`. Its consumers must deliberately set
`NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`. This does not qualify a release.

Move the complete prefix, then consume it from an independent project:

```cmake
cmake_minimum_required(VERSION 3.31)
project(my_device LANGUAGES C ASM)
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
find_package(Nexus 0.2.1 EXACT CONFIG REQUIRED)
nexus_add_firmware(my_device SOURCES main.c)
```

```sh
cmake -S . -B build -G Ninja \
  -DNexus_DIR=/relocated/nexus-sdk/lib/cmake/Nexus \
  -DCMAKE_TOOLCHAIN_FILE=/relocated/nexus-sdk/share/nexus/src/cmake/toolchains/arm-gcc.cmake
cmake --build build
```

An assembly chooses a reviewed Board package, exact part, clock, OS backend,
explicit controller modes, device endpoints and memory budgets. Relative Board
paths are relative to the assembly, including when it is outside the SDK.
`NEXUS_EXPECTED_SOURCE_REVISION` can enforce a consumer-approved exact revision.
Native models use a host compiler and omit the ARM toolchain argument.

`Nexus::Core`, `Nexus::Arch`, `Nexus::IO`, `Nexus::Platform` and optional component
or OS targets expose their own includes and dependencies. `nexus_add_firmware`
links explicit sources, selected components, startup, generated bindings and the
resolved linker input. It does not create `main`, tasks or a scheduler. Consumers
start hardware with `nx_platform_start`. Generated `nexus_factory.h` supplies
compact typed IDs and pure static getters such as
`nx_factory_gpio(NX_GPIO_ID_LED0)`. A getter neither initializes hardware nor
allocates memory. The returned readonly face dispatches through one shared
provider operations table to its own mutable context. Generated
`nx_binding_<id>` and `nx_device_<id>` symbols remain fixed typed aliases.
Backend and application startup
order remains explicit in consumer code.

Every MCU firmware produces ELF, BIN, map and a checked resource report. The ELF
contains the configuration digest and the real startup vector. The selected CPU
facts determine the instruction set, optional FPU, float ABI and enum ABI;
consumers inherit those flags through Nexus targets. C++ consumers use their declared embedded
runtime policy, such as `-fno-exceptions -fno-rtti` for the tested source SDK path.

The maintained boundary test is `tests/contracts/test_source_sdk.py`. It packages
the real checkout, relocates into a path with spaces, verifies byte tamper and
identity rejection, executes independent Native C/C++ consumers and links both
maintained MCU C/C++ consumers with actual startup/linker/resource gates.
Those software checks do not establish physical Board qualification.

Use the release asset inventory and actual execution receipts to identify the
SDK source, tool tuple, dependency/license closure and consumer results. The
declared minimum Python/CMake versions do not establish installation or
integration on every host. Linux is the maintained executed host scope; physical
MCU HIL remains `not_executed`. Historical software candidates keep their original
source identity. The [0.1.0 release scope](../../docs/status/release-0.1.0.md)
is historical and does not qualify this development version.

Release automation verifies an existing publishable installed prefix with the
same consumer template, without the development opt-in:

```sh
python cmake/package/verify_consumers.py \
  --prefix /relocated/nexus-sdk --output build/installed-sdk-check
```

`--parallel N` is the explicit total consumer job budget. When omitted, the
coordinator inherits `NEXUS_BUILD_JOBS`, then `CMAKE_BUILD_PARALLEL_LEVEL`, or
uses `2` if neither is set. An explicit option overrides this inherited default.
Concurrent independent invocations need an orchestrator allocation; this is not
a global CPU semaphore. Independent
consumers may configure, build and verify concurrently; each compiler build uses
one job. `--parallel 1` retains serial execution. Each consumer still performs
fresh installed-source verification. A failure cancels and joins all started
work before the final failed report is written.

`verification.json` and per-command logs retain actual arguments and exit codes.
This command fails for development fixtures, missing tools, any compile/run/link
failure, leaked private includes or failed ELF resource/configuration binding.
The SDK prefix and verification output must be canonical paths without symlink
aliases, and neither may contain the other. Admission failure leaves both SDK
bytes and any previous report untouched and writes its diagnostic to stderr.
Only an admitted owned verification directory is invalidated for a new attempt.

The package includes the repository formatter and comment-policy files. Git
commit hooks and whole-checkout lint workflows remain repository maintenance
tools; they are not installed into the consumer's Git repository by `find_package`.

The source SDK includes authored TOML readers, typed resolution and factory
generation, maintained default assemblies and dependency provenance locks.
The pinned `ext/googletest` Git submodule, its licenses and contract binaries
belong to host repository development and are excluded from the firmware SDK
payload.
Configuring a firmware consumer never downloads a test dependency. A consumer
that needs platform development tests uses the complete repository checkout;
the installed source SDK explicitly excludes those contract targets.


## Device and executable-window consumers

Consumer policy `nexus-sdk-consumers-v14` retains the complete v13 plan and adds
three independent C11 consumers: Native TMP117, F407 TMP117 and the F407 image
window. The full plan contains 59 source/build roots and 62 linked images. A plan
entry records required execution; these counts alone do not establish a pass.

The TMP117 compositions explicitly link `Nexus::TMP117I2C`, own a settled software
I2C endpoint and use all six public component functions. Native executes one
bounded, positive sample/cleanup path. F407 links that same model with actual
startup, generated bindings, resources and provider-retention checks; it never
runs on an MCU. Neither scenario establishes physical TMP117, PCB or bus support.
The component's two headers, two C sources and CMake target are mandatory SDK
payload. Unselected device code remains eligible for link-time removal.

The image reference copies one assembly and its physical layout from the SDK.
The existing linker/resource gate checks the derived executable window, vectors,
ELF LOADs and bounds. Its 512-byte prefix and 1,024-byte suffix reserve bytes;
this consumer supplies no metadata encoding, bootloader, signature or activation.
It selects no I/O faces, so provider-retention observation is not applicable.
Its report still binds source, layout, configuration, ELF and checked resources.
All three profiles audit actual C11/CPU/enum flags and public include boundaries;
ARM images reject linked libc heap and C++ runtime. Native heap use is unassessed.

### 中文

消费策略 `nexus-sdk-consumers-v14` 完整保留 v13 原有计划，再增加三个独立 C11
工程：Native TMP117、F407 TMP117 和 F407 执行镜像窗口。全量计划为 59 个源码／
构建根目录、62 个链接镜像；这些数量定义必须执行的范围，本身不代表验收通过。

TMP117 组合显式链接 `Nexus::TMP117I2C`，调用者持有完全结清的软件 I2C endpoint，
并使用六个公共组件函数。Native 实际执行有界的正向采样与清理；F407 使用同一软件
模型并实际链接启动代码、生成绑定，再执行资源与 provider 保留检查，不在 MCU
运行。两者均不证明真实传感器、PCB 或总线支持。两个公共头文件、两个 C 源文件和
CMake 目标列为 SDK 必需内容；未选择的器件代码仍可由链接器移除。

镜像参考从 SDK 复制唯一装配与物理布局，复用既有链接／资源门禁验证派生执行窗口、
向量、ELF LOAD 和边界。512 字节前缀与 1,024 字节尾部只保留空间，不提供元数据
格式、引导器、签名或镜像切换。该配置不选择 I/O face，因此 provider 保留观察
不适用；报告仍绑定源码、布局、配置、ELF 和已检查资源。三个工程均检查实际
C11／CPU／枚举 ABI 编译参数与公共头文件边界；ARM 拒绝链接 libc heap 和 C++
运行时。Native 的 heap 使用保持未评估，不借用 ARM 的结论。
