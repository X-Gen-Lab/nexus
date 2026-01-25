---
name: Test Failure
about: Report a failing test case
title: '[TEST] '
labels: test
assignees: ''
---

## Test Information
<!-- Which test is failing? -->

- **Test Suite**: [e.g., HAL_GPIO, OSAL_Task, Config_System]
- **Test Case**: [e.g., test_gpio_write, test_task_create]
- **Test Type**: [Unit Test / Integration Test / Property Test]
- **Test File**: [e.g., tests/hal/test_gpio.cpp]

## Environment
<!-- Test environment -->

- **OS**: [e.g., Windows 11, Ubuntu 22.04, macOS 14]
- **Compiler**: [e.g., GCC 12.2, MSVC 19.41]
- **Build Type**: [e.g., Debug, Release]
- **Platform**: [e.g., native, stm32f4]
- **Nexus Version**: [e.g., v0.1.0 or commit hash]

## Failure Type
<!-- Check the type of failure -->

- [ ] Assertion failure
- [ ] Segmentation fault
- [ ] Timeout
- [ ] Memory leak
- [ ] Unexpected behavior
- [ ] Flaky test (intermittent failure)
- [ ] Other: ___________

## Test Output
<!-- Paste the test failure output -->

```
Paste test output here
```

## Steps to Reproduce
<!-- How to reproduce the test failure -->

```bash
# Build
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build

# Run test
cd build
ctest -R test_name --output-on-failure
```

## Expected Behavior
<!-- What should the test do? -->

## Actual Behavior
<!-- What does the test actually do? -->

## Frequency
<!-- How often does this test fail? -->

- [ ] Always fails
- [ ] Fails intermittently (flaky)
- [ ] Fails only in CI
- [ ] Fails only locally
- [ ] Fails on specific platforms: ___________

## Additional Context
<!-- Add any other context -->

- Related issues: #
- Recent changes that might affect this:
- Sanitizer output (if applicable):

## Checklist

- [ ] I can reproduce this consistently
- [ ] I have provided complete test output
- [ ] I have checked if this is a known issue
- [ ] I have tried on the latest version
