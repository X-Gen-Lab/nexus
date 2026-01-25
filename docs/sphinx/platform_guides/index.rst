Platform Guides
===============

This section provides comprehensive platform-specific documentation for all supported platforms in the Nexus Embedded Platform. Each guide covers platform capabilities, build instructions, configuration options, hardware setup, example projects, and debugging procedures.

Supported Platforms
-------------------

The Nexus platform supports the following hardware platforms:

.. toctree::
   :maxdepth: 2

   native
   stm32f4
   stm32h7
   gd32

Platform Comparison
-------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Feature
     - Native
     - STM32F4
     - STM32H7
     - GD32
   * - **Core**
     - Simulated
     - Cortex-M4
     - Cortex-M7
     - Cortex-M3/M4
   * - **Max Frequency**
     - N/A
     - 180 MHz
     - 480 MHz
     - 120 MHz
   * - **Flash**
     - N/A
     - Up to 2 MB
     - Up to 2 MB
     - Up to 3 MB
   * - **SRAM**
     - N/A
     - Up to 256 KB
     - Up to 1 MB
     - Up to 256 KB
   * - **FPU**
     - N/A
     - Single
     - Double
     - Single
   * - **UART**
     - 4
     - 6
     - 8
     - 5
   * - **SPI**
     - 2
     - 6
     - 6
     - 3
   * - **I2C**
     - 2
     - 3
     - 4
     - 2
   * - **Timer**
     - 4
     - 14
     - 22
     - 14
   * - **ADC**
     - Optional
     - 3 (12-bit)
     - 3 (16-bit)
     - 3 (12-bit)
   * - **Ethernet**
     - No
     - No
     - Yes
     - No
   * - **USB**
     - No
     - 2
     - 2
     - 2
   * - **Use Case**
     - Testing/CI
     - General
     - High-perf
     - Cost-effective

Choosing a Platform
-------------------

Native Platform
~~~~~~~~~~~~~~~

**Best for:**

- Development without hardware
- Automated testing in CI/CD
- Rapid prototyping
- Learning the Nexus platform

**Not suitable for:**

- Production deployment
- Performance benchmarking
- Real-time requirements
- Hardware-specific features

STM32F4 Platform
~~~~~~~~~~~~~~~~

**Best for:**

- General-purpose embedded applications
- Cost-sensitive projects
- Moderate performance requirements
- Wide peripheral support

**Considerations:**

- Lower performance than STM32H7
- No Ethernet support
- Single-precision FPU only

STM32H7 Platform
~~~~~~~~~~~~~~~~

**Best for:**

- High-performance applications
- Ethernet connectivity
- Advanced peripherals
- DSP and signal processing

**Considerations:**

- Higher cost than STM32F4
- More complex clock configuration
- Cache management required for DMA

GD32 Platform
~~~~~~~~~~~~~

**Best for:**

- Cost-sensitive projects
- STM32 alternative
- General-purpose applications

**Considerations:**

- Less documentation than STM32
- Some compatibility differences
- Fewer third-party tools

Platform Selection Guide
------------------------

Use this decision tree to select the appropriate platform:

.. mermaid::
   :alt: Platform selection decision tree based on hardware needs, performance requirements, and budget constraints

   graph TD
       A[Start] --> B{Need Real Hardware?}
       B -->|No| C[Native Platform]
       B -->|Yes| D{Need Ethernet?}
       D -->|Yes| E[STM32H7]
       D -->|No| F{Performance Requirements?}
       F -->|High| E
       F -->|Moderate| G{Budget Constraint?}
       G -->|Yes| H[GD32]
       G-->|No| I[STM32F4]

Getting Started
---------------

To get started with a specific platform:

1. **Read the platform guide** - Understand capabilities and limitations
2. **Install toolchain** - Set up the development environment
3. **Configure platform** - Use Kconfig to configure peripherals
4. **Build example** - Build and run an example project
5. **Debug and test** - Use debugging tools to verify functionality

Common Tasks
------------

Building for a Platform
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Native
   CMake -B build -DPLATFORM=native
   CMake --build build

   # STM32F4
   CMake -B build -DPLATFORM=STM32 -DSTM32_CHIP=STM32F407 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake
   CMake --build build

   # STM32H7
   CMake -B build -DPLATFORM=STM32 -DSTM32_CHIP=STM32H743 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake
   CMake --build build

   # GD32
   CMake -B build -DPLATFORM=GD32 -DGD32_CHIP=GD32F407 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake
   CMake --build build

Configuring Peripherals
~~~~~~~~~~~~~~~~~~~~~~~

All platforms use Kconfig for configuration. See the platform-specific guides for detailed configuration options.

.. code-block:: bash

   # Copy platform defconfig
   cp platforms/native/defconfig .config

   # Or for STM32F4
   cp platforms/STM32/defconfig_stm32f4 .config

   # Customize configuration
   # Edit .config or use menuconfig

   # Build with configuration
   CMake -B build
   CMake --build build

Porting Between Platforms
~~~~~~~~~~~~~~~~~~~~~~~~~

The Nexus platform provides a unified HAL API, making it easy to port applications between platforms:

1. **Change platform in CMake** - Update ``-DPLATFORM=`` flag
2. **Update Kconfig** - Copy appropriate defconfig
3. **Adjust pin mappings** - Update GPIO/peripheral pin assignments
4. **Rebuild** - Recompile for new platform
5. **Test** - Verify functionality on new platform

See Also
--------

- :doc:`../user_guide/build_system` - Build system documentation
- :doc:`../user_guide/kconfig_platforms` - Platform configuration guide
- :doc:`../user_guide/hal` - HAL API documentation
- :doc:`../user_guide/osal` - OSAL API documentation
- :doc:`../user_guide/porting` - Porting guide
