# Validation uses the maintained preset workflow

`validate.py` forwards to `scripts/ci/ci_build.py`. There is one CMake/CTest
preset path, one effective configuration and the original CTest JUnit file.
The old ad-hoc build/controller/HTML/coverage-report framework is removed.
Legacy `--build-dir`, implicit native configuration, JSON configuration and
stdout-derived test results are rejected; use explicit presets.

```sh
python scripts/validation/validate.py --preset linux-gcc-debug --stage all
python scripts/nexus.py test --preset linux-gcc-debug
python scripts/validation/junit.py --report build/linux-gcc-debug/ctest-results.xml
python -m unittest discover -s tests/validation -p 'test_*.py'
```

The CI runner removes the old JUnit before invoking CTest with `--no-tests=error`
and `--output-junit`, requires subprocess success, then checks report freshness
against the execution start. The shared `validate_junit()` rejects missing,
symlinked, old, malformed, empty, all-skipped, failed/errored, contradictory-count,
nonexecuted or duplicate/unnamed case evidence. It retains actual case names,
classes, statuses and the report SHA-256. Partial skips stay explicitly skipped;
they are not counted as passed. A report checker alone does not establish the
source/configuration that ran: candidate evidence must bind those separately.

No replacement synthetic `test_1`/`test_2` JUnit is generated. Native tests,
ARM static artifacts, software fault models and physical HIL remain separate.
A workflow definition is not execution evidence.

## Coverage

Capture/filtering and HTML remain the maintained `build-matrix.yml` LCOV/genhtml
workflow. `check_coverage.py` is the single small optional line-threshold checker,
not a second build or collection engine:

```sh
python scripts/validation/check_coverage.py --coverage-file coverage.info --threshold 0.80
```

It requires complete nonempty `SF` records, actual `DA` measurements and matching
`LF`/`LH` counts. Missing, stale (with `--not-before-ns`), malformed, negative,
duplicate, unterminated, zero-measurement and false-count traces fail. The output
is explicitly scoped line coverage; it does not invent function/branch results
or claim boards absent from Native instrumentation were executed. Scope and
capture identity belong to the workflow/evidence manifest.

## Regression scope

The standard-library tests run the production checker as real subprocesses and
copy the production entry/CI helper into a small filesystem fixture that runs
actual CMake/CTest. Cases cover a finite passing test, zero tests, all skips,
invalid presets, forbidden embedded test execution, nonzero CTest hidden behind
a passing XML, corrupt fresh XML and an old report surviving a command which
produces no output. Controlled executable tools model missing/corrupt reports;
they do not stand in for a completed Nexus firmware suite. No third-party Python
package is needed. The replaced `tests/validation/` use these executable
contracts instead of assertions against the removed framework.
