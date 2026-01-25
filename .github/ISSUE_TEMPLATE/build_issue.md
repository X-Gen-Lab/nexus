---
name: Build/Compilation Issue
about: Report problems with building or compiling the project
title: '[BUILD] '
labels: build
assignees: ''
---

## Build Issue Type
<!-- Check the type of build issue -->

- [ ] CMake configuration fails
- [ ] Compilation error
- [ ] Linking error
- [ ] Dependency issue
- [ ] Toolchain problem
- [ ] Cross-compilation issue
- [ ] Other: ___________

## Environment
<!-- Please complete the following information -->

- **OS**: [e.g., Windows 11, Ubuntu 22.04, macOS 14]
- **Compiler**: [e.g., MSVC 19.41, GCC 12.2, arm-none-eabi-gcc 10.3]
- **CMake Version**: [e.g., 3.28]
- **Python Version**: [e.g., 3.11]
- **Make/Ninja Version**: [if applicable]
- **Platform Target**: [e.g., native, stm32f4, stm32h7]
- **Build Type**: [e.g., Debug, Release]
- **Nexus Version**: [e.g., v0.1.0 or commit hash]

## Build Configuration
<!-- Your CMake configuration command -->

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEXUS_PLATFORM=stm32f4 \
  # ... other options
```

## Build Command
<!-- The command that failed -->

```bash
cmake --build build --config Release
```

## Error Output
<!-- Full error output -->

```
Paste complete error output here
```

## CMake Output
<!-- CMake configuration output if relevant -->

<details>
<summary>CMake Configuration Output</summary>

```
Paste CMake output here
```

</details>

## Build Log
<!-- Full build log if available -->

<details>
<summary>Build Log</summary>

```
Paste build log here
```

</details>

## Steps Taken
<!-- What have you tried to fix this? -->

1. 
2. 
3. 

## Expected Behavior
<!-- What should happen? -->

## Additional Context
<!-- Add any other context -->

- Related issues: #
- Works on other platforms: [Yes/No]
- Clean build attempted: [Yes/No]

## Checklist

- [ ] I have tried a clean build
- [ ] I have checked CMake version compatibility
- [ ] I have verified toolchain installation
- [ ] I have searched existing issues
- [ ] I have provided complete error output
