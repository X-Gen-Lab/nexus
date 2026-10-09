# 接入、资源装配与源码 SDK 合同

> **PROPOSED — 未实现的设计提案。** 本文中的 schema、命令、生成物和 API 是拟议合同，不代表当前源码已实现，也不构成三块开发板的硬件支持或产品发布承诺。

本文说明通用平台如何接入 CPU、SoC、Board、外部器件、协议和组件，以及外部工程如何形成可审核的固件。总体边界见 [下一代平台设计](next-generation-platform.md)，开发、验证和维护流程见 [工程手册](engineering-handbook.md)。

首发范围是 STM32F407ZGT6 启明 V3.1、STM32F407VET6 天空星青春版、GD32F470ZGT6 梁山派，以及裸机和 FreeRTOS 两种后端。现在不连接硬件；源资料、软件实现、编译链接、模型行为和物理资格必须分开陈述。未知的实际 PCB revision、跳线、电气或器件型号保持未知。

## 1. 第一原则与工具边界

接入不是把所有硬件描述转换为一个运行时设备树。它是提交有限事实、选择已维护能力、检查资源，再显式装配静态对象。

- 使用 C11、CMake、Ninja、版本化 JSON 和一个窄 Python 校验/生成工具。核心默认不需要 heap、运行时字符串查找、自动探测或后台 worker。
- 默认入口是外部工程的 `assembly.json`。硬件输入分为 SoC、Board、assembly 三类，解析后只有一份权威 `resolved.json`。
- CMake 仍是代码目标和代码依赖的权威。生成器不生成组件依赖图、业务线程、产品 main 或调度策略。
- Kconfig 可以以后提供软件选项界面，导出到同一输入合同。它不能与 JSON、CMake cache 各自控制同一个选项，也不能覆盖 SoC/Board 的事实。
- 目录可采用 `core/ arch/ soc/ io/ os/ components/ tools/ tests/`；Board 可位于独立 package。层名说明职责，不要求每个层、端口或对象成为一个独立 translation unit。
- 未维护的能力不因 SDK 有宏、路由表有 AF 值、同系列有示例而自动可用。新路由、新 DMA 模式、新控制器分别接入。

## 2. 各类接入拥有的事实

| 类别 | 拥有的事实和实现 | 明确不拥有 |
|---|---|---|
| CPU / Arch | 指令集、异常上下文、屏障、原子/临界区、FPU 上下文和 ABI 约定 | 具体 PCB、具体控制器 IRQ/DMA、产品设备 |
| SoC | 精确料号/封装/密度、内存域、控制器、时钟约束、IRQ/EXTI、DMA、pin/AF 路由、Flash 几何、私有 SDK | Board 接线、应用 role、产品分区 |
| Board | PCB/source revision、晶振、电气电平、焊接器件、连接器、跳线条件、初始安全电平和复核接线 | 改写芯片密度、选主传感器、创建业务任务 |
| 外部器件 driver | 器件命令/寄存器、转换算法、初始化顺序、器件时序要求和错误语义 | 默认 GPIO、默认总线编号、产品采样策略 |
| protocol core | 帧格式、解析/编码、有界状态机和协议时间规则 | 默认 UART、RS485 DE 引脚、线程、客户业务 |
| component | 可复用算法或服务、窄端口、明确容量和执行上下文 | 隐式创建 worker、自动找依赖、固定产品存储 |
| product / application | main、workers、应用 role、私有协议、配置策略、分区、升级/健康/制造策略 | 让通用平台反向依赖其源码 |

SoC provider 实现公共 IO/核心合同，并使用 Arch。器件 driver 使用总线、GPIO、时间等窄端口；协议 core 不依赖 Board 或 OS；组件 adapter 才连接 IO/OS。外部 assembly 连接各实现。Board 声明板载器件的事实，不因此成为器件 driver 的所有者。

厂商 SDK、内核实现头和可变 controller 状态保持私有；公共接口不透出其结构体。STM32 与 GD32 的相似名称不能证明寄存器兼容或驱动可以直接复用。

## 3. 三类输入及少量文件模板

### SoC package

一个维护中的 SoC package 通常包含：

```text
soc/<family>/
  soc.json                 # 精确 SKU、内存、时钟与控制器事实
  routes.json              # 已复核路由及模式的有限 allowlist
  CMakeLists.txt           # 私有 SDK/provider 的显式目标依赖
  private/                 # 私有 SDK 接口与硬件实现
  ...                      # startup、controller、clock/IRQ、linker 实现
```

family 目录可以复用代码，`soc.json` 仍分别定义 STM32F407VET6、STM32F407ZGT6、GD32F470ZGT6。型号、封装、Flash、SRAM 域和 IRQ 表不能被一个家族字符串代替。新增型号不等于已有所有 controller 都支持该型号。

`routes.json` 每条记录至少绑定精确 SKU、controller、pin/AF、clock domain、IRQ、允许传输模式、允许 DMA route 和来源。可复用路由记录，但必须限定适用 SKU/封装。源资料复核与实板资格采用不同字段。

### Board package

已经有 SoC 实现时，新 Board 默认只需要：

```text
boards/<board-id>/
  board.json               # 实际晶振、接线、电气、revision、资料来源
  README.md                # 跳线/连接器与已知限制
  board.c                  # 可选：特殊电源/复位/安全初值动作
```

Board 不复制 SDK、HAL、RTOS port 或链接器脚本。普通 GPIO 初始电平与 pin binding 可直接表达为数据；只有无法用已维护数据合同表达的有界电气动作才增加 `board.c`。这种代码仍须通过中性硬件端口工作。

下面是字段示意，`example-board` 并非声称存在或已经维护的 Board：

```json
{
  "schema_version": 1,
  "id": "example-board",
  "soc": "STM32F407ZGT6",
  "source_revision": "document-revision",
  "physical_pcb_revision": null,
  "clocks": {"hse_hz": 8000000},
  "bindings": [],
  "reserved_resources": [],
  "unknowns": ["physical PCB identity and connector wiring pending"],
  "provenance": []
}
```

模板中的晶振数值必须换成该 Board 的资料事实。空 `bindings` 不提供 UART/SPI/I2C/RS485；把真实 wiring 加入其中之前，须核实连接器、引脚、电平、跳线与 SoC reviewed route。Board 引用 SoC 的内存事实，不再独立维护一份可漂移的 RAM/Flash 大小。

### 外部 assembly

外部 `nexus-examples` 或私有产品工程提供 `assembly.json`，选择一个 Board package、OS 后端、时钟 profile、需要的资源和器件实例。以下最小示意没有声明任何已可用的外设路线：

```json
{
  "schema_version": 1,
  "board_package": "./boards/example-board",
  "backend": "baremetal",
  "clock_profile": "selected-profile-id",
  "controllers": [],
  "devices": [],
  "memory_budgets": {"main_stack_bytes": 4096},
  "layout": null
}
```

`selected-profile-id` 是需替换的示意值；未知 profile 必须拒绝。`controllers` 只选择 Board 已声明且 SoC 已维护的 binding，明确 polling/IRQ/DMA 模式、必要 buffer 容量和 IRQ priority。`devices` 绑定总线 child、CS/address、器件型号与固定容量，不生成任何 worker。

没有外部产品 layout 时，镜像只能采用已维护的全物理 Flash 默认布局，平台不预留“通用产品存储”。产品 layout 若被选择，其完整内容成为 assembly 的 declared input，由同一解析路径进入 resolved IR。

## 4. 解析、生成与 CMake 装配

配置过程必须按固定顺序执行：

1. 读取 assembly，定位唯一 Board，按其精确料号定位 SoC；检查 schema、版本和依赖锁。
2. 校验 package containment、声明 inputs、资料中的已知/未知条件；展开被选中的 binding 与器件。
3. 建立资源占用表，检查模式能力、冲突、clock、IRQ、memory、ABI 和 layout。
4. 原子写入单份 `resolved.json`，记录所有决定、拒绝项和输入摘要。
5. 从该 IR 导出硬件配置头、只读资源表及 linker 输入；CMake 读取必要选项并按自己声明的目标依赖编译。
6. 对实际 ELF/map/BIN 验证 vector、startup、handler、区域容量和产物身份；结果成为证据，不能反向冒充硬件资格。

`resolved.json` 保存 schema、选中 package/料号/后端、有效选项、clock/memory/ABI、资源 claims、已选器件、证据限制和输入 hash。它不是 CMake target 依赖图，也不包含平台猜测的业务线程、role 或领域策略。

派生输出都来自该 IR。不得存在 `.config`、旧生成头、独立 Board 默认宏等回退真相。生成失败使本次配置失效，不得继续使用上次成功生成的 bundle；先写临时目录，完整成功后切换。不同 Board/backend/profile 使用独立 build root。

CMake 的每个库显式声明自己的代码依赖、公共头与私有 SDK 编译面。assembly 选择硬件实例和模式，不能替代 `target_link_libraries()`。生成器不扫描任意业务源码推断需要什么组件，也不自动链接所有 driver。若代码目标要求的能力与 resolved 配置矛盾，配置应失败。

静态硬件 descriptors 可以由工具生成；可变上下文和真正的构造/init 仍有显式所属实现。产品 main 负责所需组件和器件的初始化顺序以及调度启动。平台不因一个器件节点出现就创建采样任务。

## 5. 资源占用与编译前检查

| 资源 | 必须检查的条件 |
|---|---|
| pin | 精确封装具有该 pin；route 在 reviewed allowlist；AF/方向/电气约束满足；SWD、板载器件与选中功能不冲突 |
| controller | provider 实现所选模式；物理 controller 单一拥有；共享总线由同一个 owner 派生 child |
| SPI child | CS 独立；mode/频率/word width 可在已维护配置中满足；串行仲裁明确 |
| I2C child | 地址和 mux 层级合法；地址冲突拒绝；controller 与 mux 模式均已实现 |
| DMA | 按实际 DMA controller/stream/channel/request 模型检查；所选 route 合法；独占资源互斥；模式已有实现 |
| IRQ | 每个 vector 有唯一 provider 或显式有界 dispatcher；优先级、grouping、OS 调用权限吻合 |
| EXTI | 检查 line/port mux 独占、边沿模式、共享 vector、事件容量和优先级，不能只看 pin 名 |
| timer/PWM | 检查 timer/channel、timebase、共享 PSC/ARR/base mode 和 trigger；同 timer 的 channel 不视为独立时基 |
| ADC | 检查 channel、sample time、sequence、trigger、参考电压条件和具体采样模式；DMA/stream 单独维护 |
| watchdog | 检查已维护 IWDG 范围、LSI 误差及启用不可逆效果；feed/健康政策仍归外部产品 |
| clock | 晶振、PLL、总线、Flash wait state、外设频率和后端 timebase 约束满足已维护 profile |
| memory | 精确物理域、DMA 可达性、alignment、栈/池/队列预算、layout/erase block 边界和镜像容量 |
| ABI | CPU/Thumb/FPU/float ABI 一致；FreeRTOS port 正确；精确型号宏来自有效配置 |

resource claim 必须指出实体资源键、owner、使用模式和来源。错误例如“DMA1 stream X 同时由 A 和 B 声明”，而不是只报“配置不合法”。工具只检查显式选择，不自动寻找另一个 DMA stream、改 pin 或降低 clock。

SPI/I2C bus 共享不等于重复声明 controller。总线由单一 provider 管理，child 只持有其有界引用；SPI CS 或 I2C 同层地址重复要拒绝。直接单执行者调用与显式queue/service adapter分别声明：共享路径只有adapter执行者推进controller，client等待不接管执行。短锁只保护提交/控制元数据，不跨DMA生命周期持有task mutex，器件driver不隐式依赖kernel锁。

真实共享 IRQ 允许使用 SoC 声明的静态 dispatcher，检查 handler 清单和有界执行条件；不允许两个强符号争抢 vector，也不引入任意运行时注册。IRQ body 的响应/结算与 task/worker 的后续处理分别有所属者。

FreeRTOS 保留 SysTick/PendSV/SVC 等被其 port 使用的资源。IRQ priority grouping 统一配置；调用内核的 ISR 必须满足该 port 的 syscall priority 规则，高优先级 ISR 禁止使用相应 OS API。备用 timebase 若占用 timer，也进入同一资源表。

### clock、memory 与 ABI 的具体边界

Board 提供实际 HSE/LSE、电气条件和必要跳线事实；SoC 提供合法约束与已维护 profile；assembly 选择 profile。controller 不独立重配全局 clock，driver 不重复设置 NVIC grouping。没有实现动态 clock 切换时，运行期要求切换直接报告不支持。

各 SRAM 域分别声明地址、大小、初始化方式、DMA 可达性和用途限制。不能把 STM32 CCM 当作默认 DMA SRAM，也不能把全部物理 SRAM 合成一个未经维护的 linker heap。初始阶段只启用已实现且已检查的域，其余域保持未维护。

静态 DMA buffer 可用 linker section、alignment 和 map 检查；调用者动态传入的指针仍须在执行时验证可达域与对齐。编译检查不能替代 borrow/settlement 生命周期。内存预算区分 MSP、任务栈、OS 对象、IO buffer、组件 workspace 和可选 libc heap。

Flash 布局校验使用精确料号的 erase geometry，拒绝越界、重叠与不满足擦除边界的 region。镜像 offset、VTOR、bootloader/A-B 必须同时有维护实现，不能只改变链接地址就作为支持。

CPU 编译选项、FPU 启用、异常上下文和 kernel port 必须一致。工具链及所有预编译输入的 ABI 记录到身份中。当前推荐源码 SDK；不默认承诺不同 compiler/version/flags 之间的 installed binary ABI。

### 可由哪些阶段证明

生成前检查负责事实一致性和资源冲突；C11 `_Static_assert` 与类型系统负责容量、类型、常量关系；linker/map/ELF 检查负责镜像布局、符号、handler 和对象保留。物理波形、电气电平、clock 偏差、IRQ latency、DMA 终态和长期负载需要实板证据。

## 6. 实例名、应用 role 与静态成本

`USART1` 是 SoC controller 名；`j1_uart`、`bme280_ext0` 是 assembly 的稳定实例名；`console`、`ambient_sensor` 是外部应用 role。Board 只提供物理资源，不决定哪个器件是产品的主传感器。

role 映射和约束属于外部 consumer。它可在自己的构建步骤中读取 resolved 信息，检查 role 所需接口并生成类型化 alias 或访问器；平台不自动生成产品 role 政策。更换 physical binding 后，应用继续引用 role，而不是在业务源码里寻找具体 UART/GPIO 编号。

普通数据路径不需要运行时字符串查找。只读 wiring 放 Flash；必要 context、pool、queue 和 buffer 静态分配。role alias 可编译为直接符号，通用 driver 只在需要窄端口时保留一层函数指针。不要为每个层名建立一层对象或虚调用。

未选中的 controller、SDK source、driver 和 adapter 不编译或不链接进入镜像。适当使用 function/data sections 与链接器 GC；startup/vector/显式 handler 等必要对象的保留规则由 CMake/linker 明确维护，不能依赖 accidental whole-archive。

## 7. 新 Board 接入流程

1. 收集精确 MCU、PCB/document revision、原理图/BOM来源、晶振、电源、SWD、连接器、跳线和默认电平。未知的实际 PCB revision 保持 null。
2. 确认 SoC 已有维护实现，选择最小资源，例如 LED 加一条 UART。逐条核对 route，不默认覆盖全部连接器。
3. 编写 `board.json`、README；特殊电源/复位动作才增加中性 Board hook。标明已复核来源与未执行的物理项。
4. 在外部仓库创建 `assembly.json` 和显式 main，先选择裸机，再构建实际 ARM ELF/map/BIN。检查资源计划、时钟、内存、ABI、vector 和 handler。
5. 为同一资源创建 FreeRTOS assembly，检查 port、timebase、IRQ priority 和静态预算；两个 backend 使用独立 build root。
6. 在支持矩阵记录精确 tuple 和实际软件证据。没有接实板时，物理初始化、电气、IRQ/DMA 与长期运行全部保持未执行。

首发三板的 MCU I2C、SPI route/模式及 RS485 收发器/DE/RE 都须逐项查明维护实现和接线条件，不能从 Board 名推导可用。现有启明资料中的 SPI disabled、RS485 条件未核实、实际 PCB revision 未记录等限制，不会被本提案自动解除。

## 8. 新 SoC 或精确 SKU 接入流程

1. 明确是已有 SoC 的新封装/密度，还是新的寄存器/clock/IRQ/DMA实现；不要仅按市场家族名分类。
2. 锁定官方资料、勘误、SDK revision、许可和 import hash，记录精确 part/package/density。
3. 定义 `soc.json` 的物理 memory、Flash geometry、clock 约束、IRQ 和 controller；最初只放入准备维护的 reviewed routes。
4. 实现 Arch 所需适配、reset/startup、clock、timebase、IRQ、linker 和最小 IO provider。SDK 目标保持 PRIVATE，不暴露厂商结构体给外部 consumer。
5. 先建立最小 ELF 链接 fixture，再完成裸机和 FreeRTOS port/config 的实际编译链接；对错误、资源终态和取消等有行为的路径建立必要模型证据。
6. 新 controller/route/DMA 模式分别扩大矩阵。SDK 中存在但尚无 provider 的功能不成为可选维护能力。
7. 没有实板资格时，交付状态只到实际完成的软件项。不能由相近芯片的结果继承物理支持。

## 9. 新外部器件 driver 接入流程

以 BME280 SPI driver 为例；例子不声称三块首发 Board 已存在 reviewed SPI 接线。

1. 确认所用 Board 已提供 reviewed SPI binding、CS 和电气条件。没有时，先独立完成 SoC route/controller 与 Board wiring 接入。
2. 创建器件 core：公共头、实现和 CMake；定义有界 context 与窄 transaction/time 端口。器件寄存器、chip-ID、校准解析和转换算法归 driver。
3. 声明必需 bus mode/word width/频率、初始化时序、ISR/task 限制、timeout 起点、buffer 借用、失败后的状态和 recovery 条件。
4. 外部 assembly 创建 `bme280_ext0`，绑定 bus child/CS 和固定容量；CMake 显式链接 driver 和其 adapter。
5. consumer 自己映射 `ambient_sensor` role。裸机 main 周期调用，FreeRTOS product worker 调用同一 driver，平台不自动创建任务。
6. 用 fake port 验证解析、错误响应、deadline 与 ownership 等行为；实际 ARM 链接验证接口、容量和装配。chip-ID 检查属于实例初始化，不是自动扫描设备。
7. 真实接线、波形、传感误差和长期负载单独资格化。没有硬件时，只记录实际完成的软件证据。

器件 driver 不把 Flash 总线存储自动称为产品 Storage，也不自动分配产品 region；例如 W25Q 还需要实际 fitted part、容量、erase geometry 和外部布局策略。

## 10. 新 protocol / component 接入流程

1. 判断是否通用且值得平台维护；客户私有协议、产品流程和领域控制留在外部仓库。
2. 定义无 Board/SDK 依赖的 core，固定最大帧长/节点数/工作区及失败语义。协议 parser 不能隐式打开 UART 或创建 task。
3. 用窄 transport/time/storage 等端口连接环境，必要 adapter 独立可选；CMake 明确其代码依赖。
4. external assembly 只选择 transport 的硬件资源，产品显式配置协议 role、调度与 worker。
5. 验证用户可见协议行为、无效输入、边界和资源耗尽，建立真实固件链接。电气和帧 timing 资格仍需物理证据。

RS485 特别需要 Board/外部 wiring 说明收发器、电平、DE/RE、隔离和终端条件；SoC UART provider 负责已维护传输终态；协议/adapter 明确何时切换方向。不能用软件 delay 或 generic UART 成功返回代替真实 TC 和线路资格。

## 11. 外部源码 SDK 消费与升级

一个外部工程通常只需：

```text
nexus-examples/<application>/
  CMakeLists.txt
  CMakePresets.json
  assembly.json
  dependencies.lock.json
  main.c
  ...                      # 产品自己的 role、worker、layout 和策略
```

消费流程是固定 source SDK/commit 和依赖 lock，完成 package 校验，用其公开 CMake 入口接入，再在外部目标显式链接平台能力与组件。CMake preset 选择工具链/build mode，assembly 选择硬件与后端，两个输入职责不重复。

SDK 准备和消费必须使用完整、固定依赖的源码；缺失 submodule 或 snapshot 不能默认为可发布 SDK。source package 需要清单、许可证、相对 CMake 配置及输入身份，移动后进行实际 consumer 验证。路径可重定位不是 ABI 兼容证明。

升级 PR 同时修改平台 pin、调用者和需要的 assembly/schema。破坏性合同升级明确版本和迁移说明，重新执行精确支持 tuple 的软件回归；不把旧版本结果继承到新 source/config。正式产品 promotion 使用已审核的同一 artifact digest，不能默默重建另一份镜像。

bootloader、A/B、签名、回滚、制造与现场升级由外部产品拥有。layout、image offset 或持久化格式变更需要产品迁移/恢复合同及对应资格；源码 SDK 更新不会自动替代这些工作。

## 12. 版本、路径、摘要与产物身份

- SoC/Board/assembly/resolved 分别有明确 schema version；未知 version、未知字段或矛盾值拒绝，不静默忽略。确需扩展时使用受控 namespace，不能隐藏有效编译选项。
- package 记录 API/contract version、精确依赖 revision 和来源；schema 兼容不等于硬件或行为兼容。
- 相对路径以拥有该 manifest 的目录为基准。Board source 必须落在自己的 package 内；拒绝 `..`、symlink 逃逸和未声明 source。SDK 自身路径不能误用 consumer ancestor 的 Git 身份。
- 所有影响构建的输入均声明并 hash，包括 route 数据、Board hooks、layout、vendor import、代码依赖 lock、工具链和编译选项。hash 原始字节用于输入身份，规范化 resolved JSON 用于有效配置身份；算法和格式有版本。
- 解析或构建过程中不偷偷联网更新依赖。目录内新增但未声明的有效 source 必须被拒绝或重新加入清单，不能绕过身份校验。
- 外部产品身份与平台身份分开。平台提供料号/Board/config 等中性信息，不拥有产品名、SKU 或制造政策。
- 产物清单绑定平台及 consumer revision、依赖、工具链、SoC/Board/assembly/resolved/layout hash，以及 ELF/BIN/map digest 和证据范围。
- dirty 开发构建可显式允许，但标记 `publishable=false`；缺输入、hash 不一致、旧证据或 snapshot 不能升级为发布资格。

## 13. 支持 tuple 与支持等级

支持最小单元为：

```text
<platform source, consumer source, exact MCU part/package,
 Board source revision, physical PCB revision or unknown,
 route, transfer mode, OS backend, resolved config hash,
 toolchain/ABI, layout hash>
```

Board 名只是检索入口，不是整板所有外设都受支持的承诺。一个 UART IRQ tuple 的结果不能证明同 Board 的 SPI DMA、I2C、RS485或另一个 backend。

| 记录项 | 可证明范围 | 不证明 |
|---|---|---|
| D：描述复核 | 指定资料中的料号、接线、route 已审阅 | 真实 fitted part、PCB、线路可用 |
| C：编译/链接 | 指定工具链下真实 ARM ELF、布局和装配通过 | 硬件已运行、时序正确 |
| M：模型行为 | 指定 source/config 的 host/fake 行为通过 | 电气、IRQ/DMA 实际终态 |
| H：物理路径 | 指定 PCB、route、mode 的真实测量 | 任意负载、其他外设或整产品资格 |
| Q：产品负载资格 | 指定 workload、预算和产品策略下验收 | 其他产品、无限支持期限或 LTS |

这些是分列证据维度，而不是以模型结果替代编译或硬件的单一等级。C/M/H/Q 缺项必须明确写未执行；日志、工件、source/config 和预算绑定到同一个 tuple。

首阶段可建立三板 × 两后端的六个基础软件装配组合，但每个组合只能声明实际选入并执行的路径。I2C/SPI/RS485 和 DMA 按维护事实扩列。没有连接硬件时 H/Q 为空；角色名单、workflow 和 fixture 不填补这一缺口。

10 人、3–6 个月优先完成窄 schema、六个基础 firmware 组合、确有需要的 controller/driver 和可审核工件。完整 pin 数据库、全 SDK feature、自动拓扑求解和运行时探测不作为首阶段退出条件。
