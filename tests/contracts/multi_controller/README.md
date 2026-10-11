# Provider isolation fixture

The Google tests compile the maintained SoC provider and shared algorithms.
Only the external CPU/NVIC, DATA-register side effects and DMA W1C/disable
boundaries use a hardware model. STM32 executes its normal NVIC path, without
`NEXUS_STM32_MODEL`. Both families use the existing Cortex-M4 host shim; its
legacy GD32 variable name does not select a SoC.

This private `nexus_config.h` supplies an explicit host-only four-bit BASEPRI
policy and the selected chip's external IRQ count. It selects no board,
devices, factory or firmware configuration. Product configurations and real
ARM builds continue to use the authoritative TOML projection.

The models prove request, vector and memory ownership isolation. They do not
prove physical bus timing, DMA throughput or electrical compatibility. Those
checks still require separately recorded hardware qualification.
