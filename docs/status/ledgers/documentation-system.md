---
id: status.ledgers.documentation-system
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-documentation-system)=
# 文档系统台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/documentation-system-execution.csv)。
源文件：`docs/design/documentation-system-execution.csv`；SHA-256：`490f5b52001bf236d76f6407d09ff2266cf4f6d2c7e672fc14b18cff96e9f94b`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 58 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-documentation-system-doc-01)=
## DOC-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-01 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 登记页面权威与读者任务 |
| `dependencies` | （原字段为空） |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 逐页明确正文、API、支持、资格、台账和历史来源；目录和唯一导航可审查 |
| `owner` | codex |
| `implementation_paths` | docs/design/documentation-migration.csv;docs/index.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-02)=
## DOC-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-02 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 修正根入口与支持口径 |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | README中typed factory、TOML、ThreadX、连续模式与精确支持一致；不扩大实板范围 |
| `owner` | codex |
| `implementation_paths` | README.md;README_CN.md;docs/status/support.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-03)=
## DOC-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-03 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 修正可执行配置模板 |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | Board示例含soc_family；完整Board和CPU/TOML经实际解析；片段明确不建立完整配置资格 |
| `owner` | codex |
| `implementation_paths` | docs/design/integration-contracts.md;tools/configure/documentation.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-04)=
## DOC-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-04 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 统一最新资格导航 |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 当前代码、文档HEAD、qualified source和历史记录分别可定位；不继承旧执行 |
| `owner` | codex |
| `implementation_paths` | docs/status/software-verification.md;docs/status/hardware.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-05)=
## DOC-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-05 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 逐台账映射原退出条件与新证据 |
| `dependencies` | DOC-04 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | OS48和已过期状态按scope映射；原编号/条件/失败记录保留；不批量标passed |
| `owner` | codex |
| `implementation_paths` | tools/documentation/status.py;docs/status/ledgers.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-06)=
## DOC-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-06 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 建立迁移引用与旧URL清单 |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 每个当前内容源有去向；相对链接/anchor/工具输入/旧URL的处理规则齐全 |
| `owner` | codex |
| `implementation_paths` | docs/reference/history.md;docs/reference/redirects.csv |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-07)=
## DOC-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-07 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 选择并接入MyST |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 兼容维护Sphinx/Breathe；完整Markdown与RST正负样例实际构建；pin明确 |
| `owner` | codex |
| `implementation_paths` | docs/sphinx/conf.py;docs/sphinx/requirements.txt |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-08)=
## DOC-08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-08 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 调整Sphinx source root与排除范围 |
| `dependencies` | DOC-07 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | docs为source root；正文直接索引；archive/生成输出/大证据不误入站点 |
| `owner` | codex |
| `implementation_paths` | docs/sphinx/conf.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-09)=
## DOC-09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-09 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 实施唯一任务导航 |
| `dependencies` | DOC-08;DOC-06 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 首页提供使用/接入/维护/验收/迁移入口；当前页面可达可搜索；无第二套正文 |
| `owner` | codex |
| `implementation_paths` | docs/index.md;docs/reference/index.md;docs/guides/index.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-10)=
## DOC-10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-10 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 重组公共API和factory说明 |
| `dependencies` | DOC-08 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | Core/Arch/I/O/OS/Components模块页连到符号；factory示例基于实际生成接口 |
| `owner` | codex |
| `implementation_paths` | docs/reference/api/index.md;docs/explanation/factory.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-11)=
## DOC-11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-11 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 接入系统合同与架构解释 |
| `dependencies` | DOC-08;DOC-06 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 生命周期和安全限制保持原规范；合同与解释分别有一份authority |
| `owner` | codex |
| `implementation_paths` | docs/explanation/platform-architecture.md;docs/explanation/ownership.md;docs/delivery/architecture.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-12)=
## DOC-12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-12 |
| `priority` | P2 |
| `phase` | D2 |
| `task` | 接入源码相邻README |
| `dependencies` | DOC-08;DOC-06 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 保留实现地图和SoC局部权威；include相对引用正确或源码链接绑定站点revision |
| `owner` | codex |
| `implementation_paths` | docs/explanation/source-map.md;docs/development/implementation-map.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-13)=
## DOC-13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-13 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付外部SDK入门教程 |
| `dependencies` | DOC-03;DOC-09;DOC-27 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 固定版本的真实外部consumer配置/构建/运行或链接成功；保留实际命令和清理步骤 |
| `owner` | codex |
| `implementation_paths` | docs/tutorials/source-sdk.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-14)=
## DOC-14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-14 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付异步I/O与取消排空教程 |
| `dependencies` | DOC-13;DOC-10 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 真实示例覆盖reject/accept/cancel/drain/settled/quarantine；无借用提前回收 |
| `owner` | codex |
| `implementation_paths` | docs/tutorials/async-io.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-15)=
## DOC-15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-15 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付Board接入指南 |
| `dependencies` | DOC-03;DOC-09 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 可验证模板、路由授权、来源和unknown明确；已有SoC复用；软件与PCB资格分开 |
| `owner` | codex |
| `implementation_paths` | docs/guides/board-porting.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-16)=
## DOC-16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-16 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付SoC/provider/组件接入指南 |
| `dependencies` | DOC-03;DOC-11 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 按纵切片列facts/startup/time/provider/测试/SDK/HIL条件；通用器件不强制进生成器 |
| `owner` | codex |
| `implementation_paths` | docs/guides/provider-porting.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-17)=
## DOC-17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-17 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付OS等待与停机指南 |
| `dependencies` | DOC-02;DOC-11 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 后端语义、执行推进、真实删除和存储回收条件清晰；示例经维护后端验证 |
| `owner` | codex |
| `implementation_paths` | docs/guides/os-shutdown.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-18)=
## DOC-18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-18 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付旧配置与接口迁移指南 |
| `dependencies` | DOC-02;DOC-09 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 旧HAL/OSAL/Kconfig到当前TOML/factory有可执行转换路径；旧ID不跨bundle持久化 |
| `owner` | codex |
| `implementation_paths` | docs/guides/migration.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-19)=
## DOC-19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-19 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付配置和工具参考 |
| `dependencies` | DOC-10;DOC-11 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 字段/单位/拒绝规则和CLI来自维护实现；API声明不手抄；无重复配置权威 |
| `owner` | codex |
| `implementation_paths` | docs/reference/configuration/assembly.md;docs/reference/configuration/tools.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-20)=
## DOC-20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-20 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付性能诊断与HIL准备指南 |
| `dependencies` | DOC-09;DOC-04 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 区分ELF预算/实测cycles/stack/HIL；工具准备不声称已执行硬件 |
| `owner` | codex |
| `implementation_paths` | docs/guides/performance-hil.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-21)=
## DOC-21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-21 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 确定双语术语与发布关键页 |
| `dependencies` | DOC-01 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 原文语言和不可翻译标识明确；关键页范围固定；不要求复制CSV或原始日志 |
| `owner` | codex |
| `implementation_paths` | docs/locale/terminology.csv |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-22)=
## DOC-22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-22 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 实施gettext与翻译源版本检查 |
| `dependencies` | DOC-07;DOC-21 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 只有一条翻译链；中文/英文原文分别做en/zh_CN构建；缺译fallback与stale/fuzzy按实际正文语言和源版本标记 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/sphinx_ext.py;docs/locale |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-23)=
## DOC-23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-23 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 完成关键使用路径双语审查 |
| `dependencies` | DOC-13;DOC-14;DOC-15;DOC-17;DOC-18;DOC-22 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 入门/配置/factory/借用/支持/资格/迁移关键正文独立审查；缺失语言不假报完整 |
| `owner` | codex |
| `implementation_paths` | docs/locale;docs/locale/terminology.csv |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-24)=
## DOC-24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-24 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 显示真实页面语言与翻译状态 |
| `dependencies` | DOC-22 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | reviewed/stale/missing/original在页和入口可见；英文fallback不标中文已译；locale不覆盖source_language |
| `owner` | codex |
| `implementation_paths` | tools/documentation/sphinx_ext.py;docs/sphinx/conf.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-25)=
## DOC-25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-25 |
| `priority` | P2 |
| `phase` | D4 |
| `task` | 整理本地图示与离线资源 |
| `dependencies` | DOC-08 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 图源可审查、箭头与契约一致；图像/字体/脚本不依赖CDN；新增渲染工具受锁；DOT/SVG成对；Graphviz及实际字体受锁；SVG无外部资源/脚本/DOCTYPE |
| `owner` | codex |
| `implementation_paths` | tools/documentation/diagrams.py;docs/assets/diagrams/diagram-index.csv |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-26)=
## DOC-26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-26 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 完成读者任务走查 |
| `dependencies` | DOC-13;DOC-14;DOC-15;DOC-16;DOC-17;DOC-18;DOC-19;DOC-20;DOC-24;DOC-42 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 独立agent从入口完成任务；错误步骤/不可达合同/版本混淆修正；记录实际结果；按内容标准独立语义审查 |
| `owner` | codex |
| `implementation_paths` | docs/index.md;docs/status/index.md;docs/tutorials/index.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-27)=
## DOC-27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-27 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 统一docs CLI与旧脚本转发 |
| `dependencies` | DOC-08 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | check/build/preview/package/doctor目标明确；本地与CI严格参数一致；失败码与诊断保留 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/cli.py;tools/dev/dev.py;scripts/tools/docs.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-28)=
## DOC-28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-28 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 建立离线链接anchor导航检查 |
| `dependencies` | DOC-06;DOC-09;DOC-27 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 故意坏文件/anchor/重复ID/不可达页实际失败；合法跨页引用和迁移别名实际通过 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/check.py;tools/documentation/delivery.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-29)=
## DOC-29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-29 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 扩大实际配置片段验证 |
| `dependencies` | DOC-03;DOC-27 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 新路径/RST/完整Board和CPU输入经实际resolver；语法片段不升级可执行资格 |
| `owner` | codex |
| `implementation_paths` | tools/configure/documentation.py;tools/documentation/check.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-30)=
## DOC-30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-30 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 关联可执行代码和consumer |
| `dependencies` | DOC-13;DOC-27 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | literalinclude来源和测试身份可追溯；维护示例真实构建；不任意执行文档shell |
| `owner` | codex |
| `implementation_paths` | docs/tutorials/source-sdk.md;docs/tutorials/async-io.md;cmake/package/verify_consumers.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-31)=
## DOC-31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-31 |
| `priority` | P2 |
| `phase` | D5 |
| `task` | 建立有界外部链接检查 |
| `dependencies` | DOC-28 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 404/版本错误与网络失败分别报告；timeout/retry有界；commit默认无需网络 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/check.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-32)=
## DOC-32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-32 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 接入文档commit与严格CI门禁 |
| `dependencies` | DOC-27;DOC-28;DOC-29;DOC-30 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 快速离线commit检查和完整文档CI执行；工具负路径TDD；正文调整不伪造固件验收；图资源机器检查与正文/图语义审查分工明确 |
| `owner` | codex |
| `implementation_paths` | .pre-commit-config.yaml;.github/workflows/docs-build.yml;tools/documentation/check.py;tools/documentation/test_status.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-33)=
## DOC-33

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-33 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 锁定并实际安装文档工具闭包 |
| `dependencies` | DOC-07;DOC-27 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | Python全部传递wheel hash及Doxygen身份锁定；干净维护tuple实际安装/构建成功；Graphviz与实际字体闭包实际验证 |
| `owner` | codex |
| `implementation_paths` | dependencies/docs-tools.lock.json;dependencies/docs-python-linux-cp312.txt;tools/documentation/install.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-34)=
## DOC-34

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-34 |
| `priority` | P1 |
| `phase` | D5 |
| `task` | 隔离preview构建与发布权限 |
| `dependencies` | DOC-32;DOC-33 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | PR artifact带source身份；可信部署只消费验证输出；不在高权限执行外部PR源码 |
| `owner` | codex |
| `implementation_paths` | .github/workflows/docs-build.yml;.github/workflows/docs-publish.yml |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-35)=
## DOC-35

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-35 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 生成版本与资格manifest |
| `dependencies` | DOC-04;DOC-24;DOC-27;DOC-33 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 平台版本由既有policy读取；code/docs/qualified source独立；outputs只列payload；不包含manifest/checksums/archive或间接hash自引用 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/delivery.py;docs/reference/documentation-delivery.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-36)=
## DOC-36

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-36 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 生成相对链接离线HTML包 |
| `dependencies` | DOC-25;DOC-32;DOC-35 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 包含语言/API/manifest/校验/许可；脱离workspace可浏览验真；无hash自引用 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/delivery.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-37)=
## DOC-37

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-37 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 发布不可变版本与stable入口 |
| `dependencies` | DOC-23;DOC-26;DOC-34;DOC-36;DOC-31;DOC-58 |
| `implementation_status` | implemented |
| `validation_status` | pending_formal_evidence |
| `exit_condition` | release-id绑定manifest且不覆盖；实际部署成功；stable不宣称HIL全部通过 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/delivery.py;.github/workflows/docs-publish.yml |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-38)=
## DOC-38

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-38 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 执行发布后站点与下载验证 |
| `dependencies` | DOC-37 |
| `implementation_status` | implemented |
| `validation_status` | pending_formal_evidence |
| `exit_condition` | 在线身份与部署包一致；关键页面/旧URL/图/下载校验通过；旧API不静默映射新合同 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/delivery.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-39)=
## DOC-39

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-39 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 完成独立恢复与长期存储 |
| `dependencies` | DOC-36;DOC-37 |
| `implementation_status` | implemented |
| `validation_status` | pending_formal_evidence |
| `exit_condition` | 稳定取得位置和保留策略明确；独立根恢复浏览验真；异地恢复须真实执行 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/delivery.py;tools/evidence/store.py |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-40)=
## DOC-40

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-40 |
| `priority` | P1 |
| `phase` | D6 |
| `task` | 收尾迁移并独立交付审查 |
| `dependencies` | DOC-06;DOC-38;DOC-39;DOC-41;DOC-42;DOC-58 |
| `implementation_status` | implemented |
| `validation_status` | pending_formal_evidence |
| `exit_condition` | 映射全部处理；无重复规范/断链/错误资格；原证据保留；58项范围及15类必需图满足退出条件；未声明支持的配置、HIL和后续扩展如实保留边界 |
| `owner` | codex |
| `implementation_paths` | docs/design/documentation-migration.csv;docs/status/documentation-acceptance.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-41)=
## DOC-41

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-41 |
| `priority` | P1 |
| `phase` | D1 |
| `task` | 应用正文写作与语义标准 |
| `dependencies` | DOC-01;DOC-21 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 关键页面明确读者/目标/配置；术语/单位/规范强度/事实与资格通过独立审查 |
| `owner` | codex |
| `implementation_paths` | docs/design/documentation-standards.md;docs/development/documentation.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-42)=
## DOC-42

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-42 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 应用五类模板与逐页内容审查 |
| `dependencies` | DOC-41;DOC-11;DOC-13;DOC-14;DOC-15;DOC-16;DOC-17;DOC-18;DOC-19;DOC-20 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | tutorial/how-to/reference/explanation/status关键页面按模板走查；缺项修正或理由明确；metadata不替代内容合格 |
| `owner` | codex |
| `implementation_paths` | docs/design/documentation-templates.md;docs/tutorials;docs/guides;docs/reference;docs/explanation;docs/status |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-43)=
## DOC-43

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-43 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 交付全平台架构与外部边界图 |
| `dependencies` | DOC-11;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | Core/Arch/SoC/Board/I/O/可选OS组件和外部产品责任清楚；factory取接口与直接I/O区分 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/platform-architecture.dot;docs/assets/diagrams/platform-architecture.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-44)=
## DOC-44

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-44 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 交付真实编译目标依赖图 |
| `dependencies` | DOC-08;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 实际CMake目标和private/public依赖核对；厂商实现私有；不把生成或运行箭头当编译依赖 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/build-targets.dot;docs/assets/diagrams/build-targets.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-45)=
## DOC-45

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-45 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 交付配置与生成流程图 |
| `dependencies` | DOC-03;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | TOML/facts/typed IR/原子bundle/CMake/Ninja产物和失败失效准确；未知Board事实不修复为已知 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/configuration-generation.dot;docs/assets/diagrams/configuration-generation.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-46)=
## DOC-46

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-46 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 交付factory和实例结构图 |
| `dependencies` | DOC-10;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 静态face/共享ops/独立context与controller/endpoint对应源码；getter无分配或初始化 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/factory-instances.dot;docs/assets/diagrams/factory-instances.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-47)=
## DOC-47

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-47 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付初始化停机与失败撤回图 |
| `dependencies` | DOC-11;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | prepare/start/stop/withdraw按实际选定流程；部分效果和逆序撤回责任明确；不自动启动scheduler |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/platform-lifecycle.dot;docs/assets/diagrams/platform-lifecycle.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-48)=
## DOC-48

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-48 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付请求状态与借用图 |
| `dependencies` | DOC-14;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | admission结果与enum状态分开；取消/deadline仍借用；SETTLED和观察者/slot释放条件明确 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/request-lifecycle.dot;docs/assets/diagrams/request-lifecycle.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-49)=
## DOC-49

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-49 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付UART和SPI DMA执行时序 |
| `dependencies` | DOC-14;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 选定模式正常与取消/排空失败路径；Task/IRQ/DMA域、DMA TC/wire complete/SETTLED分开 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/dma-completion.dot;docs/assets/diagrams/dma-completion.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-50)=
## DOC-50

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-50 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付wait wake和OS退出图 |
| `dependencies` | DOC-17;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | ready-arm-ready-wait-ready、deadline和执行推进责任准确；实际join/delete后回收存储 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/wait-wake-shutdown.dot;docs/assets/diagrams/wait-wake-shutdown.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-51)=
## DOC-51

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-51 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付内存DMA和Cache参考图 |
| `dependencies` | DOC-20;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 精确芯片/模式/内存域/可达性/对齐和完整line维护；不从CPU名推导DMA或coherence |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/h743-memory-cache.dot;docs/assets/diagrams/h743-memory-cache.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-52)=
## DOC-52

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-52 |
| `priority` | P1 |
| `phase` | D3 |
| `task` | 交付Source SDK消费流程图 |
| `dependencies` | DOC-13;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 真实搬移外部consumer与源码树自测试分开；配置/构建/运行或链接的scope准确 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/source-sdk-consumer.dot;docs/assets/diagrams/source-sdk-consumer.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-53)=
## DOC-53

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-53 |
| `priority` | P1 |
| `phase` | D2 |
| `task` | 交付软件与HIL证据关系图 |
| `dependencies` | DOC-04;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 源码/配置/工具/receipt与候选身份关联；软件和HIL分别实际取得；无自动物理资格 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/qualification-evidence.dot;docs/assets/diagrams/qualification-evidence.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-54)=
## DOC-54

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-54 |
| `priority` | P2 |
| `phase` | D6 |
| `task` | 交付发布与独立恢复图 |
| `dependencies` | DOC-35;DOC-36;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 文档/SDK/档案交付渠道与身份准确；目标和实际已发布分开；独立取得/恢复/验真边界明确 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/release-recovery.dot;docs/assets/diagrams/release-recovery.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-55)=
## DOC-55

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-55 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付Cortex M能力分层图 |
| `dependencies` | DOC-19;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | CPU原语/Runtime/SoC/Board/HIL分层；FPU/MVE/Cache/安全能力和实际port不互相继承 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/cortex-m-capabilities.dot;docs/assets/diagrams/cortex-m-capabilities.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-56)=
## DOC-56

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-56 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付U575同步双域参考图 |
| `dependencies` | DOC-20;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | 限定独立TZEN1无OS同步参考；SAU/MPU/GTZC/gateway和seal失败责任；无通用NS RTOS/async声明 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/u575-security-domains.dot;docs/assets/diagrams/u575-security-domains.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-57)=
## DOC-57

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-57 |
| `priority` | P2 |
| `phase` | D3 |
| `task` | 交付连续流与可见丢块图 |
| `dependencies` | DOC-14;DOC-25;DOC-41 |
| `implementation_status` | implemented |
| `validation_status` | development_passed |
| `exit_condition` | producer/block/consumer/loan/overrun/stop准确；服务与容量条件明确；不承诺无条件零丢失 |
| `owner` | codex |
| `implementation_paths` | docs/assets/diagrams/continuous-stream.dot;docs/assets/diagrams/continuous-stream.svg;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |

(status-documentation-system-doc-58)=
## DOC-58

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DOC-58 |
| `priority` | P1 |
| `phase` | D4 |
| `task` | 执行图文一致性与视觉离线审查 |
| `dependencies` | DOC-25;DOC-28;DOC-43;DOC-44;DOC-45;DOC-46;DOC-47;DOC-48;DOC-49;DOC-50;DOC-51;DOC-52;DOC-53;DOC-54;DOC-55;DOC-56;DOC-57 |
| `implementation_status` | implemented |
| `validation_status` | pending_formal_evidence |
| `exit_condition` | 每条边和状态核对来源；图源/输出/alt/caption齐全；SVG安全资源与字体；桌面/窄屏放大/打印/离线实际验证，未执行不标通过 |
| `owner` | codex |
| `implementation_paths` | tools/documentation/diagrams.py;docs/assets/styles/nexus.css;docs/assets/diagrams/README.md |
| `evidence_paths` | docs/status/documentation-results.json;docs/status/documentation-acceptance.md |
