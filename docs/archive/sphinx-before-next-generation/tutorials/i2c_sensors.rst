I2C Support Boundary
====================

Native typed I2C models parent/child ownership, unshifted 7-bit addresses,
transaction queues, original deadlines, cancellation and terminal callbacks.
Its tests validate behavior and stale-owner rejection.

STM32F407 and GD32F470 modern typed hardware I2C providers are not implemented.
No sensor wiring or repeated-start timing is qualified. Vendor SDK I2C symbols,
legacy getters, draft configuration or Native success cannot enable production
MCU I2C. A future hardware port needs correct controller/state handling, reviewed
Board pins/electrical constraints, meaningful fault tests, real firmware links
and separate physical evidence before a sensor application is advertised.
