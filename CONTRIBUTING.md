# Contributing to Nexus

Thank you for your interest in contributing to Nexus! This document provides guidelines for contributing.

## Code of Conduct

Please be respectful and constructive in all interactions.

## Development Environment Setup

### Prerequisites

```bash
# Windows
winget install Kitware.CMake
winget install Git.Git
# Install Visual Studio 2019+ or Build Tools

# Linux (Ubuntu/Debian)
sudo apt-get install cmake gcc g++ git

# macOS
brew install cmake git
```

### Clone and Build

```bash
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# Native build (for testing)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Debug

# Run tests
ctest --test-dir build -C Debug --output-on-failure
```

### IDE Setup

**VS Code** (recommended):
- Open the `nexus` folder
- Install recommended extensions (C/C++, CMake Tools)
- Use `.vscode/` configurations provided

## How to Contribute

### Reporting Bugs

1. Check existing issues to avoid duplicates
2. Use the bug report template
3. Include:
   - Platform and version (Windows/Linux/macOS, compiler version)
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant logs or screenshots

### Suggesting Features

1. Check existing feature requests
2. Use the feature request template
3. Describe the use case and benefits

### Pull Requests

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Ensure tests pass: `ctest --test-dir build -C Debug`
5. Follow code style guidelines
6. Commit with conventional commits: `feat(hal): add PWM support`
7. Push and create a Pull Request

## Code Style

### C Code

- Follow `.clang-format` configuration
- Use Doxygen comments with **backslash style** (`\brief`, `\param`)
- Maximum line length: 80 characters
- Indent: 4 spaces (no tabs)
- Pointer alignment: left (`char* ptr`, not `char *ptr`)

### Doxygen Comment Style

Use **backslash style** for all Doxygen comments:

```c
/**
 * \file            hal_gpio.h
 * \brief           GPIO Hardware Abstraction Layer
 * \author          Your Name
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

/**
 * \brief           Initialize GPIO pin
 * \param[in]       port: GPIO port enumeration
 * \param[in]       pin: Pin number (0-15)
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 * \note            Pin must be deinitialized before re-initialization
 */
hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                           const hal_gpio_config_t* config);
```

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Files | snake_case | `hal_gpio.c` |
| Functions | snake_case | `hal_gpio_init()` |
| Types | snake_case_t | `hal_gpio_config_t` |
| Macros | UPPER_CASE | `HAL_GPIO_PORT_MAX` |
| Enums | UPPER_CASE | `HAL_GPIO_DIR_INPUT` |

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`

## Testing

### Running Tests

```bash
# Build with tests
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Release

# Run all tests
ctest --test-dir build -C Release --output-on-failure

# Run specific test suite
./build/tests/Release/nexus_tests --gtest_filter="HalGpioTest.*"

# Run with verbose output
./build/tests/Release/nexus_tests --gtest_filter="*" --gtest_print_time=1
```

### Writing Tests

Tests use Google Test framework. Place test files in `tests/` directory:

```cpp
// tests/hal/test_hal_gpio.cpp
#include <gtest/gtest.h>

extern "C" {
#include "hal/hal_gpio.h"
}

class HalGpioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(HalGpioTest, InitOutput) {
    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };
    EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
}
```

### Test Requirements

- Write unit tests for new features
- Maintain ≥90% code coverage
- Run tests before submitting PR
- Tests must pass on all platforms (Windows, Linux, macOS)

## Documentation

### Building Documentation

```bash
# API documentation (Doxygen)
doxygen Doxyfile
# Output: docs/api/html/index.html

# User documentation (Sphinx) - English
cd docs/sphinx
python -m sphinx -b html . _build/html/en

# User documentation (Sphinx) - Chinese
python -m sphinx -b html . _build/html/cn -D master_doc=index_cn -D language=zh_CN
```

### Documentation Guidelines

- Update API documentation for public interfaces
- Add examples for new features
- Keep README up to date
- Support both English and Chinese where applicable

## CI/CD

All PRs trigger GitHub Actions workflows:

| Workflow | Description |
|----------|-------------|
| `build.yml` | Multi-platform build (Windows, Linux, macOS) + ARM cross-compilation |
| `test.yml` | Unit tests, coverage, sanitizers, MISRA checks |

### Local CI Verification

Before submitting a PR, verify locally:

```bash
# 1. Build passes
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Release

# 2. Tests pass
ctest --test-dir build -C Release --output-on-failure

# 3. Code format check
clang-format --dry-run --Werror hal/**/*.c hal/**/*.h

# 4. Documentation builds
doxygen Doxyfile
```

## Review Process

1. Automated CI checks must pass
2. At least one maintainer approval required
3. Address all review comments
4. Squash commits if requested

## Questions?

Open a discussion or reach out to maintainers.

Thank you for contributing! 🎉
