---
name: Security Vulnerability
about: Report a security vulnerability (use private disclosure for critical issues)
title: '[SECURITY] '
labels: security
assignees: ''
---

<!-- 
⚠️ IMPORTANT: For critical security vulnerabilities, please use GitHub's 
private vulnerability reporting feature instead of creating a public issue.
Go to: Security -> Report a vulnerability
-->

## Severity Level
<!-- Estimate the severity -->

- [ ] Critical (Remote code execution, privilege escalation)
- [ ] High (Data exposure, authentication bypass)
- [ ] Medium (Information disclosure, DoS)
- [ ] Low (Minor information leak)

## Vulnerability Type
<!-- Check all that apply -->

- [ ] Buffer Overflow
- [ ] Memory Leak
- [ ] Use After Free
- [ ] Integer Overflow
- [ ] Format String
- [ ] Race Condition
- [ ] Injection
- [ ] Authentication/Authorization
- [ ] Cryptographic Issue
- [ ] Other: ___________

## Affected Component
<!-- Which component is affected? -->

- **Module**: [e.g., HAL GPIO, OSAL Task, Framework Shell]
- **File**: [e.g., hal/src/nx_gpio.c]
- **Function**: [e.g., nx_gpio_write()]
- **Version**: [e.g., v0.1.0 or commit hash]

## Vulnerability Description
<!-- Describe the vulnerability -->

## Impact
<!-- What can an attacker do? -->

## Affected Versions
<!-- Which versions are affected? -->

- **First Affected**: [e.g., v0.1.0]
- **Last Affected**: [e.g., v0.2.0]
- **Fixed In**: [if known]

## Proof of Concept
<!-- Provide a PoC if possible -->

```c
/* PoC code */
```

## Steps to Reproduce
1. 
2. 
3. 

## Mitigation
<!-- Temporary workaround if available -->

## Suggested Fix
<!-- If you have a fix suggestion -->

## References
<!-- Related CVEs, advisories, or documentation -->

- CVE: 
- CWE: 
- References:

## Disclosure Timeline
<!-- When did you discover this? -->

- **Discovery Date**: [YYYY-MM-DD]
- **Vendor Notification**: [YYYY-MM-DD]
- **Public Disclosure**: [YYYY-MM-DD]

## Checklist

- [ ] I have not disclosed this publicly
- [ ] I have provided sufficient detail
- [ ] I have considered the impact
- [ ] I have suggested a fix (if possible)
