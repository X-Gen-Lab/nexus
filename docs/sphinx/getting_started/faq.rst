Frequently Asked Questions
==========================

Common questions and answers about Nexus.

.. contents:: Table of Contents
   :local:
   :depth: 2

General Questions
-----------------

What is Nexus?
~~~~~~~~~~~~~~

Nexus is a professional embedded software development platform designed for building reliable, secure, and portable embedded applications across multiple MCU platforms.

**Key Features**:

* Hardware Abstraction Layer (HAL)
* OS Abstraction Layer (OSAL)
* Framework components (Log, Shell, Config)
* Multi-platform support (STM32, GD32, Native)
* Comprehensive testing (1539+ tests)
* Complete documentation (English and Chinese)

Who should use Nexus?
~~~~~~~~~~~~~~~~~~~~~

Nexus is ideal for:

* **Embedded developers** who want portable code across platforms
* **Teams** who need consistent coding standards
* **Students** learning embedded systems
* **Hobbyists** building embedded projects
* **Companies** developing commercial products

What platforms does Nexus support?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Currently Supported**:

* Native (Windows/Linux/macOS) - for testing
* STM32F4 series (Cortex-M4)

**In Progress**:

* STM32H7 series (Cortex-M7)
* GD32 series

**Planned**:

* ESP32 series
* nRF52 series

Is Nexus free?
~~~~~~~~~~~~~~

Yes, Nexus is open source under the MIT License. You can use it freely in commercial and non-commercial projects.

Installation and Setup
----------------------

What tools do I need?
~~~~~~~~~~~~~~~~~~~~~

**Required**:

* CMake 3.16+
* Git
* Python 3.8+
* C compiler (GCC, Clang, or MSVC)

**For ARM targets**:

* ARM GCC Toolchain 10.3+
* OpenOCD or J-Link (for debugging)

**Optional**:

* Doxygen (for documentation)
* Sphinx (for user guides)

See :doc:`environment_setup` for detailed installation instructions.

How do I install the ARM toolchain?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Windows**:

.. code-block:: powershell

   choco install gcc-arm-embedded

Or download from https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

**Linux**:

.. code-block:: bash

   sudo apt install gcc-arm-none-eabi

**macOS**:

.. code-block:: bash

   brew install --cask gcc-arm-embedded

Why can't CMake find the ARM toolchain?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Solution**: Ensure the toolchain is in your PATH:

.. code-block:: bash

   # Verify installation
   arm-none-eabi-gcc --version

   # If not found, add to PATH
   export PATH=$PATH:/path/to/gcc-arm-none-eabi/bin

Building and Compilation
-------------------------

How do I build for native platform?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Using Python script
   python scripts/building/build.py

   # Or using CMake
   cmake -B build -DNEXUS_PLATFORM=native
   cmake --build build

How do I build for STM32F4?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Using Python script
   python scripts/building/build.py --platform stm32f4 --toolchain arm-none-eabi

   # Or using CMake
   cmake -B build-stm32f4 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4
   cmake --build build-stm32f4

Build fails with "nexus_config.h not found"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Cause**: Configuration header not generated.

**Solution**: Generate configuration:

.. code-block:: bash

   python scripts/kconfig/generate_config.py --default --output nexus_config.h

Or copy a defconfig:

.. code-block:: bash

   cp platforms/native/defconfig .config
   python scripts/kconfig/generate_config.py --config .config --output nexus_config.h

Build fails with "undefined reference"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Cause**: Missing library or incorrect link order.

**Solution**: Check CMakeLists.txt and ensure all required libraries are linked:

.. code-block:: cmake

   target_link_libraries(my_app PRIVATE
       nexus_hal
       nexus_osal
       nexus_platform
   )

How do I enable code coverage?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   cmake -B build \
       -DNEXUS_PLATFORM=native \
       -DNEXUS_ENABLE_COVERAGE=ON \
       -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build

   # Generate report (Linux/macOS)
   bash scripts/coverage/run_coverage_linux.sh

Flashing and Debugging
-----------------------

How do I flash to STM32F4?
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Using OpenOCD**:

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

**Using st-flash**:

.. code-block:: bash

   st-flash write build-stm32f4/applications/blinky/blinky.bin 0x8000000

Cannot connect to ST-Link
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Windows**: Install ST-Link driver from https://www.st.com/en/development-tools/stsw-link009.html

**Linux**: Add udev rules and user to dialout group:

.. code-block:: bash

   sudo usermod -a -G dialout $USER
   # Log out and log back in

**macOS**: Should work out of the box. If not, try:

.. code-block:: bash

   brew install libusb

Flash verification failed
~~~~~~~~~~~~~~~~~~~~~~~~~

**Solution**: Erase flash before programming:

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "init" -c "reset halt" -c "flash erase_sector 0 0 last" -c "exit"

How do I debug with GDB?
~~~~~~~~~~~~~~~~~~~~~~~~~

**Terminal 1** (OpenOCD):

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

**Terminal 2** (GDB):

.. code-block:: bash

   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf
   (gdb) target remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) break main
   (gdb) continue

Configuration
-------------

How do I configure peripherals?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use Kconfig configuration:

1. Copy platform defconfig:

.. code-block:: bash

   cp platforms/stm32/defconfig_stm32f4 .config

2. Edit ``.config`` to enable peripherals:

.. code-block:: kconfig

   CONFIG_HAL_GPIO=y
   CONFIG_HAL_UART=y
   CONFIG_HAL_SPI=y

3. Generate header and build:

.. code-block:: bash

   python scripts/kconfig/generate_config.py --config .config --output nexus_config.h
   cmake --build build

How do I use menuconfig?
~~~~~~~~~~~~~~~~~~~~~~~~~

**Linux/macOS only**:

.. code-block:: bash

   pip install kconfiglib
   python scripts/kconfig/generate_config.py --menuconfig

Navigate with arrow keys, press Enter to select, Space to toggle, Q to quit and save.

How do I change UART baud rate?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Edit ``.config``:

.. code-block:: kconfig

   CONFIG_HAL_UART_1_BAUDRATE=115200

Or in code:

.. code-block:: c

   hal_uart_config_t config = {
       .baudrate = 115200,
       /* ... */
   };
   hal_uart_init(HAL_UART_1, &config);

Development
-----------

How do I create a new application?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Create directory:

.. code-block:: bash

   mkdir applications/my_app
   cd applications/my_app

2. Create ``CMakeLists.txt``:

.. code-block:: cmake

   add_executable(my_app main.c)
   target_link_libraries(my_app PRIVATE nexus_hal nexus_platform)

3. Create ``main.c``:

.. code-block:: c

   #include "hal/hal.h"

   int main(void) {
       hal_system_init();
       /* Your code */
       return 0;
   }

4. Add to ``applications/CMakeLists.txt``:

.. code-block:: cmake

   add_subdirectory(my_app)

See :doc:`first_application` for detailed guide.

How do I add a new command to shell?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   static int cmd_handler(int argc, char* argv[]) {
       shell_printf("My command executed\n");
       return 0;
   }

   static const shell_command_t cmd_def = {
       .name = "mycommand",
       .handler = cmd_handler,
       .help = "My custom command",
       .usage = "mycommand [args]",
       .completion = NULL
   };

   /* Register command */
   shell_register_command(&cmd_def);

How do I use logging?
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "log/log.h"

   /* Initialize logging */
   log_init(NULL);

   /* Log messages */
   LOG_ERROR("Error: %d", error_code);
   LOG_WARN("Warning: %s", warning_msg);
   LOG_INFO("Info: %d", value);
   LOG_DEBUG("Debug: %s", debug_info);

How do I use FreeRTOS?
~~~~~~~~~~~~~~~~~~~~~~~

Build with FreeRTOS backend:

.. code-block:: bash

   cmake -B build-freertos \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4 \
       -DNEXUS_OSAL_BACKEND=freertos
   cmake --build build-freertos

Use OSAL APIs:

.. code-block:: c

   #include "osal/osal.h"

   void my_task(void* arg) {
       while (1) {
           /* Task work */
           osal_task_delay(1000);
       }
   }

   int main(void) {
       osal_init();
       osal_task_create(my_task, "my_task", 512, NULL, 1, NULL);
       osal_start_scheduler();
       return 0;
   }

Testing
-------

How do I run tests?
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Using Python script
   python scripts/test/test.py

   # Or using CTest
   cd build
   ctest --output-on-failure

How do I run specific tests?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Filter by name
   python scripts/test/test.py -f "GPIO*"

   # Or with CTest
   ctest -R "GPIO*"

How do I add unit tests?
~~~~~~~~~~~~~~~~~~~~~~~~~

Create test file in ``tests/``:

.. code-block:: cpp

   #include <gtest/gtest.h>
   #include "hal/gpio.h"

   TEST(GPIO, Init) {
       hal_gpio_config_t config = {/* ... */};
       EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 5, &config));
   }

Add to ``tests/CMakeLists.txt``:

.. code-block:: cmake

   add_executable(my_tests test_my_module.cpp)
   target_link_libraries(my_tests PRIVATE gtest_main nexus_hal)
   add_test(NAME MyTests COMMAND my_tests)

Troubleshooting
---------------

Program doesn't run on hardware
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Check**:

1. Flash successful? Verify with ``st-info --probe``
2. Correct start address? STM32F4 uses 0x08000000
3. Power supply adequate? Check voltage and current
4. Reset after flashing? Press reset button or power cycle

No serial output
~~~~~~~~~~~~~~~~

**Check**:

1. Correct baud rate? Default is 115200
2. Correct pins? STM32F4 UART2 uses PA2 (TX), PA3 (RX)
3. TX/RX swapped? Try swapping connections
4. Ground connected? Ensure common ground

LEDs don't blink
~~~~~~~~~~~~~~~~

**Check**:

1. Correct pins? STM32F4 Discovery uses PD12-PD15
2. GPIO initialized? Check initialization code
3. Power supply? Ensure adequate power
4. Code running? Check with debugger

Build is slow
~~~~~~~~~~~~~

**Solutions**:

1. Use parallel build:

.. code-block:: bash

   cmake --build build -j 8

2. Use ccache (Linux/macOS):

.. code-block:: bash

   sudo apt install ccache
   export CC="ccache gcc"
   export CXX="ccache g++"

3. Disable tests if not needed:

.. code-block:: bash

   cmake -B build -DNEXUS_BUILD_TESTS=OFF

Out of memory on embedded target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Solutions**:

1. Reduce heap size in Kconfig
2. Use static allocation instead of dynamic
3. Reduce buffer sizes
4. Disable unused features
5. Use MinSizeRel build type:

.. code-block:: bash

   cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel

Getting Help
------------

Where can I get help?
~~~~~~~~~~~~~~~~~~~~~

* **Documentation**: https://nexus-platform.github.io/nexus/
* **GitHub Issues**: https://github.com/nexus-platform/nexus/issues
* **GitHub Discussions**: https://github.com/nexus-platform/nexus/discussions
* **Email**: support@nexus-platform.org (if available)

How do I report a bug?
~~~~~~~~~~~~~~~~~~~~~~

1. Check if issue already exists
2. Create new issue on GitHub
3. Include:
   * Nexus version
   * Platform (STM32F4, native, etc.)
   * Build configuration
   * Steps to reproduce
   * Expected vs actual behavior
   * Error messages and logs

How do I request a feature?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Check if feature already requested
2. Create feature request on GitHub Discussions
3. Describe:
   * Use case
   * Proposed solution
   * Alternatives considered
   * Additional context

How do I contribute?
~~~~~~~~~~~~~~~~~~~~

See :doc:`../development/contributing` for contribution guidelines.

**Quick steps**:

1. Fork repository
2. Create feature branch
3. Make changes
4. Add tests
5. Update documentation
6. Submit pull request

Contributing
------------

Can I contribute examples?
~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes! We welcome example contributions. See :doc:`../development/contributing` for guidelines.

Can I port Nexus to a new platform?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes! See :doc:`../user_guide/porting` for porting guide.

Can I add support for a new RTOS?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes! See ``osal/docs/PORTING_GUIDE.md`` for OSAL porting guide.

License
-------

What license does Nexus use?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Nexus is licensed under the MIT License. You can use it freely in commercial and non-commercial projects.

Can I use Nexus in commercial products?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes, the MIT License allows commercial use without restrictions.

Do I need to open source my application?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

No, the MIT License does not require you to open source your application code.

Next Steps
----------

* :doc:`quick_start` - Get started quickly
* :doc:`../tutorials/index` - Step-by-step tutorials
* :doc:`../user_guide/index` - Detailed documentation
* :doc:`../development/contributing` - Contribute to Nexus
