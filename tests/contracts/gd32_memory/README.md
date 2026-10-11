# Optional GD32F470 memory contract

This consumer uses the production CMake graph, reviewed Liangshan SoC facts and
one authored assembly per image. It does not execute an application on silicon.
The exact GD32F470ZGT6 has primary SRAM 192 KiB at `0x20000000`, DMA-accessible
ADDSRAM 256 KiB at `0x20030000`, and CPU data-only TCM 64 KiB at `0x10000000`.
The main MSP and ordinary storage always remain in primary SRAM.

`default.toml` selects neither extra region. The other three inputs explicitly
select ADDSRAM, TCM or both with independent 128-byte budgets. Each selected
region has one live initialized word, 16 bytes of BSS and 16 bytes of NOINIT.
Compiler-emitted unused variables in each placement class must disappear through
linker GC; no `KEEP` may retain them. NOINIT is initialized by its caller before
any read. It makes no reset or power-loss retention guarantee.

GoogleTest executes the real bound-checking initializer and real SystemInit in
a register/address model. ARM ELF checks establish actual placement, Flash load
bytes, section types, startup references, budget refusal and GC. They do not prove
physical memory timing, power behavior, DMA access or startup on the development
board. Physical execution remains `not_executed`.

The reviewed manuals and hashes are recorded in the GD32 provider README. The
family memory diagram is a maximum; the ZG density controls the actual additional
256 KiB. TCM is never an instruction/ramfunc or DMA buffer domain.
