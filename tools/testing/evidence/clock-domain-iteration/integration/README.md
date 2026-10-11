# IO58 explicit consumer integration evidence

Four fresh ARM firmware graphs configure, compile, link and pass actual resource
budgets. The same application source and assembly per family are used with sleep
selection off/on. The application is a link fixture and was not executed.

| Family | Default Flash / SRAM incl. 2 KiB MSP | Selected | Delta |
| --- | --- | --- | --- |
| STM32F407 | 1620 / 2096 B | 2724 / 2136 B | +1104 / +40 B |
| GD32F470 | 2924 / 2072 B | 4000 / 2104 B | +1076 / +32 B |

The deltas include the fixture's caller-owned state and linker alignment/GC.
Default compile exports register the optional ClockDomain rule without building
its object/archive. Default final ELFs contain neither the domain/SoC sleep
symbols nor WFI. Selected final ELFs contain the actual provider, domain, Arch
wait routine and WFI instruction. See `graph-elf-checks.json` and raw commands.

Both original selected consumers genuinely failed because function-private SDK
paths depended on an unset variable in the external caller's scope. The source
anchor fix is followed by four fresh successful final2 graphs. The initial
consumer public-dependency omission and verifier symbol expectation error are
preserved separately as setup failures, not runtime behavior regressions.

`manifest.json` binds exact fixtures, generated bundles, actual compiled
dependencies, source boundaries, maps/resources, binary identities and logs.
The copied `executed_matrix_script.py` preserves the original script and source
layout, not a path-independent installed reproduction tool. ELF/BIN bytes are
present locally but subject to the repository's binary ignore policy.

This sidecar closes the earlier pending integration proof for these four
fixtures. Earlier host/model history remains unchanged. Physical wake timing,
power, HIL, clean dual offline reproducibility and product release are not
established by these builds.
