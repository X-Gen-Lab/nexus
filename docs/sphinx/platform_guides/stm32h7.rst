:orphan:

STM32H7 Status
==============

STM32H7 is not a maintained selectable Nexus platform. Previous draft examples,
Kconfig choices and vendor headers did not establish a complete implementation.
Unmaintained platform/configuration shells have been removed.

Use :doc:`stm32f4` for maintained STM32F407VE/VG/ZG integration. An H7 port requires
its own startup/vector, clocks, memory domains/cache/MPU, DMA ownership, reviewed
Board resources, exact SDK dependencies and real software/hardware evidence.
No F407 or Native result qualifies Cortex-M7 behavior.
