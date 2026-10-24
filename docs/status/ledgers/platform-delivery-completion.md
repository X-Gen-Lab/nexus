---
id: status.ledgers.platform-delivery-completion
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-platform-delivery-completion)=
# 平台交付完善14 项台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/platform-delivery-completion.csv)。
源文件：`docs/design/platform-delivery-completion.csv`；SHA-256：`31ec1589e03f3172b5cbf1d4ddb63d05deba831cbb4e524b14e043aeb20cb672`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 14 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-platform-delivery-completion-n01)=
## N01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N01 |
| `task` | USB EP0 告警闭环 |
| `implementation_status` | implemented |
| `software_status` | development_models_passed_remote_scan_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/src/controllers/usb_device_controller.c;tests/google/usb_device_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tests/google/usb_device_test.cpp;/tmp/nexus-next-20261024/provider/handoff.json |
| `exit_condition` | CodeQL 有证据处置；错误路径不误释放请求、clock 或借用。 |
| `owner` | codex |

(status-platform-delivery-completion-n02)=
## N02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N02 |
| `task` | 配置工具依赖收敛 |
| `implementation_status` | implemented |
| `software_status` | ir_and_generated_equivalence_verified_sdk_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;cmake/package/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/configuration/equivalence-green.json;/tmp/nexus-next-20261024/configuration/import-orders-green.log |
| `exit_condition` | 维护配置 IR 与生成结果等价；导入顺序测试通过；SDK 闭包完整。 |
| `owner` | codex |

(status-platform-delivery-completion-n03)=
## N03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N03 |
| `task` | 本轮扫描提示审查 |
| `implementation_status` | reviewed_fixes_implemented |
| `software_status` | focused_regressions_passed_current_scan_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/;tools/measurement/;tests/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/io-cost/handoff.json |
| `exit_condition` | 真缺陷修复并回归；合法用法与误报有明确依据；不靠扩大忽略范围消除新增问题。 |
| `owner` | codex |

(status-platform-delivery-completion-n04)=
## N04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N04 |
| `task` | 收束分支与 PR |
| `implementation_status` | pending_merge |
| `software_status` | pending_post_merge_source_association |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json |
| `exit_condition` | 最终 main 包含目标改动；示例 E 锁定明确的平台版本；合并后源码与验证对应。 |
| `owner` | codex |

(status-platform-delivery-completion-n05)=
## N05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N05 |
| `task` | 示例本地 CI 入口 |
| `implementation_status` | implemented |
| `software_status` | local_contracts_passed_final_pinned_run_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/scripts/local_ci.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;X-Gen-Lab/nexus-examples/scripts/local_ci.py |
| `exit_condition` | 新 checkout 以固定依赖、新输出完整执行；缺工具失败，旧报告拒绝。 |
| `owner` | codex |

(status-platform-delivery-completion-n06)=
## N06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N06 |
| `task` | 当前支持与验收视图 |
| `implementation_status` | implemented |
| `software_status` | selected_query_and_contracts_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py;docs/delivery/capabilities.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/testing/test_support_selection.py;/tmp/nexus-next-20261024/documentation/exact-support-observed.json |
| `exit_condition` | 能够查询精确配置支持什么、如何验证，以及哪些是参考配置或尚未实测。 |
| `owner` | codex |

(status-platform-delivery-completion-n07)=
## N07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N07 |
| `task` | 私有 Board 与 provider 接入 |
| `implementation_status` | implemented |
| `software_status` | external_board_contracts_passed_final_sdk_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;docs/guides/board-porting.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/configuration/final-focused-green.log;/tmp/nexus-next-20261024/examples/installed-board/result.json |
| `exit_condition` | 新板与同类实例经维护入口接入；不改无关 family，不猜测 PCB 事实。 |
| `owner` | codex |

(status-platform-delivery-completion-n08)=
## N08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N08 |
| `task` | 配置诊断与变更影响 |
| `implementation_status` | implemented |
| `software_status` | focused_diagnostics_and_diff_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;docs/guides/developer-workflow.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/configure/test_experience.py;/tmp/nexus-next-20261024/configuration/final-focused-green.log |
| `exit_condition` | 人读与 JSON 来自同一记录；冲突可定位；不自动换模式或补造未知事实。 |
| `owner` | codex |

(status-platform-delivery-completion-n09)=
## N09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N09 |
| `task` | 真实热点与执行效率优化 |
| `implementation_status` | implemented |
| `software_status` | measured_attempts_recorded_no_global_speedup_claim |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/process.py;tools/dev/timings.py;tools/configure/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/devtools/actual-resource-observation.json;/tmp/nexus-next-20261024/configuration/git-after-measurements.jsonl |
| `exit_condition` | 同环境 cold/warm 对比；不同编译上下文和失败诊断完整；不缓存成功测试报告。 |
| `owner` | codex |

(status-platform-delivery-completion-n10)=
## N10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N10 |
| `task` | 可比较 I/O 性能基线 |
| `implementation_status` | implemented_tooling |
| `software_status` | synthetic_compare_contracts_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/measure.py;tools/measurement/;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/testing/test_hil_measurement_compare.py;/tmp/nexus-next-20261024/io-cost/handoff.json |
| `exit_condition` | 每项结论关联同一镜像的原始测量，并提供优化前后对照；默认未选能力无额外资源；物理部分未取得证据前保持 pending。 |
| `owner` | codex |

(status-platform-delivery-completion-n11)=
## N11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N11 |
| `task` | 三板硬件验收 |
| `implementation_status` | implemented_tooling |
| `software_status` | existing_plans_current_source_preparation_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/fixtures/;tools/hil/prepare.py;tools/hil/measure.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/hil/fixtures/;tools/hil/prepare.py |
| `exit_condition` | 每板保留固件、设备身份、仪器记录和逐 mode 结果；未知明确保留；工装准备不能代替真实硬件退出条件。 |
| `owner` | codex |

(status-platform-delivery-completion-n12)=
## N12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N12 |
| `task` | H743 Flash 与 RAM 专项 |
| `implementation_status` | implemented |
| `software_status` | selected_arm_checks_passed_full_sdk_and_candidate_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/measurement/;soc/stm32/stm32h743/drivers/flash.c |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/provider/h743-arm-suite.json;/tmp/nexus-next-20261024/provider/h743-linked-costs.json |
| `exit_condition` | 当前精确 profile 有完整软件结论；实板另验 busy-bank、watchdog、掉电与 IRQ。 |
| `owner` | codex |

(status-platform-delivery-completion-n13)=
## N13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N13 |
| `task` | 当前正式软件候选 |
| `implementation_status` | existing_workflow |
| `software_status` | pending_final_clean_source_execution |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/evidence/reproduce.py;tools/evidence/candidate.py |
| `exit_condition` | 最终 clean source 执行完整 software plan；锁定环境两份独立断网构建实际比较 ELF/BIN；经既有流程 seal/verify。 |
| `owner` | codex |

(status-platform-delivery-completion-n14)=
## N14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N14 |
| `task` | 文档与原始证据维护 |
| `implementation_status` | implemented_documentation |
| `software_status` | source_gettext_bilingual_review_passed_final_archive_and_onboarding_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/;tools/documentation/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/documentation/final-checks.json |
| `exit_condition` | 当前状态从既有索引派生；历史保留原身份；一份 raw 归档与小索引并验证恢复；外部用户实际完成 install/create/build/run/upgrade。 |
| `owner` | codex |
