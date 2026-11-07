# 单份 TOML 装配与静态实例生成

本目录把外部 **TOML schema 2** 装配与经过审核的 SoC／Board JSON 事实解析为不可变
类型化 IR，再原子发布同一套编译／链接输入。Python 3.11 及以后使用标准库 TOML
解析器；CMake target 保持软件源码图的唯一权威，生成器不另建编译依赖图。

[FIG-03 配置和原子生成](../../docs/assets/diagrams/configuration-generation.svg)
展示构建期输入／产物关系，不表示运行调用或物理资格。先使用查询解释所选事实，
再生成 bundle；配置成功不建立硬件、电气、时序或软件候选资格。

## 创建与查询装配

以下命令从仓库根执行，示例选择现有启明 V3.1；其他 Board 必须查询其真实 binding：

```sh
python tools/configure/configure.py list-bindings \
  --board stm32f407_qiming_v31 --kind uart
python tools/configure/configure.py init \
  --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
  --profile uart --binding uart1 --instance link0 \
  --output my-platform.toml
python tools/configure/configure.py list-bindings --assembly my-platform.toml
python tools/configure/configure.py check --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml --json
python tools/configure/configure.py diff \
  --assembly my-platform.toml --against my-platform-next.toml
python tools/configure/configure.py generate \
  --assembly my-platform.toml --output build/my-config
```

`init` 接纳 `empty`（默认）、`gpio`、`uart`。每种 profile 都显式指定 Board、backend
和 clock；非空 profile 另需受审 `--binding`。`--instance` 默认为 `device0`。GPIO
选择 output，UART 明确写入 115200 baud、64 项 IRQ events、priority 5 且不调用内核。
这些是可见 authored 值，不是硬件默认。模式／路线不可用即拒绝，不更换选择来通过；
拒绝覆盖已有文件，写入或校验失败只移除本次未完成文件。

私有 Board 用 `init --board-package /path/to/package`，不用 `--board`。package
相对当前目录读取，其路径相对新 TOML 写入，便于整棵产品树迁移。init、解析与查询
共用目录检查：允许普通 `..`，在路径归一化前拒绝原路径任何 symlink 分量。package
必须已有受审 `board.json`，init 不发明 pin 或 clock。

`check`、`explain`、`list-bindings`、`diff` 不产生 generated-output 副作用。
`explain` 默认输出公共接口、clock、resource、memory domain、声明预算与执行职责
的人类摘要。原 JSON 消费者须显式 `--json`，取得同一精确 IR 投影；声明预算和
provider 上限不是已测镜像、stack、吞吐或 latency。

`list-bindings` 仅选择 `--assembly`／`--board`／`--board-package` 之一。Board-first
查询不虚构 backend／clock，它们显示为 `null`；`--kind uart` 可按类别过滤。
`--json` 显示真实 implementation、执行形态、DMA path、按模式归一化的
`resource_keys`／`occupied_resources`。Board 查询包含 SoC／Board reservation，
assembly 查询还包含已选 owner。polling 不占另一 DMA 路线。assembly 查询核对事实
字节与 resolved input hash 后才追加只读视图；候选仍为
`admission = "configuration-required"`，clock、backend、options 和完整资源集还需
正常解析，缺失接线仍然缺失。

`diff` 比较实际 platform、route、mode、storage、endpoint、clock、budget 决策；
instance path 定位字段，`$order` 保留有意义的声明顺序。`before_present`／
`after_present` 区分缺字段与显式 `null`。只改文件名／配置 hash 不算有效配置变化，
两份输入必须都可解析才打印比较。

JSON `impacts` 解释声明存储、typed factory ID 顺序、公共接口、传输单位／界限／
对齐／message bound 和执行义务变化。数字 ID 重排要求重新编译，不持久化为 ABI；
DMA／cancel 改变须复查产品 service／shutdown，不据此估算 ELF 节省。

## 诊断与 provider 静态检查

所有命令接纳 `--diagnostics-json`：配置拒绝向 stderr 写一个 JSON object 并返回 1，
成功输出不变；CLI 语法错误仍由 argparse 返回 2。诊断保留 `code`、`message`、输入
`source`；已知读文件错误定位该文件，未知／缺字段给出 `context`、`unknown`、
`missing`，冲突含物理 `resource` 和双方完整 `claims`（owner／mode／source）。
`contexts` 保留外层 instance 信息，其余错误保留 `invalid_configuration` 和真实
message，不猜 line number 或替代值。

所选 provider 或其父 Python package 缺失时，返回 `provider_module_missing`，含
`family`、`module`、`source`、`missing_module`；只载入所选 family。既有入口内部的
缺依赖或其他 import 错误保留实际 exception，不改报为“不支持 provider”。

```sh
python tools/configure/configure.py provider-check \
  --assembly my-platform.toml --expect-family stm32f407 \
  --expect-part STM32F407ZGT6 --json
python tools/configure/configure.py check \
  --assembly my-platform.toml --diagnostics-json
```

`provider-check` 解析精确输入，在一次性目录执行生产 static emitter。可选 identity
断言拒绝不同 family／part，报告所选 controller face／implementation、reviewed
input、配置 module 与 generated-file SHA256。仅建立 `resolved_typed_ir` 和
`static_bundle_emitted`，不注册 provider 或创建硬件事实；实际 compile／link、
GoogleTest／GoogleMock 和 station 验证仍使用各自维护入口。

人类视图和 binding JSON 对 MCU 标 `not_established_by_configuration`，Native 为
`not_applicable`。原 IR qualification placeholder 只描述生成器 scope，不证明另一
station 结果存在或不存在，物理证据应从精确 Board／station 记录读取。

## 平台输入与实例表

完整装配示例：

```toml
schema = 2
board = "stm32f407_qiming_v31"
backend = "baremetal"
clock = "hse8-pll168"
optimization = "Os"

[memory]
main_stack_bytes = 2048
libc_heap_bytes = 0
static_ram_limit_bytes = 32768
flash_load_limit_bytes = 65536

[gpio.indicator]
binding = "led0"
mode = "output"

[uart.link0]
binding = "uart1"
mode = "irq"
baud = 115200

[uart.link0.rx]
format = "events"
capacity = 64

[uart.link0.irq]
priority = 5
calls_os = false
```

`board` 是 `boards/` 直接子目录的维护身份；外置包用
`board_package = "path/to/package"`，相对 authored 文件解析，两者必须且只能一个。
`clock` 匹配 Board 晶振和维护的 SoC profile。`backend` 为 `native`、`baremetal`、
`freertos` 或显式 `threadx`；Native 不建立 MCU 执行，ThreadX 只维护自身 Native／M4
子集，不继承 FreeRTOS CPU 覆盖，见 [ThreadX](../../docs/design/threadx.md)。

`optimization` 接纳 `O2`、`Os`、`O3` 及对应 `-lto`；显式 `Og`、`O0` 用于源码调试，
不接纳 `-lto` 后缀。`native-step-debug.toml` 用 `Og`，普通 Native debug 仍为偏生产
的 `Os`。可选 `abi` 表（`[abi]`）仅断言选定 CPU 事实，可选 `components` 选择已有明确映射的
CMake target，不表示所有组件都由该列表构造。

各 kind 表只列需要的实例，含 ID、Board `binding`、维护的 `mode` 和有限 options。
UART authored `irq` 归一化为 `irq-byte-event`；RX bytes 和带时间／错误 events 是
不同存储 profile。EXTI／SPI 仅在所选 mode 使用 IRQ 时采用嵌套 `irq`。未知字段、
kind／binding 矛盾、不支持模式或未审路线拒绝，不静默替换模式或资源。

| 基础类别示例 | binding／mode 之外的关键选择 |
| --- | --- |
| GPIO | Board pin mask、安全初始电平 |
| UART | `baud`、RX `format`／`capacity`、IRQ `priority`／可选 `calls_os` |
| SPI | controller `max_hz`，显式 device CS／mode／speed |
| I2C | 家族受审 bus rate，显式非保留七位地址 |
| Flash | 精确物理 geometry；产品保留区外置 |
| Watchdog | 构造不启用不可逆 watchdog |
| EXTI | `edge`、`event_capacity`、嵌套 IRQ priority |
| PWM | `period_ticks`、`duty_ticks`、整数分频 `tick_hz` |
| ADC | `channels`、`sample_times`、`reference_mv`、GD32 `timeout_ms`、DMA IRQ |

这张表是基础字段入口，不是全部类别或全部 family 支持表。完整字段由
`contracts.py` 定义，真实路线和逐模式范围以
[派生能力矩阵](../../docs/delivery/capabilities.md)、各 family 合同和 binding 查询为准。

## 精确模式、共享资源与端点

以下 F407／GD470 传统控制器模式说明保留各自独立实现，不能推广到整系列：

| 控制器 | 维护模式或参考入口 |
| --- | --- |
| STM USART1／GD USART0 | IRQ bytes／events、`dma-tx`、`irq-blocks`、`rx-dma-idle`、`dma-duplex`、`rx-dma-continuous`、`dma-duplex-continuous` |
| STM USART2 | IRQ bytes／events，以及精确路线的有限 `dma-tx` |
| GD USART1 | IRQ bytes／events、`irq-blocks`、有限 `dma-tx` |
| STM SPI1／SPI2 | SPI1 `short-poll`，两 controller 有限 `dma` |
| GD SPI0／SPI4 | `short-poll` 与有限 `dma-full-duplex` |
| STM I2C1／I2C2 | I2C1 `poll-messages`，两 controller 的 `irq-messages`／`dma-messages` |
| GD I2C0／I2C1 | `poll-messages`、`irq-messages`、`dma-messages` |
| STM ADC1／GD ADC0 | `single-shot`、`low-rate-scan`、有限 `trigger-dma` 与独立 `trigger-dma-continuous` |
| STM CAN1／2、GD CAN0／1 | `classic-irq`，共享 filter domain |
| STM OTG FS／GD USBFS | `device-fs`、PIO endpoint、显式 USB48 owner |
| STM ETH／GD ENET | `raw-rmii`、MDIO、描述符／RX 借用 |
| STM TIM4／GD TIMER3 | `edge-capture` |
| STM TIM1／GD TIMER0 | `complementary-pwm`、deadtime／break |
| G474 GPIOA／LPUART1 | 独立窄 GPIO／`irq-bytes` 基线，不回退到 DMA |

F407 新增 UART route 与 G474／H743／U575 等增量都按当前精确事实查询，不由这份
示例表排除或自动授予。SoC 模式不安装 PCB connector、收发器或外设。I2C 的 100 kHz
也不是全部 family 的默认，例如低端参考另有自己的受审速率。

UART `irq-blocks` 必须 `rx.format = "blocks"`，拒绝 `rx.capacity`，因为
`nx_uart_port_rx_start` 接收调用者 stream。TX DMA＋IRQ blocks 拒绝，duplex 是
独立 provider。有限 RX／ADC 存在 rearm gap；continuous 另有 caller-loan／replacement
合同，backpressure 时停止，不覆盖 consumer，也不生成默认 block pool。每模式 IRQ／
DMA 资源仍必需。

DMA 冲突按实际 controller 和 stream／channel，不按 request selector 区分。固定
provider 检查整个精确 TX／RX tuple，语法合法的替代 selector／stream 或缺 trigger
claim 仍拒绝。`timer:TIM3` 和 `controller:TIM3` 是同一物理 timer，包含 Board
reservation 和 PWM。timebase reservation 属于 SoC，STM TIM2、GD TIMER1 及 IRQ
对外部 Board 也保留，Board 重申不释放。STM DMA2／GD DMA1 clock 由 Platform
共享持有，全部 request／block loan 排空才撤回。`mode_resources` 只在
`trigger-dma`／`trigger-dma-continuous` 占 TIM3／TIMER2，与 PWM 冲突；CPU-only
CCM／TCM 不进入 immutable DMA domain。silicon route 不证明 PCB 接线。

G474 `shared-fixed-pwm` 保留两个普通 `[pwm]` 实例及字段。必须完整选择 TIM3 CH1／
PA6 与 CH2／PA7 AF2，tick／period 一致；解析得到一个 immutable physical owner、
兼容 base claim、独占 lane／pad，构造／关闭一次且保留两个原 PWM factory ID。
缺成员、不同 base、其他 whole-TIM3 personality 拒绝，普通 `fixed-pwm` 仍独占。
加入已运行 peer 仅接其当前 phase，不保证 coherent group start。
[维护的软件输入](../../cmake/package/verification/io_evolution/g474-pwm-shared.toml)
不建立 PCB、电气 timing 或 HIL。

SPI／I2C controller 与 child endpoint 身份分离。以下只是完整 Board assembly 的扩展
片段，不是独立可生成配置：

<!-- nexus-toml-fragment: endpoint tables extend a complete Board assembly. -->

```toml
[spi.bus0]
binding = "spi_flash"
mode = "short-poll"
max_hz = 1000000

[spi_device.nor0]
controller = "bus0"
# This reviewed GD32 route contains the fixed PF6 active-low CS.
mode = 0
max_hz = 1000000
```

没有内建 CS 的路线必须显式受审 `cs_binding`。I2C 使用 `[i2c_device.sensor]`、
`controller = "bus0"` 和 `address = 0x76`。endpoint `driver` 默认为 `spi-endpoint`／
`i2c-endpoint`；`bmp280` 需要其维护的 SPI component。driver 不创建组件实例或
worker，endpoint 不含 queue capacity。可选 Native `model_bytes` 精确独立分配
1～256 字节模型 buffer，Native I2C 必需，Native SPI 不提供则显式 loopback。

## 类型化 IR 与配置代码边界

`assembly.schema.json` 从 `authored.py` 同一字段模型派生，提供补全、单位和生命周期
描述，不另立接纳规则；resolver 再验证硬件路线、范围、IRQ、source identity 和资源。
补全不能证明芯片／PCB 能力。

有序 `CONTROLLER_CONTRACTS` 是公共类别字段／header facts 的权威，类别枚举、graph
envelope 和公共 IRQ normalization 从中派生。typed option constructor、family mode
与跨资源仲裁仍各负其责，不增加 firmware registry 或第二配置权威。

`authored.py` 先得到冻结 `AuthoredAssembly`，它是未准入的用户意图。生产
`resolve_ir()` 验证精确事实、选择和冲突，才返回 `ConfigurationIR`。controller、
endpoint、CPU／kernel、memory、resource 均为冻结命名记录，不提供字典接口或第二份
`values`。IRQ 为记录，ADC channel／sample 是 tuple；受审 JSON 和 keyed index 是
明确只读叶 mapping，临时草稿仅在 resolver 局部。这不宣称每个 JSON key 都有名义
Python 类型，也不让 C ID typedef 变成强名义类型。

private typed source epoch 2 与 projection schema 1 独立。`to_dict()` 仅派生审计／
诊断／产物检查的 `resolved.json`；`source_fields` 只记字段存在性，不复制决策值。
没有可变配置 cache 或软件源码图。旧 JSON 平台入口（包括旧生产 fixture）已拒绝，
维护 fixture 通过 test-only helper 写 TOML；历史报告和只读验证不能重新打开旧入口。

## family 包与 C11 硬件行为

`providers/` 显式接纳 Native、STM32F103／F407／G474／H743／U575、GD32F303／F470，
每个保留精确 part／route／mode。实际配置包在 `soc/<vendor>/<family>/configure/`，
Native 在 `soc/native/configure/`；map、`FamilyDescriptor` 和共享 leaf 留在
`tools/configure/providers/`，测试在 `tools/configure/tests/providers/<family>/`。
公共解析、仲裁、IR 和 bundle 仍由本目录负责。

`configure/contract.py` 拥有 mode map、CPU／enum ABI、数值界限、固定 DMA／trigger、
共享 IRQ 和内建 CS；`__init__.py` 显式导出合同和 constructor，兄弟模块直接 import
`contract.py`，不依赖未初始化完的 package。`bindings.py` 描述静态构造和固定 IRQ，
`emission.py` 给出 system／shared-clock binding 与调用顺序。具体 C11 adapter 负责
硬件 admission、MMIO／readback、prepare、stop、rollback、recovery。

constructor 返回冻结 `Construction`，含明确 `state_type`、`definitions`、
`initialize`、`stop`、`retain_on_failed_init`、`exports`、`pin_snapshots`。公共 emitter
读命名字段，不猜私有类型或按 family 名猜清理责任。initialize 是 `DirectCall`，stop
是 direct-call tuple；参数只允许明确 `Symbol`／`Address`、uint32 常量、boolean、
NULL 和嵌套 direct call，拒绝自由表达式、cast、赋值、函数指针／member call。

这不是完整 C AST：`definitions`、`exports`、`face_definitions` 仍保存声明、常量配置、
存储和 face，真实编译器验证符号／参数。薄 IRQ 和 Platform start／stop 仅保留固定
类型化调用顺序，不在 Python 实现外设或 clock 寄存器算法。Construction 仅在构建期
存在，不生成运行时注册或额外分派。未知 family／mode 关闭式拒绝，无 package discovery。

family 还拥有 endpoint capability、backend／clock 耦合、cache 派生字段、全选择集
排除和可选编译常量；公共 resolver 保留输入身份、schema、事务组合与仲裁。G474
ADC／shared-PWM 的精确 timer 是 family helper，不藏在 generic resolver。
`FamilyDescriptor` 列出配置 helper，CMake 仍独占编译源码；public mode trait 不能
单独授予 family。

`providers/common.py` 只含 leaf check 和冻结合同，跨域 option／clock／IRQ 检查位于
`selection.py`，`ir.py` 拥有决策与单向投影，DMA region 渲染归 `memory.py`。provider
binding 不反向 import 顶层 emitter；H743 preflight 由 emitter 注入 DMA admission
callback，不反向加载 emitter。派生支持入口在 `matrix.support_catalogue`，选定合同
不 import resolver。这些是构建期边界，不改 public C ABI／static factory。

新增 family 需真实 capability／constructor、CMake reconfigure dependencies、SDK
必需文件和测试。CPU／FPU／float ABI／enum ABI 从同一解析结果进入 CMake，Arch 和
build options 只接纳维护组合，不给新 MCU 偷套 Cortex-M4 flags。配置模块不选软件
源文件。

## CPU、IRQ 与独立 Runtime

`cpu.py` 是共享 strict CPU resolver，显式读取 arch、DSP、FPU、float ABI、DWT、
MPU、I／D cache line、安全态、可访问 SAU、MVE 以及 IRQ priority width／真实外部
vector 数。CPU 名称不启用可选硬件，SoC provider 先检查精确芯片值；Native 保留
四位模型优先级范围、零外部 vector，不宣称 NVIC／DWT。authored `[abi]` 只断言
arch／FPU／float ABI，不能覆盖芯片能力。

维护 M0／M0+／M3／M4／M7／M23／M33／M55／M85 软件 profile，得到单一 immutable
ABI、flags、Arch capability 和 IRQ／kernel port。它不自动扩大七个 MCU family 的
精确软件范围，更不证明整芯片全部外设或物理资格。

`cpu.dsp` 必须为严格布尔值。M33 为 `false` 时发出 `-mcpu=cortex-m33+nodsp`，`true`
选择受审 DSP；Native／M0／M0+／M3／M23 要求 `false`，M4／M7／M55／M85 要求 `true`。
`NEXUS_CPU_HAS_DSP` 与事实一致，缺失／矛盾拒绝，不依靠编译器默认。

| CPU 执行 profile | FreeRTOS context port | mask 合同 |
| --- | --- | --- |
| M0／M0+ | `GCC/ARM_CM0` | PRIMASK |
| M3／无 FPU M4 | `GCC/ARM_CM3` | BASEPRI |
| 有 FPU M4 | `GCC/ARM_CM4F` | BASEPRI |
| 无 FPU M7 | 受审 `nexus/ARM_CM7_integer` erratum overlay | BASEPRI |
| 有 FPU M7 | `GCC/ARM_CM7/r0p1` | BASEPRI |
| M23 | `GCC/ARM_CM23_NTZ/non_secure` | PRIMASK |
| M33／M55／M85 | 对应 `GCC/ARM_CM*_NTZ/non_secure` | BASEPRI |

v8 NTZ port 运行一个声明安全态，不引入 Secure gateway。Secure 发 `-mcmse`，NS／
single-state 不发。FP／MVE context 按精确 feature 保存；integer MVE 无 FPU 使用
softfp，因为 GNU soft ABI 禁用 MVE。M55／M85 无 MVE／FPU 时显式排除指令；CPU
flags 同时进入 compile／link，没有第二张 CMake CPU／FPU 表。

Mainline FreeRTOS 逻辑 syscall ceiling 为 5，8 个 priority bit 时为满足偶数要求的
10；Baseline 使用 PRIMASK、逻辑 ceiling=0。M0／M0+／M23 固定 2 位／4 级，Mainline
允许 3～8 位，再由真实 SoC 缩小。外部 IRQ 架构上限为 M0／M0+ 32、M3／M4／M7／
M23 240、M33／M55／M85 480；system exception 不计入，496 total exception 不是
496 external IRQ，CMSIS array 容量也不是上限。真实 count 是必需 SoC 事实，不能
用最大值默认宣称全实现。baremetal／Native 无 kernel ceiling；非法组合或后端拒绝。

外部 SoC package 也消费 `cpu.resolve`／`CpuProfileIR`。CPU／Core／Arch／OS Runtime
使用独立 `runtime.py` TOML 入口，复用 resolver／`emit_profile`，不生成 device
factory、pin route、startup、SoC driver 或 memory layout。机器入口
`cpu.py --facts cpu.json --backend baremetal --output build/cpu-config` 读取
`schema_version = 1`、CPU／IRQ 记录及正 uint32 `clock_hz`，自有输出原子发布，保护
source parent、symlink 和非自有目录；它不替代 CMake 使用的唯一 authored TOML。

## 静态对象与 factory

私有角色 namespace 为 `s_nx_port_*`、`s_nx_endpoint_*`、`s_nx_cs_*`、RX／model buffer
和 Platform 状态。`s_nx_face_*`／`s_nx_device_face_*` 为 `static const`，仅有共享 ops
指针与私有 context 指针。Native 生成实例不共享 singleton fixture 存储。

`nexus_factory.h` 定义装配内 `uint16_t` ID、`NX_UART_ID_LINK0` 等常量与
`nx_factory_uart(nx_uart_id_t)`。有效 ID 返回已有只读 face，无效／count ID 返回 NULL；
SPI／I2C controller getter 与 `nx_factory_spi_device`／`nx_factory_i2c_device` child
getter 不混用。查询不初始化、不分配、不加锁、不字符串搜索、不改生命周期。ID 是
装配内 source interface，不是持久化／binary protocol ABI；选定实例仍属于该平台。

新工程应包含 `nexus_factory_strong.h`，使用独立名义类型 `nx_gpio_ref_t`、
`NX_GPIO_REF_LED_A`、`nx_factory_gpio_ref`。C11／C++17 拒绝 UART ref 或 raw scalar
误传 GPIO getter。inline wrapper 复用原 getter，不加 registry、硬件动作或 instance
storage；普通 scalar getter 仍可不带 wrapper 使用，但 typedef 本身没有跨类别隔离。

可选 `[factory]` 表内设置 `capabilities = true`，生成只读 `nx_factory_io_capability_t` 和
`nx_factory_<class>_capability`，描述选定 mode、unit／size／alignment／message bound。
上限零表示未描述，不是无限。Native 标 `NX_FACTORY_CAP_MODEL`，不声称物理 IRQ／
DMA。无效 ID 返回 NULL；描述与 face／热分派分离，默认装配不生成。

## bundle 身份、失败失效与生命周期

bundle 含 `resolved.json`、`nexus_config.h`、`nexus_bindings.h`、`nexus_factory.h`、
`nexus_factory_strong.h`、`bindings.c`、`selection.cmake`、`memory.ld`、
`resource-budget.json`、input navigation 与 ownership sentinel。
`configuration_code_paths.json` 为 CMake 登记 Python 输入，`generator-identity.json`
另绑其字节；configuration digest 只描述决策，不是 compiler／toolchain／environment／
generator version。解析前 snapshot，解析与 emission 后复查；family 未知时初始范围
覆盖全部显式维护 family，发布身份只保留选定 closure。SDK 完整验证独立于 sidecar。

全部 authored／fact input 绑定 hash，CMake 读取精确 input path 重配，不再解析 TOML；
resolved digest 嵌入 MCU ELF，MSP／heap 有明确 symbol／interval，stack 仅计算一次。

每个 output 以非阻塞 OS file lock 允许一个 writer。先保护 input、source package、
symlink；ownership marker 不授权删除 Board／SoC 事实。然后旧自有 bundle 移至私有
sibling，公开 output 在原子发布前不存在。失败移除私有 output；中断留下有界
pending／previous 对，在同一 lock 下恢复。lock file 保留，避免不同 inode 锁。
成功时 byte-identical 文件继承 timestamp，只改预算不会重写无关 factory header；
旧输出从不作配置／校验 cache。

生成失败不留下可用 partial output。重复 TOML table／JSON key、bool-as-number、
溢出、不安全 C ID、part／ABI／pin 冲突、非法地址、未知 driver、输入变化均拒绝。
同 physical stream 的不同 DMA channel 仍冲突；共享 STM32 EXTI 仅一个有界 static
dispatcher，要求相同 priority；调用 OS 的 IRQ 还须满足 FreeRTOS ceiling。

`nx_platform_start` 显式构造硬件，不创建 scheduler／产品 worker。启动前可查询
factory identity，I/O 使用须成功启动。stop 要求 direct writer 静止，逆序排空；BUSY／
error 保留 context／clock／cleanup progress。watchdog 一经启用为显式不可逆效果。
factory pointer 不会在 cancel／timeout／shutdown／restart 时撤销借用。

可选 `nx_platform_start_diagnosed`／`nx_platform_stop_diagnosed` 使用调用者输出，NULL
保持原行为。phase 为 context／system／resources／controller，controller index 是
`resolved.json.controllers` 的零基顺序，不是类别 factory ID。不保留输出指针，
原 primary／cleanup／remaining effects 为权威，见
[生命周期诊断合同](../../docs/design/official-stm32-provider.md)。

## 检查与支持边界

`python -B -m unittest discover -s tools/configure -p 'test_*.py'` 检查 resolver。
GoogleTest／GoogleMock 执行真实编译 factory 和 provider 生命周期；
`tests/contracts/{stm32,gd32}_assembly` 是选定路线 ARM 软件链接，物理 HIL 需真实
station 执行，不能由该命令推导通过。

`assemblies/catalogue.json` 显式列出维护的公共／Native 参考。
`python -B tools/configure/matrix.py --check` 检查 authoring、resolver、factory、mode、
component target 和每项 assembly；`--output path.json` 派生精确软件支持矩阵，
`physical_status = not_executed`。新增 assembly 必须登记；私有 fixture 不会因此变成
公共 Board route 或物理资格。
