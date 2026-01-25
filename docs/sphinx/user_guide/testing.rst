Testing Guide
=============

This comprehensive guide covers testing strategies, frameworks, and best practices for Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Testing is critical for embedded systems reliability. Nexus provides comprehensive testing infrastructure including unit tests, integration tests, and property-based tests.

**Testing Pyramid:**

.. code-block:: text

   ┌─────────────────┐
   │  System Tests   │  ← Few, slow, expensive
   ├─────────────────┤
   │ Integration     │  ← Some, medium speed
   │     Tests       │
   ├─────────────────┤
   │   Unit Tests    │  ← Many, fast, cheap
   └─────────────────┘

**Key Concepts:**

* Unit testing with Google Test
* Integration testing
* Property-based testing with Hypothesis
* Hardware-in-the-loop (HIL) testing
* Code coverage analysis
* Continuous integration

Test Infrastructure
-------------------

Test Framework
~~~~~~~~~~~~~~

Nexus uses **Google Test** (gtest) for C++ tests and provides C wrappers for testing C code.

**Features:**

* Rich assertion macros
* Test fixtures for setup/teardown
* Parameterized tests
* Death tests
* Test discovery
* XML/JSON output

**Directory Structure:**

.. code-block:: text

   tests/
   ├── hal/                    # HAL unit tests
   │   ├── test_gpio.cpp
   │   ├── test_uart.cpp
   │   └── ...
   ├── osal/                   # OSAL unit tests
   │   ├── test_task.cpp
   │   ├── test_mutex.cpp
   │   └── ...
   ├── framework/              # Framework tests
   │   ├── log/
   │   ├── shell/
   │   └── config/
   ├── integration/            # Integration tests
   │   ├── test_hal_osal.cpp
   │   └── ...
   └── property/               # Property-based tests
       ├── test_gpio_properties.cpp
       └── ...

Building Tests
~~~~~~~~~~~~~~

**CMake Configuration:**

.. code-block:: bash

   # Enable tests (native platform only)
   cmake -B build \
       -DNEXUS_PLATFORM=native \
       -DNEXUS_BUILD_TESTS=ON \
       -DCMAKE_BUILD_TYPE=Debug

   # Build tests
   cmake --build build

   # Build specific test
   cmake --build build --target test_gpio

**Test Targets:**

* ``test_hal`` - All HAL tests
* ``test_osal`` - All OSAL tests
* ``test_log`` - Log framework tests
* ``test_shell`` - Shell framework tests
* ``test_config`` - Config framework tests
* ``test_integration`` - Integration tests

Running Tests
~~~~~~~~~~~~~

**Run All Tests:**

.. code-block:: bash

   # Using Python script (recommended)
   python scripts/test/test.py

   # Using CTest
   cd build
   ctest -C Debug --output-on-failure

**Run Specific Tests:**

.. code-block:: bash

   # Run specific test suite
   python scripts/test/test.py -f "GPIO*"

   # Run specific test case
   ./build/tests/hal/test_gpio --gtest_filter="GPIOTest.WriteRead"

   # Run with verbose output
   python scripts/test/test.py -v

**Test Options:**

.. code-block:: bash

   # List all tests
   ./build/tests/hal/test_gpio --gtest_list_tests

   # Run tests matching pattern
   ./build/tests/hal/test_gpio --gtest_filter="GPIO*"

   # Repeat tests
   ./build/tests/hal/test_gpio --gtest_repeat=10

   # Shuffle test order
   ./build/tests/hal/test_gpio --gtest_shuffle

   # Generate XML output
   ./build/tests/hal/test_gpio --gtest_output=xml:results.xml

Unit Testing
------------

Writing Unit Tests
~~~~~~~~~~~~~~~~~~

**Basic Test Structure:**

.. code-block:: cpp

   #include <gtest/gtest.h>
   extern "C" {
   #include "hal/nx_gpio.h"
   #include "hal/nx_factory.h"
   }

   // Test fixture
   class GPIOTest : public ::testing::Test {
   protected:
       void SetUp() override {
           // Setup before each test
           nx_hal_init();
       }

       void TearDown() override {
           // Cleanup after each test
           nx_hal_deinit();
       }
   };

   // Test case
   TEST_F(GPIOTest, WriteRead) {
       // Arrange
       nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);
       ASSERT_NE(gpio, nullptr);

       // Act
       gpio->write(gpio, 1);
       uint8_t value = gpio->read(gpio);

       // Assert
       EXPECT_EQ(value, 1);

       // Cleanup
       nx_factory_gpio_release((nx_gpio_t*)gpio);
   }

**Assertion Macros:**

.. code-block:: cpp

   // Fatal assertions (stop test on failure)
   ASSERT_TRUE(condition);
   ASSERT_FALSE(condition);
   ASSERT_EQ(expected, actual);
   ASSERT_NE(val1, val2);
   ASSERT_LT(val1, val2);
   ASSERT_LE(val1, val2);
   ASSERT_GT(val1, val2);
   ASSERT_GE(val1, val2);
   ASSERT_STREQ(str1, str2);
   ASSERT_STRNE(str1, str2);
   ASSERT_NEAR(val1, val2, abs_error);

   // Non-fatal assertions (continue test on failure)
   EXPECT_TRUE(condition);
   EXPECT_FALSE(condition);
   EXPECT_EQ(expected, actual);
   // ... (same as ASSERT_* variants)

**Testing C Functions:**

.. code-block:: cpp

   extern "C" {
   #include "my_module.h"
   }

   TEST(MyModuleTest, FunctionTest) {
       int result = my_c_function(42);
       EXPECT_EQ(result, 84);
   }

Test Fixtures
~~~~~~~~~~~~~

**Shared Setup/Teardown:**

.. code-block:: cpp

   class UARTTest : public ::testing::Test {
   protected:
       nx_uart_t* uart;

       void SetUp() override {
           nx_hal_init();
           uart = nx_factory_uart(0);
           ASSERT_NE(uart, nullptr);
       }

       void TearDown() override {
           if (uart) {
               nx_factory_uart_release(uart);
           }
           nx_hal_deinit();
       }
   };

   TEST_F(UARTTest, SendReceive) {
       // uart is available here
       nx_tx_sync_t* tx = uart->get_tx_sync(uart);
       ASSERT_NE(tx, nullptr);
       // ... test code
   }

   TEST_F(UARTTest, Baudrate) {
       // uart is available here too
       // ... test code
   }

**Parameterized Tests:**

.. code-block:: cpp

   class GPIOPinTest : public ::testing::TestWithParam<uint8_t> {
   protected:
       void SetUp() override {
           nx_hal_init();
       }

       void TearDown() override {
           nx_hal_deinit();
       }
   };

   TEST_P(GPIOPinTest, AllPins) {
       uint8_t pin = GetParam();
       nx_gpio_write_t* gpio = nx_factory_gpio_write('A', pin);
       ASSERT_NE(gpio, nullptr);

       gpio->write(gpio, 1);
       EXPECT_EQ(gpio->read(gpio), 1);

       nx_factory_gpio_release((nx_gpio_t*)gpio);
   }

   // Test pins 0-15
   INSTANTIATE_TEST_SUITE_P(
       AllPins,
       GPIOPinTest,
       ::testing::Range<uint8_t>(0, 16)
   );

Mocking
~~~~~~~

**Manual Mocks:**

.. code-block:: cpp

   // Mock HAL for testing upper layers
   class MockGPIO : public nx_gpio_write_t {
   public:
       uint8_t last_written_value = 0;
       uint8_t read_value = 0;

       static void mock_write(nx_gpio_write_t* self, uint8_t value) {
           MockGPIO* mock = static_cast<MockGPIO*>(self);
           mock->last_written_value = value;
       }

       static uint8_t mock_read(nx_gpio_write_t* self) {
           MockGPIO* mock = static_cast<MockGPIO*>(self);
           return mock->read_value;
       }

       MockGPIO() {
           this->write = mock_write;
           this->read = mock_read;
       }
   };

   TEST(ApplicationTest, UsesGPIO) {
       MockGPIO mock_gpio;
       mock_gpio.read_value = 1;

       // Test application code with mock
       application_function(&mock_gpio);

       EXPECT_EQ(mock_gpio.last_written_value, 1);
   }

**Google Mock (gmock):**

.. code-block:: cpp

   #include <gmock/gmock.h>

   class MockUART {
   public:
       MOCK_METHOD(int, send, (const uint8_t* data, size_t len), ());
       MOCK_METHOD(int, receive, (uint8_t* data, size_t len), ());
   };

   TEST(ProtocolTest, SendsCorrectData) {
       MockUART mock_uart;

       // Expect send to be called with specific data
       EXPECT_CALL(mock_uart, send(_, 10))
           .Times(1)
           .WillOnce(::testing::Return(10));

       // Test code that uses mock_uart
       protocol_send(&mock_uart, data, 10);
   }

Integration Testing
-------------------

HAL + OSAL Integration
~~~~~~~~~~~~~~~~~~~~~~

**Test Multiple Layers:**

.. code-block:: cpp

   TEST(IntegrationTest, GPIOWithTask) {
       // Initialize both layers
       nx_hal_init();
       osal_init();

       // Create GPIO
       nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
       ASSERT_NE(led, nullptr);

       // Create task that uses GPIO
       auto task_func = [](void* arg) {
           nx_gpio_write_t* gpio = static_cast<nx_gpio_write_t*>(arg);
           for (int i = 0; i < 10; i++) {
               gpio->toggle(gpio);
               osal_task_delay(100);
           }
       };

       osal_task_handle_t task;
       osal_task_create(task_func, "test", 512, led, 1, &task);

       // Wait for task to complete
       osal_task_delay(1500);

       // Cleanup
       osal_task_delete(task);
       nx_factory_gpio_release((nx_gpio_t*)led);
       osal_deinit();
       nx_hal_deinit();
   }

Framework Integration
~~~~~~~~~~~~~~~~~~~~~

**Test Framework Components:**

.. code-block:: cpp

   TEST(IntegrationTest, LogWithUART) {
       // Initialize HAL and Log framework
       nx_hal_init();

       nx_uart_t* uart = nx_factory_uart(0);
       ASSERT_NE(uart, nullptr);

       log_init(nullptr);
       log_backend_t* backend = log_backend_uart_create(uart);
       log_backend_register(backend);

       // Test logging
       LOG_INFO("Test message");

       // Verify message was sent (check UART mock)
       // ...

       // Cleanup
       log_backend_unregister("uart");
       log_backend_uart_destroy(backend);
       log_deinit();
       nx_factory_uart_release(uart);
       nx_hal_deinit();
   }

Property-Based Testing
----------------------

Overview
~~~~~~~~

Property-based testing generates random inputs to verify properties that should always hold true.

**Benefits:**

* Finds edge cases
* Tests many scenarios automatically
* Documents expected behavior
* Complements example-based tests

Using Hypothesis
~~~~~~~~~~~~~~~~

**Python Property Tests:**

.. code-block:: python

   from hypothesis import given, strategies as st
   import ctypes

   # Load native library
   lib = ctypes.CDLL('./build/libhal.so')

   @given(st.integers(min_value=0, max_value=255))
   def test_gpio_write_read(value):
       """Property: Written value should be readable"""
       gpio = lib.nx_factory_gpio_write(ord('A'), 5)
       assert gpio is not None

       lib.nx_gpio_write(gpio, value)
       read_value = lib.nx_gpio_read(gpio)

       assert read_value == value

       lib.nx_factory_gpio_release(gpio)

**C++ Property Tests:**

.. code-block:: cpp

   #include <gtest/gtest.h>
   #include <random>

   TEST(GPIOPropertyTest, WriteReadProperty) {
       std::random_device rd;
       std::mt19937 gen(rd());
       std::uniform_int_distribution<> dis(0, 1);

       nx_hal_init();
       nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);
       ASSERT_NE(gpio, nullptr);

       // Test 1000 random values
       for (int i = 0; i < 1000; i++) {
           uint8_t value = dis(gen);
           gpio->write(gpio, value);
           uint8_t read = gpio->read(gpio);
           EXPECT_EQ(read, value) << "Failed on iteration " << i;
       }

       nx_factory_gpio_release((nx_gpio_t*)gpio);
       nx_hal_deinit();
   }

Common Properties
~~~~~~~~~~~~~~~~~

**Idempotence:**

.. code-block:: cpp

   TEST(PropertyTest, SetModeIdempotent) {
       // Setting mode twice should have same effect as once
       nx_gpio_t* gpio = nx_factory_gpio('A', 5);

       gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP);
       gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP);

       // Verify mode is set correctly
       // ...
   }

**Commutativity:**

.. code-block:: cpp

   TEST(PropertyTest, ConfigOrderIndependent) {
       // Order of configuration shouldn't matter
       nx_gpio_t* gpio1 = nx_factory_gpio('A', 5);
       gpio1->set_mode(gpio1, NX_GPIO_MODE_OUTPUT_PP);
       gpio1->set_pull(gpio1, NX_GPIO_PULL_UP);

       nx_gpio_t* gpio2 = nx_factory_gpio('A', 6);
       gpio2->set_pull(gpio2, NX_GPIO_PULL_UP);
       gpio2->set_mode(gpio2, NX_GPIO_MODE_OUTPUT_PP);

       // Both should behave identically
       // ...
   }

**Inverse Operations:**

.. code-block:: cpp

   TEST(PropertyTest, ToggleTwiceReturnsOriginal) {
       nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);

       gpio->write(gpio, 0);
       gpio->toggle(gpio);
       gpio->toggle(gpio);

       EXPECT_EQ(gpio->read(gpio), 0);
   }

Code Coverage
-------------

Measuring Coverage
~~~~~~~~~~~~~~~~~~

**Enable Coverage:**

.. code-block:: bash

   # Configure with coverage
   cmake -B build \
       -DNEXUS_PLATFORM=native \
       -DNEXUS_BUILD_TESTS=ON \
       -DNEXUS_ENABLE_COVERAGE=ON \
       -DCMAKE_BUILD_TYPE=Debug

   # Build and run tests
   cmake --build build
   cd build
   ctest

**Generate Coverage Report:**

.. code-block:: bash

   # Linux/macOS
   cd scripts/coverage
   ./run_coverage_linux.sh

   # Windows
   cd scripts\coverage
   .\run_coverage_windows.ps1

**View Report:**

.. code-block:: bash

   # Linux
   xdg-open ../../coverage_html/index.html

   # macOS
   open ../../coverage_html/index.html

   # Windows
   start ..\..\coverage_report\html\index.html

Coverage Targets
~~~~~~~~~~~~~~~~

**Nexus Coverage Goals:**

* **HAL**: 100% line coverage for native platform
* **OSAL**: 100% line coverage for all backends
* **Framework**: >95% line coverage
* **Integration**: >90% line coverage

**Coverage Metrics:**

* **Line Coverage**: Percentage of lines executed
* **Function Coverage**: Percentage of functions called
* **Branch Coverage**: Percentage of branches taken

**Example Report:**

.. code-block:: text

   File                    Lines    Exec    Cover
   ------------------------------------------------
   hal/src/nx_gpio.c        245     245    100.0%
   hal/src/nx_uart.c        312     308     98.7%
   hal/src/nx_spi.c         198     195     98.5%
   osal/src/task.c          156     156    100.0%
   framework/log/log.c      423     418     98.8%
   ------------------------------------------------
   TOTAL                   1334    1322     99.1%

Improving Coverage
~~~~~~~~~~~~~~~~~~

**Identify Uncovered Code:**

.. code-block:: bash

   # Find uncovered lines
   lcov --list coverage.info | grep "0.0%"

**Add Missing Tests:**

.. code-block:: cpp

   // Cover error paths
   TEST(GPIOTest, InvalidPin) {
       nx_gpio_t* gpio = nx_factory_gpio('A', 99);  // Invalid pin
       EXPECT_EQ(gpio, nullptr);
   }

   // Cover edge cases
   TEST(GPIOTest, BoundaryValues) {
       // Test minimum pin
       nx_gpio_t* gpio0 = nx_factory_gpio('A', 0);
       EXPECT_NE(gpio0, nullptr);

       // Test maximum pin
       nx_gpio_t* gpio15 = nx_factory_gpio('A', 15);
       EXPECT_NE(gpio15, nullptr);
   }

Hardware-in-the-Loop Testing
-----------------------------

HIL Test Setup
~~~~~~~~~~~~~~

**Test Rig Components:**

* Target hardware (STM32F4 Discovery)
* Debug probe (ST-Link, J-Link)
* Test fixtures (buttons, LEDs, sensors)
* Host computer running tests

**Test Architecture:**

.. code-block:: text

   ┌──────────────┐
   │ Host PC      │
   │ (Test Runner)│
   └──────┬───────┘
          │ USB
   ┌──────┴───────┐
   │ Debug Probe  │
   │ (ST-Link)    │
   └──────┬───────┘
          │ SWD
   ┌──────┴───────┐
   │ Target MCU   │
   │ (STM32F4)    │
   └──────────────┘

Automated HIL Tests
~~~~~~~~~~~~~~~~~~~

**Python Test Script:**

.. code-block:: python

   import serial
   import time
   import subprocess

   class HILTest:
       def __init__(self, serial_port, elf_file):
           self.serial = serial.Serial(serial_port, 115200, timeout=1)
           self.elf_file = elf_file

       def flash_firmware(self):
           """Flash firmware to target"""
           cmd = [
               'openocd',
               '-f', 'interface/stlink.cfg',
               '-f', 'target/stm32f4x.cfg',
               '-c', f'program {self.elf_file} verify reset exit'
           ]
           subprocess.run(cmd, check=True)
           time.sleep(1)  # Wait for reset

       def send_command(self, cmd):
           """Send command via UART"""
           self.serial.write(f"{cmd}\r\n".encode())
           time.sleep(0.1)

       def read_response(self):
           """Read response from UART"""
           return self.serial.read(self.serial.in_waiting).decode()

       def test_gpio_toggle(self):
           """Test GPIO toggle command"""
           self.send_command("led green on")
           response = self.read_response()
           assert "LED green ON" in response

           self.send_command("led green off")
           response = self.read_response()
           assert "LED green OFF" in response

   # Run tests
   if __name__ == '__main__':
       test = HILTest('/dev/ttyUSB0', 'build/app.elf')
       test.flash_firmware()
       test.test_gpio_toggle()
       print("HIL tests passed!")

Continuous Integration
----------------------

GitHub Actions
~~~~~~~~~~~~~~

**Workflow Configuration:**

.. code-block:: yaml

   name: Tests

   on: [push, pull_request]

   jobs:
     test:
       runs-on: ubuntu-latest

       steps:
       - uses: actions/checkout@v3
         with:
           submodules: recursive

       - name: Install Dependencies
         run: |
           sudo apt-get update
           sudo apt-get install -y cmake ninja-build lcov

       - name: Configure
         run: |
           cmake -B build -G Ninja \
             -DNEXUS_PLATFORM=native \
             -DNEXUS_BUILD_TESTS=ON \
             -DNEXUS_ENABLE_COVERAGE=ON \
             -DCMAKE_BUILD_TYPE=Debug

       - name: Build
         run: cmake --build build

       - name: Test
         run: |
           cd build
           ctest --output-on-failure

       - name: Coverage
         run: |
           cd build
           lcov --capture --directory . --output-file coverage.info
           lcov --remove coverage.info '/usr/*' --output-file coverage.info
           lcov --list coverage.info

       - name: Upload Coverage
         uses: codecov/codecov-action@v3
         with:
           files: ./build/coverage.info

Test Organization
-----------------

Test Naming
~~~~~~~~~~~

**Conventions:**

* Test file: ``test_<module>.cpp``
* Test suite: ``<Module>Test``
* Test case: ``<Action><Expected>``

**Examples:**

.. code-block:: cpp

   // File: test_gpio.cpp
   class GPIOTest : public ::testing::Test { };

   TEST_F(GPIOTest, WriteHighReadsHigh) { }
   TEST_F(GPIOTest, WriteLowReadsLow) { }
   TEST_F(GPIOTest, ToggleChangesState) { }
   TEST_F(GPIOTest, InvalidPinReturnsNull) { }

Test Documentation
~~~~~~~~~~~~~~~~~~

**Document Test Intent:**

.. code-block:: cpp

   /**
    * \brief           Test GPIO write and read operations
    * \details         Verifies that writing a value to a GPIO pin
    *                  and then reading it back returns the same value.
    *                  This tests the basic GPIO functionality.
    */
   TEST_F(GPIOTest, WriteReadConsistency) {
       // Test implementation
   }

Best Practices
--------------

1. **Write Tests First (TDD)**
   * Define expected behavior
   * Write failing test
   * Implement feature
   * Verify test passes

2. **Test One Thing**
   * Each test should verify one behavior
   * Keep tests focused and simple
   * Use descriptive names

3. **Arrange-Act-Assert Pattern**
   * Arrange: Set up test conditions
   * Act: Execute code under test
   * Assert: Verify expected results

4. **Independent Tests**
   * Tests should not depend on each other
   * Use fixtures for shared setup
   * Clean up resources in teardown

5. **Fast Tests**
   * Unit tests should run in milliseconds
   * Use mocks to avoid slow operations
   * Parallelize test execution

6. **Readable Tests**
   * Use clear variable names
   * Add comments for complex logic
   * Keep tests simple

7. **Maintainable Tests**
   * Refactor test code like production code
   * Extract common test utilities
   * Keep tests up to date

8. **Comprehensive Coverage**
   * Test happy paths
   * Test error paths
   * Test edge cases
   * Test boundary conditions

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Tests Fail Intermittently**

* Check for race conditions
* Verify timing assumptions
* Use proper synchronization
* Increase timeouts if needed

**Tests Pass Locally, Fail in CI**

* Check environment differences
* Verify dependencies
* Check file paths
* Review CI logs

**Low Code Coverage**

* Identify uncovered code
* Add missing tests
* Test error paths
* Test edge cases

**Slow Test Execution**

* Profile test execution
* Use mocks for slow operations
* Parallelize tests
* Optimize test setup

See Also
--------

* :doc:`debugging` - Debugging Guide
* :doc:`../development/testing` - Development Testing Guide
* :doc:`../development/coverage_analysis` - Coverage Analysis
* :doc:`../development/ci_cd_integration` - CI/CD Integration

