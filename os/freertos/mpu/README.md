# Static MPU user tasks

MPU support is an explicit FreeRTOS policy (`[os].memory_protection = true`),
independent of ordinary privileged OS adapters. The default remains disabled.
CPU facts must declare the actual MPU model and region count. The maintained
kernel tuples are M3, integer/FP M4, and M23/M33/M55/M85 with their reviewed
integer/FP/MVE facts. M3/M4 require eight regions; the reviewed v8 ports support
eight or sixteen. Native, M0/M0+, M7 MPU, and MPU combined with split TrustZone
kernel security are rejected. This does not change their ordinary Arch/OS support.

`freertos_mpu.h` is a privileged, backend-specific ownership API. The caller owns
and zero-initializes the complete `nx_freertos_mpu_task_t`, containing the real
`StaticTask_t` and its embedded saved context and protected syscall stack. There
is no heap, task registry or maximum kernel-object pool. The caller also owns
user stack/data and the kernel idle TCB/stack supplied by its strong
`vApplicationGetIdleTaskMemory`. Missing that callback fails at the actual link.

One privileged Thread owner serializes start/delete. Start rejects ISR, suspended
scheduler and manual masks; the exact maintained pre-scheduler kernel mask is
accepted for bootstrap. The minimum user stack is 64 words. This admission floor
is not an execution-stack budget or a high-water measurement. MPU-v7 mappings must
have a power-of-two byte size and matching alignment; MPU-v8 mappings use exact
32-byte granules. Mappings are never widened. Data must be normal RAM with RO or
RW access and XN, entirely inside the reserved user RAM, separate from the stack
and every other mapping. Context spans must be inside the user stack, read-only
user flash or explicitly granted data. MMIO, private RAM, kernel code, overflow,
unknown access encodings and incomplete protected task storage are rejected
before kernel creation. A nonself deletion requires the owner to quiesce external
users first; the single-core static kernel removes its scheduler references
synchronously. Self deletion is rejected and the metadata loan remains live.

Unprivileged code includes only `freertos_user.h`. The service surface is relative
delay and the wrapping 32-bit kernel tick count, through two exact pinned naked
SVC veneers. No object handle or caller pointer crosses the boundary. The real
wrapper-v2 port checks the SVC origin, switches to its protected per-task syscall
stack, and restores privilege/context. The 70-entry pinned syscall table has only
slots 6 and 13 populated. The omnibus Common wrapper, its object translation pool
and every other veneer are excluded. Direct I/O, factory and ordinary OS services
remain privileged; a product owns any future validated user gateway.

The consumer linker is part of this security contract. All ordinary `.text*`,
`.rodata*`, `.data*`, `.bss*`, `privileged_functions` and `privileged_data` belong in
protected flash/RAM. Only explicit `.nexus_user_text` code and `.nexus_user_data`
RAM are user regions. User constants/helpers must also be explicitly reserved in
user flash by the consumer. `freertos_system_calls` occupies a distinct read-only
SVC region. Strong `__nexus_*` half-open reservations must match the exact
version-specific FreeRTOS linker symbols. `layout.c` verifies these bounds and
independently reads the actual MPU_TYPE region count. It has no weak host fallback.

`prepare_port.py` leaves the vendor tree unchanged. Complete C ports, v8 assembly
and the two retained veneer bodies are hash-checked independently. MPU-v7 uses
its four existing common regions for exact user RX flash, privileged flash,
privileged XN RAM and a distinct exact SVC RX window. Both RX windows retain the
pinned normal Flash memory attributes. The broad outer Flash and general-MMIO
user grants are removed. The enabled privileged background map still permits
privileged MMIO; unprivileged unmatched addresses receive no default permission.
No region, per-task context or kernel-object pool is added for these defaults.

The pinned special scheduler-start SVC (100 on v7, 102 on v8) receives the real
EXC_RETURN and must consume one protected four-byte bootstrap lease before the
original first-task restore can write MSP, MPU or CONTROL. The lease is armed at
the real scheduler entry before its hardware writes. Admission requires a
privileged Thread on MSP with CONTROL.nPRIV/SPSEL/FPCA all clear. Scheduler startup
after prior FP use is deliberately rejected in this maintained cold-start
contract; task FP/MVE context support remains selected by the actual port. The
SVC frame must be basic, aligned, entirely in protected RAM, returning to the
exact protected instruction label immediately following the real startup SVC.
It must originate from the actual world: F9 for v7 or v8 secure-only/single, B8
for the reviewed v8 nonsecure tuple. No cross-world encoding is accepted. Invalid,
unarmed and replayed START calls return before restore and preserve the lease.
Normal user yield and the two checked syscall entries retain their pinned paths.

Vendor drift, nonunique patch sites or unreviewed ports fail configuration before
emitting a new source. The exact two naked veneer bodies and their MIT license
are preserved. M23 MPU compiles its pinned inline assembly in GNU unified syntax.
Generated sources belong to the build directory.

The GoogleTest/GoogleMock fixtures use actual pinned v7/v8 port types and mock
only the external Arch, linker/hardware and kernel calls. A separate model
extracts the actual hash-checked derived special-SVC fallback and MPU-v7 setup
bodies; GoogleMock replaces only opcode, ISA and hardware/linker boundaries. It
checks raw user START refusal before restore and the actual MPU register writes,
including normal RX attributes and default rejection of Flash gaps/MMIO.
The software fixture
in `tests/contracts/os_mpu` uses the production Runtime CMake graph and synthetic
memory, not a board linker. `scripts/ci/os_mpu.py` builds real ARM ports, reads ELF
symbols and table bytes, checks code/data domains, requires the actual SVC
immediates, raw EXC_RETURN forwarding, protected startup label and guard-before-
restore instructions, records complete object/syscall-stack sizes, and executes the missing
idle-provider link rejection. Firmware is not executed. Real MPU faults, privilege
transitions, FP/MVE context under preemption and stack high-water behavior still
require the physical HIL station.
