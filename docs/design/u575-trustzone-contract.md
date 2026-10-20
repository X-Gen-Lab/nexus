# U575 dual-world GPIO reference contract

This is a software reference for an explicit Secure and NonSecure image pair on
STM32U575 with 2 MiB Flash. It is separate from the existing TZEN0 platform
profile. It does not turn that profile into a TrustZone provider, integrate a
NonSecure RTOS, authenticate firmware, or establish physical isolation evidence.
No board has executed this reference yet.

## Ownership and assembly

`soc/stm32u575/security.c` implements the cold-owner mechanism. Its private
`nx_stm32u575_tz_layout_t` contains caller-selected spans, not a platform-owned
product partition. The owner lives in Secure SRAM1 and remains allocated until
reset. The boot owner supplies mutually consistent linker scripts, vectors,
Secure fault handlers, partition parameters and image-loading policy.

`tests/contracts/u575_trustzone` is the explicit reference assembly. It owns the
example partition, a single fixed NonSecure-world principal and a PC7 GPIO grant.
Its two Runtime builds produce separate ELFs, a real SG veneer and Secure import
library, and an actual compiler-generated NonSecure call. The reference does not
use the TZEN0 SoC startup or claim generated factory integration.

Cold preflight performs observations without owner updates or configuration
writes. This does not promise that all hardware reads are side-effect free:
reading SysTick CTRL observes and clears COUNTFLAG. The trusted cold contract
starts with no running timer. Preflight rejects incompatible geometry, context, option
bytes, Flash permissions, active interrupts, autonomous bus masters, caches and
floating-point ownership. It requires Secure privileged Thread mode, MSP within
the supplied Secure SRAM1 stack, PRIMASK set, and BASEPRI/FAULTMASK clear. The
Secure vector table must already be at the Secure bank1 address. A previously
running world or a bootloader-owned peripheral configuration is not adopted.

After admission the owner records FAILED and effects=true before enabling the
GTZC/SYSCFG clocks. Any later failure keeps that state and requires reset; there is
no rollback, retry or release promise. Only complete configuration and readback
publish READY. Successful entry publishes RUNNING before the actual CMSE
transition. The production transition never returns; an unexpected NS return
enters terminal fault handling.

## Attribution and memory

The mechanism accepts page-aligned NS Flash within bank2, superblock-aligned NS
SRAM within SRAM3, NSC within Secure bank1 outside its first vector page, and a
Secure stack within SRAM1's Secure alias. At least one SRAM3 superblock remains
Secure so its clock/reset attribution remains Secure with INVSECSTATE clear.

The current hardware admission is intentionally narrow: TZEN=1, SWAP_BANK=0,
2 MiB Flash, SRAM3 ECC disabled, bank1 fully Secure, Secure boot address
0x0c000000, no hide protection, no security inversion, Flash programming locked
and idle, and the selected bank2 pages neither Secure nor privileged. In
particular, SRAM3_ECC=1 means ECC disabled. The mechanism never writes option
bytes, erases Flash, or programs images. Provisioning must establish this boot
contract before execution; a late register check cannot retroactively protect
instructions already executed.

After verifying actual SRAM1/SRAM3 security and privilege configuration, the
mechanism zeroes the future NS RAM through its still-Secure alias and reads it
back before declassification. It then makes only the selected SRAM3 superblocks
NonSecure and unprivileged. SRAM1 remains Secure and privileged. It locks the
implemented SRAM1 and SRAM3 superblocks and the corresponding global MPCBB
configuration until reset. CMSE checks alone do not establish these bus-firewall
permissions: GTZC unprivileged denials may read as zero or ignore writes.

The SAU enables exactly three spans: NS Flash, NS RAM and NSC at its Secure Flash
alias. All remaining addresses retain Secure attribution. The NS MPU provides
read-only executable Flash and read/write execute-never RAM, with no privileged
default map or peripheral region. MAIR uses normal non-cacheable memory. Unused
regions are disabled. Actual MPU TYPE is read: this mechanism accepts 8 or 16
regions and clears all observed regions, rejecting other counts. This is not a
claim that every U575 revision implements 16 regions: RM0456 states eight,
whereas DS13737 describes up to sixteen. The existing TZEN0 provider separately
maintains an exact eight-region CPU tuple. Protected FreeRTOS preserves its
exact comparison with hardware TYPE and rejects an actual sixteen-region
configuration rather than silently interpreting the field as a lower bound.
The no-OS dual-world mapper's observed region count does not change that contract.

NSC instruction fetch does not require an additional NS MPU region. Arm
DDI0553B.z `ValidateAddress()` selects the MPU for instruction fetch from the
resolved security attribution: `secureMpu = !sAttrib.ns`. An NSC address has
Secure attribution, so its SG fetch is checked using the Secure MPU and Secure
privilege state. This reference requires the Secure MPU disabled and Secure
Thread privileged; it does not grant NS ordinary data access to NSC.

After readback, SYSCFG locks NS VTOR, the documented NS MPU controls, SAU and the
Secure VTOR/AIRCR security routing. RM0456 explicitly lists MPU_CTRL_NS,
MPU_RNR_NS and MPU_RBAR_NS for LOCKNSMPU; it does not explicitly list every MPU
alias, RLAR or MAIR register. The contract therefore does not treat that bit as
proof that all MPU words are immutable. Final admission rechecks MAIR, the two
active regions through the base/alias registers, controls, locks, physical SRAM
permissions, Flash permissions and quiescent interrupt/master state without
rewriting a locked region selector.

The remaining privilege boundary is mandatory: NS enters unprivileged Thread
mode with no NS kernel or privileged exception handler. The immutable NS vector
table contains only MSP and Reset; every other implemented exception vector is
zero. All interrupt target bits and fault routing remain Secure. NS cannot use
an SVC/IRQ handler to alter attribution. Secure NMI/fault handlers fail stop, and
Secure software never changes mappings or enables DMA after the cold seal.

## Synchronous gateway and borrow boundary

The reference SG transport checks an actual NonSecure caller, Thread context,
Secure privilege and RUNNING ownership. Identity is the fixed NS world, not a
claimed task ID from untrusted input. It preserves and masks NS interrupts
through the synchronous call; the Secure broker retains its own documented
masking and serialization rules.

The memory port first restricts a request to the explicit NS RAM span, then uses
`nx_arch_nonsecure_check` with real CMSE NonSecure and unprivileged MPU checks.
The broker copies a bounded request into Secure storage before acting, checks
the fixed GPIO grant, and holds the output mapping until its final copy.
The permission check itself is only an observation: it neither copies nor pins
memory. The reference's synchronous pin relies on the already established
static mapping, lack of NS concurrent execution/handlers, lack of autonomous
masters, and exclusive Secure owner. It does not authorize asynchronous loans,
DMA buffers, arbitrary MMIO, callbacks or mutable user address spaces. Retained
cleanup or an impossible mapping-release failure is terminal.

No security property is inferred from a host pointer model or the presence of
SG/BLXNS opcodes alone. Host Google tests establish software admission and
failure behavior; real ARM ELFs establish ABI/link/layout properties. Hardware
must still verify attribution, fault delivery, CMSE observations, lock behavior,
GPIO effect, provisioning and the exact board revision.

## Copied cooperative GPIO jobs

`nx_reference_gpio_batch` is a second narrow SG entry. Its fixed76byte scalar
input and16byte reply must be fully aligned, disjoint spans within the sealed NS
RAM mapping. The gateway checks arithmetic overflow and complete extents, then
performs real CMSE NonSecure unprivileged read/write checks while preserving
the incoming NS interrupt mask. It copies input into Secure storage before
admission and copies a reply only after a successful operation. No NS address
survives return. The existing static-world, no-IRQ, no-DMA and sealed-attribution
preconditions remain mandatory.

One static Secure slot stores at most eight granted PC7 set/clear commands.
Admission copies and validates the complete list without GPIO effects. SERVICE
executes at most one command, POLL only copies latched status, CANCEL settles
future commands while previous writes remain effective, and CONSUME releases
only a terminal job. Replacing running or unconsumed work is rejected. Tickets
use monotonically increasing32bit epochs; UINT32_MAX exhausts admission and
never wraps to make an old ticket valid. A GPIO error latches FAILED and the
completed prefix; a caller polls that same ticket before consumption.

This copied cooperative scenario creates no user-buffer loan, hidden worker,
callback, clock or interrupt source. It provides no execution interval or
gap-free waveform guarantee. It does not establish Secure UART, arbitrary DMA,
NS IRQ, NS RTOS or general asynchronous TrustZone support. Production GoogleTest/
GoogleMock contracts exercise copying, one-step progress, stale tickets, slot
backpressure, cancellation, errors, span boundaries and alias rejection. The
paired ARM gate verifies both real SG veneers, both exact import symbols and
the Secure implementation addresses. Physical HIL remains not executed.

## Source basis and validation

The implementation uses locked STM32U5 CMSIS v1.4.3, commit
`624374fa1e21ca195d6f2102ac0caaa50d0ea4c8`, and the repository's locked Arm CMSIS.
Authoritative register review used ST RM0456 revision 7:

- Sections 3.5.2 and 3.7.1: banked MPU and documented region count.
- Sections 5.4.2, 5.4.4, 5.4.6 and 5.8.1–5.8.5: SRAM granularity, privilege,
  security aliases, clock attribution, MPCBB inversion and reset-only locks.
- Sections 7.5 and 7.9.13: Flash attribution and SRAM3 ECC option polarity.
- Sections 7.9.16–7.9.17 and 7.9.29–7.9.34: Secure boot address, watermarks,
  Secure/privileged Flash block configuration and the exact U575 bank geometry.
- Sections 11.5 and 13.3.18: RCC/GPIO security ownership.
- Sections 15.3.4–15.3.5: CPU NonSecure/Secure configuration lock scope.

The reviewed official PDF is
<https://www.st.com/resource/en/reference_manual/rm0456-stm32u5-series-armbased-32bit-mcus-stmicroelectronics.pdf>,
SHA-256 `6edaaaae0c5d57c58ffc3a576d44e7b62e8d9cd0e495e8f61bab688226fc327c`.
The instruction-fetch rule was independently reviewed against Arm DDI0553B.z
(2026-02-24), E2.1.419 `ValidateAddress()` pages 2087–2089 and E2.1.296 `MemI[]`
page 2044. Official PDF:
<https://documentation-service.arm.com/static/699eb70d77ad5021756604d8>,
SHA-256 `468594b22cf5a4bae1261f2cd39d5aa51040fbb566be18cc358c169918fd7a72`.
The actual compiler CMSE helper may contain architecture-defined FP context
instructions even in this soft-float image pair. Admission rejects active FP
ownership (both CONTROL.FPCA and CONTROL.SFPA), CPACR enablement and lazy
preservation; this is not a claim that the ELF contains no
FP-related opcodes. DDI0553B.z C2.4.368 (VLSTM, pages 1177–1178) specifies that
SFPA=0 makes that instruction a no-op; SFPA=1 would require coprocessor access.
Therefore SFPA-only state is explicitly rejected before acquiring hardware.

The Google suite compiles the production mapper and substitutes only hardware
MMIO, physical RAM and CPU instruction leaves. It covers rejected admission,
actual permission layout, every changed configuration-write failure, zeroing
failure, irreversible ownership, vector/stack boundaries, forbidden privileged
handlers, metadata changes and final transition ownership. The paired ARM gate
checks the actual vector words and executable addresses against this contract.
