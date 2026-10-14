---
id: docs-reference-configuration-tools
type: reference
audience: maintainer
source_language: en
---

(docs-reference-configuration-tools)=
# Configuration, development and evidence tools

Tools preserve actual process exits and input identities. A prepared plan is not
execution evidence; a model pass is not physical qualification. Run maintained
commands from the repository root with the selected tools on `PATH`.

| Entry | Task | Success does not establish |
| --- | --- | --- |
| `tools/dev/dev.py configure/build/test` | Explicit CMake/CTest preset path | Other presets, skipped tests or HIL |
| `tools/dev/dev.py doctor/check` | Tool availability and repository checks | A platform candidate |
| `tools/configure/configure.py` | init/list-bindings/check/explain/generate | PCB electrical truth |
| `scripts/ci/tdd_gate.py` | Actual full tool and GoogleTest/GoogleMock execution | Historical RED order or HIL |
| `cmake/package/package_source_sdk.py` | Package or verify exact source bytes | MCU execution or release promotion |
| `cmake/package/verify_consumers.py` | Relocated public consumer compile/run/link | Hardware timing |
| `tools/measurement/` | Actual ELF resource, layout and retention checks | Measured stack peaks or WCET |
| `tools/hil/` | Preparation, admission, capture and raw validation by distinct commands | Physical cases never executed |
| `tools/evidence/` | Bind executed scope, environment and actual artifact bytes | Product signature or safety certification |

Use [the SDK tutorial](../../tutorials/source-sdk.md),
[assembly reference](assembly.md), [TDD workflow](../../development/workflow.md)
and [performance/HIL guide](../../guides/performance-hil.md) for complete tasks.
Detailed flags come from each maintained CLI `--help`; do not maintain a second
parser specification here.

Fresh reports require nonzero actual executions and original logs. A missing tool,
empty XML, skipped-only run, stale output or changed input cannot satisfy a gate.
Do not filter or shard complete acceptance runs. Development subsets retain their
limited scope and cannot close a full candidate exit.
