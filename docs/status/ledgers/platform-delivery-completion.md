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
源文件：`docs/design/platform-delivery-completion.csv`；SHA-256：`413273431902252980fe72e3554e8a2a4d1d46f3f7f578d598f50456c1ccf83b`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 210 条原记录，仅表示来源行数。

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
| `software_status` | current_ab532250_stage_costs_and_context_diagnostics_retained_actual_cold_warm_selected_scope_disposition_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/process.py;tools/dev/timings.py;tools/configure/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/devtools/actual-resource-observation.json;/tmp/nexus-next-20261024/configuration/git-after-measurements.jsonl;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/comparison-semantics.json |
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
| `software_status` | eight_exact_P2_candidates16_fresh_network_none_builds_bitexact_sealed_restored_verified_at_ab532250 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;tools/evidence/reproduce.py;tools/evidence/candidate.py;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json |
| `exit_condition` | 最终 clean source 执行完整 software plan；锁定环境两份独立断网构建实际比较 ELF/BIN；经既有流程 seal/verify。 |
| `owner` | codex |

(status-platform-delivery-completion-n14)=
## N14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | N14 |
| `task` | 文档与原始证据维护 |
| `implementation_status` | implemented_documentation |
| `software_status` | P2_SDK_docs_candidates_outer_recovery_and_current_public_installed_SDK_walk12commands1CTest_passed_D_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/;tools/documentation/;tools/evidence/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;/tmp/nexus-next-20261024/documentation/final-checks.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
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
| `software_status` | three_current_advanced_recipes_covered_by_full17_presets116commands_at_private_f59debf_pinned_P2_no_HIL |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus-examples/pull/19;https://github.com/X-Gen-Lab/nexus-examples/pull/20;https://github.com/X-Gen-Lab/nexus-examples/blob/5fb06175aab72292fb075e8a9ea67096203a86dc/docs/developer-recipes.md;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | Continuous DMA cache FIFO shared SPI Flash RAM pause/resume and maintained lowpower slices have reusable recipes |
| `owner` | codex |

(status-platform-delivery-completion-e05)=
## E05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E05 |
| `task` | External starters and examples |
| `implementation_status` | implemented |
| `software_status` | current17_presets116commands_native28CTest18Google204_installed2CTest_passed_private_f59debf_pinned_P2 |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | All 17 maintained presets four starters and standalone public consumers use the final pinned platform |
| `owner` | codex |

(status-platform-delivery-completion-e06)=
## E06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | E06 |
| `task` | Bilingual current architecture docs |
| `implementation_status` | implemented |
| `software_status` | P2_strict_bilingual132pages264records_and_current_E_examples_compiled_D_final_docs_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | io;docs;X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/docs;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | English Chinese diagrams ownership call directions failures and exact support agree and examples compile |
| `owner` | codex |

(status-platform-delivery-completion-f01)=
## F01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F01 |
| `task` | Validation contract and cost inventory |
| `implementation_status` | implemented_current159_contract_cost_inventory |
| `software_status` | current159_contract_overlap_and_actual_stage_attempt_costs_reconciled_at_ab532250 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/baseline-repeat-costs.json |
| `exit_condition` | Each required check has purpose domain input output failure and measured cost; overlaps have explicit disposition |
| `owner` | codex |

(status-platform-delivery-completion-f02)=
## F02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | F02 |
| `task` | Formal analyzer parallel budget |
| `implementation_status` | implemented |
| `software_status` | bounded_tidy_cppcheck_all1193_contexts_each1303_owned_TUs_parallel4_passed_at_ab532250 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028/workflow;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/baseline/verified-execution.json;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/repeat/verified-execution.json |
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
| `implementation_status` | implemented_selected_same_environment_comparison |
| `software_status` | same_P2_tool_env_complete159_pair_actual_time_max_process_RSS_coverage_observed_no_freshcold_no_causal_speedup |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev;scripts/ci;tests |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/baseline-repeat-costs.json;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/comparison-semantics.json |
| `exit_condition` | Same environment comparison shows actual time/memory/coverage; final clean source executes complete applicable gates |
| `owner` | codex |

(status-platform-delivery-completion-g01)=
## G01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | G01 |
| `task` | Maintained end-to-end orchestration |
| `implementation_status` | partial |
| `software_status` | P2_qualify_candidates_SDK_docs_local_and_remote_recovery_plus_current_E_and_public_walk_passed_D_deploy_FF_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;build/recovery-20261103/final-delivery-ab532250-20261104/unsigned-local-bundle-handoff.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
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
| `software_status` | P2_unsigned_outer_public_remote_join_store_restore_sameNative_verify_passed_fullNative_bytes_local42plus4 |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/documentation;tools/evidence;tools/release;docs |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json;build/recovery-20261103/publication-p2-transport-ab532250-20261104/remote-recovery-liveverify-handoff.json |
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
| `software_status` | P2_ordinary_twofull159_fourNative_SDK_strictdocs8candidates_and_current_private_E_passed_D_final_docs_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;build/c-cold-20261028/legacy/ordinary-gate/package.json;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | Frozen clean source completes ordinary gates maintained ARM modes installed SDK examples and bilingual documentation |
| `owner` | codex |

(status-platform-delivery-completion-j03)=
## J03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | J03 |
| `task` | Independent delivery closure verification |
| `implementation_status` | implemented |
| `software_status` | P2_candidate_SDK_docs_unsigned_outer_closures_restored_remote_sameproducer_verified_native_fullbytes_local |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery;tools/release |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;build/c-cold-20261028;docs/status/software-verification.md;https://github.com/X-Gen-Lab/nexus/blob/ecb86832fced0192d620b9029ff2d54795fc6430/docs-platform-delivery-20261029-g7590b2d4/final-index-g7590b2d4.json;https://github.com/X-Gen-Lab/nexus/blob/d14d093879362d6a1796ee9e0d47f75f793e5135/docs-asset-retention-20261030/WITHDRAWAL.md;https://github.com/X-Gen-Lab/nexus-examples/blob/8aa825c7d2cd8f58f4fa84869f16700aea400783/docs-legacy-examples-retention-20261030/retention-index.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json |
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

(status-platform-delivery-completion-v-a01)=
## V-A01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A01 |
| `task` | 维护配置闭包 |
| `implementation_status` | partial_public_217_mode_295_configuration_review |
| `software_status` | public_inventory_reviewed_private_annex_and_final_acceptance_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | build/vendor-first-20261030/docs/derived-mode-work-packages.json;build/vendor-first-20261030/docs/platform-navigation.json;build/vendor-first-20261030/next/vendor-mode-review-20261030/README.md |
| `exit_condition` | 汇总参考装配、provider 夹具、SDK consumer、高级组件及外部示例的完整适用配置。 |
| `owner` | codex |

(status-platform-delivery-completion-v-a02)=
## V-A02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A02 |
| `task` | 逐模式行为合同 |
| `implementation_status` | partial_public_198_mcu_source_contract_review_19_native_model |
| `software_status` | source_review_and_p3_29_positive_4_reserved_software_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | build/vendor-first-20261030/next/vendor-mode-review-20261030/reviews/README.md;build/vendor-first-20261030/next/vendor-mode-review-20261030/reviews/remaining-source-gaps.md |
| `exit_condition` | 逐项核对 TX/RX、格式、错误、wake、容量、长度、计数、cancel、drain 与 stop。 |
| `owner` | codex |

(status-platform-delivery-completion-v-a03)=
## V-A03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A03 |
| `task` | 官方实现决策 |
| `implementation_status` | partial_selected_official_decisions_and_six_bounded_increments |
| `software_status` | source_implemented_complete_migration_and_final_qualification_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | build/vendor-first-20261030/next/vendor-mode-review-20261030/reviews/coverage-summary.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-leaf-implementation/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/h7-implementation/h7-implementation-handoff.json |
| `exit_condition` | 每个模式明确选择 HAL transport、LL/SPL 原语或有理由的专用引擎。 |
| `owner` | codex |

(status-platform-delivery-completion-v-a04)=
## V-A04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A04 |
| `task` | 必要直接访问 |
| `implementation_status` | partial_operation_specific_semantic_and_narrow_arm_cost_exceptions |
| `software_status` | isolated_leaf_decisions_not_whole_provider_or_dynamic_cost |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | build/vendor-first-20261030/next/vendor-mode-review-20261030/reviews/combined-mode-decisions.csv;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-cost/decision.json |
| `exit_condition` | 原子操作、SDK 缺口、errata、撤离证明及实测预算例外有准确理由。 |
| `owner` | codex |

(status-platform-delivery-completion-v-a05)=
## V-A05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A05 |
| `task` | 行为与成本基线 |
| `implementation_status` | partial_selected_old_green_baselines_and_isolated_leaf_cost |
| `software_status` | actual_source_binary_baselines_and_arm_leaf_probes_final_comparison_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | build/vendor-first-20261030/c-leaf-iteration-20261030/p4-baseline/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-old-green/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-cost/comparison.json |
| `exit_condition` | 固定同合同的行为与资源比较范围，保留既有原始证据身份。 |
| `owner` | codex |

(status-platform-delivery-completion-v-a06)=
## V-A06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-A06 |
| `task` | 分离完成状态 |
| `implementation_status` | implemented_separate_scope_reporting |
| `software_status` | P2_record_separates_implementation_ordinary_formal_candidates_unsigned_artifacts_docs_physical_and_pending_services |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/official-stm32-provider.md;tools/maintenance/work_packages.py;tools/maintenance/capabilities.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;docs/delivery/platform-delivery-completion-results.json |
| `exit_condition` | 实现、软件、实板与交付分别记录；试点通过不关闭整个家族任务。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b01)=
## V-B01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B01 |
| `task` | 官方依赖配对 |
| `implementation_status` | partial_selected_stm32_pairing_and_expanded_f1_g4_h7_u5_closures |
| `software_status` | selected_lock_identities_reviewed_final_sdk_consumption_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/;cmake/platform/STM32Official.cmake;dependencies/stm32f4-hal.lock.json;tools/maintenance/stm32f4_hal_lock.py;dependencies/stm32f1-hal.lock.json;dependencies/stm32g4-hal.lock.json;dependencies/stm32h7-hal.lock.json;dependencies/stm32u5-hal.lock.json |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-lock-closure/handoff.json;build/vendor-first-20261030/next/vendor-mode-review-20261030/dependency-identities.md;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-metadata/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/closure-red/handoff.json |
| `exit_condition` | 核对组织 fork、精确 gitlink、SDK、CMSIS Device/Core 和许可证配对。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b02)=
## V-B02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B02 |
| `task` | 私有 HAL 模块构建 |
| `implementation_status` | partial_selected_f407_transport_and_u575_gpio_private_hal_sources |
| `software_status` | selected_private_build_integration_final_clean_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/;cmake/platform/STM32Official.cmake;dependencies/stm32f4-hal.lock.json;tools/maintenance/stm32f4_hal_lock.py;soc/stm32/stm32u575/private/hal/;soc/stm32/stm32u575/drivers/gpio.c |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-lock-closure/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/default-probes-old-green/u5-i2c-gpio-barrier.json |
| `exit_condition` | 仅显式编译实际需要的官方 C 模块，避免整包选入。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b03)=
## V-B03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B03 |
| `task` | HAL 配置与符号 |
| `implementation_status` | partial_selected_f407_private_callback_abi1 |
| `software_status` | selected_private_configuration_and_real_hal_model_passed_full_symbols_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/;cmake/platform/STM32Official.cmake;dependencies/stm32f4-hal.lock.json;tools/maintenance/stm32f4_hal_lock.py |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-lock-closure/handoff.json |
| `exit_condition` | 私有配置头、模块宏、callback 配置及全局符号冲突均经检查。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b04)=
## V-B04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B04 |
| `task` | GD32 SDK C 单元 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 使用新增 SPL 原语时同步选入对应 C 单元，特别是 DMA。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b05)=
## V-B05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B05 |
| `task` | 公共编译边界 |
| `implementation_status` | partial_selected_f407_private_compile_boundary |
| `software_status` | selected_native_arm_boundary_passed_full_consumers_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/;cmake/platform/STM32Official.cmake;dependencies/stm32f4-hal.lock.json;tools/maintenance/stm32f4_hal_lock.py |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-lock-closure/handoff.json |
| `exit_condition` | 公共 consumer 不继承 vendor/private includes；生产目标不引入测试运行时。 |
| `owner` | codex |

(status-platform-delivery-completion-v-b06)=
## V-B06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-B06 |
| `task` | 依赖及 SDK 闭包 |
| `implementation_status` | partial_selected_hal_ll_headers_five_i2c_ports_and_gpio_conf_payload |
| `software_status` | selected_closure_red_green_final_source_sdk_consumption_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/;dependencies/;vendors/;tools/maintenance/source_catalogue.py;cmake/package/;cmake/platform/STM32Official.cmake;dependencies/stm32f4-hal.lock.json;tools/maintenance/stm32f4_hal_lock.py;soc/stm32/stm32f103/private/stm32f103_i2c_port.h;soc/stm32/stm32f407/private/stm32f407_i2c_port.h;soc/stm32/stm32g474/private/stm32g474_i2c_port.h;soc/stm32/stm32h743/private/stm32h743_i2c_port.h;soc/stm32/stm32u575/private/stm32u575_i2c_port.h;soc/stm32/stm32u575/private/hal/ |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-lock-closure/handoff.json;build/vendor-first-20261030/next/h743-source-closure/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/sdk-conf-red/handoff.json |
| `exit_condition` | 锁、source catalogue、descriptor、SDK 文件和许可证闭包一致，升级经过受控验证。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c01)=
## V-C01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C01 |
| `task` | 最小静态 context |
| `implementation_status` | partial_selected_f407_static_handle_owner |
| `software_status` | selected_generated_factory_and_real_hal_model_passed_resource_ab_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 仅保留所选 Handle、配置、请求和模式扩展，不增加通用最大状态。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c02)=
## V-C02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C02 |
| `task` | 副作用前校验 |
| `implementation_status` | partial_selected_f407_admission_and_alias_guards |
| `software_status` | actual_reference_and_complete_mode_red_green_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-tests/handoff.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 在硬件影响前检查上下文、长度、对齐、内存域、deadline、模式和资源。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c03)=
## V-C03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C03 |
| `task` | 准入先于保留引用 |
| `implementation_status` | partial_selected_f407_borrow_before_hal |
| `software_status` | actual_synchronous_callback_and_start_model_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | admission 先于可能保留引用或同步回调的官方 API。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c04)=
## V-C04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C04 |
| `task` | 启动失败撤离 |
| `implementation_status` | partial_selected_f407_accepted_failure_withdrawal |
| `software_status` | actual_real_hal_failure_and_drain_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 已接受操作的启动失败按实际引用撤离，不能伪装为无借用拒绝。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c05)=
## V-C05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C05 |
| `task` | 取消与期限映射 |
| `implementation_status` | partial_selected_f407_cancel_deadline_withdrawal |
| `software_status` | actual_cancel_timeout_quarantine_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | cancel/timeout 明确推进撤离、drain 和 quarantine，借用不提前释放。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c06)=
## V-C06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C06 |
| `task` | 冷态资源责任 |
| `implementation_status` | partial_selected_f407_cold_lease_and_deinit |
| `software_status` | actual_partial_cleanup_and_two_owner_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | prepare/start、部分失败和 stop 重试保留准确责任，不释放其他 owner 资源。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c07)=
## V-C07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C07 |
| `task` | Handle 安全复用 |
| `implementation_status` | partial_selected_f407_handle_irq_reference_join |
| `software_status` | actual_late_callback_and_handle_reuse_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 旧硬件、IRQ 和 callback 栈撤离后才能复用 Handle，迟到事件不冒充当前请求。 |
| `owner` | codex |

(status-platform-delivery-completion-v-c08)=
## V-C08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-C08 |
| `task` | 主错误与清理结果 |
| `implementation_status` | partial_selected_f407_first_cause_and_cleanup |
| `software_status` | actual_first_cause_and_cold_cleanup_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;io/;core/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 主错误、清理错误、剩余责任分别保留；诊断不撤销借用。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d01)=
## V-D01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D01 |
| `task` | 真实 IRQ 抢占合同 |
| `implementation_status` | partial_selected_f407_vector_deferral_protocol |
| `software_status` | actual_selected_irq_preemption_models_passed_physical_latency_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 生产 IRQ 协议明确处理抢占，不能原样使用串行夹具的跳过 handler 假设。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d02)=
## V-D02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D02 |
| `task` | 短临界区与 pending facts |
| `implementation_status` | partial_selected_f407_short_guards_and_held_en_te_deferral |
| `software_status` | actual_atomic_and_quiet_error_irq_models_passed_wcet_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | HAL 嵌套、短临界区和 pending facts 明确，不以长全局关中断覆盖 vendor 等待。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d03)=
## V-D03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D03 |
| `task` | callback 实际上下文 |
| `implementation_status` | partial_selected_f407_per_handle_callback_facts |
| `software_status` | actual_task_sync_irq_callback_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 同步、task 和 ISR callback 使用合法通知，仅记录有界事实。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d04)=
## V-D04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D04 |
| `task` | HAL 可达等待与时钟 |
| `implementation_status` | partial_selected_f407_existing_epoch_bridge |
| `software_status` | actual_clock_metadata_tick_wrap_and_real_hal_models_passed_full_power_review_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/system.c;tests/google/stm32_system_transaction_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/hal-integration/clock-final-green/run.command.json |
| `exit_condition` | 审核 bootstrap/runtime、毫秒量化、32 位回绕及低功耗暂停的实际时间推进。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d05)=
## V-D05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D05 |
| `task` | OS 与时间基准隔离 |
| `implementation_status` | partial_selected_f407_no_hal_systick |
| `software_status` | existing_epoch_and_selected_wake_models_passed_full_backend_contexts_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 保留既有 SysTick/OS 策略，验证 IRQ ceiling、wait/wake 和 observer join。 |
| `owner` | codex |

(status-platform-delivery-completion-v-d06)=
## V-D06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-D06 |
| `task` | Arch 屏障与状态恢复 |
| `implementation_status` | partial_selected_f407_saved_mask_and_barriers |
| `software_status` | actual_saved_mask_and_selected_arm_compile_passed_physical_memory_order_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;arch/;os/;tests/contracts/stm32_hal_reference/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 恢复调用前中断状态，并区分 CPU 屏障、Cache 维护和请求发布。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e01)=
## V-E01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E01 |
| `task` | 完整 UART 生产候选 |
| `implementation_status` | implemented_selected_f407_dma_tx_hal_and_disjoint_irq_rx |
| `software_status` | complete_mode_real_hal_model_native_factory_and_selected_arm_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-tests/handoff.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 有限 TX HAL 评估扩为明确完整模式候选，不以 TX-only 代替既有 dma-tx。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e02)=
## V-E02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E02 |
| `task` | UART 与 DMA 冷态接入 |
| `implementation_status` | implemented_selected_f407_hal_cold_lease |
| `software_status` | actual_initialization_readback_partial_failure_and_close_retry_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 官方初始化纳入既有 resource lease、MSP、失败读回与恢复。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e03)=
## V-E03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E03 |
| `task` | DMA 与 UART TC |
| `implementation_status` | implemented_selected_f407_hal_dma_to_wire_tc |
| `software_status` | actual_real_hal_dma_tc_uart_tc_and_count_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 复用 HAL 正常 DMA 到 TC 流程，验证完成、错误及实际计数。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e04)=
## V-E04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E04 |
| `task` | RX 完整语义 |
| `implementation_status` | implemented_selected_f407_disjoint_irq_rx |
| `software_status` | actual_byte_event_loss_timestamp_wake_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 保留 RX bytes/events、frame/error/loss 顺序、时间戳和存储合同。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e05)=
## V-E05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E05 |
| `task` | TX/RX 共同所有权 |
| `implementation_status` | implemented_selected_f407_one_tx_rx_provider_owner |
| `software_status` | actual_abort_dma_error_shared_irq_and_foreign_domain_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 审查 TX abort、DMA error、共享 IRQ 对 RX 的影响，不拼接未经审查的独立 owner。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e06)=
## V-E06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E06 |
| `task` | UART 停止与再次提交 |
| `implementation_status` | implemented_selected_f407_stop_and_resubmit |
| `software_status` | actual_sync_late_irq_deinit_retry_and_resubmit_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 同步 callback、迟到 IRQ、wake detach、stop 和恢复后再次提交均有实际回归。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e07)=
## V-E07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E07 |
| `task` | 完整 UART 成本决策 |
| `implementation_status` | partial_selected_f407_same_config_arm_flash_ram_comparison |
| `software_status` | paired_arm_images_and_byte_identical_resolved_passed_stack_cycles_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;tests/contracts/uart_rx_compact_assembly/f407-dma-tx.toml;tools/measurement/resources.py;tools/measurement/workload.c |
| `evidence_paths` | build/vendor-first-20261030/cost/normalized-comparison.json;build/vendor-first-20261030/cost/final-default-artifacts/manifest.json;build/vendor-first-20261030/cost/final-symbol-check.json |
| `exit_condition` | 同合同比较完整 provider 的 Flash/RAM/栈/执行成本后确定生产实现。 |
| `owner` | codex |

(status-platform-delivery-completion-v-e08)=
## V-E08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-E08 |
| `task` | UART 替换与清理 |
| `implementation_status` | partial_selected_f407_dma_tx_source_replacement |
| `software_status` | selected_generated_source_and_arm_passed_sdk_consumers_and_full_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/stm32/stm32f407/;tests/contracts/stm32_hal_reference/;tools/configure/;soc/stm32/stm32f407/drivers/uart_hal.c;soc/stm32/stm32f407/private/stm32f407_uart_hal.h |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 接受后更新装配及 SDK，删除已完整替换的生产路径，未满足合同的候选保留结论。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f01)=
## V-F01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F01 |
| `task` | GPIO 与 pin 迁移 |
| `implementation_status` | partial_selected_f407_gd470_primitives_and_u575_four_hal_gpio_commands |
| `software_status` | actual_default_old_green_and_selected_development_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;soc/stm32/stm32u575/drivers/gpio.c;dependencies/stm32u5-hal.lock.json |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/c-leaf-iteration-20261030/default-probes-old-green/u5-i2c-gpio-barrier.json |
| `exit_condition` | 扩大 LL/SPL，保持安全电平顺序、权限与组合 set/reset 原子性。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f02)=
## V-F02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F02 |
| `task` | SPI 短事务迁移 |
| `implementation_status` | partial_selected_f407_gd470_u575_and_h743_spi_byte_primitives |
| `software_status` | actual_default_old_green_and_selected_development_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;soc/stm32/stm32h743/drivers/spi.c |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/next/u575-spi-ll-20261030/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/h7-implementation/h7-implementation-handoff.json |
| `exit_condition` | 官方配置及收发保持完整 CS 区间、dummy/discard 和 transferred。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f03)=
## V-F03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F03 |
| `task` | SPI DMA 迁移 |
| `implementation_status` | partial_h743_spi_dma_cleanup_ll_byte_leaf_transport_engine_retained |
| `software_status` | actual_default_old_green_final_mode_and_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;soc/stm32/stm32h743/drivers/spi_dma.c |
| `evidence_paths` | build/vendor-first-20261030/c-leaf-iteration-20261030/h7-implementation/h7-implementation-handoff.json |
| `exit_condition` | 评价完整官方流程，验证 TX/RX 联合撤离、固定地址和 wire completion。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f04)=
## V-F04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F04 |
| `task` | I2C 消息序列迁移 |
| `implementation_status` | partial_five_stm32_compile_time_i2c_ll_data_ports_direct_exceptions_retained |
| `software_status` | actual_default_old_green_and_narrow_arm_cost_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;drivers/i2c/i2c_v1_async/i2c_v1_async.c;drivers/i2c/i2c_v2/i2c_v2.c |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-cost/decision.json |
| `exit_condition` | 覆盖 repeated START、1/2/3 字节窗口、NACK/ARLO、STOP 和精确 errata。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f05)=
## V-F05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F05 |
| `task` | ADC 与 group 迁移 |
| `implementation_status` | partial_f407_gd470_selected_primitives |
| `software_status` | ordinary_complete_index_passed_at_9c0460b9_scoped_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/ |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json |
| `exit_condition` | 保留校准、触发、数据格式、组序列及 DMA 数据借用。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f06)=
## V-F06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F06 |
| `task` | PWM/capture/encoder 迁移 |
| `implementation_status` | partial_f407_gd470_primitives_and_u575_checked_ll_pwm_setters |
| `software_status` | actual_default_old_green_existing_362_debug_cases_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;soc/stm32/stm32u575/drivers/pwm.c |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-leaf-implementation/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-old-green/handoff.json |
| `exit_condition` | 保留共享基座、同步更新、互补输出和安全停止。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f07)=
## V-F07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F07 |
| `task` | Flash 官方流程评价 |
| `implementation_status` | partial_bounded_P2_Flash_static_and_ECC_review |
| `software_status` | P2_seven_context_bounded_static_review_and_independent_ECC_clarification_event_owner_sticky_recovery_not_assessed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;/workspace/nexus/build/recovery-20261103/reconciliation-template/P2-V-F07-flash-static-review.json;/workspace/nexus/build/recovery-20261103/reconciliation-template/P2-V-F07-flash-static-independent-review.json;/workspace/nexus/build/recovery-20261103/reconciliation-template/P2-V-F07-ECC-clarification-addendum.md;/workspace/nexus/build/recovery-20261103/reconciliation-template/ECC-independent-review.json |
| `exit_condition` | 核对几何、ECC、执行域、供电、pulse 结清及错误恢复。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f08)=
## V-F08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F08 |
| `task` | CAN/CAN FD 迁移 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 保留 mailbox/filter/message RAM、错误处理及停止责任。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f09)=
## V-F09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F09 |
| `task` | USB/Ethernet 迁移 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 逐精确模式评价官方驱动，保持端点、描述符、缓冲区和 callback 所有权。 |
| `owner` | codex |

(status-platform-delivery-completion-v-f10)=
## V-F10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-F10 |
| `task` | RTC/LPTIM/watchdog 迁移 |
| `implementation_status` | partial_f407_watchdog_and_u575_rtc_ll_keys_alarm_command |
| `software_status` | actual_default_old_green_existing_362_debug_cases_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;io/;components/;cmake/package/verification/;soc/stm32/stm32u575/drivers/rtc.c |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-leaf-implementation/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-old-green/handoff.json |
| `exit_condition` | 保持时间连续性、唤醒合同和不可逆硬件影响。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g01)=
## V-G01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G01 |
| `task` | F103 精确验证 |
| `implementation_status` | partial_selected_f103_i2c_ll_receive_and_source_review |
| `software_status` | selected_source_and_default_old_green_full_family_exit_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/;soc/stm32/stm32f103/private/stm32f103_i2c_port.h |
| `evidence_paths` | build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json |
| `exit_condition` | 独立验证 density、低时钟、channel DMA 和 errata 限制。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g02)=
## V-G02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G02 |
| `task` | GD303 精确验证 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 独立核对 SPL、寄存器语义、启动延迟及 errata，不继承 F103 结果。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g03)=
## V-G03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G03 |
| `task` | F407 维护模式闭包 |
| `implementation_status` | partial_selected_f407_primitives |
| `software_status` | ordinary_complete_index_passed_at_9c0460b9_scoped_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/ |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json |
| `exit_condition` | 覆盖现有 UART/SPI/I2C/ADC/控制器模式和 stream DMA 资源。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g04)=
## V-G04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G04 |
| `task` | GD470 维护模式闭包 |
| `implementation_status` | partial_selected_gd470_primitives |
| `software_status` | ordinary_complete_index_passed_at_9c0460b9_scoped_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/ |
| `evidence_paths` | build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json |
| `exit_condition` | 扩大 SPL 并独立验证路由、时钟、DMA 撤离和模式差异。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g05)=
## V-G05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G05 |
| `task` | G474 耦合资源验证 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 覆盖 DMAMUX、shared/coherent PWM、PWM 到 ADC、group/injected 和 FD CAN。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g06)=
## V-G06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G06 |
| `task` | H743 域与 Cache 验证 |
| `implementation_status` | partial_h743_metadata_spi_ll_byte_and_i2c_data_ports |
| `software_status` | selected_development_scopes_not_complete_h743_or_final_candidate_exit |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/;soc/stm32/stm32h743/private/stm32h743_i2c_port.h |
| `evidence_paths` | build/vendor-first-20261030/next/h743-source-closure/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/h7-implementation/h7-implementation-handoff.json |
| `exit_condition` | 覆盖 Cache、域、DMA 内存、连续模式、SPI wire errata 与 RAM 执行组件。 |
| `owner` | codex |

(status-platform-delivery-completion-v-g07)=
## V-G07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-G07 |
| `task` | U575 精确子集验证 |
| `implementation_status` | partial_u575_spi_pwm_uart_rtc_gpio_i2c_official_operations |
| `software_status` | selected_development_scopes_final_clean_candidate_and_full_family_exit_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/;drivers/;tests/contracts/;cmake/package/verification/;soc/stm32/stm32u575/drivers/pwm.c;soc/stm32/stm32u575/drivers/uart.c;soc/stm32/stm32u575/drivers/rtc.c;soc/stm32/stm32u575/drivers/gpio.c;soc/stm32/stm32u575/private/stm32u575_i2c_port.h |
| `evidence_paths` | build/vendor-first-20261030/next/u575-spi-ll-20261030/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/u5-leaf-implementation/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json |
| `exit_condition` | 分别验证 GPDMA/chain、FIFO、保留时间、Stop2/LPDMA/LPBAM 和既有 TrustZone 窄范围。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h01)=
## V-H01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H01 |
| `task` | resolver 类别局部化 |
| `implementation_status` | partial_controller_and_endpoint_localization |
| `software_status` | ordinary_complete_index_passed_at_9c0460b9_scoped_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;drivers/;soc/;cmake/platform/;tools/dev/ |
| `evidence_paths` | build/vendor-first-20261030/configure/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json |
| `exit_condition` | 类别归一化就近维护，公共入口保留共同仲裁和原子发布。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h02)=
## V-H02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H02 |
| `task` | family descriptor 就近维护 |
| `implementation_status` | implemented_soc_local_data_only_descriptors |
| `software_status` | focused_descriptor_provider_sdk_closure_and_71bundle_equivalence_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;drivers/;soc/;cmake/platform/;tools/dev/;tools/configure/providers/descriptor.py;tools/configure/providers/__init__.py;soc/native/descriptor.py;soc/stm32/;soc/gd32/ |
| `evidence_paths` | build/vendor-first-20261030/descriptor-localization/root-handoff.json |
| `exit_condition` | 保留顶层显式映射及稳定闭包检查，避免新建平行源码图。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h03)=
## V-H03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H03 |
| `task` | 语义模式选择 |
| `implementation_status` | reviewed_existing_semantic_mode_selection_retained |
| `software_status` | source_contract_reviewed_no_new_runtime_behavior_claim |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;drivers/;soc/;cmake/platform/;tools/dev/;tools/configure/selection.py;tools/configure/contracts.py |
| `evidence_paths` | docs/design/integration-contracts.md;docs/guides/provider-porting.md |
| `exit_condition` | 用户选择语义 mode，内部明确选实现，无并行权威和隐式回退。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h04)=
## V-H04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H04 |
| `task` | 共享引擎归属 |
| `implementation_status` | reviewed_existing_shared_engine_ownership_retained |
| `software_status` | source_ownership_contract_reviewed_no_reimplementation_claim |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;drivers/;soc/;cmake/platform/;tools/dev/;io/private/ |
| `evidence_paths` | docs/explanation/platform-architecture.md;docs/guides/provider-porting.md |
| `exit_condition` | 按公共机制、vendor adapter、必要专用引擎和 SoC 集成整理实际职责。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h05)=
## V-H05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H05 |
| `task` | 编译期模板合同 |
| `implementation_status` | implemented_reviewed_specialization_and_value_header_contracts |
| `software_status` | pin_red4_green27_tools_50_google_and_arm_equivalence_full_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/;drivers/;soc/;cmake/platform/;tools/dev/;io/private/timer_capture_impl.h;io/private/advanced_pwm_impl.h;io/private/uart_fifo_impl.h;tools/testing/test_provider_templates.py |
| `evidence_paths` | build/vendor-first-20261030/template-contracts/actual-root/handoff.json;build/vendor-first-20261030/next/h05-value-headers/handoff.json |
| `exit_condition` | 输入、导出符号、宏作用域及各 specialization 的验证明确。 |
| `owner` | codex |

(status-platform-delivery-completion-v-h06)=
## V-H06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-H06 |
| `task` | 偏移镜像与交接 profile |
| `implementation_status` | implemented_selected_f407_gd470_offset_images |
| `software_status` | configuration_linker_artifact_and_six_arm_links_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/configure.py;tools/configure/memory.py;soc/stm32/stm32f407/system.c;soc/gd32/gd32f470/system.c;tools/measurement/elf.py;tools/measurement/resources.py;tools/evidence/candidate.py;tools/evidence/reproduce.py |
| `evidence_paths` | build/vendor-first-20261030/image-layout/handoff.json;build/vendor-first-20261030/image-layout/artifact-handoff.json;build/vendor-first-20261030/image-layout/arm-image-observation.json;build/vendor-first-20261030/image-layout/arm-freertos-image-observation.json |
| `exit_condition` | 按既定明确范围验证向量、链接区域和入口条件，产品升级策略外置。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i01)=
## V-I01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I01 |
| `task` | 多实例资源成本 |
| `implementation_status` | partial_shared_spi_zero_one_two_four_endpoint_matrix |
| `software_status` | four_actual_arm_images_with_interface_storage_and_retention_scope |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | build/vendor-first-20261030/next/spi-endpoint-cost-20261030/handoff.json |
| `exit_condition` | 比较空/1/2/4 实例，区分 face/config/runtime/buffer/executor。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i02)=
## V-I02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I02 |
| `task` | 实际链接保留 |
| `implementation_status` | partial_spi_endpoint_controller_face_and_shared_ops_retention |
| `software_status` | actual_elf_symbols_and_zero_selection_checked_other_classes_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | build/vendor-first-20261030/next/spi-endpoint-cost-20261030/scope-limits.json |
| `exit_condition` | 核对 ops 引用和 ELF symbols，解释未选模式的代码及常驻 RAM。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i03)=
## V-I03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I03 |
| `task` | config/runtime 成本 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 逐项测量拆分收益，避免为外观统一增加指针或状态。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i04)=
## V-I04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I04 |
| `task` | 执行与时序成本 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 按同配置记录每块 CPU、IRQ/service、最大关中断时间及端到端期限。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i05)=
## V-I05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I05 |
| `task` | 连续性与背压 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 结合预算验证重挂载、消费者持有、突发、背压和总线竞争。 |
| `owner` | codex |

(status-platform-delivery-completion-v-i06)=
## V-I06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-I06 |
| `task` | 同合同完整比较 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/;tools/hil/;tests/contracts/io_storage_cost/;docs/design/soc-execution-budgets.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 区分链接预算、栈预留、实测峰值和物理测量，未知值保持未知。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j01)=
## V-J01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J01 |
| `task` | 真实 TDD 顺序 |
| `implementation_status` | partial_selected_reference_and_complete_mode_actual_red_green |
| `software_status` | actual_source_binary_red_green_snapshots_retained_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/hal-tests/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 新行为和修复先实际 RED，再 GREEN；并行开发遵守交接屏障。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j02)=
## V-J02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J02 |
| `task` | adapter 合同测试 |
| `implementation_status` | partial_selected_real_hal_adapter_models |
| `software_status` | selected_real_adapter_regressions_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/hal-tests/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | GoogleTest/GoogleMock 检查真实 adapter，mock 仅位于明确外部边界。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j03)=
## V-J03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J03 |
| `task` | 实际官方源码测试 |
| `implementation_status` | partial_actual_official_hal_transport_gpio_and_ll_default_models |
| `software_status` | actual_selected_old_green_probes_and_metadata_red_green_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp;tests/google/official_leaf_probes.cmake |
| `evidence_paths` | build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/hal-tests/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json;build/vendor-first-20261030/c-leaf-iteration-20261030/default-probes-old-green/u5-i2c-gpio-barrier.json;build/vendor-first-20261030/c-leaf-iteration-20261030/closure-red/handoff.json |
| `exit_condition` | 实际编译执行所选官方源码，验证与 adapter 的组合。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j04)=
## V-J04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J04 |
| `task` | 抢占与隔离回归 |
| `implementation_status` | partial_selected_f407_preemption_and_instance_isolation |
| `software_status` | actual_selected_failure_sync_preemption_late_and_isolation_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 覆盖启动后失败、同步 callback、IRQ 抢占、迟到完成和实例隔离。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j05)=
## V-J05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J05 |
| `task` | 故障撤离回归 |
| `implementation_status` | partial_selected_f407_failure_withdrawal_orderings |
| `software_status` | actual_cancel_tc_error_refused_drain_repeat_stop_and_resubmit_models_passed_final_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 覆盖 cancel/TC/DMA error 排列、撤离拒绝、重复 stop 和恢复再提交。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j06)=
## V-J06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J06 |
| `task` | 独立 ARM 上下文 |
| `implementation_status` | partial_selected_driver_offset_and_hal_arm_contexts |
| `software_status` | selected_real_hal_arm_and_six_offset_links_passed_full_matrix_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/;tests/contracts/stm32f407_uart_hal/;tests/google/stm32_uart_hal.cmake;tests/google/stm32_uart_cold_factory_test.cpp |
| `evidence_paths` | build/vendor-first-20261030/stm32/handoff.json;build/vendor-first-20261030/gd32/handoff.json;build/vendor-first-20261030/image-layout/arm-image-observation.json;build/vendor-first-20261030/image-layout/arm-freertos-image-observation.json;build/vendor-first-20261030/hal-maintained/debug-green-handoff.json;build/vendor-first-20261030/cost/final2-refresh.json |
| `exit_condition` | 每个独立编译上下文验证 SDK、ABI、布局、链接和资源。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j07)=
## V-J07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J07 |
| `task` | 测试保留审查 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 删除测试需有合同过时或独立等价覆盖依据，不靠减少数量制造通过。 |
| `owner` | codex |

(status-platform-delivery-completion-v-j08)=
## V-J08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-J08 |
| `task` | 高效完整门禁 |
| `implementation_status` | partial_fail_fast_and_stage_budget_propagation |
| `software_status` | actual_budget_red2_green158_with_explicit_caller_precedence |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/;scripts/ci/;tools/dev/ |
| `evidence_paths` | build/vendor-first-20261030/native-preflight/handoff.json;build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/next/qualification-stage-budget/handoff.json |
| `exit_condition` | 开发聚焦执行，完整增量走普通 hooks，冻结源执行完整计划并预算存储。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k01)=
## V-K01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K01 |
| `task` | 完整 Source SDK |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 包内含生产模块、配置头、桥接代码、许可证和精确依赖闭包。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k02)=
## V-K02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K02 |
| `task` | 包外 C/C++ consumer |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 脱离完整 checkout 验证重定位、公共头隔离及独立构建。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k03)=
## V-K03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K03 |
| `task` | 三种执行示例 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 外部示例完成裸机独占、RTOS owner 和持续流规范用法。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k04)=
## V-K04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K04 |
| `task` | 失败与恢复示例 |
| `implementation_status` | partial_selected_existing_private_reference_and_public_SDK_path |
| `software_status` | current_private_f59debf_pinned_P2_source_installed5Cbroker_cases_each_join_release_cleanup_expected_rejections_passed_physical_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | 示例覆盖拒绝、取消、失败 drain、join、stop 和恢复。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k05)=
## V-K05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K05 |
| `task` | 完整开发入口 |
| `implementation_status` | partial_selected_existing_private_reference_and_public_SDK_path |
| `software_status` | current_public_installed_SDK_walk12commands1CTest_passed_at_P2_physical_flash_debug_monitor_not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | 核对 create/configure/explain/diff/flash/debug/monitor 的真实用户路径。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k06)=
## V-K06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K06 |
| `task` | 架构与支持导航 |
| `implementation_status` | partial_current_provider_source_review_and_bounded_delta_navigation |
| `software_status` | source_review_and_bilingual_content_checked_final_acceptance_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | build/vendor-first-20261030/next/vendor-docs-integration-20261030/handoff.json |
| `exit_condition` | 架构图、模式合同、迁移说明和精确支持查询对应实际实现。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k07)=
## V-K07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K07 |
| `task` | 双语与代码风格 |
| `implementation_status` | implemented_P2_strict_bilingual_scope |
| `software_status` | P2_132canonicalpages264reviewedrecords_Doxygen_gettext_EN_ZH_package_restore_verify_all0_laterD_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | build/vendor-first-20261030/next/vendor-docs-integration-20261030/handoff.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json |
| `exit_condition` | 中英文、gettext、schema、链接及严格文档检查完整，沿用现有格式和 Doxygen。 |
| `owner` | codex |

(status-platform-delivery-completion-v-k08)=
## V-K08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-K08 |
| `task` | 公开与私有交付边界 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/;X-Gen-Lab/nexus-examples;docs/;tools/dev/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 新产物通过既有来源与可见性校验，私有示例不进入公开平台包。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l01)=
## V-L01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L01 |
| `task` | 替换后生产清理 |
| `implementation_status` | partial_selected_official_leaf_defaults_replaced_protocol_engines_retained |
| `software_status` | source_graph_reviewed_final_clean_gate_and_full_replacement_exit_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/;soc/stm32/stm32u575/drivers/;soc/stm32/stm32h743/drivers/;drivers/i2c/ |
| `evidence_paths` | build/vendor-first-20261030/c-leaf-iteration-20261030/u5-leaf-implementation/handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/h7-implementation/h7-implementation-handoff.json;build/vendor-first-20261030/c-leaf-iteration-20261030/i2c-final/handoff.json |
| `exit_condition` | 删除已替换实现、无用开关及重复状态，保留必要比较夹具与未替换模式。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l02)=
## V-L02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L02 |
| `task` | 干净平台提交 P |
| `implementation_status` | partial_clean_89485943_and_next_increment_integration |
| `software_status` | prior_clean_commit_retained_new_ordinary_gate_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | build/vendor-first-20261030/next/qualification-stage-budget/handoff.json |
| `exit_condition` | 代码和文档就绪后形成干净提交 P，冻结源码与依赖。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l03)=
## V-L03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L03 |
| `task` | P 的完整软件计划 |
| `implementation_status` | implemented_current_P2_complete_formal_plan |
| `software_status` | two_current_P2_full159_fourNative420programs7342cases440CTest_each_all_applicable_checks_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | ;/workspace/nexus-software-qualification-89485943-20261030/report.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/baseline/verified-execution.json;build/recovery-20261103/final-p2-baseline-repeat-20261103/control/repeat/verified-execution.json |
| `exit_condition` | 在 P 上实际完成完整计划、四种 Native 和全部适用检查。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l04)=
## V-L04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L04 |
| `task` | P 的 SDK 与消费 |
| `implementation_status` | implemented_selected_P2_SDK_and_consumers |
| `software_status` | P2_v15_64roots67images270commands_sdk_and_ENTIREconsumer_actualrestored_ownedverified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;build/recovery-20261103/final-delivery-ab532250-20261104/repeat-sdk-consumer-packages/outcome-reconciliation.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json |
| `exit_condition` | P 的 SDK、资源报告和独立外部消费具有同源同产物记录。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l05)=
## V-L05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L05 |
| `task` | 示例提交 E |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 示例正式锁定合格 P，形成 E 并通过完整示例本地 CI。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l06)=
## V-L06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L06 |
| `task` | 精确交付关联 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | P/E、配置、SDK、工具和原始产物关联，保留原失败及成功记录。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l07)=
## V-L07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L07 |
| `task` | 受控环境与离线复现 |
| `implementation_status` | implemented_eight_selected_P2_offline_pairs |
| `software_status` | P2_8profiles16fresh_network_none_builds_bitexact_ELF_BIN_controlled_environment_identity_retained |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | ;/workspace/nexus-offline-matrix-89485943-20261030/two-pair-handoff.json;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250;build/recovery-20261103/final-delivery-ab532250-20261104/profiles-and-strict-docs-handoff.json;https://github.com/X-Gen-Lab/nexus/blob/71b4f0e4cc9168e708513fb647b903c4bb39c71e/docs-public-p2-ab532250-20261104/transport.json |
| `exit_condition` | 分别证明受控环境与两次新鲜离线构建的实际范围，不继承旧资格。 |
| `owner` | codex |

(status-platform-delivery-completion-v-l08)=
## V-L08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-L08 |
| `task` | 提交合并与发布 |
| `implementation_status` | partial_first_two_commits_pushed_pr43_draft |
| `software_status` | next_increment_ordinary_gate_commit_push_merge_release_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/release/;tools/evidence/;tools/dev/;X-Gen-Lab/nexus-examples;docs/status/ |
| `evidence_paths` | build/vendor-first-20261030/commits/02-commit.command.json;build/vendor-first-20261030/commits/02-push.command.json |
| `exit_condition` | 正常提交同步并合入已验证提交，发布消费同一份已验证产物。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p01)=
## V-P01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P01 |
| `task` | 实板站点接入 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 确认三板 part/PCB/UID、探针、串口、电源和实际接线。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p02)=
## V-P02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P02 |
| `task` | 启动与 UART 实测 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 实际验证启动、时钟、GPIO、UART 收发和错误恢复。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p03)=
## V-P03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P03 |
| `task` | 总线与触发实测 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 仅对已审查接线执行 SPI/I2C/ADC/PWM 总线和触发测试。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p04)=
## V-P04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P04 |
| `task` | 负载与时序实测 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 实测 IRQ、临界区、DMA 连续性、吞吐、CPU 及栈峰值。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p05)=
## V-P05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P05 |
| `task` | 停机恢复实测 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 实测超时、取消、失败撤离、停机与恢复后再次运行。 |
| `owner` | codex |

(status-platform-delivery-completion-v-p06)=
## V-P06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-P06 |
| `task` | 高级范围实测 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/;boards/;docs/design/selected-mode-hil.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 在对应设备验证 Cache/域、低功耗、Flash/watchdog 和所选安全域。 |
| `owner` | codex |

(status-platform-delivery-completion-v-x01)=
## V-X01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-X01 |
| `task` | OCI 与新 daemon 验证 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/;tools/release/;docs/status/software-verification.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 容量足够后实际完成 recipe 重建及 fresh daemon 验证。 |
| `owner` | codex |

(status-platform-delivery-completion-v-x02)=
## V-X02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-X02 |
| `task` | 服务端分支规则 |
| `implementation_status` | pending_integration_administration_access |
| `software_status` | actual_ruleset_post_retried_403_20261030_not_configured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/;tools/release/;docs/status/software-verification.md |
| `evidence_paths` | ;build/vendor-first-20261030/external-recheck-20261030/main-ruleset-create.command.json |
| `exit_condition` | 具备权限后实际配置并读回 branch rules 和 required checks。 |
| `owner` | codex |

(status-platform-delivery-completion-v-x03)=
## V-X03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-X03 |
| `task` | CodeQL 访问与处置 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/;tools/release/;docs/status/software-verification.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 取得实际 CodeQL 分析并处置问题，本地分析不替代其身份。 |
| `owner` | codex |

(status-platform-delivery-completion-v-x04)=
## V-X04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | V-X04 |
| `task` | Pages 与正式发布 |
| `implementation_status` | planned |
| `software_status` | not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/;tools/release/;docs/status/software-verification.md |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | 实际执行 Pages/Release，记录真实执行与产物身份。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p01)=
## IT-P01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P01 |
| `task` | 外置 SoC/provider 包契约 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py;docs/guides/provider-porting.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 明确包身份、接口版本、facts/routes、配置入口、CMake 入口和依赖；清单只记录输入身份，不建立第二套源码目标图。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p02)=
## IT-P02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P02 |
| `task` | 不可变 package context |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py;tools/configure/ir.py;tools/configure/configure.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 一次显式解析包选择，resolver、能力查询、bindings、factory 和构建消费同一不可变结果；不同包不串用全局状态或缓存。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p03)=
## IT-P03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P03 |
| `task` | 外置包身份与路径准入 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 拒绝重复身份、越界输入、未允许的符号链接、缺文件和版本不符；失败不保留半套生成结果，实际拒绝回归通过。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p04)=
## IT-P04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P04 |
| `task` | 外置配置解析接入 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers/descriptor.py;tools/configure/selected_contracts.py;tools/configure/configure.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 新增外置 family 不需编辑中央白名单；内置与外置包复用维护解析流程，同进程多包结果保持隔离。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p05)=
## IT-P05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P05 |
| `task` | 外置 CMake 集成 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/platform/Firmware.cmake;tools/configure/dependencies.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 选定外置包私有连接 SDK、startup、system 和 linker；普通构建作用域保留单个 Nexus::Config，未选包不进入目标。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p06)=
## IT-P06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P06 |
| `task` | 安装版 SDK 消费外置包 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/package_source_sdk.py;cmake/package/verify_consumers.py;cmake/package/verification/common_services/external/assembly.toml |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 包外工程只用安装前缀和显式包路径完成配置、编译和链接；移动路径、断网、缺包和版本错误均验证，无源码 checkout 回退。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p07)=
## IT-P07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P07 |
| `task` | 外置包输入与证据闭包 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/dependencies.py;tools/maintenance/capabilities.py;tools/configure/integration_package.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | facts、配置代码和选定构建输入进入同一身份；包变化拒绝旧证据，能力查询不混用内置来源或扩大物理资格。 |
| `owner` | codex |

(status-platform-delivery-completion-it-p08)=
## IT-P08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-P08 |
| `task` | 独立接入验收夹具 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verification/common_services/external/assembly.toml;tools/configure/test_external_package.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 不修改 Nexus 即可交付一个包外集成，实际验证静态 factory、多实例、启动失败及回滚；夹具结果不代表新芯片实板通过。 |
| `owner` | codex |

(status-platform-delivery-completion-it-r01)=
## IT-R01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-R01 |
| `task` | 只读运行状态观察 |
| `implementation_status` | implemented |
| `software_status` | optional_observation_ordinary_accepted_eb4aba66_exact_scope_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | core/include/nexus/core/request_observer.h;io/include/nexus/io/observation.h;components/owner_observer/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 先以 UART/SPI 复用真实 owner/provider 事实提供可选观察；不创建第二状态机，快照不授予操作权限，关闭观察不增加热路径常驻成本。 |
| `owner` | codex |

(status-platform-delivery-completion-it-r02)=
## IT-R02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-R02 |
| `task` | 多源 wait-set 与显式 executor |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | os/wait.c;os/include/nexus/os/wait.h |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 实际第二消费者固定合同后，以调用者存储组合完成、timer 和关闭提示；验证 arm/recheck、deadline、注销、join 与公平性，不创建隐藏线程。 |
| `owner` | codex |

(status-platform-delivery-completion-it-r04)=
## IT-R04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-R04 |
| `task` | 精确 runtime PM 路径 |
| `implementation_status` | partial |
| `software_status` | existing_h743_fixed64_cache_off_spi4_integration_and_physical_exits_open |
| `hardware_status` | not_executed |
| `implementation_paths` | components/clock_domain/;soc/stm32/stm32h743/;tests/google/h743_runtime_clock_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 固定控制器、电源/clock、pin 与唤醒依赖并验证 suspend/resume 回滚；软件结果和真实功耗、保留范围、唤醒时序分别验收。 |
| `owner` | codex |

(status-platform-delivery-completion-it-r05)=
## IT-R05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-R05 |
| `task` | 第二传感器与采样合同 |
| `implementation_status` | implemented |
| `software_status` | tmp117_bounded_contract_ordinary_accepted_eb4aba66_physical_scope_separate |
| `hardware_status` | not_executed |
| `implementation_paths` | components/tmp117/;tests/google/tmp117_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 用第二个实际器件检验单位、有效性、时间来源、校准、overflow 与 sample lifetime；复用 bus owner，覆盖坏值、过期、总线失败、取消与多实例。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n01)=
## IT-N01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N01 |
| `task` | 首发联网范围与预算 |
| `implementation_status` | partial |
| `software_status` | recovery_134304_flash_116024_sram_budget_development_current_acceptance_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/f407_qiming_network/;components/network/;components/tls/;components/mqtt/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 固定一个精确 SoC/Board/PHY/backend 和 IPv4 纵切片，声明 MTU、连接、帧、任务、存储及排除项；裸机与 RTOS 组合分别准入。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n02)=
## IT-N02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N02 |
| `task` | 成熟 C 网络栈接入 |
| `implementation_status` | implemented |
| `software_status` | selected_stack_profiles_ordinary_accepted_eb4aba66_installed_sdk_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies/lwip.lock.json;components/network/CMakeLists.txt;tools/maintenance/lwip_lock.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 评估并锁定网络栈精确来源、许可和构建选项，采用组织托管依赖；离线 SDK 可显式构建，未链接时无驻留组件。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n03)=
## IT-N03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N03 |
| `task` | Ethernet frames 网络栈适配 |
| `implementation_status` | partial |
| `software_status` | payload_loans_tx_outcome_ordinary_accepted_eb4aba66_sdk_and_hil_separate |
| `hardware_status` | not_executed |
| `implementation_paths` | components/network/src/lwip_adapter.c;components/network/include/nexus/network/lwip_loaned_rx.h;docs/reference/api/components.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 复用现有 TX/RX loan，验证多 fragment、旧 token、链路变化、持有 RX 时停止及恢复失败；复制和零复制分别证明预算与生命周期。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n04)=
## IT-N04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N04 |
| `task` | 网络执行与有界内存 |
| `implementation_status` | implemented |
| `software_status` | copied_and_loaned_profile_contracts_ordinary_accepted_eb4aba66_sdk_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/network/port/lwipopts.h;components/network/src/lwip_adapter.c |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 明确栈执行上下文、timer 进度、池/arena、packet 和连接容量；无隐式任务或可扩张全局堆，耗尽、过载和真实关闭排空可验证。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n05)=
## IT-N05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N05 |
| `task` | 基础 IPv4 联网 |
| `implementation_status` | partial |
| `software_status` | bounded_ipv4_tcp_ordinary_accepted_eb4aba66_physical_scope_separate |
| `hardware_status` | not_executed |
| `implementation_paths` | components/network/src/lwip_adapter.c;components/network/README.md |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 实现选定 ARP/ICMP/UDP/TCP 与明确 DHCP/DNS 范围，实际验证地址变化、链路反复、半关闭、超时、重连和容量耗尽；物理链路另验收。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n06)=
## IT-N06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N06 |
| `task` | 熵接口与首个 provider |
| `implementation_status` | implemented |
| `software_status` | f407_entropy_contract_ordinary_accepted_eb4aba66_physical_entropy_open |
| `hardware_status` | not_executed |
| `implementation_paths` | io/include/nexus/io/entropy.h;soc/stm32/stm32f407/drivers/entropy.c;soc/stm32/stm32f407/entropy_instance.c;tests/google/entropy_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 明确来源、健康失败、可用量、上下文、deadline 和关闭合同；不足或硬件错误明确失败，不使用时间戳或固定种子替代真实熵。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n07)=
## IT-N07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N07 |
| `task` | TLS 资源与信任端口 |
| `implementation_status` | implemented |
| `software_status` | bounded_tls_ordinary_accepted_eb4aba66_earlier205_scope_retained |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/tls/;dependencies/mbedtls.lock.json;tests/google/tls_client_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 锁定成熟 TLS 库，分开单调 deadline 与证书日历时间，产品提供 trust/credential；坏证书、未知可信时间、熵失败、耗尽、取消和停止均有回归。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n08)=
## IT-N08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N08 |
| `task` | 可选 MQTT 适配 |
| `implementation_status` | implemented |
| `software_status` | bounded_mqtt_ordinary_accepted_eb4aba66_host_broker_scope_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/mqtt/;dependencies/coremqtt.lock.json;tests/google/mqtt_client_test.cpp |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 固定协议版本、QoS、payload 和会话范围，在所选 TCP/TLS 上验证断网、重复报文、满队列和停止；topic、授权、重试与离线队列策略外置。 |
| `owner` | codex |

(status-platform-delivery-completion-it-n09)=
## IT-N09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-N09 |
| `task` | 联网端到端故障与资源验证 |
| `implementation_status` | partial |
| `software_status` | recovery_host5_pcap10_arm_budget_development_current_acceptance_in_source_record |
| `hardware_status` | not_executed |
| `implementation_paths` | tests/contracts/lwip_mqtt_interop/;tests/contracts/f407_qiming_network/;tools/testing/mqtt_tls_interop.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 实际执行 host 协议交互、抓包、失效场景和 ARM 链接预算；结果绑定精确配置和产物，PHY/DMA 吞吐与长负载仍由实板证明。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s01)=
## IT-S01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S01 |
| `task` | 有界 Settings/KV |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/settings/src/settings.c;components/settings/include/nexus/components/settings.h |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 在现有双 bank Storage 上实现有界 key/value、schema、容量和提交边界；不重造物理 Flash 或持久化底层，默认值与迁移策略外置。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s02)=
## IT-S02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S02 |
| `task` | Settings 故障回归 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/settings/src/settings.c |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 实际验证中断提交、损坏、满容量、未知 schema 和失败后 reopen；commit-last 保留旧版本或真实新版本，host 文件不代表 Flash 掉电通过。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s03)=
## IT-S03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S03 |
| `task` | 镜像维护窄端口 |
| `implementation_status` | implemented |
| `software_status` | explicit_mcuboot_control_flash_contracts_ordinary_accepted_eb4aba66_sdk_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/mcuboot/port/flash_map_backend/flash_map_backend.h;components/mcuboot/include/nexus/components/mcuboot_control.h |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 使用显式 Flash region 检查 image bounds、对齐、擦写、同步、busy 与状态读回；不自动预留分区，不把 CRC 当签名认证。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s04)=
## IT-S04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S04 |
| `task` | 成熟 bootloader 外部集成 |
| `implementation_status` | partial |
| `software_status` | selected_mcuboot_overwrite_contract_ordinary_accepted_eb4aba66_physical_open |
| `hardware_status` | not_executed |
| `implementation_paths` | components/mcuboot/;dependencies/mcuboot.lock.json;tests/contracts/mcuboot_link/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 评估 MCUboot 等成熟方案，分别构建 bootloader 与应用并验证资源可行性；产品拥有分区、信任根、签名、确认、回滚和反降级策略。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s05)=
## IT-S05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S05 |
| `task` | 镜像故障与资源验证 |
| `implementation_status` | partial |
| `software_status` | mcuboot_model_contracts_ordinary_accepted_eb4aba66_sdk_and_physical_separate |
| `hardware_status` | not_executed |
| `implementation_paths` | tests/google/mcuboot_control_test.cpp;tests/google/mcuboot_boot_test.cpp;tests/contracts/mcuboot_link/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 验证越界、部分写、无效镜像、版本及状态异常和 ARM image/link 布局；物理掉电、真实启动、确认和回滚按实板记录独立验收。 |
| `owner` | codex |

(status-platform-delivery-completion-it-s06)=
## IT-S06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-S06 |
| `task` | 文件系统准入与适配 |
| `implementation_status` | conditional_no_current_need |
| `software_status` | reviewed_consumers_byte_oriented_no_filesystem_dependency_added |
| `hardware_status` | not_executed |
| `implementation_paths` | components/;dependencies/;tests/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 由真实文件需求固定介质后选择一个文件系统，明确块几何、sync、错误、互斥和挂载寿命；无需求时记录选型结论，不引入无用依赖。 |
| `owner` | codex |

(status-platform-delivery-completion-it-x02)=
## IT-X02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-X02 |
| `task` | 可选 GDB 客户端流程 |
| `implementation_status` | implemented |
| `software_status` | optional_gdb_client_contract_ordinary_accepted_eb4aba66_exact_scope_separate |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/device.py;tools/testing/test_dev_gdb_client.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 保留 localhost GDB server，客户端绑定原构建 ELF 和工具身份，遵守超时、取消、输出与 station lock；退出实际回收所属进程。 |
| `owner` | codex |

(status-platform-delivery-completion-it-x03)=
## IT-X03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-X03 |
| `task` | 串口主机与波特率扩展 |
| `implementation_status` | implemented |
| `software_status` | ordinary_accepted_eb4aba66_installed_sdk_status_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/device.py;tools/hil/serial_echo.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 按实际需要扩展波特率和主机，保留明确设备身份；终端恢复、断开、输出上限、锁冲突与取消有回归，不自动选择首个串口。 |
| `owner` | codex |

(status-platform-delivery-completion-it-x04)=
## IT-X04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-X04 |
| `task` | 统一设备 runner 扩展 |
| `implementation_status` | conditional_no_current_need |
| `software_status` | no_new_probe_requirement_existing_openocd_path_preserved_no_runner_added |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/;tools/testing/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | OpenOCD 现有路径不退化，新增实际需要的探针工具复用设备身份、ELF/provenance、超时、输出、锁和失败隔离合同。 |
| `owner` | codex |

(status-platform-delivery-completion-it-x06)=
## IT-X06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-X06 |
| `task` | 固件产物查询 |
| `implementation_status` | implemented |
| `software_status` | current_cli_contracts_ordinary_accepted_eb4aba66_exact_scope_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/artifacts.py;tools/dev/dev.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 展示本次真实 ELF/BIN/map、链接资源与生成配置；过期、不匹配或未完成产物明确标识，不以文件或目录存在声明构建通过。 |
| `owner` | codex |

(status-platform-delivery-completion-it-t01)=
## IT-T01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-T01 |
| `task` | 测试计划只读预览 |
| `implementation_status` | implemented |
| `software_status` | current_cli_contracts_ordinary_accepted_eb4aba66_exact_scope_in_source_record |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/plan.py;tools/dev/dev.py |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 从已有 plan/preset/测试发现和生命周期目录派生执行命令、覆盖域、选择原因、硬件需求与未知项；预览不执行测试或产生成功资格。 |
| `owner` | codex |

(status-platform-delivery-completion-it-t06)=
## IT-T06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-T06 |
| `task` | 高风险输入与并发补充验证 |
| `implementation_status` | partial |
| `software_status` | focused_fault_sanitizer_host_scopes_retained_current_formal_plan_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/;tools/testing/ |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md |
| `exit_condition` | 对网络、Settings、镜像和关闭路径使用适用 sanitizer/fuzz/故障注入，保留真实最小失败输入；不以伪硬件模型扩大物理结论。 |
| `owner` | codex |

(status-platform-delivery-completion-it-e01)=
## IT-E01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-E01 |
| `task` | 工业控制外部参考系统 |
| `implementation_status` | partial |
| `software_status` | current_private_f59debf_pinned_P2_full17presets116commands_28CTest18Google204_installed2CTest_passed_physical_scope_open |
| `hardware_status` | not_executed |
| `implementation_paths` | X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | 组合现有采集、Modbus、owner、Storage 和 watchdog，实际展示启动、运行、维护、停止、失败与恢复；任务、寄存器表和产品策略保持外部。 |
| `owner` | codex |

(status-platform-delivery-completion-it-e02)=
## IT-E02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IT-E02 |
| `task` | 联网网关外部参考系统 |
| `implementation_status` | partial |
| `software_status` | current_private_f59debf_pinned_P2_source_installed5realCbroker_cases_each_passed_join_release_cleanup_physical_open |
| `hardware_status` | not_executed |
| `implementation_paths` | X-Gen-Lab/nexus-examples |
| `evidence_paths` | docs/delivery/platform-delivery-completion-results.json;docs/status/software-verification.md;docs/delivery/platform-delivery-completion-results.json#source_version_records_20261103.final_public_source_P2_ab532250.subsequent_actual_results.private_current_E |
| `exit_condition` | 用精确外部组合完成 Ethernet→TCP/TLS→MQTT，展示断网、耗尽、重连和关闭；凭据、授权和业务策略外置，私有源及证据不进入公共包。 |
| `owner` | codex |
