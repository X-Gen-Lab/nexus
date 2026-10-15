---
id: docs-guides-board-porting
type: how-to
audience: maintainer
source_language: en
---

(docs-guides-board-porting)=
# Add a reviewed Board package

Use this guide when the exact SoC already has a maintained provider. A Board adds
PCB facts and reviewed bindings; it does not copy a vendor SDK or authorize every
silicon route. Actual unobserved PCB revision, bridges and voltage stay unknown.

Prepare the [fixed source and recursive dependencies](source-checkout.md) before
using the resolver or build commands below.

## Gather the minimum facts

Record the full part, package, source document revision and retained provenance.
Record oscillator values, supply conditions, SWD reservations, pins, active levels,
jumper conditions and safe initial outputs. Distinguish document defaults from an
observation of your particular board. Start with one LED and one console route.

The following is a complete maintained Board file, including `soc_family`:

```{literalinclude} ../../boards/stm32f407_qiming_v31/board.json
:language: json
:caption: Source-reviewed Qiming V3.1 facts; physical PCB revision remains unknown.
```

The [integration contract](../design/integration-contracts.md) owns
field and resource requirements. Keep your Board package in its external product
repository when it describes private PCB data. Public reusable packages live under
`boards/`; ordinary packages require only `board.json` and a README. Add bounded
neutral Board hooks only for electrical actions not expressible by maintained data.

## Validate through the actual resolver

From the Nexus repository root, inspect a known-good reference before authoring:

```sh
python tools/configure/configure.py list-bindings     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py check     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py explain     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
```

For your package, select `board_package` in an external complete assembly and run
those same commands with its absolute path. Relative package paths resolve from
the authored assembly. A JSON syntax check alone cannot prove SoC identity, clock,
route or resource compatibility. Unknown fields and unreviewed routes must reject.

## Build and record the selected scope

Configure your external consumer using its assembly and the locked ARM toolchain.
Keep baremetal and FreeRTOS in separate output roots. Check resolved claims,
vector/startup, strong handlers, ELF/BIN/map and every physical RAM/Flash budget.
Factory lookup must be pure before start and instance state must remain isolated.
Use a software Board fixture for model/link testing; label its wiring accordingly.

Prepare [HIL plans](performance-hil.md) without equipment. Physical qualification
requires a separately reviewed station, observed Board/part/UID/probe/wiring,
exact image and real raw measurements. Source-reviewed pads never establish
connector continuity, immunity or safe electrical behavior.
