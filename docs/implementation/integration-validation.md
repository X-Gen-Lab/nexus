# Integrated software validation

This record separates executed software checks from physical product acceptance.
The current source and the latest GitHub runs are available in
[PR #3](https://github.com/X-Gen-Lab/nexus/pull/3). Check results belong to the
displayed commit; a passing earlier run does not qualify later changes.

## Executed follow-up checks

After reviewing the first real GitHub analyzer reports, the integrated Native
Debug working tree executed **1,710/1,710 CTest entries**, with zero failures and
zero skips, in 194.14 seconds. This sequential follow-up run includes 468 Native
HAL entries, 272 OSAL entries, 137 logging entries and 303 shell entries. Its
report is `build/linux-gcc-debug/ctest-online-fixes.xml`; the execution log is
`/tmp/nexus-online-fixes-native-all.log`. It is a working-tree integration result,
not a signed release or a hardware qualification result.

The follow-up includes eleven new HAL regressions and two new history
regressions. Existing contract executables also exercise invalid power instances,
allocations exceeding the Native diagnostic budget, and buffered stdout failure.
The logging executable now runs nine groups; the Native core runs fourteen.
The real FreeRTOS/POSIX kernel runs ten groups, including zero-delay yielding.

The CI policy suite executed **104/104 stdlib tests** locally, covering analyzer
failure handling, actual compiler model import, Python syntax scope, release
packaging and required-job policy. These fixtures do not establish that a C
analyzer, signing service or physical HIL station passed.

## Real GitHub findings and repairs

The first online run of revision `3cbfe180` passed three Native compiler/build
profiles, the 23 selected ASan/UBSan contracts with leak checking enabled, the
documentation build, and the enterprise tool matrices. Required static analysis,
ARM linking, coverage export and secret scanner startup failed. The aggregate
gate correctly rejected these failures.

Repairs preserve explicit failure behavior:

- STM32 startup objects and linker layout are owned by the platform target;
  missing layout/startup properties fail configuration. Application-local
  duplicate fatal handlers are removed in favor of the platform handler.
- ARM `taskYIELD()` branches are bracketed for the actual port macro expansion.
- GNU coverage uses atomic profile counter updates for concurrent execution;
  lcov errors are not ignored.
- HAL NULL guards, Flash file errors, factory formatting and ADC model ranges
  are checked. Failed Flash deinitialization preserves state for retry.
- History capacity, JSON formatting, console flush and OpenSSL signed-length
  arithmetic are checked. Binary wire fields retain their exact length.
- Cppcheck receives each translation unit's observed compiler/target model.
  The required clang-tidy profile and reviewed advisory exclusions are documented
  in [quality-gates.md](quality-gates.md).
- Secret scanning no longer duplicates the action's `--fail` flag; its subsequent
  real online run passed. Tracked Python sources are parsed and compiled without
  execution, so invalid examples cannot silently remain outside analysis.

The subsequent run of revision `da49243` passed all 1,710 tests in each of three
Native matrix jobs, 23 ASan/UBSan entries, and clang-tidy across 136 owned
translation units. Cppcheck's remaining linker-symbol pointer comparison is
replaced by validated numeric spans; repeated initialization preserves an earlier
callback failure. Coverage also exposed shared Flash fixture files: fixtures now
use separate temporary directories, and two same-directory processes completed
250 Flash test executions without interference. The eight generated Kconfig
templates now pass their own naming lint; incorrect choice mappings still fail.

Latest ARM, analyzer and coverage outcomes must be read from the linked PR's
actual checks and retained reports. CodeQL analysis/upload success alone does
not establish zero security alerts; the current connector cannot read its alerts
endpoint. No zero-alert or MISRA qualification claim follows from this record.

## Product acceptance boundary

The supported STM32F407 profiles and Native models do not establish GD32 BSP,
real-board timing, electrical/DMA behavior, watchdog safe outputs, brownout,
protected update rollback, manufacturing-line or production-signing acceptance.
Those gates remain explicit in the requirements and delivery workflow. Host
inventory/SBOM evidence must bind a freshly rebuilt clean commit, its effective
configuration, actual linked artifacts and a new execution record.
