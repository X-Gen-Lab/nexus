---
name: Bug Report
about: Report a bug to help us improve
title: '[BUG] '
labels: bug
assignees: ''
---

## Bug Description
<!-- A clear and concise description of what the bug is -->

## Environment
<!-- Please complete the following information -->

- **OS**: [e.g., Windows 11, Ubuntu 22.04, macOS 14]
- **Compiler**: [e.g., MSVC 19.41, GCC 12.2, arm-none-eabi-gcc 10.3]
- **CMake Version**: [e.g., 3.28]
- **Python Version**: [e.g., 3.11]
- **Platform**: [e.g., native, stm32f4, stm32h7]
- **OSAL Backend**: [e.g., baremetal, freertos]
- **Nexus Version**: [e.g., v0.1.0 or commit hash]

## Affected Component
<!-- Check all that apply -->

- [ ] HAL (Hardware Abstraction Layer)
- [ ] OSAL (OS Abstraction Layer)
- [ ] Framework (Config/Log/Shell/Init)
- [ ] Build System (CMake)
- [ ] Documentation
- [ ] CI/CD
- [ ] Other: ___________

## Steps to Reproduce
<!-- Provide detailed steps to reproduce the issue -->

1. Configure with `cmake -B build -DNEXUS_PLATFORM=...`
2. Build with `cmake --build build`
3. Run `...`
4. Observe error

## Expected Behavior
<!-- What you expected to happen -->

## Actual Behavior
<!-- What actually happened -->

## Error Output
<!-- Paste any error messages, logs, or stack traces -->

```
Paste error output here
```

## Code Sample
<!-- If applicable, provide a minimal code example that reproduces the issue -->

```c
/* Minimal reproducible example */
```

## Configuration
<!-- If relevant, provide your configuration -->

<details>
<summary>CMake Configuration</summary>

```bash
# Your cmake configuration command
```

</details>

<details>
<summary>Kconfig (.config)</summary>

```
# Relevant Kconfig options
```

</details>

## Workaround
<!-- If you found a workaround, please describe it -->

## Additional Context
<!-- Add any other context about the problem -->

- Related issues: #
- Related PRs: #
- Screenshots:
- Logs:

## Checklist
<!-- Please check the following before submitting -->

- [ ] I have searched existing issues to avoid duplicates
- [ ] I have read the documentation
- [ ] I have provided all required information
- [ ] I can reproduce this issue consistently
- [ ] I am using the latest version (or have noted the version above)
