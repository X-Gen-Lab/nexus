# STM32G474 generated factory integration

This record exercises the actual generated static factory, startup transaction,
GPIO and LPUART providers. The host substitutes external CPU, register and NVIC
leaves only. It does not mock the implementation or replace the static factory
with a runtime registry. The reviewed Board package is an HSE24 reference; it
is not evidence of a particular populated PCB.

The final regular execution has 14 factory, 10 system and 23 provider cases,
with no failures or skips. The final ASAN/UBSAN execution has all 14 factory
cases. Both preserve actual discovery, commands, raw output and fresh XML, and
check captured source before and after execution. The regular rerun covers the
existing 33 cases after the explicit hardware-model width cast. Host sanitizer
results do not establish MCU behavior, timing or DMA qualification.

The four authored ARM fixtures execute real configure and link operations:

| Assembly | Flash footprint | Reserved RAM including MSP |
|---|---:|---:|
| Empty baremetal | 2592 B | 2112 B |
| Selected GPIO/UART baremetal | 8424 B | 2332 B |
| Empty FreeRTOS | 7680 B | 3724 B |
| Selected GPIO/UART FreeRTOS | 13568 B | 3952 B |

These are fixture costs, not a complete product budget. Selected graphs retain
2 real faces, 2 operation tables and their 10 non-null methods. Empty graphs
prove zero selected faces and absence of GPIO/UART provider storage and code.
The exact vector words bind TIM2 IRQ28, selected LPUART1 IRQ91, and the real
FreeRTOS SVC/PendSV/SysTick handlers. The generated 170 MHz/1 kHz reload169999
configuration is compile/link evidence; it is not a measured time accuracy.

Historical RED and failed attempts are retained independently:

- A historical pre-count mutant reproduces failed-constructor cleanup and
  restart failures. It is explicitly a replay, not fabricated original TDD order.
- Live foreign CR1 and DMAR-only CR3 each produce actual preflight RED results:
  56 clock/pin writes occurred before late rejection. The early admission guard
  removes those effects.
- The first CR3 GREEN attempt still failed because Ninja used stale generated
  bindings. Real configure dependencies were added. An inert real generator edit
  and its exact restoration each trigger CMake regeneration in the same build
  directory and then execute all 14 cases successfully.
- Strict sanitizer compilation diagnosed a genuine 32-bit model conversion.
  An explicit uint32_t narrowing preserves register semantics; no warning is
  disabled. The failed compile and new regular/sanitizer results remain separate.

The initial whole-GPIO latch expectation was a test error: LPUART TX preloads a
separate pin. The first ARM auditor also mishandled the Thumb bit of a weak alias;
its original failed manifest and driver are retained alongside the new auditor.
Neither failure is relabeled as a firmware defect or as passed execution.

`manifest.json` binds original and preserved raw bytes. Small ARM ELF/BIN/map
artifacts are retained with `.raw` suffixes. Final host executable identities
refer to immutable external copies; large host binaries are not copied into
Git. Earlier phase records referenced a shared executable path and do not prove
that an earlier executable remains available after later rebuilds. Historical
source hashes are observations of those attempts; they are not rewritten to
match current source. Final source snapshots and drivers are data, not a second
maintained build graph.

Everything here qualifies dirty-source development software only. Clean SDK,
sealed offline builds, candidate qualification and physical HIL must execute
separately against the final source and artifact identities. Physical execution
remains `not_executed`. No G474 ThreadX, DMA or other peripheral support is inferred.
