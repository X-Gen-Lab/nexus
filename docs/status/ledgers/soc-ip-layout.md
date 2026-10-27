---
id: status.ledgers.soc-ip-layout
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-soc-ip-layout)=
# SoC/IP 目录整理台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/soc-ip-layout-execution.csv)。
源文件：`docs/design/soc-ip-layout-execution.csv`；SHA-256：`0564d12a1125e718b7560defc8a7cbb184c77ebeb1b13ba1eea1331b2cac8fa4`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 48 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-soc-ip-layout-r01)=
## R01

| 原字段 | 原值 |
| --- | --- |
| `id` | R01 |
| `work_package` | baseline |
| `requirement` | Branch and source baseline |
| `exit_condition` | Record actual source revision and branch before edits |
| `implementation_paths` | AGENTS.md |
| `implementation_status` | implemented |
| `software_status` | reviewed |
| `hardware_status` | not_executed |
| `evidence_paths` | .git/local-recovery/soc-ip-layout/baseline.json |
| `notes` | Local and GitHub refactor/soc-ip-layout branches start at 890a6026384b. |

(status-soc-ip-layout-r02)=
## R02

| 原字段 | 原值 |
| --- | --- |
| `id` | R02 |
| `work_package` | baseline |
| `requirement` | Per-file responsibility and destination map |
| `exit_condition` | Review every moved file and distinguish shared IP from SoC integration |
| `implementation_paths` | soc;drivers;tools/configure |
| `implementation_status` | implemented |
| `software_status` | reviewed |
| `hardware_status` | not_executed |
| `evidence_paths` | .git/local-recovery/soc-ip-layout/path-map.json |
| `notes` | Shared IP and 35 family production modules reviewed; import-normalized family AST unchanged. These are original migration-baseline identities and counts, not current P totals; final execution is recorded in R43-R48. |

(status-soc-ip-layout-r03)=
## R03

| 原字段 | 原值 |
| --- | --- |
| `id` | R03 |
| `work_package` | baseline |
| `requirement` | Exact platform mode resource and rejection baseline |
| `exit_condition` | Record maintained selections and rejection behavior without expanding support |
| `implementation_paths` | tools/configure;tests/contracts |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/baseline/configurations |
| `notes` | 136 exact assemblies including all 71 catalogue entries resolved in both source trees; no effective decision changes. Existing negative contracts remain enforced. These are original migration-baseline identities and counts, not current P totals; final execution is recorded in R43-R48. |

(status-soc-ip-layout-r04)=
## R04

| 原字段 | 原值 |
| --- | --- |
| `id` | R04 |
| `work_package` | baseline |
| `requirement` | Test identities and execution domains |
| `exit_condition` | Map retained cases to Native; tool; ARM and physical domains |
| `implementation_paths` | tests;tools/configure/tests;tools/testing |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration/case-mapping.json |
| `notes` | Original 1035 configure cases retained one-to-one; 132 family cases relocated; 8 migration regressions added. These are original migration-baseline identities and counts, not current P totals; final execution is recorded in R43-R48. |

(status-soc-ip-layout-r05)=
## R05

| 原字段 | 原值 |
| --- | --- |
| `id` | R05 |
| `work_package` | baseline |
| `requirement` | Flash RAM ops and IRQ baseline |
| `exit_condition` | Record actual linked resource and retention baseline for comparisons |
| `implementation_paths` | tools/measurement;tests/contracts |
| `implementation_status` | implemented |
| `software_status` | reviewed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/baseline/resources |
| `notes` | 32 prior resource reports from 16 presets retained with matching ELF hash/size. Historical baseline capture; not new execution or new-source resource qualification. Only resource reports were copied; ELF/map identities were observed at capture, not retained as new build evidence. Final P comparisons are recorded in R46. |

(status-soc-ip-layout-r06)=
## R06

| 原字段 | 原值 |
| --- | --- |
| `id` | R06 |
| `work_package` | baseline |
| `requirement` | Single execution ledger |
| `exit_condition` | Maintain exactly R01-R48 with implementation and execution separated |
| `implementation_paths` | docs/design/soc-ip-layout-execution.csv |
| `implementation_status` | implemented |
| `software_status` | reviewed |
| `hardware_status` | not_executed |
| `evidence_paths` | docs/design/soc-ip-layout-execution.csv |
| `notes` | One R01-R48 task record; implementation, software and physical status stay separate. Final software tested_source=P=92e669233957a2e450a528394e8f4dd0794c7049, tree=6e38d4c43d59330a0a9a9b7cac21f591d9c2719f; integrated M=c79fd9672a9fe56379744d834cfccd822bc40be1 has the same tree. This documentation update records P and does not claim its own software qualification. |

(status-soc-ip-layout-r07)=
## R07

| 原字段 | 原值 |
| --- | --- |
| `id` | R07 |
| `work_package` | infrastructure |
| `requirement` | Explicit family descriptors |
| `exit_condition` | Declare canonical source directories and configuration modules without discovery |
| `implementation_paths` | tools/configure/providers/descriptor.py |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration |
| `notes` | Explicit vendor/source/configuration directories; no family-prefix path inference. |

(status-soc-ip-layout-r08)=
## R08

| 原字段 | 原值 |
| --- | --- |
| `id` | R08 |
| `work_package` | infrastructure |
| `requirement` | Module loading and type identity |
| `exit_condition` | Explicit loading preserves common Python type identity and import boundaries |
| `implementation_paths` | tools/configure/providers;soc |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration |
| `notes` | Normal selected SoC packages and canonical shared types; six actual RED/GREEN loading regressions. |

(status-soc-ip-layout-r09)=
## R09

| 原字段 | 原值 |
| --- | --- |
| `id` | R09 |
| `work_package` | infrastructure |
| `requirement` | Input closure and freshness |
| `exit_condition` | Moved configuration dependencies remain in identity and stale-input rejection |
| `implementation_paths` | tools/configure/dependencies.py;tools/configure/bundle.py |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration |
| `notes` | All executing package initializers/helpers hashed; migrated sources protected from owned output overwrite. |

(status-soc-ip-layout-r10)=
## R10

| 原字段 | 原值 |
| --- | --- |
| `id` | R10 |
| `work_package` | infrastructure |
| `requirement` | SDK roots and required members |
| `exit_condition` | Source SDK includes declared SoC configuration and IP driver inputs |
| `implementation_paths` | cmake/package |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/sdk-driver-red.log;build/soc-ip-layout/sdk-driver-green.log;build/soc-ip-layout/sdk-payload.log;build/soc-ip-layout/sdk-selection.log |
| `notes` | New shared driver transport regression RED then GREEN; 20 device payload and 45 selection/portable tests pass. Actual final P installed consumers are recorded in R45. |

(status-soc-ip-layout-r11)=
## R11

| 原字段 | 原值 |
| --- | --- |
| `id` | R11 |
| `work_package` | infrastructure |
| `requirement` | Family test discovery |
| `exit_condition` | Retain and execute tests under tools/configure/tests/providers/&lt;family&gt; |
| `implementation_paths` | tools/configure/tests/providers |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration/case-mapping.json |
| `notes` | Family tests remain in existing tools/configure discovery domain; no new runner. |

(status-soc-ip-layout-r12)=
## R12

| 原字段 | 原值 |
| --- | --- |
| `id` | R12 |
| `work_package` | infrastructure |
| `requirement` | Style source capability and measurement coverage |
| `exit_condition` | Include drivers and SoC configuration in existing observers and gates |
| `implementation_paths` | tools/maintenance;tools/measurement;scripts/validation |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/baseline/quality-tools-green.log;build/soc-ip-layout/baseline/io-lifecycle-green.log;build/soc-ip-layout/capabilities-green.log |
| `notes` | New drivers remain covered by style, private include, ops/source/retention, static analysis and target-local lifecycle checks; independent review closed all migration findings. |

(status-soc-ip-layout-r13)=
## R13

| 原字段 | 原值 |
| --- | --- |
| `id` | R13 |
| `work_package` | configuration_providers |
| `requirement` | Move Native configuration |
| `exit_condition` | Use soc/native/configure with explicit imports and retained tests |
| `implementation_paths` | soc/native/configure |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r14)=
## R14

| 原字段 | 原值 |
| --- | --- |
| `id` | R14 |
| `work_package` | configuration_providers |
| `requirement` | Move F103 and GD303 configuration |
| `exit_condition` | Use each exact family configure package and retained tests |
| `implementation_paths` | soc/stm32/stm32f103/configure;soc/gd32/gd32f303/configure |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r15)=
## R15

| 原字段 | 原值 |
| --- | --- |
| `id` | R15 |
| `work_package` | configuration_providers |
| `requirement` | Move F407 and GD470 configuration |
| `exit_condition` | Use each exact family configure package and retained tests |
| `implementation_paths` | soc/stm32/stm32f407/configure;soc/gd32/gd32f470/configure |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r16)=
## R16

| 原字段 | 原值 |
| --- | --- |
| `id` | R16 |
| `work_package` | configuration_providers |
| `requirement` | Move G474 configuration and helpers |
| `exit_condition` | Co-locate G474 contract; construction; emission and selected helpers |
| `implementation_paths` | soc/stm32/stm32g474/configure |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r17)=
## R17

| 原字段 | 原值 |
| --- | --- |
| `id` | R17 |
| `work_package` | configuration_providers |
| `requirement` | Move H743 configuration |
| `exit_condition` | Co-locate H743 contract; construction; emission and selected helpers |
| `implementation_paths` | soc/stm32/stm32h743/configure |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r18)=
## R18

| 原字段 | 原值 |
| --- | --- |
| `id` | R18 |
| `work_package` | configuration_providers |
| `requirement` | Move U575 configuration and clear old production paths |
| `exit_condition` | Co-locate U575 policy and remove obsolete family production packages |
| `implementation_paths` | soc/stm32/stm32u575/configure;tools/configure/providers |
| `implementation_status` | implemented |
| `software_status` | phase1_complete_index_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/configuration;.git/local-recovery/soc-ip-layout/sync-first.json;build/soc-ip-layout/commit-gates.log;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Phase-1 exact tree d9270fefe377be519e2fb10d223eed8125566223; canonical commit 7b985c4c619a966c886c3cbf01902ccbfb68c955. Ordinary complete-index gates: 3215 tool tests and 352 programs / 6352 Google cases. This result does not qualify subsequent behavior changes. The first outer hook log and sync receipt retain the historical assertion; reused build/native-debug/tdd bytes are not its original detailed evidence. Final P gates and execution are recorded in R43-R48. |

(status-soc-ip-layout-r19)=
## R19

| 原字段 | 原值 |
| --- | --- |
| `id` | R19 |
| `work_package` | shared_drivers |
| `requirement` | Move UART V1 channel DMA engine |
| `exit_condition` | Select reviewed algorithm and private headers from drivers |
| `implementation_paths` | drivers/uart/uart_v1_channel_dma |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r20)=
## R20

| 原字段 | 原值 |
| --- | --- |
| `id` | R20 |
| `work_package` | shared_drivers |
| `requirement` | Move I2C V1 asynchronous engine |
| `exit_condition` | Select reviewed IRQ/DMA algorithm and private headers from drivers |
| `implementation_paths` | drivers/i2c/i2c_v1_async |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r21)=
## R21

| 原字段 | 原值 |
| --- | --- |
| `id` | R21 |
| `work_package` | shared_drivers |
| `requirement` | Move I2C V2 engine |
| `exit_condition` | Select reviewed polling algorithm and private headers from drivers |
| `implementation_paths` | drivers/i2c/i2c_v2 |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r22)=
## R22

| 原字段 | 原值 |
| --- | --- |
| `id` | R22 |
| `work_package` | shared_drivers |
| `requirement` | Split legacy_devices and move ADC |
| `exit_condition` | Separate private legacy interfaces and select the ADC algorithm from drivers |
| `implementation_paths` | drivers/adc/legacy_adc |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r23)=
## R23

| 原字段 | 原值 |
| --- | --- |
| `id` | R23 |
| `work_package` | shared_drivers |
| `requirement` | Move legacy PWM |
| `exit_condition` | Select fixed PWM algorithm and narrow private contract from drivers |
| `implementation_paths` | drivers/timer/legacy_pwm |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r24)=
## R24

| 原字段 | 原值 |
| --- | --- |
| `id` | R24 |
| `work_package` | shared_drivers |
| `requirement` | Move legacy Flash and SoC helpers |
| `exit_condition` | Select Flash algorithm; place STM32 identity helper; retain UART collaboration and update READMEs |
| `implementation_paths` | drivers/flash/legacy_flash;soc/stm32/common/stm32_identity.h;soc/common/uart_control.h;drivers/README.md;soc/common/README.md |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/native-debug/quality/soc-ip-layout-shared-drivers/;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.xml;build/native-debug/quality/soc-ip-layout-shared-drivers/ctest-results.execution.json |
| `notes` | Shared focused run: 17 targets; 17/17 CTest; 267 Google cases; zero skipped. Aggregate scope is not a per-row test count or complete qualification. Physical HIL not executed. The preserved shared-drivers XML matches the original report hash; its embedded generic Native XML path was later reused. Final P execution is separately recorded in R43-R48. |

(status-soc-ip-layout-r25)=
## R25

| 原字段 | 原值 |
| --- | --- |
| `id` | R25 |
| `work_package` | facts_and_ir |
| `requirement` | Centralize part package RAM and Flash geometry |
| `exit_condition` | Use one exact fact authority for part; package; memory and physical geometry |
| `implementation_paths` | soc;tools/configure |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/part-red.log;build/soc-ip-layout/facts-ir/clock-red.log;build/soc-ip-layout/facts-ir/parts-clocks-green.log;build/soc-ip-layout/facts-ir/f407-flash.xml;build/soc-ip-layout/facts-ir/final-focused.log;build/soc-ip-layout/facts-ir/integration-bundle-green.log;build/soc-ip-layout/facts-ir/final-closure-green.log;build/soc-ip-layout/facts-ir/all-flash-red.log;build/soc-ip-layout/flash-geometry/summary.json |
| `notes` | Exact PartPolicy and optional-memory admission; physical numbers remain in soc.json and frozen PhysicalPartIR views. All seven MCU Flash geometries derive readonly C from validated facts; six retain global symbols and F407 uses selected static geometry. Final focused configuration suite: 364 tests; separate integration/bundle suite: 143 tests. These overlapping scopes are not additive or final-source qualification. |

(status-soc-ip-layout-r26)=
## R26

| 原字段 | 原值 |
| --- | --- |
| `id` | R26 |
| `work_package` | facts_and_ir |
| `requirement` | Typed controller integration record |
| `exit_condition` | Use explicit typed integration data without family-name inference |
| `implementation_paths` | soc;tools/configure/providers/common.py |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/final-focused.log;build/soc-ip-layout/facts-ir/integration-bundle-green.log;build/soc-ip-layout/facts-ir/final-closure-green.log;build/soc-ip-layout/facts-ir/integration-red.log |
| `notes` | ControllerIntegrationIR composes existing canonical claims/DMA/IRQ/clock/pin/hardware objects; PhysicalPartIR reuses frozen facts. No new JSON/hash field or route resolver. Actual production consumers: F407 primary SPI and F407/GD470 paired secondary paths. Other constructors and explicit legacy fixture adapters remain. 364-test focused suite and overlapping 52-test final closure passed; no final-source qualification. |

(status-soc-ip-layout-r27)=
## R27

| 原字段 | 原值 |
| --- | --- |
| `id` | R27 |
| `work_package` | facts_and_ir |
| `requirement` | Clock reset pin and state integration |
| `exit_condition` | Make integration ownership explicit without hidden side effects |
| `implementation_paths` | soc;drivers |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/final-focused.log;build/soc-ip-layout/facts-ir/integration-bundle-green.log |
| `notes` | Exact family clock, GPIO/preflight, ControllerBinding and secondary cold hooks own chip integration. Common resource leases/IRQ/drain ordering remain. Focused configuration and bundle suites passed; no runtime registration or broader clock/Board/backend qualification. |

(status-soc-ip-layout-r28)=
## R28

| 原字段 | 原值 |
| --- | --- |
| `id` | R28 |
| `work_package` | facts_and_ir |
| `requirement` | Consume existing IRQ DMA and memory IR consistently |
| `exit_condition` | Use reviewed immutable IR instead of duplicated derived facts |
| `implementation_paths` | soc;tools/configure/ir.py;tools/configure/memory.py |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/final-focused.log;build/soc-ip-layout/facts-ir/integration-bundle-green.log;build/soc-ip-layout/facts-ir/final-closure-green.log;build/soc-ip-layout/facts-ir/claims-red.log;build/soc-ip-layout/facts-ir/primary-dma-red.log |
| `notes` | Typed owned_claims uses canonical ResourcePlanIR; selected hardware/DMA/IRQ/clock/pin consumers use ControllerIntegrationIR rather than reparsing routes. Meaningful omitted-lane/claim regressions recorded; other maintained typed or explicit legacy adapters remain. Focused suites are not final ARM/SDK qualification. |

(status-soc-ip-layout-r29)=
## R29

| 原字段 | 原值 |
| --- | --- |
| `id` | R29 |
| `work_package` | facts_and_ir |
| `requirement` | Move family special cases out of common tools |
| `exit_condition` | Keep family policy in exact SoC configure packages and common mechanisms shared |
| `implementation_paths` | soc;tools/configure |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/parts-clocks-green.log;build/soc-ip-layout/r30/green-final.log;build/soc-ip-layout/facts-ir/final-focused.log;build/soc-ip-layout/facts-ir/integration-bundle-green.log;build/soc-ip-layout/r29-hardware;build/soc-ip-layout/r29-public-policies;build/soc-ip-layout/facts-ir/final-closure-green.log;build/quality/soc-ip-config-r32/profile-python-green.log |
| `notes` | Explicit SoC hardware/secondary, clock/linker/runtime-power, GPIO/preflight, selected support, UART/stream/endpoint and CAN/coupled/pin policies implemented. Generated-output equivalence checked in separately recorded 18-, 23- and 8-configuration scopes; test counts overlap and must not be added. Remaining common family literals are explicit dependency paths and ThreadX cross-Board/backend OS admission. F103/GD303 frozen LegacyTransportProfile supplies exact wiring without family fallback; its 38-test focused run is separate and overlaps related suites. |

(status-soc-ip-layout-r30)=
## R30

| 原字段 | 原值 |
| --- | --- |
| `id` | R30 |
| `work_package` | facts_and_ir |
| `requirement` | Capability revision and support projection |
| `exit_condition` | Derive current views from exact reviewed facts without new support claims |
| `implementation_paths` | tools/maintenance;docs/delivery/capabilities.md |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/r30/red.log;build/soc-ip-layout/r30/owner-red.log;build/soc-ip-layout/r30/green-final.log |
| `notes` | 98 tests passed across 10 focused modules. Existing silicon policy/IR projects separately for clock and controller owners; absent review stays null and companion/endpoint policy is not inherited. No broader support or hardware observation. |

(status-soc-ip-layout-r31)=
## R31

| 原字段 | 原值 |
| --- | --- |
| `id` | R31 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | I2C V2 const configuration pilot |
| `exit_condition` | Separate immutable configuration from mutable transfer state |
| `implementation_paths` | drivers/i2c/i2c_v2;soc |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r31/i2c-bind-red;build/quality/soc-ip-config-r31/i2c-config-overlap-red;build/quality/soc-ip-config-r31/i2c-green;build/quality/soc-ip-config-r31/i2c-python-green.log |
| `notes` | Selected G474/H743/U575 const configuration and separate BSS runtime implemented. Shared focused execution: 9 targets / 121 Google cases and 38 related Python tests passed; not per-row counts. Development ARM cost comparison is recorded in R46; final P complete gates and scoped execution are recorded in R43-R48. |

(status-soc-ip-layout-r32)=
## R32

| 原字段 | 原值 |
| --- | --- |
| `id` | R32 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | UART DMA const configuration |
| `exit_condition` | Separate immutable configuration from mutable request and block state |
| `implementation_paths` | drivers/uart/uart_v1_channel_dma;soc |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r32/prepare-red;build/quality/soc-ip-config-r32/prepare-green;build/quality/soc-ip-config-r32/profile-python-green.log;build/quality/soc-ip-config-r32/uart-ring-red;build/quality/soc-ip-config-r32/uart-ring-green |
| `notes` | Selected F103/GD303 UART DMA immutable config and BSS runtime implemented; duplex receive extension remains separate. Static config/resources share runtime lifetime and successful stop preserves binding. Final focused run: 3 targets / 65 Google cases plus 38 Python tests. Host execution covers actual SoC wrapper/engine/GPIO; generated DMA factory runtime is not claimed. Development ARM comparison is recorded separately in R46; final P complete gates and scoped execution are recorded in R43-R48. |

(status-soc-ip-layout-r33)=
## R33

| 原字段 | 原值 |
| --- | --- |
| `id` | R33 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | Shared ops immutable faces and direct IRQ |
| `exit_condition` | Preserve shared readonly methods; independent contexts and direct IRQ entry |
| `implementation_paths` | drivers;soc;io |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r32/prepare-red;build/quality/soc-ip-config-r32/prepare-green;build/quality/soc-ip-config-r32/profile-python-green.log;build/quality/soc-ip-config-r32/uart-ring-red;build/quality/soc-ip-config-r32/uart-ring-green;build/quality/soc-ip-config-r31/i2c-green |
| `notes` | Shared readonly ops, immutable public faces, distinct static contexts and direct IRQ entry symbols retained for the selected I2C V2 and UART DMA changes; no hot-path dispatch layer. Focused UART wrapper/engine/GPIO and I2C model suites passed. Development ARM linked retention/costs are recorded in R46; final P scoped validation is recorded in R43-R48. |

(status-soc-ip-layout-r34)=
## R34

| 原字段 | 原值 |
| --- | --- |
| `id` | R34 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | Move initialization and stop logic into C |
| `exit_condition` | Keep explicit lifecycle ordering and cleanup responsibilities in concrete code |
| `implementation_paths` | soc;drivers;tools/configure |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r31/i2c-bind-red;build/quality/soc-ip-config-r31/i2c-config-overlap-red;build/quality/soc-ip-config-r31/i2c-green;build/quality/soc-ip-config-r31/i2c-python-green.log;build/quality/soc-ip-config-r36/legacy-final-green;build/quality/soc-ip-config-r32/prepare-red;build/quality/soc-ip-config-r32/prepare-green;build/quality/soc-ip-config-r32/profile-python-green.log;build/quality/soc-ip-config-r32/uart-ring-red;build/quality/soc-ip-config-r32/uart-ring-green |
| `notes` | Selected I2C V2, F103/GD303 UART DMA and legacy ADC/PWM/Flash cold integration lives in concrete C preparation with existing stop paths. U575 engine stop precedes clock release. UART AHB DMA gate and Platform ordering remain generated; legacy I2C V1 and other peripherals retain family-emitted behavior. Focused suites overlap and are not additive or whole-platform qualification. |

(status-soc-ip-layout-r35)=
## R35

| 原字段 | 原值 |
| --- | --- |
| `id` | R35 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | Retained failure and retry contracts |
| `exit_condition` | Preserve partial effects and borrows until proved drain or explicit retry |
| `implementation_paths` | soc;drivers;tests/google |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r31/i2c-bind-red;build/quality/soc-ip-config-r31/i2c-config-overlap-red;build/quality/soc-ip-config-r31/i2c-green;build/quality/soc-ip-config-r31/i2c-python-green.log;build/quality/soc-ip-config-r36/legacy-final-green;build/quality/soc-ip-config-r32/prepare-red;build/quality/soc-ip-config-r32/prepare-green;build/quality/soc-ip-config-r32/profile-python-green.log;build/quality/soc-ip-config-r32/uart-ring-red;build/quality/soc-ip-config-r32/uart-ring-green |
| `notes` | Selected I2C V2, UART DMA and legacy ADC/PWM/Flash live/retained guards, failed-drain responsibility and stop/retry contracts passed focused regressions. UART guards precede config/pin mutation and include outstanding wake/RX loans; failed withdrawal retains config and borrowed storage. Counts across focused suites overlap; final-source and physical qualification are separate. |

(status-soc-ip-layout-r36)=
## R36

| 原字段 | 原值 |
| --- | --- |
| `id` | R36 |
| `work_package` | static_configuration_and_lifecycle |
| `requirement` | Split legacy ADC PWM and Flash integration |
| `exit_condition` | Separate exact SoC integration from reusable algorithms and narrow contracts |
| `implementation_paths` | soc/stm32/stm32f103/drivers/devices.c;soc/gd32/gd32f303/drivers/devices.c;drivers/adc/legacy_adc;drivers/timer/legacy_pwm;drivers/flash/legacy_flash |
| `implementation_status` | implemented |
| `software_status` | final_P_native_arm_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-config-r36/legacy-high-green;build/quality/soc-ip-config-r36/legacy-python-green.log;build/quality/soc-ip-config-r36/legacy-final-green;build/quality/soc-ip-candidate/legacy-devices/report.json;build/quality/soc-ip-candidate/f103-flash-only-retention.json;build/quality/soc-ip-candidate/gd303-flash-only-retention.json;build/quality/soc-ip-candidate/completion-summary.json;build/quality/soc-ip-candidate/source-snapshot.json;/workspace/nexus-examples/build/platform-software-recovery-92e669233957/providers/mainstream_assembly/report.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/host_contracts.json |
| `notes` | F103/GD303 concrete C ADC/PWM/Flash prepare entries and existing shared close paths implemented. Final two factory targets: 11 Google cases, including full independent 128-sector geometry, live rebind, failed stop/retry and failed initialization cleanup/restart; GD303 foreign oversampling stays unchanged on BUSY. Earlier 6-target/97-case and 40-Python results overlap and are not additive. Four exact legacy ARM builds and two independent Flash-only retention consumers also passed on the 184-file dirty development overlay. This is development evidence, not final clean-source qualification; other peripheral emission is unchanged. Final clean P separately passed all four Native modes and the four f103/gd303 devices baremetal/FreeRTOS fixtures at mainstream_assembly indices 0/1/8/9. The two extra Flash-only consumers remain development observations; they were not rerun or relabeled as P. |

(status-soc-ip-layout-r37)=
## R37

| 原字段 | 原值 |
| --- | --- |
| `id` | R37 |
| `work_package` | build_integration_and_docs |
| `requirement` | Review CMake private includes and Runtime boundary |
| `exit_condition` | Selected targets retain source ownership and vendor includes stay private |
| `implementation_paths` | soc;drivers;cmake |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/integration-audit/runtime-green.log;build/soc-ip-layout/integration-audit/cmake/observations.json |
| `notes` | Three real RED Runtime inclusion regressions then 18 tests GREEN. Eight family consumer include closures show no private leakage; two Cortex-M4 baremetal/FreeRTOS Ninja/ARM configurations exclude SoC/driver/Board objects. This audit configured targets; it did not build production firmware. |

(status-soc-ip-layout-r38)=
## R38

| 原字段 | 原值 |
| --- | --- |
| `id` | R38 |
| `work_package` | build_integration_and_docs |
| `requirement` | Converge SDK transport manifest |
| `exit_condition` | Use required payload closure without redundant source authorities |
| `implementation_paths` | cmake/package |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/integration-audit/sdk-closure-red.log;build/soc-ip-layout/integration-audit/sdk-closure-green.log;build/soc-ip-layout/integration-audit/required-comparison.json |
| `notes` | 48 selection/admission/descriptor tests passed. SDK requirements reuse public source_names and descriptor facts/payload; no prior required member removed. Additional declared family helpers are included dynamically. Actual final P relocated SDK execution is recorded in R45. |

(status-soc-ip-layout-r39)=
## R39

| 原字段 | 原值 |
| --- | --- |
| `id` | R39 |
| `work_package` | build_integration_and_docs |
| `requirement` | Incremental configuration transaction regressions |
| `exit_condition` | Exercise changed-module invalidation and failed reconfiguration through existing gates |
| `implementation_paths` | tools/configure/tests;tools/testing |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/facts-ir/incremental-green.log;build/soc-ip-layout/facts-ir/incremental-receipt.json;build/soc-ip-layout/facts-ir/integration-bundle-green.log;build/soc-ip-layout/facts-ir/final-closure-green.log |
| `notes` | Fresh Native consumer CMake/Ninja configure/build and no-change rebuild executed; changing a declared family module triggered CMake and selected regeneration. All receipt commands exit zero. Separate 143-test integration/bundle suite covers transaction boundaries; not a fresh final-source SDK or full firmware matrix qualification. |

(status-soc-ip-layout-r40)=
## R40

| 原字段 | 原值 |
| --- | --- |
| `id` | R40 |
| `work_package` | build_integration_and_docs |
| `requirement` | Existing CLI diagnostics |
| `exit_condition` | Existing commands report exact provider inputs and failures without a new framework |
| `implementation_paths` | tools/configure/configure.py;tools/configure/experience.py |
| `implementation_status` | implemented |
| `software_status` | focused_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/integration-audit/diagnostic-red.log;build/soc-ip-layout/integration-audit/diagnostic-green.log;build/soc-ip-layout/integration-audit/cli/summary.json |
| `notes` | 35 focused tests passed; eight exact provider-check runs emitted static bundles and identity mismatch rejected. provider_module_missing applies only to the selected entry/parent package; other import errors remain explicit. No compile/link or physical qualification. |

(status-soc-ip-layout-r41)=
## R41

| 原字段 | 原值 |
| --- | --- |
| `id` | R41 |
| `work_package` | build_integration_and_docs |
| `requirement` | Bilingual documentation diagrams and porting migration |
| `exit_condition` | Current source maps; diagram links; READMEs and reviewed gettext match implementation |
| `implementation_paths` | README.md;README_CN.md;drivers/README.md;soc/README.md;soc/common/README.md;tools/configure/README.md;docs/explanation;docs/guides/provider-porting.md;docs/design;docs/delivery/architecture.md;docs/assets/diagrams/README.md;docs/locale |
| `implementation_status` | implemented |
| `software_status` | clean_P_docs_and_D_review_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/documentation/phase1-clean-build.json;build/soc-ip-layout-docs-review.json;build/soc-ip-layout/documentation/phase2-source-scan-final.json;build/soc-ip-layout/documentation/phase2-diagram-check.json;build/soc-ip-layout/documentation/phase2-diagram-review.json;build/soc-ip-layout/documentation/phase2-po-independent-review.json;build/soc-ip-layout/documentation/phase2-review-admission.json;build/soc-ip-layout/documentation/phase2-translations-final.json;build/soc-ip-layout-docs-92e669233957/documentation-build.json;build/soc-ip-layout/documentation/final-d-extraction/extraction.json;build/soc-ip-layout/documentation/final-d-source-check.json;build/soc-ip-layout/documentation/final-d-po-independent-review.json;build/soc-ip-layout/documentation/final-d-review-admission.json;build/soc-ip-layout/documentation/final-d-translations.json |
| `notes` | Phase-1 clean 7b985 strict bilingual build and independent 18-catalog review retained. Phase-2 current content complete: FIG-31 independent source/visual review, locked diagram verification, 131-page source scan, actual gettext extraction and whole-site 262 locale checks passed. Nine catalogs independently reviewed: 101 new messages and 945 unchanged reviewed entries. Clean P strict bilingual build passed with 262 locale checks; its receipt has qualification=false and SHA256 acdca47f4ccf9f1438a8c50f25dd8bea4135821922da202181a5237738ff8166. This later documentation-only update records P; its actual gettext/review and clean-D build are separate documentation evidence, not firmware or physical qualification. D content validation actually passed: fresh locked gettext, 131-page source scan, independent semantic review of 22 new messages with 127 baseline pairs unchanged, and 262 whole-site locale checks. Clean-D strict build is a separate postcommit receipt; no future success or D self-hash is recorded here. |

(status-soc-ip-layout-r42)=
## R42

| 原字段 | 原值 |
| --- | --- |
| `id` | R42 |
| `work_package` | build_integration_and_docs |
| `requirement` | Examples candidate consumption |
| `exit_condition` | Consume the concrete candidate in the external examples repository |
| `implementation_paths` | X-Gen-Lab/nexus-examples |
| `implementation_status` | implemented |
| `software_status` | executed_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | /workspace/nexus-examples/build/local-ci-soc-ip-layout-92e669233957/execution.json;/workspace/nexus-examples/build/soc-ip-layout-validation-92e669233957/sdk-examples-validation-evidence.json;/workspace/nexus-examples/build/soc-ip-layout-candidate-92e669233957/ordinary-hook-evidence.json;.git/local-recovery/soc-ip-layout/examples-main-merge.json |
| `notes` | Exact P consumed by clean examples 0f15022e4d5f5c51991b134d0b9d6082dcf88de9, tree bb60f413b911ec212b59b77cea1734971eddbd9f. Complete canonical local CI passed: Native, installed SDK consumer, 16 ARM presets and 32 main-matrix ELFs for platform_reference/uart_owner_runtime; dedicated example targets remain separately recorded. Formal lock and Gitlink match P; candidate_validation=false. Ordinary hook retained 80 tool tests and 15 CTest / 69 Google cases with no skips. PR15 merged as bd3b90f135600b6405459347cb9e2cff5692d2e3 with the same examples tree. This is exact code-candidate consumption, not physical execution. |

(status-soc-ip-layout-r43)=
## R43

| 原字段 | 原值 |
| --- | --- |
| `id` | R43 |
| `work_package` | verification_and_delivery |
| `requirement` | Four Native modes and complete tool suites |
| `exit_condition` | Execute maintained modes and complete applicable tool domains with fresh nonempty reports |
| `implementation_paths` | tests/google;tools/dev;tools/configure/tests;tools/testing |
| `implementation_status` | implemented |
| `software_status` | executed_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | /workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/report.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/tool-tests.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/configure-tests.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/ci-tests.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/workflow-tests.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/host_contracts.json;build/soc-ip-layout/phase2-hook-evidence/manifest.json |
| `notes` | Tested clean P=92e669233957a2e450a528394e8f4dd0794c7049. All four Native modes debug/release/ASan-UBSan/TSan actually passed: each 372 CTest and 6396 Google cases, zero failures/errors/skips. Complete applicable tool domains passed: testing 1527, configure 1071, ci 490, workflows 159 (3247 total); other tool domains are not inferred from these four. Original executions are retained in candidate evidence. A remains interrupted/running after its 90 completed stages; this row closes the actual completed Native/tool scopes, not a single full-plan pass. The separate ordinary complete-index hook archive records 3247 tools and 352 Google programs / 6396 cases. |

(status-soc-ip-layout-r44)=
## R44

| 原字段 | 原值 |
| --- | --- |
| `id` | R44 |
| `work_package` | verification_and_delivery |
| `requirement` | ARM and Runtime matrix |
| `exit_condition` | Compile and link exact maintained ARM and CPU Runtime configurations |
| `implementation_paths` | tests/contracts;tools/measurement |
| `implementation_status` | implemented |
| `software_status` | segmented_execution_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | /workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/report.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/interruption-observation.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/cortex_runtime/report.json;/workspace/nexus-examples/build/platform-software-recovery-92e669233957/report.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/host_contracts.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/candidate-portable.json |
| `notes` | Tested P=92e669233957a2e450a528394e8f4dd0794c7049: original maintained plan has actual exact segmented coverage A90+B69. A retains running and no source_after after environment interruption; B executed_passed with the exact original suffix, zero stage exits and unchanged clean source. B report SHA256 f8ecf185eadbef7e33d7a667366da7de34241ce243a5d417b5b453ae3bb99694. All original names/order/checkers/args/timeouts and raw identities were checked; no merged executed_passed report was invented. B reran all 865 tidy and 865 cppcheck contexts. CPU Runtime passed 104 variants with source_inputs_stable=true; its full_platform_status stays not_qualified and expected rejections are separate from positive images. ARM/provider/retention/resource fixtures retain their exact original reports; linked software does not establish MCU execution. |

(status-soc-ip-layout-r45)=
## R45

| 原字段 | 原值 |
| --- | --- |
| `id` | R45 |
| `work_package` | verification_and_delivery |
| `requirement` | Relocated SDK integrity |
| `exit_condition` | Verify required installed members and independent relocated consumers |
| `implementation_paths` | cmake/package |
| `implementation_status` | implemented |
| `software_status` | executed_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/sdk-relocation-92e669233957/execution.json;/workspace/nexus-examples/build/soc-ip-layout-validation-92e669233957/sdk-examples-validation-evidence.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/source_sdk.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/verify_relocated_sdk.command.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/candidate-portable.json |
| `notes` | Publishable source SDK binds clean P=92e669233957a2e450a528394e8f4dd0794c7049 and its exact tree. Actual package verification and independent relocated consumers passed at platform-sdk-92e669233957/SDK relocated, including Native C/C++, Runtime and maintained firmware selections; source closure and exported inputs retain original member hashes. The independent SDK/examples archive preserves those executions. Candidate completion-2 reverified the relocated SDK with exit 0. Initial completion used an incorrect verifier root and failed; its original failed receipt remains, followed by the actual corrected execution. No source change or guard weakening was used. |

(status-soc-ip-layout-r46)=
## R46

| 原字段 | 原值 |
| --- | --- |
| `id` | R46 |
| `work_package` | verification_and_delivery |
| `requirement` | Resource retention and cost comparison |
| `exit_condition` | Compare actual Flash RAM ops and IRQ retention for the same selections |
| `implementation_paths` | tools/measurement;tests/contracts |
| `implementation_status` | implemented |
| `software_status` | final_P_comparison_passed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/quality/soc-ip-candidate/cost-summary.json;build/quality/soc-ip-candidate/uart/report.json;build/quality/soc-ip-candidate/legacy-devices/report.json;build/quality/soc-ip-candidate/f103-flash-only-retention.json;build/quality/soc-ip-candidate/gd303-flash-only-retention.json;build/quality/soc-ip-candidate/completion-summary.json;build/quality/soc-ip-candidate/source-snapshot.json;build/quality/soc-ip-final-p-92e669233957/final-cost-summary.json;build/quality/soc-ip-final-p-92e669233957/final-cost-summary.csv;build/quality/soc-ip-cost-package-92e669233957/package.json;build/quality/soc-ip-cost-package-92e669233957/artifact-summary.json |
| `notes` | Historical 184-file dirty development overlay comparisons remain separately identified in their original reports. Final clean P versus 7b985: all 11 strict comparisons passed from actual A/B build artifacts with independent ELF/map/IRQ review; no rebuild for path normalization. I2C total loaded Flash delta G474/H743/U575 = -4/+40/+64 B, composed of code/alignment +108/+152/+224 B, readonly config +24 B each, path-pool/alignment -136/-136/-184 B; reserved RAM -24/-16/-24 B and state 28 to 8 B. Eight UART totals range +168 to +392 B Flash, with code/alignment +336 to +384 B, readonly config 60 B and negative source-path/alignment contributions; all reserve 48 B less RAM, TX state 96 to 48 B and duplex 144 to 96 B. Source-root string reductions are not architecture savings. UART selected IRQ code at most +2 B/one static instruction, up to two config loads; duplex channel local push frame grows 16 to 24 B while MSP reservation is unchanged. No new selected IRQ indirect call; cycles/WCET/dynamic stack high-water are not measured. Portable cost evidence archive SHA256 fb0f63bcae83f7a5b278a311f67107bf4f8c99fcee0014638b0fb26c0936567f, 4035953 B / 1184 files; original source/tool/SDK inputs remain explicit external dependencies. |

(status-soc-ip-layout-r47)=
## R47

| 原字段 | 原值 |
| --- | --- |
| `id` | R47 |
| `work_package` | verification_and_delivery |
| `requirement` | Independent review main integration and formal examples pin |
| `exit_condition` | Complete review and ordinary gates before approved integration and exact examples pin |
| `implementation_paths` | AGENTS.md;X-Gen-Lab/nexus-examples |
| `implementation_status` | implemented |
| `software_status` | integrated_reviewed |
| `hardware_status` | not_executed |
| `evidence_paths` | build/soc-ip-layout/r47-independent-review.json;build/soc-ip-layout/phase2-hook-evidence/manifest.json;build/soc-ip-layout/codeql-pr36-review/review.json;build/soc-ip-layout/codeql-pr36-review/comments.json;build/soc-ip-layout/codeql-pr36-review/remote-status.json;.git/local-recovery/soc-ip-layout/platform-main-merge.json;.git/local-recovery/soc-ip-layout/examples-main-merge.json;.git/local-recovery/soc-ip-layout/platform-merge-import-apply.json;.git/local-recovery/soc-ip-layout/examples-merge-import-apply.json;/workspace/nexus-examples/build/soc-ip-layout-candidate-92e669233957/ordinary-hook-evidence.json |
| `notes` | Independent production review completed without blocking findings; normal complete-index hooks passed on the tree integrated as P. PR36 actually merged to M=c79fd9672a9fe56379744d834cfccd822bc40be1, tree exactly equal to P; PR15 merged to bd3b90f135600b6405459347cb9e2cff5692d2e3, tree equal to tested examples bb60f413b911ec212b59b77cea1734971eddbd9f, with formal lock/Gitlink P. Actual signed GitHub commit objects and local lease/import receipts retained. All 14 CodeQL review comments were assessed: no confirmed production dangling/late binding or exercised test UAF, with nonblocking maintenance suggestions retained; they are not all labeled false positives. Remote Security Analysis 40388967544 CPP/Python jobs passed, not a local CodeQL run or zero-alert/zero-defect claim. This documentation-only follow-up does not retest M as a new software source. |

(status-soc-ip-layout-r48)=
## R48

| 原字段 | 原值 |
| --- | --- |
| `id` | R48 |
| `work_package` | verification_and_delivery |
| `requirement` | Clean software delivery and two fresh offline reproductions |
| `exit_condition` | Bind clean source and actual reproducible artifacts; physical HIL stays not_executed |
| `implementation_paths` | tools/evidence;docs/design/soc-ip-layout-execution.csv |
| `implementation_status` | implemented |
| `software_status` | software_qualified |
| `hardware_status` | not_executed |
| `evidence_paths` | /workspace/nexus-examples/build/platform-candidate-92e669233957/reproduction-quiet1.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/candidate/candidate.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/candidate_verify.command.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/finish-candidate-r48-2.command.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/candidate-portable.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/completion-2/execution-receipts.json;build/soc-ip-layout/qualification-timeout-92e669233957/manifest.json;/workspace/nexus-examples/build/platform-software-cleanenv-92e669233957/interruption-observation.json;/workspace/nexus-examples/build/platform-software-recovery-92e669233957/report.json;/workspace/nexus-examples/build/platform-candidate-92e669233957/finish-candidate-r48.command.json;/workspace/nexus-examples/build/platform-software-final-92e669233957/report.json;build/soc-ip-layout/software-92e669233957/report.json |
| `notes` | Sealed software candidate binds clean P=92e669233957a2e450a528394e8f4dd0794c7049, exact configuration and actual same ELF/BIN/map inputs. Existing six scopes passed: host_contracts, arm_link, resource_budget, source_sdk, reproducibility, hil_tooling; owned candidate verifier and parent completion-2 process exit 0. Two fresh distinct network-none builds passed declared container-boundary verification and bit_exact_elf_bin with no differing artifacts; map bit equality is not claimed. Candidate SHA256 35c8301ac95cdc7eee6b125e5ed8170ef44800bb6eb4995361d02c364660c060. Portable archive SHA256 7f89e8ec33a543e150e7165a3e4252aa11a96d62fe5d3187a9b0d4791aa618b4, 433850915 B / 6401 files. Signature unsigned; product promotion not_performed; HIL not_executed. Maintained-plan evidence is exact A90+B69 segmented coverage, not a single successful 159-stage run. Original output-boundary, timeout, GIT_PAGER-admission, interruption and initial SDK-root failures retain original outcomes; none was rewritten as passed. D only records P and obtains its own separate documentation receipt. |
