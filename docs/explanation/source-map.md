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

The physical layout groups Arm CPU implementations by ISA and MCU packages by
vendor. Family identifiers, public include paths and explicit target names remain
stable. These are source ownership groups, not runtime forwarding layers.

Historical development collections are retained in the
[evidence archive](../archive/evidence/README.md) with exact member identities
and restore instructions. Their original source and execution scope remain
unchanged; archive restoration does not establish current-source qualification.

```text
arch/
  include/nexus/arch/         public CPU contracts
  arm/cortex_m/              selected Arm Cortex-M implementation
  native/                    host CPU primitives
drivers/
  <kind>/<ip>/               reviewed reusable register-protocol algorithms
soc/
  common/                    cross-vendor SoC collaboration contracts
  native/configure/          Native policy and static emission
  native/drivers/            independent host device models
  stm32/common/              STM32 identity helpers
  stm32/<family>/            maintained STM32 provider packages
    configure/               STM32 family policy, construction and emission
  gd32/<family>/             maintained GD32 provider packages
    configure/               GD32 family policy, construction and emission
tools/configure/
  providers/                 explicit provider map and shared leaf contracts
  tests/providers/<family>/  host configuration tests
io/
  include/nexus/io/           public I/O contracts
  src/api/                   checked face dispatch
  src/mechanisms/            shared bounded I/O rules
  src/controllers/           shared selected controller engines
  private/                   private structures and hardware ports
components/
  <component>/               common mechanisms
  soc/stm32/<extension>/     explicitly selected STM32 extensions
```

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

```{include} ../../soc/README.md
:relative-docs: ../../soc/
:relative-images:
```

## Reusable IP implementations

```{include} ../../drivers/README.md
:relative-docs: ../../drivers/
:relative-images:
```

The private implementation boundaries and reference configurations are maintained
in [STM32F103](../../soc/stm32/stm32f103/README.md),
[GD32F303](../../soc/gd32/gd32f303/README.md),
[STM32F407](../../soc/stm32/stm32f407/README.md),
[GD32F470](../../soc/gd32/gd32f470/README.md),
[STM32G474](../../soc/stm32/stm32g474/README.md),
[STM32H743](../../soc/stm32/stm32h743/README.md) and
[STM32U575](../../soc/stm32/stm32u575/README.md). Their links bind this website revision.

## Board packages

```{include} ../../boards/README.md
:relative-docs: ../../boards/
:relative-images:
```
