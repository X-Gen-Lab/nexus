# Required quality gates

CI-002 replaces diagnostic jobs that ignored tool errors with a required static
analysis job. Both clang-tidy and cppcheck consume the actual compilation
database; the runner filters source ownership to HAL, OSAL, framework, services,
platforms, boards and SoCs. Third-party source is excluded by path rather than
by hiding finding text. Duplicate source files with different compile commands
receive separate clang-tidy databases, retaining provider/configuration coverage.

Missing tools, failed version detection, malformed databases, absent source,
zero owned translation units, timeouts, tool crashes and diagnostic exits fail.
Reports preserve the tool version, commands and return codes on success/failure.
The workflow uploads reports even after a failed analysis. No empty report or
ignored stderr establishes MISRA compliance. Product qualification still needs
an agreed rule set, reviewed deviations and suitable tool evidence.

Nine stdlib behavior tests execute process fixtures to verify the gate's
success/error/ownership rules. They passed locally. These fixtures model analyzer
execution outcomes; they do not analyze C code. This environment has no
clang-tidy/cppcheck executable, so the actual static scans were not executed.
Their CI results must be reviewed before claiming that this gate is green.

Run the policy regressions with:

```sh
python -m unittest discover -s scripts/ci -p 'test_quality_tools.py' -v
```

Repository formatting and complexity settings remain development guidance.
Removing pretend-compliance output does not replace an actual product review.
Build, sanitizers, hardware budgets and artifact evidence use their separate
execution records and may not be inferred from these policy tests.
