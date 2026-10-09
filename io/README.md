# Typed I/O

Fixed typed GPIO, UART, SPI, I2C, Flash, watchdog, EXTI, PWM and ADC contracts.
Generated bindings identify selected instances; callers do not discover devices
by name, factory, class registry or a generic object pool. Public interfaces live
in `include/nexus/io`, SoC implementations in `soc/<family>/drivers`.

Default paths need no OS locks or hidden tasks. Requests and exact-capacity
buffers are caller/assembly owned. UART tracks physical TC separately from
transmitted bytes and request release; error-only RX events do not invent data.
SPI and I2C expose their supported bounded polling semantics. Flash exposes real
physical geometry, with products explicitly constructing restricted regions.

`NativeModel` is an explicit deterministic behavior model with injection controls,
not a peripheral emulator. Real production-register fault models and independent
ARM mode-link fixtures live in `tests/contracts`. Board facts authorize reviewed
physical routes; software-only fixtures do not establish PCB connectivity.
