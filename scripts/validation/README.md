# Actual test evidence

`tools/dev/dev.py test --preset native-debug` removes any old JUnit, invokes
CTest with `--no-tests=error` and an absolute `--output-junit` path, preserves its
exit code, then validates the actual report against the execution start time.

`junit.py` rejects missing, symlinked, old, malformed, empty, all-skipped,
failed/errored, contradictory-count and duplicate/unnamed case evidence. Partial
skips stay skipped. Report hashes alone do not identify which source/config ran;
candidate qualifications bind those identities and raw execution logs separately.

```sh
python scripts/validation/junit.py --report build/native-debug/ctest-results.xml
python -m unittest discover -s tests/contracts -p test_junit.py
```

Native behavior, production-register models, ARM linkage/resource checks and
physical HIL are separate evidence scopes. A workflow file is not an execution.
