---
id: docs-reference-platforms-qualification
type: reference
audience: consumer
source_language: en
---

(docs-reference-platforms-qualification)=
# Source, software evidence and physical qualification

A document describes code; it does not qualify that code merely by being published.
A result applies to the exact source, configuration, environment and artifacts
that were actually executed. Current document source, described code source and
qualified historical code source are separate identities.

![Separate Native, ARM, SDK, reproduction and physical evidence](../../assets/diagrams/qualification-evidence.svg)

FIG-11 shows evidence inputs and their independent scope. No arrow turns a model
or a workflow into physical measurements.

| Evidence | What it can establish | Limit |
| --- | --- | --- |
| Native/GoogleTest/GoogleMock | Executed software contracts against selected models/ports | Not physical timing or electrical truth |
| ARM ELF/BIN/map | Actual compile/link, vector, configuration and resource binding | Not MCU execution |
| Relocated Source SDK consumer | Public include/ABI closure and actual consumer build/run/link | Not product integration or HIL |
| Two fresh offline builds | Actual byte comparison in the bound environment | Not a digest-only or universal reproducibility promise |
| Software candidate | Clean exact scope, identities and retained unsigned artifacts | Not signed product release, safety certification or LTS |
| Real HIL record | Observed hardware/workload/instrument scope on one admitted station | Not unobserved loads or arbitrary boards |

## Locate actual records

The [delivery index](../../delivery/README.md) retains original results and limitations.
The build-system result record binds qualified code `cc6a065c`; its document updates
do not inherit those tests to a new HEAD. Earlier SoC extension evidence binds
`98270512` independently. Read each linked JSON result and raw receipt for exact
full source identity, selected scope, artifact hashes and failed attempts.

Current physical MCU records remain `not_executed`: no connected Board station has
established clock accuracy, IRQ/DMA timing, Cache behavior, context retention,
electrical outputs, Flash power-loss or long-duration performance. Software-reference
HIL fixtures cannot be edited into physical qualification.

A new implementation must run its applicable gates with fresh nonempty reports.
A new documentation-only build may reference old qualified code while explicitly
showing that identity; it must not rewrite the old record or reuse test counts
as current execution. See the [performance/HIL guide](../../guides/performance-hil.md)
and the [build contract](../../design/build-system.md).
