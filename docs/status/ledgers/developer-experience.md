---
id: status.ledgers.developer-experience
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-developer-experience)=
# 开发体验执行台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/developer-experience-execution.csv)。
源文件：`docs/design/developer-experience-execution.csv`；SHA-256：`ad391b10a701d14d8fdbf348b53ea3c86156b415123bc47a49285c0c7d0fdeff`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 139 条原记录，仅表示来源行数。

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

(status-developer-experience-cf01)=
## CF01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF01 |
| `task` | P0: Correct strong factory authoring and SDK guidance |
| `implementation_status` | implemented |
| `software_status` | factory_guidance_and_strict_docs_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/README.md;docs/reference/configuration/assembly.md;tools/configure/test_factory_includes.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Strong header is always generated; capabilities is independent; no invented authoring switch. |
| `owner` | codex |

(status-developer-experience-cf02)=
## CF02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF02 |
| `task` | P1: Use strong references in primary public SDK consumers |
| `implementation_status` | implemented |
| `software_status` | current_sdk_strong_consumers_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verify_consumers.py;tools/testing/test_sdk_factory_strong.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Actual installed and relocated C11/C++17 primary consumers use strong references; intentional raw getter regressions remain. |
| `owner` | codex |

(status-developer-experience-cf03)=
## CF03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF03 |
| `task` | P1: Validate nominal reference errors in actual consumer contexts |
| `implementation_status` | implemented |
| `software_status` | current_sdk_nominal_type_contract_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/contracts/test_source_sdk.py;tools/testing/test_sdk_factory_strong.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Positive C11/C++17 contexts execute; cross-class controller-endpoint and raw-ID misuse fails specifically for type mismatch. |
| `owner` | codex |

(status-developer-experience-cf04)=
## CF04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF04 |
| `task` | P1: Explain exact generated factory accessors |
| `implementation_status` | implemented |
| `software_status` | accessor_projection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/factory.py;tools/configure/experience.py;tools/configure/test_factory_includes.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Include return type REF constant and getter derive from sole factory projection including hyphen word and enhanced endpoint selections. |
| `owner` | codex |

(status-developer-experience-cf05)=
## CF05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF05 |
| `task` | P0: Bind project creation and build instructions to one platform source |
| `implementation_status` | implemented |
| `software_status` | selected_platform_create_consume_and_rollback_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/projects.py;tools/testing/test_dev_projects.py;tools/testing/test_project_consumption.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Explicit source or SDK selects actual resolver and compatibility contract; exact subsequent build arguments match; expected lock and observed identity stay separate; no fallback. |
| `owner` | codex |

(status-developer-experience-cf06)=
## CF06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | CF06 |
| `task` | P1: Exercise complete pinned project creation and consumption |
| `implementation_status` | implemented |
| `software_status` | pinned_project_current_sdk_consumption_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/projects.py;tools/testing/test_project_consumption.py;tools/testing/test_dev_projects.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Pinned manifest through create then selected SDK actual build and Native execution; spaces relocation and rollback covered; private template bytes stay private. |
| `owner` | codex |

(status-developer-experience-ep01)=
## EP01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP01 |
| `task` | P1: Inspect provider manifest before trusted code loading |
| `implementation_status` | implemented |
| `software_status` | static_manifest_contract_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py;tools/configure/soc-package.schema.json;tools/configure/providers/descriptor.py;tools/configure/test_provider_manifest.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Read-only immutable manifest contract checks fields paths versions and hashes without provider import; editor schema derives from same contract. |
| `owner` | codex |

(status-developer-experience-ep02)=
## EP02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP02 |
| `task` | P1: Explicitly check and refresh declared package input hashes |
| `implementation_status` | implemented |
| `software_status` | explicit_atomic_input_refresh_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py;tools/configure/test_provider_manifest.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Only declared explicit inputs may refresh atomically; stale paths are diagnosed; missing symlink traversal duplicate and escape inputs reject without scanning or automatic upgrades. |
| `owner` | codex |

(status-developer-experience-ep03)=
## EP03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP03 |
| `task` | P1: Enable external package Board-first discovery and init |
| `implementation_status` | implemented |
| `software_status` | external_board_first_authoring_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/experience.py;tools/configure/test_provider_authoring.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Explicit soc-package supports route-first queries and relative-path TOML creation using the existing resolver; no central registration or hidden clock/backend defaults. |
| `owner` | codex |

(status-developer-experience-ep04)=
## EP04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP04 |
| `task` | P1: Diagnose selected provider CMake contract early |
| `implementation_status` | implemented |
| `software_status` | selected_provider_cmake_contract_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | CMakeLists.txt;cmake/platform/Provider.cmake;tools/testing/test_selected_provider_contract.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Selected target startup system and private dependencies have clear configure errors; builtin and external reuse one CMake graph and target policy. |
| `owner` | codex |

(status-developer-experience-ep05)=
## EP05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP05 |
| `task` | P1: Ship one canonical C provider lifecycle starter |
| `implementation_status` | implemented |
| `software_status` | canonical_c_provider_source_and_installed_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verification/external_provider/CMakeLists.txt;cmake/package/verification/external_provider/README.md;cmake/package/verification/external_provider/assembly.toml;cmake/package/verification/external_provider/board/board.json;cmake/package/verification/external_provider/consumer.c;cmake/package/verification/external_provider/lifecycle_test.cpp;cmake/package/verification/external_provider/provider/nexus-soc.json;cmake/package/verification/external_provider/provider/configure/__init__.py;cmake/package/verification/external_provider/provider/configure/bindings.py;cmake/package/verification/external_provider/provider/configure/contract.py;cmake/package/verification/external_provider/provider/configure/emission.py;cmake/package/verification/external_provider/provider/include/nexus_reference/lifecycle_probe.h;cmake/package/verification/external_provider/provider/soc/CMakeLists.txt;cmake/package/verification/external_provider/provider/soc/lifecycle_adapter.c;cmake/package/verification/external_provider/provider/soc/lifecycle_adapter.h;cmake/package/verification/external_provider/provider/soc/routes.json;cmake/package/verification/external_provider/provider/soc/soc.json;tools/testing/test_provider_sdk_starter.py;tools/configure/test_external_package.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | SDK exports one copyable C11 provider with complete facts configuration CMake and retained cleanup; source tests consume the same original. |
| `owner` | codex |

(status-developer-experience-ep06)=
## EP06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP06 |
| `task` | P1: Project SDK external fixture closure from verified manifest |
| `implementation_status` | implemented |
| `software_status` | manifest_fixture_closure_and_sdk_host_inputs_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/package_source_sdk.py;cmake/package/verify_consumers.py;tools/testing/test_provider_sdk_starter.py;tests/contracts/test_source_sdk.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Required files copy and comparison share verified package inputs; CMake owns compile selection; independent missing-file and rehashed package tests remain. |
| `owner` | codex |

(status-developer-experience-ep07)=
## EP07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP07 |
| `task` | P1: Execute source and relocated installed provider consumers |
| `implementation_status` | implemented |
| `software_status` | current_source_and_installed_provider_consumers_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verification/external_provider/consumer.c;cmake/package/verification/external_provider/lifecycle_test.cpp;cmake/package/verify_consumers.py;tools/release/transport.py;tools/configure/test_external_package.py;tools/testing/test_provider_sdk_starter.py;.github/workflows/test_release.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Canonical Native lifecycle runs in source and installed SDK; reviewed MCU ARM consumers link; rollback withdrawal recovery and header boundaries checked without physical claims. |
| `owner` | codex |

(status-developer-experience-ep08)=
## EP08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP08 |
| `task` | P1: Document one builtin or external provider onboarding path |
| `implementation_status` | implemented |
| `software_status` | provider_onboarding_commands_and_strict_docs_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/guides/provider-porting.md;cmake/package/verification/external_provider/README.md;docs/tutorials/source-sdk.md;tools/release/README.md;docs/status/software-verification.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Commands progress from starter through manifest init CMake SDK and tests; external users need no builtin map edits; trusted host-code boundary explicit. |
| `owner` | codex |

(status-developer-experience-ep09)=
## EP09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EP09 |
| `task` | P2: Remove unrelated builtin behavior from external final generator identity |
| `implementation_status` | implemented |
| `software_status` | selected_external_identity_and_sdk_closure_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/dependencies.py;tools/configure/providers/descriptor.py;tools/configure/integration_package.py;tools/configure/test_provider_manifest.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Unselected builtin changes do not invalidate external final identity; common map selected package and public tool changes still invalidate; initial snapshot and complete SDK verification remain. |
| `owner` | codex |

(status-developer-experience-ev01)=
## EV01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV01 |
| `task` | P1: Expose exact assembly-level execution leaves in existing records |
| `implementation_status` | implemented |
| `software_status` | exact_assembly_execution_leaves_associated |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py;tools/maintenance/support.py;tools/testing/test_support_index.py;docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Concrete model image and consumer records bind source resolved dependencies artifacts and raw reports; missing historical identity is never inherited or invented. |
| `owner` | codex |

(status-developer-experience-ev02)=
## EV02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV02 |
| `task` | P1: Associate independently verified stage evidence |
| `implementation_status` | implemented |
| `software_status` | independent_stage_scope_validation_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/support.py;tools/maintenance/capabilities.py;tools/evidence/proof.py;tools/evidence/native_proof.py;tools/testing/test_support_stages.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Model ARM SDK and HIL use existing validators; one scope cannot promote another or manufacture a complete plan from partial runs. |
| `owner` | codex |

(status-developer-experience-ev03)=
## EV03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV03 |
| `task` | P0: Revalidate actual scope execution semantics |
| `implementation_status` | implemented |
| `software_status` | strict_scope_and_explicit_read_only_legacy_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/proof.py;tools/evidence/native_proof.py;tools/evidence/candidate.py;tools/evidence/legacy_sdk.py;tests/contracts/test_evidence_scope_proof.py;tests/contracts/test_evidence_candidate.py;tests/contracts/test_evidence_legacy_sdk.py;tools/testing/test_reproduction_consumer.py;tools/testing/test_reproduction_boundary.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | New sealing rejects arbitrary success empty filtered skipped interrupted or wrong-checker scopes; frozen legacy evidence has an explicit read-only replay boundary. |
| `owner` | codex |

(status-developer-experience-ev04)=
## EV04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV04 |
| `task` | P1: Separate SDK package integrity consumer execution and full matrix |
| `implementation_status` | implemented |
| `software_status` | sdk_package_selected_consumer_and_full_plan_separated |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/evidence/proof.py;tools/maintenance/support.py;tools/maintenance/capabilities.py;tools/release/transport.py;tools/testing/test_support_sdk_consumption.py;.github/workflows/test_release.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Each state is independently visible; exact consumer and full plan are verified; selected images never qualify every maintained mode. |
| `owner` | codex |

(status-developer-experience-ev05)=
## EV05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV05 |
| `task` | P0: Expose declaration identity execution and semantic trust separately |
| `implementation_status` | implemented |
| `software_status` | assertion_identity_execution_and_trust_separated |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py;tools/maintenance/support.py;tools/evidence/proof.py;tools/testing/test_support_stages.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Changing reported text or rehashing an envelope cannot improve trust; original status remains a statement distinct from validated execution scope. |
| `owner` | codex |

(status-developer-experience-ev06)=
## EV06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV06 |
| `task` | P1: Explain tool environment and evidence mismatch dimensions |
| `implementation_status` | implemented |
| `software_status` | scope_mismatch_and_admission_tool_relations_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/capabilities.py;tools/maintenance/support.py;tools/evidence/candidate.py;tools/evidence/proof.py;tools/evidence/identity.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Source config dependencies tools SDK artifacts and missing originals have explicit relations; current unobserved tools are not claimed verified. |
| `owner` | codex |

(status-developer-experience-ev07)=
## EV07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV07 |
| `task` | P1: Associate precise hardware evidence through existing HIL validators |
| `implementation_status` | implemented |
| `software_status` | hil_association_tools_verified_physical_not_executed |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/maintenance/support.py;tools/maintenance/capabilities.py;tools/evidence/proof.py;tools/hil/admit.py;tools/hil/measure.py;tools/testing/test_support_stages.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Station PCB chip fixture image workload and measurement scope retain freshness rules; software fixtures never become physical execution; absent physical raw stays not_executed. |
| `owner` | codex |

(status-developer-experience-ev08)=
## EV08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV08 |
| `task` | P1: Provide one readable development support command |
| `implementation_status` | implemented |
| `software_status` | read_only_support_command_and_sdk_inventory_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/dev.py;tools/maintenance/capabilities.py;tools/maintenance/support.py;tools/configure/configure.py;tools/configure/integration_package.py;tools/evidence/identity.py;tools/testing/test_support_stages.py;tools/testing/test_support_sdk_consumption.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | One read-only command shares human and JSON projections with per-stage source scope trust relation and next missing evidence; no implicit build network or equipment use. |
| `owner` | codex |

(status-developer-experience-ev09)=
## EV09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | EV09 |
| `task` | P1: Prevent cross-selection evidence contamination end to end |
| `implementation_status` | implemented |
| `software_status` | exact_selection_and_real_sdk_contamination_rejection_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/testing/test_support_evidence.py;tools/testing/test_support_index.py;tools/testing/test_support_stages.py;tools/testing/test_support_sdk_consumption.py;tests/contracts/test_evidence_scope_proof.py;tools/evidence/proof.py;tools/evidence/identity.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Real validator composition rejects cross Board route backend clock cache security controller source SDK ELF and consumer mismatches; unchanged existing regressions remain. |
| `owner` | codex |

(status-developer-experience-mx01)=
## MX01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | MX01 |
| `task` | P1: Measure onboarding phases with exact cold and warm inputs |
| `implementation_status` | implemented |
| `software_status` | current_onboarding_phases_measured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/dev/timings.py;tools/dev/process.py;tools/configure/configure.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Record actual resolver manifest generation configure build test and report durations with exact inputs; observations are not qualification. |
| `owner` | codex |

(status-developer-experience-mx02)=
## MX02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | MX02 |
| `task` | P2: Compare actual raw and strong factory image costs |
| `implementation_status` | implemented |
| `software_status` | current_raw_strong_elf_costs_measured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/resources.py;tools/measurement/elf.py;tools/configure/factory.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Same assembly compiler and optimization compare ELF map and symbol costs; no added persistent RAM; Flash and Og/O0 differences disclosed. |
| `owner` | codex |

(status-developer-experience-mx03)=
## MX03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | MX03 |
| `task` | P2: Measure actual instance Board and provider integration edit surfaces |
| `implementation_status` | implemented |
| `software_status` | integration_edit_surfaces_measured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/integration_package.py;tools/configure/dependencies.py;tools/configure/experience.py;tools/configure/factory.py;docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Report changed files public versus private behavior and repeated decisions for explicit integration cases; code size does not substitute for correctness. |
| `owner` | codex |

(status-developer-experience-mx04)=
## MX04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | MX04 |
| `task` | P2: Measure empty single and multiple static instance costs |
| `implementation_status` | implemented |
| `software_status` | current_static_instance_costs_and_pruning_measured |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/measurement/resources.py;tools/measurement/elf.py;tests/contracts/native_assembly/multi.toml;tests/contracts/native_assembly/multi_board/board.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Exact selected-instance cohorts report loaded Flash readonly data static RAM MSP and heap; unused-provider pruning checked without invented performance results. |
| `owner` | codex |

(status-developer-experience-dx01)=
## DX01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DX01 |
| `task` | P1: Retain real TDD order for changed behavior |
| `implementation_status` | implemented |
| `software_status` | red_green_handoffs_reviewed_with_declared_historical_gap |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/testing/test_dev_projects.py;tools/testing/test_project_consumption.py;tools/testing/test_sdk_factory_strong.py;tools/configure/test_factory_includes.py;tools/configure/test_provider_manifest.py;tools/configure/test_provider_authoring.py;tools/testing/test_selected_provider_contract.py;tools/testing/test_provider_sdk_starter.py;tests/contracts/test_evidence_scope_proof.py;tests/contracts/test_evidence_candidate.py;tests/contracts/test_evidence_legacy_sdk.py;tools/testing/test_support_stages.py;tools/testing/test_support_sdk_consumption.py;cmake/package/verification/external_provider/lifecycle_test.cpp;tools/testing/test_reproduction_boundary.py;.github/workflows/test_release.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Each changed behavior or defect has contemporaneous meaningful RED then GREEN; equivalent refactors preserve GREEN and no manufactured historical failures. |
| `owner` | codex |

(status-developer-experience-dx02)=
## DX02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DX02 |
| `task` | P1: Keep independent coverage while avoiding duplicate fixtures |
| `implementation_status` | implemented |
| `software_status` | canonical_source_and_independent_coverage_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | cmake/package/verification/external_provider/CMakeLists.txt;cmake/package/verification/external_provider/README.md;cmake/package/verification/external_provider/assembly.toml;cmake/package/verification/external_provider/board/board.json;cmake/package/verification/external_provider/consumer.c;cmake/package/verification/external_provider/lifecycle_test.cpp;cmake/package/verification/external_provider/provider/configure/__init__.py;cmake/package/verification/external_provider/provider/configure/bindings.py;cmake/package/verification/external_provider/provider/configure/contract.py;cmake/package/verification/external_provider/provider/configure/emission.py;cmake/package/verification/external_provider/provider/include/nexus_reference/lifecycle_probe.h;cmake/package/verification/external_provider/provider/nexus-soc.json;cmake/package/verification/external_provider/provider/soc/CMakeLists.txt;cmake/package/verification/external_provider/provider/soc/lifecycle_adapter.c;cmake/package/verification/external_provider/provider/soc/lifecycle_adapter.h;cmake/package/verification/external_provider/provider/soc/routes.json;cmake/package/verification/external_provider/provider/soc/soc.json;tests/contracts/external_provider_lifecycle/README.md;tools/configure/test_external_package.py;tools/testing/test_provider_sdk_starter.py;tools/testing/test_support_sdk_consumption.py;tests/contracts/test_evidence_scope_proof.py;tools/testing/test_reproduction_boundary.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | One canonical reference feeds appropriate Python and real C tests; independent negative oracles remain; no repeated unchanged tool domain per Native mode. |
| `owner` | codex |

(status-developer-experience-dx03)=
## DX03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DX03 |
| `task` | P1: Complete bounded checks and ordinary full-index commit gate |
| `implementation_status` | implemented |
| `software_status` | ordinary_full_index_gate_passed |
| `hardware_status` | not_applicable |
| `implementation_paths` | scripts/ci/tdd_gate.py;tools/dev/process.py;scripts/ci/style_gate.py;tools/testing/test_reproduction_boundary.py;.github/workflows/test_release.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Focused failures stop early; final source freeze ordinary hooks full tools and Google execute fresh with bounded jobs and no skipped reports or hook bypass. |
| `owner` | codex |

(status-developer-experience-dx04)=
## DX04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | DX04 |
| `task` | P1: Synchronize existing bilingual contracts navigation and diagrams |
| `implementation_status` | implemented |
| `software_status` | strict_bilingual_docs_and_diagrams_verified |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/explanation/factory.md;docs/guides/provider-porting.md;docs/delivery/capabilities.md;docs/reference/configuration/assembly.md;docs/tutorials/source-sdk.md;docs/design/developer-experience.md;docs/design/README.md;docs/design/source-compatibility.md;docs/status/ledgers/developer-experience.md;docs/assets/diagrams/factory-instances.dot;docs/assets/diagrams/factory-instances.svg;docs/assets/diagrams/labels.json;docs/assets/diagrams/diagram-index.csv;docs/assets/diagrams/localized/en/factory-instances.dot;docs/assets/diagrams/localized/en/factory-instances.svg;docs/assets/diagrams/localized/zh_CN/factory-instances.dot;docs/assets/diagrams/localized/zh_CN/factory-instances.svg;tools/documentation/status.py;tools/release/README.md;docs/status/software-verification.md;docs/locale/en/LC_MESSAGES/delivery/capabilities.po;docs/locale/en/LC_MESSAGES/design/README.po;docs/locale/en/LC_MESSAGES/design/source-compatibility.po;docs/locale/en/LC_MESSAGES/status/ledgers/developer-experience.po;docs/locale/en/LC_MESSAGES/status/software-verification.po;docs/locale/zh_CN/LC_MESSAGES/delivery/capabilities.po;docs/locale/zh_CN/LC_MESSAGES/design/README.po;docs/locale/zh_CN/LC_MESSAGES/design/developer-experience.po;docs/locale/zh_CN/LC_MESSAGES/design/source-compatibility.po;docs/locale/zh_CN/LC_MESSAGES/explanation/factory.po;docs/locale/zh_CN/LC_MESSAGES/guides/provider-porting.po;docs/locale/zh_CN/LC_MESSAGES/reference/configuration/assembly.po;docs/locale/zh_CN/LC_MESSAGES/status/ledgers/developer-experience.po;docs/locale/zh_CN/LC_MESSAGES/status/software-verification.po;docs/locale/zh_CN/LC_MESSAGES/tutorials/source-sdk.po |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Existing pages and diagrams describe exact current contracts in both locales; commands pass validation; no parallel status authority or inherited firmware qualification. |
| `owner` | codex |

(status-developer-experience-qb01)=
## QB01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB01 |
| `task` | P0: Freeze schematic identity and reviewed differences |
| `implementation_status` | implemented |
| `software_status` | schematic_and_source_reviewed_no_new_runtime_execution |
| `hardware_status` | not_applicable |
| `implementation_paths` | boards |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Record both PDF hashes and page/component/net references; schematic facts never establish fitted hardware or PCB inspection. |
| `owner` | codex |

(status-developer-experience-qb02)=
## QB02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB02 |
| `task` | P0: Introduce two independent exact Qiming Board packages |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_core/board.json;boards/stm32f407zg_qiming_v41/board.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB01. Core and high-configuration V4.1 resolve independently; original V3.1 bytes and identity remain unchanged; PCB revision remains unknown until inspected. |
| `owner` | codex |

(status-developer-experience-qb03)=
## QB03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB03 |
| `task` | P0: Model population jumper and electrical selections |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/configure/board_hardware.py;tools/configure/gpio_electrical.py;boards/stm32f407zg_qiming_core/board.json;boards/stm32f407zg_qiming_v41/board.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB01. No invented defaults; absent or unknown devices and unmet jumper conditions reject selected bindings; declarations do not establish physical proof. |
| `owner` | codex |

(status-developer-experience-qb04)=
## QB04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB04 |
| `task` | P0: Bind hardware selections and PDF provenance into typed configuration |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_applicable |
| `implementation_paths` | tools/configure/ir.py;tools/configure/authored.py;tools/configure/experience.py;tools/configure/dependencies.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB02;QB03. Unknown keys and choices reject; immutable selections survive explain and source SDK consumption; one configuration authority and static emission remain. |
| `owner` | codex |

(status-developer-experience-qb05)=
## QB05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB05 |
| `task` | P0: Implement actual GPIO routes and electrical modes |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/drivers/gpio.c;soc/stm32/stm32f407/private/stm32f407_basic_cold.h;tools/configure/gpio_electrical.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04. Exact package ports and pins support input pulls and output drive; inactive levels precede output mode; failed setup and stop preserve ownership. |
| `owner` | codex |

(status-developer-experience-qb06)=
## QB06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB06 |
| `task` | P0: Extend EXTI lines and real vector ownership |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | io/include/nexus/io/exti.h;io/src/api/exti.c;soc/stm32/stm32f407/drivers/exti.c;soc/stm32/stm32f407/private/stm32f407_control_cold.h |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB05. Selected lines including PF11 use the correct vector; shared owners stop independently; event overflow and rejected initialization remain observable. |
| `owner` | codex |

(status-developer-experience-qb07)=
## QB07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB07 |
| `task` | P0: Review six UART instances and selected transport modes |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/drivers/uart_cold.c;soc/stm32/stm32f407/configure/hardware.py;soc/stm32/stm32f407/configure/bindings.py;tests/contracts/qiming_uart_assembly |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04. USART1/2/3/6 and UART4/5 have exact clock IRQ gate and pad admission; existing modes remain; no unimplemented DMA mode is advertised. |
| `owner` | codex |

(status-developer-experience-qb08)=
## QB08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB08 |
| `task` | P0: Add actual SPI and I2C routes without capability drift |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/routes.json;soc/stm32/stm32f407/drivers/spi.c;soc/stm32/stm32f407/drivers/i2c_cold.c |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04. SPI1 PB3/PB4/PB5 and reviewed I2C1 alternatives match real C admission; preserve SWD and reject conflicting complete JTAG ownership. |
| `owner` | codex |

(status-developer-experience-qb09)=
## QB09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB09 |
| `task` | P0: Deliver safe minimal Board startup and shutdown |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_core/assemblies/minimal.toml;boards/stm32f407zg_qiming_v41/assemblies/minimal.toml;soc/stm32/stm32f407/drivers/platform_cold.c |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB05;QB07. Correct two-versus-three LEDs and two-versus-four keys; UART electrical route and CS DE backlight levels are explicit; complete cleanup is proved or retained. |
| `owner` | codex |

(status-developer-experience-qb10)=
## QB10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB10 |
| `task` | P1: Add an AT24C02 device component |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/at24c02;tests/google/at24c02_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB08. Address 0x50 geometry page boundaries write-cycle ACK progress and failures execute against the actual device transport; product storage layout stays external. |
| `owner` | codex |

(status-developer-experience-qb11)=
## QB11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB11 |
| `task` | P1: Add a selected W25 SPI NOR component |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/w25;tests/google/w25_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB08;QB03. Expected JEDEC identity and geometry are checked; finite program erase busy and error paths preserve ownership; core unpopulated NOR stays disabled. |
| `owner` | codex |

(status-developer-experience-qb12)=
## QB12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB12 |
| `task` | P1: Bind both Boards actual CAN routes and electrical conditions |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_core/assemblies/can-loopback.toml;boards/stm32f407zg_qiming_v41/assemblies/can-loopback.toml;soc/stm32/stm32f407/configure/can.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04;QB09. Core CAN1 PB8/PB9 and high-configuration PA11/PA12 are distinct; CAN2 jumper and shared-filter ownership are reviewed; no transceiver or termination is invented. |
| `owner` | codex |

(status-developer-experience-qb13)=
## QB13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB13 |
| `task` | P1: Compose exact RS232 and RS485 paths |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_rs232_and_electrical_verification_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/configure/factory.py;io/include/nexus/io/uart.h;io/include/nexus/io/uart_rs485.h;io/src/mechanisms/uart_rs485.c;boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml;tests/contracts/qiming_rs485_assembly;tests/google/qiming_rs485_factory_test.cpp |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB07;QB09;QB03. Declared transceiver supply and P15/PG6 direction are checked; true wire-tail and cleanup execute; standard PA2 Ethernet/RS485 conflict rejects. |
| `owner` | codex |

(status-developer-experience-qb14)=
## QB14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB14 |
| `task` | P1: Bind actual USB full-speed paths |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_core/assemblies/usb-device.toml;boards/stm32f407zg_qiming_v41/assemblies/usb-device.toml;soc/stm32/stm32f407/drivers/usb_device.c |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB03;QB09. P21 USB/CAN selection and VBUS/ID facts remain explicit; OTG_HS internal FS and external ULPI HS are not confused; host support is not inferred. |
| `owner` | codex |

(status-developer-experience-qb15)=
## QB15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB15 |
| `task` | P1: Compose V4.1 MAC DP83848 and network transport |
| `implementation_status` | blocked |
| `software_status` | v41_phy_facts_unverified_no_admitted_ethernet_route |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_v41/board.json;components/dp83848 |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB03;QB04;QB09. Exact PHY identity address reference clock P5 and receive-valid variant are reviewed; application network policy stays external; unknown wiring is not repaired silently. |
| `owner` | codex |

(status-developer-experience-qb16)=
## QB16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB16 |
| `task` | P1: Integrate F407 RTC LSE and backup-domain lifecycle |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/drivers/rtc.c;soc/stm32/stm32f407/configure/rtc.py;boards/stm32f407zg_qiming_core/assemblies/rtc-adopt.toml;boards/stm32f407zg_qiming_v41/assemblies/rtc-adopt.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04;QB09. Calendar and time-domain boundaries execute; LSE failure and retained backup state are explicit; no battery fitting or measured retention is inferred. |
| `owner` | codex |

(status-developer-experience-qb17)=
## QB17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB17 |
| `task` | P2: Implement one shared FSMC owner with independent banks |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | io/include/nexus/io/external_bus.h;io/include/nexus/io/parallel16.h;io/include/nexus/io/memory_window.h;soc/stm32/stm32f407/drivers/fsmc.c;soc/stm32/stm32f407/drivers/fsmc_boot.c;soc/stm32/stm32f407/configure/fsmc.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04;QB05. Shared pads and gate have one owner; NE3 SRAM and NE4 LCD own distinct windows and timing; rollback and close never restore another live bank. |
| `owner` | codex |

(status-developer-experience-qb18)=
## QB18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB18 |
| `task` | P2: Implement SRAM windows and optional pre-runtime initialization |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/sram_boot.c;soc/stm32/stm32f407/memory_init.c;soc/stm32/stm32f407/system.c;soc/stm32/stm32f407/configure/sram.py;boards/stm32f407zg_qiming_core/assemblies/sram-startup.toml;boards/stm32f407zg_qiming_v41/assemblies/sram-startup.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB17;QB03. Internal bootstrap initializes selected FSMC before external data/bss; CPU and DMA reachability are separate; boot-backed memory prevents unsafe withdrawal. |
| `owner` | codex |

(status-developer-experience-qb19)=
## QB19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB19 |
| `task` | P2: Integrate the confirmed ILI9341 16-bit display |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/ili9341;boards/stm32f407zg_qiming_core/assemblies/display-touch.toml;boards/stm32f407zg_qiming_v41/assemblies/display-touch.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB17;QB18. Actual command/data address shift and controller identity are checked; 240x320 RGB565 rectangle and error paths execute; shared NRST is not an invented reset GPIO. |
| `owner` | codex |

(status-developer-experience-qb20)=
## QB20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB20 |
| `task` | P2: Integrate confirmed XPT2046 through bounded software SPI |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/software_spi;components/xpt2046;boards/stm32f407zg_qiming_core/assemblies/touch-nor.toml;boards/stm32f407zg_qiming_v41/assemblies/touch-nor.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB05;QB06. PA5/PB1/PB2/PB0/PF11 route and mode are explicit; raw 12-bit sampling and final PENIRQ power state execute; uncertain pin cleanup retains bus ownership. |
| `owner` | codex |

(status-developer-experience-qb21)=
## QB21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB21 |
| `task` | P2: Implement block I/O and F407 SDIO host ownership |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_polling_only_no_dma_irq_cancel_hot_swap |
| `hardware_status` | not_executed |
| `implementation_paths` | io/include/nexus/io/block.h;io/src/api/block.c;soc/stm32/stm32f407/drivers/sdio.c;soc/stm32/stm32f407/configure/block.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04. PC8-PC12/PD2 command and data paths handle CRC timeout cancellation and removal; no unconnected card-detect GPIO is invented; buffers end only after detach. |
| `owner` | codex |

(status-developer-experience-qb22)=
## QB22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB22 |
| `task` | P2: Integrate SD card protocol and optional filesystem port |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | soc/stm32/stm32f407/drivers/sdio.c;boards/stm32f407zg_qiming_v41/assemblies/sdio.toml;tests/contracts/qiming_sdio_assembly |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB21. Selected card geometry and bounded block reads/writes execute; optional filesystem policy stays distinct; product directories and persistence remain external. |
| `owner` | codex |

(status-developer-experience-qb23)=
## QB23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB23 |
| `task` | P2: Implement bounded software two-wire and SCCB ports |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/software_i2c;boards/stm32f407zg_qiming_v41/assemblies/control-wires.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB05. Actual PC0/PC13 and PD6/PD7 paths use release/read semantics with explicit timing and ACK/SCCB differences; no hidden task or long global IRQ mask. |
| `owner` | codex |

(status-developer-experience-qb24)=
## QB24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB24 |
| `task` | P2: Integrate selected WM8978 audio and complete I2S mode |
| `implementation_status` | blocked |
| `software_status` | module_selection_and_complete_driver_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_v41/board.json;boards/stm32f407zg_qiming_v41/assemblies/control-wires.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB23;QB04. Actual module must be selected; PB12/PB13/PC2/PC3/PC6 clock sample-rate RX/TX DMA and full withdrawal execute; finite slave TX is not full audio qualification. |
| `owner` | codex |

(status-developer-experience-qb25)=
## QB25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB25 |
| `task` | P2: Integrate the alternative selected VS1053 module |
| `implementation_status` | blocked |
| `software_status` | module_selection_and_complete_driver_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_v41/board.json;boards/stm32f407zg_qiming_v41/assemblies/control-wires.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB07;QB08. Actual module must be selected; SPI2 alternate route dual CS DREQ and reset execute; WM8978 and VS1053 cannot own the same connector simultaneously. |
| `owner` | codex |

(status-developer-experience-qb26)=
## QB26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB26 |
| `task` | P2: Integrate DCMI and an exact OV2640 capture configuration |
| `implementation_status` | blocked |
| `software_status` | module_selection_and_complete_driver_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_v41/board.json;boards/stm32f407zg_qiming_v41/assemblies/control-wires.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB18;QB23;QB04. Actual module format and rate are fixed; XCLK SCCB frame boundaries malformed frames and DMA withdrawal execute with declared static memory and bandwidth budgets. |
| `owner` | codex |

(status-developer-experience-qb27)=
## QB27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB27 |
| `task` | P2: Integrate selected DS18B20 and HS0038 devices |
| `implementation_status` | implemented |
| `software_status` | software_matrix_verified_source_A |
| `hardware_status` | not_executed |
| `implementation_paths` | components/onewire;components/ds18b20;components/infrared;boards/stm32f407zg_qiming_v41/assemblies/sensor-wires.toml |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB05. OneWire only pulls low or releases and declares any strong-pullup requirement; infrared timing overflow and selected protocol execute without product calibration policy. |
| `owner` | codex |

(status-developer-experience-qb28)=
## QB28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB28 |
| `task` | P0: Validate clock DMA IRQ and memory combination budgets |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_resource_guards_audio_video_admission_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/configure/resource_graph.py;tools/configure/memory.py;tools/configure/dma_topology.py;soc/stm32/stm32f407/routes.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB04. Physical DMA Stream exclusivity cannot be bypassed by different channel numbers; TIM2 stays reserved; audio/video buffer reach and bandwidth are explicit. |
| `owner` | codex |

(status-developer-experience-qb29)=
## QB29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB29 |
| `task` | P2: Deliver legal combinations and external application examples |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_minimal_private_companion_verified_media_network_exit_pending |
| `hardware_status` | not_executed |
| `implementation_paths` | boards/stm32f407zg_qiming_core/assemblies;boards/stm32f407zg_qiming_v41/assemblies;tests/contracts/qiming_devices_assembly;tests/contracts/qiming_sdio_assembly |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB09;QB10;QB11;QB12;QB13;QB14;QB15;QB16;QB18;QB19;QB20;QB22;QB24;QB25;QB26;QB27;QB28. Minimal serial CAN network storage HMI and selected media profiles consume public interfaces; impossible standard wiring rejects; private application evidence stays private. |
| `owner` | codex |

(status-developer-experience-qb30)=
## QB30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB30 |
| `task` | P0: Execute genuine TDD and final source-bound software matrix |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_strict_tdd_chronology_and_snapshot_exceptions_open |
| `hardware_status` | not_applicable |
| `implementation_paths` | tests/google;tools/configure;scripts/ci/tdd_gate.py;docs/design/developer-experience.md |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB01. Meaningful original RED precedes new behavior then GREEN; actual ARM and installed SDK images and resource costs bind exact inputs; no existence-only or stale-pass substitution. |
| `owner` | codex |

(status-developer-experience-qb31)=
## QB31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB31 |
| `task` | P2: Deliver bilingual Board guidance and evidence navigation |
| `implementation_status` | implemented |
| `software_status` | strict_en_zh_passed_source_B2 |
| `hardware_status` | not_applicable |
| `implementation_paths` | docs/design/developer-experience.md;docs/design/developer-experience-execution.csv;docs/status/ledgers/developer-experience.md;docs/locale/zh_CN/LC_MESSAGES/design/developer-experience.po;docs/delivery/developer-experience-results.json |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB02;QB29;QB30. One existing ledger records exact mode source schematic and declared population identities with actual software results and unexecuted physical scope; both locales stay reviewed. |
| `owner` | codex |

(status-developer-experience-qb32)=
## QB32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | QB32 |
| `task` | P3: Prepare and execute exact Board HIL admission |
| `implementation_status` | partial |
| `software_status` | software_matrix_verified_source_A_hil_preparation_46_tests_passed_physical_not_executed |
| `hardware_status` | not_executed |
| `implementation_paths` | tools/hil/prepare.py;tools/hil/admit.py;tools/hil/fixtures/stm32f407zg_qiming_core.json;tools/hil/fixtures/stm32f407zg_qiming_v41.json;tools/schemas/hil-fixture.schema.json;tools/testing/test_hil_qiming.py |
| `evidence_paths` | docs/delivery/developer-experience-results.json |
| `exit_condition` | Depends on QB09;QB29;QB30;QB31. Station binds PCB BOM jumper profile and fact digests; real startup clock serial buses storage USB network and failed-stop behavior execute; disconnected hardware remains not_executed. |
| `owner` | codex |
