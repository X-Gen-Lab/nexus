---
id: status.ledgers.platform-delivery-completion
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-platform-delivery-completion)=
# 平台交付完善台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/platform-delivery-completion.csv)。
源文件：`docs/design/platform-delivery-completion.csv`；SHA-256：`470e8cade4da32551804d327d4a83d691a78864e0235dd4dfb7eb1dc4c3df210`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 78 条原记录，仅表示来源行数。

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
| `software_status` | native_regressions_passed_at_7590b2d4_codeql_access_403_still_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/src/controllers/usb_device_controller.c;tests/google/usb_device_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tests/google/usb_device_test.cpp;/tmp/nexus-next-20261024/provider/handoff.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;build/completion-20261030/remote/codeql-access.log |
| `exit_condition` | CodeQL 有证据处置；错误路径不误释放请求、clock 或借用。 |
| `owner` | codex |

(status-platform-delivery-completion-n02)=
## N02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N02 |
| `task` | 配置工具依赖收敛 |
| `implementation_status` | implemented |
| `software_status` | ir_generated_import_and_sdk_contracts_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;cmake/package/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/configuration/equivalence-green.json;/tmp/nexus-next-20261024/configuration/import-orders-green.log;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 维护配置 IR 与生成结果等价；导入顺序测试通过；SDK 闭包完整。 |
| `owner` | codex |

(status-platform-delivery-completion-n03)=
## N03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N03 |
| `task` | 本轮扫描提示审查 |
| `implementation_status` | reviewed_fixes_implemented |
| `software_status` | final_local_analyzers_passed_at_7590b2d4_current_codeql_disposition_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/;tools/measurement/;tests/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/io-cost/handoff.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;build/completion-20261030/remote/codeql-access.log |
| `exit_condition` | 真缺陷修复并回归；合法用法与误报有明确依据；不靠扩大忽略范围消除新增问题。 |
| `owner` | codex |

(status-platform-delivery-completion-n04)=
## N04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N04 |
| `task` | 收束分支与 PR |
| `implementation_status` | implemented |
| `software_status` | main_7590b2d4_examples_b029cc4b_post_merge_source_association_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;build/c-cold-20261028/integration/final-main-sync/final-delivery-observation.json |
| `exit_condition` | 最终 main 包含目标改动；示例 E 锁定明确的平台版本；合并后源码与验证对应。 |
| `owner` | codex |

(status-platform-delivery-completion-n05)=
## N05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N05 |
| `task` | 示例本地 CI 入口 |
| `implementation_status` | implemented |
| `software_status` | final_pinned_examples_b029cc4b_full_local_ci_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/scripts/local_ci.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;X-Gen-Lab/nexus-examples/scripts/local_ci.py;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json |
| `exit_condition` | 新 checkout 以固定依赖、新输出完整执行；缺工具失败，旧报告拒绝。 |
| `owner` | codex |

(status-platform-delivery-completion-n06)=
## N06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N06 |
| `task` | 当前支持与验收视图 |
| `implementation_status` | implemented |
| `software_status` | exact_support_queries_and_contracts_passed_at_7590b2d4_no_physical_promotion |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py;docs/delivery/capabilities.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/testing/test_support_selection.py;/tmp/nexus-next-20261024/documentation/exact-support-observed.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 能够查询精确配置支持什么、如何验证，以及哪些是参考配置或尚未实测。 |
| `owner` | codex |

(status-platform-delivery-completion-n07)=
## N07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N07 |
| `task` | 私有 Board 与 provider 接入 |
| `implementation_status` | implemented |
| `software_status` | external_board_provider_and_installed_sdk_contracts_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;docs/guides/board-porting.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/configuration/final-focused-green.log;/tmp/nexus-next-20261024/examples/installed-board/result.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 新板与同类实例经维护入口接入；不改无关 family，不猜测 PCB 事实。 |
| `owner` | codex |

(status-platform-delivery-completion-n08)=
## N08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N08 |
| `task` | 配置诊断与变更影响 |
| `implementation_status` | implemented |
| `software_status` | diagnostics_diff_and_rejection_contracts_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;docs/guides/developer-workflow.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/configure/test_experience.py;/tmp/nexus-next-20261024/configuration/final-focused-green.log;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 人读与 JSON 来自同一记录；冲突可定位；不自动换模式或补造未知事实。 |
| `owner` | codex |

(status-platform-delivery-completion-n09)=
## N09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N09 |
| `task` | 真实热点与执行效率优化 |
| `implementation_status` | implemented |
| `software_status` | final_stage_costs_observed_at_7590b2d4_complete_controlled_cold_warm_comparison_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/process.py;tools/dev/timings.py;tools/configure/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/devtools/actual-resource-observation.json;/tmp/nexus-next-20261024/configuration/git-after-measurements.jsonl;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 同环境 cold/warm 对比；不同编译上下文和失败诊断完整；不缓存成功测试报告。 |
| `owner` | codex |

(status-platform-delivery-completion-n10)=
## N10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N10 |
| `task` | 可比较 I/O 性能基线 |
| `implementation_status` | implemented_tooling |
| `software_status` | linked_resource_and_compare_contracts_passed_at_7590b2d4_physical_comparison_open |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/measure.py;tools/measurement/;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/testing/test_hil_measurement_compare.py;/tmp/nexus-next-20261024/io-cost/handoff.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 每项结论关联同一镜像的原始测量，并提供优化前后对照；默认未选能力无额外资源；物理部分未取得证据前保持 pending。 |
| `owner` | codex |

(status-platform-delivery-completion-n11)=
## N11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N11 |
| `task` | 三板硬件验收 |
| `implementation_status` | implemented_tooling |
| `software_status` | formal_plan_hil_tooling_prepared_at_7590b2d4_no_physical_execution |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/fixtures/;tools/hil/prepare.py;tools/hil/measure.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/hil/fixtures/;tools/hil/prepare.py;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 每板保留固件、设备身份、仪器记录和逐 mode 结果；未知明确保留；工装准备不能代替真实硬件退出条件。 |
| `owner` | codex |

(status-platform-delivery-completion-n12)=
## N12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N12 |
| `task` | H743 Flash 与 RAM 专项 |
| `implementation_status` | implemented |
| `software_status` | selected_flash_ram_models_arm_and_sdk_passed_at_7590b2d4_physical_exit_open |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/measurement/;soc/stm32/stm32h743/drivers/flash.c |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/provider/h743-arm-suite.json;/tmp/nexus-next-20261024/provider/h743-linked-costs.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 当前精确 profile 有完整软件结论；实板另验 busy-bank、watchdog、掉电与 IRQ。 |
| `owner` | codex |

(status-platform-delivery-completion-n13)=
## N13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N13 |
| `task` | 当前正式软件候选 |
| `implementation_status` | implemented |
| `software_status` | eight_exact_candidates_software_qualified_at_7590b2d4_other_configurations_not_promoted |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/evidence/reproduce.py;tools/evidence/candidate.py;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | 最终 clean source 执行完整 software plan；锁定环境两份独立断网构建实际比较 ELF/BIN；经既有流程 seal/verify。 |
| `owner` | codex |

(status-platform-delivery-completion-n14)=
## N14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N14 |
| `task` | 文档与原始证据维护 |
| `implementation_status` | implemented_documentation |
| `software_status` | sdk_docs_starters_upgrade_and_remote_recovery_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/;tools/documentation/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/documentation/final-checks.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json |
| `exit_condition` | 当前状态从既有索引派生；历史保留原身份；一份 raw 归档与小索引并验证恢复；外部用户实际完成 install/create/build/run/upgrade。 |
| `owner` | codex |

(status-platform-delivery-completion-a01)=
## A01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | A01 |
| `task` | Maintained coverage inventory |
| `implementation_status` | implemented |
| `software_status` | maintained_selection_inventory_recorded_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/matrix.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/baseline-support.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every maintained part mode backend and clock/security condition is enumerated from existing facts and catalogue |
| `owner` | codex |

(status-platform-delivery-completion-a02)=
## A02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | A02 |
| `task` | End-to-end implementation ownership |
| `implementation_status` | partial |
| `software_status` | final_c_path_audits_and_50_consumer_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/matrix.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every maintained path maps authored input IR generator C lifecycle face tests and external consumer |
| `owner` | codex |

(status-platform-delivery-completion-a03)=
## A03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | A03 |
| `task` | Behavior and linked cost baseline |
| `implementation_status` | partial |
| `software_status` | source_bound_linked_cost_observations_and_final_resource_checks_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/matrix.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Exact historical and current observations are retained with source and configuration identity |
| `owner` | codex |

(status-platform-delivery-completion-b01)=
## B01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | B01 |
| `task` | C firmware and host generator boundary |
| `implementation_status` | implemented |
| `software_status` | maintained_c_hardware_boundary_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;soc;drivers;tests/google |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;AGENTS.md;docs/guides/provider-porting.md;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | C owns hardware behavior; Python owns validation static declarations and connections; existing style remains |
| `owner` | codex |

(status-platform-delivery-completion-b02)=
## B02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | B02 |
| `task` | Concrete lifecycle contracts |
| `implementation_status` | implemented |
| `software_status` | concrete_c_lifecycle_contracts_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;soc;drivers;tests/google |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Context ownership preconditions partial effects cleanup retry and irreversible peripheral behavior are explicit |
| `owner` | codex |

(status-platform-delivery-completion-b03)=
## B03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | B03 |
| `task` | Unique resource receipts |
| `implementation_status` | implemented |
| `software_status` | c_resource_receipt_ownership_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;soc;drivers;tests/google |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Pins clocks IRQ DMA and shared controller state have one restoration owner without duplicate persistent snapshots |
| `owner` | codex |

(status-platform-delivery-completion-b04)=
## B04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | B04 |
| `task` | Lifecycle failure regressions |
| `implementation_status` | implemented |
| `software_status` | recorded_lifecycle_red_green_and_final_native_modes_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;soc;drivers;tests/google |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Real C tests cover refusal partial acquire cleanup failure retained retry and instance isolation before implementation |
| `owner` | codex |

(status-platform-delivery-completion-b05)=
## B05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | B05 |
| `task` | Cold migration linked cost |
| `implementation_status` | implemented |
| `software_status` | selected_unselected_arm_budget_and_retention_checks_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | AGENTS.md;soc;drivers;tests/google |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Selected and unselected actual ARM images preserve bounded state and no mandatory allocation workers or locks |
| `owner` | codex |

(status-platform-delivery-completion-c01)=
## C01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C01 |
| `task` | Native complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_native_c_lifecycle_four_modes_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/legacy;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | All maintained Native modes and enhanced endpoints have C behavior and public consumer regressions |
| `owner` | codex |

(status-platform-delivery-completion-c02)=
## C02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C02 |
| `task` | STM32F103 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_f103_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/legacy;build/c-cold-20261028/legacy/ordinary-gate/package.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every maintained mode including AFIO channel DMA IRQ I2C and all resource emission has C hardware behavior |
| `owner` | codex |

(status-platform-delivery-completion-c03)=
## C03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C03 |
| `task` | GD32F303 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_gd303_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/legacy;build/c-cold-20261028/legacy/ordinary-gate/package.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every maintained mode including RCU AFIO ADC divider channel DMA and resource emission has C hardware behavior |
| `owner` | codex |

(status-platform-delivery-completion-c04)=
## C04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C04 |
| `task` | STM32F407 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_f407_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/f407;build/c-cold-20261028/f407/full-family-package.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | All maintained modes primary and secondary controllers and platform resource lifecycle move hardware behavior to C |
| `owner` | codex |

(status-platform-delivery-completion-c05)=
## C05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C05 |
| `task` | GD32F470 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_gd470_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/gd470;/dev/shm/nexus-next-gd470/build/gd470-cold/family-increment-paths.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | All maintained modes primary and secondary controllers and platform resource lifecycle move hardware behavior to C |
| `owner` | codex |

(status-platform-delivery-completion-c06)=
## C06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C06 |
| `task` | STM32G474 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_g474_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/modern;/dev/shm/nexus-next-g474/build/cold-g474/handoff.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Basic named-pin shared/coherent/coupled timer ADC groups encoder FDCAN and resource lifecycle close |
| `owner` | codex |

(status-platform-delivery-completion-c07)=
## C07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C07 |
| `task` | STM32H743 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_h743_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/modern;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Basic FIFO finite/continuous DMA cache flash and fixed-run resource paths have C behavior |
| `owner` | codex |

(status-platform-delivery-completion-c08)=
## C08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | C08 |
| `task` | STM32U575 complete maintained lifecycle |
| `implementation_status` | implemented |
| `software_status` | maintained_u575_c_lifecycle_models_arm_and_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tests/google;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/modern;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Basic FIFO GPDMA chain ADC4 RTC LPTIM retained domains and resource paths have C behavior |
| `owner` | codex |

(status-platform-delivery-completion-x01)=
## X01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | X01 |
| `task` | Platform preflight and resource lifecycle |
| `implementation_status` | implemented |
| `software_status` | c_preflight_and_resource_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Hardware reads admission pin capture restore readback clock and IRQ resource algorithms live in C |
| `owner` | codex |

(status-platform-delivery-completion-x02)=
## X02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | X02 |
| `task` | Secondary and shared physical owners |
| `implementation_status` | implemented |
| `software_status` | secondary_shared_owner_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Each physical controller starts/stops once; selected ownership and multi-instance isolation remain exact |
| `owner` | codex |

(status-platform-delivery-completion-x03)=
## X03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | X03 |
| `task` | Enhanced and packet endpoints |
| `implementation_status` | implemented |
| `software_status` | packet_endpoint_and_shared_domain_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/legacy;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | SPI outer engine CAN filter USB activation Ethernet and MDIO failure/stop ordering close |
| `owner` | codex |

(status-platform-delivery-completion-x04)=
## X04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | X04 |
| `task` | H743 pause resume and Flash execution |
| `implementation_status` | implemented |
| `software_status` | h743_fixed_run_and_flash_ram_closure_passed_at_7590b2d4_physical_exit_open |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Same C resource lifecycle is reused; actual public Flash retention and ARM closure remain verified separately |
| `owner` | codex |

(status-platform-delivery-completion-x05)=
## X05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | X05 |
| `task` | Lowpower I2S2 retained special paths |
| `implementation_status` | implemented |
| `software_status` | selected_retained_i2s2_and_lowpower_c_paths_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_executed |
| `implementation_paths` | soc;tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Declaration-only selection remains Python; existing C paths regress; special retained components are explicitly audited |
| `owner` | codex |

(status-platform-delivery-completion-d01)=
## D01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D01 |
| `task` | Canonical controller category contracts |
| `implementation_status` | implemented |
| `software_status` | canonical_category_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/contracts/green-final.log;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Authored fields selected fields kind order and public header mappings share immutable category metadata |
| `owner` | codex |

(status-platform-delivery-completion-d02)=
## D02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D02 |
| `task` | Resolver stage ownership |
| `implementation_status` | implemented |
| `software_status` | frozen_stage_and_device_graph_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/config;build/c-cold-20261028/device-graph-stage/handoff.json;build/c-cold-20261028/fact-stage-increment/handoff.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Fact selection memory layout dependencies controllers endpoints and resources use named stage results without reinterpreting TOML |
| `owner` | codex |

(status-platform-delivery-completion-d03)=
## D03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D03 |
| `task` | Complete selected integration consumption |
| `implementation_status` | implemented |
| `software_status` | canonical_selected_consumption_audit_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/integration/d03/results-01.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | All maintained constructors and resource emitters consume canonical selected IRQ DMA clock pin and owner records |
| `owner` | codex |

(status-platform-delivery-completion-d04)=
## D04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D04 |
| `task` | Exact clock and DMA models |
| `implementation_status` | implemented |
| `software_status` | exact_clock_and_dma_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Channel stream DMAMUX GPDMA and real kernel clock differences retain exact contracts without F4-shaped guesses |
| `owner` | codex |

(status-platform-delivery-completion-d05)=
## D05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D05 |
| `task` | Validate all exact part variants |
| `implementation_status` | implemented |
| `software_status` | exact_part_rejection_and_boundary_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/config;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Unselected package Flash memory DMA overlap and PartPolicy defects are rejected through common validation |
| `owner` | codex |

(status-platform-delivery-completion-d06)=
## D06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D06 |
| `task` | Logical runtime power references |
| `implementation_status` | implemented |
| `software_status` | logical_runtime_reference_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/config;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Renamed selected ID works with matching references; fixed physical SPI4 mode/backend/clock restrictions remain |
| `owner` | codex |

(status-platform-delivery-completion-d07)=
## D07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | D07 |
| `task` | Configuration and SDK input closure |
| `implementation_status` | implemented |
| `software_status` | configuration_payload_and_installed_sdk_closure_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every added module and private payload participates in identity stale-input invalidation and installed SDK consumption |
| `owner` | codex |

(status-platform-delivery-completion-e01)=
## E01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E01 |
| `task` | All-mode public consumption |
| `implementation_status` | partial |
| `software_status` | maintained_public_consumption_and_50_consumer_sdk_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Each maintained mode works with only public and generated headers including exact storage and typed factory |
| `owner` | codex |

(status-platform-delivery-completion-e02)=
## E02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E02 |
| `task` | Startup shutdown recipes |
| `implementation_status` | implemented |
| `software_status` | pinned_examples_startup_shutdown_recipes_passed_at_7590b2d4_examples_b029cc4b |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json |
| `exit_condition` | Submitter admission service settle drain join and platform stop responsibilities are compilable and explicit |
| `owner` | codex |

(status-platform-delivery-completion-e03)=
## E03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E03 |
| `task` | Failure and recovery recipes |
| `implementation_status` | implemented |
| `software_status` | pinned_examples_failure_recovery_recipes_passed_at_7590b2d4_examples_b029cc4b |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json |
| `exit_condition` | Timeout cancellation and failed drain retain loans; quarantine and recovery responsibility remain explicit |
| `owner` | codex |

(status-platform-delivery-completion-e04)=
## E04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E04 |
| `task` | Advanced usage recipes |
| `implementation_status` | implemented |
| `software_status` | selected_advanced_recipes_implemented_examples_5fb06175_normal_gate_passed_platform_7590b2d4_no_hil_or_new_formal_qualification |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus-examples/pull/19;https://github.com/X-Gen-Lab/nexus-examples/pull/20;https://github.com/X-Gen-Lab/nexus-examples/blob/5fb06175aab72292fb075e8a9ea67096203a86dc/docs/developer-recipes.md |
| `exit_condition` | Continuous DMA cache FIFO shared SPI Flash RAM pause/resume and maintained lowpower slices have reusable recipes |
| `owner` | codex |

(status-platform-delivery-completion-e05)=
## E05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E05 |
| `task` | External starters and examples |
| `implementation_status` | implemented |
| `software_status` | 17_presets_four_starters_and_standalone_consumers_passed_at_7590b2d4_examples_b029cc4b |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json |
| `exit_condition` | All 17 maintained presets four starters and standalone public consumers use the final pinned platform |
| `owner` | codex |

(status-platform-delivery-completion-e06)=
## E06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E06 |
| `task` | Bilingual current architecture docs |
| `implementation_status` | implemented |
| `software_status` | 132_page_strict_bilingual_package_and_consumers_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/docs;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | English Chinese diagrams ownership call directions failures and exact support agree and examples compile |
| `owner` | codex |

(status-platform-delivery-completion-f01)=
## F01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F01 |
| `task` | Validation contract and cost inventory |
| `implementation_status` | partial |
| `software_status` | validation_domain_inventory_and_final_stage_costs_at_7590b2d4_full_disposition_scope_retained |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Each required check has purpose domain input output failure and measured cost; overlaps have explicit disposition |
| `owner` | codex |

(status-platform-delivery-completion-f02)=
## F02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F02 |
| `task` | Formal analyzer parallel budget |
| `implementation_status` | implemented |
| `software_status` | bounded_tidy_cppcheck_all1135_contexts_each_passed_at_7590b2d4_speedup_not_measured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/workflow;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Formal tidy/cppcheck use bounded existing parallelism without dropping semantic compiler contexts |
| `owner` | codex |

(status-platform-delivery-completion-f03)=
## F03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F03 |
| `task` | Nested process resource budget |
| `implementation_status` | implemented |
| `software_status` | nested_budget_and_output_isolation_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Build provider SDK and analyzer concurrency obey total resource limits with isolated output paths |
| `owner` | codex |

(status-platform-delivery-completion-f04)=
## F04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F04 |
| `task` | Test and setup redundancy review |
| `implementation_status` | implemented |
| `software_status` | original_exit_mapping_review_passed_at_d742cdb5_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/workflow/f04-review/review.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;build/c-cold-20261028/workflow/f04-review/f04-original-exit-p5-review.json |
| `exit_condition` | Only immutable setup is shared; every retained merged or removed test has an independent contract/oracle mapping |
| `owner` | codex |

(status-platform-delivery-completion-f05)=
## F05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F05 |
| `task` | Incremental rebuild and cache invalidation |
| `implementation_status` | implemented |
| `software_status` | incremental_invalidation_contracts_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;/dev/shm/nexus-next-workflow/build/workflow/f05-summary.json;build/c-cold-20261028/workflow/incremental-arm-03/summary.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | No-op source public header TOML Board and tool-lock changes rebuild the correct scope without cached pass reports |
| `owner` | codex |

(status-platform-delivery-completion-f06)=
## F06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F06 |
| `task` | Timing failure and process cleanup |
| `implementation_status` | implemented |
| `software_status` | actual_final_receipts_and_failures_retained_at_7590b2d4_candidate_outer_exit_unknown |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Wall time stage sums retries diagnostics cancellation and actual child cleanup remain accurately observed |
| `owner` | codex |

(status-platform-delivery-completion-f07)=
## F07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F07 |
| `task` | Measured complete validation comparison |
| `implementation_status` | pending |
| `software_status` | complete_controlled_cold_warm_comparison_not_executed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Same environment comparison shows actual time/memory/coverage; final clean source executes complete applicable gates |
| `owner` | codex |

(status-platform-delivery-completion-g01)=
## G01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G01 |
| `task` | Maintained end-to-end orchestration |
| `implementation_status` | partial |
| `software_status` | final_scopes_executed_at_7590b2d4_maintained_orchestration_consolidation_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Existing qualify/candidate examples docs release commands form one maintained source-bound workflow |
| `owner` | codex |

(status-platform-delivery-completion-g02)=
## G02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G02 |
| `task` | Stable machine delivery results |
| `implementation_status` | implemented |
| `software_status` | atomic_result_and_manifest_contracts_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/workflow;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Explicit atomic report/manifest consumption rejects stale missing mismatched and failed results without parsing human stdout |
| `owner` | codex |

(status-platform-delivery-completion-g03)=
## G03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G03 |
| `task` | Accurate delivery failure recovery |
| `implementation_status` | partial |
| `software_status` | immutable_packages_and_independent_remote_recovery_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/workflow;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Package postprocess verify restore upload publish phases stay distinct; immutable successful package can be resumed safely |
| `owner` | codex |

(status-platform-delivery-completion-g04)=
## G04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G04 |
| `task` | Latest delivery navigation |
| `implementation_status` | implemented |
| `software_status` | source_bound_7590b2d4_evidence_navigation_updated_doc_execution_identity_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json |
| `exit_condition` | Current code selected qualified candidates docs examples and historical evidence have discoverable exact identities |
| `owner` | codex |

(status-platform-delivery-completion-g05)=
## G05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G05 |
| `task` | Portable retention and independent restore |
| `implementation_status` | implemented |
| `software_status` | public90_archives_and_private_examples_authenticated_recovery_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json |
| `exit_condition` | Necessary source SDK binaries config logs verifiers docs and licenses remain obtainable outside original checkout and tmp |
| `owner` | codex |

(status-platform-delivery-completion-g06)=
## G06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G06 |
| `task` | Bilingual publication verification |
| `implementation_status` | pending |
| `software_status` | bilingual_package_recovered_at_7590b2d4_actual_pages_publication_not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Language versions links packages actual deployed bytes and publication status are independently verified |
| `owner` | codex |

(status-platform-delivery-completion-h01)=
## H01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H01 |
| `task` | Complete physical experiment catalogue |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Each maintained board/revision profile backend mode workload instrument budget and raw requirement is explicit |
| `owner` | codex |

(status-platform-delivery-completion-h02)=
## H02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H02 |
| `task` | Reviewed physical fixture preparation |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | All selected exact reference targets have explicit fixture/tool readiness; software_reference never auto-promotes |
| `owner` | codex |

(status-platform-delivery-completion-h03)=
## H03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H03 |
| `task` | Real station admit and flash closure |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Actual UID probe serial clock power wiring tool identity flash readback reset and UART records validate |
| `owner` | codex |

(status-platform-delivery-completion-h04)=
## H04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H04 |
| `task` | Instrument and peer acquisition |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Real waveform serial register stack fault and cleanup/cancel observations bind to the selected station and image |
| `owner` | codex |

(status-platform-delivery-completion-h05)=
## H05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H05 |
| `task` | Power acquisition validation |
| `implementation_status` | implemented_tooling |
| `software_status` | affected_53_tests_passed_no_skip |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/workflow/power-all-toolchain-green.log |
| `exit_condition` | Retained calibrated bounded V/I samples yield repeatable power/energy with admission identity and failure regressions |
| `owner` | codex |

(status-platform-delivery-completion-h06)=
## H06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H06 |
| `task` | Physical peripheral and failure execution |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Every selected maintained peripheral executes positive work and actual failure stimuli within reviewed budgets |
| `owner` | codex |

(status-platform-delivery-completion-h07)=
## H07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H07 |
| `task` | Physical advanced DMA cache streams |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Memory completion wire idle CPU visibility ownership settlement and stop/recovery are observed per exact mode |
| `owner` | codex |

(status-platform-delivery-completion-h08)=
## H08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H08 |
| `task` | Physical OS latency and stack |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Each maintained backend observes IRQ-to-task service deadline complete task/MSP stack and endurance using its own contract |
| `owner` | codex |

(status-platform-delivery-completion-h09)=
## H09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H09 |
| `task` | Physical Flash lowpower security experiments |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Exact externally reserved Flash region domains clocks options paired images and recovery are independently verified |
| `owner` | codex |

(status-platform-delivery-completion-h10)=
## H10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H10 |
| `task` | Physical baseline comparison and regression |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;tools/testing |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Same-context baseline/candidate raw measurements meet budgets and are archived for recurring real-station execution |
| `owner` | codex |

(status-platform-delivery-completion-i01)=
## I01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I01 |
| `task` | Environment input provenance |
| `implementation_status` | partial |
| `software_status` | provenance_audited_13_of_15_python_inputs_absent_not_rebuilt |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies/build-recipe.lock.json;tools/evidence/recipe.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/workflow/environment-i01/provenance-audit.json |
| `exit_condition` | OCI base/frontend packages toolchain and Python artifacts map to exact retained inputs and acquisition identities |
| `owner` | codex |

(status-platform-delivery-completion-i02)=
## I02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I02 |
| `task` | Independent environment recipe reconstruction |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies/build-recipe.lock.json;tools/evidence/recipe.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028 |
| `exit_condition` | Two clean reconstructions compare declared objects and retain all real differences and logs |
| `owner` | codex |

(status-platform-delivery-completion-i03)=
## I03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I03 |
| `task` | Environment restore and delivery status |
| `implementation_status` | partial |
| `software_status` | observed_oci_bytes_recovered_at_7590b2d4_recipe_rebuild_and_fresh_daemon_not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies/build-recipe.lock.json;tools/evidence/recipe.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Recipe reproducibility and recovery of observed image remain distinct; status follows actual reconstruction evidence |
| `owner` | codex |

(status-platform-delivery-completion-j01)=
## J01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | J01 |
| `task` | Whole-scope residual audit |
| `implementation_status` | implemented |
| `software_status` | maintained_c_boundary_residual_audit_and_final_gate_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/generator-cleanup/handoff.json;build/c-cold-20261028/default-macro-cleanup/handoff.json;build/c-cold-20261028/spi-admission/handoff.json;build/c-cold-20261028/runtime-lease-c;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json |
| `exit_condition` | Every A-J path has migrated-and-verified originally-C-regressed or declaration-retained disposition; no residual omitted |
| `owner` | codex |

(status-platform-delivery-completion-j02)=
## J02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | J02 |
| `task` | Final software integration qualification |
| `implementation_status` | implemented |
| `software_status` | formal159_sdk_examples_b029cc4b_and_bilingual_docs_passed_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/legacy/ordinary-gate/package.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json |
| `exit_condition` | Frozen clean source completes ordinary gates maintained ARM modes installed SDK examples and bilingual documentation |
| `owner` | codex |

(status-platform-delivery-completion-j03)=
## J03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | J03 |
| `task` | Independent delivery closure verification |
| `implementation_status` | implemented |
| `software_status` | source_bound_candidate_sdk_docs_and_raw_closures_recovered_at_7590b2d4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json |
| `exit_condition` | Exact source config dependencies tools binaries raw reports docs and licenses form a verified restorable closure |
| `owner` | codex |

(status-platform-delivery-completion-j04)=
## J04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | J04 |
| `task` | Incremental commits and GitHub synchronization |
| `implementation_status` | implemented |
| `software_status` | six_reviewed_increments_synced_and_main_associated_at_7590b2d4_examples_b029cc4b |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;https://github.com/X-Gen-Lab/nexus/tree/refactor/c-cold-lifecycle-complete-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;build/c-cold-20261028/integration/final-main-sync/final-delivery-observation.json |
| `exit_condition` | Reviewed increments synchronize to GitHub; code software hardware and publication exits remain independently accurate |
| `owner` | codex |
