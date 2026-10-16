# H743 actual ARM assembly fixtures

The empty and GPIOB PB0/USART3 IRQ-byte TOMLs select one exact software reference,
with explicit budgets and no product task or Flash partition. Baremetal and
FreeRTOS are separate ABI/build compositions. The link-only main retains actual
factory/provider/request and, when selected, static kernel paths. It is not a
physical test application or evidence of scheduler/hardware execution.

The resulting official startup must retain the MMIO-only `ExitRun0Mode` before
`SystemInit` and primary DATA/BSS initialization. Check the actual ELF/disassembly
using `scripts/ci/stm32h743.py`. Retention and resource gates consume the same
ELF/generated bundle. Unselected PLL/STOP/handoff modes stay rejected; selected Cache and DMA
compositions have independent fixtures below.

Run the maintained26-image graph with an absent output directory:

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

The `rx*.toml` fixtures select exact USART3 `rx-dma-idle`: DMA1 Stream1,
DMAMUX1 Channel1/request45 and caller-owned normal-mode blocks. The `rx32`
variants select HSI32; ordinary `rx` selects HSI64. Cache-off/on and
baremetal/FreeRTOS are separate compositions. The link-only main retains
`nx_stream_initialize`, explicit RX start/stop, independent IRQ TX and the actual
generated Stream1 vector. Complete-line buffers are aligned to 32 bytes even in
the cache-off reference, keeping the fixture valid under either reviewed cache
profile. Actual per-profile builds prove linking, budgets and retention; they
do not prove physical RX timing, coherency, RTOS execution or no-gap reception.


The `peripherals*.toml` images add exact SPI4/I2C1/ADC1/TIM3/EXTI/IWDG1/Flash
software reference modes on the explicit non-PCB Board package. HSI32/64 and
baremetal/FreeRTOS each retain nine concrete faces including the SPI CS face and
two EXTI lines. These images establish construction/link/budget evidence only;
physical bus traffic, ADC accuracy, timer output and Flash pulses are unobserved.

`rx-sram1.toml` and `rx-sram1-cache.toml` explicitly budget D2 SRAM1 and place RX
payloads there while retaining control metadata in AXI. Before primary DATA/BSS,
reset enables the D2 SRAM1 gate, verifies it and initializes DATA/BSS from exact
linker bounds. Both Cache profiles reject borrowed SRAM1 when its live gate is
off. Backpressure preserves outstanding payloads; normal-mode service/rearm can
have gaps and makes no continuous no-loss guarantee.

SPI fixtures preserve ES0392's CRCSIZE field and wait independently for the last
SCK edge after EOT. I2C applies master BERR and reloaded-RX errata. ADC preserves
all six linearity handshakes and explicitly qualifies VDDA. Flash requires fresh
erase provenance; reading FF does not establish virgin ECC, and all-FF input is
a pulse-free no-op. See the SoC contract for exact selected modes and exclusions.

这些均是软件参考构建，不能等同于实板验证。外设组合覆盖两个HSI频率和两个OS后端，
可选SRAM1组合覆盖Cache关闭和打开。启动先证明电源和旧硅片AXI勘误条件，再初始化
显式选择的SRAM1；DMA借用前检查实际时钟。背压不覆盖已借出数据，但正常模式重启
仍可能有间隙。所有HIL状态保持`not_executed`。
