# H743 actual ARM assembly fixtures

The empty and GPIOB PB0/USART3 IRQ-byte TOMLs select one exact software reference,
with explicit budgets and no product task or Flash partition. Baremetal and
FreeRTOS are separate ABI/build compositions. The link-only main retains actual
factory/provider/request and, when selected, static kernel paths. It is not a
physical test application or evidence of scheduler/hardware execution.

The resulting official startup must retain the MMIO-only `ExitRun0Mode` before
`SystemInit` and primary DATA/BSS initialization. Check the actual ELF/disassembly
using `scripts/ci/stm32h743.py`. Retention and resource gates consume the same
ELF/generated bundle. Unsupported cache/DMA/PLL/STOP/handoff modes stay rejected.

Run the maintained four-image graph with an absent output directory:

```sh
python scripts/ci/stm32h743.py --root . --compiler arm-none-eabi-gcc \
    --output /absolute/fresh/h743-software-reference --parallel 2
```

The runner executes every build and records raw status/streams, actual Ninja
dependencies, input/source/tool identities, budgets and selected/empty retention.
It checks exact board/part/clock/CPU facts, vector entries and the whole pre-DATA
call closure. Reviewed opcode shapes bind machine bytes; Thumb BL targets are
decoded from the instruction, including relocations. Unreviewed code generation
requires a new independent review. The output remains software evidence; a dirty
checkout cannot establish a clean candidate and no physical tests are implied.

The existing `--elf image.elf --disassembly executed-objdump.txt --output report.json`
mode checks the bounded power leaf alone, including a standalone mutation fixture.
It does not substitute for the full matrix or for a hardware power observation.
