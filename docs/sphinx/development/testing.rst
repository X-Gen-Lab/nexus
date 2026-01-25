Testing
=======

Nexus uses Google Test for unit testing and provides comprehensive testing
infrastructure for embedded software development.

Test Framework
--------------

- **Unit Testing**: Google Test (gtest)
- **Mocking**: Google Mock (gmock)
- **Coverage**: gcov/lcov
- **Static Analysis**: MISRA C checker

Building Tests
--------------

Configure and build with tests enabled::

    CMake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    CMake --build build --config Release

Running Tests
-------------

Run All Tests
~~~~~~~~~~~~~



    ctest --test-dir build -C Release --output-on-failure

Run Specific Test Suite
~~~~~~~~~~~~~~~~~~~~~~~



    ./build/tests/Release/nexus_tests --gtest_filter="HalGpioTest.\*"

Run with Verbose Output
~~~~~~~~~~~~~~~~~~~~~~~



    ./build/tests/Release/nexus_tests --gtest_filter="\*" --gtest_print_time=1

List Available Tests
~~~~~~~~~~~~~~~~~~~~



    ./build/tests/Release/nexus_tests --gtest_list_tests

Writing Tests
-------------

Test File Location
~~~~~~~~~~~~~~~~~~

Place test files in the ``tests/`` directory with the following structure::

    tests/
    ├── hal/
    │   ├── test_hal_gpio.cpp
    │   ├── test_hal_uart.cpp
    │   └── test_hal_spi.cpp
    ├── osal/
    │   ├── test_osal_task.cpp
    │   ├── test_osal_mutex.cpp
    │   └── test_osal_queue.cpp
    └── framework/
        └── log/
            └── test_log.cpp

Test File Template
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    /**
     * \file            test_hal_gpio.cpp
     * \brief           HAL GPIO unit tests
     */

    #include <gtest/gtest.h>

    extern "C" {
    #include "hal/hal_gpio.h"
    }

    class HalGpioTest : public ::testing::Test {
    protected:
        void SetUp() override {
            /* Initialize test fixtures */
        }

        void TearDown() override {
            /* Clean up test fixtures */
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

    TEST_F(HalGpioTest, InitNullConfig) {
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_gpio_init(HAL_GPIO_PORT_A, 0, nullptr));
    }

    TEST_F(HalGpioTest, WriteOutput) {
        /* Setup */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT
        };
        hal_gpio_init(HAL_GPIO_PORT_A, 0, &config);

        /* Test */
        EXPECT_EQ(HAL_OK, hal_gpio_write(HAL_GPIO_PORT_A, 0, HAL_GPIO_LEVEL_HIGH));
    }

Test Naming Convention
~~~~~~~~~~~~~~~~~~~~~~

- Test class: ``ModuleNameTest`` (e.g., ``HalGpioTest``, ``OsalMutexTest``)
- Test case: Descriptive action (e.g., ``InitOutput``, ``WriteInvalidPort``)

Test Categories
~~~~~~~~~~~~~~~

1. **Positive Tests**: Verify correct behavior with valid inputs
2. **Negative Tests**: Verify error handling with invalid inputs
3. **Boundary Tests**: Test edge cases and limits
4. **Integration Tests**: Test component interactions

Code Coverage
-------------

Enable Coverage
~~~~~~~~~~~~~~~



    CMake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON -DNEXUS_ENABLE_COVERAGE=ON
    CMake --build build

Generate Coverage Report
~~~~~~~~~~~~~~~~~~~~~~~~



    cd build
    ctest --output-on-failure
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/\*' '\*/tests/\*' --output-file coverage.info
    genhtml coverage.info --output-directory coverage_report

Coverage Requirements
~~~~~~~~~~~~~~~~~~~~~

- Minimum code coverage: **90%**
- All public APIs must have tests
- All error paths must be tested

Static Analysis
---------------

MISRA C Compliance
~~~~~~~~~~~~~~~~~~

Run MISRA C checker::

    # Using cppcheck with MISRA addon
    cppcheck --addon=misra hal/ osal/ framework/

Address Sanitizer
~~~~~~~~~~~~~~~~~

Build with sanitizers for memory error detection::

    CMake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON \
          -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"

Best Practices
--------------

1. **Test Independence**: Each test should be independent and not rely on other tests
2. **Clear Assertions**: Use descriptive assertion messages
3. **Setup/Teardown**: Use fixtures for common setup and cleanup
4. **Mock External Dependencies**: Use mocks for hardware and OS dependencies
5. **Test Edge Cases**: Include boundary conditions and error cases
6. **Keep Tests Fast**: Unit tests should run quickly
7. **Avoid Test Duplication**: Use parameterized tests for similar cases

Parameterized Tests
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    class HalGpioPortTest : public ::testing::TestWithParam<hal_gpio_port_t> {};

    TEST_P(HalGpioPortTest, InitValidPort) {
        hal_gpio_port_t port = GetParam();
        hal_gpio_config_t config = { .direction = HAL_GPIO_DIR_OUTPUT };
        EXPECT_EQ(HAL_OK, hal_gpio_init(port, 0, &config));
    }

    INSTANTIATE_TEST_SUITE_P(
        AllPorts,
        HalGpioPortTest,
        ::testing::Values(
            HAL_GPIO_PORT_A,
            HAL_GPIO_PORT_B,
            HAL_GPIO_PORT_C
        )
    );

CI Integration
--------------

Tests run automatically on every PR via GitHub Actions:

- Multi-platform testing (Windows, Linux, macOS)
- Coverage reporting
- Memory sanitizer checks
- MISRA compliance checks
