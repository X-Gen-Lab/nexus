# Optional timer development evidence

This bundle records actual production-provider GoogleTest/GoogleMock TDD and
strict vendor-SDK ARM compilation. It is a development snapshot, not clean-HEAD
platform, whole-firmware or physical qualification.

Latest ordinary executions are `stm-capture-read-green.xml` and
`gd-capture-read-green.xml`: 46 tests each, zero failures/errors/skips. Latest
ASan/UBSan executions are `stm-sanitizer-capture-final.xml` and
`gd-sanitizer-capture-final.xml`: the same 46 unfiltered tests each. The four
matching `*.command.json` files retain actual argv, timestamps, status and raw
logs. Production C remains C11; GoogleTest/GoogleMock replace only external CPU,
MMIO and monotonic observation boundaries.

The retained RED history includes missing APIs and actual behavior failures for
known-storage aliases, altered hardware configuration, static lifecycle reuse,
failed interrupt withdrawal, break-before-MOE and capture read-clear races.
In particular `*-capture-read-red.xml` each has two failures before the normal
IRQ stopped clearing new unread CCIF and began observing OC after the data read.
Historical reports and binaries are never relabeled as current executions.

`arm-final-capture/report.json` binds eight actual strict Cortex-M4 production
objects and two layout fixtures against locked SDK headers. All ten commands
exited zero. Actual capture state is 36 bytes and advanced PWM state is 12 bytes;
these selected objects add no global DATA/BSS. Capture methods use four entries,
advanced PWM five; existing general-purpose PWM remains unchanged. Object code
cost is not a whole-image budget or instruction/cycle/ISR-latency measurement.

Exact focused commands (use a new evidence label and XML path for a new run):

```sh
python /workspace/nexus-timer-iteration/build.py stm timer-stm-capture-read-green-build
python /workspace/nexus-timer-iteration/run.py timer-stm-capture-read-green /workspace/nexus-timer-iteration/stm-test --gtest_output=xml:tools/testing/evidence/timer-iteration/stm-capture-read-green.xml
python /workspace/nexus-timer-iteration/build.py gd timer-gd-capture-read-green-build
python /workspace/nexus-timer-iteration/run.py timer-gd-capture-read-green /workspace/nexus-timer-iteration/gd-test --gtest_output=xml:tools/testing/evidence/timer-iteration/gd-capture-read-green.xml
python /workspace/nexus-timer-iteration/arm_final_capture.py
```

Raw external drivers are archived as `drivers/*.py.raw` without byte edits.
Layout probes use `.c.raw` and explicit `-x c` from their original compilation.
The independent read-only review did not rerun these tests; its finding drove the
recorded final read-clear regressions and is preserved separately.

`development.json` binds current owned source, latest execution reports,
original command/log history, ARM objects and preserved host executables.
Host executables remain in the external frozen-bin directory; they are not
firmware dependencies. Full typed assembly, default-image GC, shared-vector
ownership and staged normal hooks are separate integration gates. External
Board facts own electrical inactive drive/readback before lease release.
Physical edge/filter/clock/deadtime accuracy, IRQ WCET, break-pulse reaction,
shared-vector contention and electrical safety remain **not executed**.
