# Real split-world compiler/link contract

The fixture composes two independent production Runtime images. The Secure image
uses explicit Secure CPU facts, a baremetal runtime and `Nexus::SecureContext`.
The NS image uses matching Nonsecure facts and the `split` FreeRTOS profile.
Secure linking emits seven distinct SG veneers in `.gnu.sgstubs` plus the actual
CMSE import object; NS linking
consumes that object and retains the real SVC/PendSV context paths.

Both the example task and Idle task have caller-owned NS TCBs/stacks and distinct
prepared Secure context metadata/stacks. The immutable Secure selector uses the
two explicitly paired synthetic TCB addresses. Neither image supplies a default
Secure context pool or a heap.

The ROM/RAM/NSC ranges and static TCB identities here are synthetic software facts.
There is no SAU/IDAU/vector/interrupt-target initialization, external NS transfer,
board startup or hardware execution. A real SDK supplies those electrical and
memory attribution contracts; hardware acceptance remains pending.
