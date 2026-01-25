Native Platform Guide
=====================

Overview
--------

The Native platform provides a simulated hardware environment for development and testing on PC (Windows, Linux, macOS). It enables developers to build and test embedded applications without physical hardware, making it ideal for rapid prototyping, automated testing, and CI/CD integration.

**Key Features:**

- Cross-platform support (Windows, Linux, macOS)
- Simulated peripheral interfaces
- Data injection for testing
- No hardware dependencies
- Fast build and test cycles
- CI/CD friendly

Platform Capabilities
---------------------

Supported Peripherals
~~~~~~~~~~~~~~~~~~~~~

The Native platform supports the following peripherals:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Peripheral
     - Max Instances
     - Capabilities
   * - UART
     - 4
     - Async/Sync TX/RX, configurable baud rate, data injection
   * - GPIO
     - 32 pins
     - Input/Output, interrupt simulation, state queries
   * - SPI
     - 2
     - Master/Slave modes, data injection, state verification
   * - I2C
     - 2
     - Master/Slave modes, data injection, state verification
   * - Timer
     - 4
     - One-shot/Periodic modes, callback support
   * - ADC
     - Optional
     - Simulated analog input
   * - DAC
     - Optional
     - Simulated analog output
   * - CRC
     - 1
     - CRC32 calculation
   * - Flash
     - 1
     - Simulated non-volatile storage
   * - RTC
     - 1
     - Real-time clock simulation
   * - Watchdog
     - 1
     - Watchdog timer simulation

Platform Limitations
~~~~~~~~~~~~~~~~~~~~

The Native platform has the following limitations:

- **No Real Hardware**: All peripherals are simulated
- **Timing Accuracy**: Timing may not match real hardware exactly
- **No DMA**: DMA operations are simulated but not hardware-accelerated
- **No Interrupts**: Interrupts are simulated through callbacks
- **Performance**: Performance characteristics differ from real hardware

Build Instructions
------------------

Prerequisites
~~~~~~~~~~~~~

- CMake 3.15 or higher
- C compiler (GCC, Clang, MSVC)
- Python 3.7+ (for build scripts)

Basic Build
~~~~~~~~~~~

.. code-block:: bash

   # Configure for Native platform
   CMake -B build -DPLATFORM=native

   # Build
   CMake --build build

   # Run tests
   cd build
   ctest

Using Build Scripts
~~~~~~~~~~~~~~~~~~~

The project provides Python build scripts for convenience:

.. code-block:: bash

   # Build for Native platform
   python scripts/nexus.py build --platform native

   # Build and run tests
   python scripts/nexus.py test --platform native

   # Clean build artifacts
   python scripts/nexus.py clean

Cross-Platform Build
~~~~~~~~~~~~~~~~~~~~

**Windows (PowerShell):**

.. code-block:: powershell

   # Using CMake directly
   CMake -B build -G "Visual Studio 17 2022" -DPLATFORM=native
   CMake --build build --config Release

   # Using build script
   python scripts/nexus.py build --platform native --config Release

**Linux/macOS:**

.. code-block:: bash

   # Using CMake directly
   CMake -B build -G "Unix Makefiles" -DPLATFORM=native
   CMake --build build

   # Using build script
   python scripts/nexus.py build --platform native

Kconfig Options
---------------

Platform Selection
~~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   CONFIG_PLATFORM_NATIVE=y
   CONFIG_PLATFORM_NAME="native"

Platform Settings
~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # Platform identification
   CONFIG_NATIVE_PLATFORM_NAME="Native Platform (PC Simulation)"
   CONFIG_NATIVE_PLATFORM_VERSION="1.0.0"

   # Logging
   CONFIG_NATIVE_ENABLE_LOGGING=y
   CONFIG_NATIVE_LOG_LEVEL=3  # 0=Error, 1=Warn, 2=Info, 3=Debug

   # Statistics
   CONFIG_NATIVE_ENABLE_STATISTICS=y

   # Memory alignment
   CONFIG_NATIVE_BUFFER_ALIGNMENT=4

Resource Managers
~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # DMA channels
   CONFIG_NATIVE_DMA_CHANNELS=8

   # Interrupt service routine slots
   CONFIG_NATIVE_ISR_SLOTS=64

Peripheral Configuration
~~~~~~~~~~~~~~~~~~~~~~~~

**UART Configuration:**

.. code-block:: Kconfig

   CONFIG_UART_ENABLE=y
   CONFIG_UART_MAX_INSTANCES=4
   CONFIG_INSTANCE_NATIVE_UART_0=y
   CONFIG_UART0_BAUDRATE=115200
   CONFIG_UART0_DATA_BITS=8
   CONFIG_UART0_STOP_BITS=1
   CONFIG_UART0_PARITY_VALUE=0  # 0=None, 1=Odd, 2=Even
   CONFIG_UART0_MODE_VALUE=0    # 0=Polling, 1=Interrupt, 2=DMA
   CONFIG_UART0_TX_BUFFER_SIZE=256
   CONFIG_UART0_RX_BUFFER_SIZE=256

**GPIO Configuration:**

.. code-block:: Kconfig

   CONFIG_GPIO_ENABLE=y
   CONFIG_GPIO_MAX_PINS=32

**SPI Configuration:**

.. code-block:: Kconfig

   CONFIG_SPI_ENABLE=y
   CONFIG_SPI_MAX_INSTANCES=2

**I2C Configuration:**

.. code-block:: Kconfig

   CONFIG_I2C_ENABLE=y
   CONFIG_I2C_MAX_INSTANCES=2

Hardware Setup
--------------

No Physical Hardware Required
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Native platform does not require any physical hardware. All peripherals are simulated in software.

Development Environment
~~~~~~~~~~~~~~~~~~~~~~~

**Recommended IDEs:**

- Visual Studio Code (with C/C++ extension)
- CLion
- Visual Studio 2022

**Required Tools:**

- CMake 3.15+
- C compiler (GCC 7+, Clang 10+, MSVC 2019+)
- Python 3.7+

Example Projects
----------------

Basic UART Example
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_uart.h"
   #include "hal/nx_factory.h"

   int main(void) {
       /* Get UART instance */
       nx_uart_t* uart = nx_factory_uart(0);
       if (!uart) {
           return -1;
       }

       /* Initialize UART */
       nx_lifecycle_t* lc = uart->get_lifecycle(uart);
       if (lc->init(lc) != NX_OK) {
           return -1;
       }

       /* Send data */
       nx_tx_async_t* tx = uart->get_tx_async(uart);
       const uint8_t data[] = "Hello, Native Platform!";
       size_t len = sizeof(data) - 1;
       tx->transmit(tx, data, len);

       /* Clean up */
       lc->deinit(lc);
       return 0;
   }

GPIO Control Example
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_gpio.h"
   #include "hal/nx_factory.h"

   int main(void) {
       /* Get GPIO instance */
       nx_gpio_t* gpio = nx_factory_gpio(0);
       if (!gpio) {
           return -1;
       }

       /* Initialize GPIO */
       nx_lifecycle_t* lc = gpio->get_lifecycle(gpio);
       if (lc->init(lc) != NX_OK) {
           return -1;
       }

       /* Configure pin as output */
       nx_gpio_config_t config = {
           .pin = 0,
           .mode = NX_GPIO_MODE_OUTPUT,
           .pull = NX_GPIO_PULL_NONE,
           .speed = NX_GPIO_SPEED_LOW
       };
       gpio->configure(gpio, &config);

       /* Toggle pin */
       gpio->write_pin(gpio, 0, 1);  /* Set high */
       gpio->write_pin(gpio, 0, 0);  /* Set low */

       /* Clean up */
       lc->deinit(lc);
       return 0;
   }

Testing with Data Injection
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_uart.h"
   #include "platforms/native/src/uart/nx_uart_test.h"

   void test_uart_receive(void) {
       /* Get UART instance */
       nx_uart_t* uart = nx_factory_uart(0);

       /* Initialize */
       nx_lifecycle_t* lc = uart->get_lifecycle(uart);
       lc->init(lc);

       /* Inject test data */
       const uint8_t test_data[] = "Test Data";
       nx_uart_test_inject_rx_data(0, test_data, sizeof(test_data));

       /* Read injected data */
       nx_rx_async_t* rx = uart->get_rx_async(uart);
       uint8_t buffer[32];
       size_t len = sizeof(buffer);
       rx->receive(rx, buffer, &len);

       /* Verify */
       assert(len == sizeof(test_data));
       assert(memcmp(buffer, test_data, len) == 0);

       /* Clean up */
       nx_uart_test_reset(0);
       lc->deinit(lc);
   }

Debugging Procedures
--------------------

Using GDB
~~~~~~~~~

.. code-block:: bash

   # Build with debug symbols
   CMake -B build -DPLATFORM=native -DCMAKE_BUILD_TYPE=Debug
   CMake --build build

   # Run with GDB
   gdb build/applications/blinky/blinky

Using Visual Studio Code
~~~~~~~~~~~~~~~~~~~~~~~~

Create ``.vscode/launch.json``:

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug Native Platform",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/build/applications/blinky/blinky",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}",
               "environment": [],
               "externalConsole": false,
               "MIMode": "gdb",
               "setupCommands": [
                   {
                       "description": "Enable pretty-printing for gdb",
                       "text": "-enable-pretty-printing",
                       "ignoreFailures": true
                   }
               ]
           }
       ]
   }

Logging and Diagnostics
~~~~~~~~~~~~~~~~~~~~~~~

Enable detailed logging:

.. code-block:: Kconfig

   CONFIG_NATIVE_ENABLE_LOGGING=y
   CONFIG_NATIVE_LOG_LEVEL=3  # Debug level

View diagnostic information:

.. code-block:: c

   #include "hal/nx_uart.h"

   nx_uart_t* uart = nx_factory_uart(0);
   nx_diagnostic_t* diag = uart->get_diagnostic(uart);

   /* Get statistics */
   nx_uart_stats_t stats;
   diag->get_stats(diag, &stats);

   /* Print statistics */
   printf("TX Count: %zu\n", stats.tx_count);
   printf("RX Count: %zu\n", stats.rx_count);
   printf("Errors: %zu\n", stats.error_count);

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Build Fails with "Platform not found":**

Solution: Ensure ``-DPLATFORM=native`` is specified in CMake configuration.

**Tests Fail with "Device not initialized":**

Solution: Ensure lifecycle ``init()`` is called before using peripherals.

**Data Injection Not Working:**

Solution: Verify test support functions are available and instance ID is correct.

Performance Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

The Native platform is optimized for testing, not performance:

- Use for functional testing, not performance benchmarking
- Timing may not match real hardware
- Consider using real hardware for performance-critical testing

CI/CD Integration
~~~~~~~~~~~~~~~~~

The Native platform is ideal for CI/CD:

.. code-block:: yaml

   # GitHub Actions example
   - name: Build and Test Native Platform
     run: |
       CMake -B build -DPLATFORM=native
       CMake --build build
       cd build && ctest --output-on-failure

See Also
--------

- :doc:`../user_guide/build_system` - Build system documentation
- :doc:`../user_guide/kconfig_platforms` - Platform configuration guide
- :doc:`../development/testing` - Testing guide
- :doc:`stm32f4` - STM32F4 platform guide
- :doc:`stm32h7` - STM32H7 platform guide
- :doc:`gd32` - GD32 platform guide
