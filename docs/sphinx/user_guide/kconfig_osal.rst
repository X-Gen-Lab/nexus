OSAL Backend Configuration Guide
================================

This guide provides detailed configuration information for OSAL (Operating System Abstraction Layer) backends in the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The OSAL provides a unified API for different operating systems and execution environments. Configuration includes:

* Backend selection (Bare-metal, FreeRTOS, RT-Thread, Zephyr, Linux, Native)
* System parameters (tick rate, heap size, stack size)
* Linker script configuration
* Backend-specific options

Backend Selection
-----------------

Select one OSAL backend using the choice menu:

.. code-block:: Kconfig

    choice
        prompt "OSAL Backend"
        default OSAL_BAREMETAL

    config OSAL_BAREMETAL
        bool "Bare-metal (No OS)"

    config OSAL_FREERTOS
        bool "FreeRTOS"

    config OSAL_RTTHREAD
        bool "RT-Thread"

    config OSAL_ZEPHYR
        bool "Zephyr RTOS"

    config OSAL_LINUX
        bool "Linux"

    config OSAL_NATIVE
        bool "Native (PC Simulation)"

    endchoice

Only one backend can be selected at a time.


Bare-Metal Backend
------------------

Overview
^^^^^^^^

The bare-metal backend provides minimal RTOS functionality without a full operating system.

**Features:**

* No RTOS overhead
* Simple cooperative scheduler
* Basic synchronization primitives
* Minimal memory footprint
* Deterministic behavior

**Use Cases:**

* Simple applications
* Resource-constrained systems
* Real-time critical systems
* Bootloaders

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_BAREMETAL=y
    CONFIG_OSAL_BACKEND_NAME="baremetal"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=16384
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

**Generated configuration:**

.. code-block:: c

    #define NX_CONFIG_OSAL_BAREMETAL 1
    #define NX_CONFIG_OSAL_BACKEND_NAME "baremetal"
    #define NX_CONFIG_OSAL_TICK_RATE_HZ 1000
    #define NX_CONFIG_OSAL_HEAP_SIZE 16384
    #define NX_CONFIG_OSAL_MAIN_STACK_SIZE 4096

Available APIs
^^^^^^^^^^^^^^

**Task Management:**

* ``osal_task_create()`` - Create cooperative task
* ``osal_task_delete()`` - Delete task
* ``osal_task_delay()`` - Delay task
* ``osal_task_yield()`` - Yield to other tasks

**Synchronization:**

* ``osal_mutex_create()`` - Create mutex (simple lock)
* ``osal_semaphore_create()`` - Create counting semaphore
* ``osal_queue_create()`` - Create message queue

**Time:**

* ``osal_get_tick_count()`` - Get system tick count
* ``osal_delay_ms()`` - Delay in milliseconds

Limitations
^^^^^^^^^^^

* No preemptive scheduling
* No priority-based scheduling
* Limited synchronization primitives
* No advanced RTOS features

Best Practices
^^^^^^^^^^^^^^

* Keep tasks short and cooperative
* Use delays to yield CPU
* Minimize heap usage
* Avoid blocking operations

Example Configuration
^^^^^^^^^^^^^^^^^^^^^

**Minimal bare-metal system:**

.. code-block:: Kconfig

    CONFIG_OSAL_BAREMETAL=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=8192
    CONFIG_OSAL_MAIN_STACK_SIZE=2048


FreeRTOS Backend
----------------

Overview
^^^^^^^^

FreeRTOS is the most popular real-time operating system for embedded systems.

**Features:**

* Preemptive multitasking
* Priority-based scheduling
* Rich synchronization primitives
* Mature and well-tested
* Large ecosystem
* Commercial support available

**Use Cases:**

* General embedded applications
* Multi-threaded applications
* Real-time systems
* Production systems

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_BACKEND_NAME="FreeRTOS"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

**Generated configuration:**

.. code-block:: c

    #define NX_CONFIG_OSAL_FREERTOS 1
    #define NX_CONFIG_OSAL_BACKEND_NAME "FreeRTOS"
    #define NX_CONFIG_OSAL_TICK_RATE_HZ 1000
    #define NX_CONFIG_OSAL_HEAP_SIZE 32768
    #define NX_CONFIG_OSAL_MAIN_STACK_SIZE 2048
    #define NX_CONFIG_OSAL_MAX_PRIORITIES 32

Available APIs
^^^^^^^^^^^^^^

**Task Management:**

* ``osal_task_create()`` - Create task with priority
* ``osal_task_delete()`` - Delete task
* ``osal_task_suspend()`` - Suspend task
* ``osal_task_resume()`` - Resume task
* ``osal_task_delay()`` - Delay task
* ``osal_task_delay_until()`` - Delay until absolute time
* ``osal_task_get_priority()`` - Get task priority
* ``osal_task_set_priority()`` - Set task priority

**Synchronization:**

* ``osal_mutex_create()`` - Create mutex
* ``osal_mutex_lock()`` - Lock mutex
* ``osal_mutex_unlock()`` - Unlock mutex
* ``osal_semaphore_create()`` - Create semaphore
* ``osal_semaphore_give()`` - Give semaphore
* ``osal_semaphore_take()`` - Take semaphore
* ``osal_queue_create()`` - Create queue
* ``osal_queue_send()`` - Send to queue
* ``osal_queue_receive()`` - Receive from queue
* ``osal_event_group_create()`` - Create event group

**Memory:**

* ``osal_malloc()`` - Allocate memory
* ``osal_free()`` - Free memory

**Time:**

* ``osal_get_tick_count()`` - Get system tick count
* ``osal_delay_ms()`` - Delay in milliseconds

Tick Rate Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**Standard tick rate (1000 Hz):**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000

* 1 ms resolution
* Good balance
* Most common

**High resolution (10000 Hz):**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=10000

* 0.1 ms resolution
* Higher CPU overhead
* For precise timing

**Low resolution (100 Hz):**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=100

* 10 ms resolution
* Lower CPU overhead
* For power-sensitive applications

Heap Configuration
^^^^^^^^^^^^^^^^^^

**Small system (16 KB):**

.. code-block:: Kconfig

    CONFIG_OSAL_HEAP_SIZE=16384

**Medium system (32 KB):**

.. code-block:: Kconfig

    CONFIG_OSAL_HEAP_SIZE=32768

**Large system (64 KB):**

.. code-block:: Kconfig

    CONFIG_OSAL_HEAP_SIZE=65536

Priority Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Standard priorities (32 levels):**

.. code-block:: Kconfig

    CONFIG_OSAL_MAX_PRIORITIES=32

* 0 = Idle priority
* 1-30 = Application priorities
* 31 = Highest priority

**Minimal priorities (8 levels):**

.. code-block:: Kconfig

    CONFIG_OSAL_MAX_PRIORITIES=8

* Lower memory usage
* Simpler scheduling

Best Practices
^^^^^^^^^^^^^^

* Use appropriate task priorities
* Avoid priority inversion
* Use mutexes for shared resources
* Keep ISRs short
* Use queues for inter-task communication
* Monitor stack usage
* Configure heap size appropriately

Example Configurations
^^^^^^^^^^^^^^^^^^^^^^

**General purpose application:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

**Resource-constrained system:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=16384
    CONFIG_OSAL_MAIN_STACK_SIZE=1024
    CONFIG_OSAL_MAX_PRIORITIES=8

**High-performance system:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096
    CONFIG_OSAL_MAX_PRIORITIES=32


RT-Thread Backend
-----------------

Overview
^^^^^^^^

RT-Thread is a Chinese open-source RTOS with rich features and IoT support.

**Features:**

* Preemptive multitasking
* Priority-based scheduling
* Rich middleware (file system, network stack)
* IoT protocols (MQTT, CoAP)
* Device driver framework
* Package manager

**Use Cases:**

* IoT applications
* Connected devices
* Applications requiring middleware
* Chinese market

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_RTTHREAD=y
    CONFIG_OSAL_BACKEND_NAME="rtthread"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

Available APIs
^^^^^^^^^^^^^^

RT-Thread provides similar APIs to FreeRTOS through the OSAL abstraction:

* Task management
* Synchronization primitives
* Memory management
* Time management

Best Practices
^^^^^^^^^^^^^^

* Use RT-Thread's device driver framework
* Leverage built-in middleware
* Use package manager for components
* Follow RT-Thread conventions

Example Configuration
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_OSAL_RTTHREAD=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

Zephyr Backend
--------------

Overview
^^^^^^^^

Zephyr is a Linux Foundation RTOS with modern architecture and extensive hardware support.

**Features:**

* Modern C-based RTOS
* Extensive hardware support
* Device tree configuration
* Networking stack
* Bluetooth stack
* Security features

**Use Cases:**

* Modern embedded applications
* IoT devices
* Bluetooth applications
* Security-critical systems

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_ZEPHYR=y
    CONFIG_OSAL_BACKEND_NAME="zephyr"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=100
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

Best Practices
^^^^^^^^^^^^^^

* Use device tree for hardware configuration
* Leverage Zephyr's networking stack
* Use Zephyr's security features
* Follow Zephyr conventions

Example Configuration
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_OSAL_ZEPHYR=y
    CONFIG_OSAL_TICK_RATE_HZ=100
    CONFIG_OSAL_HEAP_SIZE=32768
    CONFIG_OSAL_MAIN_STACK_SIZE=2048
    CONFIG_OSAL_MAX_PRIORITIES=32

Linux Backend
-------------

Overview
^^^^^^^^

The Linux backend uses POSIX threads for testing on Linux systems.

**Features:**

* POSIX threads
* Standard Linux tools
* Easy debugging
* Fast development

**Use Cases:**

* Development and testing
* Algorithm validation
* CI/CD pipelines

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_LINUX=y
    CONFIG_OSAL_BACKEND_NAME="linux"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

Best Practices
^^^^^^^^^^^^^^

* Use for development and testing
* Validate algorithms before hardware deployment
* Use standard Linux debugging tools

Example Configuration
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_OSAL_LINUX=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

Native Backend
--------------

Overview
^^^^^^^^

The Native backend provides PC simulation for development and testing.

**Features:**

* Cross-platform (Windows, Linux, macOS)
* Fast development iteration
* No hardware required
* Standard debugging tools

**Use Cases:**

* Development and testing
* Algorithm validation
* CI/CD pipelines

Configuration
^^^^^^^^^^^^^

**Basic configuration:**

.. code-block:: Kconfig

    CONFIG_OSAL_NATIVE=y
    CONFIG_OSAL_BACKEND_NAME="native"

**System parameters:**

.. code-block:: Kconfig

    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

Best Practices
^^^^^^^^^^^^^^

* Use for development and testing
* Validate configuration before hardware deployment
* Use for CI/CD testing

Example Configuration
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    CONFIG_OSAL_NATIVE=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=65536
    CONFIG_OSAL_MAIN_STACK_SIZE=4096

Backend Comparison
------------------

Feature Comparison
^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 15 15 15 15

   * - Feature
     - Bare-metal
     - FreeRTOS
     - RT-Thread
     - Zephyr
     - Linux
     - Native
   * - Preemptive
     - No
     - Yes
     - Yes
     - Yes
     - Yes
     - Yes
   * - Priorities
     - No
     - Yes
     - Yes
     - Yes
     - Yes
     - Yes
   * - Memory
     - Minimal
     - Small
     - Medium
     - Medium
     - Large
     - Large
   * - Middleware
     - No
     - Limited
     - Rich
     - Rich
     - Full
     - Full
   * - Maturity
     - N/A
     - Very High
     - High
     - High
     - Very High
     - N/A
   * - Ecosystem
     - N/A
     - Large
     - Medium
     - Large
     - Very Large
     - N/A

Use Case Recommendations
^^^^^^^^^^^^^^^^^^^^^^^^

**Bare-metal:**

* Simple applications
* Bootloaders
* Resource-constrained systems
* Deterministic behavior required

**FreeRTOS:**

* General embedded applications
* Production systems
* Multi-threaded applications
* Industry standard

**RT-Thread:**

* IoT applications
* Chinese market
* Applications requiring middleware
* Connected devices

**Zephyr:**

* Modern embedded applications
* Bluetooth applications
* Security-critical systems
* Extensive hardware support needed

**Linux:**

* Development and testing
* Algorithm validation
* CI/CD pipelines

**Native:**

* Development and testing
* Cross-platform development
* CI/CD pipelines

See Also
--------

* :doc:`kconfig_tutorial` - Kconfig tutorial
* :doc:`kconfig_platforms` - Platform-specific configuration
* :doc:`../api/osal` - OSAL API reference
* :doc:`porting` - Platform porting guide
