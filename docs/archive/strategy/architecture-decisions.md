# Nexus 架构决策

这些决策用于当前通用平台实现；实际运行范围与未完成任务见 [RF 执行清单](../implementation/refactor-execution.csv)。历史 `affaa86f` 的 Product 和保留 Flash 分区已不代表当前架构。公共 API 以源码头和 target 为准，不以历史提案代码冒充实现。

## ADR 001 C11 契约与明确的破坏性重构

状态：采用。

保留 C11、CMake/Kconfig/CTest、维护中的 FreeRTOS 和已有有效软件回归。用户授权移除不良设计，不为错误实现保留伪成功兼容层。公共接口变更原子更新该仓生产调用者、测试和文档；外部仓库在独立提交中更新 SDK pin 与调用者。

源码兼容、配置兼容和持久化兼容分别管理。旧未认证 CBC/NXCFG 和 binary v1 不静默迁移；部署产品需要独立备份、回滚与可验证的数据转换。源码 SDK 没有跨 compiler/configuration 的二进制 ABI 承诺。

## ADR 002 通用平台与外部产品分仓

状态：采用，Runtime/Firmware 已实施。

Arch 拥有 CPU 异常上下文、saved mask 和屏障；SoC/controller 拥有时钟、IRQ/DMA、控制器、芯片内存与全物理 Flash 几何；Board 拥有 PCB 接线、晶振、电气资源与安全初值；HAL/OSAL 提供可验证能力契约。Runtime 仅初始化/释放 HAL 与 OSAL 所有权，Firmware 装配启动对象。

产品 main、workers、组件顺序、领域控制、客户协议、分区/更新/健康/恢复/制造政策在外部仓库。`Nexus::Product`、产品身份和平台内 application choice 已删除，外部固件使用 `Nexus::Firmware` 与通用 runtime API。READY 只表示基础设施，不表示产品健康。

## ADR 003 每个 build root 一份有效配置

状态：采用。

`NEXUS_CONFIG_FILE` 明确软件片段，preset 选择 compiler/build mode。Kconfig 一次解析生成 `effective.config`、`nexus_config.h`、`config.cmake`；未知/重复/非法值、choice/range/dependency 冲突与失败生成阻断配置。根 `.config` 和根生成头不参与回退。

Board/backend、CMake tests/contracts 开关和 linker 预算与同一有效输入一致。不同平台或后端使用不同 build root，不允许同次配置叠加多个 Nexus。重配清理旧 CONFIG cache；失败不允许旧 generated bundle 继续编译。

## ADR 004 所有权和 settlement 优先于表面超时

状态：采用。

所有 API 声明 task/ISR、等待、时间起点、caller storage、callback、取消/故障状态。有限 deadline 在 admission 前开始，排队、锁、操作和等待消费原预算。时钟端口真实且显式；查询次数不是时间。

异步取消请求不是所有设备统一的 buffer return proof。必须依据类型化 result 的 settled、callback/硬件停止契约，失败保持 lease 与可重试状态。零 ticket provider 违约进入 recovery-required，证明 deinit/UNINITIALIZED 才使旧 ref 失效并释放借用。有限 completion queue 不负责认证设备 settlement，也不启动默认 worker。

## ADR 005 通用组件 core 与 adapter 分开

状态：采用。

Log formatter core 无 HAL/OSAL；Log runtime 明确依赖 OSAL，UART sink 由应用选入。Shell core 与 UART adapter 分开，mock backend 属 tests。Config core/RAM/Flash、Storage core/typed HAL adapter 和协议核心均独立装配；组件不默认创建任务、命令表或产品分区。

工业产品、virtual plant、客户设备管理和测试应用在 examples/产品仓库。Nexus 可保留平台无关协议和适配契约，不内置领域控制业务。CAN/CANopen、Ethernet/MQTT、额外 MCU 或 RTOS 的投入需具体需求、维护库许可和回归预算，不能挤掉 P0 生命周期与 BSP 修复。

## ADR 006 发布使用同一批验证产物

状态：采用；工具与模型已实现，正式候选/promotion 未自动完成。

source commit/dependencies、effective config、Board/layout SHA、工具链与 ELF/map/bin/hex 必须一致。必需 suite 有真实非零执行；零测试、全部 skip、旧/空报告、dirty source、错 SHA/缺依赖/错误 linker/篡改工件都拒绝。候选完成后正式发布复用同工件摘要，不重建另一组镜像。

平台软件候选标注 physical qualification 未执行；产品 promotion 还需精确 PCB、真实 HIL、测量预算、trust/signing、许可/安全与制造审查。模型与临时公钥验签不代表企业签名服务或真实产线资格。

## ADR 007 全物理 Flash 与外部分区政策

状态：采用，typed Flash/region/StorageHAL 已实施并做软件故障回归。

SoC `FLASH0` 暴露整个物理芯片与真实密度：F407VE 8 sector、VG/ZG 12 sector，F470ZG 256 个 4 KiB page。HAL region 检查 owner/generation、权限、溢出、物理 block 和可写重叠。SoC 不链接 Storage，也不默认预留 product storage。

外部 `NEXUS_FLASH_LAYOUT_FILE` 生成 linker/region declarations。默认 whole-Flash image 且 regions 为空。相同解析拒绝重叠、越界、非完整 erase block、image 侵占；layout 和 Board 摘要各由八个绝对 ELF symbol 绑定。首次仅支持 image 起于物理 Flash 基址；没有 bootloader/VTOR relocation，非零 offset 拒绝。

StorageHAL 借用已开的外部 region，验证 uniform 完整 block、0xff/编程几何并共享整次 storage 操作的有限 budget，不选择分区或 auto unlock。Storage 是 dual-bank 完整快照；不是高频 journal/wear leveling。软件故障模型的原/新完整状态保证不等于物理掉电、电压、暂停时序与寿命资格。

## ADR 008 目标拥有源码消费上下文

状态：采用。

源码根和 build root 使用 Nexus 自身路径；`Nexus::Config` 保存选定 target、有效配置和输出上下文。`nexus_add_application()` 在父作用域读取该上下文，仅改变本目标输出/链接，不污染父工程 sentinel 或全局路径。`Nexus::Firmware` 显式注入 startup、controller/Board objects、runtime 与 typed HAL。

CMake/CTest/preset 是权威构建模型。Python CLI 和 shell/PowerShell wrappers 只编排同一命令及退出码；setup 检查本机依赖或明确初始化 pinned dependencies，不维护另一套配置 DSL。Windows/macOS wrappers 未在本 Linux session 执行。

## ADR 009 板卡通过单一路径和窄资源接入

状态：采用；校验范围为明确维护路由。

`NEXUS_BOARD_DIR` 选择一个外部 package；manifest binds schema、SoC/HSE、CMake/interface/object targets、全部声明 inputs 和资源。源码必须在 package 内并被摘要覆盖；enabled controller 的 pin/AF/clock/IRQ/DMA 与 reviewed route 一致，冲突拒绝。Board 传只读 Nexus 资源，不能持有/修改 mutable driver instance。

这是已维护 GPIO/UART/SPI 路由的 consistency validation，不能称全芯片自动拓扑求解。增加新 peripheral/alternate route 需要真实 provider、SDK 复核、负配置与 cross-build；物理 PCB/电气仍由 HIL 单独资格确认。SDK header/macros 留在实现目标私有编译面。

## ADR 010 普通 typed consumer 与 provider 内部契约分离

状态：采用。

`hal/base/nx_device.h` 对普通应用暴露 opaque descriptor、只读 metadata 和 owner/generation 值引用；发现不 init，open/close/recover 明确错误。mutable descriptor/state/registration 使用显式 `hal/provider/nx_device_provider.h`，provider target PRIVATE link `Nexus::HALProviders`。

HALCore、每类 facade、HALSupport、deadline runtime/OSAL binding 独立链接。UART ticket、SPI/I2C parent-child、Flash region/borrow 各自定义真实 settlement，无无能力同步/异步转换占位。Native typed I2C 已实现；MCU modern hardware I2C 当前 unsupported，不从 legacy getter 推导支持。

## ADR 011 后端能力、静态资源和启动上下文明示

状态：采用；真实软件后端检查已执行，ARM port 时序未上板。

OSAL 查询声明 backend capability、资源限制与累计不可重定向 token 预算。FreeRTOS 六类对象使用静态 kernel/control/payload/stack 预算；固定 slot 可复用，lifetime identity 不复用，耗尽明确失败。OSAL heap seal 不代表 SDK/libc 或全固件无 malloc。

Arch saved PRIMASK 与 FreeRTOS BASEPRI/syscall 临界区不互换。调度前拒绝 blocking/FromISR；适配器保留 incoming mask，避免构造静态对象意外阻断启动期 HAL tick。外部应用显式检查 worker create/`osal_start()`，需要调度的业务在 worker 中运行；裸机拥有自己的 loop/pump，未实现能力明确拒绝。

## ADR 012 固定官方依赖与可重定位源码包

状态：采用；历史5498b2b已完成严格clean包与真实relocated消费；后续源码必须重新绑定包与消费报告。

CMSIS/ST HAL/FreeRTOS 等锁定真实 Gitlink，GD32 3.3.3 使用 reviewed official import 的下载/archive/逐文件摘要和许可证，不伪造 Gitcommit。ARM GNU14.3.rel1 archive hash 固定；配置和打包拒绝漂移/缺失/额外/symlink。

`nexus_package_source_sdk()`/prepare script 导出源码、必要依赖、许可、relative CMakeConfig/Version。manifest binds source commit/tree、files/deps/import/toolchain、snapshot 和两个 export 摘要；验证不误取 consumer ancestor Git。find_package 要求精确版本，并可固定完整 source revision。

发布默认拒 dirty/未知 source；development fixture 只有显式 opt-in 可消费，`publishable=false`。package prepare 与 consumer 重新编译真实 startup/linker；工具链外部提供。没有 installed binarySDK、签名分发或跨配置 ABI 承诺。每个交付的 clean package、verify 与消费者矩阵在对应源码提交后完成并记录，不能继承旧包摘要。

## ADR 013 目录、构建责任与 maintained 能力统一

状态：采用；新源码的软件执行以 external pair 记录为准。

SoC family owns controllers、clock/IRQ/system、Flash/identity、private 头、SDK source selection 和 linker sections，platforms 只 owns startup/lifecycle/object assembly。目录为 `soc/stm32f407`、`soc/gd32f470`、`soc/native`；Native 是虚拟主机模型。精确 VE/VG/ZG/F470ZG 料号、密度和 Board/layout 身份保持独立。统一 `nexus_forward_component_objects()` 转发显式 OBJECT targets，避免每个平台复制 target-property 装配逻辑，并保留 startup/registration/强 IRQ。

Kconfig 可选平台只有 Native、F407VE/VG/ZG、F470ZG。不提供仅有壳/SDK的其他 STM32、GD32F407、ESP32、nRF52 选项；未实现 MCU ADC/DAC/timer/I2C/EXTI、虚假 VTOR/CCM/LL 开关不进入维护配置。UART DMA 显式 y 拒绝。STM SDK 编译7个必要基础 source与选中 UART/SPI，vendor DMA source 可能为 HAL 链接依赖，不表示 UART DMA 已可用；GD SDK 同样选择必要基础与 USART/SPI。完整固定 vendor import 的身份校验不因少编译几个 source 而省略。

公开支持与被测源码的身份权威位于 external examples 的固定 gitlink/lock 和 source-pair validation reports。本库文档记录 contract/known limits 与 delivery status，历史快照明确保留原 source，避免自身 SHA 自递归。source/config 改变需要 fresh actual suite/consumer，candidate CI 不修改正式 dependency pin，不冒充产品 promotion。

## ADR 014 分开只读拒绝与实际 cleanup 失败

状态：采用；当前源码的软件故障回归单独验收。

HAL 与 Runtime 显式 OFFLINE/PARTIAL/READY，失败 init 真实 cleanup 后保留 original 与 cleanup status。HAL 关闭取得独占 token fence，provider/IRQ/DMA 的只读 preflight 失败不改变 READY。实际 mutating cleanup 失败保留 fence 和 HALownership，禁止新open/construct/register/IRQ-DMA admission，原owner仍可settle/close/recover；cleanupretry成功才 OFFLINE。

Runtime 先释放 OSAL 后 HAL，仅在 HAL 仍 READY 时尝试 OSALrestore，防止恢复到已停止 clock 的 PARTIAL平台。idle baremetal或FreeRTOS调度前MCU有真实时钟/IRQ/timebase/vendorcleanup与reinit；运行或suspendedkernel仍BUSY。直接SDK资源及产品safe-output由外部caller先停稳。ST peripheralbankreset会改变外设状态，重建窄Board初值不等于执行器电平连续性保证。Nexus不提供全kernel/产品热重启。

## ADR 015 第一性原理下一代设计

状态：PROPOSED，待实施和资格验证；不覆盖 ADR 001–014 的当前实现状态。

用户新增约束：代码格式、命名与注释风格保持当前仓库规范；沿用.clang-format/.editorconfig、贡献指南和反斜杠Doxygen模板，包括现有文件头。架构破坏性重构不授权另起格式口径。

用户要求从性能、空间、工程管理、自动化、代码规范、设备接入与扩展重新设计，并明确现有设计不构成约束。[下一代架构蓝图](../design/next-generation-platform.md) 定义默认编译期静态资源规划、固定 typed 端口、普通 C 静态绑定、caller-owned 请求和真实执行状态。默认不设置通用运行时设备注册中心、逐 GPIO owner/ref/generation、强制统一 OSAL、隐藏 worker 或多处重复事务状态；动态撤销/复用等能力需要独立明确合同。

首版配置采用 SoC、Board、外部 assembly 三类输入到单一 resolved 结果，CMake 保持源码与依赖权威；Kconfig 不作为必须保留的前提。编译期资源检查不能证明任意 C 调用遵守 owner，也不能替代实板资格。异步 buffer 借用、取消/timeout 与 settlement 分离、IRQ/DMA drain 和失败保留责任属于必需合同。

配套 [工程手册](../design/engineering-handbook.md)、[接入契约](../design/integration-contracts.md) 和 [33 项执行清单](../design/next-generation-execution.csv) 均待实施。先用垂直切片验证，再同步迁移实现、测试、文档与外部 examples；当前代码、支持矩阵、历史验证和发布资格不会因新增提案而改变。

## 继续演进的前置条件

用户当前选择先软件和 HIL 工装。新增 MCU/复杂拓扑、cache/MPU、完整热重启、非零 imageoffset/bootloader、安全 vault/entropy、控制 worst-load 和企业 LTS 都需要独立实现与证据。实板预算、PCBrevision、RS485 收发器、掉电工装、产品信任/制造和成员主备由外部产品与团队定义，不以计划、接口或 null 预算冒充能力。
