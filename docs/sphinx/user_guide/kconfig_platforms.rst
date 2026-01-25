Platform-Specific Configuration Guide
=====================================

This guide provides detailed configuration information for each supported platform in the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The Nexus platform supports multiple hardware platforms, each with unique characteristics and configuration requirements:

* **Native**: PC simulation platform for development and testing
* **STM32F4**: ARM Cortex-M4 microcontrollers (STM32F407, STM32F429, STM32F446)
* **STM32H7**: ARM Cortex-M7 microcontrollers (STM32H743, STM32H750)
* **GD32**: GigaDevice microcontrollers (GD32F303, GD32F307)
* **ESP32**: Espressif ESP32 WiFi/Bluetooth SoC

Platform Selection
------------------

Select one platform using the choice menu:

.. code-block:: Kconfig

    choice
        prompt "Target Platform"
        default PLATFORM_NATIVE

    config PLATFORM_NATIVE
        bool "Native Platform (PC Simulation)"

    config PLATFORM_STM32
        bool "STM32 Platform"

    config PLATFORM_GD32
        bool "GD32 Platform"

    config PLATFORM_ESP32
        bool "ESP32 Platform"

    config PLATFORM_NRF52
        bool "NRF52 Platform"

    endchoice

Only one platform can be selected at a time.


Native Platform
---------------

Overview
^^^^^^^^

The Native platform provides PC simulation for development and testing without hardware. It runs on Windows, Linux, and macOS.

**Features:**

* Simulated peripherals (UART, GPIO, SPI, I2C, Timer, ADC)
* No hardware required
* Fast development iteration
* Cross-platform support
* Debugging with standard tools (GDB, Visual Studio)

Quick Start
^^^^^^^^^^^

**1. Use default configuration:**

.. code-block:: bash

    cp platforms/native/defconfig .config
    python scripts/Kconfig/generate_config.py

**2. Build:**

.. code-block:: bash

    CMake -B build -DPLATFORM=native
    CMake --build build

**3. Run:**

.. code-block:: bash

    ./build/applications/blinky/blinky

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_NATIVE=y
    CONFIG_NATIVE_PLATFORM_NAME="Native Platform (PC Simulation)"
    CONFIG_NATIVE_PLATFORM_VERSION="1.0.0"

**Platform settings:**

.. code-block:: Kconfig

    # Logging
    CONFIG_NATIVE_ENABLE_LOGGING=y
    CONFIG_NATIVE_LOG_LEVEL=3  # 0=None, 1=Error, 2=Warning, 3=Info, 4=Debug

    # Statistics
    CONFIG_NATIVE_ENABLE_STATISTICS=y

    # Memory alignment
    CONFIG_NATIVE_BUFFER_ALIGNMENT=4

**Resource managers:**

.. code-block:: Kconfig

    CONFIG_NATIVE_DMA_CHANNELS=8
    CONFIG_NATIVE_ISR_SLOTS=64

Supported Peripherals
^^^^^^^^^^^^^^^^^^^^^

**UART:**

.. code-block:: Kconfig

    CONFIG_NATIVE_UART_ENABLE=y
    CONFIG_NATIVE_UART_MAX_INSTANCES=4
    CONFIG_INSTANCE_NX_UART_0=y
    CONFIG_UART0_BAUDRATE=115200

**GPIO:**

.. code-block:: Kconfig

    CONFIG_NATIVE_GPIO_ENABLE=y
    CONFIG_INSTANCE_NX_GPIOA=y
    CONFIG_INSTANCE_NX_GPIOA_PIN0=y

**SPI:**

.. code-block:: Kconfig

    CONFIG_NATIVE_SPI_ENABLE=y
    CONFIG_INSTANCE_NX_SPI_0=y
    CONFIG_SPI0_MAX_SPEED=1000000

**I2C:**

.. code-block:: Kconfig

    CONFIG_NATIVE_I2C_ENABLE=y
    CONFIG_INSTANCE_NX_I2C_0=y
    CONFIG_NX_I2C0_SPEED_FAST=y

**Timer:**

.. code-block:: Kconfig

    CONFIG_NATIVE_TIMER_ENABLE=y
    CONFIG_INSTANCE_NX_TIMER_0=y
    CONFIG_TIMER0_FREQUENCY=1000000

**ADC:**

.. code-block:: Kconfig

    CONFIG_NATIVE_ADC_ENABLE=y
    CONFIG_INSTANCE_NX_ADC_0=y
    CONFIG_NX_ADC0_RESOLUTION_12=y

OSAL Configuration
^^^^^^^^^^^^^^^^^^

**Native OSAL backend:**

.. code-block:: Kconfig

    CONFIG_OSAL_NATIVE=y
    CONFIG_OSAL_BACKEND_NAME="native"
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

**Alternative: FreeRTOS on Native:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_BACKEND_NAME="FreeRTOS"
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768

Limitations
^^^^^^^^^^^

* Simulated peripherals don't match real hardware timing
* No real interrupt latency
* No power management
* No hardware-specific features

Best Practices
^^^^^^^^^^^^^^

* Use Native platform for algorithm development
* Test hardware-independent code
* Validate configuration before hardware deployment
* Use for CI/CD testing


STM32F4 Platform
----------------

Overview
^^^^^^^^

The STM32F4 platform supports STMicroelectronics STM32F4 series ARM Cortex-M4 microcontrollers.

**Supported Variants:**

* STM32F407: 168 MHz, 1 MB Flash, 192 KB RAM
* STM32F429: 180 MHz, 2 MB Flash, 256 KB RAM
* STM32F446: 180 MHz, 512 KB Flash, 128 KB RAM

**Features:**

* ARM Cortex-M4 with FPU
* Up to 180 MHz
* Rich peripheral set
* DMA support
* Low power modes

Quick Start
^^^^^^^^^^^

**1. Use default configuration:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32f4 .config
    python scripts/Kconfig/generate_config.py

**2. Build:**

.. code-block:: bash

    CMake -B build -DPLATFORM=STM32 -DCHIP=STM32F407
    CMake --build build

**3. Flash:**

.. code-block:: bash

    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/applications/blinky/blinky.elf verify reset exit"

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Platform selection:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32_PLATFORM_NAME="STM32 Platform"
    CONFIG_STM32_PLATFORM_VERSION="1.0.0"

**Chip selection:**

.. code-block:: Kconfig

    # Chip family
    CONFIG_STM32F4=y

    # Chip variant
    CONFIG_STM32F407=y
    CONFIG_STM32_CHIP_NAME="STM32F407xx"

**Platform settings:**

.. code-block:: Kconfig

    CONFIG_STM32_ENABLE_LOGGING=y
    CONFIG_STM32_LOG_LEVEL=3
    CONFIG_STM32_ENABLE_STATISTICS=y
    CONFIG_STM32_BUFFER_ALIGNMENT=4

**Resource managers:**

.. code-block:: Kconfig

    CONFIG_STM32_DMA_CHANNELS=8
    CONFIG_STM32_ISR_SLOTS=64

Memory Configuration
^^^^^^^^^^^^^^^^^^^^

**STM32F407 memory layout:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x00020000      # 128 KB
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00100000    # 1 MB

**STM32F429 memory layout:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x00030000      # 192 KB
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00200000    # 2 MB

**STM32F446 memory layout:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x00020000      # 128 KB
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00080000    # 512 KB

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**UART with DMA:**

.. code-block:: Kconfig

    CONFIG_STM32_UART_ENABLE=y
    CONFIG_STM32_UART_MAX_INSTANCES=6
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_MODE_DMA=y
    CONFIG_UART1_DMA_TX_CHANNEL=4
    CONFIG_UART1_DMA_RX_CHANNEL=5
    CONFIG_UART1_TX_PIN=9
    CONFIG_UART1_RX_PIN=10
    CONFIG_UART1_TX_PORT="GPIOA"
    CONFIG_UART1_RX_PORT="GPIOA"

**GPIO:**

.. code-block:: Kconfig

    CONFIG_STM32_GPIO_ENABLE=y

**SPI:**

.. code-block:: Kconfig

    CONFIG_STM32_SPI_ENABLE=y

**I2C:**

.. code-block:: Kconfig

    CONFIG_STM32_I2C_ENABLE=y

**Timer:**

.. code-block:: Kconfig

    CONFIG_STM32_TIMER_ENABLE=y

OSAL Configuration
^^^^^^^^^^^^^^^^^^

**FreeRTOS (recommended):**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_BACKEND_NAME="FreeRTOS"
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

**Bare-metal:**

.. code-block:: Kconfig

    CONFIG_OSAL_BAREMETAL=y
    CONFIG_OSAL_BACKEND_NAME="baremetal"
    CONFIG_OSAL_HEAP_SIZE=16384

Clock Configuration
^^^^^^^^^^^^^^^^^^^

**STM32F407 (168 MHz):**

* HSE: 8 MHz external crystal
* PLL: 168 MHz system clock
* AHB: 168 MHz
* APB1: 42 MHz
* APB2: 84 MHz

**STM32F429 (180 MHz):**

* HSE: 8 MHz external crystal
* PLL: 180 MHz system clock
* AHB: 180 MHz
* APB1: 45 MHz
* APB2: 90 MHz

Development Boards
^^^^^^^^^^^^^^^^^^

**STM32F407 Discovery:**

* STM32F407VGT6
* 8 MHz HSE
* ST-LINK/V2 debugger
* LEDs on PD12-PD15
* User button on PA0

**STM32F429 Discovery:**

* STM32F429ZIT6
* 8 MHz HSE
* ST-LINK/V2-B debugger
* 2.4" LCD display
* LEDs on PG13-PG14

Debugging
^^^^^^^^^

**OpenOCD:**

.. code-block:: bash

    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

**GDB:**

.. code-block:: bash

    arm-none-eabi-gdb build/applications/blinky/blinky.elf
    (gdb) target remote localhost:3333
    (gdb) monitor reset halt
    (gdb) load
    (gdb) continue

**ST-LINK Utility:**

* Use ST-LINK Utility for flashing
* Supports mass erase and option bytes

Limitations
^^^^^^^^^^^

* Maximum 180 MHz clock
* Limited RAM compared to STM32H7
* No cache (except STM32F429)
* Single-precision FPU only

Best Practices
^^^^^^^^^^^^^^

* Use DMA for high-throughput peripherals
* Enable FPU for floating-point operations
* Configure clock tree carefully
* Use appropriate power modes
* Validate pin assignments with datasheet


STM32H7 Platform
----------------

Overview
^^^^^^^^

The STM32H7 platform supports STMicroelectronics STM32H7 series ARM Cortex-M7 microcontrollers.

**Supported Variants:**

* STM32H743: 480 MHz, 2 MB Flash, 1 MB RAM
* STM32H750: 480 MHz, 128 KB Flash, 1 MB RAM (bootloader variant)

**Features:**

* ARM Cortex-M7 with double-precision FPU
* Up to 480 MHz
* Large RAM (up to 1 MB)
* Advanced peripherals
* Dual-bank Flash
* Hardware crypto accelerator
* Cache (I-Cache, D-Cache)

Quick Start
^^^^^^^^^^^

**1. Use default configuration:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32h7 .config
    python scripts/Kconfig/generate_config.py

**2. Build:**

.. code-block:: bash

    CMake -B build -DPLATFORM=STM32 -DCHIP=STM32H743
    CMake --build build

**3. Flash:**

.. code-block:: bash

    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
        -c "program build/applications/blinky/blinky.elf verify reset exit"

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Platform selection:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32_PLATFORM_NAME="STM32 Platform"
    CONFIG_STM32_PLATFORM_VERSION="1.0.0"

**Chip selection:**

.. code-block:: Kconfig

    # Chip family
    CONFIG_STM32H7=y

    # Chip variant
    CONFIG_STM32H743=y
    CONFIG_STM32_CHIP_NAME="STM32H743xx"

**Platform settings:**

.. code-block:: Kconfig

    CONFIG_STM32_ENABLE_LOGGING=y
    CONFIG_STM32_LOG_LEVEL=3
    CONFIG_STM32_ENABLE_STATISTICS=y
    CONFIG_STM32_BUFFER_ALIGNMENT=4

Memory Configuration
^^^^^^^^^^^^^^^^^^^^

**STM32H743 memory layout:**

.. code-block:: Kconfig

    # DTCM RAM (64 KB) - fastest, for stack and critical data
    CONFIG_LINKER_DTCM_START=0x20000000
    CONFIG_LINKER_DTCM_SIZE=0x00010000

    # AXI SRAM (512 KB) - main RAM
    CONFIG_LINKER_RAM_START=0x24000000
    CONFIG_LINKER_RAM_SIZE=0x00080000

    # SRAM1 (128 KB)
    CONFIG_LINKER_SRAM1_START=0x30000000
    CONFIG_LINKER_SRAM1_SIZE=0x00020000

    # SRAM2 (128 KB)
    CONFIG_LINKER_SRAM2_START=0x30020000
    CONFIG_LINKER_SRAM2_SIZE=0x00020000

    # SRAM3 (32 KB)
    CONFIG_LINKER_SRAM3_START=0x30040000
    CONFIG_LINKER_SRAM3_SIZE=0x00008000

    # SRAM4 (64 KB) - backup domain
    CONFIG_LINKER_SRAM4_START=0x38000000
    CONFIG_LINKER_SRAM4_SIZE=0x00010000

    # Flash (2 MB, dual-bank)
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00200000

**STM32H750 memory layout:**

.. code-block:: Kconfig

    # Same RAM as STM32H743
    CONFIG_LINKER_RAM_START=0x24000000
    CONFIG_LINKER_RAM_SIZE=0x00080000

    # Smaller Flash (128 KB)
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00020000

Memory Regions
^^^^^^^^^^^^^^

**DTCM (Data Tightly Coupled Memory):**

* 64 KB
* Zero wait state
* Best for stack and frequently accessed data
* Not accessible by DMA

**AXI SRAM:**

* 512 KB
* Main application RAM
* Accessible by all masters
* Use for general purpose

**SRAM1/2/3:**

* 128 KB + 128 KB + 32 KB
* Accessible by DMA
* Use for DMA buffers

**SRAM4:**

* 64 KB
* Backup domain
* Retained in standby mode
* Use for persistent data

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**UART with DMA:**

.. code-block:: Kconfig

    CONFIG_STM32_UART_ENABLE=y
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_MODE_DMA=y
    CONFIG_UART1_DMA_TX_CHANNEL=4
    CONFIG_UART1_DMA_RX_CHANNEL=5

**High-speed SPI:**

.. code-block:: Kconfig

    CONFIG_STM32_SPI_ENABLE=y
    CONFIG_INSTANCE_STM32_SPI_1=y
    CONFIG_SPI1_MAX_SPEED=50000000  # 50 MHz

**Fast I2C:**

.. code-block:: Kconfig

    CONFIG_STM32_I2C_ENABLE=y
    CONFIG_INSTANCE_STM32_I2C_1=y
    CONFIG_NX_I2C1_SPEED_FAST_PLUS=y  # 1 MHz

OSAL Configuration
^^^^^^^^^^^^^^^^^^

**FreeRTOS (recommended):**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_BACKEND_NAME="FreeRTOS"
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

Clock Configuration
^^^^^^^^^^^^^^^^^^^

**STM32H743 (480 MHz):**

* HSE: 25 MHz external crystal
* PLL1: 480 MHz system clock
* AHB: 240 MHz (with divider)
* APB1: 120 MHz
* APB2: 120 MHz
* APB3: 120 MHz
* APB4: 120 MHz

**Clock domains:**

* sys_ck: 480 MHz (CPU)
* hclk: 240 MHz (AHB)
* pclk1-4: 120 MHz (APB)

Cache Configuration
^^^^^^^^^^^^^^^^^^^

**Instruction Cache:**

.. code-block:: Kconfig

    CONFIG_STM32H7_ICACHE_ENABLE=y

**Data Cache:**

.. code-block:: Kconfig

    CONFIG_STM32H7_DCACHE_ENABLE=y

**Cache coherency:**

* Use MPU to configure cacheable regions
* Mark DMA buffers as non-cacheable
* Use cache maintenance operations

Development Boards
^^^^^^^^^^^^^^^^^^

**NUCLEO-H743ZI:**

* STM32H743ZIT6
* 25 MHz HSE
* ST-LINK/V2-1 debugger
* Arduino Uno V3 connectors
* Ethernet PHY

**STM32H743I-EVAL:**

* STM32H743XIH6
* Full evaluation board
* LCD display
* Audio codec
* Ethernet
* USB HS/FS

Debugging
^^^^^^^^^

**OpenOCD:**

.. code-block:: bash

    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

**GDB:**

.. code-block:: bash

    arm-none-eabi-gdb build/applications/blinky/blinky.elf
    (gdb) target remote localhost:3333
    (gdb) monitor reset halt
    (gdb) load
    (gdb) continue

Performance Optimization
^^^^^^^^^^^^^^^^^^^^^^^^

**Enable caches:**

.. code-block:: c

    SCB_EnableICache();
    SCB_EnableDCache();

**Use DTCM for stack:**

.. code-block:: c

    __attribute__((section(".dtcm"))) uint8_t stack[4096];

**DMA buffers in SRAM1:**

.. code-block:: c

    __attribute__((section(".sram1"))) uint8_t dma_buffer[1024];

**MPU configuration:**

* Configure Flash as cacheable
* Configure RAM as cacheable (except DMA buffers)
* Use write-through for shared data

Limitations
^^^^^^^^^^^

* Complex clock tree
* Multiple RAM regions require careful planning
* Cache coherency considerations
* Higher power consumption than STM32F4

Best Practices
^^^^^^^^^^^^^^

* Use DTCM for stack and critical data
* Enable caches for performance
* Configure MPU for cache coherency
* Use appropriate RAM region for each purpose
* Validate clock configuration
* Test with cache enabled/disabled


GD32 Platform
-------------

Overview
^^^^^^^^

The GD32 platform supports GigaDevice GD32 series ARM Cortex-M microcontrollers, which are compatible with STM32.

**Supported Variants:**

* GD32F303: Cortex-M4, 120 MHz, 256 KB Flash, 48 KB RAM
* GD32F307: Cortex-M4, 120 MHz, 512 KB Flash, 96 KB RAM

**Features:**

* STM32-compatible peripherals
* Lower cost alternative
* Good peripheral set
* DMA support

Quick Start
^^^^^^^^^^^

**1. Create configuration:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_GD32=y
    CONFIG_GD32F30X=y
    CONFIG_GD32F303=y

**2. Build:**

.. code-block:: bash

    CMake -B build -DPLATFORM=GD32 -DCHIP=GD32F303
    CMake --build build

**3. Flash:**

.. code-block:: bash

    openocd -f interface/stlink.cfg -f target/gd32f3x.cfg \
        -c "program build/applications/blinky/blinky.elf verify reset exit"

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Platform selection:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_GD32=y
    CONFIG_GD32_PLATFORM_NAME="GD32 Platform"

**Chip selection:**

.. code-block:: Kconfig

    CONFIG_GD32F30X=y
    CONFIG_GD32F303=y
    CONFIG_GD32_CHIP_NAME="GD32F303xx"

Memory Configuration
^^^^^^^^^^^^^^^^^^^^

**GD32F303:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x0000C000      # 48 KB
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00040000    # 256 KB

**GD32F307:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x00018000      # 96 KB
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00080000    # 512 KB

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

GD32 peripherals are configured similarly to STM32:

.. code-block:: Kconfig

    # UART
    CONFIG_GD32_UART_ENABLE=y
    CONFIG_INSTANCE_GD32_UART_0=y
    CONFIG_UART0_BAUDRATE=115200

    # GPIO
    CONFIG_GD32_GPIO_ENABLE=y

    # SPI
    CONFIG_GD32_SPI_ENABLE=y

    # I2C
    CONFIG_GD32_I2C_ENABLE=y

OSAL Configuration
^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=16384

Differences from STM32
^^^^^^^^^^^^^^^^^^^^^^

* Some peripheral register differences
* Different USB implementation
* Slightly different DMA channels
* Check GD32 datasheet for specifics

Best Practices
^^^^^^^^^^^^^^

* Verify peripheral compatibility
* Test thoroughly
* Use GD32-specific documentation
* Consider STM32 as reference

ESP32 Platform
--------------

Overview
^^^^^^^^

The ESP32 platform supports Espressif ESP32 WiFi/Bluetooth SoC.

**Features:**

* Dual-core Xtensa LX6
* WiFi 802.11 b/g/n
* Bluetooth Classic and BLE
* Rich peripheral set
* Large ecosystem

Quick Start
^^^^^^^^^^^

**1. Create configuration:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_ESP32=y
    CONFIG_ESP32_WROOM_32=y

**2. Build:**

.. code-block:: bash

    idf.py build

**3. Flash:**

.. code-block:: bash

    idf.py flash

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_PLATFORM_ESP32=y
    CONFIG_ESP32_PLATFORM_NAME="ESP32 Platform"

**Module selection:**

.. code-block:: Kconfig

    CONFIG_ESP32_WROOM_32=y

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    # UART
    CONFIG_ESP32_UART_ENABLE=y

    # GPIO
    CONFIG_ESP32_GPIO_ENABLE=y

    # SPI
    CONFIG_ESP32_SPI_ENABLE=y

    # I2C
    CONFIG_ESP32_I2C_ENABLE=y

    # WiFi
    CONFIG_ESP32_WIFI_ENABLE=y

    # Bluetooth
    CONFIG_ESP32_BT_ENABLE=y

OSAL Configuration
^^^^^^^^^^^^^^^^^^

ESP32 uses FreeRTOS:

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536

WiFi Configuration
^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_ESP32_WIFI_SSID="MyNetwork"
    CONFIG_ESP32_WIFI_PASSWORD="MyPassword"
    CONFIG_ESP32_WIFI_MODE_STA=y

Best Practices
^^^^^^^^^^^^^^

* Use ESP-IDF for WiFi/BT
* Manage dual-core carefully
* Monitor heap usage
* Use appropriate power modes

Platform Comparison
-------------------

Feature Comparison
^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 15 15 20

   * - Feature
     - Native
     - STM32F4
     - STM32H7
     - GD32
     - ESP32
   * - CPU
     - x86/ARM
     - Cortex-M4
     - Cortex-M7
     - Cortex-M4
     - Xtensa LX6
   * - Max Freq
     - N/A
     - 180 MHz
     - 480 MHz
     - 120 MHz
     - 240 MHz
   * - Flash
     - N/A
     - Up to 2 MB
     - Up to 2 MB
     - Up to 512 KB
     - Up to 16 MB
   * - RAM
     - N/A
     - Up to 256 KB
     - Up to 1 MB
     - Up to 96 KB
     - 520 KB
   * - FPU
     - Yes
     - Single
     - Double
     - Single
     - No
   * - WiFi
     - No
     - No
     - No
     - No
     - Yes
   * - Cost
     - Free
     - Low
     - Medium
     - Very Low
     - Low

Use Case Recommendations
^^^^^^^^^^^^^^^^^^^^^^^^

**Native:**

* Development and testing
* Algorithm validation
* CI/CD pipelines
* Cross-platform development

**STM32F4:**

* General embedded applications
* Motor control
* Industrial automation
* Cost-sensitive projects

**STM32H7:**

* High-performance applications
* Digital signal processing
* Graphics applications
* Data acquisition

**GD32:**

* Cost-sensitive projects
* STM32 alternative
* Simple applications

**ESP32:**

* IoT applications
* WiFi connectivity
* Bluetooth devices
* Smart home

See Also
--------

* :doc:`kconfig_tutorial` - Kconfig tutorial
* :doc:`kconfig_peripherals` - Peripheral configuration
* :doc:`build_system` - Build system guide
* :doc:`porting` - Platform porting guide
