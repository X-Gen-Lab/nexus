# SoC packages and private providers

MCU source ownership follows the vendor and exact maintained family. Public
family identifiers such as `stm32f407` and `gd32f470` remain unchanged. The
maintained family descriptor supplies the canonical source directory; directory
names do not create another configuration authority or a runtime registry.

```text
soc/
  common/                     reviewed cross-vendor register algorithms
  native/
    drivers/                  independent host models
  stm32/
    stm32f103/  stm32f407/  stm32g474/  stm32h743/  stm32u575/
  gd32/
    gd32f303/  gd32f470/
```

Each MCU family owns `soc.json`, `routes.json`, its private controller state,
selected driver sources and explicit CMake targets. Startup, clocks, timebase,
memory and power code remain with that family. Common register algorithms are
shared only after every wrapper checks the real vendor layout, bits, clocks,
IRQ/DMA routes and errata. Similar peripheral names do not establish equivalence.

`native/drivers/` implements the public I/O contracts as independent host models;
it does not establish MCU timing or electrical behavior. `common/` remains
cross-vendor because its reviewed algorithms serve both STM32 and GD32.

The [Board packages](../boards/README.md) authorize reviewed PCB bindings.
Adding a Board does not enable a new clock tree automatically: select an exact
maintained clock profile, or implement and verify another profile. Keep unsupported
oscillators, domains, modes and silicon revisions explicit.

Use the [provider guide](../docs/guides/provider-porting.md) for implementation,
the [derived matrix](../docs/delivery/capabilities.md) for selected routes and
modes, and the [qualification guide](../docs/reference/platforms/qualification.md)
for actual evidence. CPU support, SDK macros and source-directory presence do
not establish whole-family support. MCU physical HIL remains `not_executed`.
