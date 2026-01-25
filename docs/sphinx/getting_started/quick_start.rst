Quick Start
===========

Get up and running with Nexus in 5 minutes! This guide will help you build and run your first example.

.. contents:: Table of Contents
   :local:
   :depth: 2

Prerequisites
-------------

Before starting, ensure you have:

* Completed :doc:`environment_setup`
* Cloned the Nexus repository
* All required tools installed

If not, go back to :doc:`environment_setup` first.

Choose Your Platform
--------------------

Nexus supports multiple platforms. Choose one to get started:

**Native Platform (Recommended for First-Time Users)**

* Runs on your PC (Windows/Linux/macOS)
* No hardware required
* Fast iteration and debugging
* Perfect for learning and testing

**STM32F4 Discovery Board**

* Real hardware experience
* LED blinking and GPIO control
* UART communication
* Requires STM32F4DISCOVERY board

We'll cover both platforms in this guide.

5-Minute Quick Start (Native)
------------------------------

Step 1: Clone Repository
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Clone Nexus
   git clone https://github.com/nexus-platform/nexus.git
   cd nexus

   # Initialize submodules
   git submodule update --init --recursive

Step 2: Build
~~~~~~~~~~~~~

Using Python script (recommended):

.. code-block:: bash

   # Build for native platform
   python scripts/building/build.py

   # Or with specific options
   python scripts/building/build.py -t release -j 8

Using CMake directly:

.. code-block:: bash

   # Configure
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_PLATFORM=native

   # Build
   cmake --build build --config Release

Step 3: Run Example
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Run blinky example
   ./build/applications/blinky/blinky

   # Or on Windows
   .\build\applications\blinky\Release\blinky.exe

You should see output like:

.. code-block:: text

   [INFO] HAL initialized
   [INFO] GPIO initialized
   [INFO] LED blinking started
   LED ON
   LED OFF
   LED ON
   LED OFF
   ...

Step 4: Run Tests (Optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Run all tests
   python scripts/test/test.py

   # Or using CTest
   cd build
   ctest --output-on-failure

Congratulations! You've successfully built and run Nexus.

Quick Start for STM32F4
-----------------------

Step 1: Prepare Hardware
~~~~~~~~~~~~~~~~~~~~~~~~

* Connect STM32F4DISCOVERY board via USB
* Verify ST-Link connection:

.. code-block:: bash

   # Using OpenOCD
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

   # Or ST-Link utilities
   st-info --probe

Step 2: Build for STM32F4
~~~~~~~~~~~~~~~~~~~~~~~~~~

Using Python script:

.. code-block:: bash

   # Build for STM32F4
   python scripts/building/build.py --platform stm32f4 --toolchain arm-none-eabi

Using CMake:

.. code-block:: bash

   # Configure
   cmake -B build-stm32f4 \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4

   # Build
   cmake --build build-stm32f4 --config Release

Step 3: Flash to Board
~~~~~~~~~~~~~~~~~~~~~~

Using OpenOCD:

.. code-block:: bash

   # Flash blinky example
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

Using ST-Link utilities:

.. code-block:: bash

   # Flash using st-flash
   st-flash write build-stm32f4/applications/blinky/blinky.bin 0x8000000

Step 4: Observe Results
~~~~~~~~~~~~~~~~~~~~~~~

You should see the LEDs on the STM32F4DISCOVERY board blinking in sequence:

* Green LED (PD12)
* Orange LED (PD13)
* Red LED (PD14)
* Blue LED (PD15)

Each LED blinks for 500ms.

Exploring Examples
------------------

Nexus includes several example applications:

Blinky Example
~~~~~~~~~~~~~~

Simple LED blinking demonstration.

**Location**: ``applications/blinky/``

**Features**:

* GPIO initialization
* LED control
* Delay functions

**Run**:

.. code-block:: bash

   # Native
   ./build/applications/blinky/blinky

   # STM32F4 (flash to board)
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

Shell Demo
~~~~~~~~~~

Interactive command-line interface over UART.

**Location**: ``applications/shell_demo/``

**Features**:

* UART communication
* Command parsing
* LED control via commands
* Button status reading

**Run**:

.. code-block:: bash

   # Native
   ./build/applications/shell_demo/shell_demo

   # STM32F4 (requires serial terminal)
   # 1. Flash to board
   # 2. Connect serial terminal (115200 baud, PA2/PA3)
   # 3. Type 'help' to see available commands

**Available Commands**:

.. code-block:: text

   nexus> help
   Available commands:
     help     - Show this help message
     led      - Control LEDs (led <color> <on|off|toggle>)
     button   - Read button status
     tick     - Show system tick count
     delay    - Delay for specified milliseconds
     reboot   - Reboot the system

Config Demo
~~~~~~~~~~~

Configuration management system demonstration.

**Location**: ``applications/config_demo/``

**Features**:

* Configuration storage
* Namespace isolation
* JSON import/export
* Binary serialization

**Run**:

.. code-block:: bash

   # Native
   ./build/applications/config_demo/config_demo

   # STM32F4 (requires serial terminal)
   # Connect serial terminal to see output

FreeRTOS Demo
~~~~~~~~~~~~~

Multi-tasking with FreeRTOS.

**Location**: ``applications/freertos_demo/``

**Features**:

* Task creation
* Mutex synchronization
* Queue communication
* Software timers

**Build with FreeRTOS**:

.. code-block:: bash

   # Configure with FreeRTOS backend
   cmake -B build-freertos \
       -DNEXUS_PLATFORM=stm32f4 \
       -DNEXUS_OSAL_BACKEND=freertos \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake

   # Build
   cmake --build build-freertos

Build Options
-------------

Common Build Options
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Debug build
   python scripts/building/build.py -t debug

   # Release build
   python scripts/building/build.py -t release

   # Clean build
   python scripts/building/build.py -c

   # Parallel build (8 jobs)
   python scripts/building/build.py -j 8

   # Specific platform
   python scripts/building/build.py --platform stm32f4

Platform Options
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Platform
     - Description
   * - ``native``
     - Native platform (Windows/Linux/macOS)
   * - ``stm32f4``
     - STM32F4 series (Cortex-M4)
   * - ``stm32h7``
     - STM32H7 series (Cortex-M7)
   * - ``gd32``
     - GD32 series

OSAL Backend Options
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Backend
     - Description
   * - ``baremetal``
     - No RTOS (default)
   * - ``freertos``
     - FreeRTOS integration
   * - ``rtthread``
     - RT-Thread integration
   * - ``zephyr``
     - Zephyr RTOS integration

Example Configurations
~~~~~~~~~~~~~~~~~~~~~~

**Native with tests**:

.. code-block:: bash

   cmake -B build \
       -DNEXUS_PLATFORM=native \
       -DNEXUS_BUILD_TESTS=ON \
       -DCMAKE_BUILD_TYPE=Debug

**STM32F4 with FreeRTOS**:

.. code-block:: bash

   cmake -B build-stm32f4-rtos \
       -DNEXUS_PLATFORM=stm32f4 \
       -DNEXUS_OSAL_BACKEND=freertos \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DCMAKE_BUILD_TYPE=Release

**STM32H7 minimal size**:

.. code-block:: bash

   cmake -B build-stm32h7-min \
       -DNEXUS_PLATFORM=stm32h7 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DCMAKE_BUILD_TYPE=MinSizeRel

Debugging
---------

Native Platform
~~~~~~~~~~~~~~~

Use your favorite debugger:

**GDB**:

.. code-block:: bash

   gdb ./build/applications/blinky/blinky

**LLDB**:

.. code-block:: bash

   lldb ./build/applications/blinky/blinky

**Visual Studio**:

Open solution in Visual Studio and press F5.

**VS Code**:

Use the built-in debugger with C/C++ extension.

STM32F4 Platform
~~~~~~~~~~~~~~~~

**Using OpenOCD and GDB**:

Terminal 1 (OpenOCD):

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

Terminal 2 (GDB):

.. code-block:: bash

   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf
   (gdb) target remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) continue

**Using VS Code**:

Install Cortex-Debug extension and use provided launch configurations.

See :doc:`../user_guide/ide_integration` for detailed debugging setup.

Common Issues
-------------

Build Fails
~~~~~~~~~~~

**Issue**: CMake configuration fails

.. code-block:: text

   CMake Error: Could not find toolchain file

**Solution**: Ensure ARM toolchain is installed and in PATH:

.. code-block:: bash

   arm-none-eabi-gcc --version

**Issue**: Python module not found

.. code-block:: text

   ModuleNotFoundError: No module named 'kconfiglib'

**Solution**: Install Python dependencies:

.. code-block:: bash

   pip install -r requirements.txt

Flash Fails
~~~~~~~~~~~

**Issue**: Cannot connect to ST-Link

.. code-block:: text

   Error: libusb_open() failed with LIBUSB_ERROR_ACCESS

**Solution** (Linux): Add udev rules and user to dialout group:

.. code-block:: bash

   sudo usermod -a -G dialout $USER
   # Log out and log back in

**Solution** (Windows): Install ST-Link driver from ST website.

**Issue**: Flash verification failed

**Solution**: Erase flash before programming:

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "init" -c "reset halt" -c "flash erase_sector 0 0 last" -c "exit"

No Output
~~~~~~~~~

**Issue**: Program runs but no output

**Native**: Check if stdout is buffered. Add ``fflush(stdout)`` or run with ``-u`` flag.

**STM32F4**: Verify UART connection and baud rate (115200).

Next Steps
----------

Now that you've built and run your first example:

1. :doc:`project_structure` - Understand the codebase organization
2. :doc:`first_application` - Create your own application
3. :doc:`examples_tour` - Explore more complex examples
4. :doc:`../tutorials/index` - Follow step-by-step tutorials

See Also
--------

* :doc:`build_and_flash` - Detailed build and deployment guide
* :doc:`../user_guide/build_system` - Build system documentation
* :doc:`../platform_guides/index` - Platform-specific guides
* :doc:`faq` - Frequently asked questions
