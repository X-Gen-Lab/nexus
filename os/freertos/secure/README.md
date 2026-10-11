# Explicit Secure companion

`nexus_add_runtime(ASSEMBLY ...)` with explicit v8-M Secure CPU facts and the
baremetal backend establishes the existing production CPU/Arch graph. Call
`nexus_enable_secure_context()` and link `Nexus::SecureContext` into that Secure
image. This adds context gateways and the exact locked Secure context assembly;
it does not start another kernel. The Nonsecure image selects `[os]` with
`security_model = "split"` on matching Nonsecure CPU facts and FreeRTOS.

CPU instruction/FP/MVE/float ABI must match across the two images. Secure and
Nonsecure startup, vectors, memory attribution, image entry, interrupt targets,
and the NSC region are explicit application/SoC SDK responsibilities. Export a
CMSE import library from the Secure link and consume those veneers in the
Nonsecure link; the software reference fixture demonstrates the actual process.
No board memory or security routing is synthesized.

Split mode selects the actual locked `ARM_CM{23,33,55,85}/non_secure`
TrustZone-aware ports. The `ARM_CM*_NTZ` assembly omits Secure save/load and
allocation; enabling a configuration macro does not turn it into a split port.
The Idle task also acquires a Secure context in the pinned kernel. Supply its
`vApplicationGetIdleTaskMemory()` callback with an explicit static NS TCB/stack,
and compose matching Secure metadata/stack in the immutable selector. The
`secure_idle_stack_bytes` policy controls its request budget (default 128 bytes)
without supplying storage. Unknown or unprepared identities fail closed.

Prepare caller-owned Secure metadata and an aligned Secure stack (including an
eight-byte seal) before exposing an NS task. The trusted immutable
`nx_freertos_secure_context_for_task()` selector returns only those prepared
records. No fallback heap, default maximum pool or dynamic object discovery
exists. Each context uses a monotonically increasing lease epoch; the compound
identity is `(trusted task, epoch)`. Free retains metadata and epoch history;
rebind after task join never resets it. Epoch exhaustion rejects a new lease.

The pinned NS SVC/PendSV code is trusted. NS Thread calls cannot enter these
Handler-only context operations. The mechanism does not protect against an
adversarial privileged NS kernel forging its own task arguments. Applications
requiring that stronger boundary need an independently reviewed Secure policy.
User-MPU tasks and split-world contexts are initially separate profiles; their
combination is rejected until its shared SVC/CONTROL/ACL contract is qualified.

Context save/load uses the locked real PSP/PSPLIM assembly, including lazy FP/MVE
completion. Secure and Nonsecure PRIMASK values are saved and restored exactly.
Only one CPU's currently loaded PSP is tracked; this is an active borrow, not a
context pool. Loaded contexts cannot be freed. Broken seals, out-of-range stacks
and corrupt CPU context retain the lease and enter the explicit Secure fail-stop.
NMI/HardFault must not access this metadata.

The 64-byte port minimum excludes the eight-byte seal and is not an application
stack budget. Budget actual Secure call depth, FP/MVE exception frames, MSP,
interrupt nesting and diagnostic handling separately. Host tests establish lease
software contracts; two-image compiler/link evidence establishes the gateway and
context code; neither establishes physical attribution or stack peaks. HIL is
required on a real split-world MCU before product acceptance.
