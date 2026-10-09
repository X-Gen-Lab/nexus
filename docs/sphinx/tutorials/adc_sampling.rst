ADC Support Boundary
====================

Nexus does not maintain an STM32F407/GD32F470 production ADC provider. Native ADC
is an explicit private host model, not an analog precision, calibration, sampling
rate or DMA qualification. A chip ADC peripheral or copied configuration cannot
make a typed MCU application usable.

A future port needs reviewed reference voltage/input wiring, controller ownership,
trigger/DMA settlement, calibrated conversion and source-bound software/physical
checks. Product signal processing and calibration policy remain external.
