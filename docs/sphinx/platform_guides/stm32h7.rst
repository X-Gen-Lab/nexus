STM32H7 Platform Guide
======================

Overview
--------

The STM32H7 platform provides support for STM32H7 series microcontrollers from STMicroelectronics. This platform targets high-performance ARM Cortex-M7 MCUs with advanced features, suitable for demanding embedded applications requiring high processing power and memory bandwidth.

**Supported Variants:**

- STM32H743 (default)
- STM32H750

**Key Features:**

- ARM Cortex-M7 core with double-precision FPU
- Up to 480 MHz operation
- Up to 2 MB Flash, 1 MB SRAM
- Dual-bank Flash for read-while-write
- Advanced peripherals with DMA
- Ethernet MAC with IEEE 1588
- Crypto/hash processor
- FreeRTOS integration

Platform Capabilities
---------------------

Supported Peripherals
~~~~~~~~~~~~~~~~~~~~~

The STM32H7 platform supports the following peripherals:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Peripheral
     - Max Instances
     - Capabilities
   * - UART/USART
     - 8
     - Async/Sync TX/RX, DMA, FIFO, hardware flow control
   * - GPIO
     - 16 pins per port
     - Input/Output, alternate functions, interrupts
   * - SPI
     - 6
     - Master/Slave, DMA, NSS management, up to 150 Mbps
   * - I2C
     - 4
     - Master/Slave, DMA, Fast-mode Plus (1 MHz)
   * - Timer
     - 22
     - General-purpose, advanced, basic, low-power timers
   * - ADC
     - 3
     - 16-bit resolution, DMA, multi-channel, oversampling
   * - DAC
     - 2
     - 12-bit resolution, DMA, dual-channel
   * - CAN
     - 2
     - CAN FD, up to 8 Mbps
   * - USB
     - 2
     - USB OTG FS/HS
   * - Ethernet
     - 1
     - 10/100/1000 Mbps, IEEE 1588
   * - SDIO
     - 2
     - SD/MMC card interface
   * - QSPI
     - 1
     - Quad-SPI for external Flash/RAM
   * - RTC
     - 1
     - Calendar, alarm, tamper detection
   * - Watchdog
     - 2
     - Independent and window watchdog
   * - Crypto
     - 1
     - AES, DES, TDES, hash (SHA, MD5)

Platform Limitations
~~~~~~~~~~~~~~~~~~~~

- **Complex Clock Tree**: Requires careful clock configuration
- **Power Domains**: Multiple power domains need proper management
- **Cache Coherency**: DMA requires cache management
- **Pin Multiplexing**: Limited pins require careful planning
- **Temperature Range**: Standard range (-40°C to +85°C)
- **Cost**: Higher cost than STM32F4 series

Build Instructions
------------------

Prerequisites
~~~~~~~~~~~~~

- CMake 3.15 or higher
- ARM GCC toolchain (arm-none-eabi-gcc 10.3+)
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

   # Configure for STM32H7 platform
   CMake -B build -DPLATFORM=STM32 -DSTM32_CHIP=STM32H743 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake

   # Build
   CMake --build build

   # Generate binary
   arm-none-eabi-objcopy -O binary build/app.elf build/app.bin

Using Build Scripts
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Build for STM32H7
   python scripts/nexus.py build --platform STM32 --chip stm32h743

   # Build with specific configuration
   python scripts/nexus.py build --platform STM32 --chip stm32h743 \
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

   # STM32H7 family
   CONFIG_STM32H7=y

   # Specific variant
   CONFIG_STM32H743=y
   # CONFIG_STM32H750 is not set

   CONFIG_STM32_CHIP_NAME="STM32H743xx"

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
   CONFIG_STM32_UART_MAX_INSTANCES=8
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

   # Memory layout for STM32H743
   CONFIG_LINKER_RAM_START=0x24000000  # AXI SRAM
   CONFIG_LINKER_RAM_SIZE=0x00080000   # 512 KB
   CONFIG_LINKER_FLASH_START=0x08000000
   CONFIG_LINKER_FLASH_SIZE=0x00200000 # 2 MB

Hardware Setup
--------------

Development Boards
~~~~~~~~~~~~~~~~~~

**Recommended Boards:**

- NUCLEO-H743ZI
- STM32H743I-EVAL
- Custom boards with STM32H7 series

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

**Ethernet Pins (RMII):**

.. code-block:: text

   REF_CLK: PA1
   MDIO:    PA2
   MDC:     PC1
   CRS_DV:  PA7
   RXD0:    PC4
   RXD1:    PC5
   TX_EN:   PG11
   TXD0:    PG13
   TXD1:    PB13

Power Supply
~~~~~~~~~~~~

- **VDD**: 1.62V to 3.6V
- **VDDA**: Analog supply (same as VDD or separate)
- **VBAT**: Battery backup for RTC (optional)
- **VCAP**: External capacitors for internal regulator

**Recommended Power Setup:**

- Use 3.3V regulated supply
- Add decoupling capacitors (100nF) near each VDD pin
- Add bulk capacitor (10µF) near power input
- Add 2x 4.7µF capacitors on VCAP pins

Clock Configuration
~~~~~~~~~~~~~~~~~~~

**External Crystal:**

- HSE: 25 MHz (typical for NUCLEO boards)
- LSE: 32.768 kHz (for RTC)

**PLL Configuration for 480 MHz:**

.. code-block:: c

   /* HSE = 25 MHz */
   /* PLL1_M = 5, PLL1_N = 192, PLL1_P = 2, PLL1_Q = 4, PLL1_R = 2 */
   /* SYSCLK = 480 MHz */
   /* AHB = 240 MHz, APB1 = 120 MHz, APB2 = 120 MHz, APB3 = 120 MHz */

Cache Configuration
~~~~~~~~~~~~~~~~~~~

STM32H7 has instruction and data caches:

.. code-block:: c

   /* Enable I-Cache and D-Cache */
   SCB_EnableICache();
   SCB_EnableDCache();

**Important**: When using DMA, ensure cache coherency:

.. code-block:: c

   /* Clean D-Cache before DMA TX */
   SCB_CleanDCache_by_Addr((uint32_t*)buffer, size);

   /* Invalidate D-Cache after DMA RX */
   SCB_InvalidateDCache_by_Addr((uint32_t*)buffer, size);

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
       const uint8_t data[] = "Hello, STM32H7!";
       size_t len = sizeof(data) - 1;
       tx->transmit(tx, data, len);

       /* Start RTOS scheduler */
       nx_osal_start();

       /* Should never reach here */
       return 0;
   }

High-Speed SPI with DMA
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_spi.h"
   #include "osal/nx_osal.h"

   void spi_high_speed_example(void) {
       nx_spi_t* spi = nx_factory_spi(1);
       nx_lifecycle_t* lc = spi->get_lifecycle(spi);
       lc->init(lc);

       /* Configure for high-speed operation */
       /* SPI clock can reach 150 Mbps on STM32H7 */

       /* Prepare data in cache-aligned buffer */
       __attribute__((aligned(32))) uint8_t tx_data[256];
       __attribute__((aligned(32))) uint8_t rx_data[256];

       /* Clean cache before DMA TX */
       SCB_CleanDCache_by_Addr((uint32_t*)tx_data, sizeof(tx_data));

       /* Transfer with DMA */
       nx_transfer_async_t* transfer = spi->get_transfer_async(spi);
       size_t len = sizeof(tx_data);
       transfer->transfer(transfer, tx_data, rx_data, len);

       /* Wait for completion */
       nx_osal_delay_ms(10);

       /* Invalidate cache after DMA RX */
       SCB_InvalidateDCache_by_Addr((uint32_t*)rx_data, sizeof(rx_data));
   }

Ethernet Example
~~~~~~~~~~~~~~~~

.. code-block:: c

   #include "hal/nx_ethernet.h"

   void ethernet_example(void) {
       /* Note: Ethernet support requires additional configuration */
       /* This is a simplified example */

       nx_ethernet_t* eth = nx_factory_ethernet(0);
       nx_lifecycle_t* lc = eth->get_lifecycle(eth);
       lc->init(lc);

       /* Configure MAC address */
       uint8_t mac[6] = {0x00, 0x80, 0xE1, 0x00, 0x00, 0x00};
       eth->set_mac_address(eth, mac);

       /* Enable Ethernet */
       eth->enable(eth);

       /* Transmit packet */
       uint8_t packet[64];
       /* Fill packet data */
       eth->transmit(eth, packet, sizeof(packet));
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

   # For NUCLEO-H743ZI
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

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
               "name": "Debug STM32H7",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "openocd",
               "cwd": "${workspaceRoot}",
               "executable": "${workspaceRoot}/build/app.elf",
               "device": "STM32H743ZI",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32h7x.cfg"
               ],
               "svdFile": "${workspaceRoot}/STM32H743.svd"
           }
       ]
   }

Serial Console
~~~~~~~~~~~~~~

**Connect to UART:**

.. code-block:: bash

   # Linux
   screen /dev/ttyACM0 115200

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

**System Doesn't Start:**

Solution: Check power supply, verify VCAP capacitors, check boot pins.

**DMA Not Working:**

Solution: Ensure cache coherency (clean/invalidate), check buffer alignment, verify DMA configuration.

Cache Coherency Issues
~~~~~~~~~~~~~~~~~~~~~~

STM32H7 requires careful cache management:

1. Use cache-aligned buffers (32-byte alignment)
2. Clean D-Cache before DMA TX
3. Invalidate D-Cache after DMA RX
4. Consider using non-cacheable memory regions for DMA buffers

**Non-Cacheable Memory:**

.. code-block:: c

   /* Place DMA buffers in non-cacheable SRAM */
   __attribute__((section(".dma_buffer")))
   uint8_t dma_buffer[1024];

Clock Configuration Issues
~~~~~~~~~~~~~~~~~~~~~~~~~~

If system doesn't start or runs at wrong speed:

1. Verify HSE frequency matches hardware (usually 25 MHz)
2. Check PLL configuration
3. Ensure flash wait states are configured for clock speed (7 wait states for 480 MHz)
4. Verify voltage regulator scale setting (Scale 0 for 480 MHz)
5. Enable overdrive mode if required

Memory Issues
~~~~~~~~~~~~~

**Stack Overflow:**

Solution: Increase stack size in Kconfig (``CONFIG_OSAL_MAIN_STACK_SIZE``).

**Heap Exhausted:**

Solution: Increase heap size (``CONFIG_OSAL_HEAP_SIZE``) or use static allocation.

**Linker Error "region RAM overflowed":**

Solution: STM32H7 has multiple SRAM regions - use appropriate region or reduce memory usage.

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

- Enable compiler optimizations (``-O2`` or ``-O3``)
- Use DMA for data transfers
- Enable instruction and data caches
- Use FPU for floating-point operations
- Use ART Accelerator for Flash access
- Optimize interrupt priorities
- Use DTCM RAM for time-critical data
- Use ITCM RAM for time-critical code

See Also
--------

- :doc:`../user_guide/build_system` - Build system documentation
- :doc:`../user_guide/kconfig_platforms` - Platform configuration guide
- :doc:`../development/testing` - Testing guide
- :doc:`native` - Native platform guide
- :doc:`stm32f4` - STM32F4 platform guide
- :doc:`gd32` - GD32 platform guide
- `STM32H7 Reference Manual <https://www.st.com/resource/en/reference_manual/dm00176879.pdf>`_
- `STM32H7 Datasheet <https://www.st.com/resource/en/datasheet/stm32h743zi.pdf>`_
- `STM32H7 Programming Manual <https://www.st.com/resource/en/programming_manual/pm0253.pdf>`_
