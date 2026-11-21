---
id: reference.api.components.index
type: reference
audience: consumer
source_language: en
---

(reference.api.components.index)=

# Optional component reference

Choose reusable mechanisms by responsibility, then select the exact software
profile and its public CMake target. Components consume explicit ports and
caller-owned storage; products own scheduling, application policy and physical
recovery. Exact-profile storage headers belong only at static composition.

| Responsibility | Start here | Product input |
| --- | --- | --- |
| Numeric processing and control | [Algorithms](algorithms.md) | Units, coefficients, sample interval and independent history |
| Execution coordination | [Runtime helpers](runtime/helpers.md), [owners](runtime/owners.md) | Executor, bounded slots, service budget and joins |
| Wire protocol over selected ports | [Buses](buses.md) | Electrical facts, address, clock and cleanup budget |
| Exact external device | [Devices](devices.md) | Fitted part, power, calibration and recovery |
| Storage and diagnostics | [Services](services.md) | Region/volume, schema, workspace and observation provenance |
| Transport middleware | Protocol pages below | Selected dependency profile and product protocol policy |

The [component guide](../../../guides/component-porting.md) connects selection,
static composition, progress and shutdown. An algorithm or helper does not
implicitly create a worker, queue, heap, device registry or mandatory OS lock.
An external asynchronous transport still owns its accepted request and payload
until acquire-observed SETTLED; a component's timeout cannot revoke that borrow.

The API reference describes current software contracts. It does not authorize an unselected MCU mode, Board connection, kernel gateway or physical result.

```{toctree}
:maxdepth: 1

algorithms
buses
devices
protocols/network
protocols/usb
protocols/modbus
protocols/streams
protocols/boot
runtime/helpers
runtime/owners
runtime/clock-domain
runtime/isolation-ipc
services
```
