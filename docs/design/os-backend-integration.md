# OS backend integration

Nexus maintains Native, Baremetal and the reviewed FreeRTOS ports. An additional
RTOS is conditional on an actual integration requirement and a reviewed kernel
license, version, port and resource budget. This document and the executable
probe prepare that work; they do not claim RTX, Zephyr or another kernel is
supported. Applications continue to own task topology, storage and shutdown.

## Public contract

| Boundary | Required behavior |
| --- | --- |
| Time | Finite absolute monotonic microseconds in the same clock domain as the caller; the clock continues while waiting. Use `NX_DEADLINE_NEVER` for infinity and `nx_deadline_after` for saturating construction. |
| Completion | A wake or changed sequence is a hint to recheck the authoritative predicate. Unrelated wakes never restart a deadline. The final predicate observation can win over a timeout or another wait error. |
| Admission | A rejected operation retains no references. Timeout does not cancel, settle or reclaim an accepted I/O request. |
| Wait port | One owner calls `arm` and `wait`; multiple publishers may call `wake` within the documented context. Arming and the predicate recheck must close the lost-wake window. |
| Storage | The application supplies adapter objects, stacks, task control blocks and queue buffers. No hidden workers or maximum-size pools. Native POSIX libraries may allocate internally; their cost is not a MCU budget. |
| Notification teardown | Publishers and the single waiter must be quiesced before reclaim. A wait error is insufficient evidence of quiescence. |
| Task teardown | One lifecycle owner and one joiner. Entry return alone is insufficient; successful join establishes reclaim. Cooperative stop is explicit. |
| Queue teardown | Close rejects new sends, wakes blocked users and permits retained items to drain; users must exit before destroy. A raw kernel queue without close must expose its separate owner protocol. |
| ISR | Declare which calls are legal in task and ISR context. Reject exceptions or priorities outside the kernel syscall ceiling before calling any ISR kernel API. |
| Masks | Preserve incoming interrupt masks on every return path. Baseline ports explicitly restore their borrowed PRIMASK before kernel calls; Mainline ports obey their BASEPRI ceiling. Never assume entry was unmasked. |
| Features | Unsupported CPU, port, ABI, security or MPU combinations fail at configuration. A host model cannot establish real kernel scheduling, physical IRQ priority or board timing. |

`nx_wait_port_t` retains its direct callback ABI. The maintained
[Wait cost models](../../tests/contracts/os_wait_cost/README.md) compare actual
linked alternatives. Backend integration must not create a second deadline
helper, runtime device registry or another assembly authority.

## Integration sequence

1. Record the actual requirement, kernel license/version, dependency identity,
   CPU capabilities and explicit exclusions. Obtain the required source under
   the repository's existing dependency process; do not edit vendored kernels
   to hide adapter problems.
2. Add contract regressions first and retain actual RED failures. Use
   GoogleTest/GoogleMock with real production adapter code, mocking external
   hardware or kernel boundaries only. Add a real-kernel scheduler probe for
   kernel behavior; mocked calls alone cannot establish it.
3. Implement the static adapter and its context/deadline/lifecycle documentation.
   Define notification ownership, shutdown/join and ISR priority rejection
   before integrating I/O waits. Keep firmware C11, and keep test C++ outside
   firmware targets.
4. Propose the port in the existing CPU/kernel resolver and CMake target graph.
   Authored TOML remains the sole selection authority. The resolver must reject
   unsupported combinations. Integration changes require maintainer review;
   the probe cannot add a backend to production configuration.
5. Create a strict probe declaration using the
   [Native reference](../../tests/contracts/os_backend_probe/native.json).
   Select `host`, `baremetal` or `kernel`; list exact Google case identities for
   each requirement. Kernel candidates add mask/ISR and real scheduler groups,
   a source lock path and actual retained static kernel API symbols.
6. Run `scripts/ci/os_backend_probe.py` against the configured production build.
   It rebuilds the nominated registered Google targets, discovers and executes
   all their cases with fresh XML, checks exact case identities and reports
   actual GNU ELF measurements. It rejects missing groups, filtered identities,
   empty execution, skipped/disabled cases, missing tools and source changes.
7. MCU candidates additionally nominate a production firmware build/target, ELF
   and its generated `resolved-cpu.json` or full `resolved.json` CPU resolution.
   The probe rebuilds that firmware, verifies ARM ELF format, checks short-enum
   and floating ABI against the existing production CPU resolver, binds actual
   owned compile flags, rejects undefined symbols/heap/test runtimes and reads
   actual retained kernel API symbols and RAM/Flash sections. A missing source
   lock or CPU/backend not registered by the resolver fails closed.
8. Review raw commands, source identities, tests, actual static storage/Flash,
   retained instructions and failure paths. Establish a clean candidate through
   the normal independent CI/reproducibility process. Physical timing, watchdog,
   low-power clock continuity and IRQ stress still require HIL equipment.

## Executable scope

The declaration records a required contract; it is not evidence that the
implementation honors it. The runner actively produces fresh execution rather
than accepting a supplied successful XML or status marker. Source snapshots bind
owned implementation/tests, compilation databases and compiled source files
before and after execution. They establish a stable working tree measurement,
not clean Git qualification, dependency provenance or reproducibility.

`claim_support` must be `false`. A passing report states
`support_promoted: false`, `registration: not_performed`,
`maintainer_review_required: true`, `candidate_status: not_qualified` and
`physical_status: not_executed`. Kernel API retention proves linked symbols; the
reviewed scheduler cases and dependency process establish their semantics and
identity. A source lock digest alone is insufficient dependency provenance.

Native test ELF RAM/Flash counters describe the host executable's sections only.
The Native fixtures allow a 256 KiB caller stack for host sanitizer instrumentation;
this is not a recommended MCU stack size. POSIX library, process stack, test
framework and sanitizer overhead must never be compared as embedded adapter cost.

The native reference command and candidate options are documented beside the
[probe fixture](../../tests/contracts/os_backend_probe/README.md). CI must retain
raw logs, fresh XML and `report.json`; missing proof is a failed admission probe.
