# Stack observation and real owner lifecycle development evidence

These records bind actual dirty-development inputs, not a clean platform
candidate or physical qualification. Source fingerprints and raw file identities
are in `manifest.json`; formal SDK, host and board matrices remain separate.

- `red-commands.json` and the RED XML/logs execute the unsupported production
  stubs before implementation: Core three cases and adapter seven cases.
- `freeze-commands.json` and three fresh `*-freeze.xml` files contain the final
  Core three, adapter seven and unfiltered real-kernel eleven GoogleTest cases.
  Four new kernel cases include two static task identity queries and two lifecycle
  cases executing 32 and 256 actual POSIX-kernel rounds.
- `arm-profile-selection/report.json` uses the maintained production Runtime
  graph for standard and diagnostic M4F hard-float profiles. It verifies absent
  versus selected `stack_snapshot.c` objects and preserves actual ELF/map,
  compilation database, commands, object sections and target-ABI layout probe.
  The authored historical probe is stored as `.c.raw` and compiled with `-x c`;
  it is measurement evidence, not a maintained source translation unit.
- Early kernel SIGSEGV, timeout and temporary diagnostic checkpoint runs are
  retained as failed/debug evidence. They exposed fixture stack placement and
  duplicate wake-after-notification-destruction bugs; neither is counted as a
  positive run. The full external syscall trace identity is recorded separately.

The paint result is bounded inspection, not physical stack peak/WCET. The POSIX
port executes tasks on separate pthread stacks. Native UART events model IRQ/TC
and drainage; physical three-board soak and latency are not executed.
