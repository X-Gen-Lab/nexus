# Kernel profile linkage contract

This CPU-only consumer retains the actual production scheduler, exception ports
and optional Tick/counter contracts. It uses one authored runtime TOML `[os]`
table through `nexus_add_runtime`; it does not introduce a second source graph.

`NEXUS_TEST_EXTERNAL_TICK_PROVIDER` and `NEXUS_TEST_RUNTIME_COUNTER_PROVIDER`
select explicit synthetic symbols. They are fixture controls, never platform
configuration overrides. An enabled external Tick without its provider must
fail to link rather than silently using the Port's weak SysTick setup. Enabled
runtime statistics similarly require both counter symbols. The successful ELF
must retain their callers, and `vPortSetupTimerInterrupt` must be strong for an
external Tick. Disabled features must not require their symbols.

The linker, clock and provider bodies are software fixtures. They establish no
hardware timer frequency, interrupt priority, low-power behavior or scheduler
execution qualification.
