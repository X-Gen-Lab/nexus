# 当前证据与验收边界

本页说明平台执行报告的范围与验收边界。每次迭代都按实际源码重新建立证据，
旧版本候选、历史测试数量和开发目录不继承为新 HEAD 的资格。实施台账的
`implemented` 表示源码和工具已落地；验收状态以报告声明的受测代码身份为准。
原始报告保留各自输出路径，绑定源码、配置、依赖、工具、命令返回码和产物。

## 类型化平台统一：精确源码与执行边界

本轮代码提交 P2 为 `fc01d863c3374643c276195ae19a24e6f8b8c8ee`，tree 为
`e6f3d6310fd46265be49d47ef3fd7a4d944a8386`；对应
[平台 PR 38](https://github.com/X-Gen-Lab/nexus/pull/38)。外部示例 E 为
`acb5179ec09e0c2fae1911d32655296df39acedc`，tree 为
`4e4e504c28766f0790dc4cd4dc45e8f9bcc49beb`；对应
[示例 PR 16](https://github.com/X-Gen-Lab/nexus-examples/pull/16)。
PR 状态与软件执行是不同事实；本节不宣称上述 PR 已合并。

候选 `typed-unification-fc01d863c337-acb5179ec09e` 已封存并经 owned verifier
实际校验通过，状态为 `software_qualified`，覆盖 `host_contracts`、`arm_link`、
`resource_budget`、`source_sdk`、`reproducibility` 和 `hil_tooling` 六个原 scope。
配置 SHA-256 为 `0117c83ff7876bf3867338f31caec160c18815a629cb1c44bfe1a4ead410590b`；
候选 JSON 为 2,366,210 bytes，SHA-256 为
`f1c2af3ac73cb219b41b6d68f1c1b07db108357d0de8a92107ab7b1e8ec1ede6`。
同一 ELF／BIN／map 的字节身份由原交付索引逐项记录。`hil_tooling` 只证明工装范围；
物理 HIL 仍为 `not_executed`，签名为 `unsigned`，产品提升为 `not_performed`。

R02 已完成两次独立 fresh、`--network none` 构建，共四条实际配置／构建命令，
均返回 0；两个独立输出的 ELF／BIN 逐字节一致，原结果为 `bit_exact_elf_bin`。
所选配置是外部示例 `reproduction/assemblies/qiming-baremetal.toml`：启明 F407ZG、
裸机、`hse8-pll168`、`Os`、GPIO 输出和 UART1 IRQ events（容量 32），显式选择
`uart-owner` 组件。该结果不扩展为其他板卡、模式或实板时序的复现结论。

持久导航入口是
`/workspace/nexus-examples/build/platform-candidate-fc01d863c337-acb5179ec09e-01/delivery-index.json`，
4,716 bytes，SHA-256 为
`5e99131826172355a95506b0293c8fe98f75cb07f3b6298f5f3805d6008caf43`。
其中 `candidate_portable` 指向 6,441 个文件的完整归档：434,092,822 bytes，
SHA-256 为 `a443e9c3b39528d5d5fbbdd19ee1c065496cd86b8df78cb5380aab4505bf3aa9`；
`finalization_package` 指向保留原收尾报告和失败尝试的独立归档。
前述 `/tmp/...` 是原执行定位，环境恢复后不保证仍存在；按索引及 member 映射
恢复原字节，不修改原 locator。该主机路径不是公开下载 URL。

Q02 已按完整维护计划独立执行全部 159 阶段，状态为 `executed_passed`。
前后源码观察完全一致，均为 clean P2；原报告
`/tmp/nexus-unification/formal-fc01d863c337-02/report.json` 为 201,097 bytes，
SHA-256 为 `8c67786b80b5fb64cb0f6f216c1dbf4c05ddb84f61ab2ef4542fb0f55e0910a2`。
维护工具已核对全部阶段、原执行顺序、命令返回码、计划和源码身份。
这是一次完整 Q02 执行，没有拼接 Q01 前缀；报告中的 `software_qualified=false`
保持原值，软件计划通过不等同于候选已封存。

示例 E 的完整本地执行已通过：17 个 preset（1 个 Native、16 个 ARM）、98 条实际命令、39 个 ELF 预算检查
及 12 项 HIL 准备。HIL 输出为 `prepared_unbound`，不代表已连接实板或物理通过。
原执行 `/tmp/nexus-examples-typed-fc01d863c337-all-001/execution.json`
为 15,662 bytes，SHA-256 为
`c89682f4b2b30383dca2c3e845aad6170c6f4271b310886d72deb358b136348b`。
E 实际消费 `/tmp/nexus-unification/formal-fc01d863c337-01/sdk`；原命令保持该
真实路径，不改写为 Q02 SDK。Q01 计划失败与其中已经生成的 SDK 是不同范围，
示例结果不反向将 Q01 计划变成通过。

R01 准备阶段通过维护的 `verify_source_sdk` 分别完整校验了 Q01、Q02 的 SDK，
确认 2,931 个源码文件、相同 source manifest 字节和两份 CMake export 文件一致；
记录为 `verified_identical_installed_sdk_bytes`。两个 manifest 均为 370,911 bytes，
SHA-256 为 `77c5fa4e0eb0c584b195ce92a59e8ac6129dc5d493c3e1184df89a9a5618248d`。
原结果位于
`/tmp/nexus-unification/candidate-fc01d863c337-acb5179ec09e-01/readiness.json`，
8,263 bytes，SHA-256 为
`378fff8b25917b86bf88d4a9e5a2613947e0f4219c6f6c94bc6cf390babfaee0`。
该记录保留 E 的真实 Q01 SDK 输入；Q02 是本轮唯一完整软件计划权威。
SDK 字节等价本身不等同于双构建或候选通过。

恢复后对 clean P2 checkout 重新执行维护的 `capabilities.py --candidate` 查询，
实际返回 0，`source_match=true`，但结果为 `configuration_mismatch`。
按路径比较输入集合，SDK 侧额外包含摘要为
`77c5fa4e0eb0c584b195ce92a59e8ac6129dc5d493c3e1184df89a9a5618248d`
的 `.nexus-source-sdk.json`；输入记录顺序也不同，其余配置语义字段相同。
因此仅 `configuration=validated`，模型、ARM 镜像、SDK、候选及物理查询结果仍为
`not_assessed`，不因源码相同而放宽完整配置身份校验。新观察保存在
`/workspace/nexus/.git/local-recovery/typed-platform-unification/capability-recovery/comparison.json`，
SHA-256 为 `565993a80527d4e3c2e5af7ca7a00dd4448665023b5cee18560d0ba1050f0d87`。
已封存产物的正向验证入口是恢复候选后运行 `candidate.py verify --candidate`；
它与源码 checkout 的能力关联查询分别说明自己的范围。

Q01 的前 90 阶段通过，第 91 阶段因锁定 `clang-tidy` 未进入执行 PATH 而失败，
该阶段未完成 C 静态分析。补齐环境并通过版本、真实 C11 probe 和 doctor 后，
Q02 重新完整执行；没有拼接 Q01 前缀。Q01 的失败事实仍保留；原临时报告与命令记录在环境重置后已丢失，
不能重新核验，也未改写为成功。

候选首次准备 R01 的 `prepare-spec` 已成功；随后辅助脚本因工具链的 65-byte
installer marker 摘要口径不一致，在 Docker 启动前停止。独立 inventory 确认
其余字节与锁定归档一致，工具链未修改；R01 原记录保留在收尾归档中。
该准备失败不冒充 R02 已实际完成的断网双构建。

代码执行身份 P2 与文档提交身份分别记录。文档更新、构建和发布不会生成新的
平台软件资格。正式 schema 2 文档包仅在候选实际校验通过后显式设置
`qualification_references`；其文档源码以本次包的 `documentation_source`
和构建 receipt 为准，被引用候选继续保留原 P2 身份。

`local_ci.py` 已对完整 Q02 进行只读验证，实际返回 0；结果为
`/tmp/nexus-unification/root/local-ci-full-verified.json`，2,385 bytes，SHA-256
为 `2bb9bbcfb51c61d26f49435b4dc6cb3650cdae340db55d7e8c4aea7386e991ca`。
尝试向精确 P2 发布 `nexus/local-software-plan` 时，GitHub 返回 HTTP 403，
指出连接缺少 `statuses:write`。原 receipt
`/tmp/nexus-unification/local-status-fc01d863.json` 的 publication 为
`unconfirmed`；3,872 bytes，SHA-256 为
`4fa133eb62e7767048450743710f9ec292d349dbcaf3faaf062e258508992f2a`。
本次独立服务端回读该 SHA 的 statuses 为空，不能宣称状态已发布或远端绿灯。
本地验证不冒充 Actions jobs、发布流水线或已启用的分支保护。

## 历史 SoC/IP 目录整理执行

本次 SoC/IP 重构的软件执行绑定代码提交 P
`92e669233957a2e450a528394e8f4dd0794c7049`，tree 为
`6e38d4c43d59330a0a9a9b7cac21f591d9c2719f`。
平台 PR 36 已合并为 `c79fd9672a9fe56379744d834cfccd822bc40be1`，tree 与 P 相同；
外部 examples PR 15 已合并为 `bd3b90f135600b6405459347cb9e2cff5692d2e3`，
保留正式 P pin。[48 项重构台账](../design/soc-ip-layout-execution.csv)记录逐项结果
和原始证据。本次维护计划 159 项由同一 P 的两段实际执行覆盖：A 保留的前
90 项完整结果，加上 B 对原计划后 69 项的完整执行。

环境重启中断了 A 的静态分析。A 原报告仍为 `running`，没有 `source_after`；
独立中断观察记录后来观察到的源码和启动身份，重启原因未确定。B 按原后缀计划
完整重跑静态分析，结果为 `executed_passed`。精确的阶段、顺序、命令退出码和
源码身份已核对；这不是一次完整计划运行通过，未补造 A 的结束状态。
搬移 SDK 独立消费、外部 examples 的十六个 ARM 配置和三十二个主矩阵 ELF、
十一组资源比较及两次 fresh、network-none 离线构建均有独立结果。
既有六类 scope 的候选封存与 owned verifier 已通过，候选状态为
`software_qualified`；签名仍为 `unsigned`，产品提升未执行，物理 HIL 为
`not_executed`。

本文档更新只记录 P 的软件证据。文档自身身份由对应
`documentation-build.json` 的 `source` 和实际执行收条确定；文档提交不继承或
替换 P 的软件资格。失败和中断尝试保留原状态与原始证据，不把 A/B 覆盖重标为
一次 `executed_passed`，也不把中断阶段的部分工作计为完成。

| 范围 | 维护的执行入口 | 资格边界 |
|---|---|---|
| Native／Core／OS／组件 | `tools/dev/dev.py test --preset native-debug`，另执行 release、ASan／UBSan 和 TSan | 实际软件所有权、线程、故障行为；不构成 MCU 电气资格 |
| GoogleTest／GoogleMock | `scripts/ci/tdd_gate.py --all`；`tests/google/` | 全量发现与实际非过滤执行、fresh XML、零跳过；不是历史编辑顺序的自动证明 |
| Cortex-M CPU 机制 | `tests/google/arch_*`、`*atomic*`；`scripts/ci/arch_compile.py` | 九核宏／ABI／对象／指令／原子依赖；生产 cache/MPU/SAU 模型；不扩大 SoC/Board 或实板支持 |
| CPU runtime / kernel | `scripts/ci/cortex_runtime.py`；`tests/contracts/cortex_runtime/` | 公共 `nexus_add_runtime` 生产源图、Core/Arch/I/O/所选 OS 与真实 kernel context 的编译/ELF 审计；不是 MCU 执行 |
| 生成器 | `tools/testing/run_tool_tests.py --suite configure` | 平台/CPU runtime TOML、冻结 IR、显式可选能力/ABI、维护的 Native factory 类型、资源冲突、错误输入和 stale bundle |
| 工具门禁 | `tools/testing/run_tool_tests.py --suite testing`；`tests/contracts/test_*.py`；`scripts/ci/` | 实际负路径、暂存区稳定、报告身份和执行退出码 |
| STM／GD 寄存器模型 | `tests/google/*dma*`、`*uart_stream*`、`*adc_stream*`、`gd32_multi_test.cpp` 与已有 C contracts | 真实生产 TU；内存／线上排空、EN 拒绝关闭、迟到 IRQ、借用／隔离、校准／稳定 |
| 三板基础配置 | 六个 ARM presets | 实际 startup／vector／provider 链接、ELF／BIN／map 和预算 |
| H743／U575 参考纵切片 | `scripts/ci/stm32h743.py`、`scripts/ci/stm32u575.py` | 各自 empty／selected × 裸机／FreeRTOS，实际 vector／资源／保留；H743 另查 pre-RAM 电源入口，不证明 MCU 实际运行 |
| 可选 RAM 放置 | `scripts/ci/stm32_memory.py`、`scripts/ci/gd32_memory.py` | F407 CCM、GD ADDSRAM／TCM 的链接和初始化域；不证明掉电／reset retention |
| 高级 controller 夹具 | `tests/contracts/{stm32,gd32}_assembly/*.toml` | 维护的 controller 类型、DMA、块接收／触发、多控制器实际 ARM 链接；明确是软件 Board |
| 最小资源矩阵 | `tools/measurement/compare.py` | 三板 60 个 O2／Os／O3 ± LTO 镜像；18 empty GC 检查；实际 face／ops 存储成本 |
| 外部示例 | 独立 `X-Gen-Lab/nexus-examples` | 本轮正式 pin P 后 Native 运行与十六个 ARM 配置、三十二个主矩阵 ELF（`platform_reference`／`uart_owner_runtime`）；专用示例另列于原报告，产品实现留在外部 |
| 源码 SDK | `cmake/package/package_source_sdk.py`、`verify_consumers.py` | 干净版本、source closure/identity、移动路径 C／C++ Native 和 Runtime consumer、STM／GD 固件链接 |
| 离线复现／候选 | `tools/evidence/reproduce.py`、`candidate.py`、`proof.py` | 两次独立 network-none 构建，相同源码／配置／工具／ELF 身份及必需 scope |
| HIL | `tools/hil/prepare.py`、`admit.py`、三板 fixture 与 station template | 工装已准备；未连接设备，所有物理结果 `not_executed` |

此前 SoC 扩展迭代的 canonical commit、门禁与候选证据见
[SoC 扩展执行记录](soc-extensions-verification.md)；
[早期基线记录](soc-refinement-verification.md) 保留原版本证据。
这些历史结果保持原源码范围，不构成本次重构的资格。
版本化 datasheet／manual／errata
索引只建立来源；实际 silicon revision 和各 mode 的 workaround 适用性单独验收。
`soc_costs.py` 保存真实 ELF／反汇编／工具调用与可比 profile，不将不同 backend、
route 或容量的大小差解释为同配置优化，也不把指令数量当作测得 cycles。

`tools/hil/soc_acceptance.py` 对 SOC-37–55 提供十九项 plan/capture/validate
合同。plan 只产生 `prepared_unbound`；参考 G474/H743/U575 即使改名或重新计算
metadata hash，也不能跳过 reference-only admission。真实 station 与工具输入、
协议、仪器、连续非空 samples 和 reviewed limits 一起决定结果。物理 Flash
掉电还需外部产品提供擦除对齐且避开全部 firmware LOAD 的显式测试布局。

此前开发执行目录 `build/controller-routes-factory/` 与
`build/minimal-factory-resources/` 保留独立 attempt 和原始命令，不覆盖旧运行。资源
数字只描述对应 ELF；源码变化后重新执行。MSP 显式预留，heap=0；未使用的内存域
不因芯片具有该区域就自动驻留。汇编和 ELF 字节不能替代实际 cycles／IRQ latency。

此前 CPU/runtime 开发执行目录为 `/workspace/nexus-all-cortex-evidence`，保存
CPU 事实/schema、原子 publication/RMW、九核寄存器模型、可选 feature 编译与
真实 kernel context 审计各自的原始报告。公开报告记录实际命令、退出码、工具
与 hash；通过数和 feature 范围由执行结果读取，不继承此前数字或一种组合。
Source/installed Runtime consumer 与完整平台 consumer 分开建立证据，前者
没有具体 startup、SoC、Board 或 factory，不能替代后者或 candidate 封存。

RED 日志保存在 `tools/testing/evidence/`，GREEN 为真实框架执行。既有正确合同的
迁移明确标识，不为了制造 RED 而修改生产行为。commit hook 开始和结束检查暂存
快照及 index tree，拒绝执行期间发生的行为变更；失败不发布新的成功摘要。

正式软件资格要求 clean source/dependency、准确工具环境与配置、非零 fresh tests、
同一 ELF／BIN／map、当前预算、源码 SDK 独立消费与离线双构建。支持范围以
[精确支持矩阵](support.md) 为准，不把软件夹具的 connector 继承到参考 Board。

所有物理时序、精度、波形、reset、掉电和长期负载项目尚未执行。FP/MVE context
切换、IRQ latency、cache/DMA coherence、MPU/SAU 和安全世界协作也未进行实板
验收；可选 restricted MPU task 和 split runtime 的软件执行不能建立实板隔离
资格。实际 HIL 必须
记录 PCB、chip UID、probe／serial／power、station、预算和同一候选 image。
