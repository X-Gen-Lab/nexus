---
id: status.ledgers.developer-experience
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-developer-experience)=
# 开发体验75 项台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/developer-experience-execution.csv)。
源文件：`docs/design/developer-experience-execution.csv`；SHA-256：`4262874b4348573e4908d30634fb02ddf33a63a5f83119f731efb1634fd07855`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 75 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-developer-experience-w1-01)=
## W1-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-01 |
| `task` | Separate application and maintainer entry points |
| `implementation_status` | implemented |
| `software_status` | source_and_independent_bilingual_review_verified_strict_html_ci_live |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/tutorials/source-sdk.md;README.md;README_CN.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Application quickstart precedes SDK maintenance and both locales agree |
| `owner` | codex |

(status-developer-experience-w1-02)=
## W1-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-02 |
| `task` | Minimal Native product template |
| `implementation_status` | implemented |
| `software_status` | native_execution_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/templates/native |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | External C11 product builds and runs through strong factory references |
| `owner` | codex |

(status-developer-experience-w1-03)=
## W1-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-03 |
| `task` | F407 board product templates |
| `implementation_status` | implemented |
| `software_status` | actual_arm_link_and_budget_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/templates/qiming;X-Gen-Lab/nexus-examples/templates/sky |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Both reviewed F407 boards link with exact facts and explicit physical unknowns |
| `owner` | codex |

(status-developer-experience-w1-04)=
## W1-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-04 |
| `task` | F470 board product template |
| `implementation_status` | implemented |
| `software_status` | actual_arm_link_and_budget_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/templates/liangshan |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Independent GD32 product consumes only public interfaces |
| `owner` | codex |

(status-developer-experience-w1-05)=
## W1-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-05 |
| `task` | Versioned external project creation |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/projects.py;tools/testing/test_dev_projects.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_projects.py;/tmp/nexus-iteration-20261024/devtools/projects-origin-green.log;/tmp/nexus-iteration-20261024/devtools/projects-budget-green.log |
| `exit_condition` | Pinned template creates atomically and never overwrites an existing destination |
| `owner` | codex |

(status-developer-experience-w1-06)=
## W1-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-06 |
| `task` | Daily flash debug and monitor tools |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/dev/device.py;tools/testing/test_dev_device.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_device.py;/tmp/nexus-iteration-20261024/devtools/device-actions-green-01.log |
| `exit_condition` | Explicit target probe image and bounded process lifecycle; hardware execution separate |
| `owner` | codex |

(status-developer-experience-w1-07)=
## W1-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W1-07 |
| `task` | Clean user onboarding exercise |
| `implementation_status` | implemented |
| `software_status` | P_sdk_E_native_onboarding_and_authored_error_recovery_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/projects.py;docs/tutorials/source-sdk.md;X-Gen-Lab/nexus-examples/templates/native |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual Native creation build execution and configuration-error recovery recorded |
| `owner` | codex |

(status-developer-experience-w2-01)=
## W2-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-01 |
| `task` | Human-readable assembly explanation |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Summary derives from resolved facts and preserves explicit JSON output |
| `owner` | codex |

(status-developer-experience-w2-02)=
## W2-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-02 |
| `task` | Configuration location and provenance diagnostics |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;tools/configure/configure.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Failure identifies file key and source constraint without changing rejection |
| `owner` | codex |

(status-developer-experience-w2-03)=
## W2-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-03 |
| `task` | Resource conflict explanations |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/configure.py;tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Conflicting owners and resource are identified with real rejection regressions |
| `owner` | codex |

(status-developer-experience-w2-04)=
## W2-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-04 |
| `task` | Board-first binding and capability queries |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Reviewed choices and limits visible without a guessed full assembly |
| `owner` | codex |

(status-developer-experience-w2-05)=
## W2-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-05 |
| `task` | Validated profile-based assembly initialization |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Complete explicit TOML is resolved without fallback or implicit overlays |
| `owner` | codex |

(status-developer-experience-w2-06)=
## W2-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-06 |
| `task` | Assembly difference explanation |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Changes in routes modes storage and execution requirements are explained |
| `owner` | codex |

(status-developer-experience-w2-07)=
## W2-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W2-07 |
| `task` | Editor schema and unit guidance |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/authored.py;tools/configure/assembly.schema.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Hints and examples agree with actual accepted and rejected fields |
| `owner` | codex |

(status-developer-experience-w3-01)=
## W3-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-01 |
| `task` | Provider extension responsibility audit |
| `implementation_status` | implemented |
| `software_status` | generation_equivalence_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/README.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log;/tmp/nexus-iteration-20261024/configuration/emission-equivalence.json |
| `exit_condition` | Family-specific decisions and necessary common responsibilities mapped |
| `owner` | codex |

(status-developer-experience-w3-02)=
## W3-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-02 |
| `task` | Move family-specific resolution decisions |
| `implementation_status` | implemented |
| `software_status` | generation_equivalence_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/configure.py;tools/configure/providers/common.py;tools/configure/providers/*/__init__.py;tools/configure/providers/stm32g474/adc_group.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log;/tmp/nexus-iteration-20261024/configuration/emission-equivalence.json |
| `exit_condition` | Common resolver retains transaction identity and resource arbitration |
| `owner` | codex |

(status-developer-experience-w3-03)=
## W3-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-03 |
| `task` | Construction-driven emission |
| `implementation_status` | implemented |
| `software_status` | generation_equivalence_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/emission.py;tools/configure/i2s2.py;tools/configure/providers/stm32h743/__init__.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log;/tmp/nexus-iteration-20261024/configuration/emission-equivalence.json |
| `exit_condition` | Emitters consume explicit provider construction and cleanup contracts |
| `owner` | codex |

(status-developer-experience-w3-04)=
## W3-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-04 |
| `task` | Public class and family binding separation |
| `implementation_status` | implemented |
| `software_status` | generation_equivalence_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/providers/common.py;tools/configure/providers/descriptor.py;tools/configure/README.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log;/tmp/nexus-iteration-20261024/configuration/emission-equivalence.json |
| `exit_condition` | Same-class provider additions do not duplicate public lifecycle semantics |
| `owner` | codex |

(status-developer-experience-w3-05)=
## W3-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-05 |
| `task` | Reviewed peripheral IP reuse pilot |
| `implementation_status` | verified_existing |
| `software_status` | two_family_shared_engine_regressions_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | soc/common/i2c_v1_async.c;tests/google/i2c_multi_controller_test.cpp;tests/google/i2c_multi_controller.cmake |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/google/i2c_multi_controller_test.cpp;tests/google/i2c_multi_controller.cmake;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json;/tmp/nexus-iteration-20261024/provider-vendor/final-google-results.json |
| `exit_condition` | Equivalent hardware protocols share implementation while errata remain explicit |
| `owner` | codex |

(status-developer-experience-w3-06)=
## W3-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-06 |
| `task` | Same-class instance extension exercise |
| `implementation_status` | verified_existing |
| `software_status` | two_family_multiple_controller_regressions_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/google/i2c_multi_controller_test.cpp;tests/google/i2c_multi_controller.cmake |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/google/i2c_multi_controller_test.cpp;tests/google/i2c_multi_controller.cmake;/tmp/nexus-iteration-20261024/provider-vendor/final-google-results.json |
| `exit_condition` | Independent resources IRQ bindings and retained software ownership verified |
| `owner` | codex |

(status-developer-experience-w3-07)=
## W3-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W3-07 |
| `task` | H743 frame DMA2D source ownership |
| `implementation_status` | implemented |
| `software_status` | byte_equivalence_and_P_complete_sdk_consumption_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | components/soc/stm32/stm32h743_frame_dma2d;components/soc/stm32/CMakeLists.txt;cmake/package/package_source_sdk.py;Doxyfile |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Chip extension moved with stable public identity and updated build SDK references |
| `owner` | codex |

(status-developer-experience-w4-01)=
## W4-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-01 |
| `task` | Exact maintained support dimensions |
| `implementation_status` | implemented |
| `software_status` | configuration_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Part revision mode clock memory backend route and evidence remain distinct |
| `owner` | codex |

(status-developer-experience-w4-02)=
## W4-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-02 |
| `task` | Derived user support selection view |
| `implementation_status` | implemented |
| `software_status` | configuration_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Existing authorities produce useful capability choices without a second status database |
| `owner` | codex |

(status-developer-experience-w4-03)=
## W4-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-03 |
| `task` | Launch board fact and unknown audit |
| `implementation_status` | reviewed |
| `software_status` | official_source_and_unknown_facts_audited |
| `hardware_status` | not_applicable |
| `implementation_paths` | boards/stm32f407_qiming_v31;boards/stm32f407ve_sky_qingchun;boards/gd32f470_liangshan;tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/provider-vendor/official-reuse-audit.json |
| `exit_condition` | Reviewed sources retained and unobserved PCB wiring remains unknown |
| `owner` | codex |

(status-developer-experience-w4-04)=
## W4-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-04 |
| `task` | Mode execution responsibility summaries |
| `implementation_status` | implemented |
| `software_status` | configuration_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Admission executor service DMA borrow stop and recovery limits recorded |
| `owner` | codex |

(status-developer-experience-w4-05)=
## W4-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-05 |
| `task` | Unsupported capability rejection boundaries |
| `implementation_status` | implemented |
| `software_status` | configuration_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/test_experience.py;tools/configure/providers/*/__init__.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/configure/test_experience.py;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log;/tmp/nexus-iteration-20261024/configuration/emission-equivalence.json |
| `exit_condition` | Invalid route memory backend and modes reject without implicit substitution |
| `owner` | codex |

(status-developer-experience-w4-06)=
## W4-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-06 |
| `task` | Provider build SDK and test coverage mapping |
| `implementation_status` | implemented |
| `software_status` | mode_mapping_and_P_complete_50_consumer_plan_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/dependencies.py;tools/configure/providers/descriptor.py;tools/configure/experience.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Every admitted mode resolves to actual implementation and explicit verification scope |
| `owner` | codex |

(status-developer-experience-w4-07)=
## W4-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W4-07 |
| `task` | Compatibility and migration impact |
| `implementation_status` | implemented |
| `software_status` | configuration_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/README.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/configuration/handoff.json;/tmp/nexus-iteration-20261024/configuration/experience-final-green.log |
| `exit_condition` | Source interface configuration and reference-scope changes explained precisely |
| `owner` | codex |

(status-developer-experience-w5-01)=
## W5-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-01 |
| `task` | Three execution ownership recipes |
| `implementation_status` | implemented |
| `software_status` | native_recipes_passed_translation_reviewed |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/guides/developer-workflow.md;X-Gen-Lab/nexus-examples/docs/developer-recipes.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/docs/source-check-02.json |
| `exit_condition` | Main loop exclusive task and shared owner each define progress and shutdown |
| `owner` | codex |

(status-developer-experience-w5-02)=
## W5-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-02 |
| `task` | Baremetal UART complete lifecycle example |
| `implementation_status` | implemented |
| `software_status` | native_execution_passed_mcu_link_passed_physical_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | X-Gen-Lab/nexus-examples/templates;tests/google |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/google;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Normal busy deadline cancel late IRQ and settlement preserve storage |
| `owner` | codex |

(status-developer-experience-w5-03)=
## W5-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-03 |
| `task` | Shared SPI two-endpoint example |
| `implementation_status` | implemented |
| `software_status` | native_and_real_freertos_host_passed_arm_link_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | X-Gen-Lab/nexus-examples/apps/shared_spi_runtime;X-Gen-Lab/nexus-examples/tests/developer_recipes_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json;X-Gen-Lab/nexus-examples/tests/developer_recipes_test.cpp;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Explicit owner tasks storage arbitration cancellation and shutdown verified |
| `owner` | codex |

(status-developer-experience-w5-04)=
## W5-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-04 |
| `task` | Continuous stream slow-consumer example |
| `implementation_status` | implemented |
| `software_status` | native_ownership_passed_h743_arm_link_passed |
| `hardware_status` | not_executed |
| `implementation_paths` | X-Gen-Lab/nexus-examples/apps/stream_recovery;X-Gen-Lab/nexus-examples/tests/developer_recipes_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json;X-Gen-Lab/nexus-examples/tests/developer_recipes_test.cpp;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Block loans loss backpressure and recovery are observable without overwrite |
| `owner` | codex |

(status-developer-experience-w5-05)=
## W5-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-05 |
| `task` | Composed lifecycle diagnostics |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/include/nexus/io/diagnostic.h;io/src/mechanisms/diagnostic.c;tests/google/io_diagnostic_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Known facts distinguish causes; missing evidence stays unknown and does not mutate ownership |
| `owner` | codex |

(status-developer-experience-w5-06)=
## W5-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-06 |
| `task` | Explicit DMA storage assistance |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/include/nexus/io/dma.h;io/src/mechanisms/dma.c;tests/google/dma_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Alignment extent memory and cache ownership validated without hidden bounce buffers |
| `owner` | codex |

(status-developer-experience-w5-07)=
## W5-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W5-07 |
| `task` | Necessary ergonomic helpers and timeout audit |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/guides/developer-workflow.md;X-Gen-Lab/nexus-examples/apps/shared_spi_runtime;X-Gen-Lab/nexus-examples/apps/stream_recovery |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Real consumers justify helpers and no outstanding stack borrow escapes |
| `owner` | codex |

(status-developer-experience-w6-01)=
## W6-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-01 |
| `task` | Representative benchmark identity |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/compare.py;tests/contracts/io_storage_cost |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Configuration tools profiles load and reference workloads fixed |
| `owner` | codex |

(status-developer-experience-w6-02)=
## W6-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-02 |
| `task` | Image resource decomposition |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/resources.py;tests/contracts/io_storage_cost/probe.c |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Flash load layout sections RAM domains and caller storage reported separately |
| `owner` | codex |

(status-developer-experience-w6-03)=
## W6-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-03 |
| `task` | Instance and capability marginal costs |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/io_storage_cost;tools/measurement/compare.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Actual linked variants establish controller endpoint slot and mode deltas |
| `owner` | codex |

(status-developer-experience-w6-04)=
## W6-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-04 |
| `task` | Operation table and linker retention audit |
| `implementation_status` | implemented |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/interfaces.py;tools/testing/test_interface_storage.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Retained methods and selected modes inspected in actual ELF variants |
| `owner` | codex |

(status-developer-experience-w6-05)=
## W6-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-05 |
| `task` | IRQ service and throughput measurement preparation |
| `implementation_status` | prepared |
| `software_status` | software_verified |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/measure.py;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Existing probes and capture tools preserve units and physical limitations |
| `owner` | codex |

(status-developer-experience-w6-06)=
## W6-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-06 |
| `task` | Evidence-based resource optimization |
| `implementation_status` | reviewed_keep_existing_ops |
| `software_status` | verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | io/src/api;components/owner_observer;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Only measured benefit with equivalent behavior enters production |
| `owner` | codex |

(status-developer-experience-w6-07)=
## W6-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W6-07 |
| `task` | Performance regression rules |
| `implementation_status` | implemented |
| `software_status` | software_verified |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/measurement/soc_costs.py;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Static and physical baselines separate noise and observed latency from WCET |
| `owner` | codex |

(status-developer-experience-w7-01)=
## W7-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-01 |
| `task` | Official driver reuse inventory |
| `implementation_status` | reviewed |
| `software_status` | seven_dependency_scopes_audited |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies;tests/contracts/stm32_hal_reference/README.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/contracts/stm32_hal_reference/README.md;/tmp/nexus-iteration-20261024/provider-vendor/official-reuse-audit.json;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json |
| `exit_condition` | Header LL algorithm and complete transport reuse distinguished by family |
| `owner` | codex |

(status-developer-experience-w7-02)=
## W7-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-02 |
| `task` | Vendor dependency pairing and patch audit |
| `implementation_status` | reviewed |
| `software_status` | seven_locked_sources_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | dependencies;tools/maintenance |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/provider-vendor/official-reuse-audit.json |
| `exit_condition` | Exact source license CMSIS pairing and SDK closure reviewed |
| `owner` | codex |

(status-developer-experience-w7-03)=
## W7-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-03 |
| `task` | Bounded vendor transport comparison contract |
| `implementation_status` | implemented |
| `software_status` | bounded_reference_contract_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/stm32_hal_reference/README.md;tests/contracts/stm32_hal_reference/adapter.h |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/contracts/stm32_hal_reference/README.md;tests/contracts/stm32_hal_reference/adapter.h;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json |
| `exit_condition` | An existing maintained representative mode fixes semantics costs and failure exits |
| `owner` | codex |

(status-developer-experience-w7-04)=
## W7-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-04 |
| `task` | Real STM32 HAL adaptation evaluation |
| `implementation_status` | implemented_test_reference |
| `software_status` | real_hal_adapter_15_cases_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/stm32_hal_reference/adapter.c;tests/contracts/stm32_hal_reference/reference_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/contracts/stm32_hal_reference/adapter.c;tests/contracts/stm32_hal_reference/reference_test.cpp;/tmp/nexus-iteration-20261024/provider-vendor/hal-review-green.xml;/tmp/nexus-iteration-20261024/provider-vendor/hal-review-red.log;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json |
| `exit_condition` | Tick deadline callbacks abort and drain evaluated with one live peripheral owner |
| `owner` | codex |

(status-developer-experience-w7-05)=
## W7-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-05 |
| `task` | GD32 official-operation reuse evaluation |
| `implementation_status` | verified_existing_with_new_test |
| `software_status` | real_spl_three_cases_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/google/gd32_official_gpio_test.cpp;tests/google/gd32_official.cmake |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/google/gd32_official_gpio_test.cpp;tests/google/gd32_official.cmake;/tmp/nexus-iteration-20261024/provider-vendor/final-google-results.json |
| `exit_condition` | Actual GD32 source and errata govern the chosen bounded algorithm |
| `owner` | codex |

(status-developer-experience-w7-06)=
## W7-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-06 |
| `task` | Semantic cost and maintenance comparison |
| `implementation_status` | evaluation_completed_scoped_negative |
| `software_status` | real_vendor_behavior_production_cost_and_nonequivalent_layout_separated |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/stm32_hal_reference/README.md;tests/contracts/stm32_hal_reference/layout.c;tools/measurement/compare.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/contracts/stm32_hal_reference/README.md;tests/contracts/stm32_hal_reference/layout.c;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json;/tmp/nexus-iteration-20261024/provider-vendor/hal-layout.json |
| `exit_condition` | Actual vendor execution and linked cost evidence support a scoped decision |
| `owner` | codex |

(status-developer-experience-w7-07)=
## W7-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W7-07 |
| `task` | Single production implementation decision |
| `implementation_status` | decision_recorded |
| `software_status` | single_production_ll_retained_scoped_reason_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/stm32_hal_reference/README.md;docs/design/developer-experience.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tests/contracts/stm32_hal_reference/README.md;/tmp/nexus-iteration-20261024/provider-vendor/handoff.json |
| `exit_condition` | Accepted implementation or justified retention documented; unused experimental paths removed |
| `owner` | codex |

(status-developer-experience-w8-01)=
## W8-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-01 |
| `task` | Execution stage cost baseline |
| `implementation_status` | implemented |
| `software_status` | historical_receipts_observed_not_iteration_total |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/timings.py;tools/testing/test_dev_timings.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_timings.py;/tmp/nexus-iteration-20261024/devtools/observed-costs-01.json;/tmp/nexus-iteration-20261024/devtools/timings-green-01.log |
| `exit_condition` | Existing receipts distinguish preparation configure compile test docs and SDK with unknowns preserved |
| `owner` | codex |

(status-developer-experience-w8-02)=
## W8-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-02 |
| `task` | Focused developer feedback discovery |
| `implementation_status` | implemented |
| `software_status` | focused_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/dev.py;tools/testing/test_dev_efficiency.py;tools/dev/README.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_efficiency.py;/tmp/nexus-iteration-20261024/devtools/efficiency-green-01.log |
| `exit_condition` | Selected development tests easy to run and never qualify a complete source |
| `owner` | codex |

(status-developer-experience-w8-03)=
## W8-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-03 |
| `task` | Repeated setup and process cost reduction |
| `implementation_status` | implemented |
| `software_status` | metadata_cost_measured_only |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/dev.py;tools/testing/test_dev_efficiency.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_efficiency.py;/tmp/nexus-iteration-20261024/devtools/feedback-cost-comparison.json;/tmp/nexus-iteration-20261024/devtools/efficiency-green-01.log |
| `exit_condition` | Equivalent independent coverage retained with measured preparation improvements |
| `owner` | codex |

(status-developer-experience-w8-04)=
## W8-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-04 |
| `task` | Cache invalidation and concurrency budgets |
| `implementation_status` | verified_existing |
| `software_status` | cache_and_budget_regressions_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/cache.py;tools/dev/process.py;tools/testing/test_dev_efficiency.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json;tools/testing/test_dev_efficiency.py;/tmp/nexus-iteration-20261024/devtools/focused-integration-green-03.log |
| `exit_condition` | Validated inputs and finite resource budgets improve repeat execution without report caching |
| `owner` | codex |

(status-developer-experience-w8-05)=
## W8-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-05 |
| `task` | Test retention review |
| `implementation_status` | reviewed |
| `software_status` | independent_behavior_coverage_preserved |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/README.md;docs/development/workflow.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/devtools/test-retention-candidates.json;/tmp/nexus-iteration-20261024/devtools/test-retention-review.md |
| `exit_condition` | Each deletion candidate identifies obsolete behavior or independent equivalent coverage |
| `owner` | codex |

(status-developer-experience-w8-06)=
## W8-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-06 |
| `task` | Remove confirmed obsolete or duplicate tests |
| `implementation_status` | reviewed_no_removal_justified |
| `software_status` | no_test_deletion_or_gate_reduction_claimed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/README.md;docs/development/workflow.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/devtools/test-retention-review.md |
| `exit_condition` | Meaningful failure ownership isolation concurrency and consumption regressions retained |
| `owner` | codex |

(status-developer-experience-w8-07)=
## W8-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W8-07 |
| `task` | Sustainable feedback and delivery constraints |
| `implementation_status` | implemented |
| `software_status` | P_complete_ordinary_gate_and_feedback_boundaries_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/README.md;docs/development/workflow.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Measured feedback trends failures cancellation and normal complete gates remain enforced |
| `owner` | codex |

(status-developer-experience-w9-01)=
## W9-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-01 |
| `task` | Remote platform and examples baseline |
| `implementation_status` | reviewed |
| `software_status` | actual_remote_baselines_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | .gitmodules;X-Gen-Lab/nexus-examples/dependencies/nexus.lock.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Actual commits gitlinks and locks recorded rather than stale local snapshots |
| `owner` | codex |

(status-developer-experience-w9-02)=
## W9-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-02 |
| `task` | Real examples platform migration |
| `implementation_status` | implemented |
| `software_status` | E_lock_gitlink_and_P_public_sdk_consumption_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/dependencies/nexus.lock.json;X-Gen-Lab/nexus-examples/.gitmodules |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Final platform source lock gitlink and public consumption agree |
| `owner` | codex |

(status-developer-experience-w9-03)=
## W9-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-03 |
| `task` | Product recipes maintained externally |
| `implementation_status` | implemented |
| `software_status` | external_application_behavior_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/templates;X-Gen-Lab/nexus-examples/apps/shared_spi_runtime;X-Gen-Lab/nexus-examples/apps/stream_recovery |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/examples/development-summary.json |
| `exit_condition` | Templates and lifecycle examples are real applications outside platform core |
| `owner` | codex |

(status-developer-experience-w9-04)=
## W9-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-04 |
| `task` | Examples Native application CI |
| `implementation_status` | implemented |
| `software_status` | E_complete_local_native_gate_passed_hosted_ci_blocked_billing |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/.github/workflows/ci.yml;X-Gen-Lab/nexus-examples/scripts/tdd_gate.py;X-Gen-Lab/nexus-examples/tests/developer_recipes_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual successful rejected and shutdown behavior runs with nonempty outcomes |
| `owner` | codex |

(status-developer-experience-w9-05)=
## W9-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-05 |
| `task` | Examples ARM consumption CI |
| `implementation_status` | implemented |
| `software_status` | E_39_arm_images_and_budgets_passed_hosted_ci_blocked_billing |
| `hardware_status` | not_applicable |
| `implementation_paths` | X-Gen-Lab/nexus-examples/.github/workflows/ci.yml;X-Gen-Lab/nexus-examples/apps/shared_spi_runtime/CMakeLists.txt;X-Gen-Lab/nexus-examples/apps/stream_recovery/h743/CMakeLists.txt |
| `evidence_paths` | docs/delivery/developer-experience-results.json;X-Gen-Lab/nexus-examples/.github/workflows/ci.yml |
| `exit_condition` | Exact maintained board backend tuples link within public and resource boundaries |
| `owner` | codex |

(status-developer-experience-w9-06)=
## W9-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-06 |
| `task` | Candidate to examples evidence association |
| `implementation_status` | implemented |
| `software_status` | immutable_P_E_sdk_and_application_evidence_associated |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Platform and examples identities link independent SDK and application execution |
| `owner` | codex |

(status-developer-experience-w9-07)=
## W9-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | W9-07 |
| `task` | Upgrade exercise and bilingual migration guide |
| `implementation_status` | implemented |
| `software_status` | locked_platform_upgrade_and_actual_error_recovery_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/tutorials/source-sdk.md;docs/guides/developer-workflow.md;X-Gen-Lab/nexus-examples/docs/developer-recipes.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual locked baseline upgrade and failure diagnosis documented with hardware status separate |
| `owner` | codex |

(status-developer-experience-i-01)=
## I-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-01 |
| `task` | Establish execution source and branches |
| `implementation_status` | implemented |
| `software_status` | actual_platform_and_examples_branches_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-i-02)=
## I-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-02 |
| `task` | Independent cross-layer review |
| `implementation_status` | reviewed |
| `software_status` | independent_cross_layer_findings_fixed_and_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/test_experience.py;tools/testing/test_dev_projects.py;tests/contracts/stm32_hal_reference/reference_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-i-03)=
## I-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-03 |
| `task` | Final-source complete software gates |
| `implementation_status` | implemented |
| `software_status` | P_complete_ordinary_gate_passed_platform_remote_ci_not_yet_observed |
| `hardware_status` | not_applicable |
| `implementation_paths` | scripts/ci/tdd_gate.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-i-04)=
## I-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-04 |
| `task` | Complete source SDK and external consumption |
| `implementation_status` | implemented |
| `software_status` | P_publishable_sdk_50_consumers_and_E_onboarding_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verify_consumers.py;cmake/package/package_source_sdk.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-i-05)=
## I-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-05 |
| `task` | Bilingual documentation and diagram delivery |
| `implementation_status` | implemented |
| `software_status` | local_fresh_bilingual_checks_passed_strict_html_ci_pending |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/developer-experience.md;docs/guides/developer-workflow.md;docs/locale |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-i-06)=
## I-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | I-06 |
| `task` | Candidate identity and review handoff |
| `implementation_status` | implemented |
| `software_status` | P_E_software_review_handoff_bound_no_formal_candidate |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual final-source outputs and original evidence required |
| `owner` | codex |

(status-developer-experience-h1)=
## H1

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H1 |
| `task` | Launch board bring-up and developer tools |
| `implementation_status` | prepared |
| `software_status` | software_tools_verified_physical_not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/dev/device.py;tools/hil/fixtures |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |

(status-developer-experience-h2)=
## H2

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H2 |
| `task` | Serial SPI I2C and recovery |
| `implementation_status` | prepared |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |

(status-developer-experience-h3)=
## H3

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H3 |
| `task` | IRQ CPU critical-section and latency measurement |
| `implementation_status` | prepared |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |

(status-developer-experience-h4)=
## H4

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H4 |
| `task` | Continuous DMA and Cache coherence |
| `implementation_status` | prepared |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |

(status-developer-experience-h5)=
## H5

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H5 |
| `task` | Flash watchdog and exceptional shutdown |
| `implementation_status` | prepared |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil;docs/guides/performance-hil.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json;/tmp/nexus-iteration-20261024/io-cost/handoff.json;/tmp/nexus-iteration-20261024/io-cost/observations/summary.json |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |

(status-developer-experience-h6)=
## H6

| 原字段 | 原值 |
| --- | --- |
| `task_id` | H6 |
| `task` | Physical support-scope sealing |
| `implementation_status` | pending |
| `software_status` | not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | （原字段为空） |
| `evidence_paths` | （原字段为空） |
| `exit_condition` | Connected identified equipment exact image and actual observations required |
| `owner` | codex |
