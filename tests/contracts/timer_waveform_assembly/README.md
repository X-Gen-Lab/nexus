# Timer waveform ARM contract

These C11 consumers retain the exact generated factory face, shared operations,
private context and Platform lifecycle for STM32F407VE/ZG. The Board packages
explicitly describe silicon routes only. They establish no PCB connections,
break circuit, electrical idle levels or measured waveform/DMA timing.

Group updates, pulses and finite DMA sequences select separate personalities of
one physical timer. TIM2 remains the Core timebase. Synchronization uses only
reviewed internal trigger connections; callers arm slaves before enabling the
selected master. A declared DMA bus budget is conditional and requires hardware
measurement. Physical HIL remains `not_executed`.

`ve-peers.toml` selects new TIM8 and IRQ-free TIM12 group owners together. Its
consumer commits and starts both, stops TIM12 and checks that TIM8 remains
running before Platform withdrawal. Host MMIO tests execute that isolation;
the ARM consumer is linked and budgeted, without physical execution. The old
`complementary-pwm` cold owner retains its separate TIM9/TIM12 peer restriction.
