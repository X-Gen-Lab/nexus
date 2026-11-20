---
id: reference.api.io.index
type: reference
audience: consumer
source_language: en
---

(reference.api.io.index)=

# Typed I/O reference

Choose the interface by the operation you need. The declarations define portable identities, formats, borrowing and execution requirements; the selected part, Board and provider determine the available modes. Factory lookup returns an existing face and performs no startup or allocation.

Accepted requests and buffers remain borrowed until acquire-observed SETTLED. Explicit progress, deadline observation, final hardware withdrawal and external joins retain their separate responsibilities.

```{toctree}
:maxdepth: 1

gpio
uart
spi
i2c
timers
analog
time
media
storage
network
usb
system
execution
```
