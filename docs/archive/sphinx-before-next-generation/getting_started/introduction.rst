Introduction
============

Nexus is the reusable platform for industrial embedded software: CPU primitives,
HAL ownership/typed operations, OSAL backends, SoC/Board integration, common
components and traceable build/evidence tooling. Products and applications own
main, workers, domain logic, partitions, trust/manufacturing and recovery policy
in external repositories.

The maintained software ports are Native host models, STM32F407VE/VG/ZG and
GD32F470ZG. Four reference Boards support baremetal/FreeRTOS software profiles.
Typed MCU I2C/UART DMA, generic ADC/PWM/CAN/Ethernet, MCU boot/crypto/vault and
MPU/cache are not supplied by silicon names or vendor headers. FreeRTOS POSIX
execution and Native models do not qualify ARM electrical/timing behavior.

Use :doc:`quick_start` for platform contracts and
:doc:`first_application` for external consumption. The separate
`nexus-examples repository <https://github.com/X-Gen-Lab/nexus-examples>`_
contains actual eight application consumers with fixed SDK identity.

The user has deferred boards. Software and relocatable source SDK checks are
source-bound records; physical startup, IRQ/DMA, power loss, long-load and
enterprise/LTS qualification remain separate. Current evidence and known limits
are maintained in the root README and ``docs/strategy/support-matrix.yaml``.
