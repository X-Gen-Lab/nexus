Power Management Boundary
=========================

Nexus does not supply a complete product low-power policy or measured MCU power
qualification. A lifecycle/power interface in a provider is not proof of every
sleep/stop/standby mode, wake source, safe output or retained clock behavior.

Product applications own actuator safe-output, clocks needed by external devices,
wake policy, watchdog/health and resource settlement. Controller/operation buffers
must remain owned until hardware is stopped. Closing one GPIO must not disable a
shared port used by another owner; suspend/close errors retain retryable state.

Common HAL cleanup is limited to idle baremetal/pre-scheduler MCU state. Running
or suspended FreeRTOS kernel remains BUSY. Actual mutating cleanup error keeps
PARTIAL/admission fencing. STM peripheral-bank reset followed by narrow Board
initial-level reconstruction is not actuator-level continuity. Raw SDK callers
must first quiesce their resources.

GD TIMER1 timestamp, scheduler SysTick and STM HAL clock have explicit ownership;
untested low-power transitions cannot promise preserved timing. Native power
models do not measure voltage/current, oscillator start or wake latency. Boards
are deferred; any product power mode requires independent implementation,
reviewed wiring/budgets and physical qualification.
