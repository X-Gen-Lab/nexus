# Secondary-controller development evidence

The I2C slice executes maintained production providers and the shared V1
algorithm. Only external CPU, NVIC, DATA side effects, W1C and refused DMA
disable use test models. The STM32 dual-instance tests execute its normal
NVIC path. Pinned GoogleTest/GoogleMock provide the test framework.

API-missing compilation and the real seven-case behavior RED (six failures
per family) remain unchanged. Earlier build-driver failures are retained as
infrastructure failures, not behavior RED. Legacy forced inclusion required
POSIX feature selection before system headers; the initial ARM driver chose
stale generated headers lacking IRQ policy, corrected by selecting existing
authoritative generated CPU profiles. These failures are never promoted.

The current I2C manifest binds actual XML, commands, binary and ARM object
bytes. It is a development snapshot, not a clean-HEAD or full source-SDK
qualification. Configuration/factory integration, selected full-image retention
and normal Git hooks are root-owned later gates. Hardware HIL has not run.
Cycle counts and bus timing have not been measured. Historical raw C inputs
use .c.raw; driver files use .py.raw without changing original bytes.
