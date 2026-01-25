Examples Tour
=============

This guide provides a tour of the example applications included with Nexus, explaining what each demonstrates and how to use them.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

Nexus includes several example applications that demonstrate different features and use cases:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Example
     - Complexity
     - Demonstrates
   * - Blinky
     - Beginner
     - GPIO, delays, basic HAL usage
   * - Shell Demo
     - Intermediate
     - UART, Shell, commands, GPIO control
   * - Config Demo
     - Intermediate
     - Configuration management, JSON/binary export
   * - FreeRTOS Demo
     - Advanced
     - Multi-tasking, synchronization, queues

All examples are located in the ``applications/`` directory.

Blinky Example
--------------

**Location**: ``applications/blinky/``

**Complexity**: Beginner

What It Demonstrates
~~~~~~~~~~~~~~~~~~~~~

* HAL initialization
* GPIO configuration
* LED control (write, toggle)
* Delay functions
* Basic error handling

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

**STM32F4 Discovery**:

* 4 LEDs (PD12-PD15)
* No external components needed

**Native Platform**:

* Simulated LEDs (console output)

Code Walkthrough
~~~~~~~~~~~~~~~~

**Initialization**:

.. code-block:: c

   /* Initialize HAL layer */
   if (hal_init() != HAL_OK) {
       while (1) {
           /* HAL initialization failed */
       }
   }

   /* Initialize LEDs */
   hal_gpio_config_t config = {
       .direction = HAL_GPIO_DIR_OUTPUT,
       .pull = HAL_GPIO_PULL_NONE,
       .output_mode = HAL_GPIO_OUTPUT_PP,
       .speed = HAL_GPIO_SPEED_LOW,
       .init_level = HAL_GPIO_LEVEL_LOW
   };

   hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config);

**Main Loop**:

.. code-block:: c

   while (1) {
       /* Toggle LEDs in sequence */
       hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
       hal_delay_ms(500);

       hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
       hal_delay_ms(500);

       hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
       hal_delay_ms(500);

       hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
       hal_delay_ms(500);
   }

Building and Running
~~~~~~~~~~~~~~~~~~~~

**Native**:

.. code-block:: bash

   cmake -B build -DNEXUS_PLATFORM=native
   cmake --build build
   ./build/applications/blinky/blinky

**STM32F4**:

.. code-block:: bash

   cmake -B build-stm32f4 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4
   cmake --build build-stm32f4
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

LEDs blink in sequence:

1. Green LED (500ms)
2. Orange LED (500ms)
3. Red LED (500ms)
4. Blue LED (500ms)
5. Repeat

Learning Points
~~~~~~~~~~~~~~~

* How to initialize HAL
* How to configure GPIO pins
* How to control LEDs
* How to use delay functions
* Basic error handling patterns

Shell Demo Example
------------------

**Location**: ``applications/shell_demo/``

**Complexity**: Intermediate

What It Demonstrates
~~~~~~~~~~~~~~~~~~~~~

* UART communication
* Shell framework usage
* Command registration
* Command parsing and execution
* GPIO control via commands
* Button status reading
* System information commands

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

**STM32F4 Discovery**:

* 4 LEDs (PD12-PD15)
* User button (PA0)
* UART2 (PA2/PA3) for serial console
* USB-to-Serial adapter (if not using ST-Link VCP)

**Native Platform**:

* Console I/O

Code Walkthrough
~~~~~~~~~~~~~~~~

**Command Definition**:

.. code-block:: c

   static int cmd_led(int argc, char* argv[]) {
       if (argc < 3) {
           shell_printf("Usage: led <color> <on|off|toggle>\n");
           return 1;
       }

       const char* color = argv[1];
       const char* action = argv[2];

       /* Determine LED and perform action */
       if (strcmp(color, "green") == 0) {
           if (strcmp(action, "on") == 0) {
               hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);
           }
           /* ... */
       }

       return 0;
   }

   static const shell_command_t cmd_led_def = {
       .name = "led",
       .handler = cmd_led,
       .help = "Control LEDs on the board",
       .usage = "led <color> <on|off|toggle>",
       .completion = led_color_completion
   };

**Shell Initialization**:

.. code-block:: c

   /* Configure UART */
   hal_uart_config_t uart_config = {
       .baudrate = 115200,
       .wordlen = HAL_UART_WORDLEN_8,
       .stopbits = HAL_UART_STOPBITS_1,
       .parity = HAL_UART_PARITY_NONE,
       .flowctrl = HAL_UART_FLOWCTRL_NONE
   };
   hal_uart_init(HAL_UART_1, &uart_config);

   /* Initialize shell */
   shell_config_t config = {
       .prompt = "nexus> ",
       .cmd_buffer_size = 128,
       .history_depth = 16,
       .max_commands = 32
   };
   shell_init(&config);
   shell_set_backend(&shell_uart_backend);

   /* Register commands */
   shell_register_builtin_commands();
   shell_register_command(&cmd_led_def);

**Main Loop**:

.. code-block:: c

   while (1) {
       /* Process shell input (non-blocking) */
       shell_process();
   }

Building and Running
~~~~~~~~~~~~~~~~~~~~

**Native**:

.. code-block:: bash

   cmake -B build -DNEXUS_PLATFORM=native
   cmake --build build
   ./build/applications/shell_demo/shell_demo

**STM32F4**:

.. code-block:: bash

   cmake -B build-stm32f4 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4
   cmake --build build-stm32f4
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/shell_demo/shell_demo.elf verify reset exit"

   # Connect serial terminal (115200 baud)
   screen /dev/ttyUSB0 115200

Available Commands
~~~~~~~~~~~~~~~~~~

.. code-block:: text

   nexus> help
   Available commands:
     help     - Show this help message
     clear    - Clear screen
     reboot   - Reboot the system
     led      - Control LEDs (led <color> <on|off|toggle>)
     button   - Read button status
     tick     - Show system tick count
     delay    - Delay for specified milliseconds

Example Usage
~~~~~~~~~~~~~

.. code-block:: text

   nexus> led green on
   LED green ON

   nexus> led all off
   LED all OFF

   nexus> button
   User button: RELEASED

   nexus> tick
   System tick: 12345 ms

   nexus> delay 1000
   Delaying 1000 ms...
   Done

Learning Points
~~~~~~~~~~~~~~~

* How to initialize UART
* How to use Shell framework
* How to register custom commands
* How to parse command arguments
* How to implement command completion
* How to read GPIO inputs

Config Demo Example
-------------------

**Location**: ``applications/config_demo/``

**Complexity**: Intermediate

What It Demonstrates
~~~~~~~~~~~~~~~~~~~~~

* Configuration management
* Key-value storage
* Namespace isolation
* JSON import/export
* Binary serialization
* Configuration queries
* Data persistence

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

**STM32F4 Discovery**:

* UART2 (PA2/PA3) for serial output
* USB-to-Serial adapter

**Native Platform**:

* Console output

Code Walkthrough
~~~~~~~~~~~~~~~~

**Basic Configuration**:

.. code-block:: c

   /* Store different data types */
   config_set_i32("app.timeout", 5000);
   config_set_u32("app.retry", 3);
   config_set_float("sensor.threshold", 25.5f);
   config_set_bool("feature.enabled", true);
   config_set_str("device.name", "Nexus-Demo");

   /* Read values back */
   int32_t timeout;
   config_get_i32("app.timeout", &timeout, 0);

**Namespaces**:

.. code-block:: c

   /* Open namespace */
   config_ns_handle_t wifi_ns;
   config_open_namespace("wifi", &wifi_ns);

   /* Store in namespace */
   config_ns_set_str(wifi_ns, "ssid", "MyNetwork");
   config_ns_set_bool(wifi_ns, "auto_connect", true);

   /* Close namespace */
   config_close_namespace(wifi_ns);

**JSON Export**:

.. code-block:: c

   /* Export to JSON */
   char buffer[1024];
   size_t size;
   config_export(CONFIG_FORMAT_JSON, 0, buffer, sizeof(buffer), &size);

   /* Print JSON */
   uart_printf("Configuration:\n%s\n", buffer);

**Iteration**:

.. code-block:: c

   /* Iterate all entries */
   static bool list_callback(const config_entry_info_t* info, void* user_data) {
       uart_printf("  %s [%s]\n", info->key, type_to_string(info->type));
       return true;
   }

   config_iterate(list_callback, NULL);

Building and Running
~~~~~~~~~~~~~~~~~~~~

**Native**:

.. code-block:: bash

   cmake -B build -DNEXUS_PLATFORM=native
   cmake --build build
   ./build/applications/config_demo/config_demo

**STM32F4**:

.. code-block:: bash

   cmake -B build-stm32f4 \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4
   cmake --build build-stm32f4
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/config_demo/config_demo.elf verify reset exit"

   # Connect serial terminal
   screen /dev/ttyUSB0 115200

Expected Output
~~~~~~~~~~~~~~~

.. code-block:: text

   ========================================
     Nexus Config Manager Demo
     STM32F4 Discovery Board
   ========================================

   === Basic Configuration Demo ===
   Stored configuration values
     app.timeout = 5000
     app.retry = 3
     sensor.threshold = 25.5
     feature.enabled = true
     device.name = Nexus-Demo

   === Namespace Demo ===
   WiFi namespace:
     ssid = MyNetwork
     auto_connect = true
     channel = 6

   === JSON Export Demo ===
   Exported 256 bytes of JSON:
   {
     "app": {
       "timeout": 5000,
       "retry": 3
     },
     "sensor": {
       "threshold": 25.5
     },
     ...
   }

Learning Points
~~~~~~~~~~~~~~~

* How to use Config framework
* How to store different data types
* How to use namespaces
* How to export/import JSON
* How to iterate configurations
* How to handle binary data

FreeRTOS Demo Example
---------------------

**Location**: ``applications/freertos_demo/``

**Complexity**: Advanced

What It Demonstrates
~~~~~~~~~~~~~~~~~~~~~

* FreeRTOS integration
* Task creation and management
* Mutex synchronization
* Queue communication
* Software timers
* Task priorities
* Inter-task communication

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

**STM32F4 Discovery**:

* 4 LEDs (PD12-PD15)
* UART2 (PA2/PA3) for debug output

**Native Platform**:

* Not supported (requires FreeRTOS)

Code Walkthrough
~~~~~~~~~~~~~~~~

**Task Creation**:

.. code-block:: c

   /* Task function */
   void led_task(void* arg) {
       uint32_t led_id = (uint32_t)arg;

       while (1) {
           /* Toggle LED */
           hal_gpio_toggle(led_ports[led_id], led_pins[led_id]);

           /* Delay */
           osal_task_delay(led_delays[led_id]);
       }
   }

   /* Create tasks */
   osal_task_create(led_task, "led0", 256, (void*)0, 1, NULL);
   osal_task_create(led_task, "led1", 256, (void*)1, 1, NULL);

**Mutex Usage**:

.. code-block:: c

   /* Shared resource */
   static int shared_counter = 0;
   static osal_mutex_handle_t counter_mutex;

   void increment_task(void* arg) {
       while (1) {
           /* Lock mutex */
           osal_mutex_lock(counter_mutex, OSAL_WAIT_FOREVER);

           /* Critical section */
           shared_counter++;
           LOG_INFO("Counter: %d", shared_counter);

           /* Unlock mutex */
           osal_mutex_unlock(counter_mutex);

           osal_task_delay(100);
       }
   }

**Queue Communication**:

.. code-block:: c

   /* Message structure */
   typedef struct {
       uint32_t id;
       uint32_t data;
   } message_t;

   /* Producer task */
   void producer_task(void* arg) {
       while (1) {
           message_t msg = {.id = 1, .data = rand()};
           osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);
           osal_task_delay(500);
       }
   }

   /* Consumer task */
   void consumer_task(void* arg) {
       while (1) {
           message_t msg;
           osal_queue_receive(queue, &msg, OSAL_WAIT_FOREVER);
           LOG_INFO("Received: id=%lu, data=%lu", msg.id, msg.data);
       }
   }

**Software Timer**:

.. code-block:: c

   /* Timer callback */
   void timer_callback(void* arg) {
       LOG_INFO("Timer expired");
       hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
   }

   /* Create and start timer */
   osal_timer_handle_t timer;
   osal_timer_create(&timer, "timer", 1000, true, NULL, timer_callback);
   osal_timer_start(timer);

Building and Running
~~~~~~~~~~~~~~~~~~~~

**STM32F4 with FreeRTOS**:

.. code-block:: bash

   cmake -B build-freertos \
       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
       -DNEXUS_PLATFORM=stm32f4 \
       -DNEXUS_OSAL_BACKEND=freertos
   cmake --build build-freertos
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-freertos/applications/freertos_demo/freertos_demo.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

* Multiple LEDs blinking at different rates
* Debug output showing task execution
* Mutex-protected counter incrementing
* Queue messages being sent and received
* Timer callback executing periodically

Learning Points
~~~~~~~~~~~~~~~

* How to create and manage tasks
* How to use mutexes for synchronization
* How to use queues for communication
* How to use software timers
* How to handle task priorities
* How to debug multi-threaded applications

Comparing Examples
------------------

Complexity Progression
~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Example
     - HAL
     - OSAL
     - Framework
     - Difficulty
   * - Blinky
     - GPIO
     - Delays
     - None
     - ⭐
   * - Shell Demo
     - GPIO, UART
     - Delays
     - Shell
     - ⭐⭐
   * - Config Demo
     - GPIO, UART
     - Delays
     - Config
     - ⭐⭐
   * - FreeRTOS Demo
     - GPIO, UART
     - Tasks, Mutex, Queue
     - Log
     - ⭐⭐⭐

Learning Path
~~~~~~~~~~~~~

Recommended order for learning:

1. **Blinky** - Start here to understand basics
2. **Shell Demo** - Learn UART and Shell
3. **Config Demo** - Learn configuration management
4. **FreeRTOS Demo** - Learn multi-tasking

Modifying Examples
------------------

Adding Features
~~~~~~~~~~~~~~~

Try these modifications to learn more:

**Blinky**:

* Change blink pattern
* Add button to control blinking
* Add different LED sequences

**Shell Demo**:

* Add new commands
* Implement command history
* Add parameter validation

**Config Demo**:

* Add more configuration options
* Implement configuration presets
* Add configuration validation

**FreeRTOS Demo**:

* Add more tasks
* Implement task communication patterns
* Add semaphores and event flags

Next Steps
----------

After exploring the examples:

1. :doc:`first_application` - Create your own application
2. :doc:`../tutorials/index` - Follow step-by-step tutorials
3. :doc:`../user_guide/index` - Deep dive into components
4. :doc:`../development/contributing` - Contribute your own examples

See Also
--------

* :doc:`../tutorials/first_application` - Build your first app
* :doc:`../user_guide/hal` - HAL documentation
* :doc:`../user_guide/osal` - OSAL documentation
* :doc:`../user_guide/shell` - Shell documentation
