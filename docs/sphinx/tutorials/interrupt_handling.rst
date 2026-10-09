Interrupt Context and Admission
===============================

Arch exposes saved CPU-local PRIMASK, IPSR/mask queries and barriers. FreeRTOS
BASEPRI/syscall critical sections have a separate contract. Short metadata locks
never call vendor routines, block, allocate or deliver user callbacks.

Only explicit supported FromISR operations may run at permitted priorities.
Current FreeRTOS syscall threshold is logical priority 5; more urgent IRQs cannot
call those APIs. Board/controller resources bind actual IRQ/priority/AF/DMA.
Generic MCU GPIO EXTI and arbitrary route solving are unsupported.

Platform shutdown checks provider ownership and IRQ/DMA resources under an
exclusive admission fence. PARTIAL cleanup blocks new resource admission while
original owners settle. The private platform hook is not a product lifecycle API.
Native IRQ/DMA dispatch is a host model, not a physical ISR/preemption test.
Physical latency, nesting, wire TC and DMA races need board-identified HIL later.
