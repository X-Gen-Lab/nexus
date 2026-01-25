Vendor SDK Integration
======================

Guide to integrating vendor SDKs and libraries with the Nexus platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Nexus integrates with various vendor SDKs to provide hardware-specific functionality while maintaining a unified API.

**Supported Vendors:**

* STMicroelectronics (STM32)
* GigaDevice (GD32)
* Espressif (ESP32)
* Nordic Semiconductor (nRF52)
* ARM (CMSIS)

Vendor Directory Structure
---------------------------

Organization
~~~~~~~~~~~~

.. code-block:: text

   vendors/
   ├── st/                      # STMicroelectronics
   │   ├── STM32F4xx_HAL_Driver/
   │   ├── STM32H7xx_HAL_Driver/
   │   ├── CMSIS/
   │   └── README.md
   ├── gd/                      # GigaDevice
   │   ├── GD32VF103_standard_peripheral/
   │   └── README.md
   ├── espressif/               # Espressif
   │   ├── esp-idf/
   │   └── README.md
   ├── nordic/                  # Nordic
   │   ├── nRF5_SDK/
   │   └── README.md
   ├── arm/                     # ARM
   │   ├── CMSIS/
   │   └── README.md
   └── README.md

Version Management
~~~~~~~~~~~~~~~~~~

**Track Vendor SDK Versions:**

.. code-block:: text

   vendors/VERSIONS.md:

   # Vendor SDK Versions

   ## STMicroelectronics
   - STM32F4 HAL: v1.27.1
   - STM32H7 HAL: v1.10.0
   - CMSIS: v5.9.0

   ## GigaDevice
   - GD32VF103: v1.1.0

   ## Espressif
   - ESP-IDF: v4.4.2

   ## Nordic
   - nRF5 SDK: v17.1.0

   ## ARM
   - CMSIS: v5.9.0

STMicroelectronics Integration
-------------------------------

STM32 HAL Driver
~~~~~~~~~~~~~~~~

**Directory Structure:**

.. code-block:: text

   vendors/st/
   ├── STM32F4xx_HAL_Driver/
   │   ├── Inc/
   │   │   ├── stm32f4xx_hal.h
   │   │   ├── stm32f4xx_hal_gpio.h
   │   │   ├── stm32f4xx_hal_uart.h
   │   │   └── ...
   │   └── Src/
   │       ├── stm32f4xx_hal.c
   │       ├── stm32f4xx_hal_gpio.c
   │       ├── stm32f4xx_hal_uart.c
   │       └── ...
   └── CMSIS/
       ├── Device/ST/STM32F4xx/
       │   ├── Include/
       │   │   ├── stm32f4xx.h
       │   │   └── system_stm32f4xx.h
       │   └── Source/
       │       └── system_stm32f4xx.c
       └── Include/
           ├── core_cm4.h
           └── cmsis_gcc.h

**CMake Integration:**

.. code-block:: cmake

   # platforms/stm32/CMakeLists.txt

   if(CONFIG_PLATFORM_STM32F4)
       set(STM32_FAMILY "F4")
       set(STM32_HAL_DIR "${CMAKE_SOURCE_DIR}/vendors/st/STM32F4xx_HAL_Driver")
       set(STM32_CMSIS_DIR "${CMAKE_SOURCE_DIR}/vendors/st/CMSIS")

       # HAL sources
       file(GLOB STM32_HAL_SOURCES
           "${STM32_HAL_DIR}/Src/stm32f4xx_hal.c"
           "${STM32_HAL_DIR}/Src/stm32f4xx_hal_gpio.c"
           "${STM32_HAL_DIR}/Src/stm32f4xx_hal_uart.c"
           "${STM32_HAL_DIR}/Src/stm32f4xx_hal_spi.c"
           "${STM32_HAL_DIR}/Src/stm32f4xx_hal_i2c.c"
       )

       # Include directories
       target_include_directories(nexus_platform PUBLIC
           ${STM32_HAL_DIR}/Inc
           ${STM32_CMSIS_DIR}/Device/ST/STM32F4xx/Include
           ${STM32_CMSIS_DIR}/Include
       )

       # Add HAL sources
       target_sources(nexus_platform PRIVATE ${STM32_HAL_SOURCES})

       # Compiler definitions
       target_compile_definitions(nexus_platform PUBLIC
           STM32F407xx
           USE_HAL_DRIVER
       )
   endif()

**Wrapper Implementation:**

.. code-block:: c

   /**
    * \file            stm32_gpio_adapter.c
    * \brief           STM32 GPIO HAL adapter
    * \author          Nexus Team
    */

   #include "hal/nx_gpio.h"
   #include "stm32f4xx_hal.h"

   /*---------------------------------------------------------------------------*/
   /* Private Functions                                                         */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Convert Nexus GPIO mode to STM32 HAL mode
    */
   static uint32_t convert_gpio_mode(nx_gpio_mode_t mode)
   {
       switch (mode) {
       case NX_GPIO_MODE_INPUT:
           return GPIO_MODE_INPUT;
       case NX_GPIO_MODE_OUTPUT_PP:
           return GPIO_MODE_OUTPUT_PP;
       case NX_GPIO_MODE_OUTPUT_OD:
           return GPIO_MODE_OUTPUT_OD;
       case NX_GPIO_MODE_AF_PP:
           return GPIO_MODE_AF_PP;
       case NX_GPIO_MODE_AF_OD:
           return GPIO_MODE_AF_OD;
       case NX_GPIO_MODE_ANALOG:
           return GPIO_MODE_ANALOG;
       default:
           return GPIO_MODE_INPUT;
       }
   }

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize GPIO pin
    */
   nx_status_t stm32_gpio_init(char port, uint8_t pin,
                                const nx_gpio_config_t* config)
   {
       GPIO_TypeDef* gpio_port;
       GPIO_InitTypeDef gpio_init;

       /* Get GPIO port */
       switch (port) {
       case 'A': gpio_port = GPIOA; __HAL_RCC_GPIOA_CLK_ENABLE(); break;
       case 'B': gpio_port = GPIOB; __HAL_RCC_GPIOB_CLK_ENABLE(); break;
       case 'C': gpio_port = GPIOC; __HAL_RCC_GPIOC_CLK_ENABLE(); break;
       case 'D': gpio_port = GPIOD; __HAL_RCC_GPIOD_CLK_ENABLE(); break;
       default: return NX_ERR_PARAM;
       }

       /* Configure GPIO */
       gpio_init.Pin = (1 << pin);
       gpio_init.Mode = convert_gpio_mode(config->mode);
       gpio_init.Pull = config->pull;
       gpio_init.Speed = config->speed;

       HAL_GPIO_Init(gpio_port, &gpio_init);

       return NX_OK;
   }

CMSIS Integration
~~~~~~~~~~~~~~~~~

**Core Files:**

.. code-block:: c

   /* Include CMSIS core */
   #include "core_cm4.h"

   /* System initialization */
   void SystemInit(void)
   {
       /* FPU settings */
       #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
       SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));
       #endif

       /* Configure Flash prefetch, Instruction cache, Data cache */
       #if (INSTRUCTION_CACHE_ENABLE != 0U)
       __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
       #endif

       #if (DATA_CACHE_ENABLE != 0U)
       __HAL_FLASH_DATA_CACHE_ENABLE();
       #endif

       /* Set interrupt priorities */
       NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
   }

GigaDevice Integration
----------------------

GD32 Standard Peripheral Library
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Directory Structure:**

.. code-block:: text

   vendors/gd/
   └── GD32VF103_standard_peripheral/
       ├── Include/
       │   ├── gd32vf103.h
       │   ├── gd32vf103_gpio.h
       │   └── ...
       └── Source/
           ├── gd32vf103_gpio.c
           └── ...

**CMake Integration:**

.. code-block:: cmake

   # platforms/gd32/CMakeLists.txt

   if(CONFIG_PLATFORM_GD32)
       set(GD32_SPL_DIR "${CMAKE_SOURCE_DIR}/vendors/gd/GD32VF103_standard_peripheral")

       # SPL sources
       file(GLOB GD32_SPL_SOURCES
           "${GD32_SPL_DIR}/Source/gd32vf103_gpio.c"
           "${GD32_SPL_DIR}/Source/gd32vf103_usart.c"
           "${GD32_SPL_DIR}/Source/gd32vf103_spi.c"
       )

       # Include directories
       target_include_directories(nexus_platform PUBLIC
           ${GD32_SPL_DIR}/Include
       )

       # Add SPL sources
       target_sources(nexus_platform PRIVATE ${GD32_SPL_SOURCES})

       # Compiler definitions
       target_compile_definitions(nexus_platform PUBLIC
           GD32VF103
           USE_STDPERIPH_DRIVER
       )
   endif()

Espressif Integration
---------------------

ESP-IDF Framework
~~~~~~~~~~~~~~~~~

**Directory Structure:**

.. code-block:: text

   vendors/espressif/
   └── esp-idf/
       ├── components/
       │   ├── driver/
       │   ├── esp_wifi/
       │   ├── esp_system/
       │   └── ...
       └── CMakeLists.txt

**CMake Integration:**

.. code-block:: cmake

   # platforms/esp32/CMakeLists.txt

   if(CONFIG_PLATFORM_ESP32)
       set(IDF_PATH "${CMAKE_SOURCE_DIR}/vendors/espressif/esp-idf")

       # Include ESP-IDF build system
       include($ENV{IDF_PATH}/tools/cmake/project.cmake)

       # ESP-IDF components
       idf_component_register(
           SRCS "esp32_gpio.c" "esp32_uart.c"
           INCLUDE_DIRS "include"
           REQUIRES driver esp_wifi nvs_flash
       )
   endif()

**Wrapper Implementation:**

.. code-block:: c

   /**
    * \file            esp32_gpio_adapter.c
    * \brief           ESP32 GPIO adapter
    * \author          Nexus Team
    */

   #include "hal/nx_gpio.h"
   #include "driver/gpio.h"

   /**
    * \brief           Initialize GPIO pin
    */
   nx_status_t esp32_gpio_init(uint8_t pin, const nx_gpio_config_t* config)
   {
       gpio_config_t io_conf = {
           .pin_bit_mask = (1ULL << pin),
           .mode = (config->mode == NX_GPIO_MODE_OUTPUT_PP) ?
                   GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
           .pull_up_en = (config->pull == NX_GPIO_PULL_UP) ?
                         GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
           .pull_down_en = (config->pull == NX_GPIO_PULL_DOWN) ?
                           GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
           .intr_type = GPIO_INTR_DISABLE,
       };

       esp_err_t err = gpio_config(&io_conf);
       return (err == ESP_OK) ? NX_OK : NX_ERR_FAIL;
   }

Nordic Integration
------------------

nRF5 SDK
~~~~~~~~

**Directory Structure:**

.. code-block:: text

   vendors/nordic/
   └── nRF5_SDK/
       ├── components/
       │   ├── drivers_nrf/
       │   ├── libraries/
       │   └── softdevice/
       └── modules/
           └── nrfx/

**CMake Integration:**

.. code-block:: cmake

   # platforms/nrf52/CMakeLists.txt

   if(CONFIG_PLATFORM_NRF52)
       set(NRF5_SDK_DIR "${CMAKE_SOURCE_DIR}/vendors/nordic/nRF5_SDK")

       # nRFx driver sources
       file(GLOB NRF5_SOURCES
           "${NRF5_SDK_DIR}/modules/nrfx/drivers/src/nrfx_gpiote.c"
           "${NRF5_SDK_DIR}/modules/nrfx/drivers/src/nrfx_uart.c"
           "${NRF5_SDK_DIR}/modules/nrfx/drivers/src/nrfx_spi.c"
       )

       # Include directories
       target_include_directories(nexus_platform PUBLIC
           ${NRF5_SDK_DIR}/modules/nrfx
           ${NRF5_SDK_DIR}/modules/nrfx/hal
           ${NRF5_SDK_DIR}/modules/nrfx/drivers/include
       )

       # Add sources
       target_sources(nexus_platform PRIVATE ${NRF5_SOURCES})

       # Compiler definitions
       target_compile_definitions(nexus_platform PUBLIC
           NRF52840_XXAA
           NRFX_GPIOTE_ENABLED=1
           NRFX_UART_ENABLED=1
       )
   endif()

ARM CMSIS Integration
----------------------

CMSIS Core
~~~~~~~~~~

**Directory Structure:**

.. code-block:: text

   vendors/arm/
   └── CMSIS/
       ├── Core/
       │   └── Include/
       │       ├── cmsis_compiler.h
       │       ├── cmsis_gcc.h
       │       ├── core_cm4.h
       │       └── core_cm7.h
       └── DSP/
           ├── Include/
           └── Source/

**CMake Integration:**

.. code-block:: cmake

   # CMakeLists.txt

   set(CMSIS_DIR "${CMAKE_SOURCE_DIR}/vendors/arm/CMSIS")

   # CMSIS Core
   target_include_directories(nexus_platform PUBLIC
       ${CMSIS_DIR}/Core/Include
   )

   # CMSIS DSP (optional)
   if(CONFIG_USE_CMSIS_DSP)
       target_include_directories(nexus_platform PUBLIC
           ${CMSIS_DIR}/DSP/Include
       )

       file(GLOB CMSIS_DSP_SOURCES
           "${CMSIS_DIR}/DSP/Source/BasicMathFunctions/*.c"
           "${CMSIS_DIR}/DSP/Source/FastMathFunctions/*.c"
       )

       target_sources(nexus_platform PRIVATE ${CMSIS_DSP_SOURCES})
   endif()

Vendor SDK Configuration
-------------------------

HAL Configuration Files
~~~~~~~~~~~~~~~~~~~~~~~

**STM32 HAL Configuration:**

.. code-block:: c

   /**
    * \file            stm32f4xx_hal_conf.h
    * \brief           STM32F4 HAL configuration
    * \author          Nexus Team
    */

   #ifndef STM32F4XX_HAL_CONF_H
   #define STM32F4XX_HAL_CONF_H

   /* Module selection */
   #define HAL_MODULE_ENABLED
   #define HAL_GPIO_MODULE_ENABLED
   #define HAL_UART_MODULE_ENABLED
   #define HAL_SPI_MODULE_ENABLED
   #define HAL_I2C_MODULE_ENABLED

   /* Oscillator values */
   #define HSE_VALUE    8000000U
   #define HSI_VALUE    16000000U

   /* System configuration */
   #define VDD_VALUE    3300U
   #define TICK_INT_PRIORITY    0U
   #define USE_RTOS     0U

   /* Assert configuration */
   #ifdef DEBUG
   #define USE_FULL_ASSERT    1U
   #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
   void assert_failed(uint8_t* file, uint32_t line);
   #else
   #define assert_param(expr) ((void)0U)
   #endif

   #endif /* STM32F4XX_HAL_CONF_H */

Vendor SDK Updates
------------------

Updating Vendor SDKs
~~~~~~~~~~~~~~~~~~~~

**Update Process:**

1. **Backup Current Version:**

.. code-block:: bash

   cd vendors/st
   git tag backup-$(date +%Y%m%d)
   git commit -am "Backup before update"

2. **Download New Version:**

.. code-block:: bash

   # Download from vendor website
   wget https://vendor.com/sdk/latest.zip
   unzip latest.zip -d temp/

3. **Update Files:**

.. code-block:: bash

   # Copy new files
   cp -r temp/STM32F4xx_HAL_Driver/* STM32F4xx_HAL_Driver/

4. **Test Integration:**

.. code-block:: bash

   # Build and test
   cmake -B build
   cmake --build build
   ctest --test-dir build

5. **Update Version File:**

.. code-block:: text

   # vendors/VERSIONS.md
   ## STMicroelectronics
   - STM32F4 HAL: v1.28.0 (updated 2026-01-25)

Version Compatibility
~~~~~~~~~~~~~~~~~~~~~

**Compatibility Matrix:**

.. code-block:: text

   Nexus Version | STM32 HAL | ESP-IDF | nRF5 SDK
   --------------|-----------|---------|----------
   0.1.0         | 1.27.1    | 4.4.2   | 17.1.0
   0.2.0         | 1.28.0    | 4.4.3   | 17.1.0
   1.0.0         | 1.29.0    | 5.0.0   | 17.2.0

Best Practices
--------------

1. **Isolate Vendor Code**
   * Keep vendor SDKs in separate directory
   * Don't modify vendor files directly
   * Use wrapper/adapter pattern

2. **Version Control**
   * Track vendor SDK versions
   * Document compatibility
   * Test after updates

3. **Minimize Dependencies**
   * Only include needed components
   * Avoid vendor-specific APIs in application
   * Use Nexus abstractions

4. **Configuration Management**
   * Use Kconfig for vendor options
   * Provide sensible defaults
   * Document configuration

5. **Testing**
   * Test vendor integration
   * Verify compatibility
   * Automate testing

6. **Documentation**
   * Document vendor requirements
   * Provide integration guide
   * Update version information

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Issue: Vendor SDK not found**

.. code-block:: bash

   # Check vendor directory exists
   ls -la vendors/st/STM32F4xx_HAL_Driver

   # Update submodules if using git
   git submodule update --init --recursive

**Issue: Compilation errors**

.. code-block:: bash

   # Check include paths
   cmake -B build -DCMAKE_VERBOSE_MAKEFILE=ON

   # Verify vendor SDK version
   cat vendors/VERSIONS.md

**Issue: Linker errors**

.. code-block:: bash

   # Check library paths
   # Verify all required sources are included
   # Check linker script

See Also
--------

* :doc:`../platform_guides/index` - Platform-Specific Guides
* :doc:`porting` - Porting Guide
* :doc:`build_system` - Build System
* :doc:`../development/contributing` - Contributing Guide

