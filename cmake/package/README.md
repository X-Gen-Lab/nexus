# 可移位源码 SDK

当前版本为 Nexus 0.2.1，版本值不建立发布、软件或硬件资格。消费者拥有装配、应用、
编译器／ABI、工作任务与资源预算；vendor 头和具体 provider 状态私有，不导出通用
预编译 MCU ABI 或应用生成器。版本唯一权威是
`dependencies/platform-compatibility.json`；CMake、生成头、安装包从它派生。
source interface epoch `1` 与 assembly schema `2` 分别计代，不随语义版本自动改变。

## 打包与消费

从已初始化、干净、已提交检出执行：

```sh
python cmake/package/package_source_sdk.py --source . --output /tmp/nexus-sdk
```

包记录源码 commit／tree、每个文件摘要、递归依赖 commit／tree、已审 GD32 导入和
许可通知。安装 CMake 在配置前验证全部字节；修改、缺失、额外或 symlink 文件拒绝。
准备／校验不下载缺失依赖。dirty 源仅可用 `--development-fixture` 生成不可发布包，
消费者需显式 `NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`，不建立发行资格。

移动完整 prefix，再从独立工程消费：

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

装配明确选择 Board、part、clock、backend、controller mode、endpoint 与预算。
Board 相对路径相对于装配，即使位于 SDK 外；`NEXUS_EXPECTED_SOURCE_REVISION`
可固定消费方已审版本。Native 使用 host 编译器，不传 ARM toolchain。

`Nexus::Core`／`Arch`／`IO`／`Platform` 与可选组件、OS target 各自导出依赖。
`nexus_add_firmware` 链接显式源码、选定组件、startup、生成绑定与 linker 输入，
不创建 main／任务／scheduler。应用调用 `nx_platform_start`，使用生成
`nexus_factory_strong.h`，例如 `nx_factory_gpio_ref(NX_GPIO_REF_LED0)`。
C11／C++17 拒绝跨类别引用、controller 传 endpoint getter 和裸 scalar ID；getter
只返回共享只读方法表与独立可变 context，不分配、不启动硬件。
`nx_binding_<id>`／`nx_device_<id>` 仍是固定类型别名，启动顺序由应用明确拥有。

每个包都有 strong header，不要求 `[factory]`。该表仅接受可选布尔 `capabilities`，
true 输出 metadata，`factory.strong` 未知并拒绝。旧 scalar getter 仍可用，二者
返回同一静态对象，没有额外运行层。

MCU 输出 ELF／BIN／map／资源报告，ELF 绑定配置摘要和真实向量。精确 CPU facts
决定指令、FPU、float／enum ABI，目标传递相同参数。C++ 消费采用自己的嵌入式
runtime 策略，已测路径例如 `-fno-exceptions -fno-rtti`。
`tests/contracts/test_source_sdk.py` 对真实包做含空格路径搬移、篡改／身份拒绝、
独立 Native C／C++ 执行及两 MCU 家族 ARM startup／linker／资源检查，不证明实板。

实际资产清单和执行收条标识 SDK、工具、依赖许可与结果；最低 Python／CMake
声明不表示所有 host 已验证。维护的已执行 host 为 Linux，HIL 为 `not_executed`。
历史候选保留原身份，[0.1.0 范围](https://github.com/X-Gen-Lab/nexus/blob/83a27dfbee4e8fbf4d35875b1e9db85af7516eec/docs/status/release-0.1.0.md)不资格化新版本。

## 完整安装消费者验证

发行自动化验证已有可发布 prefix，不使用开发 opt-in：

```sh
python cmake/package/verify_consumers.py \
  --prefix /relocated/nexus-sdk --output build/installed-sdk-check
```

`--parallel N` 是总 consumer job 预算，未指定依次继承 `NEXUS_BUILD_JOBS`、
`CMAKE_BUILD_PARALLEL_LEVEL`，两者皆无时为 `2`。显式值优先；多个独立调用需外部
分配额度，不是全局 CPU semaphore。每个 compiler build 只用一个 job，`--parallel 1`
保留串行。各 consumer 都重新验证源码；失败先取消并 join 所有已启动工作，再写
最终失败报告。

`verification.json` 和逐命令日志记录实际参数／退出码。开发 fixture、缺工具、
编译／运行／链接失败、私有 include 泄漏或资源／配置绑定失败均拒绝。SDK 和输出
必须为无 symlink 的 canonical 路径，互不包含。准入失败不改 SDK 或旧报告，只向
stderr 写诊断；只允许失效已准入的自有输出目录。

包带格式和注释策略，但 `find_package` 不向消费者 Git 安装 hook。TOML reader、
typed resolver／factory、维护 assembly 和来源 lock 属于 SDK；host googletest、
它的许可和 contract binary 不属于固件 payload。消费 configure 不下载测试依赖，
需要平台测试时使用完整检出，安装 SDK 明确不提供那些合同 target。

## 器件与执行窗口消费者

当前版本化消费计划由 `tools/maintenance/consumer_profiles.py` 唯一维护，在下述设备与镜像
场景之外包括标准 Native 外置 provider 生命周期工程。每个精确的源码／构建
根目录和链接镜像都必须执行；计划结构本身不代表验收通过。此前的设备扩展
引入五个独立 C11 工程：F407／GD470 借用 RX、Native／F407 MCUboot Control，
以及单独的 F407 完整引导镜像。

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

借用 RX 组合精确持有两个 slot 和四个 piece 记录，只选择两个 512 字节备用 pbuf。
通用应用翻译单元仅见 Nexus 不透明接口；组合层显式链接所选 Storage 目标。
ARM 参考仅建立链接与资源观察，没有物理 MAC／PHY、报文执行、可用随机源，
也没有实测栈峰值或时序。

MCUboot Control 使用官方 trailer 公共实现。Native 在公开的非持久 NOR 模型上
实际执行 NONE→PERMANENT 控制：两个完整 4,096 字节擦除扇区、四字节编程单元，
同一时基上的一个有限操作 deadline。这不认证候选镜像。F407 Control 使用 NULL
操作，意外执行也不能修改 Flash；空装配不适用 provider 保留观察。完整引导作为
独立 ARM 镜像，保留官方验证与复制代码、明确署名的上游公开测试密钥，以及由唯一
布局派生的区域。它只提供链接证据，未执行镜像认证、Flash 修改、跳转、复位、
回滚或实板验证，不提供生产密钥。

完整的 311 文件 lwIP 与 80 文件 MCUboot 已评审源码／许可闭包、组件 helper、
通用与显式头文件、标准消费输入均属于 SDK 必需内容。MCUboot CLI 先调用完整 SDK
验证器，局部源码 hash 检查不能单独准入不完整 SDK。ARM 继续检查实际 C11／CPU／
枚举参数并拒绝链接 libc heap／C++ 运行时；Native heap 仍未评估。必须实际执行
当前完整计划后才能形成软件资格。
