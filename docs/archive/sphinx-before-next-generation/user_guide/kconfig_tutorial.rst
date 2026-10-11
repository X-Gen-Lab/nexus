Configure a Maintained Consumer
===============================

Start from the matching maintained ``configs/*_defconfig`` and keep it in your
external repository. Choose a named preset or supply ``NEXUS_CONFIG_FILE``;
select compiler/toolchain before the first configure. One build directory owns
one Board/backend, so use separate roots for Native, baremetal and FreeRTOS.

.. code-block:: bash

   cmake --preset stm32-sky-armgcc-freertos-release
   cmake --build --preset stm32-sky-armgcc-freertos-release --parallel 4

Inspect the actual generated effective.config/header/config.cmake and
Board/layout identity. Change the source fragment, not generated outputs. Unknown
or removed switches must be corrected explicitly; no compatibility fallback
silently enables old behavior.

Board resources and optional software
-------------------------------------

Enable only implemented GPIO/UART/SPI routes represented by the Board manifest.
DMA selection requires the reviewed complete route; UART DMA is unsupported.
There is no generic typed MCU I2C/ADC/PWM/EXTI option. Optional components and
crypto provider are explicit choices, with actual dependency and memory budgets.
For persistence, declare an external Flash layout and open the appropriate
region; no default storage area exists.

External Board/source SDK consumers use the same effective configuration model.
An external package's declared sources must remain inside it and match hashes.
Candidate CI tests a full explicit Nexus commit without changing formal
Gitlink/lock; final delivery records both source trees and actual artifacts.
See :doc:`kconfig_platforms` and the root CMake/source SDK guides.
