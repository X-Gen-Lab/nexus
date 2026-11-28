# STM3210E-EVAL SRAM reference

The MB672-F103ZGT6-D06 board connects U1, an IS61WV51216BLL-10MLI
512K × 16 asynchronous SRAM, to FSMC NE3. A 1 MiB CPU data window begins at
`0x68000000`. The board routes PE0/PE1 to BLE/BHE for byte and halfword
accesses. The assembly claims the 40 SRAM pads and holds the three sibling
device selects high at PD7, PG9 and PG12 while it owns the shared bus.

The approved HSI8 System supplies a nominal 8 MHz HCLK. FSMC Mode 1 uses
one read/write timing, expressed as RM0008 §21.5.4 register fields.
The fixture programs ADDSET=1, ADDHLD=1, DATAST=3 and BUSTURN=1. The
address phase is two HCLK cycles. The second phase is four cycles for
writes and six for reads.
These settings give ample margin over the specified 10 ns SRAM speed
grade. The exact voltage and physical timing remain to be measured.

Use the typed `work_sram` factory reference, acquire a zeroed
`nx_memory_window_lease_t`, and use direct CPU loads and stores within its
extent. Join all pointer users before releasing the original lease and its
epoch. A live lease prevents bank withdrawal. Stop restores the owned bank,
pads and clock in order. Failed cleanup retains ownership for a retry.

The fixture runs acquisition, geometry and release. A debugger-controlled
probe is compiled for byte and halfword access. It defaults to disabled.
This profile is for data in an exclusive CPU lease. Startup memory, DMA,
executable memory, allocation, NOR, NAND and display modes require their
own admission.

[ST UM0488 Rev7](https://www.st.com/resource/en/user_manual/um0488-evaluation-board-with-stm32f103zx-mcu-stmicroelectronics.pdf)
· [MB672 D06 schematic, sheet 6](https://www.st.com/resource/en/schematic_pack/mb672-f103zgt6-d06_schematic.pdf)
· [ISSI SRAM datasheet](https://www.issi.com/WW/pdf/61-64WV51216.pdf)
