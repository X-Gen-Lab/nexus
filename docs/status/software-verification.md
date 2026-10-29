---
id: status.software-verification
type: status
audience: reviewer
source_language: zh_CN
---

(status-software-verification)=
# 软件验证、源码身份与证据

本页导航已有执行记录，不建立本次文档 HEAD 的平台软件资格。必须同时比对
源码、装配、依赖、工具、实际执行与同一 ELF／BIN／map 字节，不能只看 passed。
资格规则由 [资格合同](../delivery/qualification.md)维护。

## 三种身份分别解释

下面字段用于成功打包的正式站点与离线包。构建预览先查看
`documentation-build.json` 的 `source`、状态和原命令 receipts；预览尚没有正式
`documentation-manifest.json`，也不据此声称发行完成。

| 身份 | 取得位置 | 含义 |
| --- | --- | --- |
| 当前代码源码 | 站点 `documentation-manifest.json` 的 `code_source` | 本站描述的 checkout、tree、dirty 状态和代码路径 |
| 当前文档源码 | 同一 manifest 的 `documentation_source` | 本次正式文档包的精确 commit/tree；不替代预览构建记录 |
| 被引用资格源码 | schema 2 manifest 的 `qualification_references[]`；每项保存原候选身份与 scope | 旧的精确代码执行；不是当前代码／文档 HEAD 自动继承的资格 |

成功打包后，公开站点和离线包的 manifest 由实际构建输入生成。Git HEAD 可以因文档提交改变；
这里不手填一个很快漂移的 HEAD 常量。未打包的工作树可执行 `git rev-parse HEAD`
核对 checkout，但命令输出本身不是 qualification receipt。

schema 2 不再隐式引用某个历史索引。打包者逐个显式选入原封存候选，工具调用
同一候选 verifier 核对原始闭包后记录 source、configuration、ELF／BIN／map、
qualified scopes 与候选文件摘要。没有选入候选时列表为空，不表示软件失败。
文档包离线校验保护这份引用的字节和结构，不重新执行或代替原候选资格校验。
原 locator 只说明当时的存储位置，不是公开下载地址。schema 1 历史包保留原
`qualified_code` 的只读校验，不能用它生产新的默认资格引用。

(sealed-cold-lifecycle-baseline)=
## 最终冷态生命周期交付：7590b2d4

本节描述已完成的封存基线，后续代码与文档提交不自动继承它的资格。平台
`7590b2d457d6042112abb78a4f4df74d613df6ff`，tree
`17c18aea10066d5d7418f3a7fd3b42e619ee3414`，已通过
[PR 41](https://github.com/X-Gen-Lab/nexus/pull/41)合入原 `main`，没有另造
源码合并提交。对应外部示例为 `b029cc4b7e8e7a7d3e5e1f1be080a0aa24b1ee09`，
其固定 SDK gitlink 为上述平台提交；[示例 PR 18](https://github.com/X-Gen-Lab/nexus-examples/pull/18)
已合并。示例仓库当前为私有，平台的公开访问权限不包含示例访问权限。

公开的[平台交付索引](https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json)
固定于证据提交 `ecb86832fced0192d620b9029ff2d54795fc6430`。
先读取同目录的 `public-recovery-proof.json`：初始 payload 索引保留上传当时的
观察，恢复证明记录其后实际下载、摘要核对和独立恢复的结果，不能把初始
`pending` 当作最终结果。二者均保留原始字节，不相互改写。

| 范围 | 实际完成与界限 |
| --- | --- |
| 正式软件计划 | 同一 clean 平台源码的 159 个维护阶段全部实际通过；owned verifier 通过。原执行报告的 `software_qualified=false` 保留，计划通过与候选封存分别判断 |
| Native 四模式 | Debug、Release、ASan/UBSan、TSan 各执行 406 项 CTest、386 个 Google 程序、6819 个用例，零失败、错误或跳过；四个工具域共执行 3481 个 Python 用例，不与四模式重复相加 |
| 静态分析 | Clang-Tidy 与 Cppcheck 各分析 1135 个不同编译上下文；这是软件分析，不能替代当前源码的 CodeQL 关联或物理测量 |
| Source SDK | 50 个独立 consumer root、202 条命令及维护的精确 policy-v11 选择实际通过；53 个完整策略镜像由原计划逐项约束，不能仅凭计数替换选择 |
| 八个软件候选 | H743 HSI64/Cache off、U575 MSIS4/TZEN=0，各取 empty/UART 与裸机/FreeRTOS；16 次 fresh、断网构建及候选封存、校验和恢复完成，不扩大为高级 DMA/Cache/低功耗模式资格 |
| 中英文文档 | 原平台源码的 132 页、264 个语言记录、严格 API/gettext 构建、离线打包和独立恢复完成；文档包没有隐式候选引用，实际 Pages 部署仍未执行 |
| 远程证据恢复 | 本轮 335 个公开 payload member 的精确提交下载、SHA-256/bytes 核对完成，90 个原归档均独立恢复；原始失败和中断观察仍保留 |

候选封存原外层执行会话曾丢失，原外层 receipt 保持 `running/null`；24 个已结束
子命令及后续独立恢复证明各自真实完成。不能补造外层 exit 0，亦不因此丢弃
已经完成的子命令原件。合入主线仅执行身份与固定依赖同步，没有重跑或冒认新的
软件计划、构建或封存。

示例的[授权恢复入口](https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json)
固定于私有证据提交 `1baac7b7946a3d71e855e41365972b0781af5291`。使用已有
GitHub 仓库授权读取，不向公开平台归档复制示例源码、私有执行报告或归档。
没有示例权限的读者可使用公开 Source SDK；私有链接无法访问不表示平台恢复失败。

### 封存时尚未关闭的退出

| 工作 | 仍需实际执行的验收 |
| --- | --- |
| 三板 HIL 与性能 | 接入真实 station，执行启动、时钟、UART、DMA、停机、恢复，再测 IRQ 延迟、吞吐、功耗和栈峰值；目前均为 `not_executed` |
| OCI recipe 与新 daemon | 按锁定 recipe 独立完成两次 fresh 构建，并在新 daemon 实际 load/inspect/run；现有 OCI 字节恢复不建立 recipe 可复现性 |
| CodeQL、Pages 与服务端规则 | 取得当前源码的实际 CodeQL 结果及处置，实际部署并核对 Pages，以及服务端回读分支规则；本地门禁不代替这些事实 |
| 高级外部使用路径 | 7590 基线的 H743 Flash/RAM、U575 retained-time/Stop2 仍需独立外部 recipe；已有 C 机制与 SDK 链接检查不等同于完整使用教程或实板执行 |
| 历史示例证据边界 | 对既有公开示例证据逐项分类后决定保留、迁移或撤回；元数据清单不代表已审查所有归档内容，尚未改写历史 |
| 正式 Release | 明确本地 CI 的可信准入并执行发布；不伪造 Actions run，也不把无签名软件候选当作产品发布 |

当前 firmware 的硬件算法全部保留在维护的 C11 路径；Python 负责严格配置、静态
对象和固定调用装配。后续迭代优先改进上述使用、交付与实测闭环，不能从这份
封存摘要推导“全系列、全部外设、所有原退出已完成”。

## 后续维护观察：2026-10-30

本次维护从上述平台和示例基线分支继续。[现有 78 项台账](../design/platform-delivery-completion.csv)
只更新已有证据能够证明的状态与索引，并通过原 renderer 派生[可读视图](ledgers/platform-delivery-completion.md)。
全部原编号、顺序、退出条件、物理状态、历史 Git 版本、原报告与候选字节保持；
状态分别绑定原受测源码。正式资格仍属 `7590b2d4`，后续开发执行单独记录，
不以新文档 HEAD 覆盖资格源码，不能把该基线的 159 阶段结果重标到新源码。

平台增量 `dee8be5bb1052442a7853cf792b3fd589e17a9c9` 的普通完整索引门禁
实际通过：3565 个 Python 用例（testing 1755、configure 1156、CI 493、workflows
161），以及 386 个 Google 程序、6819 个用例，零失败或跳过。这是该提交的开发
门禁，不是新的完整 159 阶段正式资格，也不扩大旧候选范围。

私有示例后续提交 `ac9f07ba7494f472144f37660aabbd4185710394`（[PR 19](https://github.com/X-Gen-Lab/nexus-examples/pull/19)）
与 `5fb06175aab72292fb075e8a9ea67096203a86dc`（[PR 20](https://github.com/X-Gen-Lab/nexus-examples/pull/20)）
均已合并；示例当前主线为后者，SDK gitlink 仍固定已验证的 `7590b2d4`。最后普通
门禁实际通过 85 个 Python 用例、21 项 CTest、12 个 Google 程序和 129 个用例，
零跳过。H743 Flash/RAM 和 U575 retained WUT、SRAM4 copy、LPTIM1 单组 LPBAM
已有独立工程、实际 ARM 链接与资源检查；H743 的 RAM 执行闭包亦经审核。这补齐
E04 当前维护的选定高级 recipe，属于软件开发执行，不代表 HIL、通用 LPBAM、
新平台正式资格或新的软件候选。

初始 LPTIM 草稿早于对应测试的事实已在 PR 20 披露；后续真实 RED → GREEN
修复保留原执行。不用重建的源码补造早期测试顺序。并行 TDD 现要求测试 owner
先保留实际 RED、源码与二进制，再释放实现 owner 的交接屏障。

| 退出 | 本次实际观察 |
| --- | --- |
| GitHub 分支规则 | 写入仍返回 HTTP 403 `Resource not accessible by integration`；规则回读为空，不能声称保护已启用 |
| CodeQL 关联 | 当前 API 读取返回 HTTP 403；未取得可处置的当前源码扫描结果 |
| Pages 部署 | 尚未实际部署；本地 HTML 和归档恢复不代替远端站点核对 |
| OCI 重建与新 daemon | 可用容量未满足既有准备估算，尚未启动两次 fresh recipe 构建或新 daemon 执行；旧 OCI 字节身份及不等价边界保留 |
| 首发三板工装 | 三板 prepare 与 measurement plan 共六条 CLI 实际返回 0，状态仍为 `prepared_unbound`；实板采集和性能测量均未执行 |
| 历史发布边界 | 私有留存与当前公开分支 tip 的 21 项 member 撤下已实际完成；旧提交、缓存与 clone 仍可能保留原数据，未改写历史 |

这些外部退出保持开放，软件实现继续推进。规则写入失败的原命令与诊断位于
执行主机的 `build/completion-20261030/remote/`；这是原主机定位，不是公开下载
URL。需要当前完整记录时，先核对后续提交的实际交付索引，再按其固定身份恢复。

历史发布边界的实际处置见[公开撤下说明](https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md)。
私有留存的[授权恢复索引](https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json)
固定于 `8aa825c7d2cd8f58f4fa84869f16700aea400783`。本次正常 hook、非强制推送、
精确提交回读与成员核对已执行；未改写旧提交，不把当前 tip 撤下称作历史擦除。
当前 `7590b2d4` 平台恢复证明与示例私有 `1baac7b7` 入口仍保持原身份。

F04 的结论保留原审查的合同／oracle 映射层级，不扩大为全项目每个历史用例
均已人工证明语义独立。真实硬件、完整受控 cold/warm 对比与环境重建等原退出
仍由同一台账保留，不能因完整软件计划已通过而批量改成完成。

## 历史独立结果索引

| 范围 | 精确代码源码 | 原权威及执行说明 |
| --- | --- | --- |
| C 冷态与工程交付增量 | `194ca6cb`；`d21dfcab`；`364f3665`；后续工作树分别记录 | [同一结果索引](../delivery/platform-delivery-completion-results.json)的 `current_iteration_20261028` 保留各自源码、普通门禁、ARM、SDK、文档和归档；[原 64 项退出](../design/platform-delivery-completion.csv)没有缩减。开发结果不代表最终合并源码的完整资格 |
| 组织托管依赖与受控交付 | N2 `ce616ea9b30fd2bbd9511f73e2f3b5d8028224b4`；E1 `cef24bd1c7bc500fb28a17a6e1ff9cc7d5511711` | 原正式软件计划 159 阶段通过；外部示例 98 条命令通过；精确配置候选的六个 scope 已通过，原候选仍无签名，物理 HIL 未执行，产品晋升未执行 |
| 类型化 IR、冷态接入与交付统一 | P2 `fc01d863c3374643c276195ae19a24e6f8b8c8ee`；E `acb5179ec09e0c2fae1911d32655296df39acedc` | [精确执行说明](../delivery/verification.md)；Q02 完整 159 阶段通过，E 完整本地执行通过；精确配置候选及六个 scope 已通过；Q01 失败单独保留，远端状态发布未确认，物理 HIL 未执行 |
| SoC/IP 目录整理与原候选 | P `92e669233957a2e450a528394e8f4dd0794c7049`；E `0f15022e4d5f5c51991b134d0b9d6082dcf88de9` | [执行说明](../delivery/verification.md)、[48 项原台账](../design/soc-ip-layout-execution.csv)、[派生视图](ledgers/soc-ip-layout.md)；同一 P 的 A90＋B69 分段覆盖，原候选只覆盖其精确配置，物理 HIL 未执行 |
| 开发体验迭代 P 与示例 E | P `c8df1c49d4f771a55490b3a8be32a9d1f24faa1e`；E `acb923cae407c8cfc4ac5788f8026d35c84cc841` | [结果索引](../delivery/developer-experience-results.json)保留本地完整 gate、SDK 独立消费、示例 ARM 和原始归档；不是正式软件候选 |
| 构建系统及其实际软件 scopes | `cc6a065c002041234a212a964be863a69198f9b4` | [结果索引](../delivery/build-system-results.json)、[执行记录](../delivery/build-system-verification.md) |
| SoC 扩展软件切片 | `98270512ea2ff7eefe8ff79720018e582be3a949` | [结果索引](../delivery/soc-extensions-results.json)、[执行记录](../delivery/soc-extensions-verification.md) |
| 较早 SoC refinement 基线 | `3728b334b02eb2eb70bcf5d4caafbb9746837dd3` | [结果索引](../delivery/soc-refinement-results.json)、[执行记录](../delivery/soc-refinement-verification.md) |

N2/E1 的原本地候选导航分别记录在外部示例仓库的
`build/controlled-dependencies-ce616ea9/delivery-index.json` 和
`build/documentation-controlled-dependencies-ce616ea9/delivery-index.json`。
前者定位原候选、产物与可移植证据；后者定位双语文档包和独立离线恢复记录，
文档发布仍为 `published=false`。这些路径是原主机留存位置，不是公开下载 URL。
Nexus 合并提交 `3ea252c6` 与受测 N2 的 Git tree 相同，示例合并提交 `4a9f5f75`
与受测 E1 的 Git tree 相同；关联保留原受测提交，不改写候选源码。本次后续 C
生命周期增量中，`194ca6cb` 已完成其精确源码的 SDK 消费与严格双语文档交付；
`d21dfcab` 已完成其普通完整索引门禁。该索引中的后续 pending 字段属于当时的
开发观察；最终 `7590b2d4` 的门禁、SDK、示例、文档、双构建与封存见本页前节。
各家族开发计数不可相加，也不覆盖四种 Native 模式或实板验收。

各索引保留原 schema、范围、失败尝试、receipt、归档和 member 身份，不能相加
测试次数、ARM 镜像数或任务数量，也不能据此计算全项目完成率。后续文档 commit
只记录这些基线。当前文档的检查、渲染、离线恢复与部署验证各自有独立记录。

构建系统记录没有执行一次完整 72-stage 软件总计划，也没有执行 OCI recipe 重建、
CP311 离线安装、Windows/macOS 集成或 MCU HIL。独立执行过的具体 scopes 按原索引
解释；不要从其 aggregate 状态扩大结论。其八个无签名软件候选是 H743 HSI64／
Cache off 和 U575 MSIS4／TZEN=0 的 empty／UART、裸机／FreeRTOS 组合。

## 如何取得与核对旧证据

1. 先打开对应结果索引，确认 canonical code commit、配置和记录 scope。
2. 使用索引中的归档定位，核对归档 SHA-256 和 bytes；记录失败尝试的原身份。
3. 根据 `archive_relative_path`、`archive_member_relative_path` 等原字段定位 member，
   继续核对该 member 的 hash 和 bytes。字段名因原 schema 而不同，按该索引解释。
4. 查看实际执行记录、原始日志和同一产物。软件模型、链接、独立消费和两次断网
   重建分别证明自己的范围，某一类成功不替代另一类。
5. 物理记录按 [HIL 状态](hardware.md)另行判断。

原 `/workspace/...`、`/dev/shm/...` 是执行主机定位，离开该主机不能当作公开下载。
构建系统索引的 `evidence_navigation` 明确候选归档、candidate document、ELF／BIN／map
和全部 member 清单的定位方式。归档当前是原记录的本地留存；没有发布的远程证据
不能伪造下载 URL。文档包包含原 JSON 以便核对，并不复制大型原始归档或重签历史字节。

![软件与物理证据分别绑定精确源码和执行范围](../assets/diagrams/qualification-evidence.svg)

FIG-11 表达资格关系，不生成 receipt。标准与图源依据见 [图册](../assets/diagrams/README.md)。
逐项原退出条件见 [台账视图](ledgers.md)。
