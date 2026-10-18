---
id: explanation.index
type: reference
audience: consumer
source_language: en
---

# Architecture explanations

Understand boundaries, static assembly and retained storage before changing a provider. The linked design contracts remain authoritative.

The [I/O evolution design](../design/io-evolution.md) explains resource plans,
optional traits and enhanced data contracts; its execution ledger keeps current
and pending work separate.

The [H743 Flash RAM design](../design/h743-flash-ram.md) explains the selected
V/cache-off maintenance boundary: a private CPU bridge, RAM vector owner,
temporary MPU protection and real drain before return. The source is integrated;
the target diagrams retain open production-ELF and complete software exits.

The [RTC/LPTIM explanation](rtc-lptim.md) separates raw calendar, finite counter and Core deadline domains. It covers current Native typed assembly, ownership, drain and explicit lifecycle; physical RTC/LPTIM and low-power support remain separate targets.

```{toctree}
:maxdepth: 2

platform-architecture
factory
ownership
rtc-lptim
source-map
```
