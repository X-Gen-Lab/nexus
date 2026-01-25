---
name: Performance Issue
about: Report performance problems or suggest optimizations
title: '[PERF] '
labels: performance
assignees: ''
---

## Performance Issue Type
<!-- Check the type of performance issue -->

- [ ] Slow execution time
- [ ] High memory usage
- [ ] Large code size
- [ ] Inefficient algorithm
- [ ] Resource leak
- [ ] Other: ___________

## Affected Component
<!-- Which component has the performance issue? -->

- [ ] HAL (specify module): ___________
- [ ] OSAL
- [ ] Framework (Config/Log/Shell/Init)
- [ ] Build System
- [ ] Other: ___________

## Environment
<!-- Platform and configuration -->

- **Platform**: [e.g., native, stm32f4, stm32h7]
- **Build Type**: [e.g., Debug, Release, MinSizeRel]
- **Compiler**: [e.g., GCC 12.2, arm-none-eabi-gcc 10.3]
- **Optimization Level**: [e.g., -O0, -O2, -Os]
- **Nexus Version**: [e.g., v0.1.0 or commit hash]

## Performance Metrics
<!-- Provide measurable data -->

### Current Performance
- **Execution Time**: [e.g., 100 ms, 1000 cycles]
- **Memory Usage**: [e.g., 10 KB RAM, 50 KB Flash]
- **Code Size**: [e.g., 5 KB]
- **CPU Usage**: [e.g., 80%]

### Expected Performance
- **Target Execution Time**: [e.g., < 50 ms]
- **Target Memory Usage**: [e.g., < 5 KB RAM]
- **Target Code Size**: [e.g., < 3 KB]
- **Target CPU Usage**: [e.g., < 50%]

## Reproduction
<!-- How to reproduce the performance issue -->

### Code Sample
```c
/* Code that demonstrates the performance issue */
```

### Profiling Data
<!-- If you have profiling data, include it -->

<details>
<summary>Profiling Output</summary>

```
Paste profiling data here (gprof, perf, etc.)
```

</details>

### Benchmark Results
<!-- If you have benchmark results -->

| Metric | Current | Target | Difference |
|--------|---------|--------|------------|
| Time   |         |        |            |
| Memory |         |        |            |
| Size   |         |        |            |

## Root Cause Analysis
<!-- If you've identified the cause -->

## Proposed Optimization
<!-- Describe your optimization idea -->

### Implementation Approach
<!-- How would you optimize this? -->

### Expected Improvement
<!-- What improvement do you expect? -->

- **Execution Time**: [e.g., 50% faster]
- **Memory Usage**: [e.g., 30% less RAM]
- **Code Size**: [e.g., 20% smaller]

### Trade-offs
<!-- Any trade-offs with this optimization? -->

- Pros:
- Cons:

## Additional Context
<!-- Add any other context -->

- Related issues: #
- Profiling tools used:
- Benchmark methodology:

## Contribution
<!-- Can you help with this optimization? -->

- [ ] I can implement the optimization
- [ ] I can provide benchmarks
- [ ] I need help from maintainers

## Checklist

- [ ] I have measured the performance issue
- [ ] I have provided profiling data
- [ ] I have considered trade-offs
- [ ] I have searched existing issues
