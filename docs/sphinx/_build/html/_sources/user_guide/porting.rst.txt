Porting Guide
=============

This guide explains how to port Nexus to a new MCU platform.

Steps
-----

1. Create platform directory: ``platforms/<platform_name>/``
2. Implement HAL drivers
3. Create startup code and linker script
4. Add CMake configuration
5. Test with blinky example

See ``platforms/stm32f4/`` for reference implementation.
