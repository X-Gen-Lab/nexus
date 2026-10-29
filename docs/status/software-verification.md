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

## 已有独立结果索引

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
`d21dfcab` 已完成其普通完整索引门禁。后来集成的家族与所有权修正仍须执行最终
源码的完整门禁、SDK、示例、文档、离线双构建与封存。各家族开发计数不可相加，
也不覆盖四种 Native 模式或实板验收。

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
