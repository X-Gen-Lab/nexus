First Application
=================

This guide will walk you through creating your first custom Nexus application from scratch.

.. contents:: Table of Contents
   :local:
   :depth: 2

What We'll Build
----------------

We'll create a simple temperature monitoring application that:

* Reads temperature from an ADC channel
* Displays temperature on LEDs (color-coded)
* Logs temperature readings
* Provides a shell command to query temperature

This application demonstrates:

* HAL usage (GPIO, ADC, UART)
* Framework usage (Log, Shell)
* OSAL usage (Tasks, Delays)
* Configuration management

Prerequisites
-------------

Before starting, ensure you have:

* Completed :doc:`quick_start`
* Built and run the blinky example
* Familiar with :doc:`project_structure`
* Development board (STM32F4 Discovery) or native platform

Project Setup
-------------

Create Application Directory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Navigate to applications directory
   cd nexus/applications

   # Create new application
   mkdir temp_monitor
   cd temp_monitor

Create CMakeLists.txt
~~~~~~~~~~~~~~~~~~~~~~

Create ``applications/temp_monitor/CMakeLists.txt``:

.. code-block:: cmake

   ##############################################################################
   # Temperature Monitor Application
   ##############################################################################

   # Create executable
   add_executable(temp_monitor
       main.c
   )

   # Link libraries
   target_link_libraries(temp_monitor PRIVATE
       nexus_hal
       nexus_osal
       nexus_log
       nexus_shell
       nexus_platform
   )

   # Include directories
   target_include_directories(temp_monitor PRIVATE
       ${CMAKE_SOURCE_DIR}/hal/include
       ${CMAKE_SOURCE_DIR}/osal/include
       ${CMAKE_SOURCE_DIR}/framework/log/include
       ${CMAKE_SOURCE_DIR}/framework/shell/include
       ${CMAKE_SOURCE_DIR}
   )

   # Set output directory
   set_target_properties(temp_monitor PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/applications/temp_monitor
   )

Update Parent CMakeLists.txt
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Edit ``applications/CMakeLists.txt`` to include your application:

.. code-block:: cmake

   # Existing applications
   add_subdirectory(blinky)
   add_subdirectory(shell_demo)
   add_subdirectory(config_demo)
   add_subdirectory(freertos_demo)

   # Add your application
   add_subdirectory(temp_monitor)

Application Code
----------------

Create Main File
~~~~~~~~~~~~~~~~

Create ``applications/temp_monitor/main.c``:

.. code-block:: c

   /**
    * \file            main.c
    * \brief           Temperature Monitor Application
    * \author          Your Name
    * \version         1.0.0
    * \date            2026-01-25
    *
    * \copyright       Copyright (c) 2026 Your Name
    *
    * \details         This application monitors temperature using ADC
    *                  and displays status on LEDs with color coding.
    */

   #include "hal/hal.h"
   #include "log/log.h"
   #include "shell/shell.h"
   #include "shell/shell_backend.h"
   #include "shell/shell_command.h"
   #include <stdio.h>
   #include <stdlib.h>

   /*---------------------------------------------------------------------------*/
   /* Configuration                                                             */
   /*---------------------------------------------------------------------------*/

   /** LED pins on STM32F4 Discovery */
   #define LED_GREEN_PORT   HAL_GPIO_PORT_D
   #define LED_GREEN_PIN    12
   #define LED_ORANGE_PORT  HAL_GPIO_PORT_D
   #define LED_ORANGE_PIN   13
   #define LED_RED_PORT     HAL_GPIO_PORT_D
   #define LED_RED_PIN      14
   #define LED_BLUE_PORT    HAL_GPIO_PORT_D
   #define LED_BLUE_PIN     15

   /** ADC configuration */
   #define ADC_INSTANCE     HAL_ADC_0
   #define ADC_CHANNEL      0

   /** Temperature thresholds (in Celsius) */
   #define TEMP_COLD        20.0f
   #define TEMP_NORMAL      25.0f
   #define TEMP_WARM        30.0f
   #define TEMP_HOT         35.0f

   /** Monitoring interval (milliseconds) */
   #define MONITOR_INTERVAL 1000

   /*---------------------------------------------------------------------------*/
   /* Global Variables                                                          */
   /*---------------------------------------------------------------------------*/

   static float g_current_temp = 0.0f;
   static uint32_t g_sample_count = 0;

   /*---------------------------------------------------------------------------*/
   /* LED Control                                                               */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize LEDs
    * \return          HAL_OK on success
    */
   static hal_status_t led_init(void) {
       hal_gpio_config_t config = {
           .direction = HAL_GPIO_DIR_OUTPUT,
           .pull = HAL_GPIO_PULL_NONE,
           .output_mode = HAL_GPIO_OUTPUT_PP,
           .speed = HAL_GPIO_SPEED_LOW,
           .init_level = HAL_GPIO_LEVEL_LOW
       };

       if (hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config) != HAL_OK) {
           return HAL_ERR_FAIL;
       }
       if (hal_gpio_init(LED_ORANGE_PORT, LED_ORANGE_PIN, &config) != HAL_OK) {
           return HAL_ERR_FAIL;
       }
       if (hal_gpio_init(LED_RED_PORT, LED_RED_PIN, &config) != HAL_OK) {
           return HAL_ERR_FAIL;
       }
       if (hal_gpio_init(LED_BLUE_PORT, LED_BLUE_PIN, &config) != HAL_OK) {
           return HAL_ERR_FAIL;
       }

       return HAL_OK;
   }

   /**
    * \brief           Turn off all LEDs
    */
   static void led_all_off(void) {
       hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_LOW);
       hal_gpio_write(LED_ORANGE_PORT, LED_ORANGE_PIN, HAL_GPIO_LEVEL_LOW);
       hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_LOW);
       hal_gpio_write(LED_BLUE_PORT, LED_BLUE_PIN, HAL_GPIO_LEVEL_LOW);
   }

   /**
    * \brief           Update LED status based on temperature
    * \param[in]       temp: Current temperature in Celsius
    */
   static void led_update_status(float temp) {
       led_all_off();

       if (temp < TEMP_COLD) {
           /* Cold: Blue LED */
           hal_gpio_write(LED_BLUE_PORT, LED_BLUE_PIN, HAL_GPIO_LEVEL_HIGH);
       } else if (temp < TEMP_NORMAL) {
           /* Normal: Green LED */
           hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);
       } else if (temp < TEMP_WARM) {
           /* Warm: Orange LED */
           hal_gpio_write(LED_ORANGE_PORT, LED_ORANGE_PIN, HAL_GPIO_LEVEL_HIGH);
       } else {
           /* Hot: Red LED */
           hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
       }
   }

   /*---------------------------------------------------------------------------*/
   /* Temperature Sensing                                                       */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize ADC for temperature sensing
    * \return          HAL_OK on success
    */
   static hal_status_t adc_init_temp(void) {
       hal_adc_config_t config = {
           .resolution = HAL_ADC_RESOLUTION_12BIT,
           .sample_time = HAL_ADC_SAMPLE_TIME_480,
           .continuous = false,
           .dma_enable = false
       };

       return hal_adc_init(ADC_INSTANCE, &config);
   }

   /**
    * \brief           Read temperature from ADC
    * \return          Temperature in Celsius
    */
   static float adc_read_temperature(void) {
       uint16_t adc_value = 0;
       float voltage = 0.0f;
       float temperature = 0.0f;

       /* Read ADC value */
       if (hal_adc_read(ADC_INSTANCE, ADC_CHANNEL, &adc_value) != HAL_OK) {
           LOG_ERROR("Failed to read ADC");
           return 0.0f;
       }

       /* Convert to voltage (assuming 3.3V reference) */
       voltage = (adc_value * 3.3f) / 4096.0f;

       /* Convert to temperature (example: LM35 sensor, 10mV/°C) */
       temperature = voltage * 100.0f;

       return temperature;
   }

   /*---------------------------------------------------------------------------*/
   /* Shell Commands                                                            */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Temperature command handler
    * \param[in]       argc: Argument count
    * \param[in]       argv: Argument array
    * \return          0 on success
    */
   static int cmd_temp(int argc, char* argv[]) {
       (void)argc;
       (void)argv;

       shell_printf("Current temperature: %.2f °C\n", g_current_temp);
       shell_printf("Sample count: %lu\n", (unsigned long)g_sample_count);

       /* Temperature status */
       if (g_current_temp < TEMP_COLD) {
           shell_printf("Status: COLD (Blue LED)\n");
       } else if (g_current_temp < TEMP_NORMAL) {
           shell_printf("Status: NORMAL (Green LED)\n");
       } else if (g_current_temp < TEMP_WARM) {
           shell_printf("Status: WARM (Orange LED)\n");
       } else {
           shell_printf("Status: HOT (Red LED)\n");
       }

       return 0;
   }

   /**
    * \brief           Reset statistics command
    * \param[in]       argc: Argument count
    * \param[in]       argv: Argument array
    * \return          0 on success
    */
   static int cmd_reset(int argc, char* argv[]) {
       (void)argc;
       (void)argv;

       g_sample_count = 0;
       shell_printf("Statistics reset\n");

       return 0;
   }

   /*---------------------------------------------------------------------------*/
   /* Command Definitions                                                       */
   /*---------------------------------------------------------------------------*/

   static const shell_command_t cmd_temp_def = {
       .name = "temp",
       .handler = cmd_temp,
       .help = "Show current temperature",
       .usage = "temp",
       .completion = NULL
   };

   static const shell_command_t cmd_reset_def = {
       .name = "reset",
       .handler = cmd_reset,
       .help = "Reset statistics",
       .usage = "reset",
       .completion = NULL
   };

   /*---------------------------------------------------------------------------*/
   /* Initialization                                                            */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize shell
    * \return          SHELL_OK on success
    */
   static shell_status_t shell_app_init(void) {
       shell_status_t status;

       /* Configure UART for shell */
       hal_uart_config_t uart_config = {
           .baudrate = 115200,
           .wordlen = HAL_UART_WORDLEN_8,
           .stopbits = HAL_UART_STOPBITS_1,
           .parity = HAL_UART_PARITY_NONE,
           .flowctrl = HAL_UART_FLOWCTRL_NONE
       };

       if (hal_uart_init(HAL_UART_1, &uart_config) != HAL_OK) {
           return SHELL_ERROR;
       }

       /* Initialize UART backend */
       status = shell_uart_backend_init(HAL_UART_1);
       if (status != SHELL_OK) {
           return status;
       }

       /* Configure shell */
       shell_config_t config = {
           .prompt = "temp> ",
           .cmd_buffer_size = 128,
           .history_depth = 16,
           .max_commands = 32
       };

       /* Initialize shell */
       status = shell_init(&config);
       if (status != SHELL_OK) {
           return status;
       }

       /* Set backend */
       status = shell_set_backend(&shell_uart_backend);
       if (status != SHELL_OK) {
           shell_deinit();
           return status;
       }

       /* Register commands */
       shell_register_builtin_commands();
       shell_register_command(&cmd_temp_def);
       shell_register_command(&cmd_reset_def);

       return SHELL_OK;
   }

   /*---------------------------------------------------------------------------*/
   /* Main Entry Point                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Main entry point
    * \return          Should never return
    */
   int main(void) {
       /* Initialize HAL system */
       hal_system_init();

       /* Initialize logging */
       log_init(NULL);
       LOG_INFO("Temperature Monitor v1.0.0");

       /* Initialize LEDs */
       if (led_init() != HAL_OK) {
           LOG_ERROR("Failed to initialize LEDs");
           while (1) {
               hal_delay_ms(100);
           }
       }
       LOG_INFO("LEDs initialized");

       /* Initialize ADC */
       if (adc_init_temp() != HAL_OK) {
           LOG_ERROR("Failed to initialize ADC");
           while (1) {
               hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
               hal_delay_ms(100);
           }
       }
       LOG_INFO("ADC initialized");

       /* Initialize shell */
       if (shell_app_init() != SHELL_OK) {
           LOG_ERROR("Failed to initialize shell");
           while (1) {
               hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
               hal_delay_ms(100);
           }
       }
       LOG_INFO("Shell initialized");

       /* Print welcome message */
       shell_printf("\n");
       shell_printf("========================================\n");
       shell_printf("  Temperature Monitor v1.0.0\n");
       shell_printf("  Type 'help' for available commands\n");
       shell_printf("========================================\n");
       shell_print_prompt();

       /* Turn on green LED to indicate ready */
       hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);

       /* Main monitoring loop */
       uint32_t last_sample_time = hal_get_tick();

       while (1) {
           /* Process shell input */
           shell_process();

           /* Check if it's time to sample */
           uint32_t current_time = hal_get_tick();
           if (current_time - last_sample_time >= MONITOR_INTERVAL) {
               last_sample_time = current_time;

               /* Read temperature */
               g_current_temp = adc_read_temperature();
               g_sample_count++;

               /* Update LED status */
               led_update_status(g_current_temp);

               /* Log temperature */
               LOG_INFO("Temperature: %.2f °C (sample #%lu)",
                        g_current_temp, (unsigned long)g_sample_count);
           }

           /* Small delay to avoid busy-waiting */
           hal_delay_ms(10);
       }

       return 0;
   }

Build and Test
--------------

Build for Native
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build -DNEXUS_PLATFORM=native

   # Build
   cmake --build build

   # Run
   ./build/applications/temp_monitor/temp_monitor

Build for STM32F4
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure
   cmake -B build-stm32f4 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4

   # Build
   cmake --build build-stm32f4

   # Flash
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/temp_monitor/temp_monitor.elf verify reset exit"

Test the Application
~~~~~~~~~~~~~~~~~~~~

1. Connect serial terminal (115200 baud)
2. You should see:

.. code-block:: text

   ========================================
     Temperature Monitor v1.0.0
     Type 'help' for available commands
   ========================================
   temp>

3. Try commands:

.. code-block:: text

   temp> help
   Available commands:
     help     - Show this help message
     temp     - Show current temperature
     reset    - Reset statistics

   temp> temp
   Current temperature: 24.50 °C
   Sample count: 15
   Status: NORMAL (Green LED)

4. Observe LEDs changing color based on temperature

Enhancements
------------

Add Configuration
~~~~~~~~~~~~~~~~~

Add Kconfig options for your application:

Create ``applications/temp_monitor/Kconfig``:

.. code-block:: kconfig

   config APP_TEMP_MONITOR
       bool "Temperature Monitor Application"
       default y
       select HAL_GPIO
       select HAL_ADC
       select HAL_UART
       select FRAMEWORK_LOG
       select FRAMEWORK_SHELL
       help
         Temperature monitoring application with LED indicators

   if APP_TEMP_MONITOR

   config APP_TEMP_MONITOR_INTERVAL
       int "Monitoring interval (ms)"
       default 1000
       range 100 10000
       help
         Temperature sampling interval in milliseconds

   config APP_TEMP_MONITOR_COLD_THRESHOLD
       int "Cold temperature threshold (°C)"
       default 20
       help
         Temperature below this is considered cold

   config APP_TEMP_MONITOR_HOT_THRESHOLD
       int "Hot temperature threshold (°C)"
       default 35
       help
         Temperature above this is considered hot

   endif # APP_TEMP_MONITOR

Use configuration in code:

.. code-block:: c

   #include "nexus_config.h"

   #ifdef CONFIG_APP_TEMP_MONITOR_INTERVAL
   #define MONITOR_INTERVAL CONFIG_APP_TEMP_MONITOR_INTERVAL
   #else
   #define MONITOR_INTERVAL 1000
   #endif

Add Multi-Tasking
~~~~~~~~~~~~~~~~~

Use OSAL for multi-tasking:

.. code-block:: c

   #include "osal/osal.h"

   /* Temperature monitoring task */
   static void temp_monitor_task(void* arg) {
       (void)arg;

       while (1) {
           /* Read temperature */
           g_current_temp = adc_read_temperature();
           g_sample_count++;

           /* Update LED status */
           led_update_status(g_current_temp);

           /* Log temperature */
           LOG_INFO("Temperature: %.2f °C", g_current_temp);

           /* Delay */
           osal_task_delay(MONITOR_INTERVAL);
       }
   }

   /* Shell processing task */
   static void shell_task(void* arg) {
       (void)arg;

       while (1) {
           shell_process();
           osal_task_delay(10);
       }
   }

   int main(void) {
       /* ... initialization ... */

       /* Create tasks */
       osal_task_create(temp_monitor_task, "temp_monitor", 512, NULL, 2, NULL);
       osal_task_create(shell_task, "shell", 1024, NULL, 1, NULL);

       /* Start scheduler */
       osal_start_scheduler();

       return 0;
   }

Add Data Logging
~~~~~~~~~~~~~~~~

Log temperature data to storage:

.. code-block:: c

   #include "config/config.h"

   /* Save temperature reading */
   static void save_temperature(float temp) {
       char key[32];
       snprintf(key, sizeof(key), "temp.reading_%lu", g_sample_count);
       config_set_float(key, temp);
   }

   /* Export to JSON */
   static void export_data(void) {
       char buffer[1024];
       size_t size;

       if (config_export(CONFIG_FORMAT_JSON, 0, buffer, sizeof(buffer), &size) == CONFIG_OK) {
           shell_printf("Temperature data:\n%s\n", buffer);
       }
   }

Next Steps
----------

Congratulations! You've created your first Nexus application. Next:

1. :doc:`core_concepts` - Understand Nexus architecture deeply
2. :doc:`configuration` - Master Kconfig configuration
3. :doc:`examples_tour` - Explore more complex examples
4. :doc:`../tutorials/index` - Follow step-by-step tutorials

See Also
--------

* :doc:`../user_guide/hal` - HAL API reference
* :doc:`../user_guide/osal` - OSAL API reference
* :doc:`../user_guide/log` - Logging framework
* :doc:`../user_guide/shell` - Shell framework
* :doc:`../tutorials/first_application` - Alternative tutorial
