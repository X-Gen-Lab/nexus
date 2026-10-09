# MCU register lifecycle models

These fixtures compile and execute the production SoC lifecycle implementation.
They do not replace `nx_stm32f407_clock_release`, the STM32 ISR manager/resource
gate, or any GD32 clock/timebase/resource helper with a return-value stub.

The STM32 target binds three production translation units: the idle clock
transition, the resource gate, and the ISR manager. The GD32 target binds the
whole production `clock.c` and `interrupt.c`, including clock validation, timer
initialization, overflow dispatch and teardown. Fake vendor/CPU ports model only
the register surface and deterministic hardware responses. Register bit values
come from the locked device SDK; peripheral selector IDs are local fixture IDs.

Register accessor evaluation advances the modeled oscillator ready flags and
SYSCLK status after a request. Injected ready/status failures stay stuck for the
production poll-count bound. The model also records attempts to stop the CPU's
active PLL. Vendor timer/NVIC functions mutate actual model register storage;
readbacks are not hardcoded successful constants. IRQ/DMA ownership gates compare
whole register snapshots before and after each busy response.

STM32 scenarios cover HSI release, repeated release, five oscillator/status/report
faults with cleanup retry, every implemented external IRQ enable/pending/active
bit, all sixteen DMA streams, actual ISR registration, shutdown fence admission,
ISR/masked registration rejection, nested pinned callbacks and disconnect during
callback/hardware activity. Callback execution checks that the Arch metadata mask
has been restored. The model provides the shutdown flag port; separate lifecycle
contracts execute the real HAL shutdown fence.

GD32 scenarios cover four real clock validate/release cycles, eight initialization
and release faults with recovery, every implemented IRQ bit including IPA and
TIMER1. The TIMER1 exemption is acquired by real initialization, retained after
partial failure, and removed only after successful cleanup. Unowned TIMER1
enable/pending is busy, unowned cleanup changes nothing, active TIMER1 is always
busy, and repeated initialization cannot reset a live or partial timebase.
Other checks cover all sixteen DMA channels, four timer teardown/restart
cycles, pending overflow/epoch and saved mask, five cleanup faults and timer
initialization readback rejection. Active TIMER1 rejection preserves all state.

These are host software models. They establish no crystal stabilization time,
PLL electrical qualification, DMA bus behavior, interrupt latency, physical
waveform, device reset safety or hardware-in-loop pass.
