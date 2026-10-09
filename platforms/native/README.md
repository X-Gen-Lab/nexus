# Native platform assembly

Native is a host behavior model for contract tests and external application
execution. `arch/native/` owns host metadata synchronization;
`soc/native/controllers/` owns virtual peripherals;
`soc/native/resources/` owns modeled DMA/interrupt resources;
`soc/native/private/` owns implementation headers.
`platforms/native/src/platform/nx_platform_init.c` owns the host lifecycle.
The platform assembles `soc_native` and the selected Board through the same
`nexus_forward_component_objects()` helper used by MCU platforms.

The `soc/native` directory is an organizational host resource model, not a
physical SoC or evidence of MCU interrupt, DMA, electrical or real-time behavior.
Typed GPIO/UART/SPI/I2C/Flash consumer contracts are tested on Native. Additional
legacy virtual peripherals are explicit test/development models and do not
establish modern MCU providers. OSAL Native uses real host threads and dynamic
resources; its capabilities differ from baremetal and FreeRTOS static pools.

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel 4
ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
```

`native-minimal-debug` disables optional services and OpenSSL. The full profile
selects OpenSSL explicitly and needs its development package; Security core has
no automatic crypto provider. Applications and workers live in the external
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples) repository.

Device discovery starts no virtual hardware. Typed open/close uses explicit
owner/generation references. Shutdown acts on already-created resources,
refuses unsettled ownership and does not create a device to close it. Exact
source/config/test evidence is linked by the
[support matrix](../../docs/strategy/support-matrix.yaml). Windows/macOS presets
are retained; Linux execution does not qualify those host environments.
