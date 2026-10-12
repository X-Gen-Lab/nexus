# U575 dual-world software reference

This is an explicit application integration fixture for STM32U575ZIT6Q. It uses
the maintained CPU Runtime, U575 cold security owner, Arch CMSE range check,
scalar user I/O broker and actual GPIO provider. It does not modify the platform's
existing TZEN=0 references or choose partitions for external products.

The Secure and NonSecure images are separate CMake builds and linker layouts.
The Secure build creates the actual SG import object; the NonSecure build links
that exact object. Both use Cortex-M33 with soft float and no FP ownership.
The toolchain CMSE helper retains its architecture-defined conditional FP
context instructions; ordinary reference code and NonSecure callers use no FP.
The Secure launch crosses through the compiler's real CMSE NonSecure call ABI.
The test gate inspects actual ELF/LOAD/vector/SG/import bytes and BLXNS code.
There is no physical station execution or software candidate promotion here.

| Owner | Exact reference extent |
|---|---|
| Secure Flash bank 1 | `0x0c000000` through `0x0c0fdfff` |
| Secure NSC veneers | `0x0c0fe000`, 8 KiB |
| Secure RAM and stack | SRAM1 Secure alias; final 4 KiB at `0x3002f000` |
| NonSecure Flash | Bank 2 first 64 KiB at `0x08100000` |
| NonSecure RAM | SRAM3 first 16 KiB at `0x20040000` |
| NonSecure stack | Last 2 KiB, initial MSP `0x20044000` |

The external boot owner and station must establish TZEN=1, SWAP_BANK=0,
appropriate Secure boot address/ownership, the reviewed Flash watermark/block
attribution and SRAM3 ECC option state before execution. The narrow mapper
observes option bytes and rejects incompatibility; it does not program them.
Use the precise private mapper contract and current device reference manual for
provisioning. No fixture command flashes or rewrites option bytes automatically.
The external image/boot owner must erase and verify the whole reserved NSC
extent, including unused bytes, so sparse programming cannot retain old SG
entries. The byte gate does not authenticate unprogrammed device Flash.

Only one unprivileged NonSecure Thread exists. It has no kernel, IRQ handlers,
DMA or FP ownership; all 140 vector words after MSP/Reset are zero. Secure
launch requires NS Reset after the reserved first 1 KiB of NS Flash; the
NonSecure linker and byte gate enforce this same boundary. Secure
fault/NMI handlers park. NonSecure code initializes its own ordinary RAM and
calls one fixed-principal scalar GPIO gateway. It cannot choose a principal,
peripheral pointer, callback or asynchronous loan. The reference grants only
PC7 and verifies that its GPIO security bit remains Secure before use. PCB
revision, LED route and electrical observations remain HIL responsibilities.

The gateway checks the actual NonSecure caller and Thread context, preserves
Secure interrupt state, and saves/masks/restores PRIMASK_NS around its complete
synchronous transaction. The broker's pin port checks both the exact SRAM
window and real CMSE NonSecure unprivileged MPU access. A pin holds the sealed
static mapping for this synchronous invocation; it is not a claim that a range
check alone creates a lifetime guarantee. Attribution cannot change in RUNNING,
no NS privileged exception can remap it, and no DMA or asynchronous caller is
admitted. A retained cleanup failure parks without returning control or loans.
The U575 owner also verifies the reset-only SYSCFG locks for the NS MPU/VTOR
and Secure SAU/VTOR/AIRCR before admitting NonSecure execution. The Secure reset
sets its own VTOR and MSPLIM before ordinary initialization or interrupt unmask.
NS MPU maps only its Flash and RAM. ARMv8-M checks an instruction fetch in
Secure NSC against the Secure MPU and Secure privilege; it does not require
granting the NS MPU access to Secure Flash. See the maintained
[U575 contract](../../../docs/design/u575-trustzone-contract.md).

Build and inspect with the locked ARM toolchain in PATH:

```
python -B scripts/ci/u575_trustzone.py --output NEW_OUTPUT_DIRECTORY
NEXUS_U575_PAIR_BUILD=NEW_OUTPUT_DIRECTORY \
    python -B tests/contracts/u575_trustzone/test_gate.py
```

The output records actual commands, source/tool identities, both original maps,
the SG import object and both ELF images. Mutation regressions use those real
ELF bytes. These checks prove software linkage and declared boundaries, not MCU
execution, secure boot authentication, physical isolation or certification.
The current single-ELF candidate schema cannot seal this pair as one qualified
unit. Pair sealing needs an explicit reviewed multi-image artifact contract.
