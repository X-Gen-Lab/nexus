## Target and resulting behavior

Exact SoC part/package/density, Board identity and observed PCB revision:
Declared clock profile and source documents:
Changed controller/mode and observable behavior:

## Implementation boundary

List the changed `arch/`, `soc/<family>/`, `boards/<board>/board.json`,
public `io/` contracts and external assembly inputs. SDK sources remain private,
locked and unchanged. State startup/rollback, IRQ, lifetime, drain and safe-output
responsibility. Reject unsupported modes and unknown routes.

## Actual evidence

| Check | Source/config/artifact identity, command and result |
|---|---|
| Resolver negative paths and resource conflicts | |
| Production-source fault model | |
| Real ARM startup/vector/controller linkage | |
| ELF Flash/RAM/MSP/heap and GC | |
| Board station/waveform/reset/power-loss or not_executed | |
| External consumer migration | |

Silicon fixtures demonstrate software construction; they do not establish PCB
connector or electrical qualification. State remaining unknowns and the precise
supported tuple.

## Actual TDD sequence

For new controller, provider or configuration behavior, record the test first:

| Step | Actual command, test identity, failure reason and raw log |
|---|---|
| RED before implementation | |
| GREEN after implementation | |
| Regression after refactoring | |

A missing API may initially fail to compile; the finished provider must execute
the behavior and meaningful failure paths. Historical test migration has no
invented RED record. Current passing reports do not prove historical test order.
