---
id: explanation.source-map
type: explanation
audience: maintainer
source_language: en
---

# Source-adjacent contracts

The layer README files remain canonical beside their implementation. The
following inclusions read those exact files rather than copying a second contract.
Relative source links are bound to the website source revision.

## Core

```{include} ../../core/README.md
:relative-docs: ../../core/
:relative-images:
```

## Arch

```{include} ../../arch/README.md
:relative-docs: ../../arch/
:relative-images:
```

## Typed I/O

```{include} ../../io/README.md
:relative-docs: ../../io/
:relative-images:
```

## OS adapters

```{include} ../../os/README.md
:relative-docs: ../../os/
:relative-images:
```

## Components

```{include} ../../components/README.md
:relative-docs: ../../components/
:relative-images:
```

## Exact SoC implementations

The private implementation boundaries and reference configurations are maintained
in [STM32F407](../../soc/stm32f407/README.md),
[GD32F470](../../soc/gd32f470/README.md),
[STM32G474](../../soc/stm32g474/README.md),
[STM32H743](../../soc/stm32h743/README.md) and
[STM32U575](../../soc/stm32u575/README.md). Their links bind this website revision.
