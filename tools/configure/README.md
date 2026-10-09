# One assembly, one resolved configuration

`configure.py` resolves JSON facts, validates the complete selected resource set
and atomically emits compile/link inputs. CMake targets own the source graph.
There is no Kconfig, runtime registry, factory discovery, source globbing or
fallback to an old successful output.

```sh
python tools/configure/configure.py \
  --assembly tools/configure/assemblies/qiming-baremetal.json \
  --output build/my-config
```

Inputs are a maintained `soc/<family>/soc.json`, its reviewed `routes.json`, a
Board package containing `board.json`, one assembly and optional erase-aligned
Flash layout/declared Board input files. Native is a software model. A silicon
route is selectable only if the Board explicitly binds it. External Board paths
are supported and remain declared, hash-bound inputs; unknown physical details
remain visible limitations.

An assembly contains these required fields:

| Field | Responsibility |
| --- | --- |
| `schema_version` | Exact supported schema, currently `1` |
| `board_package` | Path relative to the assembly, or an absolute package path |
| `backend` | `native`, `baremetal` or `freertos` |
| `clock_profile` | Maintained profile matching the Board oscillator |
| `controllers` | Explicit `{id, binding, mode}` selections with bounded options |
| `devices` | Explicit SPI CS endpoints or 7-bit I2C address endpoints |
| `memory_budgets` | Required `main_stack_bytes`, optional Flash/RAM/heap limits |
| `layout` | `null`, or an explicit reset-address erase-aligned layout JSON |

Optional `optimization` is `O2`, `Os`, `O3` or the corresponding `-lto` profile.
Optional `abi` must match the selected CPU facts. Optional `components` selects
owned, explicitly mapped CMake targets; unknown component/driver strings fail.
A BMP280 SPI device requires the `bmp280-spi` component. Applications retain all
component instance storage and initialization policy.

| Kind and maintained mode | Explicit options |
| --- | --- |
| GPIO `input` / `output` | Board route, mask and safe initial bit levels |
| UART `irq-byte-event` | `baud`, `rx_capacity`, `rx_profile` (`bytes`/`events`), `irq_priority` |
| SPI `short-poll` | Optional `max_hz`; STM32 requires reviewed CS child endpoints |
| I2C `poll-messages` | 100 kHz, explicit nonreserved 7-bit child addresses |
| Flash `poll-word` | Exact internal density/geometry; product regions stay external |
| Watchdog `independent` | Construction does not enable the irreversible watchdog |
| EXTI `edge-event` | `edge`, `event_capacity`, `irq_priority` |
| PWM `fixed-pwm` | `period_ticks`, `duty_ticks`, exact-divider `tick_hz` |
| ADC `single-shot` / `low-rate-scan` | Ordered `channels`, `sample_times`, `reference_mv`; GD32 startup `timeout_ms` |

Only currently maintained SoC routes/modes are accepted. SPI polling is bounded
and synchronous; unsupported DMA or IRQ profiles fail. Shared STM32 EXTI vectors
have one generated bounded dispatcher and require identical IRQ priorities.
OS-calling interrupts additionally declare `calls_os: true` and must respect the
FreeRTOS syscall ceiling. Polling resources do not claim unused IRQs. UART bytes
and error-event storage are separate profiles; the selected capacity is the only
RX storage emitted. GD32 fixed ADC sampling uses encoding 7 (480 cycles).

The output bundle contains `resolved.json`, `nexus_config.h`, `nexus_bindings.h`,
`bindings.c`, `selection.cmake`, `memory.ld`, `resource-budget.json` and an ownership
sentinel. `input_paths.json` only navigates to the actual declared input files;
`resolved.json` hashes remain authoritative. The semantic configuration digest is
embedded into the MCU ELF. MSP and heap reservations have explicit linker symbols
and physical intervals; the stack is counted once.

A rejected reconfiguration invalidates the old owned bundle before parsing. A
failure during generation leaves no partial usable output. Duplicate JSON keys,
unknown fields, bool-as-number, overflow, exact-part/pin/ABI contradictions,
unsupported modes, conflicting resources, invalid C identities, unbounded queues,
invalid addresses, unknown drivers and changed input bytes all fail closed.
Required SDK source commits are checked against actual clean Gitlinks or a
verified installed source SDK identity.

Generated startup creates no application workers or scheduler. Stop drains the
selected interfaces in reverse order and retains cleanup progress on BUSY/error;
clock release follows successful interface cleanup. An enabled independent
watchdog cannot be rolled back. Preserve static contexts and the reported
remaining effects until the owner completes recovery.

Run `python -B -m unittest discover -s tools/configure -p 'test_*.py'` for the
behavioral rejection and atomic-output suite. Advanced ARM link fixtures under
`tests/contracts/{stm32,gd32}_assembly` explicitly qualify software routes, not
unknown external PCB wiring or physical HIL.
