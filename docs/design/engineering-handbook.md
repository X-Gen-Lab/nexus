# 下一代平台工程运行手册

> 工程合同与风险门禁。当前可运行入口、逐项实现和实际证据见[交付状态](../delivery/README.md)。本手册规定团队责任和验收边界；示意 schema/API 不替代源码。OCI 准备、正式离线构建、软件候选及实板资格分别验收。

平台总体设计见 [next-generation-platform.md](next-generation-platform.md)，外部产品、Board 和组件接入契约见 [integration-contracts.md](integration-contracts.md)。本文负责把这些边界转换为约十人团队可以执行、审查和持续维护的工程规则；接口与目录的最终定义以上述设计文档为准。

## 1. 工程目标与完成的含义

工程系统要回答四件事：选择了什么、实现是否符合合同、实际资源与时序是否满足产品要求、交付的是否为验证过的那份产物。工具数量、测试条数、文档页数和流水线绿灯数量都不是目标。

首版遵循以下原则：

- 一个默认配置入口，一个有效配置结果，一个软件依赖图；不同工具不能分别解释同一选择。
- 标准 C、CMake 和测试工具完成各自职责；自建工具只补足配置语义、硬件资源校验与证据绑定。
- 平台提供共同机制与诚实能力，产品拥有业务 worker、协议、分区、更新、健康和制造策略。
- 编译、主机模型、真实内核运行、ARM 链接、物理测量和发布资格分别记录，互相不能替代。
- 优先使失败路径、资源边界和并发合同可验证，再优化性能。不能通过删掉所有权或结清条件换取纸面速度。
- 每个阶段有可执行退出条件。计划完成、实现完成、软件验收、物理验收与发布资格是不同状态。

当前用户延期物理设备操作。新工程模型可以先完成软件与 HIL 工具准备；物理阶段继续记录为未执行，直到获得真实站点和实际执行结果。

## 2. 目录、语言与依赖边界

建议生产代码按下列职责组织。表格是目录约定，不是另一个自动加载或自动启动框架。

| 目录 | 工程职责 |
| --- | --- |
| `core/` | 小型共同类型、状态与基础合同，以及有边界的基础设施启动；不容纳产品主循环 |
| `arch/` | CPU 本地中断状态、屏障、必要原子原语与可选计时基础 |
| `soc/` | 精确 silicon/variant、clock、controller、IRQ/DMA 与私有 vendor SDK 集成 |
| `io/` | typed I/O 合同、明确的实现接入面与必要可移植适配 |
| `os/` | 有边界的 OS/backend 集成；应用 task 和调度策略留给产品 |
| `components/` | 可独立选择、测试和链接的共同组件 |
| `tools/` | 配置解析、构建入口、产物检查、证据和打包工具 |
| `tests/` | 主机契约、故障模型、链接 fixture、外部 consumer 与 HIL 工具测试 |

产品和 Board 可以是外部 package。公共示例展示产品组装，不成为平台内部必须链接的业务模块。维护中的 SoC 和 Board 使用明确的 target，不通过扫描目录、名称匹配或隐式默认平台决定实现。

### 2.1 C11 生产代码与 C++ 测试隔离

首版生产模块使用 C11；必要的 startup/CPU 原语使用受控汇编。生产目标不因为构建测试而获得 C++ runtime、异常、RTTI 或额外 STL 依赖。平台 public C header 必须自足，并提供需要的 C++ linkage 声明，使外部 C++ 产品可以正常消费 C API。

主机 C API、组件和 provider 的新增行为测试统一使用 C++17、GoogleTest 和 GoogleMock。测试 helper、fault injection、host fixture 与 mock SDK 仅进入测试 target，不能通过目录全局 include/define 混入生产编译。Python 工具使用 stdlib unittest，不要求嵌入式工程采用 Python runtime。历史 C 模型测试作为已有回归继续执行，新行为按下述 TDD 约束扩展。

至少验证三个消费边界：纯 C public header consumer、C++ public header consumer、真实外部 product consumer。测试打开与关闭时，生产 target 的接口和资源合同保持一致；instrumentation profile 的编译产物另行标识。

### 2.2 公共头必须形成真正的编译边界

public、provider 和 private 头在物理目录上分开。普通 consumer 的 include 路径仅包含所选 public 根；SoC、Board 和实现适配 target 才获得需要的 provider/private 根。不能把所有头放在同一 PUBLIC 根，再用“不要 include”约定宣称隔离。

检查同时包含正例和反例：公共消费者能编译必要类型；它无法直接 include vendor、私有 controller 或实现注册头。SDK 自身源码可见不等于任意 target 都应继承其 include 路径。

Vendor 源码和规则独立管理。公共结构不得含 vendor 类型，也不得要求 product 手动继承 vendor 编译宏来使用平台 public API。

## 3. 标准 CMake 是唯一软件依赖图

组件、SoC、Board、测试与产品使用普通 CMake target 表达源码、usage requirements 和链接关系。`PUBLIC`、`PRIVATE`、`INTERFACE` 的区别是合同的一部分；不能依赖目录全局的 include、link 或编译选项。

- 显式列出维护中的源码与已知生成输出。首版不通过 `GLOB_RECURSE` 扫描发现任意组件。
- CMake target 是软件依赖权威。配置 JSON 和 generator 不再维护一份“组件 A 依赖 B”的平行图。
- 所选组件的合法性由 target 定义和必要语义校验确认；generator 只输出选择与配置事实，CMake 完成依赖传播与构建顺序。
- 不自建链接器、不重新实现 CMake 的 target graph、测试发现或增量构建。
- 需要保留的 startup、vector、静态绑定和强 IRQ 必须有明确的链接根与 ELF 检查。不得使用全局 `whole-archive` 或删除保留根来掩盖组装问题；默认架构不生成通用设备 registry。
- 所有 CPU/FPU/ABI、优化和 instrumentation 选项通过命名的配置 target 传播，不能在不同子目录追加相互覆盖的 flags。

组件的源码、public/private includes 与 target dependency 留在组件的 CMake 定义中。可选能力 metadata 只描述实现事实，不能重新规定软件依赖或凭 metadata 宣称能力已经验证。

生成器输出应当小且可检查：有效配置、配置头、选中目标标识、静态资源绑定和必要 linker layout。普通 typed C 初始化与数据路径仍由人维护。避免为了减少几行初始化代码生成业务 worker、调度算法、驱动状态机或通用执行 DSL。

## 4. 首版配置：assembly.toml 到 resolved.json

### 4.1 唯一默认输入模型

装配采用 product-owned TOML schema 2 的 `assembly.toml`，引用明确的 Board，按其精确料号选择 SoC，再选择 OS/backend、组件、实例资源和预算。Python 标准 `tomllib` 解析，严格字段模型同时提供 editor schema；CLI 提供 `init/list-bindings/check/explain/generate`。

`assembly.toml` 不包含任意软件依赖声明、shell 命令或代码表达式。它不能通过字符串重新定义 CPU 指令集、ABI、controller 功能或任意编译 flags。模式和优化选项从受支持的命名 profile 选择，实际 flags 由工具链规则生成并记录。

首版不引入 `overlay`、`inherit`、递归 merge 或其他配置语言。外部产品通过完整、显式的 assembly 选择表达需求。相同字段不能在 assembly、Board 输入、CMake cache 和环境变量中各有一套覆盖规则。

SoC 输入声明准确变体、物理内存、controller 与实现能力；Board 输入声明晶振、布线和已审资源路由；product assembly 声明应用选择与所需容量。字段的责任归属由 schema 固定，冲突失败，不由最后一个输入获胜。

### 4.2 一次解析，派生物只有一个来源

配置阶段构造冻结 `ConfigurationIR`、`ControllerIR`、`EndpointIR` 和 `MemoryBudgetIR`。构造器直接消费 IR，`resolved.json` 保存相同决定、输入身份与解析器版本。以下内容由同一次解析派生：

- 编译所需常量、独立私有实例、只读 face 与 `nexus_factory.h` 的 typed ID/getter。
- 供 CMake 使用的明确选择值。
- Board 路由和资源冲突检查结果。
- product layout 的 linker/region 定义。
- 配置摘要与资源预算输入。

这些派生文件不得手工编辑。`resolved.json` 也不是用户输入；缺失或与输入身份不符时必须重新生成，不能从旧生成目录回退。配置失败不留下可被当作成功结果消费的部分 bundle。

解析器拒绝重复 TOML table/key、重复 JSON 事实 key、未知字段、错误类型、越界值、矛盾选择、不存在的输入、未实现能力、非法 IRQ/DMA/pin route 与 layout overlap。输入声明的 capability 不能覆盖实现 capability。字符串到 C/header/CMake 的转换必须有明确编码与转义，不把用户数据当脚本执行。

### 4.3 独立构建根与 cache

每个 `assembly + toolchain + profile` 组合使用独立 build root；CMake cache 不能把同一根悄悄切成另一种 silicon、Board、backend 或 ABI。工具入口验证其身份，不匹配时要求新的构建根或执行有边界的重配置。

构建 cache 只提高速度，不建立验收身份。CI 可复用经过 key 隔离的 cache，但正式候选至少有 clean build 证据。cache key 应包含工具环境、配置和依赖身份，而不是只用 branch name。

### 4.4 Kconfig 的位置

首版没有 Kconfig frontend。Kconfig 不是资源安全、静态组装或 CMake 构建的必需条件。

若未来配置规模证明交互式 choice/dependency UI 有价值，可以另行实现 Kconfig frontend，把选择转换为同一 typed IR 合同。届时必须确定唯一权威前端，并维持相同语义校验；不能让 Kconfig、TOML 和 CMake cache 同时成为真源，也不能让隐式 `select/default` 绕过未实现能力检查。

## 5. 外部 package 接入

### 5.1 组件 package

组件提供普通 CMake target、明确的 public 头、private 实现和验证范围。应用显式链接所需组件。组件不能在被 `add_subdirectory` 时启动线程、注册产品 main、读取设备或改变外部环境。

组件所需硬件通过 typed port 和调用方注入接入。是否需要 worker、queue、storage 和 deadline 必须写入合同，由 product 选择并提供或显式批准；不能隐藏在组件初始化副作用中。

### 5.2 Board package

Board package 包含准确 board/silicon 身份、声明输入、资源 manifest 和有边界的 C/CMake wiring。它不拥有产品 task、私有 protocol、product partition 或制造凭据。板上器件和路由存在不等于已经验证电气行为。

声明输入须覆盖实际编译的 Board 源码及生成所需文件。package 接入前检查 route、资源冲突、variant/density、clock 与必要安全初始状态。hash 绑定只证明输入身份，不自动隔离任意 CMake 代码；正式构建环境仍按可信源码与受控环境运行。

### 5.3 产品 package 与 SDK

产品仓库提供 assembly、main/workers、业务调度、protocol、分区、更新和健康策略，显式消费平台与组件。平台的标准初始化只处理基础设施，不自动生成应用。

首版承诺范围以可重定位 source SDK 为主。binary SDK、稳定 ABI、支持窗口和 LTS 各自需要独立策略与证据。每个正式 source package 需用包外的真实 C/C++ consumer 从新目录验证；源码树内成功不能代替 relocatable package 验证。

外部 package 以准确版本或 source commit/digest 固定。按名称自动下载最新 package 的行为不能进入正式候选构建。

## 6. 工具环境、工具锁与入口

### 6.1 少量工具，各自负责一个问题

| 职责 | 首版工具选择 |
| --- | --- |
| 构建和测试编排 | CMake、Ninja、CTest |
| production 编译 | 锁定的 ARM GCC；Native GCC 提供对应软件验证环境 |
| 主机 C/C++ 测试 | 锁定的 GoogleTest／GoogleMock；不传播至 production target |
| C/C++ 格式与精选分析 | 同一锁定版本系列的 clang-format、clang-tidy |
| 配置、证据与打包工具 | Python；工具测试使用 stdlib unittest；Ruff 统一 Python 格式/lint |
| 正式构建环境 | 按 digest 固定的 Linux OCI image |

不默认叠加多个 formatter、多个同类 analyzer 或多个构建系统。若第二种工具能证明独有价值，再给它明确 owner、规则和维护成本。外部工具检查结果须由实际执行产生，不能将配置文件存在当作检查通过。

### 6.2 一个正式工具链锁

一份 toolchain/environment lock 固定 ARM/Native compiler、binutils/sysroot、CMake、Ninja、Python、formatter/analyzer、Ruff 和 build image digest。安装入口、CI 与发布工具读取这份锁，不能分别写版本。

锁记录准确版本、内容 digest、合法来源与校验方式。版本字符串本身不足以标识编译器/sysroot 的实际 bytes。跨宿主平台可有明确的开发环境条目，但正式候选先限定一个经过验证的 Linux 环境，避免把未验证的宿主组合加入发布承诺。

源依赖使用单独的 dependencies lock，固定 source revision、内容、来源和许可。源码锁与工具锁职责不同，不把 compiler 安装和组件依赖混成另一个软件依赖求解器。

### 6.3 一个薄入口，直接调用原生工具

`tools/dev/dev.py` 调用真实 CMake、CTest、style 与 TDD gate，传播 exit code 并验证新鲜测试报告。preset 决定构建目录；额外原生命令参数直接传递，assembly 仍由唯一解析器解释。

```sh
python tools/dev/dev.py doctor
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug
python tools/dev/dev.py test --preset native-debug
python tools/dev/dev.py check --preset native-debug
```

local 与 CI 调用同一入口和同一底层命令。CI yaml 负责触发、权限、矩阵和 artifact transport，不重复另一套配置解析或检查算法。命令、工作目录、非敏感环境摘要与 exit code进入执行记录；凭据不能进入日志和产物。

## 7. Profile、资源预算与性能门禁

### 7.1 调试、benchmark 与量产是不同身份

development profile 提供符号、调试诊断和适当检查；Native sanitizer profile 提供 ASan/UBSan；benchmark profile 控制测量装置和最少 instrumentation；production profile 固定实际优化、ABI、诊断与资源选择。

这些 profile 的产物不能互相继承资格。生产 mirror 可以保留调试符号，符号存在本身不改变设备合同；但编译选项、插桩或运行代码改变时，必须记录新产物身份。instrumented timing结果不能直接宣称是量产镜像时序，实际 release image仍须完成必要验收。

首版不预设 O3、Os 或 LTO 永远最优。用少量代表工作负载比较 O2/Os/O3 与 LTO，随后选择一到两个正式优化 profile。更改profile必须重新执行编译、ABI/静态绑定/IRQ、正确性、外部 consumer 与资源检查。优化规则不能被调用方任意 flag覆盖。

量产profile保留具体合同必需的参数、request状态、动态借用/撤销、资源和安全检查；固定端口已在构建与控制路径成立的class/owner/lifecycle事实，不因此重新加到每次GPIO数据调用。调用上下文与单owner组织前提按各API明确，debug身份断言不被宣传为C权限隔离。可以关闭纯调试诊断，不能通过 `NDEBUG` 删除必要失败处理。默认不启用 `fast-math` 等改变数值合同的优化。

### 7.2 预算来自产品，不来自库的宣传数字

product assembly提供经评审的 Flash、静态 RAM、MSP/PSP、task stack、heap/pool、buffer、deadline与负载假设。平台验证预算表达合法，真实产物和现场测量验证满足性。合理默认值也不等于产品已批准预算。

资源报告至少区分：Flash LOAD bytes与layout空间、`.data/.bss`、链接保留heap/MSP、task/queue storage、管理域heap、运行时high-water和最小余量。不能把ELF文件大小、含debug的section Total或仅text+bss作为全部设备成本。

每个启用实例有明确资源成本。先按真实 BOM裁剪未使用controller与legacy storage，再讨论抽象开销。优化长期热路径前应测量固定pool容量、lookup/critical duration、copy次数和后台worker成本。

### 7.3 三类性能检查

| 检查 | 执行环境与门禁 |
| --- | --- |
| 静态资源 | 在PR检查实际ELF/map，阻断绝对预算越界，并报告与同profile基线的增量 |
| 受控benchmark | 主机或专用MCU runner记录实际成功工作量、原始时间/cycles、样本分布、测量开销与环境；共享云runner的小时间差仅作趋势 |
| 产品时序 | 专用HIL在真实production artifact、负载和干扰下检查deadline、IRQ响应、传输结清与资源余量 |

benchmark必须校验成功工作量，不能把FULL、错误快速返回或丢弃消息计成成功吞吐。预生成测试输入；避免把不相关的key格式化、日志输出和测量存储混入目标区间。记录mean、分位数、最大观测值以及失败/重试/丢弃，不用平均吞吐掩盖尾部延迟。

MCU计时记录clock、flash wait state、FPU配置、IRQ/负载模式、counter wrap和instrumentation开销。CPU cycle测量与wire/IRQ端到端测量职责不同。最大观测时长不是严格WCET证明；产品必须声明测量条件、分析范围和保留余量。

不在共享CI runner用易抖动的微秒阈值阻断普通功能PR。需要相对时序门禁时，先建立专用runner校准和可重复基线，再评审容差与误报处理。

## 8. 五层验证流水线

### 8.1 Fast local：帮助开发者尽早发现问题

运行输入/schema校验、已改owned文件格式、增量编译和相关契约测试。涉及公共接口或跨层状态时，开发者主动执行相应集成集合。local结果给出明确范围，不直接成为发布资格。

低影响文档或纯格式修改使用对应校验，不为每段文字创建镜像实现的测试。行为、状态、资源或错误语义改变时，测试必须证明用户可观察的成功与失败路径。

### 8.2 PR：验证共同合同没有被破坏

必需集合包括：owned production编译与精选warnings、格式、精选静态规则、关键完整Native契约及ASan/UBSan、公共头隔离、真实外部consumer，以及两SoC乘两backend的代表ARM链接和资源预算检查。

配置解析、public API、ownership、IRQ/DMA、同步、linker、toolchain和发布工具改变时，触发跨层完整检查。选择性测试依据显式依赖和风险规则，不能只按修改路径猜测影响。所选检查计划进入报告，避免没有测试被选择却显示passed。

测试覆盖失败语义与不变量，包括pool耗尽、stale reference、busy/retry、部分初始化、timeout/cancel race、cleanup失败与buffer最终归还。Native fault model证明其模型合同；它不证明真实MCU电气或DMA结清。

### 8.3 Nightly：验证广度和复杂路径

执行全部维护Board/profile、完整source SDKconsumer、fault/property/fuzz与较长软件负载，并保存资源趋势。依赖/工具升级、优化探索、第二编译器或跨宿主检查可在这里先评估，再决定是否进入正式支持。

nightly发现关键回归后，应将可重复的最小验证纳入PR集合。夜间绿灯不补足未执行的PR必需检查，也不允许无限增长矩阵而没有维护者和能力承诺。

### 8.4 Hardware：验证真实设备条件

HIL admission先验证准确artifact、profile、board/silicon、fixture、station和必要instrument身份，再执行真实操作。检查IRQ/DMA、电气、timeout/cancel settlement、极端负载、stack/pool high-water、掉电/恢复与产品deadline。

空station、错image、错board、缺原始数据与仅host model结果不能生成物理passed状态。记录仪器/fixture版本、校准和负载条件，避免把一块板的一次轻载结果扩展为所有density、Board和产品。

工具readiness与设备操作分开。只有admission测试通过时，状态仍是HIL tooling ready；当前硬件延期期间，物理资格保持未执行。

### 8.5 Release：晋升同一候选产物

release候选绑定准确source/dependency/config/environment、编译链接实况、ELF/BIN/HEX及相关证据。验证source package、许可/SBOM、签名/来源和产品所需物理资格；发布责任人审查明确支持范围与恢复策略。

HIL或签名后的晋升使用同hashartifact，不重新构建一个名称相同的image。更改代码、配置、依赖或构建环境产生新候选，相关资格重新执行。source SDK验收不自动建立binary ABI、签名、产品认证或LTS。

## 9. Provenance、hermetic与reproducible分开验收

### 9.1 Provenance：输入与输出的可追溯关系

每个发布候选绑定一个唯一的完整输入元组：平台与产品source身份、外部package和依赖身份、assembly/SoC/Board/layout输入、resolved配置、工具环境、profile与生成器版本。每个产物文件记录内容hash。

这里的一对一关系是“该候选的输入上下文对应该次构建产物”，不是“一个Git SHA只能生成一个binary”。同一source有多个profile或Board时，形成不同候选记录；source SHA不能替代artifact身份。字节相同的文件也可以出现在不同上下文，候选manifest必须保留各自来源。

执行报告记录实际命令、exit、测试计划/实际枚举、编译数据库、链接输入与输出、raw log和hash引用。unsigned hash能发现内容变化，不能证明发布者或审批者身份；正式attestation和签名另有trust policy。

### 9.2 Hermetic：只读取声明的输入

正式构建使用固定image和已校验依赖；准备下载与实际构建分离。构建阶段关闭网络，不读取任意宿主目录、开发者凭据、未声明环境配置或系统工具fallback。源码输入只读，输出在独立目录，禁止边构建边改source。

记录compiler/sysroot及其读取的工具环境，不仅记录compiler banner。固定tool版本和Git revision本身不会使构建hermetic；声明Board输入也不会限制任意CMake代码访问其它目录。

### 9.3 Reproducible：相同输入重复得到相同输出

用两次独立clean build改变绝对目录和cache条件，比较目标ELF/BIN和声明可确定的package内容。固定`SOURCE_DATE_EPOCH`与path mapping，控制archive顺序、mtime和生成文本；禁止当前时间和机器路径进入firmware身份之外的随机内容。

执行时间、日志、签名和可确定payload分别存储，避免manifest自引用或把每次run timestamp嵌入binary。对于需要随机性的签名/包装，明确比较范围，不用“忽略不同内容”制造bit-exact结论。

只有独立重建比较完成才可声明可复现。固定container加hash、一次成功build或源码归档deterministic都不够。

## 10. 证据模型与失败传播

使用一个版本化结果schema，表达run身份、planned/executed checks、exit/status、raw-log/artifact引用、qualified scope与limitations。完整compile database、map和log存为artifact，manifest引用，避免每个profile重复嵌套相同大文件。

- mandatory检查缺失、零执行、全部skipped、过期或工具失败时，不输出passed。
- 明确区分passed、failed、not executed与unsupported；不把unsupported统计成已验证实现。
- source、配置或环境变化后，历史结果可保留作比较，但不能自动继承资格。
- 按风险解释测试结果和未关闭条件，不以“通过N项”掩盖未执行的关键验收。
- summary由结果生成；workflow定义、target存在、角色文件和计划勾选不是执行证据。
- 按candidate/run身份保存证据，引用不可变artifact；CI artifact传输期限不能成为长期支持记录的唯一存储。

诊断日志保留实际错误与cleanup结果，避免每层重复包装成generic。任何工具失败都应保留可调查输出并返回非零，不能以生成一个空JSON或上传artifact成功替代检查成功。

## 11. 十人的工作与review模型

为Arch/SoC/Board、I/O/OS、components、build/evidence、product qualification五类领域安排实际primary和backup，可以由十人按五对覆盖。角色不是强制组织图；一个人可跨领域，但不能将无人负责的领域视为已获得review能力。

每个高风险领域至少培养一名替补。primary/backup名单需实际团队确认；本文不创建虚构reviewer、远程branch rule、credential或发布授权。

### 11.1 普通改动与高风险改动

普通内部实现至少有一个独立domain reviewer。public API、依赖方向、IRQ/DMA/ownership、persistent format、安全和发布链改变，需要domain reviewer与消费者或跨层reviewer共同审查。作者不能自我批准。

不要为所有PR开架构委员会。小ADR只用于真实跨层决策，记录问题、决定、tradeoff、接口和验收；纯机械重命名使用普通PR说明。review主要检查合同、失败路径、并发、资源/时序与维护成本，格式交给工具。

变更单位要可review：一个可以解释清楚的行为与其调用方/测试/文档。不得把接口改造拆成多个长期红色中间状态。多个产品无法原子迁移时，提供明确窗口、兼容层owner和删除条件；兼容层不是永久第二套对象体系。

### 11.2 工作项与退出状态

每个工作项记录问题、owner、依赖、验收与风险。状态至少区分设计接受、实现完成、软件验收、物理验收、发布可用。必要外部条件单独列出，不把等待硬件或责任落实隐藏在implemented状态中。

工程评估关注可重复失败、逃逸缺陷、资源/时序风险与交付阻塞。不要用代码行数、PR数、测试数、总覆盖率或文档数量作为个人/模块绩效指标。覆盖信息用于寻找未验证路径，不单独证明可靠性。

发布由实际产品owner与release责任人承担；HIL reviewer审查真实预算和资格范围，security/trust owner审查签名与恢复策略。CI执行可自动证明的条件，不能替代人的产品决策。

## 12. 代码风格、注释与静态检查

### 12.1 沿用现有formatter与命名规范

代码格式沿用仓库根目录 [.clang-format](../../.clang-format) 与 [.editorconfig](../../.editorconfig)，不另起格式口径。工具版本进入正式环境锁；现有配置声明兼容clang-format 14及以上，实际采用的版本须通过同配置验证，不把版本升级作为改变风格的机会。

保持80列、4空格、禁止tab、K&R attached braces、类型侧pointer alignment（`char* ptr`）、switch case缩进、现有include排序和最多一个连续空行。控制语句不压成单行，普通函数体保持多行。长typed接口按现有formatter折行，不能为减少折行改成100列。UTF-8、LF、文件末尾换行及各文件类型缩进由EditorConfig约束。

命名沿用 [贡献指南](../../CONTRIBUTING.md) 和 [coding standards](../sphinx/development/coding_standards.rst)：文件、函数和普通变量使用snake_case；函数与公共symbol使用现有模块前缀，如`nx_`、`osal_`；类型以`_t`结尾；宏、枚举值和常量使用UPPER_CASE。文件内静态变量使用`s_`前缀，全局变量使用`g_`并尽量避免；private函数和只在文件内使用的数据为static。优先普通函数、const table和typed结构，避免宏生成隐藏控制流与清理动作。

文件按真实状态、生命周期和职责拆分。不能为追求文件短把同一个状态机切成大量`.inc`，也不能把所有接口塞进巨型umbrella头。

### 12.2 沿用Doxygen形式，完善合同内容

注释形式沿用 [现有注释规范](../../.kiro/steering/comment-standards.md)。C/C++使用反斜杠Doxygen标签：`\brief`、`\param[in]`、`\param[out]`、`\param[in,out]`、`\return`、`\retval`、`\details`和`\note`；按现有模板对齐说明文本，不引入`@brief`风格。

public header写完整参数方向、返回值、必要错误和共同合同。source不重复header中的`\param`和`\return`，使用`\brief`、`\details`、`\note`说明实现；static helper使用简化`\brief`。结构体和枚举成员沿用`/**< ... */`；普通和行尾注释使用`/* ... */`，行尾前两个空格；section采用现有`/*---...---*/`模板。C/C++源码与文档内C/C++示例同样遵守这些形式。

头文件保持含`\file/\brief/\author`的文件头；C源文件保持含`\version/\date/\copyright`的完整文件头，必要时增加`\details`。既有更完整文件头与许可证声明保留，字段内容准确，不编造作者、版本、日期。Git和发布记录补充历史追踪，不替代文件头，也不要求给每个函数新增手写修改日志。

CMake和Shell沿用`#`注释；Python沿用`#`与三引号docstring。C/C++注释形式不套用到这些语言。第三方源码保持上游规范和许可声明。

public API在以上现有形式中提供调用者需要的合同：

| 项目 | 必须说明的含义 |
| --- | --- |
| context | task/ISR、允许的IRQ优先级或mask条件、单core/SMP范围 |
| timing | 是否阻塞、timeout/deadline起点、单位、zero/forever含义与推进责任 |
| ownership | 谁持有对象、借用何时开始/结束、复制handle与close后的有效性 |
| callback | 运行context、lock状态、可否reentry、callback返回前后的借用状态 |
| failure | 返回码、output有效性、remaining ownership、retry/recovery行为 |
| resources | 固定容量、可能allocation、copy、worker与预算要求 |

共用规则集中在type或模块contract，函数写必要差异，通过`\details/\note`等现有标签表达。内部注释解释硬件勘误、barrier、竞态和不变量，避免复述代码。保留已有可追溯需求和许可声明，不新增无来源的requirements编号；契约内容完善不改变现有注释展示风格。

TODO必须对应一个有owner和退出条件的工作项。注释中的能力、时序与“线程安全”声明需要具体范围，不能靠措辞替代证明。

### 12.3 精选warnings与静态规则

owned production使用`Wall/Wextra`和经评审的prototype、shadow、conversion等规则；正式门禁阻断所选规则的新diagnostic。先确认规则在C11与支持toolchain下有稳定含义，不一次打开所有analyzer规则制造海量忽略。

clang-tidy消费实际compile commands与生成配置，只检查owned边界。vendor代码按其target独立处理；不能通过全局静音隐藏自身错误。每条suppression要有具体原因和局部范围，不提交无解释的全目录排除。

Cppcheck 2.13不读取编译数据库中的`-isystem`。工具按每个真实翻译单元将其投影为`-I`，保留普通目录先于系统目录的搜索顺序；编译器ABI查询仍使用原始参数。分析边界与clang-tidy一致：只对该翻译单元实际SYSTEM目录中、位于`vendors/`或`ext/`的第三方头文件逐一排除，并记录具体路径、规范路径与SHA-256。自有头文件、第三方实现文件、非SYSTEM目录及指向自有文件的别名仍接受检查；不能全局关闭诊断或重封基线。此边界用于分析自有代码，不能证明第三方代码没有缺陷。

本规则集是工程检查基线，不构成MISRA、功能安全或其他认证。若产品要求标准符合性，另立准确standard/version、覆盖范围、deviation、工具资格与独立review项目，不能仅凭`cert-*`或analyzer配置宣称合规。

### 12.4 当前落实与历史迁移

原架构曾存在文件头、注释标签和格式不一致。本轮已经移除旧HAL/OSAL、Runtime及业务服务，将当前Core、I/O、OS、Arch、SoC、组件和测试按原有格式与反斜杠Doxygen规则统一。根`.clang-format`和`.editorconfig`保持不变。

[格式目录清单](../../.clang-format-dirs)覆盖所有维护中的C/C++与`.inc`，包括测量workload。[安装入口](../../scripts/setup/install_dev_tools.py)固定安装pre-commit 4.3.0和clang-format 14.0.6，并安装真实`pre-commit`、`commit-msg` hooks。默认提交检查暂存快照；手工`--files`、`--all-files`检查工作区。CI运行相同的整文件门禁，静态分析消费实际编译数据库。

首次历史审计的596文件及337/291格式／注释欠账仅用于追溯。当前[冻结基线](../../dependencies/style-baseline.json)的两类欠账集合均为空；新源码全部按整文件规则检查，禁止增加或重封豁免。最终通过状态由当前提交的实际门禁报告决定，不能继承历史计数。归档原始记录与第三方源码分别保留自身身份，不冒充当前自有代码验收。

环境锁、统一dev入口、源码SDK、ELF预算、离线复现和HIL工装已实现；准确范围见[交付说明](../delivery/README.md)。formatter负责排版，注释检查器只验证机械规则；API合同语义仍需review和行为测试。安全扫描诊断按维护范围逐项核对，不通过全局静音消除自身问题。软件检查不能替代实板时序、产品安全或标准符合性资格。

## 13. 错误、所有权与并发API规范

状态码保持少量、caller可以行动的类别：invalid input/state/context、unsupported、not found、busy、full/empty、timeout/cancelled与I/O/integrity等。vendor raw flags作为可选detail保留，不传播vendor类型，不把不同错误全部压成generic。

- return status不自动触发global日志或global error callback；调用方决定策略。并发API不以global errno/last-error作为权威结果。
- ISR诊断使用有界、可关闭的binary记录，字符串格式化和复杂存储在合适task进行。
- 状态与ownership正交。cancel accepted、terminal result和buffer settled分别定义；`TIMEOUT/CANCELLED`单独不证明buffer已归还。
- create/open或submit admission被拒绝时不产生有效新handle/ticket、不建立新借用；已ACCEPTED请求的start/error/timeout属于该请求结果，不能倒退称为零借用拒绝。query/receive明确失败output是untouched还是invalid，禁止调用方猜测。
- close失败保持既有引用与责任。rollback report分别保存原失败、cleanup失败和remaining ownership，不能以第二个错误覆盖整个原因。
- 所有长度、offset和资源乘法在操作前检查。协议/持久化编码不依赖C struct padding、enum宽度或host endian。
- `volatile`不构成线程/ISR同步。atomics、barrier与IRQ mask由明确Arch合同限定，不能把single-core规则称为SMP安全。
- assert用于内部不变量和明确fail-stop，不代替可恢复输入/资源错误处理。production保持必要边界检查。

有限资源耗尽应返回合同中的失败，不循环直到“总有一天成功”。阻塞与retry必须有预算；后台推进责任由caller或明确组件worker承担，不能出现文档未声明的隐藏线程或timer。

## 14. 工程批次与退出条件

这些批次可以与平台实现垂直切片协同，但状态必须由实际结果关闭。时间和人数不能替代退出条件。

| 批次 | 交付与退出条件 |
| --- | --- |
| E1 工具与入口 | fresh checkout进入锁定环境；一个薄入口调用标准build/test工具；local与CI共享实现，版本不再多处定义 |
| E2 配置与接入 | assembly/SoC/Board一次解析产生resolved bundle；矛盾/unsupported失败；真实外部product与Board接入；public/private/vendor编译边界有正反验证 |
| E3 垂直合同与代码规范 | 一个端到端切片完整处理成功、失败、context、ownership与预算；C11 production/C++ test隔离；formatter和精选规则实际执行 |
| E4 软件流水线与证据 | PR/nightly风险矩阵执行；缺失/零执行/skipped/stale结果实际拒绝；产物资源越界实际阻断；重复旧入口删除，结果绑定准确输入与artifact |
| E5 hermetic与HIL准备 | 无网络正式build成功；独立clean rebuild比较完成；HIL admission拒绝错artifact/profile/empty station；物理资格继续未执行直到真实测量 |
| E6 产品发布资格 | 产品所需物理、安全、恢复与负载预算有真实证据和reviewer；sealed candidate签名与来源验证完成；同hashartifact晋升；责任人与支持范围真实落实 |

E1至E4可形成完整软件工程交付。E5中的工具readiness、hermetic与reproducible各自验收；HIL实际执行独立关闭。E6不能由本文、计划表或软件通过数量代替。

E1–E6描述工程验收领域，不是另一套排期或任务真源。唯一实施任务表是 [执行清单](next-generation-execution.csv) 的B0–B6：E1/E2在B1建立，E3随B2–B4切片执行，E4/E5在B5收束，B6只封存软件候选；E6待外部产品和实际硬件证据齐备后另行完成。

## 15. 首个落地切片

先用一个精确Board、一个OS/backend和一个外部产品完成fresh checkout到真实consumer build，再扩展第二backend和第二SoC。第一切片需同时包含一个小typed I/O、错误/cleanup路径、资源预算、public/private include边界和原始产物证据。

工具、schema、CMake模块和review规则应由这个切片证明需要。每扩展一种组合，都增加实际验证及维护责任；不先构造庞大通用框架，再用空fixture宣布工程模型完成。

## 14. TDD 与 GoogleTest／GoogleMock 强制开发合同

新增生产行为和缺陷修复遵循 RED → GREEN → REFACTOR。先从公开合同编写可失败的测试，执行并确认失败原因是所需行为尚未实现，再以最小正确实现让测试通过，最后整理实现并重复相关测试。新 API 尚不存在时，首次编译失败可以记录接口 RED；接口建立后还要实际执行行为测试。已经存在的行为补回归不能宣称历史上采用了 TDD。

GoogleTest 承担断言、fixture 和边界场景；GoogleMock 只替代硬件、时间、通知和执行器等依赖端口。测试实际调用 Nexus 的公共 API，不能把被测 provider、factory 或 owner 整体 mock 掉，再用期望调用证明自身正确。优先检查不同实例的隔离、零副作用拒绝、资源冲突、deadline、取消后的排空和最终发布顺序。正常路径和失败路径共同确定合同，不要求为了数字重复实现细节。

框架源以 `dependencies/googletest.lock.json` 固定准确版本、commit、archive SHA256 和许可；主机测试只从仓库内归档解压，没有 configure-time 下载或网络回退。`tests/google/CMakeLists.txt` 启用 C++17，并保持依赖在 Native 测试树内。ARM、关闭测试的构建和 source SDK 消费者不获得 C++ runtime 或测试框架依赖。

统一执行入口为：

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
python tools/dev/dev.py test --preset native-debug
```

第一条命令运行测试报告检查器的工具测试，配置并构建 `nexus_google_contracts`，从各二进制发现完整用例，随后执行不带筛选的实际测试。它移除环境中的 GoogleTest filter/shard 设置，删除旧 XML，拒绝零用例、未执行、跳过、失败、过期报告和发现数不符，并在 `build/<preset>/tdd/` 保存原始输出、GoogleTest XML 和汇总。第二条命令执行完整的现有 CTest 模型与工具回归；两者的证据职责不同。

已安装的 pre-commit hook 对生产、测试、配置和构建行为变更执行同一 GoogleTest gate；只改说明文档无需重建。hook 在执行前后检查未暂存／未跟踪的行为改动，并比较起止 `git write-tree`，要求暂存区内容完全一致。执行过程中修改或重新暂存源码会拒绝验收，不生成新的汇总，避免工作区修复或并发编辑误认证即将提交的源码。CI 独立执行 gate，不依赖贡献者本地是否安装或运行 hook。`--no-verify`、`SKIP`、禁用测试或手写 XML 不构成有效验收。

每项行为变更的评审说明包含失败测试名称、RED 命令及实际失败原因、GREEN 命令和相关场景覆盖。代码评审检查测试是否先定义了可观察合同。自动化能够证明真实执行、报告完整性和当前源码验证，无法机械证明所有开发者历史上遵守了测试先行顺序；因此不能把一个绿灯当作 TDD 流程全部合规。
