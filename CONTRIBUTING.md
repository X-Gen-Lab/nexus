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

### Testing Requirements

**All contributions must include appropriate tests.** Testing is a critical part of maintaining code quality and preventing regressions.

#### When Adding New Features

1. **Unit Tests Required**: Write unit tests that verify specific examples and edge cases
2. **Property Tests Recommended**: For HAL implementations, write property-based tests that verify universal properties across many inputs
3. **Test Coverage**: New code must maintain or improve overall coverage
4. **Test Documentation**: Include clear comments explaining what each test validates

#### When Modifying Existing Code

1. **Run All Tests**: Ensure all existing tests still pass
2. **Update Tests**: Modify tests if behavior changes are intentional
3. **Add Tests**: Add new tests for newly covered scenarios
4. **No Regression**: Do not reduce test coverage

#### When Removing Features

1. **Remove Tests**: Delete tests for removed functionality
2. **Update Dependencies**: Update tests that depend on removed features
3. **Verify Build**: Ensure test suite still compiles and runs

### Coverage Requirements

**Target**: 100% code coverage for Native platform HAL implementations

**Minimum Requirements**:
- Line coverage: ≥95%
- Branch coverage: ≥95%
- Function coverage: ≥95%

**Coverage Verification**:
```bash
# Generate coverage report (Linux/WSL)
cd scripts/coverage
./run_coverage_linux.sh

# Generate coverage report (Windows)
cd scripts\coverage
.\run_coverage_windows.ps1

# View report
# Linux: xdg-open ../../coverage_html/index.html
# Windows: start ..\..\coverage_report\html\index.html
```

**Coverage Enforcement**:
- CI/CD pipeline automatically checks coverage
- PRs that reduce coverage below threshold will be flagged
- Maintainers may request additional tests to meet coverage requirements

### Running Tests

#### Quick Start

```bash
# Build with tests
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build --config Debug

# Run all tests
cd build && ctest --output-on-failure

# Or run test executable directly
./build/tests/nexus_tests
```

#### Running Specific Tests

```bash
# Run specific test suite
./build/tests/nexus_tests --gtest_filter="GPIO*"

# Run specific test case
./build/tests/nexus_tests --gtest_filter="GPIOTest.BasicInitialization"

# Run multiple test suites
./build/tests/nexus_tests --gtest_filter="GPIO*:UART*:SPI*"

# Run with verbose output
./build/tests/nexus_tests --gtest_verbose
```

#### Platform-Specific Notes

**Linux/WSL (Recommended for Native Platform)**:
- Full test support
- Coverage tools: lcov or gcovr
- All tests should pass

**Windows (MSVC)**:
- Native platform has known device registration issues
- Use WSL for Native platform testing
- Other platforms work correctly

### Writing Tests

#### Test File Organization

Tests use Google Test framework. Each peripheral should have two test files:

1. **Unit Tests**: `tests/hal/test_nx_<peripheral>.cpp`
   - Verify specific examples and edge cases
   - Test error handling and boundary conditions
   - Test integration between components

2. **Property Tests**: `tests/hal/test_nx_<peripheral>_properties.cpp`
   - Verify universal properties across random inputs
   - Run minimum 100 iterations per property
   - Test correctness properties from design document

#### Unit Test Example

```cpp
/**
 * \file            test_nx_gpio.cpp
 * \brief           GPIO HAL unit tests
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

class GPIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        native_gpio_reset_all();
        gpio = nx_factory_gpio(0);
        ASSERT_NE(nullptr, gpio);
    }
    
    void TearDown() override {
        if (gpio != nullptr && gpio->deinit != nullptr) {
            gpio->deinit(gpio);
        }
        native_gpio_reset_all();
    }
    
    nx_gpio_t* gpio = nullptr;
};

TEST_F(GPIOTest, BasicInitialization) {
    nx_gpio_config_t config = {
        .mode = NX_GPIO_MODE_OUTPUT,
        .pull = NX_GPIO_PULL_NONE,
        .level = NX_GPIO_LEVEL_LOW
    };
    
    ASSERT_EQ(NX_OK, gpio->init(gpio, 5, &config));
    
    /* Verify state */
    native_gpio_state_t state;
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 5, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(GPIOTest, ErrorHandling_NullPointer) {
    EXPECT_NE(NX_OK, gpio->init(nullptr, 0, nullptr));
}
```

#### Property Test Example

```cpp
/**
 * \file            test_nx_gpio_properties.cpp
 * \brief           GPIO HAL property-based tests
 */

#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

class GPIOPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng.seed(std::random_device{}());
        native_gpio_reset_all();
        gpio = nx_factory_gpio(0);
        ASSERT_NE(nullptr, gpio);
    }
    
    void TearDown() override {
        if (gpio != nullptr && gpio->deinit != nullptr) {
            gpio->deinit(gpio);
        }
        native_gpio_reset_all();
    }
    
    std::mt19937 rng;
    nx_gpio_t* gpio = nullptr;
};

/**
 * Feature: native-hal-validation, Property 11: GPIO Read-Write Consistency
 *
 * *For any* GPIO pin and level value, writing then immediately reading
 * should return the same level value.
 *
 * **Validates: Requirements 1.2, 1.3**
 */
TEST_F(GPIOPropertyTest, Property11_ReadWriteConsistency) {
    const int iterations = 100;
    
    for (int i = 0; i < iterations; ++i) {
        /* Generate random pin and level */
        std::uniform_int_distribution<uint8_t> pin_dist(0, 15);
        uint8_t pin = pin_dist(rng);
        
        nx_gpio_level_t level = (rng() % 2) ? NX_GPIO_LEVEL_HIGH : NX_GPIO_LEVEL_LOW;
        
        /* Initialize pin as output */
        nx_gpio_config_t config = {
            .mode = NX_GPIO_MODE_OUTPUT,
            .pull = NX_GPIO_PULL_NONE,
            .level = NX_GPIO_LEVEL_LOW
        };
        ASSERT_EQ(NX_OK, gpio->init(gpio, pin, &config));
        
        /* Write level */
        ASSERT_EQ(NX_OK, gpio->write(gpio, pin, level));
        
        /* Read level */
        nx_gpio_level_t read_level;
        ASSERT_EQ(NX_OK, gpio->read(gpio, pin, &read_level));
        
        /* Verify consistency */
        EXPECT_EQ(level, read_level) << "Iteration " << i << ", Pin " << (int)pin;
        
        /* Cleanup */
        gpio->deinit(gpio);
        native_gpio_reset(0);
    }
}
```

#### Test Helper Functions

For Native platform testing, use test helper functions to:
- Query internal peripheral state
- Inject receive data (simulate hardware)
- Capture transmit data (verify output)
- Advance time (for timers)
- Reset peripherals to clean state

Example:
```cpp
#include "tests/hal/native/devices/native_uart_helpers.h"

/* Inject data to simulate hardware reception */
uint8_t rx_data[] = {0x01, 0x02, 0x03};
native_uart_inject_rx_data(0, rx_data, sizeof(rx_data));

/* Capture transmitted data */
uint8_t tx_buffer[10];
size_t tx_len = sizeof(tx_buffer);
native_uart_get_tx_data(0, tx_buffer, &tx_len);

/* Query internal state */
native_uart_state_t state;
native_uart_get_state(0, &state);
```

### Test Best Practices

1. **Independence**: Each test should run independently
2. **Repeatability**: Tests should produce consistent results
3. **Clear Assertions**: Use descriptive assertion messages
4. **Single Concept**: Each test should verify one concept
5. **Cleanup**: Always clean up resources in TearDown()
6. **Documentation**: Comment what each test validates

### Detailed Testing Guide

For comprehensive testing documentation, see:
- **[Native Platform Testing Guide](tests/hal/native/TESTING_GUIDE.md)**: Complete guide for running tests, adding new tests, using test helpers, and generating coverage reports
- **[Coverage Analysis Guide](docs/testing/COVERAGE_ANALYSIS.md)**: Detailed coverage analysis and improvement strategies
- **[Coverage Scripts README](scripts/coverage/README.md)**: Coverage script usage and options

### Pre-Submission Checklist

Before submitting a PR, verify:

- [ ] All new code has corresponding tests
- [ ] All tests pass locally: `cd build && ctest --output-on-failure`
- [ ] Coverage meets requirements (≥95% or maintains 100%)
- [ ] Property tests run minimum 100 iterations
- [ ] Test code follows Nexus coding standards
- [ ] Test documentation is clear and complete
- [ ] No test warnings or errors

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
