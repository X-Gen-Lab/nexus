# Industrial services and executable reference

Implemented on 2026-10-08 for APP-001, COM-001 and the bounded diagnostic portion
of OBS-001. These are executable software contracts and a Native model; the
first-board pilot, electrical RS485 behavior, real watchdog, worst-load timing
and field crash capture remain hardware/product acceptance items.

## Dependency and execution boundary

`nexus_modbus_rtu` and `nexus_industrial` are independent C11 libraries. Neither
includes vendor types, RTOS types, network libraries or a heap allocator. A
product injects its clock, UART/RS485, register transaction, safe-output and
watchdog ports. The generated per-build `nexus_config.h` is consumed by the
reference executable; it is not a root source configuration.

MCU portability of the service code is distinct from a working board adapter.
No production STM32/GD32 UART + RS485 adapter is implemented or physically
validated by this change. In particular, a candidate STM32F4 Discovery MB997
does not include an RS485 transceiver; it requires an explicitly selected
external transceiver, wiring, termination and biasing before COM-001 can pass
electrical acceptance. Compiling this engine for Cortex-M does not establish
industrial communication support on that board.

A board adapter must select a UART, peripheral clock and baud/parity/stop
configuration, DE/RE polarity and GPIO, optional DMA/IRQ mapping, and a monotonic
timer with wrap extension. It must timestamp received bytes, wake the owner task
for frame silence, detect UART errors/queue overflow, test final-stop-bit drain,
and settle DMA on abort. Define the RX queue capacity, interrupt/task priority,
maximum callback duration, register snapshot/commit synchronization and static
memory/stack budget in a product profile. No pin assignments, transceiver supply
voltage, isolation class or production clock assumptions are invented here.

Both services execute under a single task owner. An ISR captures UART data and
arrival times into a bounded queue and marks overruns; it never parses a frame,
commits registers, formats diagnostics or feeds the watchdog. Communication and
parameter persistence belong to the management domain, with a separate resource
and scheduling budget from periodic acquisition/control. A supervisor mask must
contain actual critical jobs, never an always-running background heartbeat.

## Modbus RTU and RS485

The server implements functions 03 (read holding registers), 06 (write one) and
16 (write multiple), a 256-byte ADU bound, 125-register read bound and
123-register write bound. It verifies CRC-16/Modbus, exact function lengths,
byte counts, zero quantities, address-space overflow, configured slave address,
unsupported functions and callback failure codes. Other slave addresses and
invalid CRCs are silent. Address 0 only executes permitted writes and always
stays silent, including an authorization or commit failure. Exceptions use the
standard function-high-bit form and a verified response CRC.

At baud rates through 19200, 1.5/3.5-character intervals are rounded up from the
configured start/data/parity/stop bit count. Above 19200 the intervals are
750/1750 microseconds. Initial receive state requires bus silence. An internal
gap over 1.5 but below 3.5 character times invalidates the entire frame; receive
overrun or UART error discards data until a full silence interval. `poll` must
run after that silence before a new frame starts. If a new byte arrives while
the prior frame still has not been serviced, the prior frame is discarded rather
than initiating a half-duplex reply over the incoming request. The firmware must
provide a timer/communication-task budget that meets this scheduling requirement.

All times are absolute monotonic microseconds; the UART port must extend a
wrapping hardware counter. Backward timestamps are rejected. TX has a single
absolute deadline covering write and final-wire drain. It verifies that a port
reporting success has not already exhausted that deadline. Error cleanup has a
separate bounded abort deadline. The successful drain contract includes the
final stop bit, not only DMA transfer completion. Abort must settle DMA and
callbacks before releasing storage and deasserting DE. A failed abort latches
the link, retains its static TX buffer, and prevents further requests until an
explicit successful recovery. Missing operations make initialization fail.

Register callbacks enforce range, value, permissions and product interlocks.
`write_atomic` must commit all values or none. With concurrent product state it
must lock/recheck critical interlocks during commit: a prior authorization
callback alone is insufficient against state changes. Reads must be coherent
snapshots. Callback execution itself must have a measured bound; the parser
cannot bound arbitrary application code. Modbus RTU provides no peer identity,
authentication or confidentiality.

## Industrial supervision and diagnostics

The test suite uses a 1 ms model cycle and the reference uses a 10 ms model
cycle. These deliberately chosen software test inputs are not product control
deadlines, jitter allowances or measured MCU performance. Real periods, job
deadlines, watchdog window and safe-output policy must come from the product
requirements and worst-load hardware measurements. An independent hardware
protection layer is required where the product risk assessment demands it.

The supervisor holds at most eight critical jobs. Each cycle has a generation
token and per-job deadlines relative to the release time. Completion must occur
strictly before the configured deadline. Only a complete set of fresh,
successful, valid-quality completions feeds the physical watchdog, exactly once
per cycle. Old-generation and duplicate completions cannot satisfy a new cycle.

A missed deadline, skipped control cycle, stale/invalid sample, regressing clock,
failed watchdog operation or explicit product trip latches the safe state and
calls the product's safe-output port. A failure of that port is separately
recorded. The service stops feeding after a fault; the board's independent
watchdog remains responsible for recovery if execution itself stalls. Rearm is
an explicit authorized maintenance operation, requires the safe-output port,
invalidates old tokens and requires a complete fresh cycle. It must not be wired
to an unrestricted network register or automatic retry loop.

Build identity is copied into bounded storage, reset reason is board-provided,
and a 16-record event ring retains state changes with timestamps, job and cycle.
On overflow the oldest event is replaced and a loss counter increments. Event
draining/formatting occurs outside the control path. No field ELF/crash parser,
physical reset-reason implementation or persistent event store is claimed here.

## Executed verification

The standalone regression executable has 19 named cases, using checks that
remain active in Release builds. Cases cover valid/maximal reads and writes,
atomic commit failure, critical write authorization, CRC/address/broadcast
behavior, malformed length/quantity/range, intra-frame gaps, unserviced silence,
RX overflow, clock regression, TX/direction/drain failures, failed abort with
retained ownership, explicit recovery and absolute deadline exhaustion. A
deterministic 50000-byte fuzz stream exercises bounded parsing, and another
10000 CRC-valid structured frames exercise the decoder's quantity/length/function
boundaries. Supervisor cases
cover all-job watchdog gating, cycle boundaries, stale tokens, data quality,
missing jobs, skipped cycles, safe/watchdog ports and bounded event loss.

Executed with GCC 14, C11, `-Wall -Wextra -Werror -Wconversion -Wshadow`, AddressSanitizer
and UndefinedBehaviorSanitizer. All 19 cases passed, including an optimized
`-DNDEBUG` build whose checks remain enabled. LeakSanitizer cannot run
under the execution environment's ptrace supervision, so the run used
`ASAN_OPTIONS=detect_leaks=0`; neither library allocates heap memory. The Native
object sizes were 976 bytes (RTU) and 720 bytes (supervisor), excluding product
callback state, UART queue, stack and the executable's runtime. These are not
MCU resource measurements.

The reference executable passed the same strict compilation and sanitizer run,
using the generated header from `build/linux-gcc-debug/generated`. Its behavioral
result was:

```json
{"backend":"native-wire-plant-model","source":"126e544e3fcdf8355a22a8e5bdb40352b8acf514-dirty","config_sha256":"577faaf7836e7c3b75731188ce3de3716a2a1d107a19de0d73b093b9984ed0f6","board":"native-reference","setpoint":350,"critical_write_blocked":true,"watchdog_feeds":1,"safe_output":0,"fault":"stale-sensor","rtu_bytes":976,"supervisor_bytes":720}
```

Its real protocol engine accepts a setpoint write, refuses enable until a local
interlock is asserted, and produces CRC-valid replies. The real supervisor
completes a healthy acquisition/control/diagnostic cycle, then rejects a stale
sensor sample, switches outputs off and stops watchdog progress. Product
parameters in the reference are volatile; persistence is a separate service.
The CMake-integrated executable also emits its configured source revision,
effective-configuration SHA256 and board profile. A `-dirty` source identity is
an explicit development indication, not a hash of the uncommitted source tree;
release artifacts still need a clean source and matching release identity.

Full CMake/CTest, ARM compilation, actual board timing and physical fault
injection are reported by their corresponding build/platform evidence. This
document does not replace them. CAN/CANopen and Ethernet/MQTT are still
product-selected follow-up capabilities (COM-002/003), not transport stubs or
successful hardware implementations.
