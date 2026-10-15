Porting Guide
=============

Comprehensive guide for porting the Nexus Embedded Platform to new hardware platforms.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Porting Nexus to a new platform involves:

* **HAL Implementation**: Hardware abstraction layer for the target
* **OSAL Integration**: Operating system abstraction layer
* **Platform Configuration**: Build system and Kconfig setup
* **Testing**: Validation of the port
* **Documentation**: Platform-specific documentation

This guide walks through the complete porting process.

Prerequisites
-------------

Before Starting
~~~~~~~~~~~~~~~

**Hardware Requirements**

* Target development board
* Debug probe (J-Link, ST-Link, etc.)
* Serial console connection
* Power supply

**Software Requirements**

* Cross-compiler toolchain
* Debugger (GDB, OpenOCD, etc.)
* Flash programming tools
* Terminal emulator

**Knowledge Requirements**

* Target MCU architecture
* Peripheral specifications
* Memory map
* Clock configuration
* Interrupt system

Platform Architecture
---------------------

Nexus Platform Layers
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   ┌─────────────────────────────────────┐
   │        Applications                 │
   ├─────────────────────────────────────┤
   │        Framework                    │
   │  (Log, Shell, Config, etc.)         │
   ├─────────────────────────────────────┤
   │        OSAL (FreeRTOS/Native)       │
   ├─────────────────────────────────────┤
   │        HAL (Platform-Specific)      │
   ├─────────────────────────────────────┤
   │        Hardware                     │
   └─────────────────────────────────────┘

**HAL Layer**

Platform-specific hardware abstraction:

* GPIO, UART, SPI, I2C, ADC, etc.
* Clock configuration
* Interrupt management
* DMA support
* Power management

**OSAL Layer**

Operating system abstraction:

* Task management
* Synchronization primitives
* Timers
* Memory management
* Message queues

Porting Process
---------------

Step 1: Create Platform Directory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create directory structure for new platform:

.. code-block:: bash

   # Create platform directory
   mkdir -p platforms/myplatform

   # Create subdirectories
   mkdir -p platforms/myplatform/hal
   mkdir -p platforms/myplatform/startup
   mkdir -p platforms/myplatform/linker
   mkdir -p platforms/myplatform/config

Directory structure:

.. code-block:: text

   platforms/myplatform/
   ├── hal/                    # HAL implementation
   │   ├── hal_gpio.c
   │   ├── hal_uart.c
   │   ├── hal_spi.c
   │   └── ...
   ├── startup/                # Startup code
   │   ├── startup.c
   │   └── system_init.c
   ├── linker/                 # Linker scripts
   │   └── myplatform.ld
   ├── config/                 # Configuration
   │   └── platform_config.h
   ├── CMakeLists.txt          # Build configuration
   └── Kconfig                 # Kconfig options

Step 2: Implement HAL
~~~~~~~~~~~~~~~~~~~~~

**GPIO Implementation**

Create `platforms/myplatform/hal/hal_gpio.c`:

.. code-block:: c

   /**
    * \file            hal_gpio.c
    * \brief           GPIO HAL implementation for MyPlatform
    * \author          Nexus Team
    */

   #include "hal/hal_gpio.h"
   #include "myplatform_gpio.h"  /* Platform-specific header */

   /*---------------------------------------------------------------------------*/
   /* Private Functions                                                         */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Validate GPIO parameters
    */
   static hal_status_t gpio_validate_params(hal_gpio_port_t port, uint8_t pin) {
       if (port >= HAL_GPIO_PORT_MAX) {
           return HAL_ERROR_PARAM;
       }
       if (pin >= HAL_GPIO_PIN_MAX) {
           return HAL_ERROR_PARAM;
       }
       return HAL_OK;
   }

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize GPIO pin
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config) {
       hal_status_t status;

       /* Validate parameters */
       status = gpio_validate_params(port, pin);
       if (status != HAL_OK) {
           return status;
       }

       if (config == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Enable GPIO clock */
       platform_gpio_enable_clock(port);

       /* Configure GPIO mode */
       platform_gpio_set_mode(port, pin, config->mode);

       /* Configure pull-up/down */
       platform_gpio_set_pull(port, pin, config->pull);

       /* Configure speed */
       platform_gpio_set_speed(port, pin, config->speed);

       /* Set initial level */
       if (config->mode == HAL_GPIO_MODE_OUTPUT_PP ||
           config->mode == HAL_GPIO_MODE_OUTPUT_OD) {
           platform_gpio_write(port, pin, config->init_level);
       }

       return HAL_OK;
   }

   /**
    * \brief           Write to GPIO pin
    */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level) {
       hal_status_t status;

       /* Validate parameters */
       status = gpio_validate_params(port, pin);
       if (status != HAL_OK) {
           return status;
       }

       /* Write to GPIO */
       platform_gpio_write(port, pin, level);

       return HAL_OK;
   }

   /**
    * \brief           Read from GPIO pin
    */
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin,
                             hal_gpio_level_t* level) {
       hal_status_t status;

       /* Validate parameters */
       status = gpio_validate_params(port, pin);
       if (status != HAL_OK) {
           return status;
       }

       if (level == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Read from GPIO */
       *level = platform_gpio_read(port, pin);

       return HAL_OK;
   }

**UART Implementation**

Create `platforms/myplatform/hal/hal_uart.c`:

.. code-block:: c

   /**
    * \file            hal_uart.c
    * \brief           UART HAL implementation for MyPlatform
    * \author          Nexus Team
    */

   #include "hal/hal_uart.h"
   #include "myplatform_uart.h"

   /*---------------------------------------------------------------------------*/
   /* Private Data                                                              */
   /*---------------------------------------------------------------------------*/

   static struct {
       bool initialized;
       hal_uart_callback_t callback;
       void* user_data;
   } uart_state[HAL_UART_MAX];

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize UART
    */
   hal_status_t hal_uart_init(hal_uart_id_t id,
                             const hal_uart_config_t* config) {
       if (id >= HAL_UART_MAX) {
           return HAL_ERROR_PARAM;
       }

       if (config == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Enable UART clock */
       platform_uart_enable_clock(id);

       /* Configure UART */
       platform_uart_set_baudrate(id, config->baudrate);
       platform_uart_set_wordlen(id, config->wordlen);
       platform_uart_set_stopbits(id, config->stopbits);
       platform_uart_set_parity(id, config->parity);

       /* Enable UART */
       platform_uart_enable(id);

       uart_state[id].initialized = true;

       return HAL_OK;
   }

   /**
    * \brief           Send data via UART
    */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                             size_t length, uint32_t timeout) {
       if (id >= HAL_UART_MAX) {
           return HAL_ERROR_PARAM;
       }

       if (!uart_state[id].initialized) {
           return HAL_ERROR_STATE;
       }

       if (data == NULL || length == 0) {
           return HAL_ERROR_PARAM;
       }

       /* Send data */
       for (size_t i = 0; i < length; i++) {
           if (!platform_uart_send_byte(id, data[i], timeout)) {
               return HAL_ERROR_TIMEOUT;
           }
       }

       return HAL_OK;
   }



Step 3: Startup Code
~~~~~~~~~~~~~~~~~~~~

Create `platforms/myplatform/startup/startup.c`:

.. code-block:: c

   /**
    * \file            startup.c
    * \brief           Startup code for MyPlatform
    * \author          Nexus Team
    */

   #include <stdint.h>

   /*---------------------------------------------------------------------------*/
   /* External Symbols                                                          */
   /*---------------------------------------------------------------------------*/

   extern uint32_t _estack;
   extern uint32_t _sdata;
   extern uint32_t _edata;
   extern uint32_t _sidata;
   extern uint32_t _sbss;
   extern uint32_t _ebss;

   extern int main(void);

   /*---------------------------------------------------------------------------*/
   /* Function Prototypes                                                       */
   /*---------------------------------------------------------------------------*/

   void Reset_Handler(void);
   void Default_Handler(void);

   /*---------------------------------------------------------------------------*/
   /* Vector Table                                                              */
   /*---------------------------------------------------------------------------*/

   __attribute__((section(".isr_vector")))
   const void* vector_table[] = {
       &_estack,                   /* Initial stack pointer */
       Reset_Handler,              /* Reset handler */
       /* Add other interrupt handlers */
   };

   /*---------------------------------------------------------------------------*/
   /* Reset Handler                                                             */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Reset handler
    */
   void Reset_Handler(void) {
       uint32_t* src;
       uint32_t* dst;

       /* Copy data section from flash to RAM */
       src = &_sidata;
       dst = &_sdata;
       while (dst < &_edata) {
           *dst++ = *src++;
       }

       /* Zero-initialize BSS section */
       dst = &_sbss;
       while (dst < &_ebss) {
           *dst++ = 0;
       }

       /* Call system initialization */
       SystemInit();

       /* Call main */
       main();

       /* Infinite loop if main returns */
       while (1);
   }

   /**
    * \brief           Default interrupt handler
    */
   void Default_Handler(void) {
       while (1);
   }

Step 4: Linker Script
~~~~~~~~~~~~~~~~~~~~~

Create `platforms/myplatform/linker/myplatform.ld`:

.. code-block:: text

   /* Memory layout */
   MEMORY
   {
       FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
       RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
   }

   /* Stack size */
   _stack_size = 0x2000;

   /* Entry point */
   ENTRY(Reset_Handler)

   SECTIONS
   {
       /* Vector table */
       .isr_vector :
       {
           . = ALIGN(4);
           KEEP(*(.isr_vector))
           . = ALIGN(4);
       } >FLASH

       /* Code section */
       .text :
       {
           . = ALIGN(4);
           *(.text)
           *(.text*)
           *(.rodata)
           *(.rodata*)
           . = ALIGN(4);
       } >FLASH

       /* Data section */
       .data :
       {
           . = ALIGN(4);
           _sdata = .;
           *(.data)
           *(.data*)
           . = ALIGN(4);
           _edata = .;
       } >RAM AT>FLASH

       _sidata = LOADADDR(.data);

       /* BSS section */
       .bss :
       {
           . = ALIGN(4);
           _sbss = .;
           *(.bss)
           *(.bss*)
           *(COMMON)
           . = ALIGN(4);
           _ebss = .;
       } >RAM

       /* Stack */
       .stack :
       {
           . = ALIGN(8);
           . = . + _stack_size;
           . = ALIGN(8);
           _estack = .;
       } >RAM
   }

Step 5: CMake Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create `platforms/myplatform/CMakeLists.txt`:

.. code-block:: cmake

   # Platform: MyPlatform

   # Set platform name
   set(PLATFORM_NAME "myplatform")

   # Toolchain
   set(CMAKE_SYSTEM_NAME Generic)
   set(CMAKE_SYSTEM_PROCESSOR arm)

   # Compiler
   set(CMAKE_C_COMPILER arm-none-eabi-gcc)
   set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
   set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

   # Compiler flags
   set(PLATFORM_C_FLAGS
       -mcpu=cortex-m4
       -mthumb
       -mfloat-abi=hard
       -mfpu=fpv4-sp-d16
   )

   # Linker flags
   set(PLATFORM_LINKER_FLAGS
       -T${CMAKE_CURRENT_SOURCE_DIR}/linker/myplatform.ld
       -Wl,--gc-sections
       -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
   )

   # HAL sources
   set(PLATFORM_HAL_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/hal/hal_gpio.c
       ${CMAKE_CURRENT_SOURCE_DIR}/hal/hal_uart.c
       ${CMAKE_CURRENT_SOURCE_DIR}/hal/hal_spi.c
       ${CMAKE_CURRENT_SOURCE_DIR}/hal/hal_i2c.c
   )

   # Startup sources
   set(PLATFORM_STARTUP_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/startup/startup.c
       ${CMAKE_CURRENT_SOURCE_DIR}/startup/system_init.c
   )

   # Include directories
   set(PLATFORM_INCLUDE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}/config
   )

   # Create platform library
   add_library(platform_hal STATIC
       ${PLATFORM_HAL_SOURCES}
       ${PLATFORM_STARTUP_SOURCES}
   )

   target_include_directories(platform_hal PUBLIC
       ${PLATFORM_INCLUDE_DIRS}
   )

   target_compile_options(platform_hal PRIVATE
       ${PLATFORM_C_FLAGS}
   )

Step 6: Kconfig Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create `platforms/myplatform/Kconfig`:

.. code-block:: kconfig

   # MyPlatform Configuration

   config PLATFORM_MYPLATFORM
       bool "MyPlatform Support"
       default n
       help
         Enable support for MyPlatform

   if PLATFORM_MYPLATFORM

   config MYPLATFORM_CPU_FREQ
       int "CPU Frequency (Hz)"
       default 168000000
       help
         CPU frequency in Hz

   config MYPLATFORM_GPIO_PORTS
       int "Number of GPIO Ports"
       default 11
       help
         Number of GPIO ports available

   config MYPLATFORM_UART_COUNT
       int "Number of UART Instances"
       default 6
       help
         Number of UART instances available

   endif # PLATFORM_MYPLATFORM

Step 7: Testing
~~~~~~~~~~~~~~~

**Unit Tests**

Create platform-specific tests:

.. code-block:: c

   /* tests/platform/myplatform/test_gpio.c */

   #include "gtest/gtest.h"
   #include "hal/hal_gpio.h"

   TEST(MyPlatform_GPIO, Init) {
       hal_gpio_config_t config = {
           .mode = HAL_GPIO_MODE_OUTPUT_PP,
           .pull = HAL_GPIO_PULL_NONE,
           .speed = HAL_GPIO_SPEED_LOW,
           .init_level = HAL_GPIO_LEVEL_LOW
       };

       EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 5, &config));
   }

   TEST(MyPlatform_GPIO, Write) {
       EXPECT_EQ(HAL_OK, hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH));
   }

**Hardware Tests**

Create hardware validation tests:

.. code-block:: c

   /* tests/hardware/myplatform/test_gpio_hw.c */

   void test_gpio_loopback(void) {
       /* Configure PA5 as output */
       hal_gpio_config_t out_config = {
           .mode = HAL_GPIO_MODE_OUTPUT_PP,
           .pull = HAL_GPIO_PULL_NONE,
           .speed = HAL_GPIO_SPEED_LOW,
           .init_level = HAL_GPIO_LEVEL_LOW
       };
       hal_gpio_init(HAL_GPIO_PORT_A, 5, &out_config);

       /* Configure PA6 as input */
       hal_gpio_config_t in_config = {
           .mode = HAL_GPIO_MODE_INPUT,
           .pull = HAL_GPIO_PULL_NONE,
       };
       hal_gpio_init(HAL_GPIO_PORT_A, 6, &in_config);

       /* Test loopback (PA5 -> PA6) */
       hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);
       hal_gpio_level_t level;
       hal_gpio_read(HAL_GPIO_PORT_A, 6, &level);
       assert(level == HAL_GPIO_LEVEL_HIGH);
   }

Platform-Specific Features
--------------------------

Clock Configuration
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /**
    * \brief           Initialize system clocks
    */
   void platform_clock_init(void) {
       /* Enable HSE (external oscillator) */
       RCC->CR |= RCC_CR_HSEON;
       while (!(RCC->CR & RCC_CR_HSERDY));

       /* Configure PLL */
       RCC->PLLCFGR = (
           RCC_PLLCFGR_PLLSRC_HSE |
           (8 << RCC_PLLCFGR_PLLM_Pos) |
           (336 << RCC_PLLCFGR_PLLN_Pos) |
           (0 << RCC_PLLCFGR_PLLP_Pos) |
           (7 << RCC_PLLCFGR_PLLQ_Pos)
       );

       /* Enable PLL */
       RCC->CR |= RCC_CR_PLLON;
       while (!(RCC->CR & RCC_CR_PLLRDY));

       /* Configure flash latency */
       FLASH->ACR = FLASH_ACR_LATENCY_5WS;

       /* Switch to PLL */
       RCC->CFGR |= RCC_CFGR_SW_PLL;
       while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
   }

Interrupt Management
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /**
    * \brief           Enable interrupt
    */
   void platform_irq_enable(IRQn_Type irq) {
       NVIC_EnableIRQ(irq);
   }

   /**
    * \brief           Disable interrupt
    */
   void platform_irq_disable(IRQn_Type irq) {
       NVIC_DisableIRQ(irq);
   }

   /**
    * \brief           Set interrupt priority
    */
   void platform_irq_set_priority(IRQn_Type irq, uint32_t priority) {
       NVIC_SetPriority(irq, priority);
   }

DMA Support
~~~~~~~~~~~

.. code-block:: c

   /**
    * \brief           Configure DMA for UART TX
    */
   void platform_uart_dma_tx_init(hal_uart_id_t id, const uint8_t* data,
                                  size_t length) {
       DMA_Stream_TypeDef* stream = get_uart_tx_stream(id);

       /* Disable stream */
       stream->CR &= ~DMA_SxCR_EN;
       while (stream->CR & DMA_SxCR_EN);

       /* Configure stream */
       stream->PAR = (uint32_t)&UART_INSTANCES[id]->DR;
       stream->M0AR = (uint32_t)data;
       stream->NDTR = length;
       stream->CR = (
           DMA_SxCR_CHSEL_4 |
           DMA_SxCR_MINC |
           DMA_SxCR_DIR_0 |
           DMA_SxCR_TCIE
       );

       /* Enable stream */
       stream->CR |= DMA_SxCR_EN;
   }

Best Practices
--------------

Code Organization
~~~~~~~~~~~~~~~~~

* Keep platform-specific code isolated
* Use consistent naming conventions
* Document hardware dependencies
* Provide clear error messages
* Handle all error conditions

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

* Use DMA for bulk transfers
* Optimize interrupt handlers
* Minimize critical sections
* Use hardware features efficiently
* Profile and measure performance

Testing Strategy
~~~~~~~~~~~~~~~~

* Unit test all HAL functions
* Property-based testing for HAL
* Hardware validation tests
* Stress testing
* Power consumption testing

Documentation
~~~~~~~~~~~~~

* Document hardware requirements
* Provide pin mapping tables
* Document clock configuration
* List known limitations
* Provide usage examples

Common Issues
-------------

Clock Configuration
~~~~~~~~~~~~~~~~~~~

**Issue**: System doesn't start

**Solutions**:
* Verify oscillator frequency
* Check PLL configuration
* Verify flash wait states
* Check power supply voltage

Memory Layout
~~~~~~~~~~~~~

**Issue**: Hard fault on startup

**Solutions**:
* Verify linker script
* Check stack size
* Verify memory regions
* Check vector table alignment

Interrupt Priorities
~~~~~~~~~~~~~~~~~~~~

**Issue**: Interrupt not firing

**Solutions**:
* Enable interrupt in NVIC
* Set appropriate priority
* Clear pending flags
* Verify interrupt handler name

See Also
--------

* :doc:`architecture_design` - System architecture
* :doc:`api_design_guidelines` - API design principles
* :doc:`testing` - Testing guidelines
* :doc:`debugging_guide` - Debugging techniques

Summary
-------

Porting Nexus to a new platform involves:

1. Create platform directory structure
2. Implement HAL for all peripherals
3. Create startup code and linker script
4. Configure CMake build system
5. Add Kconfig options
6. Write comprehensive tests
7. Document platform-specific features

Following this guide ensures a complete, tested, and maintainable platform port.
