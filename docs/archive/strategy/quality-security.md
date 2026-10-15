# Nexus 质量、安全与现场可靠性规划

本文分析基线为 `7a203266082ad1f655b6686713b2b7950ba31ec6`，审查日期为 2026-10-08。代码证据来自固定提交的 GitHub 文件读取；维护分析期间完成了一项本地单文件编译对照，见 BUILD-01。本文未执行完整构建、单元测试、硬件测试、攻击验证或认证评估。已有测试源码、工作流定义和文档声明分别说明“测试意图”“自动化配置”“项目自述”，均不能替代该提交的成功执行证据。

维护目标是让企业能回答四个问题：这个板卡与配置是否真正受支持；失败后设备如何恢复；交付物能否追溯与重建；安全问题出现后谁能在什么期限内修复。当前约束为约 10 人、STM32/GD32、3–6 个月，首个参考板尚未确定。第 3 个月争取一个受控工业试点，第 6 个月形成 STM32 与 GD32 双板候选；第一阶段聚焦一个参考板、Native 与 FreeRTOS，不以新增 MCU 数量衡量进展。

## 1. 已证事实与静态风险

以下 P0/P1/P2 是建议维护顺序，不是 CVSS 分数、外部漏洞评级或已发生事故的证明。P0 指进入产品试点前必须关闭的发布阻碍；P1 指形成稳定版前必须关闭的问题。

| 编号 / 顺序 | 审查结论与证据 | 边界 / 产品影响 | 建议处理与验收 |
|---|---|---|---|
| SEC-01 / P0 | IV 由 xorshift 生成；设置密钥时将状态重置为密钥字节 XOR 派生的 32 位值。见 [config_crypto.c:108-133](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_crypto.c#L108-L133)、[440-444](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_crypto.c#L440-L444)。 | 已证实现事实。相同密钥重新设置会重放 IV 序列；派生状态为 0 时 xorshift 始终为 0。这不是密码学安全随机源。不能只归因于初始常量 `0x12345678`，也不据此断言 AES 轮函数错误。 | 引入独立 RNG/DRBG 服务、平台熵源及失败传播；存储格式采用 AEAD 并明确 nonce 唯一性协议。测试重复初始化、复位、全零状态、熵源失败及 nonce 耗尽。 |
| SEC-02 / P0 | 加密输出为 IV + CBC 密文；解密只验证长度、对齐和 PKCS7 填充。见 [config_crypto.c:476-638](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_crypto.c#L476-L638)。 | 已证在该实现中无认证标签校验；静态风险为密文完整性不可保证。未执行篡改利用验证。配置属于普通参数还是授权/安全参数，决定产品严重程度。 | 用受维护的密码库提供 AES-GCM 或适合目标 MCU 的 AEAD；选择后做性能和 nonce 生命周期评审。认证失败时不得向上层交付明文；AAD 绑定记录版本、键名/命名空间、设备和 key id。 |
| DATA-01 / P0 | 密钥轮换先替换全局密钥；旧数据重新加密仍是 TODO，函数返回成功。见 [config_crypto.c:767-791](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_crypto.c#L767-L791)。轮换测试明确接受只改密钥的现状，未读取旧值，见 [test_config_crypto.cpp:259-271](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/tests/config/test_config_crypto.cpp#L259-L271)。 | 已证功能未完成且测试未覆盖关键用户结果。已有密文在换钥后存在不可读风险；尚未验证全部数据读取路径的运行结果。 | 未实现前返回明确 unsupported/error，不能声称成功轮换。完成 key id、双代读取、事务迁移、断电恢复、旧 key 安全退役；验收包含旧值可读、全量校验和任一步失败可恢复。 |
| DATA-02 / P0 | Flash 后端是静态内存结构；`commit()` 只清 dirty 标志，注释明确为 Native 模拟。见 [config_flash_backend.c:10-16](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_flash_backend.c#L10-L16)、[62-93](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_flash_backend.c#L62-L93)、[328-354](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/framework/config/src/config_flash_backend.c#L328-L354)。 | 已证模拟实现，不是实际 Flash 掉电持久化。按默认 128 个条目，仅 key/data 数组就占 73,728 字节，另加元数据与对齐；这是源码估算，不是链接实测。 | 明确命名模拟后端；真实后端采用记录日志、提交标记、序号、损坏检测、双区回收与擦写计数。以链接 map 核验内存；断电后只能恢复完整旧版本或完整新版本，不能暴露半写值。 |
| MEM-01 / P1 | Native 队列只检查参数非零，直接 `malloc(item_size * item_count)`。见 [osal_native.c:1378-1425](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/osal/adapters/native/osal_native.c#L1378-L1425)。 | 已证缺少乘法溢出检查；静态风险为 size_t 乘积回绕后分配不足。实际可达性依赖调用方尺寸来源，未执行越界复现。 | 检查 `item_size > SIZE_MAX / item_count` 及总容量预算；极限参数必须在分配/状态变更前失败；用 ASan/UBSan 执行边界与后续 send/receive 测试。 |
| SEM-01 / P1 | FreeRTOS 超时转换直接调用 `pdMS_TO_TICKS`；Native 的 ISR 入口按普通上下文处理。见 [osal_freertos.c:302-314](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/osal/adapters/freertos/osal_freertos.c#L302-L314)、[osal_native.c:1887-1896](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/osal/adapters/native/osal_native.c#L1887-L1896)。 | 已证行为差异。有限毫秒超时在某些 tick 配置下会舍入为 0；这是否违反契约需先定义。Native 不能证明真实 ISR 优先级约束正确。 | 统一“最小非零等待、最大等待、tick 回绕、取消与销毁”的契约；不同 tick rate 执行同一契约套件；ISR/调度验证留给真实 port 与 HIL。 |
| BUILD-01 / P0 | `nx_mutex.c` 以 GCC/Clang 宏选择 Cortex-M 的 PRIMASK 汇编，没有先限定 ARM 架构。见 [nx_mutex.c:45-80](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/hal/src/nx_mutex.c#L45-L80)。维护分析在 x86、GCC 14.2 下对照编译：启用 `NX_CONFIG_HAL_THREAD_SAFE=1` 产生 `primask` / `cpsid` assembler 错误；默认线程安全关闭时同一单文件可编译。 | 已复现单文件编译失败；不是整个 Native 构建或测试结果，也不证明关闭线程安全配置的全平台正确性。编译器种类不能代替处理器架构判断。 | 按 CPU/port 提供 critical-section 实现，Native 使用相应同步机制；线程安全开/关均进入 Native 与两套目标板的配置矩阵。不得通过全局关闭线程安全隐藏不受支持的组合。 |
| GATE-01 / P0 | 正式 Release 先创建，构建任务随后执行；缺失 bin/lib 的复制被忽略。见 [release.yml:44-56](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/release.yml#L44-L56)、[96-102](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/release.yml#L96-L102)。 | 已证工作流顺序缺口；静态风险为失败构建留下正式发布或无有效二进制的包。本文未审查实际 Release 资产是否为空。 | 先构建一次、验证必需资产与哈希、汇总门禁，再从已验证工件发布；候选版采用 draft/prerelease。最终发布引用同一 commit 和工件，不重新构建未验证内容。 |
| GATE-02 / P1 | 安全依赖/许可扫描用 `\|\| true`；静态、MISRA、性能与 memcheck 也存在吞失败。见 [security.yml:39-42](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/security.yml#L39-L42)、[136-138](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/security.yml#L136-L138)、[quality-checks.yml:50-64](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/quality-checks.yml#L50-L64)、[120-127](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/quality-checks.yml#L120-L127)、[performance.yml:34-35](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/performance.yml#L34-L35)、[85-89](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/performance.yml#L85-L89)。 | 已证“产生报告”不等于“阻断违规”。不能据此声称项目已通过 MISRA、内存安全或性能门禁。 | 区分工具失败、无测试、实际发现和有期限豁免；新严重发现、关键测试失败、报告缺失均阻断。遗留发现以基线管理，不把整类检查永久设为可忽略。 |
| SUPPLY-01 / P1 | TruffleHog Action 引用浮动 `main`；GoogleTest 引用 tag；厂商版本文件只列 CMSIS 与 STM32F4 部分依赖。见 [security.yml:109-115](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/.github/workflows/security.yml#L109-L115)、[tests/CMakeLists.txt:21-27](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/tests/CMakeLists.txt#L21-L27)、[vendors/VERSIONS.md:5-16](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/vendors/VERSIONS.md#L5-L16)。 | 已证部分依赖不可由这份人工清单完整追溯；tag 可变，不等于不可变来源锁定。仓库 gitlink 本身固定 commit 是已有优点，需纳入自动清单。未发现依赖被篡改的证据。 | Actions、工具链容器、FetchContent 固定完整 SHA/摘要，自动升级 PR；生成实际产品 SBOM 和开发依赖清单，完成哈希、来源与许可证核对。 |

BUILD-01 的启用配置复现命令为 `gcc -DNX_CONFIG_HAL_THREAD_SAFE=1 -Ihal/include -Iosal/include -I. -c hal/src/nx_mutex.c`，执行目录为源码根目录；这是 x86 GCC 14.2 的编译检查。关闭宏的单文件对照仅说明这一编译阻碍与所选配置相关，未执行链接或运行时互斥验证。

## 2. 企业质量模型与支持等级

支持对象必须是组合：`SoC + board/revision + bootloader + OSAL backend + compiler/version + config profile + drivers/features`。单独写“支持 STM32F4”不足以指导产品决策。公共平台与具体产品的发布证据分别保存，产品集成不得仅引用平台单元测试报告。

建议使用下列等级，每次发布从证据计算支持矩阵，维护者有责任降级失去验证的组合。

| 等级 | 条件 | 对外承诺 |
|---|---|---|
| 声明/规划 | 接口、Kconfig 或目录存在，尚无完整实现 | 设计意图，不承诺可编译与可运行 |
| 实验 | 能构建并跑最小示例；契约、异常路径或硬件证据不完整 | 原型和评估；允许接口变更，标注风险 |
| 验证 | 指定矩阵的 SIL/HIL、资源预算和恢复场景通过，有可追溯报告 | 指定版本与组合可用于受控产品试点 |
| 企业支持 | 验证等级 + 多轮现场试点 + 更新/漏洞响应/生命周期负责人 | 明确补丁期限、兼容窗口、支持截止日期与退役路径 |
| LTS | 企业支持等级 + 冻结兼容范围 + 专用补丁分支 + 持续回归资源 | 例如建议 24 个月维护周期；须由预算、团队与硬件供应共同承诺后生效 |

本次静态审查不能把任何组合提升为“验证”“企业支持”或 LTS。涉及汽车、医疗、轨交、工业安全控制的项目，应按具体危害分析选择 ISO 26262、IEC 62304、IEC 61508 或行业适用要求，并建立独立的安全案例、工具/组件资格与组织流程。MISRA 检查只是其中一个工程活动；本文不声明标准符合性、功能安全认证或认证组件资格。

## 3. 测试架构：从接口契约到真实故障恢复

### 3.1 每层测什么

| 层次 | 覆盖对象 | 关键失败场景 | 必须保存的证据 |
|---|---|---|---|
| 静态与构建 | 编译、头文件、自包含、配置约束、整数/生命周期分析 | 不支持配置、驱动缺实现、告警、链接资源超限、工具未执行 | 编译数据库、日志、toolchain/config/submodule 摘要、map 与诊断 |
| 单元/契约 | HAL/OSAL 公共语义、序列化、配置、初始化 | null、容量边界、溢出、重复 init/deinit、句柄失效、timeout、资源耗尽 | 需求到测试映射、JUnit/CTest、失败 seed 与最小复现 |
| SIL（软件在环） | Native 模拟、业务状态机、协议与更新策略 | 消息重排/丢失、时间推进、解析畸形输入、后端失败、任务竞态 | 固定 seed、虚拟时钟轨迹、故障脚本与覆盖报告 |
| 仿真/目标集成 | 可支持的 QEMU/指令模拟、实际 FreeRTOS port | tick/ABI/链接启动、系统调用契约；不能替代电气行为 | 镜像 hash、仿真版本、配置和运行输出 |
| HIL（硬件在环） | 参考板、时钟/电源/DMA/中断/总线/存储 | 掉电、复位、线缆断开、DMA 卡住、错误 IRQ、器件无响应 | 板卡序号/版本、固件 hash、仪器配置、时间戳、波形与恢复结果 |
| 系统/现场 | 产品固件、实际网络和运维流程 | 升级/回滚、后台不可用、证书到期、长时间运行、供应批次差异 | 场景、设备群体、升级成功率、复位原因、业务恢复时间 |

Native 使用统一契约测试保证 API 语义，HAL 模拟使用可脚本化的设备模型；两者不伪装成真实 ISR、DMA、一致性缓存、模拟 ADC 精度或低功耗计量。HIL 以板卡池、租约、隔离电源和防误烧录设计实现并发；产品板接入前先确认硬件版本、串口和固件标识，不能依赖人工挑选串口。

### 3.2 故障注入验收

| 对象 | 注入点 | 结果不变量 |
|---|---|---|
| 配置存储 | 每次 program/erase、记录头/体/提交标记之间断电；损坏单页、空间耗尽、擦写失败 | 重新启动后只看到完整版本；损坏可检测且不返回假成功；工厂参数受保护；恢复时间有上界 |
| 加密/身份 | 熵源失败、nonce 重复、tag/key id 错误、密文位翻转、密钥迁移任一步复位 | 拒绝未认证数据；不输出明文/密钥；错误可审计；旧数据迁移可恢复且不复用 nonce |
| OTA | 下载任意块后断电、元数据半写、镜像篡改、错误板型、旧版本重放、首次启动崩溃、云端失联 | 未验证镜像不启动；合法的待确认版本能回滚；最低安全版本策略仍生效；保留可操作恢复入口 |
| OSAL/驱动 | OOM、队列满、锁竞争、等待时销毁、回调后释放、重复 IRQ、取消与完成竞争、DMA timeout | 无死锁/UAF/双完成；错误码明确；资源最终回收；设备恢复/安全降级可重复 |
| Watchdog/电源 | 任务卡死、中断风暴、brownout、sleep/wakeup 连续切换 | watchdog 能识别业务停止而非“喂狗任务仍运行”；复位原因保留；安全状态与恢复策略受产品要求约束 |
| 协议/Shell | 超长包、分片重排、乱码、重入、输出堵塞、权限不足 | 有界解析和内存；输入不能绕过权限；日志背压不拖垮控制任务；生产调试入口策略生效 |

故障测试按状态机转换点枚举，同时辅以随机序列；失败保存 seed、注入序号和镜像，修复后保留回归用例。断电仿真应模拟 Flash 的实际约束（擦除粒度、program 单位、1→0、部分写入），用真正断电 HIL 验证模型，而不是在 RAM 中简单抛出错误。

### 3.3 并发、性能和内存

- Native 分开运行 ASan/UBSan 与 TSan；对解析器、配置导入、协议帧和加密包封装做 fuzz。Fuzz harness 必须保持有效状态初始化，并对 length/offset 组合、损坏格式和持久化恢复建 corpus。
- OSAL 契约测试覆盖 Linux/Windows/macOS 可用后端及真实 FreeRTOS 配置。等待 API 应定义 deadline/timeout 的单位、舍入、tick wrap、spurious wakeup、取消、对象销毁与 ISR 禁止规则；测试跨后端结果，而不是复述各自实现。
- 目标板测启动时间、ISR 最大延迟、任务唤醒抖动、最坏负载 deadline、总线吞吐、DMA CPU 占用、配置 commit 延迟、升级空间、功耗。GPIO/逻辑分析仪、DWT cycle counter 或 RTOS trace 是可选测量手段；记录测量开销与 MCU 支持情况。
- 每个 profile 规定 `.text/.rodata/.data/.bss`、heap、各任务栈、DMA/通信 buffer、boot/OTA/存储分区预算。禁止把 heap free 和栈 high-watermark 当作全部内存证明；另评估最深中断嵌套、最坏调用栈和 DMA 对齐/缓存一致性。
- 建议起始门槛：新增 ASan/UBSan 严重发现为 0；平台关键状态机转换/异常路径须有测试；普通维护用变更分支覆盖率 80% 作为审查参考而非发布正确性的唯一证明。控制 deadline、栈余量、CPU 和功耗阈值必须由产品 profile 提供；不能用统一“低于 10 μs”替代产品需求。
- 对同一板型/工具链/配置/负载建立稳定基线，建议资源增长超过 5% 或关键延迟增长超过 10% 时触发解释与评审。若绝对产品预算更严格，以绝对预算为准。云 runner 的时间测量先作为趋势；不把其噪声直接用作 MCU 实时性门禁。

### 3.4 首发方向：工业控制与设备联网

平台应验证控制环在网络异常时仍保持产品要求。控制任务、现场总线、联网/OTA、诊断日志分别规定 priority、CPU/栈/队列预算和允许调用；控制路径不直接调用 TLS、DNS、云接口、动态证书操作或无界 Flash 写入。网络侧传来的命令经过权限、范围、时效与序列检查，以有界消息提交给控制状态机；控制侧保留本地联锁与超时策略。能使用 MPU/TrustZone 时进一步隔离域；仅依赖任务优先级时明确其不提供完整内存隔离。

| 工业场景 | 专项验证 | 产品必须确定的判据 |
|---|---|---|
| 控制 deadline | 同时执行 TLS 握手、最大网络包/断线重连、OTA 下载、日志拥塞与现场总线负载；测 ISR/任务延迟与 deadline miss | 控制周期与允许抖动、超时安全状态、最坏负载、可暂停 OTA 的状态 |
| RS485/Modbus | DE/RE 切换、turnaround、帧间隔、丢包/CRC 错误、串口 overrun、线路断开/短路、错误地址、设备无响应 | 重试/退避上界、重复命令是否幂等、bus owner、恢复时间；时序测量依据 baud rate 和协议 |
| CAN/CAN FD | 仲裁饱和、error passive/bus-off、收发 FIFO 溢出、断线重连、对端重启、乱序/旧序列；FD 支持只对实际硬件成立 | bus-off 恢复策略、失联联锁、消息 freshness/deadline、拥塞时关键帧保留与降级 |
| 电源与复位 | ramp-up/down、brownout、重复微断电、看门狗复位、上电时外设异常；检查复位早期输出与 bootloader 行为 | brownout 阈值、保持时间、输出默认态、复位原因保存、启动时间与安全输出时序 |
| Flash 寿命 | 参数热点写入、日志与配置共享区域、垃圾回收、接近满盘、目标温度/电压范围内 program/erase 失败 | 按实际写频率、datasheet endurance 和寿命估算验证 wear budget；合并写入、限流、热点分散与剩余空间 |
| EMC 现场故障 | 结合硬件团队安排 ESD/EFT/浪涌/传导与辐射扰动，观察总线/电源/复位/输出；HIL 回放已发现的通信与 reset 模式 | 按产品市场与适用标准制定试验等级；错误可检测、恢复有界、输出不进入危险态；实验室硬件报告归档 |

Flash erase、cache/总线竞争和 DMA 对控制实时性的影响必须在所选 MCU 上测量，不能假设低优先级任务天然避免 stall。对不能满足要求的单芯片方案，选择维护窗口、独立存储、双 MCU 或外部安全控制器等具体设计。远程参数变更采用快照校验与明确激活点，避免控制环读到一组半更新参数。

急停、隔离、保护器件、硬件联锁、接地/屏蔽、收发器与电源设计由产品电气和功能安全要求确定；Nexus 的软件检查、watchdog 与 HIL 不替代这些措施，也不能作为 EMC 或功能安全认证的证明。已知团队约 10 人，周期 3–6 个月，目标覆盖 STM32/GD32；具体 board、HIL 设备预算与控制目标仍待确定，所有绝对 deadline、寿命和功耗目标待产品需求与实测冻结。

## 4. 安全架构与设备全生命周期

安全边界至少包含设备、生产烧录站、更新签名服务、构建平台、设备管理后台与维修工具。威胁模型先划分远程、邻近网络、物理读取、调试口、供应链与内部操作风险，并把控制责任分配到实际硬件和服务；不要假定配置加密等同于设备安全。

### 4.1 启动信任与 OTA

建议评估受维护的 bootloader（例如 MCUboot）及可用平台集成，避免自行实现密码算法和签名协议。设计要求如下，属于目标架构，未核实为 Nexus 当前能力。

1. 第一可执行代码从硬件保护的 root of trust 验证后续镜像；公钥/公钥 hash、Flash 保护、调试锁及读取保护的能力按 SoC 明确记录。如果 MCU 无可信写保护/不可变根，对物理攻击的限制必须写入产品安全说明。
2. 签名 manifest 绑定产品、硬件兼容标识、版本、安全计数、镜像摘要、大小和依赖关系；设备先校验范围、边界与签名，再写入或激活。镜像认证是必选，镜像加密按业务要求选择；TLS 不能替代镜像签名。
3. 使用 A/B、swap 或其它可恢复布局，决定前先按板卡容量核算。下载可续传，候选启动有确认窗口和 watchdog；应用通过业务健康判据确认新版本，未确认可恢复到允许版本。
4. 将“回退到上一可用版本”和“防降级到已撤销的脆弱版本”分别建模。安全计数/最低允许版本采用硬件支持或可证明恢复的持久化协议；不能在新镜像确认前提前推进到使旧可用镜像无法恢复的状态。
5. Bootloader 更新要有独立的恢复设计；量产前验证 debugger/ROM boot/恢复镜像等实际可行路径。错误密钥、错误板型、容量不足、元数据损坏、整机断电都应给出确定行为。
6. 更新服务分批灰度，记录 eligibility、download、verify、activate、confirm、rollback 各阶段结果；暂停阈值按产品风险和样本量制定。离线设备的重试、证书到期和跨多版本跳升级包含在兼容测试中。

### 4.2 身份、密钥与生产

- 每台设备采用唯一身份和独立凭据；UID 是标识，不能当作秘密。优先利用 Secure Element/TrustZone/硬件密钥能力，能力不足时明确密钥读取风险。设备认证、固件签名、配置加密分别使用用途分离的 key，不能全线共用一把设备私钥。
- 生产提供 provisioning manifest、固件/工装版本、设备序列号、证书/公钥指纹和结果审计；工装不得保存全量生产签名私钥。适当设备可在硬件内生成私钥后提交 CSR；受限设备需经过批准的安全注入流程。
- 企业端签名 key 存于 KMS/HSM；CI 用短时身份调用受约束签名服务，产品/版本授权、审计和职责分离由服务策略完成。私钥不写入仓库、普通 CI secret 日志或发布包。
- 建立证书续期、撤销、离线有效期、RTC 不可靠和首次联网的信任策略；TLS peer/主机名验证不得用“暂时忽略”变成生产配置。
- 密钥轮换和配置 schema 迁移一同考虑可恢复事务。序列化记录应有 `format_version/schema_version/key_id/sequence/nonce/tag` 等显式元数据，读取旧格式受策略约束，迁移完成才退役旧 key。
- 生产 Shell、SWD/JTAG、恢复模式、维修权限采用产品 profile 控制。维修解锁应有授权和审计，退出后恢复正常保护；日志、crash dump 与导出接口默认屏蔽密码、私钥、token 和敏感业务字段。
- 退役包含解绑、证书撤销、凭据销毁、维修转售处理与设备后台状态；能否可靠清除 Flash 由存储实现和硬件验证证明。

## 5. 供应链、许可证与发布证据

根仓库 [LICENSE](https://github.com/X-Gen-Lab/nexus/blob/7a203266082ad1f655b6686713b2b7950ba31ec6/LICENSE#L1-L21) 为 MIT，这是已证事实。根许可证不能覆盖厂商子模块、RTOS、工具链库和新接入中间件；它们需要逐组件审查，尤其关注再分发、通知、源码提供及硬件使用限制。本文未完成所有子模块的许可证审计。

发布流水线目标顺序为：固定 source/配置 → 可重建构建 → SIL/资源检查 → HIL → 安全与许可证判定 → 生成证据/签名 → 候选试点 → 通过发布门禁后正式发布。企业维护工作流可以选择 GitHub/GitLab/Jenkins 实现，但证据字段保持一致。

每个产品/参考 profile 交付一个 evidence bundle，至少包括：

| 证据类别 | 内容 |
|---|---|
| 来源与配置 | source commit/tag、所有 gitlink/第三方依赖 commit、Kconfig 最终配置、board revision、链接脚本、toolchain/container digest |
| 产物 | ELF/bin/hex、map、调试符号、签名 manifest、每个工件 SHA-256、boot/app/schema 版本与兼容矩阵 |
| 测试 | 测试套件与执行数、通过/失败/skip 原因、覆盖范围、SIL seed、HIL 板卡与仪器标识、资源基线/结果 |
| 安全与许可 | 实际链接组件 SBOM（SPDX 或 CycloneDX）、开发工具单独清单、许可证文本/notice、漏洞扫描时点、VEX/豁免与期限 |
| 发布与运维 | 变更说明、已知问题、更新/回退/恢复说明、支持等级、EOL 日期、签名与可验证 provenance、责任人与审批记录 |

依赖源仓库锁定 commit，下载镜像/包锁定 hash，所有 Action 锁定完整 SHA；开发与发布使用最小权限的短时凭据，构建 job 与签名/发布 job 隔离。定期 dependency bot 升级有回归测试，不因“锁死版本”放弃安全补丁。对实际链接入固件的组件建立漏洞可达性判断，SBOM 全量匹配并不自动说明设备可利用。

发布门禁须拒绝：关键测试实际未执行、零测试、缺少必须硬件报告、必需产物缺失、摘要不匹配、超资源预算、未授权依赖许可、未处置高影响可达漏洞。报告归档步骤可以 `always()`，判定步骤不能吞失败。豁免须记录理由、范围、owner、补偿措施与过期时间，安全关键问题不允许靠格式化流水线报告绕过。

重建验证固定编译时间/路径等会影响确定性的输入；先比较 ELF 可重复部分和 unsigned image，再解释签名时间等合法差异。不能只因存在 Dockerfile 就宣称可重现构建或达到 SLSA 等级。

## 6. 现场可靠性与企业响应

设备最小诊断能力包含 build id、board id、boot/app version、复位原因、watchdog 触发来源、错误计数、队列背压、栈/heap 趋势与最后一次更新状态。日志使用有界 buffer、限流和异步导出；控制路径不能无限阻塞等待日志。时间源未知时同时保存 uptime、boot counter 与时间可信度，避免伪造绝对时间的诊断结论。

crash dump 应使用固定格式并关联 ELF/符号；存储空间有界，写入避免加剧原故障，上传前按产品数据政策过滤。指标与业务场景绑定，建议追踪启动成功率、OTA 各阶段成功率、确认/回滚率、异常复位次数、更新失败恢复时间、现场缺陷逃逸率和测试 flake 率。目标值应在参考板基线与产品试点后冻结，不能用无现场数据的数字宣称高可靠性。

漏洞响应建议明确安全联系人和受控报告渠道，由维护、产品、兼任安全职责的成员联合 triage，不假定 10 人团队另有专职安全部门。建议起始服务目标为 2 个工作日确认收到报告、5 个工作日完成初步影响/可达性判断；紧急遏制和补丁期限按产品风险、联网条件与客户合同制定。修复需覆盖 LTS 分支、SBOM/VEX、设备 eligibility 和撤回/更新路径，不仅合并主分支。现场事故保留证据，做因果分析和回归用例，区分硬件批次、应用、平台、配置和操作因素。

## 7. 分阶段落地与完成条件

下列时段按约 10 人和 3–6 个月规划，建议保留 2 人承担 QA/HIL（测试自动化与硬件验证各一主责，互为备份），与 BSP/应用成员共同建立板卡池和故障脚本。安全和发布是由资深维护者兼任的明确职责，评审需有第二位合格成员，不要求单设专职安全或发布岗位。具体排期仍依赖板卡到位、测试仪器预算、首发产品范围和已有实现可复用程度；质量平台不等待全部高级能力完成才产生价值。

| 阶段 | 工作包 | 完成条件 |
|---|---|---|
| 第 0–2 周：可信基线 | 纠正文档/支持等级；恢复 Native 与参考 ARM 构建；列出 P0；取消假成功；发布先验证后公布；留存现有测试结果 | 指定 commit 与 profile 可重复构建；测试执行证据可查；所有未实现能力显式拒绝/标记；未验证组合不能作为正式支持 |
| 第 3–6 周：契约与恢复 | 统一 OSAL 超时/销毁/ISR 契约；ASan/UBSan 与解析 fuzz；真实存储接口/恢复测试；参考板 HIL smoke、资源 map | MEM/SEM 风险关闭或明确限制；核心故障不变量通过；关键硬件行为有报告；无关键测试缺失或默认忽略 |
| 第 7–12 周 / 第 3 个月：一个参考工业试点 | 首发选定一套 STM32 或 GD32 参考板；验证实时控制与网络负载隔离、RS485/CAN 及配置恢复；按试点需求完成 RNG/AEAD、boot/OTA、身份与 SBOM | 一套板型形成完整契约与 HIL 证据；控制 deadline/恢复判据通过；涉及的安全 P0 关闭；OTA 若纳入试点必须通过篡改/错板/断电/回滚，不以未验证更新方式交付 |
| 第 4–6 个月：STM32 + GD32 双板候选 | 补齐第二套实际板卡 BSP；运行同一 HAL/OSAL 契约套件和两套 HIL；硬件独有能力单独声明；完善身份/安全更新、长期测试、证据与维修响应 | 两套板卡均有独立镜像/config/map/仪器报告；共享契约结果一致，差异有 capability 声明；各自的实时、总线、断电、资源和 OTA 场景通过；候选升级为验证等级须以证据决定，日历到期不自动晋级 |
| 第 6 个月后：企业支持与 LTS 准入 | 多批次硬件长测；LTS 回归池；漏洞响应/维修/退役；供应链升级演练 | 现场与实验室证据满足产品门槛；生命周期 owner 与预算确认；支持窗口和 EOL 可执行；行业适用项目单独形成认证计划 |

验收责任建议为：模块维护者负责契约与回归，BSP 维护者与 2 位 QA/HIL 成员共同负责硬件证据及能力矩阵，兼任安全负责人负责威胁模型/密码与身份/漏洞响应，兼任发布负责人负责来源和证据 bundle，产品负责人负责业务 deadline、安全状态与现场准入。兼任不意味着省略复核：签名/发布和安全关键变更由另一成员检查，QA/HIL 不负责代替产品方决定危险输出状态。每一项 P0/P1 转为有 owner、证据、验收不变量和依赖的任务；先形成可审查修复，再进入主线和产品发布。
