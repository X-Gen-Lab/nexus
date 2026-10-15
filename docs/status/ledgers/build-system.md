---
id: status.ledgers.build-system
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-build-system)=
# 构建系统原专项台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/build-system-execution.csv)。
源文件：`docs/design/build-system-execution.csv`；SHA-256：`cc459a0d064abc1ec360f1d3daf4ed5e5b531f299e581d0b2724d04aa7c7074c`。

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

(status-build-system-bld-01)=
## BLD-01

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-01 |
| `priority` | P1 |
| `requirement` | Protect authored and fact inputs from output cleanup |
| `exit_condition` | Reject output containment before mutation; all input bytes unchanged |
| `implementation_paths` | tools/configure/bundle.py;tools/configure/configure.py;tools/configure/test_bundle_transaction.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/red.log;/workspace/nexus-build-maintenance-20261012/config/nested-fact-red.log;/workspace/nexus-build-maintenance-20261012/config/nested-fact-green.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-02)=
## BLD-02

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-02 |
| `priority` | P1 |
| `requirement` | Transactional configuration and failure invalidation |
| `exit_condition` | Errors and interrupted generation never expose stale valid or partial bundles |
| `implementation_paths` | tools/configure/bundle.py;tools/configure/configure.py;tools/configure/runtime.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/code-race-red.log;/workspace/nexus-build-maintenance-20261012/config/code-race-green.log;/workspace/nexus-build-maintenance-20261012/config/transaction-green-v2.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-03)=
## BLD-03

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-03 |
| `priority` | P2 |
| `requirement` | Single writer for each generated output |
| `exit_condition` | Same output concurrency rejects or serializes; separate outputs remain independent |
| `implementation_paths` | tools/configure/bundle.py;tools/configure/test_bundle_transaction.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/red.log;/workspace/nexus-build-maintenance-20261012/config/green.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-04)=
## BLD-04

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-04 |
| `priority` | P1 |
| `requirement` | Complete derived artifact rules |
| `exit_condition` | Normal target build restores deleted BIN map and resource report |
| `implementation_paths` | cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-arm-canonical-budget.log;/workspace/nexus-build-maintenance-20261012/targets/arm-artifacts-contract.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-05)=
## BLD-05

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-05 |
| `priority` | P1 |
| `requirement` | Resource checker input dependencies |
| `exit_condition` | ELF configuration budget or checker changes cause a new checked execution |
| `implementation_paths` | tools/configure/memory.py;tools/measurement/resources.py;tests/contracts/test_measurement_resources.py;cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/config/budget-origin-red.log;/workspace/nexus-build-maintenance-20261012/config/budget-origin-final-green.log;/workspace/nexus-build-maintenance-20261012/targets/green-arm-canonical-budget.log;/workspace/nexus-build-maintenance-20261012/targets/arm-artifacts-contract.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-06)=
## BLD-06

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-06 |
| `priority` | P1 |
| `requirement` | Actual configuration code dependencies |
| `exit_condition` | Changed factory epoch or required verifier causes automatic reconfiguration |
| `implementation_paths` | tools/configure/dependencies.py;cmake/platform/Configure.cmake;cmake/platform/Runtime.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/red.log;/workspace/nexus-build-maintenance-20261012/config/code-race-red-return.json;/workspace/nexus-build-maintenance-20261012/config/code-race-red.log;/workspace/nexus-build-maintenance-20261012/config/code-race-green.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-07)=
## BLD-07

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-07 |
| `priority` | P1 |
| `requirement` | Explicit target LTO for external consumers |
| `exit_condition` | Owned application and startup targets share selected IPO policy |
| `implementation_paths` | cmake/platform/TargetPolicy.cmake;cmake/platform/Options.cmake;cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/targets/green-arm-canonical-budget.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-08)=
## BLD-08

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-08 |
| `priority` | P1 |
| `requirement` | Shared hook and CI change classification |
| `exit_condition` | Gitlinks vendors dependency locks and build environments trigger the same gates |
| `implementation_paths` | tools/dev/change_paths.py;scripts/ci/tdd_gate.py;scripts/ci/native_contracts.py;.github/workflows/ci.yml;scripts/ci/test_change_paths.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-09)=
## BLD-09

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-09 |
| `priority` | P1 |
| `requirement` | Preserve unchanged generated file timestamps |
| `exit_condition` | Identical resolution preserves unchanged header source and linker mtimes |
| `implementation_paths` | tools/configure/bundle.py;tools/configure/configure.py;tools/configure/runtime.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/red.log;/workspace/nexus-build-maintenance-20261012/config/green.log;/workspace/nexus-build-maintenance-20261012/config/incremental-consumer.json;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-10)=
## BLD-10

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-10 |
| `priority` | P1 |
| `requirement` | No-change build regression |
| `exit_condition` | Repeated build and identical configure perform no unnecessary compilation or linking |
| `implementation_paths` | tools/configure/test_incremental_consumer.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/incremental-consumer.json;/workspace/nexus-build-maintenance-20261012/config/incremental-consumer.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-11)=
## BLD-11

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-11 |
| `priority` | P2 |
| `requirement` | Changes rebuild the intended scope |
| `exit_condition` | Application driver header assembly and budget mutations follow declared dependencies |
| `implementation_paths` | tools/configure/test_bundle_transaction.py;tools/configure/test_incremental_consumer.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/red.log;/workspace/nexus-build-maintenance-20261012/config/incremental-consumer.json;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-12)=
## BLD-12

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-12 |
| `priority` | P2 |
| `requirement` | Stage-level execution metrics |
| `exit_condition` | Actual duration command and execution counts are recorded per stage |
| `implementation_paths` | tools/dev/process.py;tools/dev/qualification.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/plan-success-pinned-run/report.json;/workspace/nexus-build-maintenance-20261012/delivery/plan-failure-pinned-run/report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-13)=
## BLD-13

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-13 |
| `priority` | P1 |
| `requirement` | Bounded build concurrency |
| `exit_condition` | Local hooks and CI use explicit memory-conscious parallel bounds |
| `implementation_paths` | tools/dev/process.py;tools/dev/dev.py;scripts/ci/tdd_gate.py;scripts/ci/native_contracts.py;.github/workflows/ci.yml |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-14)=
## BLD-14

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-14 |
| `priority` | P1 |
| `requirement` | Timeout cancellation and process cleanup |
| `exit_condition` | Hung child processes terminate with retained nonzero evidence and no surviving descendants |
| `implementation_paths` | tools/dev/process.py;tools/testing/test_build_workflow.py;tools/testing/test_staged_execution.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-15)=
## BLD-15

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-15 |
| `priority` | P2 |
| `requirement` | Correctly keyed compiler cache |
| `exit_condition` | Compiler flags configuration and dependencies distinguish cache inputs; qualification builds remain fresh |
| `implementation_paths` | tools/dev/cache.py;cmake/platform/TargetPolicy.cmake;cmake/platform/Options.cmake;dependencies/ccache.lock.json;tests/contracts/test_build_system.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/delivery/ccache-archive.json;/workspace/nexus-build-maintenance-20261012/delivery/ccache-prepare.log;/workspace/nexus-build-maintenance-20261012/targets/actual-ccache-contract-final.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_measured_cache_and_fresh_builds_verified |

(status-build-system-bld-16)=
## BLD-16

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-16 |
| `priority` | P2 |
| `requirement` | Content-addressed download and tool caches |
| `exit_condition` | Cached bytes require digest verification; corruption is rejected |
| `implementation_paths` | tools/dev/cache.py;tools/dev/dependencies.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/ccache-archive.json;/workspace/nexus-build-maintenance-20261012/delivery/final-all-owned-green.log;/workspace/nexus-build-maintenance-20261012/workflow/hook-isolation-final-v2/final-report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_actual_digest_checked_cache_verified |

(status-build-system-bld-17)=
## BLD-17

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-17 |
| `priority` | P2 |
| `requirement` | Prepare dependencies for the actual execution scope |
| `exit_condition` | Each scope initializes required sources including host hardware model headers |
| `implementation_paths` | tools/dev/dependencies.py;.github/actions/setup-build/action.yml |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/prepare-native.json;/workspace/nexus-build-maintenance-20261012/delivery/prepare-h743.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_actual_scoped_preparation_verified |

(status-build-system-bld-18)=
## BLD-18

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-18 |
| `priority` | P3 |
| `requirement` | Avoid duplicate verification within one SDK configure |
| `exit_condition` | Internal verified identity may be reused without cross-run stale-byte acceptance |
| `implementation_paths` | cmake/package/NexusConfig.cmake.in;cmake/platform/SDK.cmake;tools/configure/sdk.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/sdk-current-repeat-verification-decision.json;/workspace/nexus-build-maintenance-20261012/delivery/sdk-current-full-verify-0.raw;/workspace/nexus-build-maintenance-20261012/delivery/sdk-current-full-verify-1.raw;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | decision_verified |

(status-build-system-bld-19)=
## BLD-19

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-19 |
| `priority` | P2 |
| `requirement` | Resolver stages with clear responsibilities |
| `exit_condition` | Input facts selection resources memory and IR sealing have explicit stage boundaries |
| `implementation_paths` | tools/configure/configure.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-20)=
## BLD-20

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-20 |
| `priority` | P2 |
| `requirement` | Typed IR emission interfaces |
| `exit_condition` | Maintained emitters consume named immutable records with checked projection boundaries |
| `implementation_paths` | tools/configure/ir.py;tools/configure/factory.py;tools/configure/bindings.py;tools/configure/secondary_controllers.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/factory-refactor-first.log;/workspace/nexus-build-maintenance-20261012/config/endpoint-ir-red.log;/workspace/nexus-build-maintenance-20261012/config/endpoint-ir-green.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-21)=
## BLD-21

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-21 |
| `priority` | P2 |
| `requirement` | Configuration factory and target mapping consistency |
| `exit_condition` | Missing kinds modes or component mappings fail owned consistency checks |
| `implementation_paths` | tools/configure/contracts.py;tools/configure/matrix.py;tools/configure/test_matrix.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/matrix-tests-red.log;/workspace/nexus-build-maintenance-20261012/config/matrix-tests-green-v3.log;/workspace/nexus-build-maintenance-20261012/config/support-matrix.json;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-22)=
## BLD-22

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-22 |
| `priority` | P2 |
| `requirement` | Separate configuration and source identities |
| `exit_condition` | Configuration generator source dependency and toolchain identities retain distinct meanings |
| `implementation_paths` | tools/configure/dependencies.py;tools/configure/sdk.py;tools/configure/memory.py;tools/measurement/resources.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/code-race-red-return.json;/workspace/nexus-build-maintenance-20261012/config/code-race-green.log;/workspace/nexus-build-maintenance-20261012/config/budget-origin-final-green.log;/workspace/nexus-build-maintenance-20261012/config/sdk-actual-old-boundary/report.json;/workspace/nexus-build-maintenance-20261012/config/sdk-final-restored-verify.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-23)=
## BLD-23

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-23 |
| `priority` | P2 |
| `requirement` | Actionable configuration errors |
| `exit_condition` | Errors identify source field instance conflict and allowed range |
| `implementation_paths` | tools/configure/configure.py;tools/configure/test_bundle_transaction.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/diagnostics-red.log;/workspace/nexus-build-maintenance-20261012/config/diagnostics-green.log;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-24)=
## BLD-24

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-24 |
| `priority` | P2 |
| `requirement` | Maintained selection and validation inventory |
| `exit_condition` | Supported selections bind existing fixtures and evidence scopes without promoting physical support |
| `implementation_paths` | tools/configure/matrix.py;tools/configure/assemblies/catalogue.json;tools/configure/test_matrix.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/config/implementation-report.json;/workspace/nexus-build-maintenance-20261012/config/support-matrix.log;/workspace/nexus-build-maintenance-20261012/config/support-matrix.json;/workspace/nexus-build-maintenance-20261012/config/complete-green-final.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-25)=
## BLD-25

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-25 |
| `priority` | P1 |
| `requirement` | Compile and link policy propagation |
| `exit_condition` | ABI requirements propagate while private policy remains scoped |
| `implementation_paths` | cmake/platform/Options.cmake;cmake/platform/TargetPolicy.cmake;cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/targets/arm-artifacts-contract.json; /workspace/nexus-build-maintenance-20261012/targets/soc-sleep-policy-fixture-report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-26)=
## BLD-26

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-26 |
| `priority` | P2 |
| `requirement` | Explicit debuggable optimization profiles |
| `exit_condition` | Symbolic production optimization and single-step profiles have inspectable final flags |
| `implementation_paths` | cmake/platform/Options.cmake;CMakePresets.json;tools/configure/assemblies/native-step-debug.toml;tools/configure/runtime.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-27)=
## BLD-27

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-27 |
| `priority` | P2 |
| `requirement` | Consumer-owned application warnings |
| `exit_condition` | Owned platform remains strict; applications explicitly choose warning policy |
| `implementation_paths` | cmake/platform/TargetPolicy.cmake;cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-28)=
## BLD-28

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-28 |
| `priority` | P2 |
| `requirement` | Firmware helper argument validation |
| `exit_condition` | Unknown missing repeated or invalid setup arguments fail at configuration |
| `implementation_paths` | cmake/platform/Firmware.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/red-repeat-keywords.log;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-29)=
## BLD-29

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-29 |
| `priority` | P2 |
| `requirement` | Early single assembly enforcement |
| `exit_condition` | Mixed resolved sources or Runtime and Platform modes reject before target collisions |
| `implementation_paths` | CMakeLists.txt;cmake/platform/Options.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/red-second-assembly.log;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-30)=
## BLD-30

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-30 |
| `priority` | P2 |
| `requirement` | Declared generator support |
| `exit_condition` | Maintained single-config Ninja works; unsupported generator modes fail clearly |
| `implementation_paths` | CMakeLists.txt;cmake/platform/TargetPolicy.cmake |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-31)=
## BLD-31

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-31 |
| `priority` | P3 |
| `requirement` | Measure unnecessary compilation |
| `exit_condition` | Compile-unit and linked-image costs are measured before choosing source splits |
| `implementation_paths` | tests/contracts/test_build_artifacts.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/arm-artifacts-contract.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | decision_verified |

(status-build-system-bld-32)=
## BLD-32

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-32 |
| `priority` | P2 |
| `requirement` | External consumer boundary coverage |
| `exit_condition` | Subdirectory SDK C and C++ LTO relocated paths and private include isolation execute |
| `implementation_paths` | tests/contracts/test_build_system.py;tests/contracts/test_build_artifacts.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/targets/arm-artifacts-contract.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-33)=
## BLD-33

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-33 |
| `priority` | P1 |
| `requirement` | One preset resolver |
| `exit_condition` | Supported inheritance includes and user presets resolve identically for all runners |
| `implementation_paths` | tools/dev/presets.py;tools/dev/dev.py;scripts/ci/tdd_gate.py;scripts/ci/native_contracts.py;tools/testing/test_build_workflow.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-34)=
## BLD-34

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-34 |
| `priority` | P2 |
| `requirement` | Discoverable maintained platform builds |
| `exit_condition` | H743 U575 and explicitly labelled G474 fixtures have clear build entry points |
| `implementation_paths` | CMakePresets.json;tools/configure/assemblies/catalogue.json;tools/configure/assemblies/native-step-debug.toml |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-35)=
## BLD-35

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-35 |
| `priority` | P2 |
| `requirement` | Scope-specific environment diagnosis |
| `exit_condition` | Selected Native ARM or candidate scope checks necessary versions tools and dependencies |
| `implementation_paths` | tools/dev/dev.py;tools/dev/dependencies.py;dependencies/environment.lock.json;tools/testing/test_dev.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-36)=
## BLD-36

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-36 |
| `priority` | P2 |
| `requirement` | Development and qualification execution scopes |
| `exit_condition` | Focused execution never claims full suite qualification; receipts declare actual scope |
| `implementation_paths` | tools/dev/dev.py;tools/dev/process.py;tools/testing/test_google_report.py;scripts/ci/tdd_gate.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-37)=
## BLD-37

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-37 |
| `priority` | P2 |
| `requirement` | Shared dependency and tool preparation |
| `exit_condition` | Development and CI use one checked online or offline preparation path |
| `implementation_paths` | tools/dev/dependencies.py;tools/dev/dev.py;.github/actions/setup-build/action.yml |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-38)=
## BLD-38

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-38 |
| `priority` | P2 |
| `requirement` | Explicit qualification and candidate orchestration |
| `exit_condition` | Fixed maintained checkers execute from a reviewable plan with real failures and identity |
| `implementation_paths` | tools/dev/qualification.py;tools/dev/plans/software.json;tools/dev/plans/candidate.json |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/plan-success-pinned-run/report.json;/workspace/nexus-build-maintenance-20261012/delivery/plan-failure-pinned-run/report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_actual_candidate_plan_verified_full_software_plan_not_executed |

(status-build-system-bld-39)=
## BLD-39

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-39 |
| `priority` | P2 |
| `requirement` | Dependency upgrade validation |
| `exit_condition` | Upgrade binds source license locks host ARM SDK and applicable reproducibility evidence |
| `implementation_paths` | tools/dev/dependencies.py;tools/maintenance/dependency_proposals.py;dependencies/actions.lock.json |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/final-tools-catalogue-green.log;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_upgrade_contract_verified |

(status-build-system-bld-40)=
## BLD-40

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-40 |
| `priority` | P2 |
| `requirement` | Complete hash-locked offline Python tools |
| `exit_condition` | Transitive dependencies and wheel digests support installation without network |
| `implementation_paths` | tools/dev/dependencies.py;dependencies/python-tools-linux-cp311.lock.json;dependencies/python-tools-linux-cp311.txt;dependencies/python-tools-linux-cp312.lock.json;dependencies/python-tools-linux-cp312.txt |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/offline-install-cp312-result.json;/workspace/nexus-build-maintenance-20261012/delivery/python-offline-cp312-envelope.json;/workspace/nexus-build-maintenance-20261012/delivery/wheel-package-restore.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/workflow/hook-isolation-final-v2/final-report.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | actual_cp312_offline_install_and_hook_isolation_verified |

(status-build-system-bld-41)=
## BLD-41

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-41 |
| `priority` | P1 |
| `requirement` | Portable qualified environment distribution |
| `exit_condition` | Another directory imports the recorded OCI and toolchain identities and verifies bytes |
| `implementation_paths` | tools/evidence/environment.py;tools/evidence/store.py;tools/evidence/container.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/environment-execution/oci-transport-receipt.json;/workspace/nexus-build-maintenance-20261012/delivery/portable-navigation-red.log;/workspace/nexus-build-maintenance-20261012/delivery/portable-navigation-green.log;/workspace/nexus-build-maintenance-20261012/delivery/environment-report-archive-red.log;/workspace/nexus-build-maintenance-20261012/delivery/environment-report-archive-green.log;/workspace/nexus-build-maintenance-20261012/environment-execution/toolchain-transport-receipt.json;/workspace/nexus-build-maintenance-20261012/environment-execution/oci-runtime-provenance-clarification.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | actual_toolchain_and_oci_transport_verified |

(status-build-system-bld-42)=
## BLD-42

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-42 |
| `priority` | P3 |
| `requirement` | Reproducible environment recipe boundary |
| `exit_condition` | Snapshot package versions are declared; artifact reuse and recipe reconstruction remain distinct |
| `implementation_paths` | tools/evidence/recipe.py;dependencies/build-recipe.lock.json;tools/dev/Dockerfile |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/observed-apt.json;/workspace/nexus-build-maintenance-20261012/delivery/observed-apt-command.json;/workspace/nexus-build-maintenance-20261012/delivery/recipe-declared-inputs.json;/workspace/nexus-build-maintenance-20261012/delivery/recipe-green.log; /workspace/nexus-build-maintenance-20261012/delivery/recipe-ca-lock-preservation.json;/workspace/nexus-build-maintenance-20261012/workflow/hook-isolation-final-v2/final-report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | declared_recipe_verified_reconstruction_unverified |

(status-build-system-bld-43)=
## BLD-43

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-43 |
| `priority` | P2 |
| `requirement` | Complete build provenance |
| `exit_condition` | Formal output binds source configuration dependencies compiler environment and executed commands |
| `implementation_paths` | tools/dev/provenance.py;tools/dev/qualification.py;tools/dev/dev.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/delivery/provenance-fixture/verification.json;/workspace/nexus-build-maintenance-20261012/workflow/green-automatic-provenance.txt;/workspace/nexus-build-maintenance-20261012/workflow/green-final-metadata-provenance.txt; /workspace/nexus-build-maintenance-20261012/workflow/report-v5.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-44)=
## BLD-44

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-44 |
| `priority` | P1 |
| `requirement` | Durable independently verifiable evidence |
| `exit_condition` | Delivery evidence survives original workspace removal and retains independent integrity verification |
| `implementation_paths` | tools/evidence/store.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/delivery/delivery-implementation-status.json;/workspace/nexus-build-maintenance-20261012/delivery/durable-adoption.json;/workspace/nexus-build-maintenance-20261012/delivery/wheel-package-restore.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | actual_current_evidence_archive_restoration_verified |

(status-build-system-bld-45)=
## BLD-45

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-45 |
| `priority` | P2 |
| `requirement` | Current executable build documentation |
| `exit_condition` | TOML and CPU ABI descriptions match implementation; maintained command examples execute |
| `implementation_paths` | cmake/README.md;cmake/package/README.md;tools/dev/README.md;tools/configure/README.md;docs/design/build-system.md |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/doctor-arm.txt;/workspace/nexus-build-maintenance-20261012/workflow/prepare-h743-v2.txt;/workspace/nexus-build-maintenance-20261012/delivery/final-tools-catalogue-green.log;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_documentation_and_commands_verified |

(status-build-system-bld-46)=
## BLD-46

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-46 |
| `priority` | P2 |
| `requirement` | Explicit host and tool support matrix |
| `exit_condition` | Tools compiler OS versions and validated versus unknown scopes are stated accurately |
| `implementation_paths` | docs/design/build-system.md;dependencies/environment.lock.json;dependencies/toolchains.lock.json;dependencies/python-tools-linux-cp311.lock.json;dependencies/python-tools-linux-cp312.lock.json |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/documentation/host-tool-support-observations.json;/workspace/nexus-build-maintenance-20261012/workflow/hook-isolation-final-v2/final-report.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_observed_matrix_verified |

(status-build-system-bld-47)=
## BLD-47

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-47 |
| `priority` | P1 |
| `requirement` | Build-system TDD regression suite |
| `exit_condition` | Real small projects cover protection no-op repair dependencies external LTO rejection and cancellation |
| `implementation_paths` | tests/contracts/test_build_system.py;tests/contracts/test_build_artifacts.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/targets/targets-implementation-report-v2.json;/workspace/nexus-build-maintenance-20261012/targets/green-native-final-20.log;/workspace/nexus-build-maintenance-20261012/targets/green-arm-canonical-budget.log;/workspace/nexus-build-maintenance-20261012/targets/actual-ccache-contract-final.json; /workspace/nexus-build-maintenance-20261012/failed-normal-hook-1/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/configure/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/native/execution.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/sdk-boundaries-attempt-2-receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |

(status-build-system-bld-48)=
## BLD-48

| 原字段 | 原值 |
| --- | --- |
| `id` | BLD-48 |
| `priority` | P2 |
| `requirement` | Common execution and failure record format |
| `exit_condition` | Stage argv exit duration identity and artifacts are retained; retry preserves earlier failed records |
| `implementation_paths` | tools/dev/process.py;tools/dev/provenance.py;scripts/ci/tdd_gate.py;scripts/ci/native_contracts.py;tools/testing/test_staged_execution.py |
| `evidence_paths` | /workspace/nexus-build-maintenance-20261012/workflow/report-v4.json;/workspace/nexus-build-maintenance-20261012/workflow/final-related-tests-v4.command.json; /workspace/nexus-build-maintenance-20261012/workflow/report-v5.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/ci/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/github-ci/cc6a065c002041234a212a964be863a69198f9b4/summary.json;/workspace/nexus-build-maintenance-20261012/normal-memory-contract-fix-hook-success/receipt.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c00/candidate-matrix.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/testing/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/cc6a065c/workflows/execution.json;/workspace/nexus-build-maintenance-20261012/final-qualification/formal-execution-evidence-index-cc6a065c00.json;/workspace/nexus-build-maintenance-20261012/independent-review/candidate-archive-audit/final-eight-archive-only-independent-review.json |
| `software_status` | implemented_software_validated |
