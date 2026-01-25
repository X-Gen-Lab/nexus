STM32F4 Platform Guide
======================

Overview
--------

The STM32F4 platform provides support for STM32F4 series microcontrollers from STMicroelectronics. This platform targets high-performance ARM Cortex-M4 MCUs with DSP and FPU capabilities, suitable for a wide range of embedded applications.

**Supported Variants:**

- STM32F407 (default)
- STM32F429
- STM32F446

**Key Features:**

- ARM Cortex-M4 core with FPU
- Up to 180 MHz operation
- Up to 2 MB Flash, 256 KB SRAM
- Rich peripheral set
- DMA support
- FreeRTOS integration

Platform Capabilities
---------------------

Supported Peripherals
~~~~~~~~~~~~~~~~~~~~~

The STM32F4 platform supports the following peripherals:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Peripheral
     - Max Instances
     - Capabilities
   * - UART/USART
     - 6
     - Async/Sync TX/RX, DMA, hardware flow control
   * - GPIO
     - 16 pins per port
     - Input/Output, alternate functions, interrupts
   * - SPI
     - 6
     - Master/Slave, DMA, NSS management
   * - I2C
     - 3
     - Master/Slave, DMA, 10-bit addressing
   * - Timer
     - 14
     - General-purpose, advanced, basic timers
   * - ADC
     - 3
     - 12-bit resolution, DMA, multi-channel
   * - DAC
     - 2
     - 12-bit resolution, DMA
   * - CAN
     - 2
     - CAN 2.0A/B, up to 1 Mbps
   * - USB
     - 2
     - USB OTG FS/HS
   * - SDIO
     - 1
     - SD/MMC card interface
   * - RTC
     - 1
     - Calendar, alarm, tamper detection
   * - Watchdog
     - 2
     - Independent and window watchdog

Platform Limitations
~~~~~~~~~~~~~~~~~~~~

- **Clock Configuration**: Requires proper clock tree configuration
- **Pin Multiplexing**: Limited pins require careful planning
- **DMA Channels**: Limited DMA channels may require prioritization
- **Power Consumption**: Higher power consumption than low-power variants
- **Temperature Range**: Standard range (-40°C to +85°C)

Build Instructions
------------------

Prerequisites
~~~~~~~~~~~~~

- CMake 3.15 or higher
- ARM GCC toolchain (arm-none-eabi-gcc)
- Python 3.7+ (for build scripts)
- OpenOCD or ST-Link utilities (for flashing)

Toolchain Installation
~~~~~~~~~~~~~~~~~~~~~~

**Windows:**

.. code-block:: powershell

   # Download ARM GCC from ARM website
   # Add to PATH
   $env:PATH += ";C:\Program Files (x86)\GNU Arm Embedded Toolchain\bin"

**Linux:**

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install gcc-arm-none-eabi

   # Arch Linux
   sudo pacman -S arm-none-eabi-gcc

**macOS:**

.. code-block:: bash

   # Using Homebrew
   brew install --cask gcc-arm-embedded

Basic Build
~~~~~~~~~~~

.. code-block:: bash

   # Configure for STM32F4 platform
   CMake -B build -DPLATFORM=STM32 -DSTM32_CHIP=STM32F407 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake

   # Build
   CMake --build build

   # Generate binary
   arm-none-eabi-objcopy -O binary build/app.elf build/app.bin

Using Build Scripts
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Build for STM32F4
   python scripts/nexus.py build --platform STM32 --chip stm32f407

   # Build with specific configuration
   python scripts/nexus.py build --platform STM32 --chip stm32f407 \
          --config Release

Kconfig Options
---------------

Platform Selection
~~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   CONFIG_PLATFORM_STM32=y
   CONFIG_PLATFORM_NAME="STM32"

Chip Selection
~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # STM32F4 family
   CONFIG_STM32F4=y

   # Specific variant
   CONFIG_STM32F407=y
   # CONFIG_STM32F429 is not set
   # CONFIG_STM32F446 is not set

   CONFIG_STM32_CHIP_NAME="STM32F407xx"

Platform Settings
~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # Platform identification
   CONFIG_STM32_PLATFORM_NAME="STM32 Platform"
   CONFIG_STM32_PLATFORM_VERSION="1.0.0"

   # Logging
   CONFIG_STM32_ENABLE_LOGGING=y
   CONFIG_STM32_LOG_LEVEL=3

   # Statistics
   CONFIG_STM32_ENABLE_STATISTICS=y

   # Memory alignment
   CONFIG_STM32_BUFFER_ALIGNMENT=4

Resource Managers
~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # DMA channels
   CONFIG_STM32_DMA_CHANNELS=8

   # ISR slots
   CONFIG_STM32_ISR_SLOTS=64

Peripheral Configuration
~~~~~~~~~~~~~~~~~~~~~~~~

**UART Configuration:**

.. code-block:: Kconfig

   CONFIG_STM32_UART_ENABLE=y
   CONFIG_STM32_UART_MAX_INSTANCES=6
   CONFIG_INSTANCE_STM32_UART_1=y
   CONFIG_UART1_BAUDRATE=115200
   CONFIG_UART1_DATA_BITS=8
   CONFIG_UART1_STOP_BITS=1
   CONFIG_UART1_PARITY_NONE=y
   CONFIG_UART1_MODE_DMA=y
   CONFIG_UART1_TX_BUFFER_SIZE=256
   CONFIG_UART1_RX_BUFFER_SIZE=256
   CONFIG_UART1_DMA_TX_CHANNEL=4
   CONFIG_UART1_DMA_RX_CHANNEL=5
   CONFIG_UART1_TX_PIN=9
   CONFIG_UART1_RX_PIN=10
   CONFIG_UART1_TX_PORT="GPIOA"
   CONFIG_UART1_RX_PORT="GPIOA"

**GPIO Configuration:**

.. code-block:: Kconfig

   CONFIG_STM32_GPIO_ENABLE=y

**SPI Configuration:**

.. code-block:: Kconfig

   CONFIG_STM32_SPI_ENABLE=y

**I2C Configuration:**

.. code-block:: Kconfig

   CONFIG_STM32_I2C_ENABLE=y

OSAL Configuration
~~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # FreeRTOS backend
   CONFIG_OSAL_FREERTOS=y
   CONFIG_OSAL_BACKEND_NAME="FreeRTOS"
   CONFIG_OSAL_TICK_RATE_HZ=1000
   CONFIG_OSAL_HEAP_SIZE=32768
   CONFIG_OSAL_MAIN_STACK_SIZE=2048
   CONFIG_OSAL_MAX_PRIORITIES=32

Linker Configuration
~~~~~~~~~~~~~~~~~~~~

.. code-block:: Kconfig

   # Memory layout for STM32F407
   CONFIG_LINKER_RAM_START=0x20000000
   CONFIG_LINKER_RAM_SIZE=0x00020000    # 128 KB
   CONFIG_LINKER_FLASH_START=0x08000000
   CONFIG_LINKER_FLASH_SIZE=0x00100000  # 1 MB

Hardware Setup
--------------

Development Boards
~~~~~~~~~~~~~~~~~~

**Recommended Boards:**

- STM32F4DISCOVERY (STM32F407VGT6)
- NUCLEO-F446RE
- Custom boards with STM32F4 series

Pin Configuration
~~~~~~~~~~~~~~~~~

**UART1 Pins (Default):**

.. code-block:: text

   TX: PA9  (USART1_TX)
   RX: PA10 (USART1_RX)

**SPI1 Pins:**

.. code-block:: text

   SCK:  PA5  (SPI1_SCK)
   MISO: PA6  (SPI1_MISO)
   MOSI: PA7  (SPI1_MOSI)
   NSS:  PA4  (SPI1_NSS)

**I2C1 Pins:**

.. code-block:: text

   SCL: PB6 (I2C1_SCL)
   SDA: PB7 (I2C1_SDA)

Power Supply
~~~~~~~~~~~~

- **VDD**: 1.8V to 3.6V
- **VDDA**: Analog supply (same as VDD or separate)
- **VBAT**: Battery backup for RTC (optional)

**Recommended Power Setup:**

- Use 3.3V regulated supply
- Add decoupling capacitors (100nF) near each VDD pin
- Add bulk capacitor (10µF) near power input

Clock Configuration
~~~~~~~~~~~~~~~~~~~

**External Crystal:**

- HSE: 8 MHz (typical for discovery boards)
- LSE: 32.768 kHz (for RTC)

**PLL Configuration for 168 MHz:**

.. code-block:: c

   /* HSE = 8 MHz */
   /* PLL_M = 8, PLL_N = 336, PLL_P = 2, PLL_Q = 7 */
   /* SYSCLK = 168 MHz */
   /* AHB = 168 MHz, APB1 = 42 MHz, APB2 = 84 MHz */

Example Projects
----------------

Basic UART Example
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_uart.h"
   #include "hal/nx_factory.h"
   #include "osal/nx_osal.h"

   int main(void) {
       /* Initialize OSAL */
       nx_osal_init();

       /* Get UART instance */
       nx_uart_t* uart = nx_factory_uart(1);  /* UART1 */
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
       const uint8_t data[] = "Hello, STM32F4!";
       size_t len = sizeof(data) - 1;
       tx->transmit(tx, data, len);

       /* Start RTOS scheduler */
       nx_osal_start();

       /* Should never reach here */
       return 0;
   }

FreeRTOS Task Example
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "osal/nx_osal.h"
   #include "hal/nx_gpio.h"

   void led_task(void* param) {
       nx_gpio_t* gpio = nx_factory_gpio(0);
       nx_lifecycle_t* lc = gpio->get_lifecycle(gpio);
       lc->init(lc);

       /* Configure LED pin */
       nx_gpio_config_t config = {
           .pin = 13,  /* PD13 on STM32F4DISCOVERY */
           .mode = NX_GPIO_MODE_OUTPUT,
           .pull = NX_GPIO_PULL_NONE,
           .speed = NX_GPIO_SPEED_LOW
       };
       gpio->configure(gpio, &config);

       /* Blink LED */
       while (1) {
           gpio->write_pin(gpio, 13, 1);
           nx_osal_delay_ms(500);
           gpio->write_pin(gpio, 13, 0);
           nx_osal_delay_ms(500);
       }
   }

   int main(void) {
       nx_osal_init();

       /* Create task */
       nx_osal_task_create(led_task, "LED", 512, NULL, 1, NULL);

       /* Start scheduler */
       nx_osal_start();
       return 0;
   }

DMA UART Example
~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_uart.h"

   void uart_dma_example(void) {
       nx_uart_t* uart = nx_factory_uart(1);
       nx_lifecycle_t* lc = uart->get_lifecycle(uart);
       lc->init(lc);

       /* DMA is configured via Kconfig */
       /* CONFIG_UART1_MODE_DMA=y */

       /* Transmit with DMA */
       nx_tx_async_t* tx = uart->get_tx_async(uart);
       const uint8_t data[] = "DMA Transfer";
       size_t len = sizeof(data) - 1;
       tx->transmit(tx, data, len);

       /* DMA handles transfer in background */
   }

Debugging Procedures
--------------------

Using OpenOCD
~~~~~~~~~~~~~

**Install OpenOCD:**

.. code-block:: bash

   # Linux
   sudo apt-get install openocd

   # macOS
   brew install openocd

**Start OpenOCD:**

.. code-block:: bash

   # For STM32F4DISCOVERY
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

**Connect GDB:**

.. code-block:: bash

   arm-none-eabi-gdb build/app.elf
   (gdb) target remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) continue

Using ST-Link Utility
~~~~~~~~~~~~~~~~~~~~~

**Flash Firmware:**

.. code-block:: bash

   # Using st-flash
   st-flash write build/app.bin 0x08000000

**Erase Flash:**

.. code-block:: bash

   st-flash erase

Using Visual Studio Code
~~~~~~~~~~~~~~~~~~~~~~~~

Create ``.vscode/launch.json``:

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
               "executable": "${workspaceRoot}/build/app.elf",
               "device": "STM32F407VG",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32f4x.cfg"
               ]
           }
       ]
   }

Serial Console
~~~~~~~~~~~~~~

**Connect to UART:**

.. code-block:: bash

   # Linux
   screen /dev/ttyUSB0 115200

   # Windows (using PuTTY)
   # COM port, 115200 baud, 8N1

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Build Fails with "arm-none-eabi-gcc not found":**

Solution: Install ARM GCC toolchain and add to PATH.

**Flash Fails with "Error: init mode failed":**

Solution: Check ST-Link connection, try different USB port, update ST-Link firmware.

**UART Not Working:**

Solution: Verify pin configuration matches hardware, check baud rate, ensure clock is configured.

**DMA Not Transferring:**

Solution: Verify DMA channel configuration, check buffer alignment, ensure DMA clock is enabled.

Clock Configuration Issues
~~~~~~~~~~~~~~~~~~~~~~~~~~

If system doesn't start or runs at wrong speed:

1. Verify HSE frequency matches hardware (usually 8 MHz)
2. Check PLL configuration
3. Ensure flash wait states are configured for clock speed
4. Verify voltage regulator scale setting

Memory Issues
~~~~~~~~~~~~~

**Stack Overflow:**

Solution: Increase stack size in Kconfig (``CONFIG_OSAL_MAIN_STACK_SIZE``).

**Heap Exhausted:**

Solution: Increase heap size (``CONFIG_OSAL_HEAP_SIZE``) or use static allocation.

**Linker Error "region RAM overflowed":**

Solution: Reduce memory usage or use external RAM.

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

- Enable compiler optimizations (``-O2`` or ``-O3``)
- Use DMA for data transfers
- Enable instruction and data caches
- Use FPU for floating-point operations
- Optimize interrupt priorities

See Also
--------

- :doc:`../user_guide/build_system` - Build system documentation
- :doc:`../user_guide/kconfig_platforms` - Platform configuration guide
- :doc:`../development/testing` - Testing guide
- :doc:`native` - Native platform guide
- :doc:`stm32h7` - STM32H7 platform guide
- :doc:`gd32` - GD32 platform guide
- `STM32F4 Reference Manual <https://www.st.com/resource/en/reference_manual/dm00031020.pdf>`_
- `STM32F4 Datasheet <https://www.st.com/resource/en/datasheet/stm32f407vg.pdf>`_
