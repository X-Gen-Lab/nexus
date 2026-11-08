---
id: docs-guides-provider-porting
type: how-to
audience: maintainer
source_language: en
---

(docs-guides-provider-porting)=
# Extend the STM32F4 support package

Use this guide to add an exact F4 part, reviewed route or concrete mode. Current
production source ownership is one F4 package; existing concrete support remains
F407VE/ZG. Do not create a parallel driver set for each part or infer support
from a similar register name.

Read the [integration contract](../design/integration-contracts.md),
[official-library boundary](../design/official-stm32-provider.md#build-time-provider-contract)
and [exact support](../delivery/support.md) first.

## Choose the smallest real change

| Change | Required work |
| --- | --- |
| Same-part Board | Supply reviewed PCB facts and select existing routes |
| New exact F4 part | Add device/part/package/memory/startup facts and validate the actual hardware profile |
| Additional route | Verify AF, DMA, IRQ, conflicts and electrical constraints |
| New hardware behavior | Implement C and meaningful contract tests; extend only necessary schema |
| External device | Use public typed ports from an optional component |

Reusable private IP sources belong inside the F4 package. The
[ownership diagram](../assets/diagrams/driver-source-ownership.svg) distinguishes
source selection from runtime calls; it does not add another dispatch layer.

## Implement and bind

1. Fix the official SDK revision and exact manual/datasheet/errata sources.
2. Describe exact facts without C expressions, register scripts or executable
   family plugins. Keep Board wiring separate from silicon capabilities.
3. Reuse the reviewed HAL/LL path, or document the exact reason for a private
   algorithm. Keep vendor Handles private and one transport owner per domain.
4. Add actual C admission, acquire, stop and partial-failure cleanup. A failed
   current step may already own hardware effects and must retain its receipt.
5. Generate only selected static configuration, storage, faces and connections.
   Factory lookup returns existing objects and does not initialize hardware.
6. Select sources in CMake and reject unsupported parts, modes or resource
   combinations before firmware generation.

## Validate the complete contract

Execute actual RED before production behavior changes, then GREEN with
GoogleTest/GoogleMock for C contracts. Exercise rejection, multiple instances,
partial initialization, cancel, deadline, late IRQ, failed withdrawal and retained
borrows as applicable. Mock external ports, not a replacement implementation.

Compile and link the exact part/mode with its real compiler flags, startup,
geometry and memory constraints. Check unused-mode elimination and actual
resource costs. A host model, header compile or SDK macro is not physical
qualification; station execution remains separate and currently not executed.

Update current documentation and translations with the implementation. Historical
cross-family integration is available from
[the fixed prior source](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)
and does not define the new production catalogue.
