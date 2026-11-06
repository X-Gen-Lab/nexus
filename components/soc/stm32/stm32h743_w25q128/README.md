# H743 W25Q128JV single-line reference

This optional software reference uses the existing mapped-storage owner and its
caller-owned slots, requests, clock and backend state. It targets the EF4018
Q/N/JQ identity from Winbond W25Q128JV Rev M, 24 December 2024. EF7018 M/JM parts
require their separate DTR profile. No Board wiring or physical qualification is
established by this component.

The first profile uses mode 0, single-line 03h reads, 02h page program and 20h
4 KiB sector erase at at most 16 MHz. Program requests split at 256-byte page
boundaries, require erased input locations and verify written bytes. Erase
completion also verifies the sector contents. Each owner service advances one
command or bounded FIFO step; verification uses a 16-byte private buffer.

The SoC cold owner must acquire and preserve the actual QUADSPI clock and exact
pin leases before preparation. Both CPU caches remain disabled, the QUADSPI NVIC
remains disabled and inactive, and every external CPU/DMA/XIP user is excluded.
Startup must already enable the MPU with HardFault/NMI coverage. The cold owner
supplies two exclusive MPU region claims, with the readable region at higher
priority. Both regions must initially be disabled. The adapter installs a
256 MiB no-access/XN guard over the complete QUADSPI alias window and permits
only the verified 16 MiB CPU aperture as read-only/XN/noncacheable while mapped.
It rejects foreign higher-priority aperture overrides and never changes global
MPU control. MPU protection does not join DMA, debugger or other bus masters.
The supplied kernel frequency and clock domain remain stable. Preparation waits
at least 5 ms before discovery, assuming flash power is already stable. There is
no default PCB pinout, external device power control or implicit recovery reset.

Only CPU read loans are advertised. The last release must withdraw the mapped
aperture; a failed release retains its loan. Cancellation stops future mutating
commands and finishes or retains an already issued page/sector operation.
Deadline, a HAL callback, and HAL READY alone do not prove drain. An uncertain
controller, flash BUSY/WEL, interrupt or buffer reference keeps storage borrowed.
Final release removes readable MPU permission and fences the CPU before
controller abort. On failed release the old view must never be used again;
consumers stay joined while cleanup is retried, and new admission is closed.
Map failure must prove deny and rollback or enter the retained platform fault
path. Stop restores the cold descriptors and selector after controller
withdrawal; refused restoration retains ownership.
The generic owner must stop and its consumers/controls must join before the
private controller stop releases its state. Clock and pin release remain the
explicit SoC cold owner's responsibility.

The adapter reuses complete official HAL command, transmit/receive interrupt,
IRQ dispatch, mapping and abort functions. HAL peripheral interrupt facts are
polled by the serialized task; no NVIC interrupt, worker or MDMA is enabled.
MDMA source is linked only because the official QSPI IRQ implementation references
its abort entry point. Its presence grants no DMA capability.
This is an H743 Platform component, not a Runtime-only HAL package. Consumers use
`nexus_add_firmware` or equivalently provide the selected real SoC startup and
clock owner. A private supplier prelude reads the actual System `core_hz` for
the official MDMA clock dependency; it creates no duplicate global clock or
guessed peripheral frequency. Vendor and construction headers remain private.

Reviewed SDK gaps require bounded Nexus C:

- HAL initialization replaces a reset handle's timeout with 5000 ms before a
  BUSY wait. The cold adapter instead validates reset/idle hardware and writes
  only the selected CR/DCR fields with readback, using official CMSIS masks.
- HAL command and mapping functions still wait internally despite their IT
  names. The private handle uses timeout zero and independent idle admission.
  HAL abort/READY and deinitialization do not establish complete source removal;
  the adapter separately checks BUSY/ABORT/FIFO, mode/source withdrawal, pending
  IRQ and retained HAL pointers before allowing final release.
- Official command configuration does not verify the address before an
  irreversible command. The adapter first disarms the old CCR, stages and
  reads back the address, then invokes the complete HAL function and checks its
  result. A normal RX callback can still leave an incomplete count, and a
  simultaneous transfer error can lose priority to completion in HAL dispatch;
  exact remaining counts and error flags precede parsing any response.

The immutable datasheet and official HAL source audit are recorded in the
delivery evidence. Cache, arbitrary MPU policy, quad/QE changes, mapped DMA, XIP, erase
suspend, status/protection writes and power-loss durability are outside this
first profile. Test models and ARM links do not establish electrical behavior.
