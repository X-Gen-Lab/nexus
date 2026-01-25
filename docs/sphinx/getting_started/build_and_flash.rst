Build and Flash
===============

This guide covers building Nexus for different platforms and flashing to target hardware.

.. contents:: Table of Contents
   :local:
   :depth: 2

Build System Overview
---------------------

Nexus uses CMake as its build system with Python scripts for convenience. The build process:

1. **Configuration**: Kconfig generates ``nexus_config.h``
2. **CMake Configuration**: Selects platform, toolchain, and options
3. **Compilation**: Builds libraries and applications
4. **Linking**: Creates executables or firmware images
5. **Post-processing**: Generates ``.bin``, ``.hex`` files for embedded targets

Build Methods
-------------

There are three ways to build Nexus:

1. **Python Scripts** (Recommended) - Cross-platform, easy to use
2. **CMake Directly** - More control, standard CMake workflow
3. **IDE Integration** - Visual Studio, VS Code, CLion

Using Python Build Scripts
---------------------------

The Python build script provides a unified interface across platforms.

Basic Usage
~~~~~~~~~~~

.. code-block:: bash

   # Build for native platform (default)
   python scripts/building/build.py

   # Build for specific platform
   python scripts/building/build.py --platform stm32f4

   # Build with specific toolchain
   python scripts/building/build.py --platform stm32f4 --toolchain arm-none-eabi

Build Options
~~~~~~~~~~~~~

.. code-block:: bash

   # Build type
   python scripts/building/build.py -t debug      # Debug build
   python scripts/building/build.py -t release    # Release build

   # Clean build
   python scripts/building/build.py -c            # Clean before build

   # Parallel build
   python scripts/building/build.py -j 8          # Use 8 cores

   # Verbose output
   python scripts/building/build.py -v            # Show all commands

   # Specific application
   python scripts/building/build.py --app blinky  # Build only blinky

Combined Options
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Release build for STM32F4 with 8 parallel jobs
   python scripts/building/build.py --platform stm32f4 -t release -j 8

   # Clean debug build for native with verbose output
   python scripts/building/build.py -c -t debug -v

Using CMake Directly
--------------------

For more control, use CMake directly.

Native Platform
~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build \
       -DCMAKE_BUILD_TYPE=Release \
       -DNEXUS_PLATFORM=native \
       -DNEXUS_BUILD_TESTS=ON \
       -DNEXUS_BUILD_EXAMPLES=ON

   # Build
   cmake --build build --config Release

   # Or with parallel jobs
   cmake --build build --config Release -j 8

STM32F4 Platform
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build-stm32f4 \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4 \
       -DNEXUS_BUILD_TESTS=OFF \
       -DNEXUS_BUILD_EXAMPLES=ON

   # Build
   cmake --build build-stm32f4 --config Release

STM32H7 Platform
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build-stm32h7 \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32h7 \
       -DNEXUS_BUILD_TESTS=OFF

   # Build
   cmake --build build-stm32h7 --config Release

GD32 Platform
~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build-gd32 \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=gd32

   # Build
   cmake --build build-gd32 --config Release

Build Configuration
-------------------

CMake Options
~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Option
     - Type
     - Default
     - Description
   * - ``NEXUS_PLATFORM``
     - STRING
     - native
     - Target platform
   * - ``NEXUS_OSAL_BACKEND``
     - STRING
     - baremetal
     - OSAL backend
   * - ``NEXUS_BUILD_TESTS``
     - BOOL
     - ON
     - Build unit tests
   * - ``NEXUS_BUILD_EXAMPLES``
     - BOOL
     - ON
     - Build examples
   * - ``NEXUS_ENABLE_COVERAGE``
     - BOOL
     - OFF
     - Enable coverage
   * - ``CMAKE_BUILD_TYPE``
     - STRING
     - Debug
     - Build type

Platform Options
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Platform
     - Description
   * - ``native``
     - Native platform for host testing
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

Build Type Options
~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Build Type
     - Description
   * - ``Debug``
     - Debug build with symbols, no optimization
   * - ``Release``
     - Release build with optimization
   * - ``RelWithDebInfo``
     - Release with debug info
   * - ``MinSizeRel``
     - Minimum size release

Kconfig Configuration
---------------------

Using Defconfig
~~~~~~~~~~~~~~~

Each platform has a default configuration:

.. code-block:: bash

   # Copy platform defconfig
   cp platforms/native/defconfig .config

   # Or for STM32F4
   cp platforms/stm32/defconfig_stm32f4 .config

   # Build with this configuration
   cmake -B build
   cmake --build build

Manual Configuration
~~~~~~~~~~~~~~~~~~~~

Edit ``.config`` file directly:

.. code-block:: kconfig

   # Platform selection
   CONFIG_PLATFORM_STM32F4=y

   # Enable GPIO
   CONFIG_HAL_GPIO=y
   CONFIG_HAL_GPIO_A_5=y
   CONFIG_HAL_GPIO_A_5_MODE=OUTPUT_PP

   # Enable UART
   CONFIG_HAL_UART=y
   CONFIG_HAL_UART_1=y
   CONFIG_HAL_UART_1_BAUDRATE=115200

Using menuconfig (Linux/macOS)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Install menuconfig dependencies
   pip install kconfiglib

   # Run menuconfig
   python scripts/kconfig/generate_config.py --menuconfig

   # Build with new configuration
   cmake -B build
   cmake --build build

See :doc:`configuration` for detailed Kconfig usage.

Build Output
------------

Native Platform
~~~~~~~~~~~~~~~

.. code-block:: text

   build/
   ├── applications/
   │   ├── blinky/
   │   │   └── blinky.exe          # Windows
   │   │   └── blinky              # Linux/macOS
   │   ├── shell_demo/
   │   │   └── shell_demo.exe
   │   └── config_demo/
   │       └── config_demo.exe
   └── tests/
       └── nexus_tests.exe

Embedded Platforms
~~~~~~~~~~~~~~~~~~

.. code-block:: text

   build-stm32f4/
   ├── applications/
   │   ├── blinky/
   │   │   ├── blinky.elf          # ELF with debug symbols
   │   │   ├── blinky.bin          # Raw binary
   │   │   ├── blinky.hex          # Intel HEX format
   │   │   └── blinky.map          # Memory map
   │   ├── shell_demo/
   │   │   ├── shell_demo.elf
   │   │   ├── shell_demo.bin
   │   │   └── shell_demo.hex
   │   └── config_demo/
   │       ├── config_demo.elf
   │       ├── config_demo.bin
   │       └── config_demo.hex
   └── hal/
       └── libnexus_hal.a

File Formats
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Format
     - Description
   * - ``.elf``
     - Executable with debug symbols (for debugging)
   * - ``.bin``
     - Raw binary (for flashing)
   * - ``.hex``
     - Intel HEX format (for some programmers)
   * - ``.map``
     - Memory map showing symbol addresses and sizes

Flashing to Hardware
--------------------

STM32F4 Discovery
~~~~~~~~~~~~~~~~~

Using OpenOCD
^^^^^^^^^^^^^

.. code-block:: bash

   # Flash ELF file
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

   # Flash BIN file
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.bin 0x08000000 verify reset exit"

Using st-flash
^^^^^^^^^^^^^^

.. code-block:: bash

   # Flash BIN file
   st-flash write build-stm32f4/applications/blinky/blinky.bin 0x8000000

   # Erase flash
   st-flash erase

Using STM32CubeProgrammer
^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Open STM32CubeProgrammer
2. Connect to ST-Link
3. Select ``.hex`` or ``.bin`` file
4. Set start address (0x08000000 for STM32F4)
5. Click "Download"

STM32H7 Nucleo
~~~~~~~~~~~~~~

Using OpenOCD
^^^^^^^^^^^^^

.. code-block:: bash

   # Flash ELF file
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
       -c "program build-stm32h7/applications/blinky/blinky.elf verify reset exit"

Using st-flash
^^^^^^^^^^^^^^

.. code-block:: bash

   # Flash BIN file
   st-flash --connect-under-reset write build-stm32h7/applications/blinky/blinky.bin 0x8000000

GD32 Boards
~~~~~~~~~~~

Using OpenOCD
^^^^^^^^^^^^^

.. code-block:: bash

   # Flash ELF file
   openocd -f interface/stlink.cfg -f target/gd32vf103.cfg \
       -c "program build-gd32/applications/blinky/blinky.elf verify reset exit"

Using J-Link
^^^^^^^^^^^^

.. code-block:: bash

   # Create J-Link script
   cat > flash.jlink << EOF
   connect
   device GD32VF103CBT6
   speed 4000
   loadfile build-gd32/applications/blinky/blinky.bin 0x08000000
   r
   g
   exit
   EOF

   # Flash
   JLinkExe -CommanderScript flash.jlink

Debugging
---------

Native Platform
~~~~~~~~~~~~~~~

Using GDB
^^^^^^^^^

.. code-block:: bash

   # Start GDB
   gdb ./build/applications/blinky/blinky

   # GDB commands
   (gdb) break main
   (gdb) run
   (gdb) next
   (gdb) print variable
   (gdb) continue

Using LLDB
^^^^^^^^^^

.. code-block:: bash

   # Start LLDB
   lldb ./build/applications/blinky/blinky

   # LLDB commands
   (lldb) breakpoint set --name main
   (lldb) run
   (lldb) next
   (lldb) print variable
   (lldb) continue

STM32F4 Platform
~~~~~~~~~~~~~~~~

Using OpenOCD + GDB
^^^^^^^^^^^^^^^^^^^

Terminal 1 (OpenOCD):

.. code-block:: bash

   # Start OpenOCD
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

Terminal 2 (GDB):

.. code-block:: bash

   # Start GDB
   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf

   # Connect to OpenOCD
   (gdb) target remote localhost:3333

   # Reset and halt
   (gdb) monitor reset halt

   # Load program
   (gdb) load

   # Set breakpoint
   (gdb) break main

   # Continue
   (gdb) continue

Using VS Code
^^^^^^^^^^^^^

Install Cortex-Debug extension and use this ``launch.json``:

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug STM32F4",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "openocd",
               "cwd": "${workspaceRoot}",
               "executable": "${workspaceRoot}/build-stm32f4/applications/blinky/blinky.elf",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32f4x.cfg"
               ],
               "svdFile": "${workspaceRoot}/vendors/st/cmsis/stm32f4xx.svd"
           }
       ]
   }

Press F5 to start debugging.

Using J-Link
^^^^^^^^^^^^

.. code-block:: bash

   # Start J-Link GDB server
   JLinkGDBServer -device STM32F407VG -if SWD -speed 4000

   # In another terminal, start GDB
   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf
   (gdb) target remote localhost:2331
   (gdb) monitor reset
   (gdb) load
   (gdb) break main
   (gdb) continue

Serial Console
--------------

For applications using UART (shell_demo, config_demo):

Windows
~~~~~~~

Using PuTTY:

1. Download PuTTY from https://www.putty.org/
2. Select "Serial"
3. Set COM port (check Device Manager)
4. Set baud rate: 115200
5. Click "Open"

Using TeraTerm:

1. Download TeraTerm from https://ttssh2.osdn.jp/
2. File → New Connection → Serial
3. Select COM port
4. Setup → Serial Port: 115200, 8N1
5. Click "OK"

Linux
~~~~~

Using screen:

.. code-block:: bash

   # Find device
   ls /dev/ttyUSB* /dev/ttyACM*

   # Connect
   screen /dev/ttyUSB0 115200

   # Exit: Ctrl+A, then K

Using minicom:

.. code-block:: bash

   # Configure
   sudo minicom -s

   # Connect
   sudo minicom -D /dev/ttyUSB0 -b 115200

   # Exit: Ctrl+A, then X

Using picocom:

.. code-block:: bash

   # Connect
   picocom -b 115200 /dev/ttyUSB0

   # Exit: Ctrl+A, then Ctrl+X

macOS
~~~~~

Using screen:

.. code-block:: bash

   # Find device
   ls /dev/tty.*

   # Connect
   screen /dev/tty.usbserial-* 115200

   # Exit: Ctrl+A, then K

Troubleshooting
---------------

Build Issues
~~~~~~~~~~~~

**CMake configuration fails**

.. code-block:: text

   CMake Error: Could not find toolchain file

Solution: Ensure ARM toolchain is installed and in PATH.

**Compilation errors**

.. code-block:: text

   error: 'CONFIG_HAL_GPIO' undeclared

Solution: Regenerate configuration:

.. code-block:: bash

   python scripts/kconfig/generate_config.py --config .config --output nexus_config.h

**Linker errors**

.. code-block:: text

   undefined reference to 'hal_gpio_init'

Solution: Ensure all required libraries are linked in CMakeLists.txt.

Flash Issues
~~~~~~~~~~~~

**Cannot connect to ST-Link**

.. code-block:: text

   Error: libusb_open() failed with LIBUSB_ERROR_ACCESS

Solution (Linux): Add udev rules and user to dialout group:

.. code-block:: bash

   sudo usermod -a -G dialout $USER
   # Log out and log back in

Solution (Windows): Install ST-Link driver.

**Flash verification failed**

Solution: Erase flash before programming:

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "init" -c "reset halt" -c "flash erase_sector 0 0 last" -c "exit"

**Wrong start address**

Ensure correct flash start address:

* STM32F4: 0x08000000
* STM32H7: 0x08000000
* GD32: 0x08000000

Debug Issues
~~~~~~~~~~~~

**GDB cannot connect**

Solution: Ensure OpenOCD is running and listening on port 3333.

**Breakpoints not working**

Solution: Build with debug symbols:

.. code-block:: bash

   cmake -B build -DCMAKE_BUILD_TYPE=Debug

**No serial output**

Solution: Check UART configuration and connections:

* Baud rate: 115200
* Data bits: 8
* Stop bits: 1
* Parity: None
* Flow control: None

Next Steps
----------

Now that you can build and flash:

1. :doc:`first_application` - Create your own application
2. :doc:`configuration` - Customize your build with Kconfig
3. :doc:`examples_tour` - Explore more examples
4. :doc:`../user_guide/ide_integration` - Set up your IDE

See Also
--------

* :doc:`../user_guide/build_system` - Detailed build system documentation
* :doc:`../platform_guides/index` - Platform-specific guides
* :doc:`../development/scripts` - Build scripts documentation
* :doc:`faq` - Frequently asked questions
