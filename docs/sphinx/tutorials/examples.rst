Example Applications
====================

This page documents the example applications included in the Nexus repository. Each example demonstrates specific features and serves as a reference for building your own applications.

All examples are located in the ``applications/`` directory of the repository.

Overview
--------

The Nexus repository includes the following example applications:

.. list-table:: Example Applications
   :header-rows: 1
   :widths: 20 30 50

   * - Example
     - Purpose
     - Key Features
   * - blinky
     - Basic GPIO control
     - LED blinking, GPIO initialization, HAL basics
   * - config_demo
     - Configuration management
     - Config storage, namespaces, import/export, UART output
   * - freertos_demo
     - Multi-tasking with OSAL
     - Tasks, mutexes, semaphores, queues, producer-consumer pattern
   * - shell_demo
     - Command-line interface
     - Shell commands, UART CLI, command registration, line editing

Blinky Example
--------------

**Location**: ``applications/blinky/``

**Description**: A simple LED blinky application that demonstrates basic GPIO control and HAL initialization.

Features
~~~~~~~~

- HAL system initialization
- GPIO configuration as output
- LED blinking in sequence
- Error handling with LED indicators

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

- STM32F4 Discovery board (or compatible)
- LEDs on PD12 (Green), PD13 (Orange), PD14 (Red), PD15 (Blue)

Building and Running
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Configure for STM32F4
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    CMake --build build-stm32f4 --target blinky

    # Flash
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

All four LEDs on the STM32F4 Discovery board blink in sequence:

1. Green LED toggles (500ms)
2. Orange LED toggles (500ms)
3. Red LED toggles (500ms)
4. Blue LED toggles (500ms)
5. Repeat

Key Code Sections
~~~~~~~~~~~~~~~~~

**LED Initialization**:

.. code-block:: c

    static hal_status_t led_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        /* Initialize all LEDs */
        hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config);
        hal_gpio_init(LED_ORANGE_PORT, LED_ORANGE_PIN, &config);
        hal_gpio_init(LED_RED_PORT, LED_RED_PIN, &config);
        hal_gpio_init(LED_BLUE_PORT, LED_BLUE_PIN, &config);

        return HAL_OK;
    }

**Main Loop**:

.. code-block:: c

    while (1) {
        hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
        hal_delay_ms(BLINK_DELAY_MS);
    }

Learning Outcomes
~~~~~~~~~~~~~~~~~

After studying this example, you will understand:

- How to initialize the HAL subsystem
- How to configure GPIO pins as outputs
- How to control LED states
- How to use HAL delay functions
- Basic error handling patterns

Related Tutorials
~~~~~~~~~~~~~~~~~

- :doc:`first_application` - Build your first Nexus application
- :doc:`gpio_control` - Advanced GPIO techniques

Config Demo Example
-------------------

**Location**: ``applications/config_demo/``

**Description**: Demonstrates the Config Manager middleware for storing and retrieving configuration data.

Features
~~~~~~~~

- Configuration storage and retrieval
- Multiple data types (int32, uint32, float, bool, string, blob)
- Namespace isolation
- Configuration query and enumeration
- JSON import/export
- Binary import/export
- UART output for demonstration

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

- STM32F4 Discovery board
- USB-to-Serial adapter connected to UART2 (PA2/PA3)
- Serial terminal at 115200 baud

Building and Running
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Configure for STM32F4
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    CMake --build build-stm32f4 --target config_demo

    # Flash
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/applications/config_demo/config_demo.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

The application runs through several demonstrations:

1. **Basic Configuration**: Stores and retrieves various data types
2. **Namespaces**: Demonstrates namespace isolation (WiFi and BLE configs)
3. **Query**: Lists all configuration entries
4. **JSON Export/Import**: Exports config to JSON and reimports
5. **Binary Export/Import**: Exports config to binary format and reimports

Green LED turns on when ready, and blinks slowly when complete.

Key Code Sections
~~~~~~~~~~~~~~~~~

**Basic Configuration Storage**:

.. code-block:: c

    /* Store different data types */
    config_set_i32("app.timeout", 5000);
    config_set_u32("app.retry", 3);
    config_set_float("sensor.threshold", 25.5f);
    config_set_bool("feature.enabled", true);
    config_set_str("device.name", "Nexus-Demo");

    /* Read values back */
    int32_t timeout = 0;
    config_get_i32("app.timeout", &timeout, 0);

**Namespace Usage**:

.. code-block:: c

    /* Open namespaces */
    config_ns_handle_t wifi_ns, ble_ns;
    config_open_namespace("wifi", &wifi_ns);
    config_open_namespace("ble", &ble_ns);

    /* Store WiFi settings */
    config_ns_set_str(wifi_ns, "ssid", "MyNetwork");
    config_ns_set_bool(wifi_ns, "auto_connect", true);

    /* Close namespaces */
    config_close_namespace(wifi_ns);
    config_close_namespace(ble_ns);

**JSON Export**:

.. code-block:: c

    /* Export to JSON */
    char buffer[1024];
    size_t actual_size = 0;
    config_status_t status = config_export(CONFIG_FORMAT_JSON, 0, buffer,
                                           sizeof(buffer), &actual_size);

Learning Outcomes
~~~~~~~~~~~~~~~~~

After studying this example, you will understand:

- How to initialize and use the Config Manager
- How to store and retrieve different data types
- How to use namespaces for configuration isolation
- How to export and import configurations
- How to query configuration entries
- How to use UART for debug output

Related Documentation
~~~~~~~~~~~~~~~~~~~~~

- :doc:`../user_guide/config` - Config Manager user guide
- :doc:`uart_communication` - UART communication tutorial

FreeRTOS Demo Example
---------------------

**Location**: ``applications/freertos_demo/``

**Description**: Demonstrates multi-tasking using the OSAL with FreeRTOS backend.

Features
~~~~~~~~

- Multiple concurrent tasks
- Task priorities and scheduling
- Mutex for resource protection
- Semaphore for task synchronization
- Message queue for inter-task communication
- Producer-consumer pattern
- System statistics tracking

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

- STM32F4 Discovery board with FreeRTOS support
- LEDs for visual feedback

Building and Running
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Configure for STM32F4 with FreeRTOS
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4 \
        -DNEXUS_OSAL_BACKEND=FreeRTOS

    # Build
    CMake --build build-stm32f4 --target freertos_demo

    # Flash
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/applications/freertos_demo/freertos_demo.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

The application creates four tasks:

1. **Producer Task**: Generates sensor data every 100ms, sends to queue
2. **Consumer Task**: Receives data from queue, processes it
3. **LED Task**: Blinks green LED as heartbeat (500ms)
4. **Stats Task**: Periodically reports system statistics (2s)

LED Indicators:

- Green: Heartbeat (system running)
- Orange: Consumer processing data
- Blue: Producer sending data
- Red: Queue overflow or error

Key Code Sections
~~~~~~~~~~~~~~~~~

**Task Creation**:

.. code-block:: c

    /* Producer task - Normal priority */
    osal_task_config_t producer_config = {
        .name = "Producer",
        .func = producer_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = TASK_STACK_SIZE
    };
    osal_task_create(&producer_config, &g_producer_task);

**Message Queue**:

.. code-block:: c

    /* Create message queue for sensor data */
    osal_queue_create(sizeof(sensor_data_t), SENSOR_QUEUE_SIZE,
                     &g_sensor_queue);

    /* Send to queue */
    osal_queue_send(g_sensor_queue, &data, 10);

    /* Receive from queue */
    osal_queue_receive(g_sensor_queue, &data, OSAL_WAIT_FOREVER);

**Mutex Protection**:

.. code-block:: c

    /* Create mutex */
    osal_mutex_create(&g_stats_mutex);

    /* Lock mutex */
    if (osal_mutex_lock(g_stats_mutex, 100) == OSAL_OK) {
        /* Critical section - update shared statistics */
        g_stats.samples_produced++;
        osal_mutex_unlock(g_stats_mutex);
    }

**Semaphore Signaling**:

.. code-block:: c

    /* Create semaphore */
    osal_sem_create_counting(SENSOR_QUEUE_SIZE, 0, &g_data_ready_sem);

    /* Signal data ready */
    osal_sem_give(g_data_ready_sem);

    /* Wait for data */
    osal_sem_take(g_data_ready_sem, 500);

Learning Outcomes
~~~~~~~~~~~~~~~~~

After studying this example, you will understand:

- How to initialize OSAL and create tasks
- How to use task priorities
- How to protect shared resources with mutexes
- How to synchronize tasks with semaphores
- How to pass messages between tasks with queues
- How to implement producer-consumer patterns
- How to start the RTOS scheduler

Related Documentation
~~~~~~~~~~~~~~~~~~~~~

- :doc:`task_creation` - Multi-tasking tutorial
- :doc:`../user_guide/osal` - OSAL user guide

Shell Demo Example
------------------

**Location**: ``applications/shell_demo/``

**Description**: Demonstrates the Shell/CLI middleware for interactive command-line interfaces.

Features
~~~~~~~~

- Interactive command-line interface over UART
- Custom command registration
- Command parsing and execution
- Command completion
- Built-in commands (help, version, etc.)
- LED control commands
- System commands (tick, delay, reboot)

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

- STM32F4 Discovery board
- USB-to-Serial adapter connected to UART2 (PA2/PA3)
- Serial terminal at 115200 baud

Building and Running
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Configure for STM32F4
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    CMake --build build-stm32f4 --target shell_demo

    # Flash
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/applications/shell_demo/shell_demo.elf verify reset exit"

Expected Behavior
~~~~~~~~~~~~~~~~~

After flashing, open a serial terminal and you'll see:

.. code-block:: text

    ========================================
      Nexus Shell Demo v1.0.0
      STM32F4 Discovery Board
      Type 'help' for available commands
    ========================================
    nexus>

Available Commands:

- ``help`` - Show available commands
- ``led <color> <on|off|toggle>`` - Control LEDs (green, orange, red, blue, all)
- ``button`` - Read user button status
- ``tick`` - Show system tick count
- ``delay <ms>`` - Delay for specified milliseconds
- ``reboot`` - Reboot the system

Example Session:

.. code-block:: text

    nexus> help
    Available commands:
      help     - Show this help
      led      - Control LEDs on the board
      button   - Read user button status
      tick     - Show system tick count
      delay    - Delay for specified milliseconds
      reboot   - Reset system

    nexus> led green on
    LED green ON

    nexus> tick
    System tick: 12345 ms

    nexus> button
    User button: RELEASED

Key Code Sections
~~~~~~~~~~~~~~~~~

**Command Handler**:

.. code-block:: c

    static int cmd_led(int argc, char* argv[]) {
        if (argc < 3) {
            shell_printf("Usage: led <color> <on|off|toggle>\n");
            return 1;
        }

        const char* color = argv[1];
        const char* action = argv[2];

        /* Determine which LED and perform action */
        if (strcmp(action, "on") == 0) {
            hal_gpio_write(port, pin, HAL_GPIO_LEVEL_HIGH);
            shell_printf("LED %s ON\n", color);
        }

        return 0;
    }

**Command Registration**:

.. code-block:: c

    static const shell_command_t cmd_led_def = {
        .name = "led",
        .handler = cmd_led,
        .help = "Control LEDs on the board",
        .usage = "led <color> <on|off|toggle>",
        .completion = led_color_completion
    };

    /* Register command */
    shell_register_command(&cmd_led_def);

**Shell Initialization**:

.. code-block:: c

    /* Configure shell */
    shell_config_t config = {
        .prompt = "nexus> ",
        .cmd_buffer_size = 128,
        .history_depth = 16,
        .max_commands = 32
    };

    /* Initialize shell */
    shell_init(&config);

    /* Set UART backend */
    shell_set_backend(&shell_uart_backend);

    /* Register commands */
    shell_register_builtin_commands();
    shell_register_command(&cmd_led_def);

Learning Outcomes
~~~~~~~~~~~~~~~~~

After studying this example, you will understand:

- How to initialize the Shell framework
- How to register custom commands
- How to parse command arguments
- How to implement command handlers
- How to use UART for interactive CLI
- How to provide command completion
- How to integrate shell with your application

Related Documentation
~~~~~~~~~~~~~~~~~~~~~

- :doc:`../user_guide/shell` - Shell framework user guide
- :doc:`uart_communication` - UART communication tutorial

Building All Examples
---------------------

To build all examples at once:

.. code-block:: bash

    # Configure
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4

    # Build all examples
    CMake --build build-stm32f4 --target all

Example binaries will be located in:

- ``build-stm32f4/applications/blinky/blinky.elf``
- ``build-stm32f4/applications/config_demo/config_demo.elf``
- ``build-stm32f4/applications/freertos_demo/freertos_demo.elf``
- ``build-stm32f4/applications/shell_demo/shell_demo.elf``

Modifying Examples
------------------

All examples are designed to be easily modified and extended:

1. **Copy the Example**: Copy the example directory to create your own application
2. **Modify CMakeLists.txt**: Update project name and dependencies
3. **Modify main.c**: Add your custom functionality
4. **Build and Test**: Build and flash to your hardware

Example: Creating a Custom Application from Blinky
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Copy blinky example
    cp -r applications/blinky applications/my_app

    # Edit CMakeLists.txt
    # Change: project(blinky C)
    # To:     project(my_app C)

    # Edit main.c with your custom code

    # Build
    CMake --build build-stm32f4 --target my_app

Troubleshooting
---------------

**Example doesn't build:**

- Ensure you have the correct toolchain installed
- Check that the platform is set correctly (``-DNEXUS_PLATFORM=stm32f4``)
- Verify all dependencies are available

**Example doesn't run after flashing:**

- Check that you flashed the correct .elf file
- Verify the board is powered correctly
- Try resetting the board
- Check LED indicators for error states

**UART examples show no output:**

- Verify UART wiring (TX/RX crossed correctly)
- Check baud rate matches (115200)
- Ensure correct COM port is selected in terminal
- Try a different USB-to-Serial adapter

**FreeRTOS example crashes:**

- Increase task stack sizes if needed
- Check for stack overflow
- Verify FreeRTOS is configured correctly

Next Steps
----------

- Try modifying the examples to add your own features
- Combine concepts from multiple examples
- Create your own application based on these examples
- Explore the :doc:`../user_guide/hal` and :doc:`../user_guide/osal` for more advanced features

