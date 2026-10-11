Timer and PWM Support Boundary
==============================

The platform maintains specific time sources: STM32 startup/HAL tick and GD32
reserved TIMER1 timestamp timebase, plus scheduler SysTick. These owned resources
are not generic application timer/PWM providers. Native timer simulation does
not qualify PWM waveform or motor/actuator control.

STM32F407/GD32F470 generic timer/PWM configuration and typed providers are not
implemented. Future routes need explicit clock/IRQ/channel/pin ownership, safe
levels, cancellation/settlement and real firmware/hardware evidence. Product
control, actuator limits and budgets belong in external applications.
