# Industrial controller reference

The executable runs the actual Modbus RTU parser and industrial supervisor with
a Native wire and plant model. It writes a setpoint, refuses an unauthorized
enable request, accepts a locally interlocked enable, completes one healthy
control cycle, and then injects stale sensor data. The output turns off and the
watchdog feed count stops at one. A failed invariant exits nonzero.

This is an explicitly service-only model, so it deliberately does not call
`nx_product_boot()` or start HAL/OSAL. It has no real device handles, tasks or
hardware ownership to shut down. Firmware integrations use `Nexus::Product`,
boot before opening typed devices and settle objects before product shutdown.

Build with the Native preset and `NEXUS_BUILD_EXAMPLES=ON`, then run
`nexus_industrial_controller` from that build directory. `nexus_config.h` comes
from that build's effective configuration. The model uses no physical UART,
watchdog or Flash and does not establish timing or HIL support.

Holding registers: 0 is the sensor sample, 1 is the setpoint (0..1000), 2 is the
output status, 3 is sample quality, 4 is enable (0/1). Only setpoint and enable
are writable. An enable request requires the local commissioning interlock.
Modbus RTU has no authentication: deploy behind appropriate physical/network
controls and apply product write policy in `authorize_write`. Production
parameters should be validated and committed through the storage service in a
separate management task; this executable does not claim persistent settings.

Control callbacks execute under a single owner. UART ISR handlers should queue
arrival timestamps, bytes and overflow markers to a bounded communication-task
queue. That task must poll the RTU engine after 3.5 character times of silence
before another frame arrives, and provide real DE, final-stop-bit, DMA abort and
monotonic-clock operations. A real board must supply product-safe outputs,
watchdog hardware and measured job deadlines.
