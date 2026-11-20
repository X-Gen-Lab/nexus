---
id: reference.api.index
type: reference
audience: consumer
source_language: en
---

(reference.api.index)=

# Public C API

Declarations and contracts below come from the maintained public C headers
through Doxygen and Breathe. Vendor implementations and private provider state
are excluded. Exact-profile composition headers explicitly identify their
supplier-layout requirements; Native controls remain host-test interfaces.

Select the exact part, Board, backend and mode from the current support
reference. A public header or optional method does not authorize every provider,
board connection or hardware mode.

Factory lookup returns an existing immutable face without starting hardware.
Controllers, child endpoints and optional faces retain their declared
identities. Accepted requests and buffers stay borrowed until acquire-observed
SETTLED; cancellation, deadlines and DMA completion do not revoke the borrow.
Applications own execution progress, waiting, policy and joined shutdown.

## Core, architecture and OS

```{toctree}
:maxdepth: 1

core
arch
os
```

## Typed I/O

```{toctree}
:maxdepth: 1

io/index
```

## Common components

```{toctree}
:maxdepth: 2

components/index
```

## System and explicit test support

```{toctree}
:maxdepth: 1

soc/stm32f4-system
testing/native-models
```
