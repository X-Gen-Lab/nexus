# Cortex-M 软件支持与平台接入边界

Nexus 的 CPU 软件层支持 M0、M0+、M3、M4、M7、M23、M33、M55、M85。
同一不可变 CPU profile 解析器驱动 Core、Arch、原子操作及裸机/FreeRTOS
运行时配置；DSP、FPU、MVE、cache、MPU、DWT、security 和 SAU 均显式声明。
选择某个内核不生成新 SoC、Board、向量表、启动、驱动、链接布局或 typed factory。
当前完整 SoC 仍为 STM32F407/GD32F470，参考 Board 仍为三板，实板 HIL 未执行。

详细所有权、上下文与拒绝语义见 [Arch 契约](../design/arch-contracts.md)，
OS 生命周期见 [OS 适配](../../os/README.md)，精确整平台范围见
[支持矩阵](support.md)。旧 HEAD 的软件候选和 qualification 不继承到本轮。

## 九核与可选能力

下表列出解析器接受的能力格式；可选能力必须由具体芯片事实证明，不能从 CPU
名称推断存在或已启用。`none`/零明确表示未声明该能力，不做运行时发现。

| CPU | 原子 RMW / IRQ mask | 可选硬件格式 | 维护的 FreeRTOS port |
|---|---|---|---|
| M0 | saved-PRIMASK / PRIMASK | 无 FPU/MVE/cache/MPU/SAU | GCC/ARM_CM0 |
| M0+ | saved-PRIMASK / PRIMASK | 可选 v7-format MPU | GCC/ARM_CM0 |
| M3 | compiler exclusive / BASEPRI | v7 MPU、DWT | GCC/ARM_CM3 |
| M4 | compiler exclusive / BASEPRI | fpv4-sp-d16、v7 MPU、DWT | 无 FP 用 CM3；有 FP 用 CM4F |
| M7 | compiler exclusive / BASEPRI | fpv5-sp-d16/fpv5-d16、32-byte cache、v7 MPU、DWT | 无 FP 用受审 integer overlay；有 FP 用 CM7/r0p1 |
| M23 | compiler exclusive / PRIMASK | v8 MPU、显式安全状态、Secure SAU | ARM_CM23_NTZ/non_secure |
| M33 | compiler exclusive / BASEPRI | 可选 DSP、fpv5-sp-d16、v8 MPU、DWT、显式安全状态、Secure SAU | ARM_CM33_NTZ/non_secure |
| M55 | compiler exclusive / BASEPRI | 显式 FPU/MVE、32-byte cache、v8 MPU、DWT、安全状态、Secure SAU | ARM_CM55_NTZ/non_secure |
| M85 | compiler exclusive / BASEPRI | 显式 FPU/MVE、32-byte cache、v8 MPU、DWT、安全状态、Secure SAU | ARM_CM85_NTZ/non_secure |

Cortex-M 使用 short-enums ABI。无 FPU、无 MVE 时使用 soft；有 FPU 时明确选择
hard 或 softfp。M55/M85 的 MVE 为 `none`、`integer` 或 `float`：无 MVE 添加
`+nomve`，无 scalar FP 添加 `+nofp`；integer-only MVE 使用 softfp，FPU 事实
仍为 `none`。FP-MVE 必须同时声明可用 FPU。M55/M85 的 FPU 选择为 `auto`、
`fpv5-sp-d16` 或 `fpv5-d16`，`auto` 是显式编译器选项，不是硬件自动探测。

安全状态为 `single`、`secure` 或 `nonsecure`。只有支持安全扩展的 M23/M33/
M55/M85 可声明后两者；Secure 编译添加 `-mcmse`。SAU 仅允许 Secure 事实为真。
Cache line 仅允许零或 32 bytes，MPU 为零或该内核维护的 v7/v8 格式。M0/M0+/
M23 禁止 DWT capability；其他内核也必须显式声明才能读已启用的周期计数器。

IRQ priority width、external IRQ count 和 clock 是芯片事实。PRIMASK profile
固定为 2 个优先级 bits，BASEPRI profile 支持 3–8 bits。M0/M0+ 最多支持
32 个 external IRQ，M3/M4/M7/M23 为 240，M33/M55/M85 为 480；SoC 必须
声明实际有效范围。这些边界不代替厂商芯片事实或电气验证。

## CPU 机制与所有权

Arch 无 SoC、I/O、内核或厂商 SDK 依赖。生产算法通过内联指令/MMIO 边界执行；
GoogleTest/GoogleMock 只替换该硬件边界。Native 提供真实递归线程排他、C11
fence 与 publication 行为，不模拟 NVIC、物理 IRQ、特权切换或真实周期。

PRIMASK token 只在同 CPU/线程逆序恢复一次，保留输入 BASEPRI/FAULTMASK。
被保护元数据只允许特权 Thread/可配置 IRQ 参与，NMI/HardFault、其他核、安全
世界和 DMA 不在排他域内。上下文查询不授权 kernel ISR 调用；屏障不证明外设
idle 或 DMA drain。DWT 不启用/复位计数器，也不作为单调 deadline 时钟。

对齐 word acquire/release load/store 不屏蔽 IRQ。M0/M0+ RMW 使用 bounded
saved-PRIMASK exclusion，不静默链接 libatomic；其他核使用 compiler lock-free
RMW，exclusive retry 不等于固定 cycles。request/buffer/notify 生命周期仍由
owner 保证，atomic publication 不自动回收对象。

Cache 操作只维护独占的完整 32-byte lines，不 rounding、不 enable；所有相关
cache 在写入前完成 capability/enabled 检查。DMA receive 前置/排空后的维护、
可达地址和 buffer loan 由 provider/产品负责。MPU v7/v8 与 SAU 使用不同精确
encoder；实际单 region 编程要求本域内 privileged Thread、输入 PRIMASK=1，
相应单元已经 disabled，保留 CTRL、其他 region 与输入 RNR，不隐式启用。

产品拥有 MPU/MAIR policy、SAU/IDAU、NSC veneers、安全启动、共享资源协议和
恢复。CPU 机制不提供用户任务 MPU 隔离，也不把 range encode 当作安全验收。

## FreeRTOS 软件合同

全部 CPU profile 选择锁定内核的精确 port 源码；M0 及 v8-M port 同时编译所需
独立 assembly 文件。M7 无 FP 使用从锁定 CM3 派生的受审构建 overlay，添加
errata 837070 的 PRIMASK-preserving BASEPRI helpers 与 PendSV 处理，厂商
源文件保持锁定内容。禁止为了省事将所有内核映射到 CM4F。

Armv8-M 的 NTZ、Secure-only 和 standalone NonSecure 由显式事实选择，使用
`configENABLE_TRUSTZONE=0` 的维护路径。没有 Secure task heap、双世界 task
调用或 gateway；Secure 启动、IDAU/SAU 与 NonSecure coprocessor access 仍由
外部工程负责。用户 MPU task gateway 未实现，API 和 task 均为特权合同。

M55/M85 使用实际 MVE-aware context port。`configENABLE_FPU` 在 FP 或 MVE
存在时打开共享寄存器 bank 和 extended/lazy frame；integer-only MVE 不因此
获得 scalar FP capability。Upper bank 的保存路径在 port 源码/对象中单独检查；
VPR/lower bank 依赖硬件 extended exception framing，是该 port 的验收假设，
必须结合架构资料与实板核对。编译/ELF 不能证明完整 payload 保留或真实 FP/MVE
切换，不替代物理验收。

任务、TCB、stack、queue 和 notification storage 由调用者静态提供，内核不链接
heap allocator。应用拥有 scheduler start、workers、队列深度、公平性、shutdown
进度与 join 后的 reclaim；平台启动不启动 scheduler、不生成隐藏任务或 daemon。

运行中 API 要求 privileged、unmasked Thread。仅在 NOT_STARTED 阶段接受锁定
port 的 canonical bootstrap mask：Baseline 为 PRIMASK=1，Mainline 为精确
syscall BASEPRI；它们不能证明 mask 的来源，不能在运行/暂停时继承许可。Fault
mask 与其他 mask 在对象变更前拒绝。IRQ wake 另验证外部异常范围、优先级宽度
与实际 port 的 syscall 合同，notification 只提示重查 request/sequence 谓词。

## 外部 CPU runtime 配置

完整 SoC 工程继续使用原有严格平台 assembly。仅复用 CPU/Core/OS 时，外部工程
提供 `runtime.toml` 与芯片包维护的 `cpu.json`。以下是 **M33 NonSecure 软件配置
示例**，不是新增芯片或 Board 声明；clock、IRQ 数量和可选能力应替换为真实芯片
的受审事实。无需 Board 文件，也不输出 SoC/设备 factory。

`runtime.toml` 仅接受以下四项，路径相对此 TOML：

```toml
schema_version = 1
cpu_facts = "cpu.json"
backend = "freertos"
optimization = "Os"
```

`backend` 为 `baremetal` 或 `freertos`，`optimization` 为 `O2`、`Os` 或 `O3`。
CPU package 和 CPU/IRQ 对象的字段完整且唯一，未知字段、重复 JSON keys、矛盾
feature/ABI、非法 schema/clock 和 symlink path 被拒绝，不能用 authored override
伪造芯片能力。

```json
{
  "schema_version": 1,
  "cpu": {
    "arch": "cortex-m33",
    "dsp": false,
    "fpu": "none",
    "float_abi": "soft",
    "dwt_cyccnt": false,
    "mpu_version": 8,
    "icache_line_bytes": 0,
    "dcache_line_bytes": 0,
    "security": "nonsecure",
    "sau": false,
    "mve": "none"
  },
  "irq": {"priority_bits": 3, "external_count": 64},
  "clock_hz": 64000000
}
```

单独检查并原子生成 bundle：

```sh
python -B tools/configure/runtime.py \
    --assembly /path/to/product/runtime.toml \
    --output /path/to/build/generated-runtime
```

输出 `resolved-cpu.json`、`nexus_config.h`、`selection.cmake`、`input_paths.json`
与 ownership marker，绑定输入和配置 SHA-256，不输出 linker script、bindings
或 factory。配置失败使已有 owned bundle 失效，不继续使用 stale selection；
非 owned 目录不会被清空，输出不能覆盖输入或其父目录。`clock_hz` 不设时钟。

Source checkout 的公共 CMake 入口：

```cmake
cmake_minimum_required(VERSION 3.24)
project(product_runtime LANGUAGES C ASM)
include("/path/to/nexus/cmake/platform/Runtime.cmake")
nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_SOURCE_DIR}/runtime.toml")
add_library(application STATIC application.c)
target_link_libraries(application PRIVATE Nexus::Runtime)
```

安装后的 source SDK 使用下列入口，其余两条 target 操作相同：

```cmake
find_package(Nexus REQUIRED COMPONENTS Runtime)
nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_SOURCE_DIR}/runtime.toml")
```

`Nexus::Runtime` 传递 Config/ABI、Core、Arch、typed I/O face 和选定 OS adapter，
没有 SoC/Board/factory。一个 CMake build 只允许一个解析后的 Nexus assembly，
不能在同一 build 混用相互冲突的 ABI；额外组件由外部工程显式链接。MCU 工程在
首次 CMake configure 时选择 `cmake/toolchains/arm-gcc.cmake`。应用自己的启动、
向量表、链接布局、时钟、具体硬件和最终 executable 仍需由外部工程实现。示例
只展示可复用静态 library 的源图，不声称生成了可上板运行的 firmware。

Installed SDK 在生成/编译前核验完整 source identity。开发 fixture SDK 需要明确
opt-in，不能当作 publishable candidate；该 opt-in 也不会绕过 checksum/closure
校验。SDK/外部 consumer 必须针对本轮新 HEAD 重新执行。

## 执行证据与剩余验收

CPU profile/Runtime resolver 测试覆盖 immutable IR、严格 schema、ABI/feature
冲突、原子发布 bundle、输入变化和 stale output；Google 模型执行实际 CPU
算法，ARM 编译门禁保留宏、对象、汇编/反汇编、命令、返回码和 raw hash。
Runtime 门禁复用 `nexus_add_runtime` 的生产源图构建 Core/Arch/OS 组合，分别
检查真实 kernel 的 FP/MVE context 与 ELF，而不维护第二份手写 source graph。

```sh
python -B -m unittest discover -s tools/configure -p 'test_*.py'
python scripts/ci/tdd_gate.py --all --preset native-debug
python -B -m unittest discover -s scripts/ci -p 'test_*.py'
python scripts/ci/arch_compile.py --compiler arm-none-eabi-gcc \
    --output build/arch-compiler
python scripts/ci/cortex_runtime.py --compiler arm-none-eabi-gcc \
    --output build/cortex-runtime
```

这些门禁分别建立 CPU primitive、CPU runtime、完整 F407/F470 平台和实板证据。
可选 feature 组合按实际执行范围记录，单个组合通过不继承给所有可能组合。
当前开发执行目录为 `/workspace/nexus-all-cortex-evidence`；最终通过数与源码身份
由实际报告核对，不引用之前版本的 counts。

本轮没有新增真实 Board，没有执行物理 HIL。IRQ latency、FPU/MVE context
切换、cache/DMA coherence、MPU/SAU 行为、安全世界协作、stack high-water、时钟
和物理波形均保持未验收。正式 clean-source SDK、离线双构建、镜像 hash 和
candidate 封存须依据 [qualification](qualification.md) 重新执行；软件支持不
自动提升为 production release、硬件资格、安全认证或 LTS。
